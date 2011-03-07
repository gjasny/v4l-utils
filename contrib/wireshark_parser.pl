#!/usr/bin/perl
use strict;

my $debug = 1;

sub process_frame(%) {
	my %frame = @_;

	while (my ($key, $value) = each(%frame) ) {
		print "$key => $value\n";
	}

	print "\n";
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
			$frame{"Time"} = $2 if (m/^\s*([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)/);
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
		if (m/^\s+URB\s+status\:\s+([^\(]*)\s+\(/) {
			$frame{"Status"} = $1;
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
			$frame{$1} = $2;
			next;
		}
		if (m/^\s+\[bInterfaceClass\:\s+(.*)\s+\(/) {
			$frame{"bInterfaceClass"} = $1;
			next;
		}
		if (m/^\s+Configuration\s+bmAttributes\:\s+(.*)/) {
			$frame{"ConfigurationbmAttributes"} = $1;
			next;
		}
		if (m/^\s+bMaxPower\:\s+(.*)\s+\(/) {
			$frame{"bInterfaceClass"} = $1;
			next;
		}
		next if (m/^\s+URB\s+setup/);
		if (m/^\s+bmRequestType\:\s+(.*)/) {
			$frame{"bmRequestType"} = $1;
			next;
		}
		if (m/^\s+bmAttributes\:\s+(.*)/) {
			$frame{"bmAttributes"} = $1;
			next;
		}
		if (m/^\s+bRequest\:\s+(.*)\s+\(/) {
			$frame{"bRequest"} = $1;
			next;
		}
		if (m/^\s+Descriptor\s+Index\:\s+(.*)/) {
			$frame{"DescriptorIndex"} = $1;
			next;
		}
		if (m/^\s+(bDescriptorType|bInterfaceClass)\:\s+(.*)\s+\(/) {
			$frame{$1} = $2;
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
			$frame{"ArrivalTime"} = $1;
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
		print "# Unparsed: $_" if ($debug > 2);
	}
}

wireshark_parser();