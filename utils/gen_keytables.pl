#!/usr/bin/perl
use strict;

my $keyname="";
my $debug=0;

while (<>) {
	if (m/struct\s+(dvb_usb_rc_key|ir_scancode)\s+(\w[\w\d_]+)/) {
		$keyname = $2;
		$keyname =~ s/^ir_codes_//;

		print "Generating keycodes/$keyname\n" if $debug;
		open OUT, ">keycodes/$keyname";
		next;
	}
	if ($keyname ne "") {
		if (m/(0x[\dA-Fa-f]+).*(KEY_[^\s\,\}]+)/) {
			printf OUT "%s %s\n",$1, $2;
			next;
		}
		if (m/\}/) {
			close OUT;
			$keyname="";
			next;
		}
	}
}
