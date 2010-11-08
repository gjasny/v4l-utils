#!/usr/bin/perl
use strict;
use File::Find;

my @ir_files = (
	"drivers/media/dvb/dvb-usb/a800.c",
	"drivers/media/dvb/dvb-usb/af9005-remote.c",
	"drivers/media/dvb/dvb-usb/af9015.c",
	"drivers/media/dvb/dvb-usb/af9015.h",
	"drivers/media/dvb/dvb-usb/anysee.c",
	"drivers/media/dvb/dvb-usb/cinergyT2-core.c",
	"drivers/media/dvb/dvb-usb/cxusb.c",
	"drivers/media/dvb/dvb-usb/dibusb-common.c",
	"drivers/media/dvb/dvb-usb/digitv.c",
	"drivers/media/dvb/dvb-usb/dtt200u.c",
	"drivers/media/dvb/dvb-usb/dvb-usb-remote.c",
	"drivers/media/dvb/dvb-usb/dvb-usb.h",
	"drivers/media/dvb/dvb-usb/dw2102.c",
	"drivers/media/dvb/dvb-usb/m920x.c",
	"drivers/media/dvb/dvb-usb/nova-t-usb2.c",
	"drivers/media/dvb/dvb-usb/opera1.c",
	"drivers/media/dvb/dvb-usb/vp702x.c",
	"drivers/media/dvb/dvb-usb/vp7045.c",
);

my $debug = 1;
my $dir="rc_keymaps";
my $deftype = "UNKNOWN";

my $keyname="";
my $out;
my $read=0;
my $type = $deftype;
my $check_type = 0;
my $name;
my $warn;
my $warn_all;

my $kernel_dir = shift or die "Need a file name to proceed.";

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

	$warn = 0;

	printf "processing file $filename\n" if ($debug);
	open IN, "<$filename" or die "couldn't find $filename";
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

	$warn_all += $warn;
}

sub parse_dir()
{
	my $file = $File::Find::name;

	return if ($file =~ m/^\./);

	return if (! ($file =~ m/\.c$/));

	parse_file $file;
}

# Main logic
#

find({wanted => \&parse_dir, no_chdir => 1}, "$kernel_dir/drivers/media/IR/keymaps");

foreach my $file (@ir_files) {
	parse_file "$kernel_dir/$file";
}

printf STDERR "WARNING: there are %d tables not defined at rc_maps.h\n", $warn_all if ($warn_all);
