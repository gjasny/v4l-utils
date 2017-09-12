#!/usr/bin/perl

#   Copyright (C) 2008-2011 Mauro Carvalho Chehab
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

use strict;

# Enable autoflush
BEGIN { $| = 1 }

my $min_delay = 10;

my %req_map = (
	0x0 => "REQUEST_SET_USB_XFER_LEN",
	0x2 => "REQUEST_I2C_READ",
	0x3 => "REQUEST_I2C_WRITE",
	0x4 => "REQUEST_POLL_RC",
	0x8 => "REQUEST_JUMPRAM",
	0xB => "REQUEST_SET_CLOCK",
	0xC => "REQUEST_SET_GPIO",
	0xF => "REQUEST_ENABLE_VIDEO",
	0x10 => "REQUEST_SET_I2C_PARAM",
	0x11 => "REQUEST_SET_RC",
	0x12 => "REQUEST_NEW_I2C_READ",
	0x13 => "REQUEST_NEW_I2C_WRITE",
	0x15 => "REQUEST_GET_VERSION",
);

my %gpio_map = (
	0 => "GPIO0",
	2 => "GPIO1",
	3 => "GPIO2",
	4 => "GPIO3",
	5 => "GPIO4",
	6 => "GPIO5",
	8 => "GPIO6",
	10 => "GPIO7",
	11 => "GPIO8",
	14 => "GPIO9",
	15 => "GPIO10",
);

sub type_req($)
{
	my $reqtype = shift;
	my $s;

	if ($reqtype & 0x80) {
		$s = "RD ";
	} else {
		$s = "WR ";
	}
	if (($reqtype & 0x60) == 0x20) {
		$s .= "CLAS ";
	} elsif (($reqtype & 0x60) == 0x40) {
		$s .= "VEND ";
	} elsif (($reqtype & 0x60) == 0x60) {
		$s .= "RSVD ";
	}

	if (($reqtype & 0x1f) == 0x00) {
		$s .= "DEV ";
	} elsif (($reqtype & 0x1f) == 0x01) {
		$s .= "INT ";
	} elsif (($reqtype & 0x1f) == 0x02) {
		$s .= "EP ";
	} elsif (($reqtype & 0x1f) == 0x03) {
		$s .= "OTHER ";
	} elsif (($reqtype & 0x1f) == 0x04) {
		$s .= "PORT ";
	} elsif (($reqtype & 0x1f) == 0x05) {
		$s .= "RPIPE ";
	} else {
		$s .= sprintf "RECIP 0x%02x ", $reqtype & 0x1f;
	}

	$s =~ s/\s+$//;
	return $s;
}

my $delay = 0;

while (<>) {
	tr/A-F/a-f/;

	if (m/(.*)([0-9a-f].) ([0-9a-f].) ([0-9a-f].) ([0-9a-f].) ([0-9a-f].) ([0-9a-f].) ([0-9a-f].) ([0-9a-f].)\s*[\<\>]+\s*(.*)/) {
		my $prev = $1;
		my $reqtype = hex($2);
		my $req = hex($3);
		my $wvalue = hex("$5$4");
		my $windex = hex("$7$6");
		my $wlen = hex("$9$8");
		my $payload = $10;
		my @bytes = split(/ /, $payload);
		for (my $i = 0; $i < scalar(@bytes); $i++) {
			$bytes[$i] = hex($bytes[$i]);
		}

		if (defined($req_map{$req})) {
			$req = $req_map{$req};
		} else {
			$req = sprintf "0x%02x", $req;
		}

		printf "msleep(%d);\n", $delay if ($delay);
		$delay = 0;

		if (m/(IN|OUT): (\d+) ms \d+ ms/) {
			$delay = $min_delay * int(($2 + $min_delay / 2) / $min_delay);
		}

		if ($req eq "REQUEST_I2C_READ") {
			my $txlen = ($wvalue >> 8) + 2;
			my $addr = sprintf "0x%02x >> 1", $wvalue & 0xfe;
			my $val;

			if ($txlen == 2) {
				printf("dib0700_i2c_read($addr); /* $payload */\n");
				next;
			} elsif ($txlen == 3) {
				$val = $windex >> 8;
				printf("dib0700_i2c_read($addr, %d); /* $payload */\n", $val);
				next;
			} elsif ($txlen == 4) {
				$val = $windex;
				printf("dib0700_i2c_read($addr, %d); /* $payload */\n", $val);
				next;
			}
		}

		if ($req eq "REQUEST_I2C_WRITE") {
			if ($wlen == 5 || $wlen == 6) {
				my $addr = sprintf "0x%02x >> 1", $bytes[1];
				my $reg = sprintf "0x%04x", $bytes[2] << 8 | $bytes[3];
				my $val;
				if ($wlen == 6) {
					$val = sprintf "%d", $bytes[4] << 8 | $bytes[5];
				} else {
					$val = sprintf "%d", $bytes[4];
				}
				printf("dib0700_i2c_write($addr, $reg, $val);\n");
				next;
			}
		}

		if ($req eq "REQUEST_SET_I2C_PARAM") {
			my $divider1 = $bytes[2] << 8 | $bytes[3];
			my $divider2 = $bytes[4] << 8 | $bytes[5];
			my $divider3 = $bytes[6] << 8 | $bytes[7];

			my $xclk1 = 30000/$divider1;
			my $xclk2 = 72000/$divider2;
			my $xclk3 = 72000/$divider3;

			if ($xclk1 == $xclk2 && $xclk2 == $xclk3) {
				printf("dib0700_set_i2c_speed(adap->dev, $xclk1 /* kHz */);\n");
				next;
			}
			printf("dib0700_set_i2c_speed: $divider1 ($xclk1 kHz), $divider2 ($xclk2 kHz), $divider3 ($xclk3 kHz)\n");
			next;
		}

		if ($req eq "REQUEST_GET_VERSION") {
			my $hwversion  = $bytes[0] << 24 | $bytes[1] << 16 | $bytes[2] << 8 | $bytes[3];
			my $romversion  = $bytes[4] << 24 | $bytes[5] << 16 | $bytes[6] << 8 | $bytes[7];
			my $fw_version = $bytes[8] << 24 | $bytes[9] << 16 | $bytes[10] << 8 | $bytes[11];
			my $fwtype = $bytes[12] << 24 | $bytes[13] << 16 | $bytes[14] << 8 | $bytes[15];

			printf("dib0700_get_version(adap->dev, NULL, NULL, NULL, NULL); /* hw: 0x%x rom: 0x%0x fw version: 0x%x, fw type: 0x%x */\n",
			       $hwversion, $romversion, $fw_version, $fwtype);
			next;
		}

		if ($req eq "REQUEST_SET_GPIO") {
				my $gpio = $bytes[1];
				my $v = $bytes[2];

				my $dir = "GPIO_IN";
				my $val = 0;

				$dir = "GPIO_OUT" if ($v & 0x80);
				$val = 1 if ($v & 0x40);

				if (!($v & 0x3f)) {
					$gpio = $gpio_map{$gpio} if (defined($gpio_map{$gpio}));
					printf("dib0700_set_gpio(adap->dev, $gpio, $dir, $val);\n");
					next;
				}
		}

		if (($reqtype & 0xf0) == 0xc0) {
			my $txlen = ($wvalue >> 8) + 2;
			my $addr = $req;
			my $val;

			if ($txlen == 2) {
				printf("dib0700_ctrl_rd(adap->dev, $txlen, { cmd, $addr }, &buf, $wlen); /* $payload */\n");
				next;
			} elsif ($txlen == 3) {
				$val = $windex >> 8;
				printf("dib0700_ctrl_rd(adap->dev, $txlen, { cmd, $addr, $val }, &buf, $wlen); /* $payload */\n");
				next;
			} elsif ($txlen == 4) {
				$val = $windex;
				printf("dib0700_ctrl_rd(adap->dev, $txlen, { cmd, $addr, $val }, &buf, $wlen); /* $payload */\n");
				next;
			}
		}
		if ($wvalue == 0 && $windex == 0 && (($reqtype & 0xf0) == 0x40)) {
			my $cmd = $req;
			for (my $i = 0; $i < scalar(@bytes); $i++) {
				$cmd .= sprintf ", 0x%02x", $bytes[$i];
			}

			printf("dib0700_ctrl_wr(adap->dev, { $cmd }, %d);\n", $wlen + 1);
			next;
		}

		printf("%s(0x%02x), Req %s, wValue: 0x%04x, wIndex 0x%04x, wlen %d: %s\n",
			type_req($reqtype), $reqtype, $req, $wvalue, $windex, $wlen, $payload);
	}
}
