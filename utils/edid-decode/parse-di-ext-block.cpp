// SPDX-License-Identifier: MIT
/*
 * Copyright 2020 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#include "edid-decode.h"

void edid_state::parse_digital_interface(const unsigned char *x)
{
	data_block = "Digital Interface";
	printf("  %s:\n", data_block.c_str());

	printf("    Supported Digital Interface: ");
	unsigned short v = x[2];
	switch (v) {
	case 0x00:
		printf("Analog Video Input\n");
		if (!memchk(x + 2, 12))
			fail("Bytes 0x02-0x0d should be 0.\n");
		return;
	case 0x01: printf("DVI\n"); break;
	case 0x02: printf("DVI Single Link\n"); break;
	case 0x03: printf("DVI Dual Link - High Resolution\n"); break;
	case 0x04: printf("DVI Dual Link - High Color\n"); break;
	case 0x05: printf("DVI - Consumer Electronics\n"); break;
	case 0x06: printf("Plug & Display\n"); break;
	case 0x07: printf("DFP\n"); break;
	case 0x08: printf("Open LDI - Single Link\n"); break;
	case 0x09: printf("Open LDI - Dual Link\n"); break;
	case 0x0a: printf("Open LDI - Consumer Electronics\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", v);
		   fail("Unknown Digital Interface 0x%02x.\n", v);
		   break;
	}

	switch ((x[3]) >> 6) {
	case 0x00:
		if (!memchk(x + 3, 4))
			fail("Bytes 0x03-0x06 should be 0.\n");
		break;
	case 0x01:
		printf("    Version: %u.%u\n    Release: %u.%u\n", x[3] & 0x3f, x[4], x[5], x[6]);
		if (x[4] > 99)
			fail("Version number > 99.\n");
		if (x[6] > 99)
			fail("Release number > 99.\n");
		break;
	case 0x02:
		if (x[3] & 0x3f)
			fail("Bits 5-0 of byte 0x03 should be 0.\n");
		if (x[5] || x[6])
			fail("Bytes 0x05-0x06 should be 0.\n");
		printf("    Letter Designation: %c\n", x[4]);
		break;
	case 0x03:
		if (x[3] & 0x3f)
			fail("Bits 5-0 of byte 0x03 should be 0.\n");
		printf("    Date Code: Year %u Week %u Day %u\n", 1990 + x[4], x[5], x[6]);
		if (!x[5] || x[5] > 12)
			fail("Bad month number.\n");
		if (!x[6] || x[6] > 31)
			fail("Bad day number.\n");
		break;
	}

	v = x[7];
	printf("    Data Enable Signal Usage %sAvailable\n",
	       (v & 0x80) ? "" : "Not ");
	if (v & 0x80)
		printf("    Data Enable Signal %s\n",
		       (v & 0x40) ? "High" : "Low");
	else if (v & 0x40)
		fail("Bit 6 of byte 0x07 should be 0.\n");
	printf("    Edge of Shift Clock: ");
	switch ((v >> 4) & 0x03) {
	case 0: printf("Not specified\n"); break;
	case 1: printf("Use rising edge of shift clock\n"); break;
	case 2: printf("Use falling edge of shift clock\n"); break;
	case 3: printf("Use both edges of shift clock\n"); break;
	}
	printf("    HDCP is %ssupported\n", (v & 0x08) ? "" : "not ");
	printf("    Digital Receivers %ssupport Double Clocking of Input Data\n",
	       (v & 0x04) ? "" : "do not ");
	printf("    Packetized Digital Video is %ssupported\n", (v & 0x02) ? "" : "not ");
	if (v & 0x01)
		fail("Bit 0 of byte 0x07 should be 0.\n");

	v = x[8];
	printf("    Data Formats: ");
	switch (v) {
	case 0x15: printf("8-Bit Over 8-Bit RGB\n"); break;
	case 0x19: printf("12-Bit Over 12-Bit RGB\n"); break;
	case 0x24: printf("24-Bit MSB-Aligned RGB (Single Link)\n"); break;
	case 0x48: printf("48-Bit MSB-Aligned RGB (Dual Link - High Resolution)\n"); break;
	case 0x49: printf("48-Bit MSB-Aligned RGB (Dual Link - High Color)\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", v);
		   fail("Unknown Data Format 0x%02x.\n", v);
		   break;
	}
	if (x[2] == 0x03 && v != 0x48)
		fail("Data Format should be 0x48, not 0x%02x.\n", v);
	if (x[2] == 0x04 && v != 0x49)
		fail("Data Format should be 0x49, not 0x%02x.\n", v);

	v = x[9];
	if (v) {
		printf("    Minimum Pixel Clock Frequency Per Link: %u MHz\n", v);
		if (v == 0xff)
			fail("Invalid Min-PCF 0x%02x.\n", v);
	}

	v = x[10] | (x[11] << 8);
	if (v) {
		printf("    Maximum Pixel Clock Frequency Per Link: %u MHz\n", v);
		if (v == 0xffff)
			fail("Invalid Max-PCF 0x%04x.\n", v);
	}

	v = x[12] | (x[13] << 8);
	if (v == 0xffff)
		printf("    Crossover Frequency: None - Single Link\n");
	else if (v)
		printf("    Crossover Frequency: %u MHz\n", v);
}

void edid_state::parse_display_device(const unsigned char *x)
{
	data_block = "Display Device";
	printf("  %s:\n", data_block.c_str());

	printf("    Sub-Pixel Layout: ");
	unsigned char v = x[0x0e];
	switch (v) {
	case 0x00: printf("Not defined\n"); break;
	case 0x01: printf("RGB\n"); break;
	case 0x02: printf("BGR\n"); break;
	case 0x03: printf("Quad Pixel - G at bottom left & top right\n"); break;
	case 0x04: printf("Quad Pixel - G at bottom right & top left\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", v);
		   fail("Unknown Sub-Pixel Layout 0x%02x.\n", v);
		   break;
	}
	printf("    Sub-Pixel Configuration: ");
	v = x[0x0f];
	switch (v) {
	case 0x00: printf("Not defined\n"); break;
	case 0x01: printf("Delta (Tri-ad)\n"); break;
	case 0x02: printf("Stripe\n"); break;
	case 0x03: printf("Stripe Offset\n"); break;
	case 0x04: printf("Quad Pixel\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", v);
		   fail("Unknown Sub-Pixel Configuration 0x%02x.\n", v);
		   break;
	}
	printf("    Sub-Pixel Shape: ");
	v = x[0x10];
	switch (v) {
	case 0x00: printf("Not defined\n"); break;
	case 0x01: printf("Round\n"); break;
	case 0x02: printf("Square\n"); break;
	case 0x03: printf("Rectangular\n"); break;
	case 0x04: printf("Oval\n"); break;
	case 0x05: printf("Elliptical\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", v);
		   fail("Unknown Sub-Pixel Shape 0x%02x.\n", v);
		   break;
	}
	if (x[0x11])
		printf("    Horizontal Dot/Pixel Pitch: %.2f mm\n",
		       x[0x11] / 100.0);
	if (x[0x12])
		printf("    Vertical Dot/Pixel Pitch: %.2f mm\n",
		       x[0x12] / 100.0);
	v = x[0x13];
	printf("    Display Device %s a Fixed Pixel Format\n",
	       (v & 0x80) ? "has" : "does not have");
	printf("    View Direction: ");
	switch ((v & 0x60) >> 5) {
	case 0x00: printf("Not specified\n"); break;
	case 0x01: printf("Direct\n"); break;
	case 0x02: printf("Reflected\n"); break;
	case 0x03: printf("Direct & Reflected\n"); break;
	}
	printf("    Display Device uses %stransparent background\n",
	       (v & 0x10) ? "" : "non-");
	printf("    Physical Implementation: ");
	switch ((v & 0x0c) >> 2) {
	case 0x00: printf("Not specified\n"); break;
	case 0x01: printf("Large Image device for group viewing\n"); break;
	case 0x02: printf("Desktop or personal display\n"); break;
	case 0x03: printf("Eyepiece type personal display\n"); break;
	}
	printf("    Monitor/display does %ssupport DDC/CI\n",
	       (v & 0x02) ? "" : "not ");
	if (v & 0x01)
		fail("Bit 0 of byte 0x13 should be 0.\n");
}

void edid_state::parse_display_caps(const unsigned char *x)
{
	data_block = "Display Capabities & Feature Support Set";
	printf("  %s:\n", data_block.c_str());

	unsigned short v = x[0x14];

	printf("    Legacy Modes: %s VGA/DOS Legacy Timing Modes are supported\n",
	       (v & 0x80) ? "All" : "Not all");
	printf("    Stereo Video: ");
	switch ((v & 0x70) >> 4) {
	case 0x00: printf("No direct stereo\n"); break;
	case 0x01: printf("Field seq. stereo via stereo sync signal\n"); break;
	case 0x02: printf("auto-stereoscopic, column interleave\n"); break;
	case 0x03: printf("auto-stereoscopic, line interleave\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", (v & 0x70) >> 4);
		   fail("Unknown Stereo Video 0x%02x.\n", (v & 0x70) >> 4);
		   break;
	}
	printf("    Scaler On Board: %s\n", (v & 0x08) ? "Yes" : "No");
	printf("    Image Centering: %s\n", (v & 0x04) ? "Yes" : "No");
	printf("    Conditional Update: %s\n", (v & 0x02) ? "Yes" : "No");
	printf("    Interlaced Video: %s\n", (v & 0x01) ? "Yes" : "No");

	v = x[0x15];
	printf("    Frame Lock: %s\n", (v & 0x80) ? "Yes" : "No");
	printf("    Frame Rate Conversion: ");
	switch ((v & 0x60) >> 5) {
	case 0x00: printf("Not supported\n"); break;
	case 0x01: printf("Vertical is converted to a single frequency\n"); break;
	case 0x02: printf("Horizontal is convertred to a single frequency\n"); break;
	case 0x03: printf("Both Vertical & Horizontal are converted to single frequencies\n"); break;
	}
	if (v & 0x1f)
		fail("Bits 4-0 of byte 0x15 should be 0.\n");
	v = x[0x16] | (x[0x17] << 8);
	printf("    Vertical Frequency: ");
	if (!v) {
		printf("Not available\n");
	} else if (v == 0xffff) {
		printf("Reserved\n");
		fail("Vertical Frequency uses 0xffff (reserved value).\n");
	} else {
		printf("%.2f kHz\n", v / 100.0);
	}
	v = x[0x18] | (x[0x19] << 8);
	printf("    Horizontal Frequency: ");
	if (!v) {
		printf("Not available\n");
	} else if (v == 0xffff) {
		printf("Reserved\n");
		fail("Horizontal Frequency uses 0xffff (reserved value).\n");
	} else {
		printf("%.2f kHz\n", v / 100.0);
	}

	v = x[0x1a];
	printf("    Display/Scan Orientation Definition Type: ");
	switch ((v & 0xc0) >> 6) {
	case 0x00: printf("Not defined\n"); break;
	case 0x01: printf("Fixed Orientation\n"); break;
	case 0x02: printf("Pivots: Default Orientation\n"); break;
	case 0x03: printf("Pivots: Current Orientation (requires multiple EDID Extension Tables)\n"); break;
	}
	printf("    Screen Orientation: %s\n",
	       (v & 0x20) ? "Portrait" : "Landscape");
	printf("    Zero Pixel Location: ");
	switch ((v & 0x18) >> 3) {
	case 0x00: printf("Upper Left\n"); break;
	case 0x01: printf("Upper Right\n"); break;
	case 0x02: printf("Lower Left\n"); break;
	case 0x03: printf("Lower Right\n"); break;
	}
	printf("    Scan Direction: ");
	switch ((v & 0x06) >> 1) {
	case 0x00: printf("Not defined\n"); break;
	case 0x01: printf("Fast Scan is on the Major (Long) Axis and Slow Scan is on the Minor Axis\n"); break;
	case 0x02: printf("Fast Scan is on the Minor (Short) Axis and Slow Scan is on the Major Axis\n"); break;
	case 0x03:
		   printf("Reserved\n");
		   fail("Scan Direction used the reserved value 0x03.\n");
		   break;
	}
	printf("    Standalone Projector: %s\n",
	       (v & 0x01) ? "Yes" : "No");

	v = x[0x1b];
	printf("    Default Color/Luminance Decoding: ");
	switch (v) {
	case 0x00: printf("Not defined\n"); break;
	case 0x01: printf("BGR\n"); break;
	case 0x02: printf("Y/C (S-Video) NTSC\n"); break;
	case 0x03: printf("Y/C (S-Video) PAL\n"); break;
	case 0x04: printf("Y/C (S-Video) SECAM\n"); break;
	case 0x05: printf("YCrCb 4:4:4 per SMPTE 293M & 294M\n"); break;
	case 0x06: printf("YCrCb 4:2:2 per SMPTE 293M & 294M\n"); break;
	case 0x07: printf("YCrCb 4:2:0 per SMPTE 293M & 294M\n"); break;
	case 0x08: printf("YCrCb per SMPTE 260M (Legacy HDTV)\n"); break;
	case 0x09: printf("YPbPr per SMPTE 240M (Legacy HDTV)\n"); break;
	case 0x0a: printf("YCrCb per SMPTE 274M (Modern HDTV)\n"); break;
	case 0x0b: printf("YPbPr per SMPTE 274M (Modern HDTV)\n"); break;
	case 0x0c: printf("Y B-Y R-Y BetaCam (Sony)\n"); break;
	case 0x0d: printf("Y B-Y R-Y M-2 (Matsushita)\n"); break;
	case 0x0e: printf("Monochrome\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", v);
		   fail("Unknown Default Color/Luminance Decoding 0x%02x.\n", v);
		   break;
	}
	v = x[0x1c];
	printf("    Preferred Color/Luminance Decoder: ");
	switch (v) {
	case 0x00: printf("Uses Default Decoding\n"); break;
	case 0x01: printf("BGR\n"); break;
	case 0x02: printf("Y/C (S-Video)\n"); break;
	case 0x03: printf("Yxx (SMPTE 2xxM)\n"); break;
	case 0x04: printf("Monochrome\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", v);
		   fail("Unknown Preferred Color/Luminance Decoding 0x%02x.\n", v);
		   break;
	}
	v = x[0x1d];
	if (v && (x[0x1e] & 0xfc)) {
		printf("    Color/Luminance Decoding Capabilities:\n");
		printf("      BGR: %s\n", (v & 0x80) ? "Yes" : "No");
		printf("      Y/C (S-Video) NTSC: %s\n", (v & 0x40) ? "Yes" : "No");
		printf("      Y/C (S-Video) PAL: %s\n", (v & 0x20) ? "Yes" : "No");
		printf("      Y/C (S-Video) SECAM: %s\n", (v & 0x10) ? "Yes" : "No");
		printf("      YCrCb 4:4:4 per SMPTE 293M & 294M: %s\n", (v & 0x08) ? "Yes" : "No");
		printf("      YCrCb 4:2:2 per SMPTE 293M & 294M: %s\n", (v & 0x04) ? "Yes" : "No");
		printf("      YCrCb 4:2:0 per SMPTE 293M & 294M: %s\n", (v & 0x02) ? "Yes" : "No");
		printf("      YCrCb per SMPTE 260M (Legacy HDTV): %s\n", (v & 0x01) ? "Yes" : "No");
		v = x[0x1e];
		printf("      YPbPr per SMPTE 240M (Legacy HDTV): %s\n", (v & 0x80) ? "Yes" : "No");
		printf("      YCrCb per SMPTE 274M (Modern HDTV): %s\n", (v & 0x40) ? "Yes" : "No");
		printf("      YPbPr per SMPTE 274M (Modern HDTV): %s\n", (v & 0x20) ? "Yes" : "No");
		printf("      Y B-Y R-Y BetaCam (Sony): %s\n", (v & 0x10) ? "Yes" : "No");
		printf("      Y B-Y R-Y M-2 (Matsushita): %s\n", (v & 0x08) ? "Yes" : "No");
		printf("      Monochrome: %s\n", (v & 0x04) ? "Yes" : "No");
	} else {
		printf("    Color/Luminance Decoding Capabilities: None\n");
	}
	if (v & 0x03)
		fail("Bits 1-0 of byte 0x1e should be 0.\n");

	v = x[0x1f];
	printf("    Dithering: %s\n", (v & 0x80) ? "Yes" : "No");
	if (v & 0x7f)
		fail("Bits 6-0 of byte 0x1f should be 0.\n");
	v = x[0x20];
	printf("    Supported Color Bit-Depth of Sub-Channel 0 (Blue): ");
	if (!v) {
		printf("No Information\n");
	} else if (v <= 16) {
		printf("%u\n", v);
	} else {
		printf("Reserved (0x%02x)\n", v);
		fail("Supported Color Bit-Depth of Sub-Channel Blue value is 0x%02x.\n", v);
	}
	v = x[0x21];
	printf("    Supported Color Bit-Depth of Sub-Channel 1 (Green): ");
	if (!v) {
		printf("No Information\n");
	} else if (v <= 16) {
		printf("%u\n", v);
	} else {
		printf("Reserved (0x%02x)\n", v);
		fail("Supported Color Bit-Depth of Sub-Channel Green value is 0x%02x.\n", v);
	}
	v = x[0x22];
	printf("    Supported Color Bit-Depth of Sub-Channel 2 (Red): ");
	if (!v) {
		printf("No Information\n");
	} else if (v <= 16) {
		printf("%u\n", v);
	} else {
		printf("Reserved (0x%02x)\n", v);
		fail("Supported Color Bit-Depth of Sub-Channel Red value is 0x%02x.\n", v);
	}
	v = x[0x23];
	printf("    Supported Color Bit-Depth of Sub-Channel 0 (Cb/Pb): ");
	if (!v) {
		printf("No Information\n");
	} else if (v <= 16) {
		printf("%u\n", v);
	} else {
		printf("Reserved (0x%02x)\n", v);
		fail("Supported Color Bit-Depth of Sub-Channel Cb/Pb value is 0x%02x.\n", v);
	}
	v = x[0x24];
	printf("    Supported Color Bit-Depth of Sub-Channel 1 (Y): ");
	if (!v) {
		printf("No Information\n");
	} else if (v <= 16) {
		printf("%u\n", v);
	} else {
		printf("Reserved (0x%02x)\n", v);
		fail("Supported Color Bit-Depth of Sub-Channel Y value is 0x%02x.\n", v);
	}
	v = x[0x25];
	printf("    Supported Color Bit-Depth of Sub-Channel 2 (Cr/Pr): ");
	if (!v) {
		printf("No Information\n");
	} else if (v <= 16) {
		printf("%u\n", v);
	} else {
		printf("Reserved (0x%02x)\n", v);
		fail("Supported Color Bit-Depth of Sub-Channel Cr/Pr value is 0x%02x.\n", v);
	}

	v = x[0x26];
	printf("    Aspect Ratio Conversion Modes:");
	if (!v) {
		printf(" None\n");
	} else {
		printf("\n");
		printf("      Full Mode: %s\n", (v & 0x80) ? "Yes" : "No");
		printf("      Zoom Mode: %s\n", (v & 0x40) ? "Yes" : "No");
		printf("      Squeeze (Side Bars/Letterbox) Mode: %s\n", (v & 0x20) ? "Yes" : "No");
		printf("      Variable (Expand/Shrink) Mode: %s\n", (v & 0x10) ? "Yes" : "No");
	}
	if (v & 0x0f)
		fail("Bits 3-0 of byte 0x26 should be 0.\n");
}

void edid_state::parse_display_xfer(const unsigned char *x)
{
	data_block = "Display Transfer Characteristics - Gamma";
	printf("  %s:\n", data_block.c_str());

	unsigned char v = x[0x51];
	unsigned num_entries = v & 0x3f;

	switch ((v & 0xc0) >> 6) {
	case 0x00:
		printf("    No Display Transfer Characteristics\n");
		if (!memchk(x + 0x51, 46))
			fail("Bytes 0x51-0x7e should be 0.\n");
		return;
	case 0x03:
		fail("Bits 7-6 of byte 0x51 cannot be 0x03.\n");
		return;
	default:
		break;
	}

	if (((v & 0xc0) >> 6) == 0x01) {
		if (!num_entries || num_entries > 45)
			fail("White Curve with %u entries.\n", num_entries);
		if (num_entries > 45)
			num_entries = 45;
		if (!memchk(x + 0x52 + num_entries, 45 - num_entries))
			fail("Bytes 0x%02x-0x7e should be 0.\n", 0x52 + num_entries);
		printf("    White Curve (%u entries):\n", num_entries);
		hex_block("      ", x + 0x52, num_entries, false, 15);
	} else {
		if (!num_entries || num_entries > 15)
			fail("Sub-Channel Curve with %u entries.\n", num_entries);
		if (num_entries > 15)
			num_entries = 15;
		printf("    Sub-Channel 0 (Blue) Curve with %u entries:\n", num_entries);
		hex_block("      ", x + 0x52, num_entries, false);
		if (!memchk(x + 0x52 + num_entries, 15 - num_entries))
			fail("Bytes 0x%02x-0x7e should be 0.\n", 0x52 + num_entries);
		printf("    Sub-Channel 1 (Green) Curve with %u entries:\n", num_entries);
		hex_block("      ", x + 0x52 + 15, num_entries, false);
		if (!memchk(x + 0x52 + 15 + num_entries, 15 - num_entries))
			fail("Bytes 0x%02x-0x7e should be 0.\n", 0x52 + 15 + num_entries);
		printf("    Sub-Channel 2 (Red) Curve with %u entries:\n", num_entries);
		hex_block("      ", x + 0x52 + 30, num_entries, false);
		if (!memchk(x + 0x52 + 30 + num_entries, 15 - num_entries))
			fail("Bytes 0x%02x-0x7e should be 0.\n", 0x52 + 30 + num_entries);
	}
}

void edid_state::parse_di_ext_block(const unsigned char *x)
{
	printf("  Version: %u\n", x[1]);
	if (!x[1])
		fail("Invalid version 0.\n");

	parse_digital_interface(x);
	parse_display_device(x);
	parse_display_caps(x);
	if (!memchk(x + 0x27, 16))
		fail("Bytes 0x27-0x36 should be 0.\n");
	if (!memchk(x + 0x37, 17))
		fail("Bytes 0x37-0x47 should be 0.\n");
	if (!memchk(x + 0x48, 9))
		fail("Bytes 0x48-0x50 should be 0.\n");
	parse_display_xfer(x);
}
