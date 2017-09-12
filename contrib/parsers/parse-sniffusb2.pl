#!/usr/bin/perl
#
# Author: Franklin Meng <fmeng2002@yahoo.com>
# Parser for USB snoops captured from SniffUSB 2.0.
#

use strict;
use warnings;
use Data::Dumper;

my $urbhash;

foreach my $curfile (@ARGV) {
	parsefile($curfile);
	#we can only process 1 file
	exit;
}

sub parsefile {
	my $curfile = shift;

	my $s1 = 0;
	my $t1 = 0;
	my $tmp1 = "";
	my $printurb = 0;
	my $cururb = 0;

	open(FD, $curfile) || die("Error: $!\n");

	while(<FD>) {
		chomp;
		if (/URB\s+(\d+)\s+going\s+down/) {
			# print previous urb if available
			if($printurb != 0 && exists($urbhash->{$printurb}->{'SetupPacket'})) {
				print "$urbhash->{$printurb}->{'SetupPacket'} $urbhash->{$printurb}->{'Direction'} $urbhash->{$printurb}->{'TransferBufferMDL'}\n";
#				# delete urb information
#				delete($urbhash->{$printurb});
			}
			# delete urb information
			delete($urbhash->{$printurb});
			$printurb = 0;  #reset here
			$tmp1 = "";
			$s1 = 0;
			$t1 = 0;
			# store next urb info here
			$cururb = $1;
			$urbhash->{$1} = undef;
			next;
		} elsif (/URB\s+(\d+)\s+coming\s+back/) {
			# print previous urb if available
			if($printurb != 0 && exists($urbhash->{$printurb}->{'SetupPacket'})) {
				print "$urbhash->{$printurb}->{'SetupPacket'} $urbhash->{$printurb}->{'Direction'} $urbhash->{$printurb}->{'TransferBufferMDL'}\n";
#				# delete urb information
#				delete($urbhash->{$printurb});
			}
			# delete urb information
			delete($urbhash->{$printurb});
			$printurb = 0;  #reset here
			$tmp1 = "";
			$s1 = 0;
			$t1 = 0;
			# flag next urb for print out
			if(exists($urbhash->{$1})) {
				$printurb = $1;
			} else {
				warn "Error: cannot match urb!!\n";
			}
			$cururb = $1;
			next;
		} elsif (/\-{2}\s+(URB_FUN.+)\:/) {  # store urb function (used for debugging)
			if(!exists($urbhash->{$cururb}->{'Function'})) {
				$urbhash->{$cururb}->{'Function'} = $1;
			}
			next;
		} elsif (/USBD_TRANSFER_DIRECTION_IN/) {  #store in direction
			#check if we already stored a value
			if(!exists($urbhash->{$cururb}->{'Direction'})) {
				$urbhash->{$cururb}->{'Direction'} = "<<<";
			}
			next;
		} elsif (/USBD_TRANSFER_DIRECTION_OUT/) { #store out direction
			#check if we already stored a value
			if(!exists($urbhash->{$cururb}->{'Direction'})) {
				$urbhash->{$cururb}->{'Direction'} = ">>>";
			}
			next;
		} elsif (/TransferBufferMDL\s+=\s+/) {  #flag data packet
			$t1 = 1;
			next;
		} elsif (/SetupPacket\s+=/) {  #flag setup packet
			$s1 = 1;
			next;
		} elsif (/(.+\s+\=|ms\])/ && ($s1 || $t1)) { #save data packet and reset
			if($s1 && ($tmp1 ne "")) {
				$tmp1 =~ s/^\s+//;
				$urbhash->{$cururb}->{'SetupPacket'} = $tmp1;
			} elsif($t1 && ($tmp1 ne "")) {
				$tmp1 =~ s/^\s+//;
				$urbhash->{$cururb}->{'TransferBufferMDL'} = $tmp1
			}
			$tmp1 = "";
			$s1 = 0;
			$t1 = 0;
			next;
		} elsif (/^\s+\d+\:(.+)/ && ($s1 || $t1)) { #capture packet
			$tmp1 = $tmp1 . $1;
		}

	}

	# print remaining URB
	if($printurb != 0 && exists($urbhash->{$printurb}->{'SetupPacket'})) {
		print "$urbhash->{$printurb}->{'SetupPacket'} $urbhash->{$printurb}->{'Direction'} $urbhash->{$printurb}->{'TransferBufferMDL'}\n";
#		# delete urb information
#		delete($urbhash->{$printurb});
	}
	# delete urb information
	delete($urbhash->{$printurb});

	# Maybe we should warn for the URB's that did not have matches?

	# print out stuff remaining in the hash for debugging
	#print Dumper($urbhash);
}

