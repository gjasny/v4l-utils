#!/usr/bin/perl
use strict;
use Date::Parse;

my $debug = 1;

my @pending;

my $initial_time;
my $last_time;

sub print_frame($$)
{
	my %req = %{ @_[0] };
	my %resp = %{ @_[1] };

#	# For now, let's concern only when there are some data
#	return if (!$resp{"ApplicationData"});

	my $rel_time = $req{"Arrival"} - $initial_time;

	# Print timestamps:
	#	relative time from resp 1
	#	relative time from last resp
	#	time to complete
	printf "%09d ms %06d ms (%06d us",
		1000 * $req{"Time"},
		1000 * ($req{"Time"} - $last_time),
		($resp{"Time"} - $req{"Time"}) * 1000000;
	$last_time = $req{"Time"};

	printf " EP=%s)", $resp{"Endpoint"};

	printf " %02x", $req{"bmRequestType"};
	printf " %02x", $req{"bRequest"} if ($req{"bRequest"});
	printf " %02x", $req{"Index"} if ($req{"Index"});
	printf " %02x", $req{"bDescriptorType"} if ($req{"bDescriptorType"});
	printf " %02x %02x", $req{"LanguageId"} & 0xff, $req{"LanguageId"} >> 8 if ($req{"LanguageId"});
	printf " %02x %02x", $req{"wLength"} & 0xff, $req{"wLength"} >> 8  if ($req{"wLength"});

	my $app_data = $req{"ApplicationData"};
	if ($app_data ne "") {
		printf " >>>";
	}
	while ($app_data ne "") {
		printf " %s", substr($app_data, 0, 2);
		$app_data = substr ($app_data, 2);
	}

	my $app_data = $resp{"ApplicationData"};
	if ($app_data ne "") {
		printf " <<<";
	}
	while ($app_data ne "") {
		printf " %s", substr($app_data, 0, 2);
		$app_data = substr ($app_data, 2);
	}


	print "\n";

	if ($debug) {
		my ($key, $value);
		print "\tREQ:  $key => $value\n" while (($key, $value) = each(%req));
		print "\tRESP: $key => $value\n" while (($key, $value) = each(%resp));
		print "\n";
	}

	return;
}

sub process_frame(%) {
	my %frame = @_;

	$initial_time = $frame{"Arrival"} if (!$initial_time);

	if ($debug > 1) {
		my ($key, $value);
		print "\t\tRAW: $key => $value\n" while (($key, $value) = each(%frame));
		print "\n";
	}

	# For now, we'll take a look only on control frames
	return if ($frame{"TransferType"} ne "URB_CONTROL");

	if ($frame{"Status"} eq "-EINPROGRESS") {
		push @pending, \%frame;
		return;
	}

	# Seek for operation origin
	my $related = $frame{"__RelatedTo"};
	if (!$related) {
		print "URB %d incomplete\n", $frame{"Number"};
		return;
	}
	for (my $i = 0; $i < scalar(@pending); $i++) {
		if ($related == $pending[$i]{"Number"}) {
			my %req = %{$pending[$i]};

			print_frame (\%req, \%frame);

			# Remove from array, as it were already used
			splice(@pending, $i, 1);
			return;
		}
	}
	printf "URB %d incomplete: Couldn't find related URB %d\n", $frame{"Number"}, $related;
	return;
}

sub wireshark_parser() {
	my %frame;
	my $next_is_time_frame;

	while (<>) {
		next if (m/^\n/);
		next if (m/^\s+(INTERFACE|ENDPOINT|DEVICE|CONFIGURATION|STRING)\s+DESCRIPTOR/);
		next if (m/^\s+\[Protocols\s+in\s+frame:\s+usb\]/);
		if (m/^No.\s+Time\s+Source\s+Destination\s+Protocol Info/) {
			process_frame (%frame) if (%frame);
			%frame = ();
			$next_is_time_frame = 1;
			next;
		}
		if ($next_is_time_frame) {
			if (m/^\s*([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)/) {
				$frame{"Time"} = $2 + 0;
				if ($3 eq "host") {
					$frame{"Direction"} = "Device";
				} else {
					$frame{"Direction"} = "Host";
				}
			}
			$next_is_time_frame = 0;
			next;
		}
		if (m/^Frame\s+(\d+)/) {
			$frame{"Number"} = $1;
			next;
		}
		if (m/^USB\s+URB/) {
			next;
		}
		if (m/^\s+URB\s+id\:\s+(.*)/) {
			$frame{"ID"} = $1;
			next;
		}
		if (m/^\s+URB\s+type\:\s+([^\s]+)/) {
			$frame{"Type"} = $1;
			next;
		}
		if (m/^\s+URB\s+transfer\s+type\:\s+([^\s]+)/) {
			$frame{"TransferType"} = $1;
			next;
		}
		if (m/^\s+(Device|Endpoint|iConfiguration|idProduct|idVendor|iManufacturer|iSerialNumber|bcdDevice|bcdUSB|bDeviceClass|bDeviceProtocol|bDeviceSubClass|bMaxPacketSize0|bNumConfigurations|bNumInterfaces|bString|iProduct|wTotalLength)\:\s+(.*)/) {
			$frame{$1} = $2;
			next;
		}


		if (m/^\s+URB\s+bus\s+id\:\s+(.*)/) {
			$frame{"BusID"} = $1;
			next;
		}
		if (m/^\s+Device\s+setup\s+request\:\s+(.*)\s+\(/) {
			$frame{"SetupRequest"} = $1;
			next;
		}
		if (m/^\s+Data\:\s+(.*)\s+\(/) {
			$frame{"HasData"} = 1 if ($1 eq "present");
			next;
		}
		if (m/^\s+URB\s+status\:\s+([^\(]*)\s+\((.*)\)\s+\(/ || m/^\s+URB\s+status\:\s+([^\(]*)\s+\((.*)\)/) {
			$frame{"Status"} = $2;
			next;
		}
		if (m/^\s+URB\s+length\s+\[bytes\]\:\s+(.*)/) {
			$frame{"URBLength"} = $1;
			next;
		}
		if (m/^\s+Data\s+length\s+\[bytes\]\:\s+(.*)/) {
			$frame{"DataLength"} = $1;
			next;
		}
		if (m/^\s+wLANGID:.*(0x.*)/) {
			$frame{"wLANGID"} = $1;
			next;
		}
		if (m/^\s+bEndpointAddress\:\s+(.*)/) {
			# Probably need more parsing
			$frame{"EndpointAddress"} = $1;
			next;
		}
		if (m/^\s+\[(Request|Response)\s+in\:\s+(\d+)\]/) {
			$frame{"__RelatedTo"} = $2;
			$frame{"__RelationType"} = $1;
			next;
		}
		if (m/^\s+Configuration\s+bmAttributes\:\s+(.*)/) {
			$frame{"ConfigurationbmAttributes"} = $1;
			next;
		}
		if (m/^\s+bMaxPower\:\s+(.*)\s+\((.*)\)/) {
			$frame{"bMaxPower"} = $2;
			next;
		}
		next if (m/^\s+URB\s+setup/);
		if (m/^\s+bmRequestType\:\s+(.*)/) {
			$frame{"bmRequestType"} = hex($1);
			next;
		}
		if (m/^\s+bmAttributes\:\s+(.*)/) {
			$frame{"bmAttributes"} = $1;
			next;
		}
		if (m/^\s+bRequest\:\s+(.*)\s+\((.*)\)/) {
			$frame{"bRequest"} = hex($2);
			next;
		}
		if (m/^\s+Descriptor\s+Index\:\s+(.*)/) {
			$frame{"DescriptorIndex"} = $1;
			next;
		}
		if (m/^\s+(bDescriptorType|bInterfaceClass)\:\s+(.*)\s+\((.*)\)/) {
			$frame{$1} = hex($3);
			next;
		}
		if (m/^\s+\[(bInterfaceClass)\:\s+(.*)\s+\((.*)\)\]/) {
			$frame{$1} = hex($3);
			next;
		}
		if (m/^\s+(bInterval|bInterfaceNumber|bInterfaceSubClass|bInterfaceProtocol)\:\s+(.*)/) {
			$frame{$1} = $2;
			next;
		}
		if (m/^\s+bAlternateSetting\:\s+(.*)/) {
			$frame{"bAlternateSetting"} = $1;
			next;
		}
		if (m/^\s+bConfigurationValue\:\s+(.*)/) {
			$frame{"bConfigurationValue"} = $1;
			next;
		}
		if (m/^\s+bLength\:\s+(.*)/) {
			$frame{"bLength"} = $1;
			next;
		}
		if (m/^\s+bNumEndpoints\:\s+(.*)/) {
			$frame{"bNumEndpoints"} = $1;
			next;
		}
		if (m/^\s+iInterface\:\s+(.*)/) {
			$frame{"iInterface"} = $1;
			next;
		}
		if (m/^\s+Language\s+Id\:\s+(.*)\s+\(/) {
			$frame{"LanguageId"} = $1;
			next;
		}
		if (m/^\s+wLength\:\s+(.*)/) {
			$frame{"wLength"} = $1;
			next;
		}
		if (m/^\s+wIndex\:\s+(.*)/) {
			$frame{"wIndex"} = $1;
			next;
		}
		if (m/^\s+wMaxPacketSize\:\s+(.*)/) {
			$frame{"wMaxPacketSize"} = $1;
			next;
		}
		if (m/^\s+wInterface\:\s+(.*)/) {
			$frame{"wInterface"} = $1;
			next;
		}
		if (m/^\s+Application\s+Data\:\s+(.*)/) {
			$frame{"ApplicationData"} = $1;
			next;
		}
		if (m/^\s+Frame\s+Number\:\s+(.*)/) {
			$frame{"FrameNumber"} = $1;
			next;
		}
		if (m/^\s+(Frame|Capture)\s+Length\:\s+(.*)\s+bytes/) {
			$frame{"$1Length"} = $2;
			next;
		}

		if (m/^\s+Arrival\s+Time:\s+(.*)/) {
			$frame{"ArrivalTime"} = str2time($1);
			next;
		}
		if (m/^\s+\[Time\s+from\s+request\:\s+(.*)\s+seconds\]/) {
			$frame{"TimeFromRequest"} = $1;
			next;
		}
		if (m/^\s+\[Time\s+delta\s+from\s+previous\s+(captured|displayed)\s+frame\:\s+(.*)\s+seconds\]/) {
			next;
		}
		if (m/^\s+\[Time\s+since\s+reference\s+or\s+first\s+frame\:\s+(.*)\s+seconds\]/) {
			next;
		}
		if (m/^\s+\[Frame\s+is\s+marked\:\s+(.*)/) {
			next;
		}

		# Remove some bitmap descriptions
		next if (m/=\s+Direction\:/);
		next if (m/=\s+Type\:/);
		next if (m/=\s+Recipient\:/);
		next if (m/=\s+Transfertype\:/);
		next if (m/=\s+Synchronisationtype\:/);
		next if (m/=\s+Behaviourtype\:/);
		next if (m/=\s+Endpoint\s+Number\:/);
		next if (m/=\s+Remote\s+Wakeup\:/);
		next if (m/=\s+Self-Powered\:/);
		next if (m/=\s+Must\s+be\s+1\:/);


		# Prints unparsed strings
		print "# Unparsed: $_" if ($debug);
	}
}

wireshark_parser();