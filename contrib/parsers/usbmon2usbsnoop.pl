#!/usr/bin/perl
#
# This perl script converts output from usbmon to a format (usbsnoop's log format) that is compatible with usbreplay.
# Taken from http://www.linuxtv.org/wiki/index.php/Usbmon2usbsnoop

sub print_bytes{
	my($str) = @_;

	@str_1 = split(/ /, $str);

	foreach(@str_1){
		if (length($_) == 8) {
			print substr($_, 0, 2) . " " . substr($_, 2, 2) . " " . substr($_, 4, 2) . " " . substr($_, 6, 2);
		}elsif(length($_) == 4) {
			print substr($_, 2, 2) . " " . substr($_, 0, 2);
		}elsif(length($_) == 2) {
			print $_;
		}elsif(length($_) == 1) {
			next;
		}
		print " ";
	}
}


$i = 0;
while($line = <STDIN>) {
	$i++;

	if($line =~ m/\S+ \S+ \S+ \S+ \S+ (.+) \S+ </) {
		printf "%06d:  OUT: %06d ms %06d ms ", $i, 1, $i;
		print_bytes($1);
		print "<<< ";
		$line = <STDIN>;
		$i++;
		if($line =~ m/\S+ \S+ \S+ \S+ [a-fA-F0-9 ]+ = ([a-fA-F0-9 ]+)/) {
			print_bytes($1);
			#print "\n";
			#print " $1\n";
		}
		print "\n";
	}elsif($line =~ m/\S+ \S+ \S+ \S+ ([a-fA-F0-9 ]+) [a-fA-F0-9]+ = ([a-fA-F0-9 ]+)/) {
		printf "%06d:  OUT: %06d ms %06d ms ", $i, 1, $i;
		print_bytes($1);
		print ">>> ";
		print_bytes($2);
		print "\n";
	}elsif($line =~ m/\S+ \S+ \S+ \S+ s (.+)/) {
		printf "%06d:  OUT: %06d ms %06d ms ", $i, 1, $i;
		print_bytes($1);
		print ">>>\n";
	}
}
