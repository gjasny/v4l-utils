#!/usr/bin/env python3
"""
SPDX-License-Identifier: GPL-2.0-only

Copyright 2024 Cisco Systems, Inc. and/or its affiliates. All rights reserved.

This script tests EDID and CEC compliance of a connected display.
The actual compliance tests are performed by the edid-decode and cec-ctl,
cec-compliance and cec-follower utilities, and this script is a convenient
frontend for that.

The CEC tests must be performed with root permissions as that is needed to
run the monitor mode of cec-ctl: this will produce the monitor.log during
the CEC tests, which is very useful when debugging problems.

The edid-compliance test can be used as follows:

  test_display.py edid-compliance

  will just list the available EDID files found in /sys/class/drm/, and you
  can select one and give it to edid-compliance as follows:

  test_display.py edid-compliance --edid-file /sys/class/drm/card1-HDMI-A-1/edid

  The results of the test are written in /tmp/test_display.

The remainder of the tests are all CEC tests. You can list the available CEC
devices by running 'cec-ctl -A'.

All CEC tests run with cec-follower and cec-ctl -M (monitoring CEC traffic)
in the background. This ensures that the CEC output acts like a standard
Playback device and that all CEC traffic is monitored, which is helpful
for debugging. The script assumes no other CEC applications are active
since that can interfere with the tests.

- cec-compliance: this runs the cec-compliance test. It runs a subset of
  all the possible compliance tests, meant to test the core CEC commands
  and anything related to the basic display functionality.
- cec-stress: this runs cec-ctl --stress-test-standby-wakeup-cycle for
  10000 cycles.
- cec-stress-sleep: this runs cec-ctl --stress-test-standby-wakeup-cycle max-sleep=5
  for 10000 cycles.
- cec-stress-random: this runs cec-ctl --stress-test-random-standby-wakeup-cycle
  for 4000 cycles.

These CEC stress tests do a very good job uncovering race conditions in the
display's firmware.
"""
import argparse
import contextlib
import datetime
import logging
import os
import pathlib
import sys
import time
from subprocess import run, PIPE, STDOUT, CalledProcessError, Popen

log = logging.getLogger(__file__)

class FollowerSetupError(Exception):
    """Exception when follower setup fails."""


class MonitorSetupError(Exception):
    """Exception when monitor setup fails."""


class EdidSetupError(Exception):
    """Exception when DDC reader setup fails."""


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
    log.debug("Running cmd: %s", cmd)
    try:
        if file:
            stdout = open(file, "w", encoding = "utf-8")
            if not shell:
                version = run_cmd("cec-compliance --version")
                stdout.write(f"Running cmd: {cmd}\n\n")
                stdout.write(f"{version}\n")
                stdout.flush()
        else:
            stdout = PIPE
        cmd = cmd.split() if not shell else cmd
        output = run(args=cmd, stdout=stdout,
                     stderr=STDOUT, shell=shell, check=False, encoding="utf-8")
        output.check_returncode()
    except CalledProcessError:
        log.error(output.stdout)
        raise
    finally:
        if file:
            stdout.close()

    log.debug("Output: %s", output.stdout)
    return output.stdout


@contextlib.contextmanager
def follower_context(device, logpath):
    """Setup a cec follower context for cec device.

    Args:
        device (str): CEC device number from /dev/cec<device>
        logpath (str): path to logfile
    """
    follower_cmd = f"cec-follower -d{device} -i 0,0x36 -w -v"
    log.info("Starting follower: %s", follower_cmd)
    fdesc = open(logpath, "w", encoding = "utf-8")
    proc = Popen(follower_cmd.split(), stderr=fdesc, stdout=fdesc, encoding = "utf-8")
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
    log.info("Starting monitor: %s", monitor_cmd)
    fdesc = open(logpath, "w", encoding = "utf-8")
    proc = Popen(monitor_cmd.split(), stderr=fdesc, stdout=fdesc, encoding = "utf-8")
    try:
        if proc.poll():
            raise MonitorSetupError("Failed to set up monitor")
        yield
    finally:
        log.info("Soaking monitor log for %s seconds", soak_time_exit)
        try:
            time.sleep(soak_time_exit)
        finally:
            log.info("Terminating monitor")
            proc.terminate()
            fdesc.close()


@contextlib.contextmanager
def edid_context(device, duration, logpath):
    """Setup an EDID reader context for DDC device.

    Args:
        device (str): DDC device /dev/i2c-X
        duration (int): duration of test in seconds
        logpath (str): path to logfile
    """
    edid_cmd = f"edid-decode -a{device} --i2c-test-reliability duration={duration}"
    log.info("Starting DDC reader: %s", edid_cmd)
    fdesc = open(logpath, "w", encoding = "utf-8")
    proc = Popen(edid_cmd.split(), stderr=fdesc, stdout=fdesc, encoding = "utf-8")
    try:
        if proc.poll():
            raise EdidSetupError("Failed to set up DDC reader")
        yield
    finally:
        log.info("Terminating DDC reader")
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
    with open(log_file, "a+", encoding = "utf-8") as f:
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

    log.info("Executing: %s", command)
    write_header(command, log_file)
    print("")

    with open(log_file, "a+", encoding = "utf-8") as f:
        proc = Popen(command.split(), stdout=PIPE, stderr=STDOUT, encoding = "utf-8")
        for line in proc.stdout:
            f.write(line)
            print(line.strip("\n"))

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
        out = run_cmd(f"cat {std_log}", shell=True)
        print(f"{out}")
        print(f"Logs directory: {log_dir}")
        if not args.no_tar:
            tar_cmd = f"tar -C {log_dir.parent} -czvf {log_dir}.tar.gz {log_dir.name}"
            run_cmd(tar_cmd)
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
        if os.geteuid() != 0:
            sys.exit("The CEC tests require root privileges, run this program with sudo.\n")

        device = setup_cec_device(args)
        port = run_cmd(f"cec-ctl -d {device} -x -s").strip("\n")
        if port == 'f.f.f.f':
            print("No valid physical address: wake up display, wait 15 seconds and try again.")
            run_cmd(f"cec-ctl -d {device} -s -t0 --image-view-on")
            time.sleep(15)  # wait for the display to wake up
            port = run_cmd(f"cec-ctl -d {device} -x -s").strip("\n")
            if port == 'f.f.f.f':
                sys.exit(f"The CEC device {device} has no physical address. Is a display connected?\n")

    port = port.replace(".0.0.0", "")
    port = "-hdmi-" + port
    date = datetime.datetime.now().strftime("%Y.%m.%d-%H.%M.%S")
    folder_name = args.command + port + "-" + date
    if args.log_dir:
        log_dir = pathlib.Path(args.log_dir)
    else:
        log_dir = pathlib.Path(f"{args.top_log_dir}/tmp/test_display/{folder_name}")
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

    if args.command == "cec-stress":
        std_log = os.path.join(log_dir, "cec-stress-stdout.log")
    elif args.command == "cec-stress-sleep":
        std_log = os.path.join(log_dir, "cec-stress-sleep-stdout.log")
    elif args.command == "cec-stress-random":
        std_log = os.path.join(log_dir, "cec-stress-random-stdout.log")
    elif args.command == "cec-compliance":
        std_log = os.path.join(log_dir, "cec-compliance-stdout.log")
    elif args.command == "cec-ddc-reliability":
        std_log = os.path.join(log_dir, "cec-ddc-reliability-stdout.log")
    else:
        sys.exit(f"Unknown command {args.command}")

    follower_log = os.path.join(log_dir, "follower.log")
    monitor_log = os.path.join(log_dir, "monitor.log")
    edid_log = os.path.join(log_dir, "edid.log")

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
            if args.command == "cec-ddc-reliability":
                outer_stack.enter_context(edid_context(args.edid_ddc, args.duration, edid_log))

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
                cnt = 0
                while cnt < 150:
                    run_cmd('date +"<2> %D %T" >/dev/kmsg', std_log, shell=True)
                    cnt = cnt + 1

            if args.command == "cec-stress":
                command = (f"cec-ctl -d{device} -t0"
                           f" --stress-test-standby-wakeup-cycle {args.args} -w")
                execute_cmd(command, std_log)
            elif args.command == "cec-stress-sleep":
                sleep_args = args.args
                if not "max-sleep=" in sleep_args:
                    sleep_args = sleep_args + ",max-sleep=5"
                command = (f"cec-ctl -d{device} -t0"
                           f" --stress-test-standby-wakeup-cycle {sleep_args} -w")
                execute_cmd(command, std_log)
            elif args.command == "cec-stress-random":
                command = (f"cec-ctl -d{device} -t0"
                           f" --stress-test-random-standby-wakeup-cycle {args.args} -w")
                execute_cmd(command, std_log)
            elif args.command == "cec-compliance":
                command = f"cec-compliance -S -w -d{device} -r {tests_to_run}"
                execute_cmd(command, std_log)
            elif args.command == "cec-ddc-reliability":
                command = f"cec-ctl -S -w -d{device} --test-reliability {args.duration}"
                execute_cmd(command, std_log)

    finally:
        if args.console:
            run_cmd('systemctl start graphical.target', std_log, shell=True)

        print(f"\nLogs directory: {log_dir}")
        if not args.no_tar:
            tar_cmd = f"tar -C {log_dir.parent} -czvf {log_dir}.tar.gz {log_dir.name}"
            run_cmd(tar_cmd)
            print(f"Archive file: {log_dir}.tar.gz\n")


def add_cec_args(cecparser, soak_time, have_console):
    """Add standard CEC options to subparser.

    Args:
        cecparser (argparse): CEC subparser
        soak_time (int): CEC monitor soak time in seconds
        have_console (bool): if true, add --console option
    """

    cecparser.add_argument("-d", "--cec-device",
                           dest="cec_device",
                           type=str,
                           default="/dev/cec0",
                           help="/dev/cecX device for the output connected to the display, default is /dev/cec0")
    cecparser.add_argument("-V", "--vendor-id",
                           dest="cec_vendor_id",
                           type=str,
                           default='0x000c03',
                           help="CEC Vendor ID to use, default is 0x000c03")
    cecparser.add_argument("-s", "--log-soak-time",
			   metavar="WAIT",
			   type=int,
			   default=soak_time,
			   help=f"Seconds to wait before closing the monitor log, default is {soak_time}s")
    if have_console:
        cecparser.add_argument("-C", "--console",
                               action='store_true',
                               help="Switch to console mode before running CEC test")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Display Test",
        formatter_class=argparse.RawTextHelpFormatter)

    parser.add_argument("--top-log-dir",
                        type=str,
                        default='/tmp/test_display',
                        help="Name of the top log directory that is created, default is '/tmp/test_display'")
    parser.add_argument("--log-dir",
                        type=str,
                        default=None,
                        help="All logs are created in this directory. Overrides --top-log-dir")
    parser.add_argument("--no-tar",
			action='store_true',
			help="Do not create the tar archive for the results")

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
    add_cec_args(cec_parser, 5, True)

    stress_parser = subparser.add_parser("cec-stress",
                                         help="Run standby-wakeup cycle stress test")
    add_cec_args(stress_parser, 120, True)
    stress_parser.add_argument("-a", "--args",
                               type=str,
                               default="cnt=10000",
                               help="Arguments for the cec-ctl --stress-test-standby-wakeup-cycle option, "
                               "default is cnt=10000")

    stress_sleep_parser = subparser.add_parser("cec-stress-sleep",
                                               help="Run standby-wakeup cycle stress test with a sleep before each state transition")
    add_cec_args(stress_sleep_parser, 120, True)
    stress_sleep_parser.add_argument("-a", "--args",
                                     type=str,
                                     default="cnt=10000,max-sleep=5",
                                     help="Arguments for the cec-ctl --stress-test-standby-wakeup-cycle option, "
                                     "default is cnt=10000,max-sleep=5")

    stress_random_parser = subparser.add_parser("cec-stress-random",
                                                help="Run random standby-wakeup cycle stress test")
    add_cec_args(stress_random_parser, 120, True)
    stress_random_parser.add_argument("-a", "--args",
                                      type=str,
                                      default="cnt=4000",
                                      help="Arguments for the cec-ctl --stress-test-random-standby-wakeup-cycle option, "
                                      "default is cnt=4000")

    cec_ddc_test = subparser.add_parser("cec-ddc-reliability",
					help="Run a CEC and DDC cable reliability test")
    add_cec_args(cec_ddc_test, 5, False)
    cec_ddc_test.add_argument("-A", "--edid-ddc",
			      type=str,
			      help="The /dev/i2c-X device from where to read the EDID over the DDC lines from the display")
    cec_ddc_test.add_argument("-D", "--duration",
			      type=int,
                              default=0,
			      help="The duration in seconds to run this test (default=0=forever)")

    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        sys.exit(1)

main(args)
