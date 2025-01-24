// SPDX-License-Identifier: MIT
/*
 * Copyright 2024 Linaro Ltd. All rights reserved.
 *
 * Author: Dmitry Baryshkov <dmitry.baryshkov@linaro.org>
 */

#include <stdio.h>

#include "edid-decode.h"

void edid_state::parse_eld_baseline(const unsigned char *x, unsigned size)
{
	unsigned mnl, sad_count, data;
	unsigned char dummy_sadb[3] = {};
	char *manufacturer;

	printf("Baseline ELD:\n");

	if (size < 16) {
		fail("Baseline ELD too short (%d)\n", size);
		return;
	}

	mnl = x[0] & 0x1f;

	data = x[0] >> 5;
	switch (data) {
	case 0:
		printf("  no CEA EDID Timing Extension present\n");
		break;
	case 1:
		printf("  CEA EDID 861\n");
		break;
	case 2:
		printf("  CEA EDID 861.A\n");
		break;
	case 3:
		printf("  CEA EDID 861.B/C/D\n");
		break;
	default:
		warn("Unsupported CEA EDID version (%d)\n", data);
		break;
	}

	if (x[1] & 1)
		printf("  HDCP Content Protection is supported\n");
	if (x[1] & 2)
		printf("  ACP / ISRC / ISRC2 packets are handled\n");

	data = (x[1] >> 2) & 3;
	switch (data) {
	case 0:
		printf("  HDMI connection\n");
		break;
	case 1:
		printf("  DisplayPort connection\n");
		break;
	default:
		warn("  Unrecognized connection type (%d)\n", data);
	}

	sad_count = x[1] >> 4;

	if (x[2])
		printf("  Audio latency: %d ms\n", x[2] * 2);
	else
		printf("  No Audio latency information\n");

	printf("  Speaker Allocation:\n");
	dummy_sadb[0] = x[3];
	cta_sadb(dummy_sadb, sizeof(dummy_sadb));

	printf("  Port ID:\n");
	hex_block("    ", x + 0x4, 8);

	manufacturer = manufacturer_name(x + 0x0c);
	printf("  Manufacturer: %s\n", manufacturer);
	printf("  Model: %u\n", (unsigned short)(x[0x0e] + (x[0x0f] << 8)));

	if (0x10 + mnl >= size) {
		fail("Manufacturer name overflow (MNL = %d)\n", mnl);
		return;
	}

	printf("  Display Product Name: '%s'\n", extract_string(x + 0x10, mnl, true));

	if (0x10 + mnl + (3 * sad_count) >= size) {
		fail("Short Audio Descriptors overflow\n");
		return;
	}

	if (sad_count != 0) {
		printf("  Short Audio Descriptors:\n");
		cta_audio_block(x + 0x10 + mnl, 3 * sad_count);
	}
}
