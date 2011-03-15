#!/usr/bin/perl

#   Copyright (C) 2011 Mauro Carvalho Chehab <mchehab@redhat.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, version 2 of the License.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
# tcpdump parser imported from TcpDumpLog.pm, in order to improve performance,
# reduce memory footprint and allow doing realtime parsing.
# The TcpDumpLog.pm is copyrighted by Brendan Gregg.
#
# Currently, the program is known to work with the frame formats as parsed by
# libpcap 1.0.0 and 1.1.1, and provided that usbmon is using the mmaped
# header: USB with padded Linux header (LINKTYPE_USB_LINUX_MMAPPED)

use strict;
use Getopt::Long;
use Pod::Usage;

# Debug levels:
#	1 - frame request and frame response
#	2 - parsed frames
#	4 - raw data
my $debug = 0;

my $man = 0;
my $help = 0;
GetOptions('debug=i' => \$debug,
	   'help|?' => \$help,
	    man => \$man
	  ) or pod2usage(2);
pod2usage(1) if $help;
pod2usage(-exitstatus => 0, -verbose => 2) if $man;

my $filename = shift;

#
# tcpdump code imported from Tcpdumplog.pm
# Copyright (c) 2003 Brendan Gregg. All rights reserved.  This
#     library is free software; you can redistribute it and/or
#     modify it under the same terms as Perl itself
# Perl is dual-licensed between GPL and Artistic license, so we've opted
# to make this utility as GPLv2.
#
# This is basically the code from TcpDumpLog.pm. The only change is that
# instead of implementing a read() method, it was broken into two routines:
# get_header() and get_packet(). Also, only the used sub-routines were
# imported.
#

sub new {
	my $proto = shift;
	my $class = ref($proto) || $proto;
	my $self = {};

	my $bits = shift;
	my $skip = shift;

	$self->{major} = undef;
	$self->{minor} = undef;
	$self->{zoneoffset} = undef;
	$self->{accuracy} = undef;
	$self->{dumplength} = undef;
	$self->{linktype} = undef;
	$self->{bigendian} = undef;
	$self->{data} = [];
	$self->{length_orig} = [];
	$self->{length_inc} = [];
	$self->{drops} = [];
	$self->{seconds} = [];
	$self->{msecs} = [];
	$self->{count} = 0;
	$self->{sizeint} = length(pack("I",0));

	if (defined $bits && $bits == 64) {
		$self->{bits} = 64;
	} elsif (defined $bits && $bits == 32) {
		$self->{bits} = 32;
	} else {
		$self->{bits} = 0;	# Use native OS bits
	}

	if (defined $skip && $skip > 0) {
		$self->{skip} = $skip;
	}

	bless($self,$class);
	return $self;
}

sub get_header {
        my $self = shift;
	my $fh = shift;

	my ($header, $length, $major, $minor, $zoneoffset, $accuracy);
	my ($dumplength, $linktype, $version, $ident, $rest);

	$length = read($fh, $header, 24);
	die "ERROR: Can't read from tcpdump log\n" if $length < 24;

	### Check file really is a tcpdump file
	($ident, $rest) = unpack('a4a20', $header);

	### Find out what type of tcpdump file it is
	if ($ident =~ /^\241\262\303\324/) {
		#
		#  Standard format big endian, header "a1b2c3d4"
		#  Seen from:
		#	Solaris tcpdump
		#	Solaris Ethereal "libpcap" format
		#
		$self->{style} = "standard1";
		$self->{bigendian} = 1;
		($ident,$major,$minor,$zoneoffset,$accuracy,$dumplength,
		 $linktype) = unpack('a4nnNNNN',$header);
	}
	if ($ident =~ /^\324\303\262\241/) {
		#
		#  Standard format little endian, header "d4c3b2a1"
		#  Seen from:
		#	Windows Ethereal "libpcap" format
		#
		$self->{style} = "standard2";
		$self->{bigendian} = 0;
		($ident,$major,$minor,$zoneoffset,$accuracy,$dumplength,
		 $linktype) = unpack('a4vvVVVV',$header);
	}
	if ($ident =~ /^\241\262\315\064/) {
		#
		#  Modified format big endian, header "a1b2cd34"
		#  Seen from:
		#	Solaris Ethereal "modified" format
		#
		$self->{style} = "modified1";
		$self->{bigendian} = 1;
		($ident,$major,$minor,$zoneoffset,$accuracy,$dumplength,
		 $linktype) = unpack('a4nnNNNN',$header);
	}
	if ($ident =~ /^\064\315\262\241/) {
		#
		#  Modified format little endian, header "cd34a1b2"
		#  Seen from:
		#	Red Hat tcpdump
		#	Windows Ethereal "modified" format
		#
		$self->{style} = "modified2";
		$self->{bigendian} = 0;
		($ident,$major,$minor,$zoneoffset,$accuracy,$dumplength,
		 $linktype) = unpack('a4vvVVVV',$header);
	}

	### Store values
	$self->{version} = $version;
	$self->{major} = $major;
	$self->{minor} = $minor;
	$self->{zoneoffset} = $zoneoffset;
	$self->{accuracy} = $accuracy;
	$self->{dumplength} = $dumplength;
	$self->{linktype} = $linktype;
}

sub get_packet {
        my $self = shift;
	my $fh = shift;

	my ($frame_data, $frame_drops, $frame_length_inc, $frame_length_orig);
	my ($frame_msecs, $frame_seconds, $header_rec, $length, $more);

	if ($self->{bits} == 64) {
		#
		#  64-bit timestamps, quads
		#

		### Fetch record header
		$length = read($fh, $header_rec, 24);

		### Quit loop if at end of file
		return -1 if $length < 24;

		### Unpack header
		($frame_seconds, $frame_msecs, $frame_length_inc,
			$frame_length_orig) = unpack('QQLL',$header_rec);
	} elsif ($self->{bits} == 32) {
		#
		#  32-bit timestamps, big-endian
		#

		### Fetch record header
		$length = read($fh, $header_rec, 16);

		### Quit loop if at end of file
		return -1 if $length < 16;

		### Unpack header
		if ($self->{bigendian}) {
			($frame_seconds, $frame_msecs,
				$frame_length_inc, $frame_length_orig)
				= unpack('NNNN', $header_rec);
		} else {
			($frame_seconds, $frame_msecs,
				$frame_length_inc, $frame_length_orig)
				= unpack('VVVV', $header_rec);
		}
	} else {
		#
		#  Default to OS native timestamps
		#

		### Fetch record header
		$length = read($fh, $header_rec,
			($self->{sizeint} * 2 + 8) );

		### Quit loop if at end of file
		return -1 if $length < ($self->{sizeint} * 2 + 8);

		### Unpack header
		if ($self->{sizeint} == 4) {	# 32-bit
			if ($self->{bigendian}) {
				($frame_seconds, $frame_msecs,
					$frame_length_inc, $frame_length_orig)
					= unpack('NNNN', $header_rec);
			} else {
				($frame_seconds, $frame_msecs,
					$frame_length_inc, $frame_length_orig)
					= unpack('VVVV', $header_rec);
			}
		} else {			# 64-bit?
			if ($self->{bigendian}) {
				($frame_seconds, $frame_msecs,
					$frame_length_inc, $frame_length_orig)
					= unpack('IINN', $header_rec);
			} else {
				($frame_seconds,$frame_msecs,
					$frame_length_inc, $frame_length_orig)
					= unpack('IIVV', $header_rec);
			}
		}
	}

	### Fetch extra info if in modified format
	if ($self->{style} =~ /^modified/) {
		$length = read($fh, $more, 8);
	}

	### Check for skip bytes
	if (defined $self->{skip}) {
		$length = read($fh, $more, $self->{skip});
	}

	### Fetch the data
	$length = read($fh, $frame_data, $frame_length_inc);

	$frame_drops = $frame_length_orig - $frame_length_inc;

	### Store values in memory
	$self->{data} = $frame_data;
	$self->{length_orig} = $frame_length_orig;
	$self->{length_inc} = $frame_length_inc;
	$self->{drops} = $frame_drops;
	$self->{seconds} = $frame_seconds;
	$self->{msecs} = $frame_msecs;
	$self->{count}++;

	return 0;
}

sub packet {
	my $self = shift;
	return ($self->{length_orig},
		$self->{length_inc},
		$self->{drops},
		$self->{seconds},
		$self->{msecs},
		$self->{data});
}

sub linktype {
	my $self = shift;
	return sprintf("%u",$self->{linktype});
}

#
# USBMON-specific code, written by Mauro Carvalho Chehab
#

my @pending;

my $initial_time;
my $last_time;

sub print_frame($$)
{
	my %req = %{ @_[0] };
	my %resp = %{ @_[1] };

	if (!$initial_time) {
		$initial_time = $req{"Time"};
		$last_time = $initial_time;
	}

	# Print timestamps:
	#	relative time from resp 1
	#	relative time from last resp
	#	time to complete
	printf "%09d ms %06d ms (%06d us",
		1000 * ($req{"Time"} - $initial_time) + 0.5,
		1000 * ($req{"Time"} - $last_time) + 0.5,
		($resp{"Time"} - $req{"Time"}) * 1000000 + 0.5;
	$last_time = $req{"Time"};

	printf " EP=%02x)", $resp{"Endpoint"};

	my $app_data = substr($req{"Payload"}, 0, 8 * 2);
	my $type = hex(substr($app_data, 0, 2));
	while ($app_data ne "") {
		printf " %s", substr($app_data, 0, 2);
		$app_data = substr($app_data, 2);
	}

	# Extra data

	if ($type > 128) {
		printf " <<<";
	} else {
		printf " >>>";
	}

	my $app_data = substr($req{"Payload"}, 24 * 2);
	while ($app_data ne "") {
		printf " %s", substr($app_data, 0, 2);
		$app_data = substr($app_data, 2);
	}

	my $app_data = substr($resp{"Payload"}, 24 * 2);
	while ($app_data ne "") {
		printf " %s", substr($app_data, 0, 2);
		$app_data = substr($app_data, 2);
	}

	printf " ERROR %d",$resp{"Status"} if ($resp{"Status"});

	print "\n";

	if ($debug & 1) {
		my ($key, $value);
		print "\tREQ:  $key => $value\n" while (($key, $value) = each(%req));
		print "\tRESP: $key => $value\n" while (($key, $value) = each(%resp));
		print "\n";
	}

	return;
}

sub process_frame($) {
	my %frame = %{ @_[0] };

	if ($debug & 2) {
		my ($key, $value);
		print "PARSED data:\n";
		print "\t\tRAW: $key => $value\n" while (($key, $value) = each(%frame));
		print "\n";
	}

	# For now, we'll take a look only on control frames
	return if ($frame{"TransferType"} ne "2");

	if ($frame{"Status"} eq "-115") {
		push @pending, \%frame;
		return;
	}

	# Seek for operation origin
	my $related = $frame{"ID"};
	if (!$related) {
		print "URB %d incomplete\n", $frame{"ID"};
		return;
	}
	for (my $i = 0; $i < scalar(@pending); $i++) {
		if ($related == $pending[$i]{"ID"}) {
			my %req = %{$pending[$i]};

			print_frame (\%req, \%frame);

			# Remove from array, as it were already used
			splice(@pending, $i, 1);
			return;
		}
	}
	printf "URB %d incomplete: Couldn't find related URB %d\n", $related;
	return;
}

sub parse_file($)
{
	my $fh = shift;

	my $log = new();
	$log->get_header($fh);

	# Check for LINKTYPE_USB_LINUX_MMAPPED (220)
	if ($log->linktype() != 220) {
		printf"Link type %d\n", $log->linktype();
		die "ERROR: Link type is not USB";
	}

	while ($log->get_packet($fh) == 0) {
		my %frame;
		my ($length_orig,$length_incl,$drops,$secs,$msecs,$strdata) = $log->packet();
		$frame{"Time"} = sprintf "%d.%06d", $secs,$msecs;

		my @data=unpack('C*', $strdata);

		if ($debug & 4) {
			print "RAW DATA: ";
			for (my $i = 0; $i < scalar(@data); $i++) {
				printf " %02x", $data[$i];
			}
			print "\n";
		}

	#typedef struct _usb_header_mmapped {
	#	u_int64_t id;
	#	u_int8_t event_type;
	#	u_int8_t transfer_type;
	#	u_int8_t endpoint_number;
	#	u_int8_t device_address;
	#	u_int16_t bus_id;
	#	char setup_flag;/*if !=0 the urb setup header is not present*/
	#	char data_flag; /*if !=0 no urb data is present*/
	#	int64_t ts_sec;
	#	int32_t ts_usec;
	#	int32_t status;
	#	u_int32_t urb_len;
	#	u_int32_t data_len; /* amount of urb data really present in this event*/
	#	union {
	#		pcap_usb_setup setup;
	#		iso_rec iso;
	#	} s;
	#	int32_t	interval;	/* for Interrupt and Isochronous events */
	#	int32_t start_frame;	/* for Isochronous events */
	#	u_int32_t xfer_flags;	/* copy of URB's transfer flags */
	#	u_int32_t ndesc;	/* number of isochronous descriptors */
	#} pcap_usb_header_mmapped;

		# Not sure if this would work on 32-bits machines
		$frame{"ID"} = $data[0]       | $data[1] << 8 |
			$data[2] << 16 | $data[3] << 24 |
			$data[4] << 32 | $data[5] << 40 |
			$data[6] << 48 | $data[7] << 56;
		$frame{"Type"} = chr($data[8]);
		$frame{"TransferType"} = $data[9];
		$frame{"Endpoint"} = $data[10];
		$frame{"Device"} = $data[11];
		$frame{"BusID"} = $data[12] | $data[13] << 8;
		if ($data[14] == 0) {
			$frame{"SetupRequest"} = "present";
		} else {
			$frame{"SetupRequest"} = "not present";
		}
		if ($data[15] == 0) {
			$frame{"HasData"} = "present";
		} else {
			$frame{"HasData"} = "not present";
		}
		my $tsSec = $data[16]       | $data[17] << 8 |
			$data[18] << 16 | $data[19] << 24 |
			$data[20] << 32 | $data[21] << 40 |
			$data[22] << 48 | $data[23] << 56;
		my $tsUsec = $data[24]       | $data[25] << 8 |
			$data[26] << 16 | $data[27] << 24;
		$frame{"ArrivalTime"} = sprintf "%d.%06d", $tsSec,$tsUsec;

		# Status is signed with 32 bits. Fix signal, as errors are negative
		$frame{"Status"} = $data[28]       | $data[29] << 8 |
				$data[30] << 16 | $data[31] << 24;
		$frame{"Status"} = $frame{"Status"} - 0x100000000 if ($frame{"Status"} & 0x80000000);

		$frame{"URBLength"} = $data[32]       | $data[33] << 8 |
				$data[34] << 16 | $data[35] << 24;
		$frame{"DataLength"} = $data[36]       | $data[37] << 8 |
				$data[38] << 16 | $data[39] << 24;

		my $payload;
		my $payload_size;
		for (my $i = 40; $i < scalar(@data); $i++) {
			$payload .= sprintf "%02x", $data[$i];
			$payload_size++;
		}
		$frame{"Payload"} = $payload;
		$frame{"PayloadSize"} = $payload_size;

		process_frame(\%frame);
	}
}

# Main program, reading from a file. A small change is needed to allow it to
# accept a pipe

my $fh;
if (!$filename) {
	$fh = *STDIN;
} else {
	open $fh, "<$filename" || die "ERROR: Can't read log $filename: $!\n";
}
binmode $fh;
parse_file $fh;
#parse_file $fh;
close $fh;

__END__

=head1 NAME

parse_tcpdump_log.pl - Parses a tcpdump log captured via usbmon.

=head1 SYNOPSIS

parse_tcpdump_log.pl [options] [file ...]

Options:

	--help			brief help message

	--man			full documentation

	--debug [log level]	enables debug

=head1 OPTIONS

=over 8

=item B<--help>

Print a brief help message and exits.

=item B<--man>

Prints the manual page and exits.

=item B<--debug> [log level]

Changes the debug log level.

=back

=head1 DESCRIPTION

B<parse_tcpdump_log.pl> will parse a tcpdump log captured via usbmon.

A typical usage is to call tcpdump with:

	# tcpdump -i usbmon1 -w usb_device.tcpdump

after finishing data collection, parse it with:

	$ parse_tcpdump_log.pl usb_device.tcpdump

it is also possible to use it via pipe, like:

	$ cat usb_device.tcpdump | parse_tcpdump_log.pl

=head1 BUGS

Report bugs to Mauro Carvalho Chehab <mchehab@redhat.com>

=head1 COPYRIGHT

Copyright (c) 2011 by Mauro Carvalho Chehab <mchehab@redhat.com>.

License GPLv2: GNU GPL version 2 <http://gnu.org/licenses/gpl.html>.

This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.

=cut
