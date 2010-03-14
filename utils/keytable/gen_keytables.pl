#!/usr/bin/perl
use strict;

my $keyname="";
my $debug=0;
my $out;
my $read=0;

sub flush()
{
	return if (!$keyname || !$out);
	open OUT, ">keycodes/$keyname";
	print OUT $out;
	close OUT;

	$keyname = "";
	$out = "";
}

while (<>) {
	if (m/struct\s+(dvb_usb_rc_key|ir_scancode)\s+(\w[\w\d_]+)/) {
		flush();

		$keyname = $2;
		$keyname =~ s/^ir_codes_//;
		$read = 1;
		next;
	}
	if (m/IR_TABLE\(\s*([^\,\s]+)\s*,\s*([^\,\s]+)\s*,\s*([^\,\s]+)\s*\)/) {
		my $name = $1;
		my $type = $2;
		$type =~ s/IR_TYPE_//;
		$out = "# table $name, type: $type\n$out";
		$read = 0;

		flush();
	}
	if ($read) {
		if (m/(0x[\dA-Fa-f]+).*(KEY_[^\s\,\}]+)/) {
			$out .= "$1 $2\n";
			next;
		}
		if (m/\}/) {
			$read = 0;
		}
	}
}

flush();
