// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2020 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil+cisco@kernel.org>
 */

#include <math.h>

#include "edid-decode.h"

static const char *bpc444[] = {"6", "8", "10", "12", "14", "16", NULL, NULL};
static const char *bpc4xx[] = {"8", "10", "12", "14", "16", NULL, NULL, NULL};
static const char *audiorates[] = {"32", "44.1", "48", NULL, NULL, NULL, NULL, NULL};

// misc functions

static void print_flags(const char *label, unsigned char flag_byte,
			const char **flags, bool reverse = false)
{
	if (!flag_byte)
		return;

	unsigned countflags = 0;

	printf("%s: ", label);
	for (unsigned i = 0; i < 8; i++) {
		if (flag_byte & (1 << (reverse ? 7 - i : i))) {
			if (countflags)
				printf(", ");
			if (flags[i])
				printf("%s", flags[i]);
			else
				printf("Undefined (%u)", i);
			countflags++;
		}
	}
	printf("\n");
}

void edid_state::check_displayid_datablock_revision(unsigned char hdr,
						    unsigned char valid_flags,
						    unsigned char rev)
{
	unsigned char revision = hdr & 7;
	unsigned char flags = hdr & ~7 & ~valid_flags;

	if (revision != rev)
		warn("Unexpected revision (%u != %u).\n", revision, rev);
	if (flags)
		warn("Unexpected flags (0x%02x).\n", flags);
}

static bool check_displayid_datablock_length(const unsigned char *x,
					     unsigned expectedlenmin = 0,
					     unsigned expectedlenmax = 128 - 2 - 5 - 3,
					     unsigned payloaddumpstart = 0)
{
	unsigned char len = x[2];

	if (expectedlenmin == expectedlenmax && len != expectedlenmax)
		fail("DisplayID payload length is different than expected (%d != %d).\n", len, expectedlenmax);
	else if (len > expectedlenmax)
		fail("DisplayID payload length is greater than expected (%d > %d).\n", len, expectedlenmax);
	else if (len < expectedlenmin)
		fail("DisplayID payload length is less than expected (%d < %d).\n", len, expectedlenmin);
	else
		return true;

	if (len > payloaddumpstart)
		hex_block("    ", x + 3 + payloaddumpstart, len - payloaddumpstart);
	return false;
}

// tag 0x00 and 0x20

void edid_state::parse_displayid_product_id(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1]);

	dispid.has_product_identification = true;
	printf("    Product Code: %u\n", x[6] | (x[7] << 8));
	unsigned sn = x[8] | (x[9] << 8) | (x[10] << 16) | (x[11] << 24);
	if (sn) {
		if (hide_serial_numbers)
			printf("    Serial Number: ...\n");
		else
			printf("    Serial Number: %u\n", sn);
	}
	unsigned week = x[12];
	unsigned year = 2000 + x[13];
	printf("    %s: %u",
	       week == 0xff ? "Model Year" : "Year of Manufacture", year);
	if (week && week <= 0x36)
		printf(", Week %u", week);
	printf("\n");
	if (x[14]) {
		const unsigned char maxlen = EDID_PAGE_SIZE - 15;
		char buf[maxlen];
		if (x[14] >= maxlen) {
			fail("Product ID length is more than expected (%u >= %u).\n", x[14], maxlen);
			return;
		}
		memcpy(buf, x + 15, x[14]);
		buf[x[14]] = 0;
		printf("    Product ID: %s\n", buf);
	}
}

// tag 0x01

static const char *feature_support_flags[] = {
	"De-interlacing",
	"Support ACP, ISRC1, or ISRC2packets",
	"Fixed pixel format",
	"Fixed timing",
	"Power management (DPM)",
	"Audio input override",
	"Separate audio inputs provided",
	"Audio support on video interface"
};

static void print_flag_lines(const char *indent, const char *label,
			     unsigned char flag_byte, const char **flags)
{
	if (flag_byte) {
		printf("%s\n", label);

		for (int i = 0; i < 8; i++)
			if (flag_byte & (1 << i))
				printf("%s%s\n", indent, flags[i]);
	}
}

void edid_state::set_displayid_native_res(unsigned w, unsigned h)
{
	if (dispid.native_width &&
	    (dispid.native_width != w || dispid.native_height != h)) {
		fail("Native resolution mismatch: %ux%u -> %ux%u.\n",
		     dispid.native_width, dispid.native_height, w, h);
		return;
	}

	if (!w && !h)
		return;

	if (!w ^ !h) {
		fail("Invalid Native Pixel Format %ux%u.\n", w, h);
	} else {
		dispid.native_width = w;
		dispid.native_height = h;
	}
}

void edid_state::parse_displayid_parameters(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1]);

	if (!check_displayid_datablock_length(x, 12, 12))
		return;

	if (dispid.has_display_parameters)
		fail("Duplicate Display Parameters Data Block.\n");
	dispid.has_display_parameters = true;
	dispid.image_width = (x[4] << 8) + x[3];
	dispid.image_height = (x[6] << 8) + x[5];
	if (dispid.image_width > image_width ||
	    dispid.image_height > image_height) {
		image_width = dispid.image_width;
		image_height = dispid.image_height;
	}
	printf("    Image size: %.1f mm x %.1f mm\n",
	       dispid.image_width / 10.0, dispid.image_height / 10.0);
	unsigned w = (x[8] << 8) + x[7];
	unsigned h = (x[10] << 8) + x[9];
	printf("    Display native pixel format: %ux%u\n", w, h);
	set_displayid_native_res(w, h);
	print_flag_lines("      ", "    Feature support flags:",
			 x[11], feature_support_flags);

	if (x[12] != 0xff)
		printf("    Gamma: %.2f\n", ((x[12] + 100.0) / 100.0));
	printf("    Aspect ratio: %.2f\n", ((x[13] + 100.0) / 100.0));
	printf("    Dynamic bpc native: %d\n", (x[14] & 0xf) + 1);
	printf("    Dynamic bpc overall: %d\n", ((x[14] >> 4) & 0xf) + 1);
}

// tag 0x02

static const char *std_colorspace_ids[] = {
	"sRGB",
	"BT.601",
	"BT.709",
	"Adobe RGB",
	"DCI-P3",
	"NTSC",
	"EBU",
	"Adobe Wide Gamut RGB",
	"DICOM"
};

static double fp2d(unsigned short fp)
{
	return fp / 4096.0;
}

void edid_state::parse_displayid_color_characteristics(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1], 0xf8, 1);

	unsigned cie_year = (x[1] & 0x80) ? 1976 : 1931;
	unsigned xfer_id = (x[1] >> 3) & 0x0f;
	unsigned num_whitepoints = x[3] & 0x0f;
	unsigned num_primaries = (x[3] >> 4) & 0x07;
	bool temporal_color = x[3] & 0x80;
	unsigned offset = 4;

	printf("    Uses %s color\n", temporal_color ? "temporal" : "spatial");
	printf("    Uses %u CIE (x, y) coordinates\n", cie_year);
	if (xfer_id) {
		printf("    Associated with Transfer Characteristics Data Block with Identifier %u\n", xfer_id);
		if (!(dispid.preparsed_xfer_ids & (1 << xfer_id)))
			fail("Missing Transfer Characteristics Data Block with Identifier %u.\n", xfer_id);
	}
	if (!num_primaries) {
		printf("    Uses color space %s\n",
		       x[4] >= ARRAY_SIZE(std_colorspace_ids) ? "Reserved" :
								std_colorspace_ids[x[4]]);
		offset++;
	}
	for (unsigned i = 0; i < num_primaries; i++) {
		unsigned idx = offset + 3 * i;

		printf("    Primary #%u: (%.4f, %.4f)\n", i,
		       fp2d(x[idx] | ((x[idx + 1] & 0x0f) << 8)),
		       fp2d(((x[idx + 1] & 0xf0) >> 4) | (x[idx + 2] << 4)));
	}
	offset += 3 * num_primaries;
	for (unsigned i = 0; i < num_whitepoints; i++) {
		unsigned idx = offset + 3 * i;

		printf("    White point #%u: (%.4f, %.4f)\n", i,
		       fp2d(x[idx] | ((x[idx + 1] & 0x0f) << 8)),
		       fp2d(((x[idx + 1] & 0xf0) >> 4) | (x[idx + 2] << 4)));
	}
}

// tag 0x03 and 0x22

void edid_state::parse_displayid_type_1_7_timing(const unsigned char *x,
						 bool type7, unsigned block_rev, bool is_cta)
{
	struct timings t = {};
	unsigned hbl, vbl;
	std::string name = is_cta ? std::string("VTDB ") + std::to_string(cta.vec_vtdbs.size() + 1) : "DTD";
	std::string s("aspect ");

	dispid.has_type_1_7 = true;
	t.pixclk_khz = (type7 ? 1 : 10) * (1 + (x[0] + (x[1] << 8) + (x[2] << 16)));
	switch (x[3] & 0xf) {
	case 0:
		s += "1:1";
		t.hratio = t.vratio = 1;
		break;
	case 1:
		s += "5:4";
		t.hratio = 5;
		t.vratio = 4;
		break;
	case 2:
		s += "4:3";
		t.hratio = 4;
		t.vratio = 3;
		break;
	case 3:
		s += "15:9";
		t.hratio = 15;
		t.vratio = 9;
		break;
	case 4:
		s += "16:9";
		t.hratio = 16;
		t.vratio = 9;
		break;
	case 5:
		s += "16:10";
		t.hratio = 16;
		t.vratio = 10;
		break;
	case 6:
		s += "64:27";
		t.hratio = 64;
		t.vratio = 27;
		break;
	case 7:
		s += "256:135";
		t.hratio = 256;
		t.vratio = 135;
		break;
	case 8:
		s += "undefined";
		break;
	default:
		s += "reserved";
		fail("Unknown aspect 0x%02x.\n", x[3] & 0xf);
		break;
	}
	switch ((x[3] >> 5) & 0x3) {
	case 0:
		s += ", no 3D stereo";
		break;
	case 1:
		s += ", 3D stereo";
		dispid.has_stereo = true;
		break;
	case 2:
		s += ", 3D stereo depends on user action";
		dispid.has_stereo = true;
		break;
	case 3:
		s += ", reserved";
		fail("Reserved stereo 0x03.\n");
		break;
	}
	if (block_rev >= 2 && (x[3] & 0x80)) {
		s += ", YCbCr 4:2:0";
		dispid.has_ycbcr_420 = true;
	}

	t.hact = 1 + (x[4] | (x[5] << 8));
	hbl = 1 + (x[6] | (x[7] << 8));
	t.hfp = 1 + (x[8] | ((x[9] & 0x7f) << 8));
	t.hsync = 1 + (x[10] | (x[11] << 8));
	t.hbp = hbl - t.hfp - t.hsync;
	if ((x[9] >> 7) & 0x1)
		t.pos_pol_hsync = true;
	t.vact = 1 + (x[12] | (x[13] << 8));
	vbl = 1 + (x[14] | (x[15] << 8));
	t.vfp = 1 + (x[16] | ((x[17] & 0x7f) << 8));
	t.vsync = 1 + (x[18] | (x[19] << 8));
	t.vbp = vbl - t.vfp - t.vsync;
	if ((x[17] >> 7) & 0x1)
		t.pos_pol_vsync = true;

	if (x[3] & 0x10) {
		t.interlaced = true;
		t.vfp /= 2;
		t.vsync /= 2;
		t.vbp /= 2;
	}
	if (block_rev < 2 && (x[3] & 0x80)) {
		s += ", preferred";
		dispid.preferred_timings.push_back(timings_ext(t, "DTD", s));
	}

	print_timings("    ", &t, name.c_str(), s.c_str(), true);
	if (is_cta) {
		timings_ext te(t, name.c_str(), s);
		cta.vec_vtdbs.push_back(te);

		// Only use a T7VTDB if is cannot be expressed by a
		// DTD or a T10VTDB.
		if (t.hact <= 4095 && t.vact <= 4095 &&
		    t.pixclk_khz <= 655360 && !(x[3] & 0xe0)) {
			fail("This T7VTDB can be represented as an 18-byte DTD.\n");
			return;
		}
		unsigned htot = t.hact + t.hfp + t.hsync + t.hbp;
		unsigned vtot = t.vact + t.vfp + t.vsync + t.vbp;
		if (htot * vtot == 0) {
			fail("Cannot calculate refresh rate: htot * vtot is 0.\n");
			return;
		}
		unsigned refresh = (t.pixclk_khz * 1000ULL) / (htot * vtot);

		for (unsigned rb = RB_NONE; rb <= RB_CVT_V3; rb++) {
			timings cvt_t = calc_cvt_mode(t.hact, t.vact, refresh, rb);
			if (match_timings(t, cvt_t)) {
				fail("This T7VTDB can be represented as a T10VTDB.\n");
				return;
			}
		}
		timings cvt_t = calc_cvt_mode(t.hact, t.vact, refresh, RB_CVT_V3,
					      false, false, true);
		if (match_timings(t, cvt_t))
			fail("This T7VTDB can be represented as a T10VTDB.\n");
	}
}

// tag 0x04

void edid_state::parse_displayid_type_2_timing(const unsigned char *x)
{
	struct timings t = {};
	unsigned hbl, vbl;
	std::string s("aspect ");

	t.pixclk_khz = 10 * (1 + (x[0] + (x[1] << 8) + (x[2] << 16)));
	t.hact = 8 + 8 * (x[4] | ((x[5] & 0x01) << 8));
	hbl = 8 + 8 * ((x[5] & 0xfe) >> 1);
	t.hfp = 8 + 8 * ((x[6] & 0xf0) >> 4);
	t.hsync = 8 + 8 * (x[6] & 0xf);
	t.hbp = hbl - t.hfp - t.hsync;
	if ((x[3] >> 3) & 0x1)
		t.pos_pol_hsync = true;
	t.vact = 1 + (x[7] | ((x[8] & 0xf) << 8));
	vbl = 1 + x[9];
	t.vfp = 1 + (x[10] >> 4);
	t.vsync = 1 + (x[10] & 0xf);
	t.vbp = vbl - t.vfp - t.vsync;
	if ((x[17] >> 2) & 0x1)
		t.pos_pol_vsync = true;

	if (x[3] & 0x10) {
		t.interlaced = true;
		t.vfp /= 2;
		t.vsync /= 2;
		t.vbp /= 2;
	}

	calc_ratio(&t);

	s += std::to_string(t.hratio) + ":" + std::to_string(t.vratio);

	switch ((x[3] >> 5) & 0x3) {
	case 0:
		s += ", no 3D stereo";
		break;
	case 1:
		s += ", 3D stereo";
		dispid.has_stereo = true;
		break;
	case 2:
		s += ", 3D stereo depends on user action";
		dispid.has_stereo = true;
		break;
	case 3:
		s += ", reserved";
		fail("Reserved stereo 0x03.\n");
		break;
	}
	if (x[3] & 0x80) {
		s += ", preferred";
		dispid.preferred_timings.push_back(timings_ext(t, "DTD", s));
	}

	print_timings("    ", &t, "DTD", s.c_str(), true);
}

// tag 0x05

void edid_state::parse_displayid_type_3_timing(const unsigned char *x)
{
	struct timings t = {};
	std::string s("aspect ");

	switch (x[0] & 0xf) {
	case 0:
		s += "1:1";
		t.hratio = t.vratio = 1;
		break;
	case 1:
		s += "5:4";
		t.hratio = 5;
		t.vratio = 4;
		break;
	case 2:
		s += "4:3";
		t.hratio = 4;
		t.vratio = 3;
		break;
	case 3:
		s += "15:9";
		t.hratio = 15;
		t.vratio = 9;
		break;
	case 4:
		s += "16:9";
		t.hratio = 16;
		t.vratio = 9;
		break;
	case 5:
		s += "16:10";
		t.hratio = 16;
		t.vratio = 10;
		break;
	case 6:
		s += "64:27";
		t.hratio = 64;
		t.vratio = 27;
		break;
	case 7:
		s += "256:135";
		t.hratio = 256;
		t.vratio = 135;
		break;
	case 8:
		s += "undefined";
		t.hratio = t.vratio = 1;
		break;
	default:
		s += "reserved";
		t.hratio = t.vratio = 1;
		fail("Unknown aspect 0x%02x.\n", x[0] & 0xf);
		break;
	}

	t.rb = ((x[0] & 0x70) >> 4) == 1 ? RB_CVT_V1 : RB_NONE;
	t.hact = 8 + 8 * x[1];
	t.vact = t.hact * t.vratio / t.hratio;

	edid_cvt_mode(1 + (x[2] & 0x7f), t);

	if (x[0] & 0x80) {
		s += ", preferred";
		dispid.preferred_timings.push_back(timings_ext(t, "CVT", s));
	}

	print_timings("    ", &t, "CVT", s.c_str());
}

// tag 0x06 and 0x23

void edid_state::parse_displayid_type_4_8_timing(unsigned char type, unsigned short id, bool is_cta)
{
	const struct timings *t = NULL;
	char type_name[16];

	switch (type) {
	case 0: t = find_dmt_id(id); sprintf(type_name, "DMT 0x%02x", id); break;
	case 1: t = find_vic_id(id); sprintf(type_name, "VIC %3u", id); break;
	case 2: t = find_hdmi_vic_id(id); sprintf(type_name, "HDMI VIC %u", id); break;
	default: break;
	}
	if (t)
		print_timings("    ", t, type_name);
	if (t && is_cta && !cta.t8vtdb.is_valid()) {
		timings_ext te(*t, type_name, "");
		cta.t8vtdb = te;
	}
}

// tag 0x09

void edid_state::parse_displayid_video_timing_range_limits(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1]);

	if (!check_displayid_datablock_length(x, 15, 15))
		return;
	printf("    Pixel Clock: %.3f-%.3f MHz\n",
	       (double)((x[3] | (x[4] << 8) | (x[5] << 16)) + 1) / 100.0,
	       (double)((x[6] | (x[7] << 8) | (x[8] << 16)) + 1) / 100.0);
	printf("    Horizontal Frequency: %u-%u kHz\n", x[9], x[10]);
	printf("    Minimum Horizontal Blanking: %u pixels\n", x[11] | (x[12] << 8));
	printf("    Vertical Refresh: %u-%u Hz\n", x[13], x[14]);
	printf("    Minimum Vertical Blanking: %u lines\n", x[15] | (x[16] << 8));
	if (x[17] & 0x80)
		printf("    Supports Interlaced\n");
	if (x[17] & 0x40)
		printf("    Supports CVT\n");
	if (x[17] & 0x20)
		printf("    Supports CVT Reduced Blanking\n");
	if (x[17] & 0x10)
		printf("    Discrete frequency display device\n");
}

// tag 0x0a and 0x0b

void edid_state::parse_displayid_string(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1]);
	if (check_displayid_datablock_length(x))
		printf("    Text: '%s'\n", extract_string(x + 3, x[2], true));
}

// tag 0x0c

void edid_state::parse_displayid_display_device(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1]);

	if (!check_displayid_datablock_length(x, 13, 13))
		return;

	printf("    Display Device Technology: ");
	switch (x[3]) {
	case 0x00: printf("Monochrome CRT\n"); break;
	case 0x01: printf("Standard tricolor CRT\n"); break;
	case 0x02: printf("Other/undefined CRT\n"); break;
	case 0x10: printf("Passive matrix TN\n"); break;
	case 0x11: printf("Passive matrix cholesteric LC\n"); break;
	case 0x12: printf("Passive matrix ferroelectric LC\n"); break;
	case 0x13: printf("Other passive matrix LC type\n"); break;
	case 0x14: printf("Active-matrix TN\n"); break;
	case 0x15: printf("Active-matrix IPS (all types)\n"); break;
	case 0x16: printf("Active-matrix VA (all types)\n"); break;
	case 0x17: printf("Active-matrix OCB\n"); break;
	case 0x18: printf("Active-matrix ferroelectric\n"); break;
	case 0x1f: printf("Other LC type\n"); break;
	case 0x20: printf("DC plasma\n"); break;
	case 0x21: printf("AC plasma\n"); break;
	}
	switch (x[3] & 0xf0) {
	case 0x30: printf("Electroluminescent, except OEL/OLED\n"); break;
	case 0x40: printf("Inorganic LED\n"); break;
	case 0x50: printf("Organic LED/OEL\n"); break;
	case 0x60: printf("FED or sim. \"cold-cathode,\" phosphor-based types\n"); break;
	case 0x70: printf("Electrophoretic\n"); break;
	case 0x80: printf("Electrochromic\n"); break;
	case 0x90: printf("Electromechanical\n"); break;
	case 0xa0: printf("Electrowetting\n"); break;
	case 0xf0: printf("Other type not defined here\n"); break;
	}
	printf("    Display operating mode: ");
	switch (x[4] >> 4) {
	case 0x00: printf("Direct-view reflective, ambient light\n"); break;
	case 0x01: printf("Direct-view reflective, ambient light, also has light source\n"); break;
	case 0x02: printf("Direct-view reflective, uses light source\n"); break;
	case 0x03: printf("Direct-view transmissive, ambient light\n"); break;
	case 0x04: printf("Direct-view transmissive, ambient light, also has light source\n"); break;
	case 0x05: printf("Direct-view transmissive, uses light source\n"); break;
	case 0x06: printf("Direct-view emissive\n"); break;
	case 0x07: printf("Direct-view transflective, backlight off by default\n"); break;
	case 0x08: printf("Direct-view transflective, backlight on by default\n"); break;
	case 0x09: printf("Transparent display, ambient light\n"); break;
	case 0x0a: printf("Transparent emissive display\n"); break;
	case 0x0b: printf("Projection device using reflective light modulator\n"); break;
	case 0x0c: printf("Projection device using transmissive light modulator\n"); break;
	case 0x0d: printf("Projection device using emissive image transducer\n"); break;
	default: printf("Reserved\n"); break;
	}
	if (x[4] & 0x08)
		printf("    The backlight may be switched on and off\n");
	if (x[4] & 0x04)
		printf("    The backlight's intensity can be controlled\n");
	unsigned w = x[5] | (x[6] << 8);
	unsigned h = x[7] | (x[8] << 8);

	if (w) w++;
	if (h) h++;

	printf("    Display native pixel format: %ux%u\n", w, h);
	set_displayid_native_res(w, h);
	printf("    Aspect ratio and orientation:\n");
	printf("      Aspect Ratio: %.2f\n", (100 + x[9]) / 100.0);
	unsigned char v = x[0x0a];
	printf("      Default Orientation: ");
	switch ((v & 0xc0) >> 6) {
	case 0x00: printf("Landscape\n"); break;
	case 0x01: printf("Portrait\n"); break;
	case 0x02: printf("Not Fixed\n"); break;
	case 0x03: printf("Undefined\n"); break;
	}
	printf("      Rotation Capability: ");
	switch ((v & 0x30) >> 4) {
	case 0x00: printf("None\n"); break;
	case 0x01: printf("Can rotate 90 degrees clockwise\n"); break;
	case 0x02: printf("Can rotate 90 degrees counterclockwise\n"); break;
	case 0x03: printf("Can rotate 90 degrees in either direction)\n"); break;
	}
	printf("      Zero Pixel Location: ");
	switch ((v & 0x0c) >> 2) {
	case 0x00: printf("Upper Left\n"); break;
	case 0x01: printf("Upper Right\n"); break;
	case 0x02: printf("Lower Left\n"); break;
	case 0x03: printf("Lower Right\n"); break;
	}
	printf("      Scan Direction: ");
	switch (v & 0x03) {
	case 0x00: printf("Not defined\n"); break;
	case 0x01: printf("Fast Scan is on the Major (Long) Axis and Slow Scan is on the Minor Axis\n"); break;
	case 0x02: printf("Fast Scan is on the Minor (Short) Axis and Slow Scan is on the Major Axis\n"); break;
	case 0x03: printf("Reserved\n");
		   fail("Scan Direction used the reserved value 0x03.\n");
		   break;
	}
	printf("    Sub-pixel layout/configuration/shape: ");
	switch (x[0x0b]) {
	case 0x00: printf("Not defined\n"); break;
	case 0x01: printf("RGB vertical stripes\n"); break;
	case 0x02: printf("RGB horizontal stripes\n"); break;
	case 0x03: printf("Vertical stripes using primary order\n"); break;
	case 0x04: printf("Horizontal stripes using primary order\n"); break;
	case 0x05: printf("Quad sub-pixels, red at top left\n"); break;
	case 0x06: printf("Quad sub-pixels, red at bottom left\n"); break;
	case 0x07: printf("Delta (triad) RGB sub-pixels\n"); break;
	case 0x08: printf("Mosaic\n"); break;
	case 0x09: printf("Quad sub-pixels, RGB + 1 additional color\n"); break;
	case 0x0a: printf("Five sub-pixels, RGB + 2 additional colors\n"); break;
	case 0x0b: printf("Six sub-pixels, RGB + 3 additional colors\n"); break;
	case 0x0c: printf("Clairvoyante, Inc. PenTile Matrix (tm) layout\n"); break;
	default: printf("Reserved\n"); break;
	}
	printf("    Horizontal and vertical dot/pixel pitch: %.2fx%.2f mm\n",
	       (double)(x[0x0c]) / 100.0, (double)(x[0x0d]) / 100.0);
	printf("    Color bit depth: %u\n", x[0x0e] & 0x0f);
	v = x[0x0f];
	printf("    Response time for %s transition: %u ms\n",
	       (v & 0x80) ? "white-to-black" : "black-to-white", v & 0x7f);
}

// tag 0x0d

void edid_state::parse_displayid_intf_power_sequencing(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1]);

	if (!check_displayid_datablock_length(x, 6, 6))
		return;

	printf("    Power Sequence T1 Range: %.1f-%u.0 ms\n", (x[3] >> 4) / 10.0, (x[3] & 0xf) * 2);
	printf("    Power Sequence T2 Range: 0.0-%u.0 ms\n", (x[4] & 0x3f) * 2);
	printf("    Power Sequence T3 Range: 0.0-%u.0 ms\n", (x[5] & 0x3f) * 2);
	printf("    Power Sequence T4 Min: %u.0 ms\n", (x[6] & 0x7f) * 10);
	printf("    Power Sequence T5 Min: %u.0 ms\n", (x[7] & 0x3f) * 10);
	printf("    Power Sequence T6 Min: %u.0 ms\n", (x[8] & 0x3f) * 10);
}

// tag 0x0e

void edid_state::parse_displayid_transfer_characteristics(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1], 0xf0, 1);

	if (!check_displayid_datablock_length(x, 1, 248))
		return;

	unsigned xfer_id = x[1] >> 4;
	bool first_is_white = x[3] & 0x80;
	bool four_param = x[3] & 0x20;

	if (xfer_id) {
		printf("    Transfer Characteristics Data Block Identifier: %u\n", xfer_id);
		if (!(dispid.preparsed_color_ids & (1 << xfer_id)))
			fail("Missing Color Characteristics Data Block using Identifier %u.\n", xfer_id);
	}
	if (first_is_white)
		printf("    The first curve is the 'white' transfer characteristic\n");
	if (x[3] & 0x40)
		printf("    Individual response curves\n");

	unsigned offset = 4;
	unsigned len = x[2] - 1;

	for (unsigned i = 0; len; i++) {
		unsigned samples = x[offset];

		if (!samples) {
			fail("Found a curve with 0 samples.\n");
			samples = 1;
		}

		// If not using four_param, then decrement the number of samples
		// by one since the last sample isn't stored.
		if (!four_param)
			samples--;

		if (four_param && samples != 5) {
			fail("Expected 5 samples.\n");
			samples = 5;
		}
		if (samples + 1 > len) {
			fail("Length %u is too small to hold %u samples at offset %u.\n",
			     len, samples, offset + 1);
			break;
		}

		if ((x[3] & 0x80) && !i)
			printf("    White curve:      ");
		else
			printf("    Response curve #%u:",
			       i - first_is_white);

		if (four_param) {
			printf(" A0=%u A1=%u A2=%u A3=%u Gamma=%.2f\n",
			       x[offset + 1], x[offset + 2], x[offset + 3], x[offset + 4],
			       (double)(x[offset + 5] + 100.0) / 100.0);
		} else {
			double sum = 0;

			// The number of samples is the number of samples in the
			// curve, where the last sample is not actually stored
			// since it is always 0x3ff.
			for (unsigned j = offset + 1; j < offset + samples; j++) {
				sum += x[j];
				printf(" %.2f", sum * 100.0 / 1023.0);
			}
			printf(" 100.00\n");
		}
		offset += samples + 1;
		len -= samples + 1;
	}
}

// tag 0x0f

void edid_state::parse_displayid_display_intf(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1]);

	if (!check_displayid_datablock_length(x, 10, 10))
		return;

	dispid.has_display_interface_features = true;
	printf("    Interface Type: ");
	switch (x[3] >> 4) {
	case 0x00:
		switch (x[3] & 0xf) {
		case 0x00: printf("Analog 15HD/VGA\n"); break;
		case 0x01: printf("Analog VESA NAVI-V (15HD)\n"); break;
		case 0x02: printf("Analog VESA NAVI-D\n"); break;
		default: printf("Reserved\n"); break;
		}
		break;
	case 0x01: printf("LVDS\n"); break;
	case 0x02: printf("TMDS\n"); break;
	case 0x03: printf("RSDS\n"); break;
	case 0x04: printf("DVI-D\n"); break;
	case 0x05: printf("DVI-I, analog\n"); break;
	case 0x06: printf("DVI-I, digital\n"); break;
	case 0x07: printf("HDMI-A\n"); break;
	case 0x08: printf("HDMI-B\n"); break;
	case 0x09: printf("MDDI\n"); break;
	case 0x0a: printf("DisplayPort\n"); break;
	case 0x0b: printf("Proprietary Digital Interface\n"); break;
	default: printf("Reserved\n"); break;
	}
	if (x[3] >> 4)
		printf("    Number of Links: %u\n", x[3] & 0xf);
	printf("    Interface Standard Version: %u.%u\n",
	       x[4] >> 4, x[4] & 0xf);
	print_flags("    Supported bpc for RGB encoding", x[5], bpc444);
	print_flags("    Supported bpc for YCbCr 4:4:4 encoding", x[6], bpc444);
	print_flags("    Supported bpc for YCbCr 4:2:2 encoding", x[7], bpc4xx);
	printf("    Supported Content Protection: ");
	switch (x[8] & 0xf) {
	case 0x00: printf("None\n"); break;
	case 0x01: printf("HDCP "); break;
	case 0x02: printf("DTCP "); break;
	case 0x03: printf("DPCP "); break;
	default: printf("Reserved "); break;
	}
	if (x[8] & 0xf)
		printf("%u.%u\n", x[9] >> 4, x[9] & 0xf);
	unsigned char v = x[0x0a] & 0xf;
	printf("    Spread Spectrum: ");
	switch (x[0x0a] >> 6) {
	case 0x00: printf("None\n"); break;
	case 0x01: printf("Down Spread %.1f%%\n", v / 10.0); break;
	case 0x02: printf("Center Spread %.1f%%\n", v / 10.0); break;
	case 0x03: printf("Reserved\n"); break;
	}
	switch (x[3] >> 4) {
	case 0x01:
		printf("    LVDS Color Mapping: %s mode\n",
		       (x[0x0b] & 0x10) ? "6 bit compatible" : "normal");
		if (x[0x0b] & 0x08) printf("    LVDS supports 2.8V\n");
		if (x[0x0b] & 0x04) printf("    LVDS supports 12V\n");
		if (x[0x0b] & 0x02) printf("    LVDS supports 5V\n");
		if (x[0x0b] & 0x01) printf("    LVDS supports 3.3V\n");
		printf("    LVDS %s Mode\n", (x[0x0c] & 0x04) ? "Fixed" : "DE");
		if (x[0x0c] & 0x04)
			printf("    LVDS %s Signal Level\n", (x[0x0c] & 0x02) ? "Low" : "High");
		else
			printf("    LVDS DE Polarity Active %s\n", (x[0x0c] & 0x02) ? "Low" : "High");
		printf("    LVDS Shift Clock Data Strobe at %s Edge\n", (x[0x0c] & 0x01) ? "Rising" : "Falling");
		break;
	case 0x0b:
		printf("    PDI %s Mode\n", (x[0x0b] & 0x04) ? "Fixed" : "DE");
		if (x[0x0b] & 0x04)
			printf("    PDI %s Signal Level\n", (x[0x0b] & 0x02) ? "Low" : "High");
		else
			printf("    PDI DE Polarity Active %s\n", (x[0x0b] & 0x02) ? "Low" : "High");
		printf("    PDI Shift Clock Data Strobe at %s Edge\n", (x[0x0b] & 0x01) ? "Rising" : "Falling");
		break;
	}
}

// tag 0x10 and 0x27

void edid_state::parse_displayid_stereo_display_intf(const unsigned char *x)
{
	dispid.has_stereo_display_interface = true;

	check_displayid_datablock_revision(x[1], 0xc0, 1);

	switch (x[1] >> 6) {
	case 0x00: printf("    Timings that explicitly report 3D capability\n"); break;
	case 0x01: printf("    Timings that explicitly report 3D capability & Timing Codes listed here\n"); break;
	case 0x02: printf("    All listed timings\n"); break;
	case 0x03: printf("    Only Timings Codes listed here\n"); break;
	}

	unsigned len = x[2];

	switch (x[4]) {
	case 0x00:
		printf("    Field Sequential Stereo (L/R Polarity: %s)\n",
		       (x[5] & 1) ? "0/1" : "1/0");
		break;
	case 0x01:
		printf("    Side-by-side Stereo (Left Half = %s Eye View)\n",
		       (x[5] & 1) ? "Right" : "Left");
		break;
	case 0x02:
		printf("    Pixel Interleaved Stereo:\n");
		for (unsigned y = 0; y < 8; y++) {
			unsigned char v = x[5 + y];

			printf("      ");
			for (int x = 7; x >= 0; x--)
				printf("%c", (v & (1 << x)) ? 'L' : 'R');
			printf("\n");
		}
		break;
	case 0x03:
		printf("    Dual Interface, Left and Right Separate\n");
		printf("      Carries the %s-eye view\n",
		       (x[5] & 1) ? "Right" : "Left");
		printf("      ");
		switch ((x[5] >> 1) & 3) {
		case 0x00: printf("No mirroring\n"); break;
		case 0x01: printf("Left/Right mirroring\n"); break;
		case 0x02: printf("Top/Bottom mirroring\n"); break;
		case 0x03: printf("Reserved\n"); break;
		}
		break;
	case 0x04:
		printf("    Multi-View: %u views, Interleaving Method Code: %u\n",
		       x[5], x[6]);
		break;
	case 0x05:
		printf("    Stacked Frame Stereo (Top Half = %s Eye View)\n",
		       (x[5] & 1) ? "Right" : "Left");
		break;
	case 0xff:
		printf("    Proprietary\n");
		break;
	default:
		printf("    Reserved\n");
		break;
	}
	if (!(x[1] & 0x40)) // Has No Timing Codes
		return;
	if (len < x[3] + 1U) {
		fail("Length is smaller than expected (%u < %u)\n", len, x[3] + 1);
		return;
	}
	len -= 1 + x[3];
	x += 4 + x[3];
	while (1U + (x[0] & 0x1f) <= len) {
		unsigned num_codes = x[0] & 0x1f;
		unsigned type = x[0] >> 6;
		char type_name[16];

		for (unsigned i = 1; i <= num_codes; i++) {
			switch (type) {
			case 0x00:
				sprintf(type_name, "DMT 0x%02x", x[i]);
				print_timings("    ", find_dmt_id(x[i]), type_name);
				break;
			case 0x01:
				sprintf(type_name, "VIC %3u", x[i]);
				print_timings("    ", find_vic_id(x[i]), type_name);
				break;
			case 0x02:
				sprintf(type_name, "HDMI VIC %u", x[i]);
				print_timings("    ", find_hdmi_vic_id(x[i]), type_name);
				break;
			}
		}

		len -= 1 + num_codes;
		x += 1 + num_codes;
	}
}

// tag 0x11

void edid_state::parse_displayid_type_5_timing(const unsigned char *x)
{
	struct timings t = {};
	std::string s("aspect ");

	t.hact = 1 + (x[2] | (x[3] << 8));
	t.vact = 1 + (x[4] | (x[5] << 8));
	calc_ratio(&t);
	s += std::to_string(t.hratio) + ":" + std::to_string(t.vratio);
	switch ((x[0] >> 5) & 0x3) {
	case 0:
		s += ", no 3D stereo";
		break;
	case 1:
		s += ", 3D stereo";
		dispid.has_stereo = true;
		break;
	case 2:
		s += ", 3D stereo depends on user action";
		dispid.has_stereo = true;
		break;
	case 3:
		s += ", reserved";
		fail("Reserved stereo 0x03.\n");
		break;
	}
	if (x[0] & 0x10)
		s += ", refresh rate * (1000/1001) supported";

	t.rb = RB_CVT_V2;
	if ((x[0] & 0x03) == 1)
		warn("Unexpected use of 'custom reduced blanking'.\n");
	else if ((x[0] & 0x03) > 1)
		fail("Invalid Timing Formula.\n");

	edid_cvt_mode(1 + x[6], t);

	if (x[0] & 0x80) {
		s += ", preferred";
		dispid.preferred_timings.push_back(timings_ext(t, "CVT", s));
	}

	print_timings("    ", &t, "CVT", s.c_str());
}

// tag 0x12 and 0x28

void edid_state::parse_displayid_tiled_display_topology(const unsigned char *x, bool is_v2)
{
	check_displayid_datablock_revision(x[1]);

	if (!check_displayid_datablock_length(x, 22, 22))
		return;

	dispid.has_tiled_display_topology = true;

	unsigned caps = x[3];
	unsigned num_v_tile = (x[4] & 0xf) | (x[6] & 0x30);
	unsigned num_h_tile = (x[4] >> 4) | ((x[6] >> 2) & 0x30);
	unsigned tile_v_location = (x[5] & 0xf) | ((x[6] & 0x3) << 4);
	unsigned tile_h_location = (x[5] >> 4) | (((x[6] >> 2) & 0x3) << 4);
	unsigned tile_width = x[7] | (x[8] << 8);
	unsigned tile_height = x[9] | (x[10] << 8);
	unsigned pix_mult = x[11];

	printf("    Capabilities:\n");
	printf("      Behavior if it is the only tile: ");
	switch (caps & 0x07) {
	case 0x00: printf("Undefined\n"); break;
	case 0x01: printf("Image is displayed at the Tile Location\n"); break;
	case 0x02: printf("Image is scaled to fit the entire tiled display\n"); break;
	case 0x03: printf("Image is cloned to all other tiles\n"); break;
	default: printf("Reserved\n"); break;
	}
	printf("      Behavior if more than one tile and fewer than total number of tiles: ");
	switch ((caps >> 3) & 0x03) {
	case 0x00: printf("Undefined\n"); break;
	case 0x01: printf("Image is displayed at the Tile Location\n"); break;
	default: printf("Reserved\n"); break;
	}
	if (caps & 0x80)
		printf("    Tiled display consists of a single physical display enclosure\n");
	else
		printf("    Tiled display consists of multiple physical display enclosures\n");
	printf("    Num horizontal tiles: %u Num vertical tiles: %u\n",
	       num_h_tile + 1, num_v_tile + 1);
	printf("    Tile location: %u, %u\n", tile_h_location, tile_v_location);
	printf("    Tile resolution: %ux%u\n", tile_width + 1, tile_height + 1);
	if (caps & 0x40) {
		if (pix_mult) {
			printf("    Top bezel size: %.1f pixels\n",
			       pix_mult * x[12] / 10.0);
			printf("    Bottom bezel size: %.1f pixels\n",
			       pix_mult * x[13] / 10.0);
			printf("    Right bezel size: %.1f pixels\n",
			       pix_mult * x[14] / 10.0);
			printf("    Left bezel size: %.1f pixels\n",
			       pix_mult * x[15] / 10.0);
		} else {
			fail("Bezel information bit is set, but the pixel multiplier is zero.\n");
		}
		printf("    Tile resolution: %ux%u\n", tile_width + 1, tile_height + 1);
	} else if (pix_mult) {
		fail("No bezel information, but the pixel multiplier is non-zero.\n");
	}
	if (is_v2)
		printf("    Tiled Display Manufacturer/Vendor ID: %02X-%02X-%02X\n",
		       x[0x10], x[0x11], x[0x12]);
	else
		printf("    Tiled Display Manufacturer/Vendor ID: %c%c%c\n",
		       x[0x10], x[0x11], x[0x12]);
	printf("    Tiled Display Product ID Code: %u\n",
	       x[0x13] | (x[0x14] << 8));
	unsigned int sn = x[0x15] | (x[0x16] << 8) | (x[0x17] << 16)| (x[0x18] << 24);
	if (sn) {
		if (hide_serial_numbers)
			printf("    Tiled Display Serial Number: ...\n");
		else
			printf("    Tiled Display Serial Number: %u\n", sn);
	} else {
		fail("Tiled Display Serial Number must be non-zero.\n");
	}
}

// tag 0x13

void edid_state::parse_displayid_type_6_timing(const unsigned char *x)
{
	struct timings t = {};
	std::string s("aspect ");

	t.pixclk_khz = 1 + (x[0] + (x[1] << 8) + ((x[2] & 0x3f) << 16));
	t.hact = 1 + (x[3] | ((x[4] & 0x3f) << 8));
	if ((x[4] >> 7) & 0x1)
		t.pos_pol_hsync = true;
	unsigned hbl = 1 + (x[7] | ((x[9] & 0xf) << 8));
	t.hfp = 1 + (x[8] | ((x[9] & 0xf0) << 4));
	t.hsync = 1 + x[10];
	t.hbp = hbl - t.hfp - t.hsync;
	t.vact = 1 + (x[5] | ((x[6] & 0x3f) << 8));
	if ((x[6] >> 7) & 0x1)
		t.pos_pol_vsync = true;
	unsigned vbl = 1 + x[11];
	t.vfp = 1 + x[12];
	t.vsync = 1 + (x[13] & 0x0f);
	t.vbp = vbl - t.vfp - t.vsync;

	if (x[13] & 0x80) {
		t.interlaced = true;
		t.vfp /= 2;
		t.vsync /= 2;
		t.vbp /= 2;
	}
	calc_ratio(&t);
	s += std::to_string(t.hratio) + ":" + std::to_string(t.vratio);
	if (x[2] & 0x40) {
		double aspect_mult = x[14] * 3.0 / 256.0;
		unsigned size_mult = 1 + (x[16] >> 4);

		t.vsize_mm = size_mult * (1 + (x[15] | ((x[16] & 0xf) << 8)));
		t.hsize_mm = t.vsize_mm * aspect_mult;
	}

	switch ((x[13] >> 5) & 0x3) {
	case 0:
		s += ", no 3D stereo";
		break;
	case 1:
		s += ", 3D stereo";
		dispid.has_stereo = true;
		break;
	case 2:
		s += ", 3D stereo depends on user action";
		dispid.has_stereo = true;
		break;
	case 3:
		s += ", reserved";
		fail("Reserved stereo 0x03.\n");
		break;
	}

	if (x[2] & 0x80) {
		s += ", preferred";
		dispid.preferred_timings.push_back(timings_ext(t, "DTD", s));
	}

	print_timings("    ", &t, "DTD", s.c_str(), true);
}

static std::string ieee7542d(unsigned short fp)
{
	int exp = ((fp & 0x7c00) >> 10) - 15;
	unsigned fract = (fp & 0x3ff) | 0x400;

	if (fp == 0x8000)
		return "do not use";
	if (fp & 0x8000)
		return "reserved";
	return std::to_string(pow(2, exp) * fract / 1024.0) + " cd/m^2";
}

// tag 0x21

void edid_state::parse_displayid_parameters_v2(const unsigned char *x,
					       unsigned block_rev)
{
	check_displayid_datablock_revision(x[1], 0, (x[1] & 7) == 1);
	if (!check_displayid_datablock_length(x, 29, 29))
		return;
	if (dispid.has_display_parameters)
		fail("Duplicate Display Parameters Data Block.\n");
	dispid.has_display_parameters = true;

	unsigned hor_size = (x[4] << 8) + x[3];
	unsigned vert_size = (x[6] << 8) + x[5];

	dispid.image_width = hor_size;
	dispid.image_height = vert_size;
	if (x[1] & 0x80) {
		printf("    Image size: %u mm x %u mm\n", hor_size, vert_size);
		dispid.image_width *= 10;
		dispid.image_height *= 10;
	} else {
		printf("    Image size: %.1f mm x %.1f mm\n",
		       hor_size / 10.0, vert_size / 10.0);
	}
	if (dispid.image_width > image_width ||
	    dispid.image_height > image_height) {
		image_width = dispid.image_width;
		image_height = dispid.image_height;
	}

	unsigned w = (x[8] << 8) + x[7];
	unsigned h = (x[10] << 8) + x[9];

	printf("    Display native pixel format: %ux%u\n", w, h);
	set_displayid_native_res(w, h);

	unsigned char v = x[11];
	printf("    Scan Orientation: ");
	switch (v & 0x07) {
	case 0x00: printf("Left to Right, Top to Bottom\n"); break;
	case 0x01: printf("Right to Left, Top to Bottom\n"); break;
	case 0x02: printf("Top to Bottom, Right to Left\n"); break;
	case 0x03: printf("Bottom to Top, Right to Left\n"); break;
	case 0x04: printf("Right to Left, Bottom to Top\n"); break;
	case 0x05: printf("Left to Right, Bottom to Top\n"); break;
	case 0x06: printf("Bottom to Top, Left to Right\n"); break;
	case 0x07: printf("Top to Bottom, Left to Right\n"); break;
	}
	printf("    Luminance Information: ");
	switch ((v >> 3) & 0x03) {
	case 0x00: printf("Minimum guaranteed value\n"); break;
	case 0x01: printf("Guidance for the Source device\n"); break;
	default: printf("Reserved\n"); break;
	}
	printf("    Color Information: CIE %u\n",
	       (v & 0x40) ? 1976 : 1931);
	printf("    Audio Speaker Information: %sintegrated\n",
	       (v & 0x80) ? "not " : "");
	printf("    Native Color Chromaticity:\n");
	printf("      Primary #1:  (%.6f, %.6f)\n",
	       fp2d(x[0x0c] | ((x[0x0d] & 0x0f) << 8)),
	       fp2d(((x[0x0d] & 0xf0) >> 4) | (x[0x0e] << 4)));
	printf("      Primary #2:  (%.6f, %.6f)\n",
	       fp2d(x[0x0f] | ((x[0x10] & 0x0f) << 8)),
	       fp2d(((x[0x10] & 0xf0) >> 4) | (x[0x11] << 4)));
	printf("      Primary #3:  (%.6f, %.6f)\n",
	       fp2d(x[0x12] | ((x[0x13] & 0x0f) << 8)),
	       fp2d(((x[0x13] & 0xf0) >> 4) | (x[0x14] << 4)));
	printf("      White Point: (%.6f, %.6f)\n",
	       fp2d(x[0x15] | ((x[0x16] & 0x0f) << 8)),
	       fp2d(((x[0x16] & 0xf0) >> 4) | (x[0x17] << 4)));
	printf("    Native Maximum Luminance (Full Coverage): %s\n",
	       ieee7542d(x[0x18] | (x[0x19] << 8)).c_str());
	printf("    Native Maximum Luminance (10%% Rectangular Coverage): %s\n",
	       ieee7542d(x[0x1a] | (x[0x1b] << 8)).c_str());
	printf("    Native Minimum Luminance: %s\n",
	       ieee7542d(x[0x1c] | (x[0x1d] << 8)).c_str());
	printf("    Native Color Depth: ");
	if (!(x[0x1e] & 0x07))
		printf("Not defined\n");
	else if (bpc444[x[0x1e] & 0x07])
		printf("%s bpc\n", bpc444[x[0x1e] & 0x07]);
	else
		printf("Reserved\n");
	printf("    Display Device Technology: ");
	switch ((x[0x1e] >> 4) & 0x07) {
	case 0x00: printf("Not Specified\n"); break;
	case 0x01: printf("Active Matrix LCD\n"); break;
	case 0x02: printf("Organic LED\n"); break;
	default: printf("Reserved\n"); break;
	}
	if (block_rev)
		printf("    Display Device Theme Preference: %s\n",
		       (x[0x1e] & 0x80) ? "Dark Theme Preferred" : "No Preference");
	if (x[0x1f] != 0xff)
		printf("    Native Gamma EOTF: %.2f\n",
		       (100 + x[0x1f]) / 100.0);
}

// tag 0x24

void edid_state::parse_displayid_type_9_timing(const unsigned char *x)
{
	struct timings t = {};
	std::string s("aspect ");

	t.hact = 1 + (x[1] | (x[2] << 8));
	t.vact = 1 + (x[3] | (x[4] << 8));
	calc_ratio(&t);
	s += std::to_string(t.hratio) + ":" + std::to_string(t.vratio);
	switch ((x[0] >> 5) & 0x3) {
	case 0:
		s += ", no 3D stereo";
		break;
	case 1:
		s += ", 3D stereo";
		dispid.has_stereo = true;
		break;
	case 2:
		s += ", 3D stereo depends on user action";
		dispid.has_stereo = true;
		break;
	case 3:
		s += ", reserved";
		fail("Reserved stereo 0x03.\n");
		break;
	}
	if (x[0] & 0x10)
		s += ", refresh rate * (1000/1001) supported";

	switch (x[0] & 0x07) {
	case 1: t.rb = RB_CVT_V1; break;
	case 2: t.rb = RB_CVT_V2; break;
	default: break;
	}

	edid_cvt_mode(1 + x[5], t);

	print_timings("    ", &t, "CVT", s.c_str());
}

// tag 0x25

void edid_state::parse_displayid_dynamic_video_timings_range_limits(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1], 0, (x[1] & 7) == 1);

	if (!check_displayid_datablock_length(x, 9, 9))
		return;

	printf("    Minimum Pixel Clock: %u kHz\n",
	       1 + (x[3] | (x[4] << 8) | (x[5] << 16)));
	printf("    Maximum Pixel Clock: %u kHz\n",
	       1 + (x[6] | (x[7] << 8) | (x[8] << 16)));
	printf("    Minimum Vertical Refresh Rate: %u Hz\n", x[9]);
	if (x[1] & 7)
		printf("    Maximum Vertical Refresh Rate: %u Hz\n", x[10] + ((x[11] & 3) << 8));
	else
		printf("    Maximum Vertical Refresh Rate: %u Hz\n", x[10]);
	printf("    Seamless Dynamic Video Timing Support: %s\n",
	       (x[11] & 0x80) ? "Yes" : "No");
}

// tag 0x26

static const char *colorspace_eotf_combinations[] = {
	"sRGB",
	"BT.601",
	"BT.709/BT.1886",
	"Adobe RGB",
	"DCI-P3",
	"BT.2020",
	"BT.2020/SMPTE ST 2084",
	NULL
};

static const char *colorspace_eotf_reserved[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

static const char *colorspaces[] = {
	"Undefined",
	"sRGB",
	"BT.601",
	"BT.709",
	"Adobe RGB",
	"DCI-P3",
	"BT.2020",
	"Custom"
};

static const char *eotfs[] = {
	"Undefined",
	"sRGB",
	"BT.601",
	"BT.1886",
	"Adobe RGB",
	"DCI-P3",
	"BT.2020",
	"Gamma function",
	"SMPTE ST 2084",
	"Hybrid Log",
	"Custom"
};

void edid_state::parse_displayid_interface_features(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1]);

	if (!check_displayid_datablock_length(x, 9))
		return;

	dispid.has_display_interface_features = true;
	unsigned len = x[2];
	if (len > 0) print_flags("    Supported bpc for RGB encoding", x[3], bpc444);
	if (len > 1) print_flags("    Supported bpc for YCbCr 4:4:4 encoding", x[4], bpc444);
	if (len > 2) print_flags("    Supported bpc for YCbCr 4:2:2 encoding", x[5], bpc4xx);
	if (len > 3) print_flags("    Supported bpc for YCbCr 4:2:0 encoding", x[6], bpc4xx);
	if (len > 4 && x[7])
		printf("    Minimum pixel rate at which YCbCr 4:2:0 encoding is supported: %.3f MHz\n",
		       74.25 * x[7]);
	if (len > 5) print_flags("    Supported audio capability and features (kHz)",
				 x[8], audiorates, true);
	if (len > 6) print_flags("    Supported color space and EOTF standard combination 1",
				 x[9], colorspace_eotf_combinations);
	if (len > 7) print_flags("    Supported color space and EOTF standard combination 2",x[10], colorspace_eotf_reserved);

	unsigned i = 0;

	if (len > 8 && x[11]) {
		printf("    Supported color space and EOTF additional combinations:");
		for (i = 0; i < x[11]; i++) {
			if (i > 6) {
				printf("\n    Number of additional color space and EOTF combinations (%d) is greater than allowed (7).", x[11]);
				break;
			} else if (i + 10 > len) {
				printf("\n    Number of additional color space and EOTF combinations (%d) is too many to fit in block (%d).", x[11], len - 9);
				break;
			}

			const char *colorspace = "Out of range";
			const char *eotf = "Out of range";
			unsigned colorspace_index = (x[12 + i] >> 4) & 0xf;
			unsigned eotf_index = x[12 + i] & 0xf;

			if (colorspace_index < sizeof(colorspaces) / sizeof(colorspaces[0]))
				colorspace = colorspaces[colorspace_index];
			if (eotf_index < sizeof(eotfs) / sizeof(eotfs[0]))
				eotf = eotfs[eotf_index];

			if (i > 0)
				printf(", ");
			if (!strcmp(colorspace, eotf))
				printf("%s", colorspace);
			else
				printf("%s/%s", colorspace, eotf);
		}
		printf("\n");
	}
	check_displayid_datablock_length(x, 9 + i, 9 + i, 9 + i);
}

// tag 0x29

void edid_state::parse_displayid_ContainerID(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1]);

	if (check_displayid_datablock_length(x, 16, 16)) {
		x += 3;
		printf("    Container ID: %s\n", containerid2s(x).c_str());
	}
}

// tag 0x2a

void edid_state::parse_displayid_type_10_timing(const unsigned char *x,
						unsigned sz, bool is_cta)
{
	struct timings t = {};
	std::string name = is_cta ? std::string("VTDB ") + std::to_string(cta.vec_vtdbs.size() + 1) : "CVT";
	std::string s("aspect ");

	t.hact = 1 + (x[1] | (x[2] << 8));
	t.vact = 1 + (x[3] | (x[4] << 8));
	calc_ratio(&t);
	s += std::to_string(t.hratio) + ":" + std::to_string(t.vratio);

	switch ((x[0] >> 5) & 0x3) {
	case 0:
		s += ", no 3D stereo";
		break;
	case 1:
		s += ", 3D stereo";
		dispid.has_stereo = true;
		break;
	case 2:
		s += ", 3D stereo depends on user action";
		dispid.has_stereo = true;
		break;
	case 3:
		s += ", reserved";
		fail("Reserved stereo 0x03.\n");
		break;
	}

	switch (x[0] & 0x07) {
	case 1: t.rb = RB_CVT_V1; break;
	case 2: t.rb = RB_CVT_V2; break;
	case 3: t.rb = RB_CVT_V3; break;
	default: break;
	}

	unsigned rb = t.rb;
	unsigned rb_h_blank = rb == RB_CVT_V3 ? 80 : 0;
	bool alt_min_vblank = sz >= 8 ? (x[7] & 1) : 0;
	unsigned rb_v_min_blank = alt_min_vblank ? 300 : 460;
	unsigned rb_v_blank = rb_v_min_blank;
	bool early_vsync_rqd = false;

	if (x[0] & 0x10) {
		if (rb == RB_CVT_V2) {
			s += ", refresh rate * (1000/1001) supported";
			t.rb |= RB_ALT;
		} else if (rb == RB_CVT_V3) {
			s += ", hblank is 160 pixels";
			t.rb |= RB_ALT;
			rb_h_blank = 160;
		} else {
			fail("VR_HB must be 0.\n");
		}
	}
	if (x[0] & 0x80) {
		s += ", YCbCr 4:2:0";
		dispid.has_ycbcr_420 = true;
	}

	if (x[0] & 0x08) {
		if (rb == RB_CVT_V3) {
			early_vsync_rqd = true;
			s += ", early-vsync";
		} else {
			fail("EVS must be 0.\n");
		}
	}

	unsigned refresh = 1 + x[5] + (sz == 6 ? 0 : ((x[6] & 3) << 8));

	if (sz > 6) {
		if (rb == RB_CVT_V3) {
			unsigned delta_hblank = (x[6] >> 2) & 7;

			if (rb_h_blank == 80)
				rb_h_blank = 80 + 8 * delta_hblank;
			else if (delta_hblank <= 5)
				rb_h_blank = 160 + 8 * delta_hblank;
			else
				rb_h_blank = 160 - (delta_hblank - 5) * 8;
			if (delta_hblank)
				s += ", delta-hblank=" + std::to_string(delta_hblank);

			rb_v_blank += ((x[6] >> 5) & 7) * (alt_min_vblank ? 20 : 35);
			if (rb_v_blank > rb_v_min_blank)
				s += ", add-vblank=" + std::to_string(rb_v_blank - rb_v_min_blank);
		} else {
			if (x[6] & 0xe0)
				fail("Additional_Vertical_Blank_Time must be 0.\n");
			if (x[6] & 0x1c)
				fail("Delta_Horizontal_Blank must be 0.\n");
		}
	}

	edid_cvt_mode(refresh, t, rb_h_blank, rb_v_blank, early_vsync_rqd);

	print_timings("    ", &t, name.c_str(), s.c_str());
	if (is_cta) {
		timings_ext te(t, name.c_str(), s);
		cta.vec_vtdbs.push_back(te);
	}
}

// tag 0x2b

void edid_state::parse_displayid_adaptive_sync(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1], 0x70);

	unsigned size = 6 + ((x[1] >> 4) & 0x7);
	unsigned len = x[2];
	unsigned descriptor = 1;

	x += 3;
	if (len % size)
		fail("DisplayID payload length %u is not a multiple of %u.\n", len, size);
	while (len >= size) {
		printf("    Descriptor #%u:\n", descriptor++);

		printf("      %sNative Panel Range\n", (x[0] & 1) ? "" : "Non-");
		unsigned v = (x[0] >> 2) & 3;
		switch (v) {
		case 0: printf("      Fixed Average V-Total\n"); break;
		case 1: printf("      Fixed Average V-Total and Adaptive V-Total\n"); break;
		default:
			printf("      Reserved %u\n", v);
			fail("Use of reserved value %u.\n", v);
			break;
		}
		if (!(x[0] & 0x10))
			printf("      Supports Seamless Transition\n");
		if (x[0] & 0x02)
			printf("      'Max Single Frame Duration Increase' field value without jitter impact\n");
		if (x[0] & 0x20)
			printf("      'Max Single Frame Duration Decrease' field value without jitter impact\n");
		printf("      Max Duration Increase: %.2f ms\n", x[1] / 4.0);
		printf("      Max Duration Decrease: %.2f ms\n", x[5] / 4.0);
		printf("      Min Refresh Rate: %u Hz\n", x[2]);
		printf("      Max Refresh Rate: %u Hz\n", 1 + x[3] + (x[4] & 3) * 256);

		len -= size;
		x += size;
	}
}

// tag 0x2c

void edid_state::parse_displayid_arvr_hmd(const unsigned char *x)
{
	dispid.has_arvr_hdm = true;

	if (!native_dispid && dispid.is_arvr)
		fail("Not allowed for DisplayID Extension Blocks.\n");

	check_displayid_datablock_revision(x[1], 1);

	if (!check_displayid_datablock_length(x, 79, 79))
		return;

	// TODO: parse the DB
}

// tag 0x2d

void edid_state::parse_displayid_arvr_layer(const unsigned char *x)
{
	dispid.has_arvr_layer = true;

	if (!native_dispid && dispid.is_arvr)
		fail("Not allowed for DisplayID Extension Blocks.\n");

	check_displayid_datablock_revision(x[1], 1);

	if (!check_displayid_datablock_length(x, 20, 20))
		return;

	// TODO: parse the DB
}

// tag 0x2e

void edid_state::parse_displayid_brightness_lum_range(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1]);

	if (!check_displayid_datablock_length(x, 6, 6))
		return;

	printf("    Minimum SDR Luminance (Full Coverage): %s\n",
	       ieee7542d(x[3] | (x[4] << 8)).c_str());
	// TODO: test that this is > Native Minimum Luminance from Display Params DB
	printf("    Maximum Suggested SDR Luminance (Full Coverage): %s\n",
	       ieee7542d(x[5] | (x[6] << 8)).c_str());
	// TODO: test that this is > Native Minimum Luminance from Display Params DB
	// and <= Native Maximum Luminance (Full Coverage) in same DB.
	printf("    Maximum Boost SDR Luminance: %s\n",
	       ieee7542d(x[7] | (x[8] << 8)).c_str());
	// TODO: test that this is >= the previous value
}

// tag 0x7e, OUI 3A-02-92 (VESA)

void edid_state::parse_displayid_vesa(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1]);

	if (!check_displayid_datablock_length(x, 5, 7))
		return;

	unsigned len = x[2];
	x += 6;
	printf("    Data Structure Type: ");
	switch (x[0] & 7) {
	case 0: printf("eDP\n"); break;
	case 1: printf("DP\n"); break;
	default: printf("Reserved (%d)\n", x[0] & 7); break;
	}

	if ((x[0] >> 3) & 15)
		warn("Reserved bits 6:3 (%d) are not 0.\n", (x[0] >> 3) & 15);

	printf("    Default Colorspace and EOTF Handling: %s\n",
	       (x[0] & 0x80) ? "Native as specified in the Display Parameters DB" : "sRGB");

	printf("    Number of Pixels in Hor Pix Cnt Overlapping an Adjacent Panel: %u\n",
	       x[1] & 0xf);
	if ((x[1] & 0xf) > 8)
		warn("Number of Pixels in Hor Pix Cnt Overlapping an Adjacent Panel exceeds 8.\n");

	if ((x[1] >> 4) & 1)
		warn("Reserved bit 4 is not 0.\n");

	printf("    Multi-SST Operation: ");
	switch ((x[1] >> 5) & 3) {
	case 0: printf("Not Supported\n"); break;
	case 1: printf("Two Streams (number of links shall be 2 or 4)\n"); break;
	case 2: printf("Four Streams (number of links shall be 4)\n"); break;
	case 3: printf("Reserved\n"); warn("Invalid option for Multi-SST Operation.\n"); break;
	}

	if ((x[1] >> 7) & 1)
		warn("Reserved bit 7 is not 0.\n");

	if (len >= 7) {
		double bpp = (x[2] & 0x3f) + (x[3] & 0x0f) / 16.0;
		printf("    Pass through timing's target DSC bits per pixel: %.4f\n", bpp);
	}
}

// tag 0x7f, OUI 00-10-FA (Apple)

void edid_state::parse_displayid_apple(const unsigned char *x)
{
	int length = x[2] - 3;

	x += 6;

	// Based on the very limited information I found here:
	// https://opensource.apple.com/source/IOKitUser/IOKitUser-1445.40.1/graphics.subproj/IODisplayLib.c
	switch (x[0]) {
	case 1:
		printf("    Type: BLC Info/Corrections, Version: %u\n", x[1]);
		break;
	default:
		printf("    Type: %u, Version: %u\n", x[0], x[1]);
		break;
	}
	hex_block("    ", x + 2, length - 2);
}

// tag 0x81

void edid_state::parse_displayid_cta_data_block(const unsigned char *x)
{
	check_displayid_datablock_revision(x[1]);

	unsigned len = x[2];
	unsigned i;

	if (len > 248) {
		fail("Length is > 248.\n");
		len = 248;
	}
	x += 3;

	for (i = 0; i < len; i += (x[i] & 0x1f) + 1) {
		cta_block(x + i, dispid.found_tags);
	}

	if (i != len)
		fail("Length is %u instead of %u.\n", len, i);
}

// DisplayID main

std::string edid_state::product_type(unsigned char x, bool heading)
{
	std::string headingstr;

	if (dispid.version < 0x20) {
		headingstr = "Display Product Type";
		if (heading) return headingstr;
		dispid.is_display = x == 2 || x == 3 || x == 4 || x == 6;
		switch (x) {
		case 0: return "Extension Section";
		case 1: return "Test Structure; test equipment only";
		case 2: return "Display panel or other transducer, LCD or PDP module, etc.";
		case 3: return "Standalone display device";
		case 4: return "Television receiver";
		case 5: return "Repeater/translator";
		case 6: return "DIRECT DRIVE monitor";
		default: break;
		}
	} else {
		headingstr = "Display Product Primary Use Case";
		if (heading) return headingstr;
		dispid.is_display = x >= 2 && x <= 8;
		dispid.is_arvr = x >= 7 && x <= 8;
		switch (x) {
		case 0: return "Same primary use case as the base section";
		case 1: return "Test Structure; test equipment only";
		case 2: return "None of the listed primary use cases; generic display";
		case 3: return "Television (TV) display";
		case 4: return "Desktop productivity display";
		case 5: return "Desktop gaming display";
		case 6: return "Presentation display";
		case 7: return "Head-mounted Virtual Reality (VR) display";
		case 8: return "Head-mounted Augmented Reality (AR) display";
		default: break;
		}
	}
	fail("Unknown %s 0x%02x.\n", headingstr.c_str(), x);
	return std::string("Unknown " + headingstr + " (") + utohex(x) + ")";
}

void edid_state::preparse_displayid_block(unsigned char *x)
{
	bool update_checksum = false;
	unsigned length = x[2];
	unsigned offset = 5;

	if (length > 121)
		length = 121;

	dispid.preparsed_displayid_blocks++;
	while (length > 0) {
		unsigned tag = x[offset];
		unsigned len = x[offset + 2];

		switch (tag) {
		case 0x00:
		case 0x20:
			if (replace_unique_ids &&
			    (x[offset + 0x08] || x[offset + 0x09] ||
			     x[offset + 0x0a] || x[offset + 0x0b])) {
				// Replace by 123456
				x[offset + 0x08] = 0x40;
				x[offset + 0x09] = 0xe2;
				x[offset + 0x0a] = 0x01;
				x[offset + 0x0b] = 0x00;
				update_checksum = true;
			}
			if (replace_unique_ids && x[offset + 0x0c] != 0xff) {
				x[offset + 0x0c] = 0;
				x[offset + 0x0d] = 0;
				update_checksum = true;
			}
			break;
		case 0x12:
		case 0x28:
			if (replace_unique_ids &&
			    (x[offset + 0x15] || x[offset + 0x16] ||
			     x[offset + 0x17] || x[offset + 0x18])) {
				// Replace by 123456
				x[offset + 0x15] = 0x40;
				x[offset + 0x16] = 0xe2;
				x[offset + 0x17] = 0x01;
				x[offset + 0x18] = 0x00;
				update_checksum = true;
			}
			break;
		case 0x29:
			if (replace_unique_ids) {
				update_checksum = true;
				memset(x + offset + 3, 0, 16);
			}
			break;
		case 0x02:
			dispid.preparsed_color_ids |= 1 << ((x[offset + 1] >> 3) & 0x0f);
			break;
		case 0x0e:
			dispid.preparsed_xfer_ids |= 1 << ((x[offset + 1] >> 4) & 0x0f);
			break;
		default:
			break;
		}

		if (length < 3)
			break;

		if (length < len + 3)
			break;

		if (!tag && !len)
			break;

		length -= len + 3;
		offset += len + 3;
	}
	if (update_checksum) {
		replace_checksum(x + 1, x[2] + 5);
		replace_checksum(x, EDID_PAGE_SIZE);
	}
}

unsigned edid_state::displayid_block(const unsigned version, const unsigned char *x, unsigned length)
{
	unsigned i;
	unsigned tag = x[0];
	unsigned tag_version = (tag < 0x20) ? 1 : (tag < 0x7f) ? 2 : (tag < 0x80) ? 1 : 0;
	bool dooutputname = true;
	unsigned len = (length < 3) ? 0 : x[2];
	bool hasoui = false;
	unsigned ouinum;

	switch (tag) {
	// DisplayID 1.3:
	case 0x00:
		data_block_oui("Product Identification Data Block (" + utohex(tag) + ")",
			       x + 3, len, &ouinum, true, true, true);
		dooutputname = false;
		hasoui = true;
		break;
	case 0x01: data_block = "Display Parameters Data Block (" + utohex(tag) + ")"; break;
	case 0x02: data_block = "Color Characteristics Data Block"; break;
	case 0x03: data_block = "Video Timing Modes Type 1 - Detailed Timings Data Block"; break;
	case 0x04: data_block = "Video Timing Modes Type 2 - Detailed Timings Data Block"; break;
	case 0x05: data_block = "Video Timing Modes Type 3 - Short Timings Data Block"; break;
	case 0x06: data_block = "Video Timing Modes Type 4 - DMT Timings Data Block"; break;
	case 0x07: data_block = "Supported Timing Modes Type 1 - VESA DMT Timings Data Block"; break;
	case 0x08: data_block = "Supported Timing Modes Type 2 - CTA-861 Timings Data Block"; break;
	case 0x09: data_block = "Video Timing Range Data Block"; break;
	case 0x0a: data_block = "Product Serial Number Data Block"; break;
	case 0x0b: data_block = "GP ASCII String Data Block"; break;
	case 0x0c: data_block = "Display Device Data Data Block"; break;
	case 0x0d: data_block = "Interface Power Sequencing Data Block"; break;
	case 0x0e: data_block = "Transfer Characteristics Data Block"; break;
	case 0x0f: data_block = "Display Interface Data Block"; break;
	case 0x10: data_block = "Stereo Display Interface Data Block (" + utohex(tag) + ")"; break;
	case 0x11: data_block = "Video Timing Modes Type 5 - Short Timings Data Block"; break;
	case 0x12: data_block = "Tiled Display Topology Data Block (" + utohex(tag) + ")"; break;
	case 0x13: data_block = "Video Timing Modes Type 6 - Detailed Timings Data Block"; break;
	// 0x14 .. 0x7e RESERVED for Additional VESA-defined Data Blocks
	// DisplayID 2.0
	case 0x20:
		data_block_oui("Product Identification Data Block (" + utohex(tag) + ")",
			       x + 3, len, &ouinum, false, false, true);
		dooutputname = false;
		hasoui = true;
		break;
	case 0x21: data_block = "Display Parameters Data Block (" + utohex(tag) + ")"; break;
	case 0x22: data_block = "Video Timing Modes Type 7 - Detailed Timings Data Block"; break;
	case 0x23: data_block = "Video Timing Modes Type 8 - Enumerated Timing Codes Data Block"; break;
	case 0x24: data_block = "Video Timing Modes Type 9 - Formula-based Timings Data Block"; break;
	case 0x25: data_block = "Dynamic Video Timing Range Limits Data Block"; break;
	case 0x26: data_block = "Display Interface Features Data Block"; break;
	case 0x27: data_block = "Stereo Display Interface Data Block (" + utohex(tag) + ")"; break;
	case 0x28: data_block = "Tiled Display Topology Data Block (" + utohex(tag) + ")"; break;
	case 0x29: data_block = "ContainerID Data Block"; break;
	case 0x2a: data_block = "Video Timing Modes Type 10 - Formula-based Timings Data Block"; break;
	case 0x2b: data_block = "Adaptive Sync Data Block"; break;
	case 0x2c: data_block = "ARVR_HMD Data Block"; break;
	case 0x2d: data_block = "ARVR_Layer Data Block"; break;
	case 0x2e: data_block = "Brightness Luminance Range Data Block"; break;
	// 0x2f .. 0x7d RESERVED for Additional VESA-defined Data Blocks
	case 0x7e: // DisplayID 2.0
		data_block_oui("Vendor-Specific Data Block (" + utohex(tag) + ")",
			       x + 3, len, &ouinum, false, false, true);
		dooutputname = false;
		hasoui = true;
		tag |= ouinum;
		break;
	case 0x7f: // DisplayID 1.3
		data_block_oui("Vendor-Specific Data Block (" + utohex(tag) + ")",
			       x + 3, len, &ouinum, false, true, true);
		dooutputname = false;
		hasoui = true;
		tag |= ouinum;
		break;
	// 0x80 RESERVED
	case 0x81: data_block = "CTA-861 DisplayID Data Block"; break;
	// 0x82 .. 0xff RESERVED
	default:   data_block = "Unknown DisplayID Data Block (" + utohex(tag) + ", length " + std::to_string(len) + ")"; break;
	}

	if (length < 3) {
		// Report a problem when the remaining bytes are not 0.
		data_block.clear(); // Probably not a Data Block so clear this.
		if (tag || (length > 1 && x[1])) {
			printf("  Filler:\n");
			fail("Not enough bytes remain (%d) for a DisplayID data block and the DisplayID filler is non-0.\n", length);
			hex_block("    ", x, length);
		}
		return length;
	}

	if (length < len + 3) {
		data_block.clear(); // Probably not a Data Block so clear this.
		printf("  Filler:\n");
		fail("The length of this DisplayID data block (%d) exceeds the number of bytes remaining (%d).\n", len + 3, length);
		hex_block("    ", x, length);
		return length;
	}

	if (!tag && !len) {
		// A Product Identification Data Block with no payload bytes is not valid - assume this is the end.
		data_block.clear(); // Probably not a Product Identification Data Block so clear this.
		if (!memchk(x, length)) {
			printf("  Filler:\n");
			fail("Non-0 filler bytes in the DisplayID block.\n");
			hex_block("    ", x, length);
		}
		return length;
	}

	if (dooutputname && data_block.length())
		printf("  %s:\n", data_block.c_str());

	if (version >= 0x20 && tag_version == 1)
		fail("Use of DisplayID v1.x tag for DisplayID v%u.%u.\n",
			 version >> 4, version & 0xf);
	if (version < 0x20 && tag_version == 2)
		fail("Use of DisplayID v2.0 tag for DisplayID v%u.%u.\n",
			 version >> 4, version & 0xf);

	unsigned block_rev = x[1] & 0x07;

	switch (tag) {
	case 0x00: parse_displayid_product_id(x); break;
	case 0x01: parse_displayid_parameters(x); break;
	case 0x02: parse_displayid_color_characteristics(x); break;
	case 0x03:
		   check_displayid_datablock_revision(x[1], 0, block_rev & 1);
		   for (i = 0; i < len / 20; i++)
			   parse_displayid_type_1_7_timing(&x[3 + (i * 20)], false, block_rev);
		   break;
	case 0x04:
		   check_displayid_datablock_revision(x[1]);
		   for (i = 0; i < len / 11; i++)
			   parse_displayid_type_2_timing(&x[3 + (i * 11)]);
		   break;
	case 0x05:
		   check_displayid_datablock_revision(x[1], 0, block_rev & 1);
		   for (i = 0; i < len / 3; i++)
			   parse_displayid_type_3_timing(&x[3 + (i * 3)]);
		   break;
	case 0x06:
		   check_displayid_datablock_revision(x[1], 0xc0, 1);
		   for (i = 0; i < len; i++)
			   parse_displayid_type_4_8_timing((x[1] & 0xc0) >> 6, x[3 + i]);
		   break;
	case 0x07:
		   check_displayid_datablock_revision(x[1]);
		   for (i = 0; i < min(len, 10) * 8; i++)
			   if (x[3 + i / 8] & (1 << (i % 8))) {
				   char type[16];
				   sprintf(type, "DMT 0x%02x", i + 1);
				   print_timings("    ", find_dmt_id(i + 1), type);
			   }
		   break;
	case 0x08:
		   check_displayid_datablock_revision(x[1]);
		   for (i = 0; i < min(len, 8) * 8; i++)
			   if (x[3 + i / 8] & (1 << (i % 8))) {
				   char type[16];
				   sprintf(type, "VIC %3u", i + 1);
				   print_timings("    ", find_vic_id(i + 1), type);
			   }
		   break;
	case 0x09: parse_displayid_video_timing_range_limits(x); break;
	case 0x0a:
	case 0x0b: parse_displayid_string(x); break;
	case 0x0c: parse_displayid_display_device(x); break;
	case 0x0d: parse_displayid_intf_power_sequencing(x); break;
	case 0x0e: parse_displayid_transfer_characteristics(x); break;
	case 0x0f: parse_displayid_display_intf(x); break;
	case 0x10: parse_displayid_stereo_display_intf(x); break;
	case 0x11:
		   check_displayid_datablock_revision(x[1]);
		   for (i = 0; i < len / 7; i++)
			   parse_displayid_type_5_timing(&x[3 + (i * 7)]);
		   break;
	case 0x12: parse_displayid_tiled_display_topology(x, false); break;
	case 0x13:
		   check_displayid_datablock_revision(x[1]);
		   for (i = 0; i < len; i += (x[3 + i + 2] & 0x40) ? 17 : 14)
			   parse_displayid_type_6_timing(&x[3 + i]);
		   break;
	case 0x20: parse_displayid_product_id(x); break;
	case 0x21:
		   if (block_rev >= 1)
			   check_displayid_datablock_revision(x[1], 0x80, 1);
		   else
			   check_displayid_datablock_revision(x[1], 0x80, 0);
		   parse_displayid_parameters_v2(x, block_rev);
		   break;
	case 0x22: {
		   unsigned sz = 20;

		   if (block_rev >= 2)
			   check_displayid_datablock_revision(x[1], 0x08, 2);
		   else if (block_rev == 1)
			   check_displayid_datablock_revision(x[1], 0x08, 1);
		   else
			   check_displayid_datablock_revision(x[1]);
		   sz += (x[1] & 0x70) >> 4;
		   if (block_rev >= 1 && (x[1] & 0x08))
			   printf("    These timings support DSC pass-through\n");
		   for (i = 0; i < len / sz; i++)
			   parse_displayid_type_1_7_timing(&x[3 + i * sz], true, block_rev);
		   break;
	}
	case 0x23:
		   if (block_rev)
			   check_displayid_datablock_revision(x[1], 0xe8, 1);
		   else
			   check_displayid_datablock_revision(x[1], 0xc8);
		   if (x[1] & 0x08) {
			   for (i = 0; i < len / 2; i++)
				   parse_displayid_type_4_8_timing((x[1] & 0xc0) >> 6,
								   x[3 + i * 2] |
								   (x[4 + i * 2] << 8));
		   } else {
			   for (i = 0; i < len; i++)
				   parse_displayid_type_4_8_timing((x[1] & 0xc0) >> 6,
								   x[3 + i]);
		   }
		   break;
	case 0x24:
		   check_displayid_datablock_revision(x[1]);
		   for (i = 0; i < len / 6; i++)
			   parse_displayid_type_9_timing(&x[3 + i * 6]);
		   break;
	case 0x25: parse_displayid_dynamic_video_timings_range_limits(x); break;
	case 0x26: parse_displayid_interface_features(x); break;
	case 0x27: parse_displayid_stereo_display_intf(x); break;
	case 0x28: parse_displayid_tiled_display_topology(x, true); break;
	case 0x29: parse_displayid_ContainerID(x); break;
	case 0x2a: {
		   unsigned sz = 6 + ((x[1] & 0x70) >> 4);

		   check_displayid_datablock_revision(x[1], 0x70);
		   if (sz > 8)
			   fail("Invalid descriptor size %u.\n", sz);

		   for (i = 0; i < len / sz; i++)
			   parse_displayid_type_10_timing(&x[3 + i * sz], sz);
		   break;
	}
	case 0x2b: parse_displayid_adaptive_sync(x); break;
	case 0x2c: parse_displayid_arvr_hmd(x); break;
	case 0x2d: parse_displayid_arvr_layer(x); break;
	case 0x2e: parse_displayid_brightness_lum_range(x); break;
	case 0x7e|kOUI_VESA: parse_displayid_vesa(x); break;
	case 0x7f|kOUI_Apple: parse_displayid_apple(x); break;
	case 0x81: parse_displayid_cta_data_block(x); break;
	default: hex_block("    ", x + 3 + (hasoui ? 3 : 0), (len > (hasoui ? 3 : 0)) ? len - (hasoui ? 3 : 0) : 0); break;
	}

	if ((tag == 0x00 || tag == 0x20) &&
		(!dispid.is_base_block || dispid.block_number > 0))
		fail("%s is required to be the first DisplayID Data Block.\n",
		     data_block.c_str());

	dispid.block_number++;
	return len + 3;
}

void edid_state::parse_displayid_block(const unsigned char *x)
{
	unsigned version = x[1];
	unsigned length = x[2];
	unsigned prod_type = x[3]; // future check: based on type, check for required data blocks
	unsigned ext_count = x[4];

	printf("  Version: %u.%u\n  Extension Count: %u\n",
	       version >> 4, version & 0xf, ext_count);

	if (dispid.is_base_block) {
		dispid.version = version;
		printf("  %s: %s\n", product_type(prod_type, true).c_str(),
		       product_type(prod_type, false).c_str());
		if (!prod_type)
			fail("DisplayID Base Block has no product type.\n");
		if (ext_count != dispid.preparsed_displayid_blocks - 1)
			fail("Expected %u DisplayID Extension Block%s, but got %u.\n",
			     ext_count,
			     ext_count > 1 ? "s" : "",
			     dispid.preparsed_displayid_blocks - 1);
	} else {
		if (prod_type)
			fail("Product Type should be 0 in extension block.\n");
		if (ext_count)
			fail("Extension Count should be 0 in extension block.\n");
		if (version != dispid.version)
			fail("Got version %u.%u, expected %u.%u.\n",
			     version >> 4, version & 0xf,
			     dispid.version >> 4, dispid.version & 0xf);
	}

	if (length > 121) {
		fail("DisplayID length %d is greater than 121.\n", length);
		length = 121;
	}

	unsigned len;
	unsigned saved_length = length;
	for (const unsigned char *y = x + 5; length > 0; y += len) {
		len = displayid_block(version, y, length);
		length -= len;
	}

	/*
	 * DisplayID length field is number of following bytes
	 * but checksum is calculated over the entire structure
	 * (excluding DisplayID-in-EDID magic byte)
	 */
	data_block.clear();
	do_checksum("  ", x + 1, saved_length + 5, x[2] + 4);

	unused_bytes = 0x7f - (1 + saved_length + 5);
	if (!memchk(x + 1 + saved_length + 5, unused_bytes)) {
		data_block = "Padding";
		fail("Contains non-zero bytes.\n");
	}
	dispid.is_base_block = false;
}

void edid_state::check_displayid_blocks()
{
	data_block = "DisplayID";
	if (!dispid.has_product_identification &&
	    (native_dispid || dispid.has_tiled_display_topology))
		fail("Missing DisplayID Product Identification Data Block.\n");
	if (dispid.is_display && (native_dispid || !dispid.has_display_parameters))
		fail("Missing DisplayID Display Parameters Data Block.\n");
	if (dispid.is_display && !dispid.has_display_interface_features &&
	    (native_dispid || dispid.has_ycbcr_420))
		fail("Missing DisplayID Display Interface Features Data Block.\n");
	if (native_dispid && dispid.is_arvr && !dispid.has_arvr_hdm)
		fail("Missing DisplayID ARVR_HMD Data Block.\n");
	if (native_dispid && dispid.is_arvr && !dispid.has_arvr_layer)
		fail("Missing DisplayID ARVR_Layer Data Block.\n");
	if (dispid.has_stereo && !dispid.has_stereo_display_interface)
		fail("Missing DisplayID Stereo Display Interface Data Block.\n");
	if (dispid.is_display && !dispid.has_type_1_7)
		fail("Missing DisplayID Type %s Detailed Timing Data Block.\n",
		     dispid.version >= 0x20 ? "VII" : "I");
	if (dispid.preferred_timings.empty())
		fail("DisplayID expects at least one preferred timing.\n");
	if (cta.image_width && dispid.image_width &&
	    (cta.image_width != dispid.image_width ||
	     cta.image_height != dispid.image_height))
		fail("Image size mismatch: CTA-861: %.1fx%.1fmm DisplayID: %.1fx%.1fmm.\n",
		     cta.image_width / 10.0, cta.image_height / 10.0,
		     dispid.image_width / 10.0, dispid.image_height / 10.0);
	if (dispid.image_width && dispid.image_width < 25600 && dispid.image_height < 25600 &&
	    (abs((int)dispid.image_width - (int)base.max_display_width_mm * 10) >= 100 ||
	     abs((int)dispid.image_height - (int)base.max_display_height_mm * 10) >= 100))
		fail("Image size mismatch: DisplayID: %.1fx%.1fmm Base EDID: %u.0x%u.0mm.\n",
		     dispid.image_width / 10.0, dispid.image_height / 10.0,
		     base.max_display_width_mm, base.max_display_height_mm);
}
