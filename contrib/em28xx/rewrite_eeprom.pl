#!/usr/bin/perl -w
#
################################################################################
#  Copyright (C) 2009
#
#  Mauro Carvalho Chehab <mchehab@redhat.com>
#  Douglas Schilling Landgraf <dougsland@redhat.com>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, version 2 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  Although not very common, on a few devices, the eeprom may be erased, due to a
#  bug on a *few eeprom* chipsets that sometimes considers i2c messages to other
#  devices as being for it.
#
#  The solution for it is to reprogram the eeprom with their original contents.
#
#  Modules this script is known to work with: em28xx and saa7134
#  * Not tested against newer em28xx chipsets like the em2874 and em2884
#
########################################################
# NOTE                                                 #
################################################################################
# Since the script will try to detect the proper i2c bus address, it will only #
# work well if you have just one V4L device connected.                         #
################################################################################
#
########################################
# What do you need to run this script? #
########################################
#
# * eeprom - A dump file with the older eeprom.
#
# As example this is a dump from EMPIRE TV DUAL (310U):
#				 ^^^^^^^^^^^^^^
# shell> dmesg
#
# [11196.181543] em28xx #0: i2c eeprom 00: 1a eb 67 95 1a eb 10 e3 d0 12 5c 03 6a 22 00 00
# [11196.181559] em28xx #0: i2c eeprom 10: 00 00 04 57 4e 07 00 00 00 00 00 00 00 00 00 00
# [11196.181572] em28xx #0: i2c eeprom 20: 46 00 01 00 f0 10 01 00 00 00 00 00 5b 1e 00 00
# [11196.181585] em28xx #0: i2c eeprom 30: 00 00 20 40 20 80 02 20 01 01 00 00 00 00 00 00
# [11196.181598] em28xx #0: i2c eeprom 40: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
# [11196.181610] em28xx #0: i2c eeprom 50: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
# [11196.181622] em28xx #0: i2c eeprom 60: 00 00 00 00 00 00 00 00 00 00 22 03 55 00 53 00
# [11196.181635] em28xx #0: i2c eeprom 70: 42 00 20 00 32 00 38 00 38 00 31 00 20 00 44 00
# [11196.181648] em28xx #0: i2c eeprom 80: 65 00 76 00 69 00 63 00 65 00 00 00 00 00 00 00
# [11196.181660] em28xx #0: i2c eeprom 90: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
# [11196.181673] em28xx #0: i2c eeprom a0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
# [11196.181685] em28xx #0: i2c eeprom b0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
# [11196.181698] em28xx #0: i2c eeprom c0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
# [11196.181710] em28xx #0: i2c eeprom d0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
# [11196.181722] em28xx #0: i2c eeprom e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
# [11196.181735] em28xx #0: i2c eeprom f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
#
#############################################
# Where can I find my original eeprom?      #
#############################################
#
# e.g: Old dmesg output
#
########################################
# How this script works?	       #
########################################
#
# To use it, you'll need to run the script, passing, as a parameter, the original dmesg with
# your original eeprom (before the corruption). Something like:
#
# shell> perl ./rewrite_eeprom my_old_dmesg_with_right_eeprom.txt > eeprom_script
#
# It will generate the eeprom_script file, with a script capable of recovering
# your eeprom.
#
# After having the proper eeprom_script generated, you'll need to run it, as root:
#
# shell> sh ./eeprom_script
#
#
# After running the script, your original contents should be restored. Try to
# remove it and reinsert to see if it will be properly detected again.
#
use Switch;

$argv = $ARGV[0];

# Files
$file_eeprom = "/tmp/eeprom-original.txt";
$file_bus = "/tmp/i2c-detect-bus.txt";
$file_addr = "/tmp/i2c-detect-addr.txt";
$file_i2c_tmp = "/tmp/i2ctmp.txt";

# Bus
$businfo = "";
$addrinfo = "";

# Modules
$modules = "em28xx|saa713";
$_  = $modules;
tr/|/ /d;
s/saa713/saa7134/g;
$modules_str = $_;

# eeprom stuff
$eeprom_backup_dir = "~/.eeprom";
$NR_EEPROM_REGS = 0;

##########################
# Subroutines            #
##########################

sub build_script
{
	my $script_prevent;

	# Building script

	$script_prevent  = "#!/bin/bash\n";
	$script_prevent .= "#\n";
	$script_prevent .= "# Notes:\n";
	$script_prevent .= "# - i2c_dev module should be loaded\n";
	$script_prevent .= "# - v4l driver should be loaded\n\n";


	$script_prevent .= "if [ ! \"\$UID\" = \"0\" ]; then\n\techo \"You must run this script as root\";\n\texit;\nfi\n\n";

	$script_prevent .= "i2cset &> /dev/null\n";
	$script_prevent .= "if [ ! \"\$\?\" = \"1\" ]; then\n";
	$script_prevent .= "\techo \"Please install i2c-tools package before continue.\";\n";
	$script_prevent .= "\texit;\n";
	$script_prevent .= "fi\n\n";

	$script_prevent .= "modprobe i2c-dev\n";
	$script_prevent .= "if [ ! \"\$\?\" = \"0\" ]; then\n";
	$script_prevent .= "\techo \"Can't load i2c-dev module.\";\n";
	$script_prevent .= "\texit;\n";
	$script_prevent .= "fi\n\n";

	$script_prevent .= "clear;\n";
	$script_prevent .= "echo \"";
	$script_prevent .=  "\n\033[1;31m#     #     #     ######   #     #  ###  #     #   #####\n";
	$script_prevent .= "#  #  #    # #    #     #  ##    #   #   ##    #  #     #\n";
	$script_prevent .= "#  #  #   #   #   #     #  # #   #   #   # #   #  #      \n";
	$script_prevent .= "#  #  #  #     #  ######   #  #  #   #   #  #  #  #  ####\n";
	$script_prevent .= "#  #  #  #######  #   #    #   # #   #   #   # #  #     #\n";
	$script_prevent .= "#  #  #  #     #  #    #   #    ##   #   #    ##  #     #\n";
	$script_prevent .= " ## ##   #     #  #     #  #     #  ###  #     #   #####\033[0m\n\n";


	$script_prevent .= "This tool is *ONLY RECOMMENDED* in cases of: LOST or CORRUPTED EEPROM data.\n";
	$script_prevent .= "Otherwise:\n\nYOU MAY *LOST* THE CURRENT EEPROM FROM YOUR DEVICE AND IT WILL MAKE YOUR BOARD ";
	$script_prevent .= "*DO NOT WORK* UNTIL YOU SET THE RIGHT EEPROM AGAIN!\n\n";
	$script_prevent .= "If you have *any doubt*, BEFORE run it contact people from: linux-media\@vger.kernel.org\n";

	$script_prevent .= "Are you \033[1;31mABSOLUTELY SURE\033[0m to continue? (yes or not)\";\n\n";
	$script_prevent .= "read confirmation;\n";
	$script_prevent .= "if \[ ! \"\$confirmation\" = \"yes\" \]; then\n";
	$script_prevent .= "\techo \"process aborted\";\n";
	$script_prevent .= "\texit;\n";
	$script_prevent .= "fi\n\n";

	$script_prevent .= "lsmod | egrep -e \"$modules\" &> /dev/null\n";
	$script_prevent .= "if [ ! \"\$\?\" = \"0\" ]; then\n";
	$script_prevent .= "\techo \"Aborting script.. None of the supported driver $modules_str are loaded. Did you forget to connect the device?\";\n";
	$script_prevent .= "\texit;";
	$script_prevent .= "\nfi\n\n";

	print $script_prevent;
}

sub check_user
{
	if ($>) {
		die "You must run this program as root\n";
	}

}

sub get_bus_and_addr
{

	my $lines = 0;
	my $output = "#!/bin/bash\n";

	system("\ni2cdetect -l | egrep -e \"$modules\" 2> /dev/null | cut -b 5 > $file_bus\n");

	# Checking number if lines
	open(FILE, $file_bus) or die "Can't open `$file_bus': $!";
	while (sysread FILE, $buffer, 1) {
		$lines += ($buffer =~ tr/\n//);
	}
	close FILE;

	switch ($lines) {
		case 0 {
			$output .= "echo \"Could not detect i2c bus from any device, run again $0. Did you forget to connect the device?\";\n";
			$output .= "echo \"Modules supported: $modules_str\";\n";
			$output .= "exit;";
			print $output;
		}
		case 1 {
			# Starting script
			&build_script;
		}
		else {
			$output .= "humm, I got too many busses, please connect or plug just a hardware peer time!\n";
			$output .= "Read a note inside the script!\n";
			$output .= "exit;";
			print $output;
		}
	}

	# Reading BUS from temporary file
	open (FILE, $file_bus);
	while (<FILE>) {
		$businfo = "$_";
		chomp($businfo);
	}
	close FILE;

	system("i2cdetect -y $businfo > $file_i2c_tmp 2> /dev/null");
	system("awk \'NR==7\' $file_i2c_tmp | cut -d \' \' -f 2 > $file_addr");

	# Reading BUS from temporary file
	open (FILE, $file_addr);
	while (<FILE>) {
		$addrinfo = "$_";
		chomp($addrinfo);
	}

	if($addrinfo eq "--") {
		print "\necho \"**** Failed to recognize bus address!\n**** Please connect your device *with eeprom* and try to run $0 tool again, aborted!\";\n";
		print "exit;";
	}

	# Double check
	$bkp_eeprom  = "\n\ni2cdetect -y $businfo > $file_i2c_tmp 2> /dev/null;\n";
	$bkp_eeprom .= "BUSCHECK=\`awk \'NR==7\' $file_i2c_tmp | cut -d \' \' -f 2\`;\n";
	$bkp_eeprom .= "rm -f $file_i2c_tmp;\n";
	$bkp_eeprom .= "if [ \"\$BUSCHECK\" == \"--\" ]; then\n";
	$bkp_eeprom .= "\t echo \"Aborting script.. I cannot make backup of your current eeprom.. It's not safe to continue!\";\n";
	$bkp_eeprom .= "\t exit;\n";
	$bkp_eeprom .= "fi\n\n";

	# Backup
	$bkp_eeprom .= "\nDATE=\`/bin/date +%Y%m%d_%I%M%S\`\n";
	$bkp_eeprom .= "echo \"\nMaking backup of current eeprom - dir [$eeprom_backup_dir/eeprom-\$DATE]\";\n";
	$bkp_eeprom .= "mkdir -p $eeprom_backup_dir\n";
	$bkp_eeprom .= "\necho \"\n--EEPROM DUMP START HERE--\n\" > $eeprom_backup_dir/eeprom-\$DATE\n";
	$bkp_eeprom .= "i2cdump -y $businfo 0x$addrinfo >> $eeprom_backup_dir/eeprom-\$DATE 2> /dev/null\n";
	$bkp_eeprom .= "if [ ! \"\$\?\" = \"0\" ]; then\n";
	$bkp_eeprom .= "\t echo \"Aborting script.. I cannot make backup of your current eeprom.. It's not safe to continue!\";\n";
	$bkp_eeprom .= "\t exit;";
	$bkp_eeprom .= "\nfi\n";
	$bkp_eeprom .= "\necho \"\n--DMESG START HERE--\n\" >> $eeprom_backup_dir/eeprom-\$DATE\n";
	$bkp_eeprom .= "\ndmesg >> $eeprom_backup_dir/eeprom-\$DATE\n";

	print $bkp_eeprom;

	close FILE;
}

sub print_i2c
{
	$cmd = "cat $argv | egrep \"eeprom [0-9a-f]\"| sed -e \"s/.*eeprom/eeprom/\" | cut -d ' ' -f 3-22 > $file_eeprom";
	system($cmd);

	open (INPUT, "$file_eeprom") or die "Can't open data file: $!";

	# Reading dump
	@eeprom = "";
	my $eeprom_pos = 0;
	while (!eof(INPUT)) {
		read(INPUT, $fc, 2);
		$eeprom[$eeprom_pos] = "0x$fc";
		seek(INPUT, tell(INPUT) + 1, 0);
		$eeprom_pos++;
		$NR_EEPROM_REGS++;
	}
	close INPUT;

	if ($NR_EEPROM_REGS == 0) {
		print "\necho \"**** Failed to recognize any dump in: $argv! Make sure that you have the right dump file before run again $0 tool, aborted!\";\n";
		print "exit;";
	}

	print "\n\necho \"\033[1;31m\n[DO NOT REMOVE YOUR DEVICE UNTIL THE UPDATE IS FINISHED]\033[0m\";\n";
	print "echo \"Press ENTER to start\";\n";
	print "read\n";

	for ($i=0; $i < $NR_EEPROM_REGS; $i++) {
		printf("i2cset -y $businfo 0x$addrinfo 0x%02x $eeprom[$i] b\n", $i);
	}

	printf("\necho \"\nDone! Remove and re-insert your device to see if it will be properly detected again. :-)\n\";\n");
}

################
# Main         #
################

&check_user;

if (@ARGV <= 0) {

	my $em28xx_note = "\n\033[1;31m\nNOTES\033[0m:\n\t Not tested against newer em28xx chipsets like the em2874 and em2884\n";

	print "\033[1;31m\nWARNING\033[0m:\n \t This script can *\033[1;31mDAMAGE\033[0m* your board, if you are not sure how to use it, *DO NOT* run it\n";
	print "\t Current modules supported: $modules_str *\033[1;31mONLY\033[0m*";
	print $em28xx_note;
	print "\t If you have *any doubt*, \033[1;31mBEFORE run it\033[0m contact people from: linux-media\@vger.kernel.org\n";

	print "\nUsage:\n";
	print "\tshell>perl $0 ./dmesg-dump-eeprom > eeprom_script.sh\n";
	print "\tshell>sh ./eeprom_script.sh\n\n";
	exit();
}

if (! -e $argv) {
	printf("No such file: $argv\n");
	exit();
}

# Calling sub routines
&get_bus_and_addr;
&print_i2c;

# Removing tmp files
system("rm -f $file_bus");
system("rm -f $file_addr");
system("rm -f $file_i2c_tmp");
system("rm -f $file_eeprom");
