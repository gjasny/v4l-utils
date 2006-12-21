#!/usr/bin/perl
use strict;

my $keyname="";
while (<>) {
#IR_KEYTAB_TYPE ir_codes_proteus_2309[IR_KEYTAB_SIZE] = {
	if (m/IR_KEYTAB_TYPE\s+(\w[\w\d_]+)/) {
		$keyname = $1;
		$keyname =~ s/^ir_codes_//;

		print "Generating keycodes/$keyname\n";
		open OUT, ">keycodes/$keyname";
		next;
	}
	if ($keyname ne "") {
		if (m/(0x[\d\w]+).*(KEY_[^\s\,]+)/) {
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
