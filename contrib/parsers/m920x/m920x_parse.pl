#!/usr/bin/perl
#
# This ULi M920x specific script processes usbsnoop log files (as well as those which have been parsed by mrec's parser.pl utility).
# Taken from http://www.linuxtv.org/wiki/index.php/ULi_M920x_parse

use strict;
use warnings;
use Getopt::Std;

my $line;
my $mode;
my $input;
my %opt;

sub expand_string {
	my @arr = ();
	my ($str) = @_;

	if (length($str) == 8) {
		push(@arr, substr($str, 0, 2));
		push(@arr, substr($str, 2, 2));
		push(@arr, substr($str, 4, 2));
		push(@arr, substr($str, 6, 2));
	}elsif(length($str) == 4) {
		push(@arr, substr($str, 0, 2));
		push(@arr, substr($str, 2, 2));
	}elsif(length($str) == 2) {
		push(@arr, $str);
	}elsif(length($str) == 1) {
		return;
	}
	return @arr;
}

sub expand_string_long {
	my @bytes = ();
	my (@str) = @_;

	foreach(@str) {
		#@arr = expand_string($_);
		#foreach(@arr){
		#	push(@bytes, $_);
		#}
		@bytes = ( @bytes, expand_string($_) );
	}

	return @bytes;
}

sub print_array_bytes {
	my (@str) = @_;

	foreach(expand_string_long(@str)){
		print "$_ ";
	}
}

sub print_bytes {
	my ($str) = @_;

	print_array_bytes(split(/ /, $str));
}

sub check {
	my ($cmd, @bytes) = @_;
	my @cmp;
	my $i;
	#print "cmd <$cmd>\n";
	my $fail = 0;

	@cmp = split(/ /, $cmd);
	for ($i = 0; $i < scalar(@cmp); $i++) {
		#print "check $bytes[$i] vs $cmp[$i]\n";
		if ($cmp[$i] eq "-1") {
			next;
		}

		if (not($bytes[$i] =~ m/$cmp[$i]/)) {
			$fail = 1;
			print "($bytes[$i]!=$cmp[$i], $i)";
		}
	}
	if ($fail) {
		print "\n";
		print_array_bytes(@bytes);
		print "\n$cmd\n";
	}
}

sub get_line {
	my ($cmd) = @_; # xxx: could be more flexible
	my @ret;
	my @cmp;
	my $i;
	my $foo;

	again:
	while($line = <STDIN>) {
		#001295:  OUT: 000002 ms 135775 ms 40 23 c0 00 80 00 00 00 >>>
		if($input eq "us" && $line =~ m/\S+:  \S+: \S+ ms \S+ ms ([a-fA-F0-9 ]+)/) {
			@ret = split(/ /, $1); $foo = $1;
			@ret[2,3,4,5,6,7] = @ret[3,2,5,4,7,6];
			last;
		}

		#40 23 c0 00 80 00 00 00 >>>
		if($input eq "us2" && $line =~ m/([a-fA-F0-9 ]+)/) {
			@ret = split(/ /, $1); $foo = $1;
			@ret[2,3,4,5,6,7] = @ret[3,2,5,4,7,6];
			last;
		}

		if($input eq "um" && $line =~ m/\S+ \S+ \S+ \S+ s ([a-fA-F0-9 ]+)/) {
			@ret = expand_string_long(split(/ /, $1)); $foo = $1;
			last;
		}
	}
	@cmp = split(/ /, $cmd);
	for ($i = 0; $i < scalar(@cmp); $i++) {
		if ($cmp[$i] eq "-1") {
			next;
		}

		if (not($cmp[$i] eq $ret[$i])) {
			#print "fail\n";
			goto again;
		}
	}

	return @ret;
}

sub us_get_write {
	#print "<$line>\n";
	if(($input eq "us" || $input eq "us2") && $line =~ m/>>>\s+([a-fA-F0-9 ]+)/) {
		return split(/ /, $1);
	}
	if($input eq "um") {
		if($line =~ m/\S+ \S+ \S+ \S+ \S+ \S+ \S+ \S+ \S+ \S+ \S+ = ([a-fA-F0-9 ]+)/) {
			#print "read match $line\n";
			return expand_string_long(split(/ /, $1));
		}
	}
}

sub get_read {
	#print "<$line>\n";
	if(($input eq "us" || $input eq "us2") && $line =~ m/<<<\s+([a-fA-F0-9 ]+)/) {
		return split(/ /, $1);
	}
	if($input eq "um") {
		while($line = <STDIN>) {
			if($line =~ m/\S+ \S+ \S+ \S+ \S+ \S+ = ([a-fA-F0-9 ]+)/) {
				return expand_string_long(split(/ /, $1));
			}

		}
	}
}

sub usage {
	print STDERR << "EOF";

	-i	um (usbmon)
		us (usb snoop)
		us2 (usb snoop as produced by parse-sniffusb2.pl)
		sp (snoopy pro)

	-m	fw (extract firmware)
		i2c (show i2c traffic)
EOF
exit;
}

getopts("m:i:", \%opt ) or usage();

$mode = $opt{m};
$input = $opt{i};

if ($input ne "um" && $input ne "us" && $input ne "us2" && $input ne "sp") {
	usage();
}

if ($mode ne "fw" && $mode ne "i2c") {
	usage();
}

my @bytes;

if ($mode eq "fw") {
	open(OUT, ">", "fw") || die "Can't open fw";

	while(@bytes = get_line("-1")) {
		if(scalar(@bytes) <= 1) {
			last;
		}

		my $is_fw_msg = $bytes[0] eq "40" && $bytes[1] eq "30";
		my $len = hex($bytes[6] . $bytes[7]);
		if (!$is_fw_msg || $len < 32) {
			next;
		}

		my @fw_bytes = us_get_write();
		if ($len != scalar(@fw_bytes)) {
			#note: usbmon will not log bulk writes longer than 32 bytes by default
			print "bulk size doesn't match! Check usbmon.\n";
			print $len . " != " . scalar(@fw_bytes) . "\n";
			exit(0);
		}
		print OUT pack("v", hex($bytes[2] . $bytes[3]));
		print OUT pack("v", hex($bytes[4] . $bytes[5]));
		print OUT pack("v", scalar(@fw_bytes));

		foreach(@fw_bytes) {
			print OUT pack("C", hex($_));
		}
	}
	exit(1);
}

while(@bytes = get_line("-1")) {
	if(scalar(@bytes) <= 1) {
		last;
	}

	my $master_line = $. - 1;

	if ($bytes[0] eq "40" && $bytes[1] eq "23") {

		if ($bytes[4] eq "80" || $bytes[4] eq "00") {
			my $multibyte = 0;
			my $addr;

			$addr = $bytes[2];

			printf "%06d: ", $master_line;
			print "addr $addr ";

			if (hex($addr) & 0x1) {
				print "Invalid address\n";
			}

			@bytes = get_line("40 23");

			my $reg = $bytes[2];
			if ($bytes[4] eq "80") {
				$multibyte = 1;
			} else {
				@bytes = get_line("40 23");
			}
			#if ($bytes[4] ne "40") {
			#	print "(missing 40)";
			#}

			if ($bytes[4] eq "80") {
				if ($multibyte == 0) {
					my $raddr = sprintf("%02x", hex($addr) | 0x1);

					check("40 23 $raddr 00 80 00 00 00", @bytes);

					@bytes = get_line("c0 23");
					print "reg $reg = ";
				} else {
					print "$reg = ";
					@bytes = get_line("c0 23");
					while ($bytes[4] eq "21") {
						check("c0 23 00 00 21 00 -1 -1", @bytes);

						@bytes = get_read();
						print_array_bytes(@bytes);

						@bytes = get_line("c0 23");
					}
				}

				check("c0 23 -1 00 60 00 -1 -1", @bytes);

				@bytes = get_read();
				print_array_bytes(@bytes);
				print "read\n";

			} else {
				check("40 23 -1 00 4|00 00 00 00", @bytes);
				print "reg $reg = $bytes[2]";

				while ($bytes[4] ne "40") {
					@bytes = get_line("40 23");
					check("40 23 -1 00 4|00 00 00 00", @bytes);
					print " $bytes[2]";
				}
				print "\n";
			}
		}
	}
}
