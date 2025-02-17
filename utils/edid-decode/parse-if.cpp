// SPDX-License-Identifier: MIT
/*
 * Copyright 2024 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#include <stdio.h>

#include "edid-decode.h"

int edid_state::parse_if_hdr(const unsigned char *x, unsigned size, unsigned char mask)
{
	unsigned char length = x[2];

	printf("%s\n", data_block.c_str());
	printf("  Version: %u\n", x[1] & mask);
	printf("  Length: %u\n", length);

	if (length + 3U > size) {
		fprintf(stderr, "\nExpected InfoFrame total length of %d, but have only %d bytes.\n",
			length + 3, size);
		fail("Expected InfoFrame total length of %d, but have only %d bytes.\n",
		     length + 3, size);
		return -1;
	}
	if (length + 3U < size) {
		warn("There are %d dummy bytes after the payload.\n", size - length - 3);
		if (!memchk(x + length + 3, length + 3 - size))
			warn("There are non-zero dummy bytes after the payload.\n");
	}
	return 0;
}

static const char *Structure_map[] = {
	"Frame packing",
	"Field alternative",
	"Line alternative",
	"Side-by-Side (Full)",
	"L + depth",
	"L + depth + graphics + graphics-depth",
	"Top-and-Bottom",
	"Reserved (7)",
	"Side-by-Side (Half)",
	"Reserved (9)",
	"Reserved (10)",
	"Reserved (11)",
	"Reserved (12)",
	"Reserved (13)",
	"Reserved (14)",
	"Not in use"
};

static const char *ExtData_map[] = {
	"Horizontal sub-sampling (0)",
	"Horizontal sub-sampling (1)",
	"Horizontal sub-sampling (2)",
	"Horizontal sub-sampling (3)",
	"Quincunx matrix: Odd/Left picture, Odd/Right picture",
	"Quincunx matrix: Odd/Left picture, Even/Right picture",
	"Quincunx matrix: Even/Left picture, Odd/Right picture",
	"Quincunx matrix: Even/Left picture, Even/Right picture"
};

void edid_state::parse_if_hdmi(const unsigned char *x, unsigned len)
{
	if (len < 4) {
		fail("Expected InfoFrame length of at least 4, got %d.\n", len);
		return;
	}
	if (x[4] & 0x1f)
		fail("Bits 4-0 of PB4 are not 0.\n");

	printf("  HDMI Video Format: ");

	char buf[32];

	switch (x[4] >> 5) {
	case 0:
		printf("No additional data\n");
		if (!memchk(x + 5, len - 4))
			fail("Trailing non-0 bytes.\n");
		return;
	case 1: {
		printf("HDMI_VIC is present\n");

		if (len < 5) {
			fail("Expected InfoFrame length of at least 5, got %d.\n", len);
			return;
		}
		sprintf(buf, "HDMI VIC %u", x[5]);
		const struct timings *t = find_hdmi_vic_id(x[5]);
		if (!t) {
			printf("  HDMI VIC: %d (0x%02x)\n", x[5], x[5]);
			fail("Unknown HDMI VIC code.\n");
		} else {
			print_timings("  ", t, buf);
		}
		if (!memchk(x + 6, len - 5))
			fail("Trailing non-0 bytes.\n");
		return;
	}
	case 2:
		printf("3D format indication present\n");

		if (len < 5) {
			fail("Expected InfoFrame length of at least 5, got %d.\n", len);
			return;
		}
		break;
	default:
		printf("Reserved (%d)\n", x[4] >> 5);
		fail("Invalid HDMI Video Format (%d).\n", x[4] >> 5);
		return;
	}

	// Parsing of 3D extension continues here
	unsigned char v = x[5] >> 4;

	printf("  3D Structure: %s\n", Structure_map[v]);
	if (x[5] & 7)
		fail("Bits 2-0 of PB5 are not 0.\n");
	printf("3D Metadata Present: %s\n", (x[5] & 8) ? "Yes" : "No");

	if (v < 8) {
		if (!memchk(x + 6, len - 5))
			fail("Trailing non-0 bytes.\n");
		return;
	}

	if (len < 6) {
		fail("Expected InfoFrame length of at least 6, got %d.\n", len);
		return;
	}

	if (x[6] & 0xf)
		fail("Bits 3-0 of PB6 are not 0.\n");

	if ((x[6] >> 4) >= 8)
		printf("  3D Extended Data: Reserved (%d)\n", (x[6] >> 4));
	else
		printf("  3D Extended Data: %s\n", ExtData_map[x[6] >> 4]);

	if (!(x[5] & 8)) {
		if (!memchk(x + 7, len - 6))
			fail("Trailing non-0 bytes.\n");
		return;
	}

	if (len < 7) {
		fail("Expected InfoFrame length of at least 7, got %d.\n", len);
		return;
	}
	unsigned mlen = x[7] & 0x1f;
	if (len < mlen + 7) {
		fail("Expected InfoFrame length of at least %d, got %d.\n", mlen + 7, len);
		return;
	}
	if (!memchk(x + mlen + 7, len - mlen - 6))
		fail("Trailing non-0 bytes.\n");
	if (x[7] >> 5) {
		printf("  3D Metadata Type: Reserved (%d)\n", x[7] >> 5);
		fail("Invalid 3D Metadata Type.\n");
		return;
	} else {
		printf("  3D Metadata Type: Parallax Information\n");
	}
	printf("  Metadata: Parallax Zero: %u\n", (x[8] << 8) | x[9]);
	printf("  Metadata: Parallax Scale: %u\n", (x[10] << 8) | x[11]);
	printf("  Metadata: DRef: %u\n", (x[12] << 8) | x[13]);
	printf("  Metadata: WRef: %u\n", (x[14] << 8) | x[15]);
}

void edid_state::parse_if_hdmi_forum(const unsigned char *x, unsigned len)
{
	if (len < 5) {
		fail("Expected InfoFrame length of at least 5, got %d.\n", len);
		return;
	}
	if (x[5] & 0x0c)
		fail("Bits 3-2 of PB5 are not 0.\n");
	unsigned char v = x[5] >> 4;
	printf("  Color Content Bits Per Component: ");
	if (!v)
		printf("No Indication\n");
	else if (v > 9)
		printf("Reserved (%d)\n", v);
	else
		printf("%d bits\n", v + 7);
	if (x[5] & 1)
		printf("  3D Valid\n");
	printf("  Auto Low-Latency Mode: %s\n", (x[5] & 2) ? "Yes" : "No");
	if (!(x[5] & 1)) {
		if (!memchk(x + 6, len - 5))
			fail("Trailing non-0 bytes.\n");
		return;
	}

	// Parsing of 3D extension continues here
	unsigned offset = 6;
	v = x[offset] >> 4;

	if (len < offset)
		goto err_len;

	printf("  3D Structure: %s\n", Structure_map[v]);
	if (x[offset] & 1)
		fail("Bit 0 of PB6 is not 0.\n");
	printf("3D Additional Info Present: %s\n", (x[offset] & 8) ? "Yes" : "No");
	printf("3D Disparity Data Present: %s\n", (x[offset] & 4) ? "Yes" : "No");
	printf("3D Metadata Present: %s\n", (x[offset] & 2) ? "Yes" : "No");
	offset++;

	if (v >= 8) {
		if (len < offset)
			goto err_len;

		if (x[offset] & 0xf)
			fail("Bits 3-0 of PB7 are not 0.\n");

		if ((x[offset] >> 4) >= 8)
			printf("  3D Extended Data: Reserved (%d)\n", (x[offset] >> 4));
		else
			printf("  3D Extended Data: %s\n", ExtData_map[x[offset] >> 4]);
		offset++;
	}

	if (x[6] & 8) {
		if (len < offset)
			goto err_len;

		if (x[offset] & 0xe0)
			fail("Bits 7-5 if PB%d are not 0.\n", offset);
		printf("  3D Dual View: %s\n", (x[offset] & 0x10) ? "Yes" : "No");

		static const char *ViewDep_map[] = {
			"No Indication",
			"Independent Right View",
			"Independent Left View",
			"Independent Right and Left Views"
		};
		printf("  3D View Dependency: %s\n", ViewDep_map[(x[offset] & 0x0c) >> 2]);

		static const char *Pref2DView_map[] = {
			"No Indication",
			"Right View",
			"Left View",
			"Either View"
		};
		printf("  3D Preferred 2D View: %s\n", Pref2DView_map[x[offset] & 3]);
		offset++;
	}

	if (x[6] & 4) {
		if (len < offset)
			goto err_len;

		printf("  Disparity Data Version: %d\n", x[offset] >> 5);
		unsigned dlen = x[offset] & 0x1f;
		printf("  Disparity Data Length: %d\n", dlen);
		if (len < offset + dlen)
			goto err_len;
		offset++;

		hex_block("  Disparity Data Payload: ", x + offset, dlen, false, dlen);

		offset += dlen;
	}

	if (x[6] & 2) {
		if (len < offset)
			goto err_len;

		if (x[offset] >> 5) {
			printf("  3D Metadata Type: Reserved (%d)\n", x[offset] >> 5);
			fail("Invalid 3D Metadata Type.\n");
			return;
		}
		printf("  3D Metadata Type: Parallax Information\n");
		printf("  3D Metadata Length: %d\n", x[offset] & 0x1f);
		if (len < offset + (x[offset] & 0x1f))
			goto err_len;
		offset++;

		printf("  Metadata: Parallax Zero: %u\n", (x[offset] << 8) | x[offset+1]);
		printf("  Metadata: Parallax Scale: %u\n", (x[offset+2] << 8) | x[offset+3]);
		printf("  Metadata: DRef: %u\n", (x[offset+4] << 8) | x[offset+5]);
		printf("  Metadata: WRef: %u\n", (x[offset+6] << 8) | x[offset+7]);

		offset += x[offset - 1] & 0x1f;
	}

	if (!memchk(x + offset + 1, len - offset))
		fail("Trailing non-0 bytes.\n");
	return;

err_len:
	fail("Expected InfoFrame length of at least %d, got %d.\n", offset, len);
	return;
}

void edid_state::parse_if_vendor(const unsigned char *x, unsigned size)
{
	data_block = "Vendor-Specific InfoFrame";

	unsigned oui;
	unsigned char len = x[2];

	data_block_oui(data_block, x + 3, 3, &oui, false, false, false, true);

	if (parse_if_hdr(x, size, (x[1] & 0x7f) == 2 ? 0x7f : 0xff))
		return;

	if (x[1] != 1 && (x[1] & 0x7f) != 2) {
		fail("Invalid version %d\n", x[1] & 0x7f);
		return;
	}
	if (len < 3) {
		fail("Expected InfoFrame length of at least 3, got %d.\n", len);
		return;
	}

	if (x[1] != 1)
		printf("  VSIF Change: %s\n", (x[1] & 0x80) ? "Yes" : "No");

	// After this x[1] will refer to Data Byte 1
	x += 2;

	switch (oui) {
	case kOUI_HDMI:
		parse_if_hdmi(x, len);
		break;
	case kOUI_HDMIForum:
		parse_if_hdmi_forum(x, len);
		break;
	default:
		hex_block("  Payload: ", x + 4, len - 3, false, len - 3);
		break;
	}
}

void edid_state::parse_if_avi(const unsigned char *x, unsigned size)
{
	unsigned char version = x[1];
	unsigned char length = x[2];

	data_block = "AVI InfoFrame";

	if (parse_if_hdr(x, size))
		return;

	if (version == 0 || version > 4) {
		fail("Invalid version %u\n", version);
		return;
	}
	if (version == 1)
		fail("Sources shall not use version 1.\n");
	if (length < 13) {
		fail("Expected InfoFrame length of 13, got %u.\n", length);
		return;
	}

	// After this x[1] will refer to Data Byte 1
	x += 2;

	if (version == 1) {
		if (x[3] & 0xfc)
			fail("Bits F37-F32 are not 0.\n");
		if (x[4])
			fail("Bits F47-F40 are not 0.\n");
		if (x[5])
			fail("Bits F57-F50 are not 0.\n");
	}
	if (version == 2) {
		if (x[1] & 0x80)
			fail("Bit Y2 is not 0.\n");
		if (x[4] & 0x80)
			fail("Bit VIC7 is not 0.\n");
	}
	if (version == 4 && length == 15) {
		if (x[15] & 0x80)
			fail("Bit F157 is not 0.\n");
	}
	if (version == 4 && length < 14) {
		fail("Version 4 expects a length of >= 14, got %u\n", length);
		return;
	}

	if (edid_size) {
		if (version > cta.avi_version)
			warn("AVI version is %u, but EDID indicates support for %u only.\n",
			     version, cta.avi_version);
		if (version == 4 && length > cta.avi_v4_length)
			warn("AVI version 4 length is %u, but EDID indicates support for %u only.\n",
			     length, cta.avi_v4_length);
	}

	char buf[32];
	unsigned vic_fps = 0;
	unsigned char vic = x[4];
	unsigned char rid = length >= 14 ? x[15] & 0x3f : 0;

	sprintf(buf, "VIC %3u", vic);
	const struct timings *t = find_vic_id(vic);
	if (t) {
		print_timings("  ", t, buf);
		vic_fps = calc_fps(t);
	} else if (vic) {
		printf("  VIC: %d (0x%02x)\n", vic, vic);
		fail("Unknown VIC code.\n");
	}

	static const char *Y_map[] = {
		"RGB",
		"YCbCr 4:2:2",
		"YCbCr 4:4:4",
		"YCbCr 4:2:0",
		"Reserved (4)",
		"Reserved (5)",
		"Reserved (6)",
		"IDO-Defined",
	};
	unsigned char y = x[1] >> 5;
	unsigned char v;

	printf("  Y: Color Component Sample Format: %s\n", Y_map[y]);
	if (y == 7 && version == 2)
		warn("Y == 7 but AVI Version == 2.\n");
	if (edid_size) {
		if ((y == 1 && !cta.has_ycbcr422) ||
		    (y == 2 && !cta.has_ycbcr444) ||
		    (y == 3 && !cta.has_ycbcr420))
			fail("Y == %s, but this capability is not enabled in the EDID.\n", Y_map[y]);
	}

	printf("  A: Active Format Information Present: %s\n",
	       ((x[1] >> 4) & 1) ? "Yes" : "No");

	static const char *B_map[] = {
		"Bar Data not present",
		"Vertical Bar Info present",
		"Horizontal Bar Info present",
		"Vertical and Horizontal Bar Info present"
	};
	printf("  B: Bar Data Present: %s\n", B_map[(x[1] >> 2) & 3]);

	static const char *S_map[] = {
		"No Data",
		"Composed for an overscanned display",
		"Composed for an underscanned display",
		"Reserved"
	};
	printf("  S: Scan Information: %s\n", S_map[x[1] & 3]);

	static const char *C_map[] = {
		"No Data",
		"SMPTE ST 170",
		"Rec. ITU-R BT.709",
		"Extended Colorimetry Information Valid"
	};
	printf("  C: Colorimetry: %s\n", C_map[x[2] >> 6]);

	static const char *M_map[] = {
		"No Data",
		"4:3",
		"16:9",
		"Reserved"
	};
	v = (x[2] >> 4) & 3;
	printf("  M: Picture Aspect Ratio: %s\n", M_map[v]);
	if ((vic || rid) && v)
		warn("If a VID or RID is specified, then set M to 0.\n");
	printf("  R: Active Portion Aspect Ratio: %d\n", x[2] & 0xf);

	static const char *ITC_map[] = {
		"No Data",
		"IT Content (CN is valid)"
	};
	printf("  ITC: IT Content: %s\n", ITC_map[x[3] >> 7]);

	static const char *EC_map[] = {
		"xvYCC601",
		"xvYCC709",
		"sYCC601",
		"opYCC601",
		"opRGB",
		"Rec. ITU-R BR.2020 YcCbcCrc",
		"Rec. ITU-R BR.2020 RGB or YCbCr",
		"Additional Colorimetry Extension Information Valid"
	};
	printf("  EC: Extended Colorimetry: %s\n", EC_map[(x[3] >> 4) & 7]);

	static const char *Q_map[] = {
		"Default",
		"Limited Range",
		"Full Range",
		"Reserved"
	};
	printf("  Q: RGB Quantization Range: %s\n", Q_map[(x[3] >> 2) & 3]);

	static const char *SC_map[] = {
		"No Known non-uniform scaling",
		"Picture has been scaled horizontally",
		"Picture has been scaled vertically",
		"Picture has been scaled horizontally and vertically"
	};
	printf("  SC: Non-Uniform Picture Scaling: %s\n", SC_map[x[3] & 3]);

	static const char *YQ_map[] = {
		"Limited Range",
		"Full Range",
		"Reserved",
		"Reserved"
	};
	printf("  YQ: YCC Quantization Range: %s\n", YQ_map[x[5] >> 6]);

	static const char *CN_map[] = {
		"Graphics",
		"Photo",
		"Cinema",
		"Game"
	};
	printf("  CN: IT Content Type: %s\n", CN_map[(x[5] >> 4) & 3]);
	unsigned char pr = x[5] & 0xf;
	printf("  PR: Pixel Data Repetition Count: %d\n", pr);

	const unsigned short pr_2 = 2;
	const unsigned short pr_1_10 = 0x3ff;
	const unsigned short pr_1_2 = 3;
	const unsigned short pr_1_2_4 = 0xb;
	static const unsigned short vic_valid_pr[] = {
		// VIC 0-7
		0, 0, 0, 0, 0, 0, pr_2, pr_2,
		// VIC 8-15
		pr_2, pr_2, pr_1_10, pr_1_10, pr_1_10, pr_1_10, pr_1_2, pr_1_2,
		// VIC 16-23
		0, 0, 0, 0, 0, pr_2, pr_2, pr_2,
		// VIC 24-31
		pr_2, pr_1_10, pr_1_10, pr_1_10, pr_1_10, pr_1_2, pr_1_2, 0,
		// VIC 32-39
		0, 0, 0, pr_1_2_4, pr_1_2_4, pr_1_2_4, pr_1_2_4, 0,
		// VIC 40-47
		0, 0, 0, 0, pr_2, pr_2, 0, 0,
		// VIC 48-55
		0, 0, pr_2, pr_2, 0, 0, pr_2, pr_2,
		// VIC 56-59
		0, 0, pr_2, pr_2
	};

	if (pr >= 10)
		fail("PR >= 10 is a reserved value.\n");
	else if (pr && (vic >= ARRAY_SIZE(vic_valid_pr) ||
			!(vic_valid_pr[vic] & (1 << pr))))
		fail("PR %u is not supported by VIC %u.\n", pr, vic);

	printf("  Line Number of End of Top Bar: %d\n", (x[7] << 8) | x[6]);
	printf("  Line Number of Start of Bottom Bar: %d\n", (x[9] << 8) | x[8]);
	printf("  Pixel Number of End of Left Bar: %d\n", (x[11] << 8) | x[10]);
	printf("  Pixel Number of Start of Right Bar: %d\n", (x[13] << 8) | x[12]);
	if (length <= 13)
		return;

	static const char *ACE_map[] = {
		"SMPTE ST 2113 P3D65 RGB",
		"SMPTE ST 2113 P3DCI RGB",
		"Rec. ITU-R BT.2100 ICtCp",
		"sRGB",
		"defaultRGB",
		"Reserved (5)",
		"Reserved (6)",
		"Reserved (7)",
		"Reserved (8)",
		"Reserved (9)",
		"Reserved (10)",
		"Reserved (11)",
		"Reserved (12)",
		"Reserved (13)",
		"Reserved (14)",
		"Reserved (15)"
	};
	printf("  ACE: Additional Colorimetry Extension: %s\n", ACE_map[x[14] >> 4]);
	if (length <= 14) {
		if (x[14] & 0xf) {
			printf("  FR: %d\n", x[14] & 0xf);
			fail("InfoFrame length is 14, but FR != 0.\n");
		}
		return;
	}

	unsigned fr = ((x[15] & 0x40) >> 2) | (x[14] >> 4);

	printf("  RID/FR: %u/%u\n", rid, fr);
	if (vic && rid)
		fail("Both a RID and a VIC were specified.\n");

	if (!rid && !fr)
		return;

	if (rid && !fr) {
		fail("RID is set, but FR is 0.\n");
		return;
	}

	static const unsigned fr_rate_values[] = {
		/* FR 0-7 */
		  0,  24,  24,  25,  30,  30,  48,  48,
		/* FR 8-15 */
		 50,  60,  60, 100, 120, 120, 144, 144,
		/* FR 16-23 */
		200, 240, 240, 300, 360, 360, 400, 480,
		/* FR 24 */
		480
	};
	static const bool fr_ntsc[] = {
		/* FR 0-7 */
		0, 1, 0, 0, 1, 0, 1, 0,
		/* FR 8-15 */
		0, 1, 0, 0, 1, 0, 1, 0,
		/* FR 16-23 */
		0, 1, 0, 0, 1, 0, 0, 1,
		/* FR 24 */
		0
	};

	if (fr >= ARRAY_SIZE(fr_rate_values)) {
		fail("Unknown FR %u.\n", fr);
		return;
	}
	unsigned fps = fr_rate_values[fr];
	bool ntsc = fr_ntsc[fr];

	printf("  Frame Rate: %u%s\n", fps, ntsc ? "/1.001" : "");

	if (vic) {
		if (vic_fps != fps)
			warn("VIC %u is %u fps, while FR indicates %u fps.\n",
			     x[4], vic_fps, fps);
		return;
	}

	if (!rid)
		return;

	const struct cta_rid *crid = find_rid(rid);

	if (!crid) {
		fail("Unknown RID %u.\n", rid);
		return;
	}

	unsigned char rid_vic = rid_fps_to_vic(rid, fps);
	if (rid_vic) {
		sprintf(buf, "VIC %3u", rid_vic);
		const struct timings *t = find_vic_id(rid_vic);
		print_timings("  ", t, buf);
		warn("RID/FR %u/%u maps to VIC %d.\n", rid, fr, rid_vic);
	} else {
		sprintf(buf, "RID/FR %u/%u", rid, fr);
		timings t = calc_ovt_mode(crid->hact, crid->vact,
					  crid->hratio, crid->vratio, fps);
		print_timings("", &t, buf, "", false, false, ntsc);
	}
}

void edid_state::parse_if_spd(const unsigned char *x, unsigned size)
{
	data_block = "Source Product Description InfoFrame";

	if (parse_if_hdr(x, size))
		return;

	if (x[1] != 1) {
		fail("Invalid version %d\n", x[1]);
		return;
	}
	if (x[2] < 25) {
		fail("Expected InfoFrame length of 25, got %d.\n", x[2]);
		return;
	}

	// After this x[1] will refer to Data Byte 1
	x += 2;

	for (unsigned i = 1; i <= 24; i++) {
		if (x[i] & 0x80) {
			fail("SPD contains ASCII character with bit 7 set.\n");
		}
	}
	char vendor[9] = {};
	memcpy(vendor, x + 1, 8);
	printf("  Vendor Name: '%s'\n", vendor);
	unsigned len = strlen(vendor);
	if (!memchk(x + 1 + len, 8 - len))
		fail("Vendor name has trailing non-zero characters.\n");

	char product[17] = {};
	memcpy(product, x + 9, 16);
	printf("  Product Description: '%s'\n", product);
	len = strlen(product);
	if (!memchk(x + 9 + len, 16 - len))
		fail("Product name has trailing non-zero characters.\n");

	if (x[25] >= 0x0e) {
		printf("  Source Information: %d (Reserved)\n", x[25]);
		fail("Source Information value %d is reserved.\n", x[25]);
	} else {
		static const char *SI_map[] = {
			"Unknown",
			"Digital STB",
			"DVD player",
			"D-VHS",
			"HDD Videorecorder",
			"DVC",
			"DSC",
			"Video CD",
			"Game",
			"PC general",
			"Blu-Ray Disck (DB)",
			"Super Audio CD",
			"HD DVD",
			"PMP"
		};
		printf("  Source Information: %s\n", SI_map[x[25]]);
	}
}

void edid_state::parse_if_audio(const unsigned char *x, unsigned size)
{
	data_block = "Audio InfoFrame";

	if (parse_if_hdr(x, size))
		return;

	if (x[1] != 1) {
		fail("Invalid version %d\n", x[1]);
		return;
	}
	if (x[2] < 10) {
		fail("Expected InfoFrame length of 10, got %d.\n", x[2]);
		return;
	}

	// After this x[1] will refer to Data Byte 1
	x += 2;

	if (x[1] & 0x08)
		fail("Bit F13 is not 0.\n");
	if (x[2] & 0xe0)
		fail("Bits F27-F25 are not 0.\n");
	if (x[3] & 0xe0)
		fail("Bits F37-F35 are not 0.\n");
	if (x[5] & 0x04)
		fail("Bit F52 is not 0.\n");
	if (x[4] <= 0x31 && !memchk(x + 6, 5))
		fail("Bits F107-F60 are not 0.\n");
	else if (x[4] == 0xfe && (x[10] || x[9] || (x[8] & 0xf8)))
		fail("Bits F107-F90 and/or F87-F83 are not 0.\n");
	else if (x[4] == 0xff && x[10])
		fail("Bits F107-F100 are not 0.\n");

	static const char *CT_map[] = {
		"Refer to Stream Header",
		"L-PCM",
		"AC-3",
		"MPEG-1",
		"MP3",
		"MPEG2",
		"AAC LC",
		"DTS",
		"ATRAC",
		"DSD",
		"Enhanced AC-3",
		"DTS-(U)HD",
		"MAT",
		"DST",
		"WMA Pro",
		"Refer to Audio Coding Extension Type (CXT) Field"
	};
	printf("  CT: Audio Coding Type: %s\n", CT_map[x[1] >> 4]);
	if (x[1] & 7)
		printf("  CC: Audio Channel Count: %d\n", (x[1] & 7) + 1);
	else
		printf("  CC: Audio Channel Count: Refer to Stream Header\n");

	static const char *SF_map[] = {
		"Refer to Stream Header",
		"32 kHz",
		"44.1 kHz (CD)",
		"48 kHz",
		"88.2 kHz",
		"96 kHz",
		"176.4 kHz",
		"192 kHz"
	};
	printf("  SF: Sampling Frequency: %s\n", SF_map[(x[2] >> 2) & 7]);

	static const char *SS_map[] = {
		"Refer to Stream Header",
		"16 bit",
		"20 bit",
		"24 bit"
	};
	printf("  SS: Bits/Sample: %s\n", SS_map[x[2] & 3]);

	static const char *CXT_map[] = {
		"Refer to Audio Coding Type (CT) Field",
		"Not in Use (1)",
		"Not in Use (2)",
		"Not in Use (3)",
		"MPEG-4 HE AAC",
		"MPEG-4 HE AAC v2",
		"MPEG-4 AAC LC",
		"DRA",
		"MPEG-4 HE AAC + MPEG Surround",
		"Reserved (9)",
		"MPEG-4 AAC LC + MPEG Surround",
		"MPEG-H 3D Audio",
		"AC-4",
		"L-PCM 3D Audio",
		"Auro-Cx",
		"MPEG-D USAC"
	};
	if ((x[3] & 0x1f) < ARRAY_SIZE(CXT_map))
		printf("  CXT: Audio Coding Extension Type: %s\n", CXT_map[x[3] & 0x1f]);
	else
		printf("  CXT: Audio Coding Extension Type: Reserved (%d)\n", x[3] & 0x1f);
	if ((x[3] & 0x1f) == 9 || (x[3] & 0x1f) >= ARRAY_SIZE(CXT_map))
		fail("CXT: Reserved value.\n");

	static const char *CA_map[] = {
		"FR/FL",
		"LFE1, FR/FL",
		"FC, FR/FL",
		"FC, LFE1, FR/FL",
		"BC, FR/FL",
		"BC, LFE1, FR/FL",
		"BC, FC, FR/FL",
		"BC, FC, LFE1, FR/FL",
		"RS/LS, FR/FL",
		"RS/LS, LFE1, FR/FL",
		"RS/LS, FC, FR/FL",
		"RS/LS, FC, LFE1, FR/FL",
		"BC, RS/LS, FR/FL",
		"BC, RS/LS, LFE1, FR/FL",
		"BC, RS/LS, FC, FR/FL",
		"BC, RS/LS, FC, LFE1, FR/FL",
		"BR/BL, RS/LS, FR/FL",
		"BR/BL, RS/LS, LFE1, FR/FL",
		"BR/BL, RS/LS, FC, FR/FL",
		"BR/BL, RS/LS, FC, LFE1, FR/FL",
		"FRc/FLc, FR/FL",
		"FRc/FLc, LFE1, FR/FL",
		"FRc/FLc, FC, FR/FL",
		"FRc/FLc, FC, LFE1, FR/FL",
		"FRc/FLc, BC, FR/FL",
		"FRc/FLc, BC, LFE1, FR/FL",
		"FRc/FLc, BC, FC, FR/FL",
		"FRc/FLc, BC, FC, LFE1, FR/FL",
		"FRc/FLc, RS/LS, FR/FL",
		"FRc/FLc, RS/LS, LFE1, FR/FL",
		"FRc/FLc, RS/LS, FC, FR/FL",
		"FRc/FLc, RS/LS, FC, LFE1, FR/FL",
		"TpFC, RS/LS, FC, FR/FL",
		"TpFC, RS/LS, FC, LFE1, FR/FL",
		"TpC, RS/LS, FC, FR/FL",
		"TpC, RS/LS, FC, LFE1, FR/FL",
		"TpFR/TpFL, RS/LS, FR/FL",
		"TpFR/TpFL, RS/LS, LFE1, FR/FL",
		"FRw/FLw, RS/LS, FR/FL",
		"FRw/FLw, RS/LS, LFE1, FR/FL",
		"TpC, BC, RS/LS, FC, FR/FL",
		"TpC, BC, RS/LS, FC, LFE1, FR/FL",
		"TpFC, BC, RS/LS, FC, FR/FL",
		"TpFC, BC, RS/LS, FC, FR/FL",
		"TpC, TpFC, RS/LS, FC, FR/FL",
		"TpC, TpFC, RS/LS, FC, FR/FL",
		"TpFR/TpFL, RS/LS, FC, FR/FL",
		"TpFR/TpFL, RS/LS, FC, FR/FL",
		"FRw/FLw, RS/LS, FC, FR/FL",
		"FRw/FLw, RS/LS, FC, FR/FL"
	};
	if (x[4] < ARRAY_SIZE(CA_map))
		printf("  CA: Channel Allocation: %s\n", CA_map[x[4]]);
	else if (x[4] < 0xfe) {
		printf("  CA: Channel Allocation: Reserved (%d, 0x%02x)\n", x[4], x[4]);
		fail("CA: Reserved value.\n");
	}
	else if (x[4] == 0xfe)
		printf("  CA: Channel Allocation: According to the Speaker Mask\n");
	else
		printf("  CA: Channel Allocation: According to Channel Index\n");
	printf("  LSV: Level Shift Value: %d dB\n", (x[5] >> 3) & 0xf);
	printf("  DM_INH: Allow the Down Mixed Stereo Output: %s\n",
	       (x[5] & 0x80) ? "Prohibited" : "Yes");

	static const char *LFEPBL_map[] = {
		"Unknown or refer to other information",
		"0 dB",
		"+10 dB",
		"Reserved"
	};
	printf("  LFEPBL: LFE Playback Level compared to other channels: %s\n", LFEPBL_map[x[5] & 3]);
	if ((x[5] & 3) == 3)
		fail("LFEPBL: Reserved value.\n");

	if (x[4] < 0xfe)
		return;

	if (x[4] == 0xfe) {
		if (x[6] & 0x40)
			warn("F66 is not 0, the use of this bit is deprecated.\n");
		if (x[8] & 0x08)
			warn("F83 is not 0, the use of this bit is deprecated.\n");
		printf("  SPM: Speaker Mask:\n");

		unsigned spm = (x[8] << 16) | (x[7] << 8) | x[6];

		for (unsigned i = 0; cta_speaker_map[i]; i++) {
			if ((spm >> i) & 1)
				printf("    %s\n", cta_speaker_map[i]);
		}
		return;
	}

	// CA == 0xff
	printf("  CID: Channel Index: ");

	unsigned cid = (x[9] << 24) | (x[8] << 16) | (x[7] << 8) | x[6];
	bool first = true;

	for (unsigned i = 0; i < 32; i++) {
		if ((cid >> i) & 1) {
			if (!first)
				printf(" ,");
			first = false;
			printf("%u", i);
		}
	}
	printf("\n");
}

void edid_state::parse_if_mpeg_source(const unsigned char *x, unsigned size)
{
	data_block = "MPEG Source InfoFrame";

	warn("The use of the %s is not recommended.\n", data_block.c_str());

	if (parse_if_hdr(x, size))
		return;

	if (x[1] != 1) {
		fail("Invalid version %d\n", x[1]);
		return;
	}
	if (x[2] < 10) {
		fail("Expected InfoFrame length of 10, got %d.\n", x[2]);
		return;
	}

	// After this x[1] will refer to Data Byte 1
	x += 2;

	unsigned mb = (x[4] << 24) | (x[3] << 16) | (x[2] << 8) | x[1];

	if (mb)
		printf("  MB: MPEG Bit Rate: %u Hz\n", mb);
	else
		printf("  MB: MPEG Bit Rate: Unknown/Does Not Apply\n");

	static const char *MF_map[] = {
		"Unknown (No Data)",
		"I Picture",
		"B Picture",
		"P Picture"
	};
	printf("  MF: MPEG Frame: %s\n", MF_map[x[5] & 3]);
	printf("  FR: Field Repeat: %s\n", (x[5] & 0x10) ? "Repeated Field" : "New Field (Picture)");
	if (x[5] & 0xec)
		fail("Bits F57-F55 and/or F53-F52 are not 0.\n");
	if (x[6] || x[7] || x[8] || x[9] || x[10])
		fail("Bits F100-F60 are not 0.\n");
}

void edid_state::parse_if_ntsc_vbi(const unsigned char *x, unsigned size)
{
	data_block = "NTSC VBI InfoFrame";

	if (parse_if_hdr(x, size))
		return;

	int len = x[2];

	if (x[1] != 1) {
		fail("Invalid version %d\n", x[1]);
		return;
	}

	// After this x[1] will refer to Data Byte 1
	x += 2;

	// See SCTE 127, Table 2 for more details
	hex_block("  PES_data_field: ", x + 1, len, false, len);
}

void edid_state::parse_if_drm(const unsigned char *x, unsigned size)
{
	unsigned length = x[2];

	data_block = "Dynamic Range and Mastering InfoFrame";

	if (parse_if_hdr(x, size))
		return;

	if (x[1] != 1) {
		fail("Invalid version %d\n", x[1]);
		return;
	}

	// After this x[1] will refer to Data Byte 1
	x += 2;

	if (x[1] & 0xf8)
		fail("Bits F17-F13 are not 0.\n");

	static const char *TF_map[] = {
		"Traditional Gamma - SDR Luminance Range",
		"Traditional Gamma - HDR Luminance Range",
		"Perceptual Quantization (PQ) based on SMPTE ST 2084",
		"Hybrid Log-Gamma (HLG) based on Rec. ITU-R BT.2100",
		"Reserved (4)",
		"Reserved (5)",
		"Reserved (6)",
		"Reserved (7)"
	};
	printf("Transfer Function: %s\n",
	       TF_map[x[1] & 7]);
	if (length < 2)
		return;

	if (x[2] & 0xf8)
		fail("Bits F27-F23 are not 0.\n");
	if (x[2] & 7) {
		printf("Static Metadata Descriptor ID: Reserved (%d)\n", x[2] & 7);
		if (!memchk(x + 3, length - 2))
			fail("Trailing non-zero bytes.\n");
		return;
	}
	printf("Static Metadata Descriptor ID: Type 1\n");
	if (length < 26) {
		fail("Expected a length of 26, got %d.\n", length);
		return;
	}
	if (!memchk(x + 3, 12)) {
		printf("    Display Primary 0: (%.5f, %.5f)\n", chrom2d(x + 3), chrom2d(x + 5));
		printf("    Display Primary 1: (%.5f, %.5f)\n", chrom2d(x + 7), chrom2d(x + 7));
		printf("    Display Primary 2: (%.5f, %.5f)\n", chrom2d(x + 11), chrom2d(x + 13));
	}
	if (!memchk(x + 15, 4)) {
		printf("    White Point: (%.5f, %.5f)\n", chrom2d(x + 15), chrom2d(x + 17));
	}
	if (!memchk(x + 19, 2)) {
		printf("    Max Display Mastering Luminance: %u cd/m^2\n",
		       x[19] + (x[20] << 8));
	}
	if (!memchk(x + 21, 2)) {
		printf("    Min Display Mastering Luminance: %f cd/m^2\n",
		       (x[21] + (x[22] << 8)) * 0.0001);
	}
	if (!memchk(x + 23, 2)) {
		printf("    Maximum Content Light Level (MaxCLL): %u cd/m^2\n",
		       x[23] + (x[24] << 8));
	}
	if (!memchk(x + 25, 2)) {
		printf("    Maximum Frame-Average Light Level (MaxFALL): %u cd/m^2\n",
		       x[25] + (x[26] << 8));
	}
}
