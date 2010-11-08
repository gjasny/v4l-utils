#!/usr/bin/perl
use strict;

my $dir="rc_keymaps";
my $deftype = "UNKNOWN";

my $keyname="";
my $debug=0;
my $out;
my $read=0;
my $type = $deftype;
my $check_type = 0;
my $name;
my $warn;

my $file = shift or die "Need a file name to proceed.";
sub flush()
{
	return if (!$keyname || !$out);
	print "Creating $dir/$keyname\n";
	open OUT, ">$dir/$keyname";
	print OUT "# table $keyname, type: $type\n";
	print OUT $out;
	close OUT;

	if (!$name) {
		$warn++;
	}

	$keyname = "";
	$out = "";
	$type = $deftype;
	$name = "";
}

sub parse_file($)
{
	my $filename = shift;

	open IN, "<$filename";
	while (<IN>) {
		if (m/struct\s+ir_scancode\s+(\w[\w\d_]+)/) {
			flush();

			$keyname = $1;
			$keyname =~ s/^ir_codes_//;
			$keyname =~ s/_table$//;
			$read = 1;
			next;
		}
		if (m/struct\s+rc_keymap.*=\s+{/) {
			$check_type = 1;
			next;
		}
		if (m/\.name\s*=\s*(RC_MAP_[^\s\,]+)/) {
			$name = $1;
		}

		if ($check_type) {
			if (m/^\s*}/) {
				$check_type = 0;
				next;
			}
			if (m/IR_TYPE_([\w\d_]+)/) {
				$type = $1;
			}
			next;
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
	close IN;

	flush();

	printf STDERR "WARNING: keyboard name not found on %d tables at file $filename\n", $warn if ($warn);
}

parse_file $file;
