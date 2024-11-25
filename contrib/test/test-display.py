#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright 2024 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
#
# This script tests EDID and CEC compliance of a connected display.
# The actual compliance tests are performed by the edid-decode and cec-ctl,
# cec-compliance and cec-follower utilities, and this script is a convenient
# frontend for that.
#
# The CEC tests must be performed with root permissions as that is needed to
# run the monitor mode of cec-ctl: this will produce the monitor.log during
# the CEC tests, which is very useful when debugging problems.
#
# The edid-compliance test can be used as follows:
#
#	test-display.py edid-compliance
#
#   will just list the available EDID files found in /sys/class/drm/, and you
#   can select one and give it to edid-compliance as follows:
#
#	test-display.py edid-compliance --edid-file /sys/class/drm/card1-HDMI-A-1/edid
#
#   The results of the test are written in /tmp/test-display.
#
# The remainder of the tests are all CEC tests. You can list the available CEC
# devices by running 'cec-ctl -A'.
#
# All CEC tests run with cec-follower and cec-ctl -M (monitoring CEC traffic)
# in the background. This ensures that the CEC output acts like a standard
# Playback device and that all CEC traffic is monitored, which is helpful
# for debugging. The script assumes no other CEC applications are active
# since that can interfere with the tests.
#
# - cec-compliance: this runs the cec-compliance test. It runs a subset of
#   all the possible compliance tests, meant to test the core CEC commands
#   and anything related to the basic display functionality.
# - cec-stress: this runs cec-ctl --stress-test-standby-wakeup-cycle for
#   10000 cycles.
# - cec-stress-sleep: this runs cec-ctl --stress-test-standby-wakeup-cycle max-sleep=5
#   for 10000 cycles.
# - cec-stress-random: this runs cec-ctl --stress-test-random-standby-wakeup-cycle
#   for 4000 cycles.
#
# These CEC stress tests do a very good job uncovering race conditions in the
# display's firmware.
import argparse
import contextlib
import datetime
import logging
import os
import pathlib
import re
import sys
import time
from subprocess import run, PIPE, STDOUT, CalledProcessError, Popen

log = logging.getLogger(__file__)


def run_cmd(cmd, file=None, shell=False):
    """Run a shell command using subprocess.run.

    Args:
        cmd (str): Command to run
        file (str, optional): Path to logfile. Defaults to None.
        shell (bool, optional): Spawn an intermediate shell process. Defaults to False.

    Returns:
        str: Output from the cmd
    """
    cmd = cmd.strip()
    log.debug(f"Running cmd: {cmd}\n")
    try:
        if file:
            stdout = open(file, "w")
            if not shell:
                version = run_cmd("cec-compliance --version")
                stdout.write(f"Running cmd: {cmd}\n\n")
                stdout.write(f"{version}\n")
                stdout.flush()
        else:
            stdout = PIPE
        cmd = cmd.split() if not shell else cmd
        output = run(args=cmd, stdout=stdout,
                     stderr=STDOUT, shell=shell)
        output.check_returncode()
    except CalledProcessError:
        log.error(output.stdout.decode("utf-8"))
        raise
    finally:
        if file:
            stdout.close()
            return
    log.debug(f"Output: {output.stdout.decode('utf-8')}")
    return output.stdout.decode("utf-8")


@contextlib.contextmanager
def follower_context(device, logpath):
    """Setup a cec follower context for cec device.

    Args:
        device (str): CEC device number from /dev/cec<device>
        logpath (str): path to logfile
    """
    follower_cmd = f"cec-follower -d{device} -i 0,0x36 -w -v"
    log.info(f"Starting follower: {follower_cmd}")
    fdesc = open(logpath, "w")
    proc = Popen(follower_cmd.split(), stderr=fdesc, stdout=fdesc)
    try:
        if proc.poll():
            raise FollowerSetupError("Failed to set up follower")
        time.sleep(2)  # wait for cec-follower to finish polling remote devices
        yield
    finally:
        log.info("Terminating follower")
        proc.terminate()
        fdesc.close()


@contextlib.contextmanager
def monitor_context(device, logpath, soak_time_exit):
    """Setup a cec monitor context for cec device.

    Args:
        device (str): CEC device number from /dev/cec<device>
        logpath (str): path to logfile
        soak_time_exit (int): seconds to wait for msg after test
    """
    monitor_cmd = f"cec-ctl -d{device} -M -w -v"
    log.info(f"Starting monitor: {monitor_cmd}")
    fdesc = open(logpath, "w")
    proc = Popen(monitor_cmd.split(), stderr=fdesc, stdout=fdesc)
    try:
        if proc.poll():
            raise MonitorSetupError("Failed to set up monitor")
        yield
    finally:
        log.info(f"Soaking monitor log for {soak_time_exit} seconds")
        try:
            time.sleep(soak_time_exit)
        finally:
            log.info("Terminating monitor")
            proc.terminate()
            fdesc.close()


def write_header(cmd, log_file):
    """Write header.

    Args:
        cmd (str): Log command line
        log_file (str): Path to log file
    """
    version = run_cmd("cec-compliance --version")
    cmd = cmd.strip("\n")
    with open(log_file, "a+") as f:
        f.write(f"Running cmd: {cmd}\n\n")
        f.write(f"{version}\n")
        f.flush()


def execute_cmd(command, log_file):
    """Execute a command and write to stdout and the log.

    Args:
        command (str): The command to execute
        log_file (str): Path to log file

    Raises:
        CalledProcessError: [description]
    """

    log.info(f"Executing: {command}")
    write_header(command, log_file)
    print("")

    with open(log_file, "a+") as f:
        proc = Popen(command.split(), stdout=PIPE, stderr=STDOUT)
        for line in proc.stdout:
            f.write(line.decode("utf-8"))
            print(line.decode("utf-8").strip("\n"))

        proc.stdout.close()
        proc.wait()
        print("")


def setup_cec_device(args):
    """Configure the CEC device.

    Returns:
        str: CEC device number from /dev/cec<device>
    """
    dev = args.cec_device
    if dev.isdigit():
        dev = "/dev/cec" + dev
    run_cmd(f"cec-ctl -d {dev} --playback -V {args.cec_vendor_id}")
    return dev


def set_to_state(device, state, timeout=5):
    """Send standby | image-view-on to display and assert power status.

    This will ask for power status every second.

    Args:
        device (str): CEC device number from /dev/cec<device>
        state (str): Set state to assert from give power status
        timeout (int, optional): Set timeout for power status assertion. Defaults to 5.

    Raises:
        PowerStatusTimeout: Power status request timed out.
    """
    state = state.lower()
    assert state in ["standby", "on"], "Invalid power state"
    cmd = "--image-view-on" if state == "on" else "--standby"
    run_cmd(f"cec-ctl -d{device} -t0 {cmd} -v -w")

    start = time.time()
    while (time.time() - start < timeout):
        if state == power_status(device).lower():
            return state
        time.sleep(1)
    else:
        error_msg = f"Display never returned power state {state}"
        raise PowerStatusTimeout(error_msg)


def power_status(device):
    """Check power status on display.

    Args:
        device (str): CEC device number from /dev/cec<device>

    Returns:
        str: Current power state
    """
    power_status_cmd = f"cec-ctl -d{device} -t0 --give-device-power-status -v -w"
    power_status_pattern = r"pwr-state:\s(to-on|on|to-standby|standby)"
    power_status_output = run_cmd(power_status_cmd)
    match = re.search(power_status_pattern, power_status_output)

    if match:
        state = match.group(1).lower()
        log.info(f"Display's power state is {state}")
    else:
        state = ""
    return state


def run_edid_test(args, log_dir = None):
    """Run EDID test.

    Args:
        args (argparse): Arguments for the test
        log_dir (str): Path to the log dir
    """
    if args.edid_file is None and args.edid_ddc is None:
        first = True
        for i in pathlib.Path("/sys/class/drm/").glob("card*/edid"):
            if str(i).find("Writeback") < 0:
                edid_bytes = pathlib.Path(i).read_bytes()
                if len(edid_bytes):
                    if first:
                        print("The following EDIDs are found:\n")
                        first = False
                    print(f"{i}: {len(edid_bytes)} bytes")
        if first:
            print("No EDIDs were found in /sys/class/drm/")
        return

    std_log = os.path.join(log_dir, "edid-compliance-stdout.log")

    try:
        if args.edid_file:
            cmd = f"edid-decode --check -p -n {args.edid_file}"
        else:
            cmd = f"edid-decode -a {args.edid_ddc} --i2c-edid --check -p -n"
        run_cmd(cmd, std_log, shell=True)
    finally:
        archive_files = os.path.relpath(std_log, log_dir.parent)
        tar_cmd = f"tar -C {log_dir.parent} -czvf {log_dir}.tar.gz {archive_files}"
        run_cmd(tar_cmd)
        out = run_cmd(f"cat {std_log}", shell=True)
        print(f"{out}")
        print(f"Logs directory: {log_dir}")
        print(f"Archive file: {log_dir}.tar.gz\n")

def main(args):
    """Entry point for command line interface.

    Args:
        args (argparse): Arguments for the test
    """

    if args.command == "edid-compliance":
        if args.edid_file:
            cmd = f"edid-decode -P {args.edid_file}"
        elif args.edid_ddc:
            cmd = f"edid-decode -a {args.edid_ddc} --i2c-edid -P"
        else:
            run_edid_test(args)
            return
        port = run_cmd(cmd).strip("\n")
    else:
        device = setup_cec_device(args)
        port = run_cmd(f"cec-ctl -d {device} -x -s").strip("\n")
        if port == 'f.f.f.f':
            print("No valid physical address: wake up display, wait 15 seconds and try again.")
            run_cmd(f"cec-ctl -d {device} -s -t0 --image-view-on")
            time.sleep(15)  # wait for the display to wake up
            port = run_cmd(f"cec-ctl -d {device} -x -s").strip("\n")
            if port == 'f.f.f.f':
                exit(f"The CEC device {device} has no physical address. Is a display connected?\n")

    port = port.replace(".0.0.0", "")
    port = "-hdmi-" + port
    date = datetime.datetime.now().strftime("%Y.%m.%d-%H.%M.%S")
    folder_name = args.command + port + "-" + date
    log_dir = pathlib.Path(f"{args.log_dir}/{folder_name}")
    os.makedirs(log_dir)

    logging.basicConfig(
        level=logging.DEBUG,
        format="%(asctime)s [%(levelname)s] - %(message)s",
        filename=os.path.join(log_dir, "script.log")
    )

    log_fmt = "%(asctime)s: [%(levelname)s] - %(message)s"
    console = logging.StreamHandler()
    console.setLevel(logging.INFO)
    console.setFormatter(logging.Formatter(log_fmt))
    log.addHandler(console)

    if args.command == "edid-compliance":
        run_edid_test(args, log_dir)
        return

    if os.geteuid() != 0:
        exit("The CEC tests require root privileges, run this program with sudo.\n")

    if args.command == "cec-stress":
        std_log = os.path.join(log_dir, "cec-stress-stdout.log")
    elif args.command == "cec-stress-random":
        std_log = os.path.join(log_dir, "cec-stress-random-stdout.log")
    elif args.command == "cec-compliance":
        std_log = os.path.join(log_dir, "cec-compliance-stdout.log")

    follower_log = os.path.join(log_dir, "follower.log")
    monitor_log = os.path.join(log_dir, "monitor.log")

    tests_to_run = ("--test-audio-return-channel-control "
                    "--test-device-osd-transfer "
                    "--test-dynamic-auto-lipsync "
                    "--test-one-touch-play "
                    "--test-power-status "
                    "--test-routing-control "
                    "--test-remote-control-passthrough "
                    "--test-system-audio-control "
                    "--test-system-information "
                    "--test-vendor-specific-commands "
                    "--test-standby-resume "
                    "--expect-with-no-warnings request-current-latency=0 "
                    "--expect-with-no-warnings give-osd-name=0 ")

    try:
        with contextlib.ExitStack() as outer_stack:
            outer_stack.enter_context(
                follower_context(device, follower_log))
            outer_stack.enter_context(monitor_context(
                device, monitor_log, soak_time_exit=args.log_soak_time))

            if args.console:
                # Desktop managers can get very confused by displays appearing and
                # disappearing due to Hotplug Detect toggles that happen while going
                # into and out of standby.
                # Testing demonstrated that it can end up with a black image.
                # If the --console option is given, then switch to the console mode
                # and fill the console with lines of text (otherwise the video
                # might just consists of a cursor at the top-left corner, which is
                # almost indistinguishable to just black video). The console drm/kms
                # code is much better at handling HPD toggles.
                run_cmd('systemctl start multi-user.target', std_log, shell=True)
                for i in range(150):
                    run_cmd('date +"<2> %D %T" >/dev/kmsg', std_log, shell=True)

            if args.command == "cec-stress":
                command = (f"cec-ctl -d{device} -t0"
                           f" --stress-test-standby-wakeup-cycle {args.args} -w")
                execute_cmd(command, std_log)
            elif args.command == "cec-stress-random":
                command = (f"cec-ctl -d{device} -t0"
                           f" --stress-test-random-standby-wakeup-cycle {args.args} -w")
                execute_cmd(command, std_log)
            elif args.command == "cec-compliance":
                command = f"cec-compliance -S -w -d{device} -r {tests_to_run}"
                execute_cmd(command, std_log)

    finally:
        if args.console:
            run_cmd('systemctl start graphical.target', std_log, shell=True)

        archive_files = [
            os.path.basename(monitor_log),
            os.path.basename(follower_log),
            os.path.basename(std_log), ]

        archive_files = [os.path.join(folder_name, f) for f in archive_files]
        archive_files = " ".join(archive_files)
        tar_cmd = f"tar -C {log_dir.parent} -czvf {log_dir}.tar.gz {archive_files}"

        run_cmd(tar_cmd)
        print(f"\nLogs directory: {log_dir}")
        print(f"Archive file: {log_dir}.tar.gz\n")


def add_cec_args(subparser, soak_time):
    """Add standard CEC options to subparser.

    Args:
        subparser (argparse): CEC subparser
        soak_time (int): CEC monitor soak time in seconds
    """

    subparser.add_argument("-d", "--cec-device",
                           dest="cec_device",
                           type=str,
                           default="/dev/cec0",
                           help="/dev/cecX device for the output connected to the display, default is /dev/cec0")
    subparser.add_argument("-V", "--vendor-id",
                           dest="cec_vendor_id",
                           type=str,
                           default='0x000c03',
                           help="CEC Vendor ID to use, default is 0x000c03")
    subparser.add_argument("-s", "--log-soak-time",
                           metavar="WAIT",
                           type=int,
                           default=soak_time,
                           help=f"Seconds to wait before closing the monitor log, default is {soak_time}s")
    subparser.add_argument("-C", "--console",
                           action='store_true',
                           help="Switch to console mode before running CEC test")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Display Test",
        formatter_class=argparse.RawTextHelpFormatter)

    parser.add_argument("--log-dir",
                        dest="log_dir",
                        type=str,
                        default='/tmp/test-display',
                        help="Name of the log directory that is created, default is '/tmp/test-display'")

    subparser = parser.add_subparsers(help=" ", dest="command")

    edid_parser = subparser.add_parser("edid-compliance",
                                       help="Run the EDID compliance test, without arguments it will list available EDIDs in /sys/class/drm/")
    edid_parser.add_argument("--edid-file",
                             type=str,
                             help="The EDID file from the display")
    edid_parser.add_argument("--edid-ddc",
                             type=str,
                             help="The /dev/i2c-X device from where to read the EDID over the DDC lines from the display")

    cec_parser = subparser.add_parser("cec-compliance",
                                      help="Run cec compliance test")
    add_cec_args(cec_parser, 5)

    stress_parser = subparser.add_parser("cec-stress",
                                         help="Run standby-wakeup cycle stress test")
    add_cec_args(stress_parser, 120)
    stress_parser.add_argument("-a", "--args",
                               type=str,
                               default="cnt=10000",
                               help="Arguments for the cec-ctl --stress-test-standby-wakeup-cycle option, "
                               "default is cnt=10000")

    stress_sleep_parser = subparser.add_parser("cec-stress-sleep",
                                               help="Run standby-wakeup cycle stress test with a sleep before each state transition")
    add_cec_args(stress_sleep_parser, 120)
    stress_sleep_parser.add_argument("-a", "--args",
                                     type=str,
                                     default="cnt=10000,max-sleep=5",
                                     help="Arguments for the cec-ctl --stress-test-standby-wakeup-cycle option, "
                                     "default is cnt=10000,max-sleep=5")

    stress_random_parser = subparser.add_parser("cec-stress-random",
                                                help="Run random standby-wakeup cycle stress test")
    add_cec_args(stress_random_parser, 120)
    stress_random_parser.add_argument("-a", "--args",
                                      type=str,
                                      default="cnt=4000",
                                      help="Arguments for the cec-ctl --stress-test-random-standby-wakeup-cycle option, "
                                      "default is cnt=4000")

    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        sys.exit(1)

main(args)
