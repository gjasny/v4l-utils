#!/usr/bin/perl
# using cpan, you should install Net::TcpDumpLog
use Net::TcpDumpLog;
use strict;
use Getopt::Long;

# Currently, accepts only one usbmon format:
#	USB with padded Linux header (LINKTYPE_USB_LINUX_MMAPPED)
# This is the one produced by Beagleboard sniffer GSOC.

my $debug = 0;
GetOptions('debug' => \$debug);

# Frame format as parsed by libpcap 1.0.0 and 1.1.1. Not sure if format
# changed on different versions.

my $filename;

# FIXME: use shift of die, after finishing the tests
$filename = shift or die "Please specify a file name";

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

	if ($debug) {
		my ($key, $value);
		print "\tREQ:  $key => $value\n" while (($key, $value) = each(%req));
		print "\tRESP: $key => $value\n" while (($key, $value) = each(%resp));
		print "\n";
	}

	return;
}

sub process_frame($) {
	my %frame = %{ @_[0] };

	if ($debug > 1) {
		my ($key, $value);
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
	my $file = shift;

	my $log = Net::TcpDumpLog->new();
	$log->read($file);

	# Check for LINKTYPE_USB_LINUX_MMAPPED (220)
	if ($log->linktype() != 220) {
		printf"Link type %d\n", $log->linktype();
		die "Link type is not USB";
	}
	my @Indexes = $log->indexes;

	foreach my $index (@Indexes) {
		my %frame;
		my ($length_orig,$length_incl,$drops,$secs,$msecs) = $log->header($index);
		$frame{"Time"} = sprintf "%d.%06d", $secs,$msecs;

		my $strdata = $log->data($index);
		my @data=unpack('C*', $strdata);

		if ($debug > 2) {
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

		if ($debug > 1) {
			my ($key, $value);
			print "\t$key => $value\n" while (($key, $value) = each(%frame));
			printf "\n";
		}
		process_frame(\%frame);
	}
}

# Main program
parse_file $filename;
