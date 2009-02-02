#!/usr/bin/perl
#
# Author: Franklin Meng <fmeng2002@yahoo.com>
# Parser for USB snoops captured from SniffUSB 2.0.
#

use strict;
use warnings;
use Data::Dumper;

foreach my $curfile (@ARGV) {
	parsefile($curfile);
	#we can only process 1 file
	exit;
}

sub parsefile {
	my $curfile = shift;
	my $SetupPacket ='';
	my $preS = '';
	my $TransferBuffer ='';
	my $preT = '';
	my $Direction ='';
	my $preD = '';
	my @tmpsplit;
	my $t=0;
	my $s=0;

	open(FD, $curfile) || die("Error: $!\n");

	while(<FD>) {
		chomp;
		if($t==1 && /^\s+\d{8}/) {
#			print $_ . "\n";
			@tmpsplit = split(/\:\s/,$_);
			$TransferBuffer = $TransferBuffer . $tmpsplit[1] . " ";
		} elsif($s==1 && /^\s+\d{8}/) {
#			print $_ . "\n";
			@tmpsplit = split(/\:\s/,$_);
			$SetupPacket = $SetupPacket . $tmpsplit[1] ;
		} else {
			$t=0;
			$s=0;
		}
		if(/[<>]{3}/){
			#print out last packet if valid
			if($SetupPacket) {
				if($preT) {
					print "$SetupPacket $preD $preT\n";

				} else {
					print "$SetupPacket $Direction $TransferBuffer\n";
				}
			}
#			print "$SetupPacket $Direction $TransferBuffer\n";
			#clear variables
			$preT = $TransferBuffer;
			$TransferBuffer = '';
			$preS = $SetupPacket;
			$SetupPacket = '';
			$preD = $Direction;
			$t = 0;
			$s = 0;
			# get direction
			@tmpsplit = split(/\s+/, $_);
			$Direction = $tmpsplit[2];
#			print $_ . "\n";
		} elsif(/TransferBufferMDL/) {
			$t = 1
		} elsif(/SetupPacket/) {
			$s = 1;
		}
	}
	#print last packet
#	print "$SetupPacket $Direction $TransferBuffer\n";
	if($SetupPacket) {
		if($preT) {
			print "$SetupPacket $preD $preT\n";
		} else {
			print "$SetupPacket $Direction $TransferBuffer\n";
		}
	}
}

