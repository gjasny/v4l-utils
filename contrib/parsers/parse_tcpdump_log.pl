#!/usr/bin/perl

#   Copyright (C) 2011-2012 Mauro Carvalho Chehab
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

# using cpan, you should install Net::Pcap, in order to allow direct capture
#	On Fedora/RHEL6, it is called "perl-Net-Pcap"
# FIXME: make this dependency optional
use Net::Pcap;

use strict;
#use warnings;
use Getopt::Long;
use Pod::Usage;
use File::Find;

# Enable autoflush
BEGIN { $| = 1 }

# Debug levels:
#	1 - frame request and frame response
#	2 - parsed frames
#	4 - raw data
my $debug = 0;

my $man = 0;
my $help = 0;
my $pcap = 0;
my $list_devices = 0;
my $device;
my @usbdev = ();
my $frame_processor;

GetOptions('debug=i' => \$debug,
	   'help|?' => \$help,
	   'pcap' => \$pcap,
	   'device=s' => \$device,
	    man => \$man,
	   'usbdev=i' => \@usbdev,
	   'list-devices' => \$list_devices,
	   'frame_processor=s' => \$frame_processor,
	  ) or pod2usage(2);
pod2usage(1) if $help;
pod2usage(-exitstatus => 0, -verbose => 2) if $man;

my %devs = map { $_ => 1 } @usbdev;

my $filename = shift;

$pcap = 1 if ($device);
$device = "usbmon1" if ($pcap && !$device);
die "Either use pcap or specify a filename" if ($pcap && $filename);

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
	elsif ($ident =~ /^\324\303\262\241/) {
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
	elsif ($ident =~ /^\241\262\315\064/) {
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
	elsif ($ident =~ /^\064\315\262\241/) {
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
	else {
		die "unknown magic in header, cannot parse this file, make sure it is pcap and not a pcapng (run file <filename> to find out) and then convert with wireshark.";
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
	$self->{more} = $more;
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
		$self->{more},
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
	my %req = %{ $_[0] };
	my %resp = %{ $_[1] };

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

	printf " EP=%02x, Dev=%02x)", $resp{'Endpoint'}, $resp{'Device'};

	my ($app_data, $type);

	if ($req{"Endpoint"} == 0x80 || $req{"SetupFlag"} == 0) {
		$app_data = substr($req{"Payload"}, 0, 8 * 2);
		$type = hex(substr($app_data, 0, 2));
		while ($app_data ne "") {
			printf " %s", substr($app_data, 0, 2);
			$app_data = substr($app_data, 2);
		}
	}

	# Extra data
	if ($resp{TransferType} == 2 || $resp{"Endpoint"} != 0x80) {
		if ($type > 128) {
			printf " <<<";
		} else {
			printf " >>>";
		}
	} else {
		if ($resp{Endpoint} < 0x80) {
			print " <<<";
		} else {
			print " >>>";
		}
	}

	$app_data = substr($req{"Payload"}, 24 * 2);
	while ($app_data ne "") {
		printf " %s", substr($app_data, 0, 2);
		$app_data = substr($app_data, 2);
	}

	$app_data = substr($resp{"Payload"}, 24 * 2);
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

my %frametype = (
	0 => "ISOC",
	1 => "Interrupt",
	2 => "Control",
	3 => "Bulk",
);

sub process_frame($) {
	my %frame = %{ $_[0] };

	if ($debug & 2) {
		my ($key, $value);
		print "PARSED data:\n";
		print "\t\tRAW: $key => $value\n" while (($key, $value) = each(%frame));
		print "\n";
	}

	if ($frame{"Status"} eq "-115") {
		push @pending, \%frame;
		return;
	}

	# Seek for operation origin
	my $related = $frame{"ID"};
	if (!$related) {
		print "URB %s incomplete\n", $frame{"ID"};
		return;
	}
	for (my $i = 0; $i < scalar(@pending); $i++) {
		if ($related eq $pending[$i]{"ID"} && $frame{'Device'} eq $pending[$i]{'Device'}) {
			my %req = %{$pending[$i]};

# skip unwanted URBs
			if (scalar @usbdev == 0 or exists($devs{$frame{'Device'}})) {
				if ($frame_processor) {
					require $frame_processor;
					frame_processor(\%req, \%frame);
				} else {
					print_frame(\%req, \%frame);
				}
			}

			# Remove from array, as it were already used
			splice(@pending, $i, 1);
			return;
		}
	}
	printf "URB %s incomplete: Couldn't find related URB\n", $related;
	return;
}

# Decode an USB header mapped frame. The frame is defined at libpcap as:
#
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
sub decode_frame($) {
	my $strdata = shift;
	my %frame;

	if ($debug & 4) {
		print "RAW DATA: ";
		for (my $i = 0; $i < length($strdata); $i++) {
			printf " %02x", ord(substr($strdata, $i, 1));
		}
		print "\n";
	}

	my ($frame_id, $tsSecHigh, $tsSecLow, $tsUsec);

	($frame_id, $frame{"Type"}, $frame{"TransferType"},
		$frame{"Endpoint"}, $frame{"Device"}, $frame{"BusID"},
		$frame{"SetupFlag"}, $frame{"DataFlag"},
		$tsSecHigh, $tsSecLow, $tsUsec,
		$frame{"Status"}, $frame{"URBLength"},
		$frame{"DataLength"}) = unpack("A8CCCCvCCVVVlVV", $strdata);
	$frame{"ID"} = "0x";
	for (my $i = 7; $i >= 0; $i--) {
		$frame{"ID"} .= sprintf "%02x", ord(substr($frame_id, $i, 1));
	}
	$frame{"Type"} = chr($frame{"Type"});
	$frame{"ArrivalTime"} = sprintf "%d.%06d", $tsSecHigh << 32 | $tsSecLow, $tsUsec;

	my $payload;
	my $payload_size;
	for (my $i = 40; $i < length($strdata); $i++) {
		$payload .= sprintf "%02x", ord(substr($strdata, $i, 1));
		$payload_size++;
	}
	$frame{"Payload"} = $payload;
	$frame{"PayloadSize"} = $payload_size;

	return %frame;
}

sub parse_file($$)
{
	my $log = shift;
	my $fh = shift;

	while ($log->get_packet($fh) == 0) {
		my ($length_orig,$length_incl,$drops,$secs,$msecs,$more,$strdata) = $log->packet();
		my %frame = decode_frame($strdata);
		$frame{"Time"} = sprintf "%d.%06d", $secs,$msecs;
		my $s;
		for (my $i = 0; $i < length($more); $i++) {
			$s .= sprintf "%02x", ord(substr($more, $i, 1));
		}
		$frame{"More"} = $s;

		process_frame(\%frame);
	}
}

sub handle_pcap_packet($$$)
{
	my $user_data = $_[0];
	my %hdr = %{ $_[1] };
	my $strdata = $_[2];

	my %frame = decode_frame($strdata);
	$frame{"Time"} = sprintf "%d.%06d", $hdr{tv_sec}, $hdr{tv_usec};
	process_frame(\%frame);
}

my $pcap_descr;
sub sigint_handler {
	# Close pcap gracefully after CTRL/C
	if ($pcap_descr) {
		Net::Pcap::close($pcap_descr);
		print "End of capture.\n";
		exit(0);
	}
}
#
# Ancillary routine to list what's connected to each USB port
#

if ($list_devices) {
	my ($bus, $dev, $name, $usb, $lastname);

	open IN, "/sys/kernel/debug/usb/devices";
	while (<IN>) {
		if (m/T:\s+Bus=(\d+).*Dev\#\=\s*(\d+)/) {
			$bus = $1 + 0;
			$dev = $2 + 0;
		}
		if (m/S:\s+Product=(.*)/) {
			$name = $1;
		}
		if (m/P:\s+Vendor=(\S+)\s+ProdID=(\S+)\s+Rev=(\S+)/) {
			$usb = "($1:$2 rev $3)";
		}
		if ($name && m/^$/) {
			printf("For %-36s%-22s ==> $0 --device usbmon%s --usbdev %s\n", $name, $usb, $bus, $dev);
			$lastname = $name;
		}
	}
	printf("For %-36s%-22s ==> $0 --device usbmon%s --usbdev %s\n", $name, $usb, $bus, $dev) if ($lastname ne $name);

	exit;
}

# Main program, reading from a file. A small change is needed to allow it to
# accept a pipe

if (!$pcap) {
	my $fh;
	if (!$filename) {
		$fh = *STDIN;
	} else {
		open $fh, "<$filename" || die "ERROR: Can't read log $filename: $!\n";
	}
	binmode $fh;

	my $log = new();
	$log->get_header($fh);

	# Check for LINKTYPE_USB_LINUX_MMAPPED (220)
	if ($log->linktype() != 220) {
		printf"Link type %d\n", $log->linktype();
		die "ERROR: Link type is not USB";
	}

	parse_file $log, $fh;
	close $fh;
} else {
	my $err;

	$pcap_descr = Net::Pcap::open_live($device, 65535, 0, 1000, \$err);
	die $err if ($err);

	# Trap  signals to exit nicely
	$SIG{HUP} = \&sigint_handler;
	$SIG{INT} = \&sigint_handler;
	$SIG{QUIT} = \&sigint_handler;
	$SIG{TERM} = \&sigint_handler;

	my $dl = Net::Pcap::datalink($pcap_descr);
	if ($dl != 220) {
		printf"Link type %d\n", $dl;
		die "ERROR: Link type is not USB";
	}

	Net::Pcap::loop($pcap_descr, -1, \&handle_pcap_packet, '');
	Net::Pcap::close($pcap_descr);
	die $err;
}

__END__

=head1 NAME

parse_tcpdump_log.pl - Parses a tcpdump log captured via usbmon.

=head1 SYNOPSIS

parse_tcpdump_log.pl [options] [file ...]

Options:

	--help			brief help message

	--man			full documentation

	--debug [log level]	enables debug

	--pcap			enables pcap capture

	--device [usbmon dev]	allow changing the usbmon device (default: usbmon1)

	--usbdev [usbdev id]    filter only traffic for a specific device

	--list-devices          list the available USB devices for each usbmon port

	--frame_processor [file] have this script call the function frame_processor of the script in file instead of printing.

=head1 OPTIONS

=over 8

=item B<--help>

Print a brief help message and exits.

=item B<--man>

Prints the manual page and exits.

=item B<--debug> [log level]

Changes the debug log level. The available levels are:

	1 - frame request and frame response

	2 - parsed frames

	4 - raw data

=item B<--pcap>

Enables the capture from the usbmon directly, using Net::Pcap. For this
to work, the kernel should be compiled with CONFIG_USB_MON, and the driver
needs to be loaded, if compiled as a module.

=item B<--device>

Enables the capture from the usbmon directly, using Net::Pcap, using an
interface different than usbmon1. It should be noticed, however, that the
only datalink that this script can parse is the one provided by usbmon,
e. g. datalink equal to 220 (LINKTYPE_USB_LINUX_MMAPPED).

=item B<--list-devices>

Lists all connected USB devices, and the associated usbmon device.

=item B<--usbdev [id]>

Filter traffic with given usbdev-id. By default no filtering is done
and usbdev is -1.

=item B<--frame_processor [perl-script]>

Provide this option with a filename to a perl-script which contains a function
frame_processor and instead of having the USB-frames printed to the screen
you can process them programmatically. See print_frame for an example. This
option exists to avoid the reparsing of the output generated by this script
for analyzing.

=back

=head1 DESCRIPTION

B<parse_tcpdump_log.pl> will parse a tcpdump log captured via usbmon.

A typical usage is to do a real time capture and parser with:

	# parse_tcpdump_log.pl --pcap

Alternatively, it may work with an offline capture. In this case, the
capture should be done with:

	# tcpdump -i usbmon1 -w usb_device.tcpdump

And the file will be parsed it with:

	$ parse_tcpdump_log.pl usb_device.tcpdump

It is also possible to parse a file via pipe, like:

	$ cat usb_device.tcpdump | parse_tcpdump_log.pl

=head1 DUMP OUTPUT FORMAT:

The output of the script looks like:

 000000000 ms 000000 ms (000127 us EP=80) 80 06 00 01 00 00 28 00 >>> 12 01 00 02 00 00 00 40 40 20 13 65 10 01 00 01 02 01
 000000000 ms 000000 ms (000002 us EP=80) 80 06 00 01 00 00 28 00 >>> 12 01 00 02 09 00 00 40 6b 1d 02 00 06 02 03 02 01 01
 000000006 ms 000005 ms (000239 us EP=80) c0 00 00 00 45 00 03 00 <<< 00 00 10
 000001006 ms 001000 ms (000112 us EP=80) c0 00 00 00 45 00 03 00 <<< 00 00 10
 000001106 ms 000100 ms (000150 us EP=80) c0 00 00 00 45 00 03 00 <<< 00 00 10

The provided info are:

    - Time from the script start;
    - Time from the last transaction;
    - Time between URB send request and URB response;
    - Endpoint for the transfer;
    - 8 bytes with the following URB fields:
	- Type (1 byte);
	- Request (1 byte);
	- wValue (2 bytes);
	- wIndex (2 bytes);
	- wLength (2 bytes);
    - URB direction:
	>>> - To URB device
	<<< - To host
    - Optional data (length is given by wLength).

=head1 BUGS

Report bugs to Mauro Carvalho Chehab <m.chehab@samsung.com>

=head1 COPYRIGHT

Copyright (c) 2011-2012 by Mauro Carvalho Chehab.

License GPLv2: GNU GPL version 2 <http://gnu.org/licenses/gpl.html>.

This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.

=cut
