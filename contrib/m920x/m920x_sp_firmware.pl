#!/usr/bin/perl
#
# This script converts the output from SnoopyPro's log window into a M920x driver compatible binary form.
# Taken from http://www.linuxtv.org/wiki/index.php/ULi_M920x_sp_firmware

sub get_line {


	while($line = <STDIN>) {
		if($line =~ m/\S+ \S+ \S+ \S+ \S+ \S+ \S+ \S+ \S+ \S+ \S+/) {
			#print "returning line $line";
			return $line;
		}else{
			#print "not matched $line\n";
		}
	}

	#exit;
}

sub get_line_read {

	while($line = <STDIN>) {

		#d5748560 78669980 C Ci:004:00 0 1 = 50
		#d3ea8260 44357030 C Ci:004:00 0 1 = bc
		if($line =~ m/\S+ \S+ C \S+ \S+ (\S+) = (\S+)/) {
			return $2;
		}else{
			#print "not matched $line\n";
		}
	}

}

$linenum = 0;
sub get_tf_data {
	$full = "";

	$line = <STDIN>; $linenum++;
	while($line =~ m/^\S\S\S\S: (.+)/) {
		$full .= $1;
		#print "$1";
		$line = <STDIN>; $linenum++;
	}
	return $full;
}

open(out,">fw") || die "Can't open fw";

sub write_bytes {
	my($str) = @_;

	#print "ds $str sd\n";

	@bytes = split(/ /, $str);
	foreach(@bytes){
		#print "$_\n";
		print out pack("C", hex($_));
	}

	#exit(1);
}

sub to_words {
	my($str) = @_;

	print "ds $str sd\n";

	@bytes = split(/\S+ \S+/, $str);
	foreach(@bytes){
		print "$_\n";
		print out pack("v", hex($_));
	}

	exit(1);
}

while($line = <STDIN>) { $linenum++;

	if($line =~ m/SetupPacket:/) {
		$setup_linenum = $linenum;
		$setup = get_tf_data();

		#to_words($setup);

		#write_bytes($setup);

		#print "$line\n";

		while($line = <STDIN>) { $linenum++;
			if($line =~ m/TransferBuffer: 0x00000040/) {
				#print "$setup, $setup_linenum\n";

				@bytes = split(/ /, $setup);
				print out pack("v", hex($bytes[3] . $bytes[2]));
				print out pack("v", hex($bytes[5] . $bytes[4]));
				print out pack("v", hex("0x40"));

				$lid = get_tf_data();
				write_bytes($lid);
				#print $lid;
				#print "\n";

				last;
			}elsif($line =~ m/No TransferBuffer/) {
				last;
			}elsif($line =~ m/TransferBuffer:/) {
				last;
			}

		}
	}

}
