// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2020 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil+cisco@kernel.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "edid-decode.h"

char *edid_state::manufacturer_name(const unsigned char *x)
{
	static char name[4];

	name[0] = ((x[0] & 0x7c) >> 2) + '@';
	name[1] = ((x[0] & 0x03) << 3) + ((x[1] & 0xe0) >> 5) + '@';
	name[2] = (x[1] & 0x1f) + '@';
	name[3] = 0;

	if (!isupper(name[0]) || !isupper(name[1]) || !isupper(name[2]))
		fail("Manufacturer name field contains garbage.\n");

	return name;
}

static const struct {
	unsigned dmt_id;
	unsigned std_id;
	unsigned cvt_id;
	struct timings t;
} dmt_timings[] = {
	{ 0x01, 0x0000, 0x000000, { 640, 350, 64, 35, 31500, 0, false,
				    32, 64, 96, true, 32, 3, 60, false } },

	{ 0x02, 0x3119, 0x000000, { 640, 400, 16, 10, 31500, 0, false,
				    32, 64, 96, false, 1, 3, 41, true } },

	{ 0x03, 0x0000, 0x000000, { 720, 400, 9, 5, 35500, 0, false,
				    36, 72, 108, false, 1, 3, 42, true } },

	{ 0x04, 0x3140, 0x000000, { 640, 480, 4, 3, 25175, 0, false,
				    8, 96, 40, false, 2, 2, 25, false, 8, 8 } },
	{ 0x05, 0x314c, 0x000000, { 640, 480, 4, 3, 31500, 0, false,
				    16, 40, 120, false, 1, 3, 20, false, 8, 8 } },
	{ 0x06, 0x314f, 0x000000, { 640, 480, 4, 3, 31500, 0, false,
				    16, 64, 120, false, 1, 3, 16, false } },
	{ 0x07, 0x3159, 0x000000, { 640, 480, 4, 3, 36000, 0, false,
				    56, 56, 80, false, 1, 3, 25, false } },

	{ 0x08, 0x0000, 0x000000, { 800, 600, 4, 3, 36000, 0, false,
				    24, 72, 128, true, 1, 2, 22, true } },
	{ 0x09, 0x4540, 0x000000, { 800, 600, 4, 3, 40000, 0, false,
				    40, 128, 88, true, 1, 4, 23, true } },
	{ 0x0a, 0x454c, 0x000000, { 800, 600, 4, 3, 50000, 0, false,
				    56, 120, 64, true, 37, 6, 23, true } },
	{ 0x0b, 0x454f, 0x000000, { 800, 600, 4, 3, 49500, 0, false,
				    16, 80, 160, true, 1, 3, 21, true } },
	{ 0x0c, 0x4559, 0x000000, { 800, 600, 4, 3, 56250, 0, false,
				    32, 64, 152, true, 1, 3, 27, true } },
	{ 0x0d, 0x0000, 0x000000, { 800, 600, 4, 3, 73250, 1, false,
				    48, 32, 80, true, 3, 4, 29, false } },

	{ 0x0e, 0x0000, 0x000000, { 848, 480, 16, 9, 33750, 0, false,
				    16, 112, 112, true, 6, 8, 23, true } },

	{ 0x0f, 0x0000, 0x000000, { 1024, 768, 4, 3, 44900, 0, true,
				    8, 176, 56, true, 0, 4, 20, true } },
	{ 0x10, 0x6140, 0x000000, { 1024, 768, 4, 3, 65000, 0, false,
				    24, 136, 160, false, 3, 6, 29, false } },
	{ 0x11, 0x614c, 0x000000, { 1024, 768, 4, 3, 75000, 0, false,
				    24, 136, 144, false, 3, 6, 29, false } },
	{ 0x12, 0x614f, 0x000000, { 1024, 768, 4, 3, 78750, 0, false,
				    16, 96, 176, true, 1, 3, 28, true } },
	{ 0x13, 0x6159, 0x000000, { 1024, 768, 4, 3, 94500, 0, false,
				    48, 96, 208, true, 1, 3, 36, true } },
	{ 0x14, 0x0000, 0x000000, { 1024, 768, 4, 3, 115500, 1, false,
				    48, 32, 80, true, 3, 4, 38, false } },

	{ 0x15, 0x714f, 0x000000, { 1152, 864, 4, 3, 108000, 0, false,
				    64, 128, 256, true, 1, 3, 32, true } },

	{ 0x55, 0x81c0, 0x000000, { 1280, 720, 16, 9, 74250, 0, false,
				    110, 40, 220, true, 5, 5, 20, true } },

	{ 0x16, 0x0000, 0x7f1c21, { 1280, 768, 5, 3, 68250, 1, false,
				    48, 32, 80, true, 3, 7, 12, false } },
	{ 0x17, 0x0000, 0x7f1c28, { 1280, 768, 5, 3, 79500, 0, false,
				    64, 128, 192, false, 3, 7, 20, true } },
	{ 0x18, 0x0000, 0x7f1c44, { 1280, 768, 5, 3, 102250, 0, false,
				    80, 128, 208, false, 3, 7, 27, true } },
	{ 0x19, 0x0000, 0x7f1c62, { 1280, 768, 5, 3, 117500, 0, false,
				    80, 136, 216, false, 3, 7, 31, true } },
	{ 0x1a, 0x0000, 0x000000, { 1280, 768, 5, 3, 140250, 0, false,
				    48, 32, 80, true, 3, 7, 35, false } },

	{ 0x1b, 0x0000, 0x8f1821, { 1280, 800, 16, 10, 71000, 1, false,
				    48, 32, 80, true, 3, 6, 14, false } },
	{ 0x1c, 0x8100, 0x8f1828, { 1280, 800, 16, 10, 83500, 0, false,
				    72, 128, 200, false, 3, 6, 22, true } },
	{ 0x1d, 0x810f, 0x8f1844, { 1280, 800, 16, 10, 106500, 0, false,
				    80, 128, 208, false, 3, 6, 29, true } },
	{ 0x1e, 0x8119, 0x8f1862, { 1280, 800, 16, 10, 122500, 0, false,
				    80, 136, 216, false, 3, 6, 34, true } },
	{ 0x1f, 0x0000, 0x000000, { 1280, 800, 16, 10, 146250, 1, false,
				    48, 32, 80, true, 3, 6, 38, false } },

	{ 0x20, 0x8140, 0x000000, { 1280, 960, 4, 3, 108000, 0, false,
				    96, 112, 312, true, 1, 3, 36, true } },
	{ 0x21, 0x8159, 0x000000, { 1280, 960, 4, 3, 148500, 0, false,
				    64, 160, 224, true, 1, 3, 47, true } },
	{ 0x22, 0x0000, 0x000000, { 1280, 960, 4, 3, 175500, 1, false,
				    48, 32, 80, true, 3, 4, 50, false } },

	{ 0x23, 0x8180, 0x000000, { 1280, 1024, 5, 4, 108000, 0, false,
				    48, 112, 248, true, 1, 3, 38, true } },
	{ 0x24, 0x818f, 0x000000, { 1280, 1024, 5, 4, 135000, 0, false,
				    16, 144, 248, true, 1, 3, 38, true } },
	{ 0x25, 0x8199, 0x000000, { 1280, 1024, 5, 4, 157500, 0, false,
				    64, 160, 224, true, 1, 3, 44, true } },
	{ 0x26, 0x0000, 0x000000, { 1280, 1024, 5, 4, 187250, 1, false,
				    48, 32, 80, true, 3, 7, 50, false } },

	{ 0x27, 0x0000, 0x000000, { 1360, 768, 85, 48, 85500, 0, false,
				    64, 112, 256, true, 3, 6, 18, true } },
	{ 0x28, 0x0000, 0x000000, { 1360, 768, 85, 48, 148250, 1, false,
				    48, 32, 80, true, 3, 5, 37, false } },

	{ 0x51, 0x0000, 0x000000, { 1366, 768, 85, 48, 85500, 0, false,
				    70, 143, 213, true, 3, 3, 24, true } },
	{ 0x56, 0x0000, 0x000000, { 1366, 768, 85, 48, 72000, 1, false,
				    14, 56, 64, true, 1, 3, 28, true } },

	{ 0x29, 0x0000, 0x0c2021, { 1400, 1050, 4, 3, 101000, 1, false,
				    48, 32, 80, true, 3, 4, 23, false } },
	{ 0x2a, 0x9040, 0x0c2028, { 1400, 1050, 4, 3, 121750, 0, false,
				    88, 144, 232, false, 3, 4, 32, true } },
	{ 0x2b, 0x904f, 0x0c2044, { 1400, 1050, 4, 3, 156000, 0, false,
				    104, 144, 248, false, 3, 4, 42, true } },
	{ 0x2c, 0x9059, 0x0c2062, { 1400, 1050, 4, 3, 179500, 0, false,
				    104, 152, 256, false, 3, 4, 48, true } },
	{ 0x2d, 0x0000, 0x000000, { 1400, 1050, 4, 3, 208000, 1, false,
				    48, 32, 80, true, 3, 4, 55, false } },

	{ 0x2e, 0x0000, 0xc11821, { 1440, 900, 16, 10, 88750, 1, false,
				    48, 32, 80, true, 3, 6, 17, false } },
	{ 0x2f, 0x9500, 0xc11828, { 1440, 900, 16, 10, 106500, 0, false,
				    80, 152, 232, false, 3, 6, 25, true } },
	{ 0x30, 0x950f, 0xc11844, { 1440, 900, 16, 10, 136750, 0, false,
				    96, 152, 248, false, 3, 6, 33, true } },
	{ 0x31, 0x9519, 0xc11868, { 1440, 900, 16, 10, 157000, 0, false,
				    104, 152, 256, false, 3, 6, 39, true } },
	{ 0x32, 0x0000, 0x000000, { 1440, 900, 16, 10, 182750, 1, false,
				    48, 32, 80, true, 3, 6, 44, false } },

	{ 0x53, 0xa9c0, 0x000000, { 1600, 900, 16, 9, 108000, 1, false,
				    24, 80, 96, true, 1, 3, 96, true } },

	{ 0x33, 0xa940, 0x000000, { 1600, 1200, 4, 3, 162000, 0, false,
				    64, 192, 304, true, 1, 3, 46, true } },
	{ 0x34, 0xa945, 0x000000, { 1600, 1200, 4, 3, 175500, 0, false,
				    64, 192, 304, true, 1, 3, 46, true } },
	{ 0x35, 0xa94a, 0x000000, { 1600, 1200, 4, 3, 189000, 0, false,
				    64, 192, 304, true, 1, 3, 46, true } },
	{ 0x36, 0xa94f, 0x000000, { 1600, 1200, 4, 3, 202500, 0, false,
				    64, 192, 304, true, 1, 3, 46, true } },
	{ 0x37, 0xa959, 0x000000, { 1600, 1200, 4, 3, 229500, 0, false,
				    64, 192, 304, true, 1, 3, 46, true } },
	{ 0x38, 0x0000, 0x000000, { 1600, 1200, 4, 3, 268250, 1, false,
				    48, 32, 80, true, 3, 4, 64, false } },

	{ 0x39, 0x0000, 0x0c2821, { 1680, 1050, 16, 10, 119000, 1, false,
				    48, 32, 80, true, 3, 6, 21, false } },
	{ 0x3a, 0xb300, 0x0c2828, { 1680, 1050, 16, 10, 146250, 0, false,
				    104, 176, 280, false, 3, 6, 30, true } },
	{ 0x3b, 0xb30f, 0x0c2844, { 1680, 1050, 16, 10, 187000, 0, false,
				    120, 176, 296, false, 3, 6, 40, true } },
	{ 0x3c, 0xb319, 0x0c2868, { 1680, 1050, 16, 10, 214750, 0, false,
				    128, 176, 304, false, 3, 6, 46, true } },
	{ 0x3d, 0x0000, 0x000000, { 1680, 1050, 16, 10, 245500, 1, false,
				    48, 32, 80, true, 3, 6, 53, false } },

	{ 0x3e, 0xc140, 0x000000, { 1792, 1344, 4, 3, 204750, 0, false,
				    128, 200, 328, false, 1, 3, 46, true } },
	{ 0x3f, 0xc14f, 0x000000, { 1792, 1344, 4, 3, 261000, 0, false,
				    96, 216, 352, false, 1, 3, 69, true } },
	{ 0x40, 0x0000, 0x000000, { 1792, 1344, 4, 3, 333250, 1, false,
				    48, 32, 80, true, 3, 4, 72, false } },

	{ 0x41, 0xc940, 0x000000, { 1856, 1392, 4, 3, 218250, 0, false,
				    96, 224, 352, false, 1, 3, 43, true } },
	{ 0x42, 0xc94f, 0x000000, { 1856, 1392, 4, 3, 288000, 0, false,
				    128, 224, 352, false, 1, 3, 104, true } },
	{ 0x43, 0x0000, 0x000000, { 1856, 1392, 4, 3, 356500, 1, false,
				    48, 32, 80, true, 3, 4, 74, false } },

	{ 0x52, 0xd1c0, 0x000000, { 1920, 1080, 16, 9, 148500, 0, false,
				    88, 44, 148, true, 4, 5, 36, true } },

	{ 0x44, 0x0000, 0x572821, { 1920, 1200, 16, 10, 154000, 1, false,
				    48, 32, 80, true, 3, 6, 26, false } },
	{ 0x45, 0xd100, 0x572828, { 1920, 1200, 16, 10, 193250, 0, false,
				    136, 200, 336, false, 3, 6, 36, true } },
	{ 0x46, 0xd10f, 0x572844, { 1920, 1200, 16, 10, 245250, 0, false,
				    136, 208, 344, false, 3, 6, 46, true } },
	{ 0x47, 0xd119, 0x572862, { 1920, 1200, 16, 10, 281250, 0, false,
				    144, 208, 352, false, 3, 6, 53, true } },
	{ 0x48, 0x0000, 0x000000, { 1920, 1200, 16, 10, 317000, 1, false,
				    48, 32, 80, true, 3, 6, 62, false } },

	{ 0x49, 0xd140, 0x000000, { 1920, 1440, 4, 3, 234000, 0, false,
				    128, 208, 344, false, 1, 3, 56, true } },
	{ 0x4a, 0xd14f, 0x000000, { 1920, 1440, 4, 3, 297000, 0, false,
				    144, 224, 352, false, 1, 3, 56, true } },
	{ 0x4b, 0x0000, 0x000000, { 1920, 1440, 4, 3, 380500, 1, false,
				    48, 32, 80, true, 2, 3, 78, false } },

	{ 0x54, 0xe1c0, 0x000000, { 2048, 1152, 16, 9, 162000, 1, false,
				    26, 80, 96, true, 1, 3, 44, true } },

	{ 0x4c, 0x0000, 0x1f3821, { 2560, 1600, 16, 10, 268500, 1, false,
				    48, 32, 80, true, 3, 6, 37, false } },
	{ 0x4d, 0x0000, 0x1f3828, { 2560, 1600, 16, 10, 348500, 0, false,
				    192, 280, 472, false, 3, 6, 49, true } },
	{ 0x4e, 0x0000, 0x1f3844, { 2560, 1600, 16, 10, 443250, 0, false,
				    208, 280, 488, false, 3, 6, 63, true } },
	{ 0x4f, 0x0000, 0x1f3862, { 2560, 1600, 16, 10, 505250, 0, false,
				    208, 280, 488, false, 3, 6, 73, true } },
	{ 0x50, 0x0000, 0x000000, { 2560, 1600, 16, 10, 552750, 1, false,
				    48, 32, 80, true, 3, 6, 85, false } },

	{ 0x57, 0x0000, 0x000000, { 4096, 2160, 256, 135, 556744, 1, false,
				    8, 32, 40, true, 48, 8, 6, false } },
	{ 0x58, 0x0000, 0x000000, { 4096, 2160, 256, 135, 556188, 1, false,
				    8, 32, 40, true, 48, 8, 6, false } },
};

// The timings for the IBM/Apple modes are copied from the linux
// kernel timings in drivers/gpu/drm/drm_edid.c, except for the
// 1152x870 Apple format, which is copied from
// drivers/video/fbdev/macmodes.c since the drm_edid.c version
// describes a 1152x864 format.
static const struct {
	unsigned dmt_id;
	struct timings t;
	const char *type;
} established_timings12[] = {
	/* 0x23 bit 7 - 0 */
	{ 0x00, { 720, 400, 9, 5, 28320, 0, false,
	          18, 108, 54, false, 21, 2, 26, true }, "IBM" },
	{ 0x00, { 720, 400, 9, 5, 35500, 0, false,
	          18, 108, 54, false, 12, 2, 35, true }, "IBM" },
	{ 0x04 },
	{ 0x00, { 640, 480, 4, 3, 30240, 0, false,
	          64, 64, 96, false, 3, 3, 39, false }, "Apple" },
	{ 0x05 },
	{ 0x06 },
	{ 0x08 },
	{ 0x09 },
	/* 0x24 bit 7 - 0 */
	{ 0x0a },
	{ 0x0b },
	{ 0x00, { 832, 624, 4, 3, 57284, 0, false,
	          32, 64, 224, false, 1, 3, 39, false }, "Apple" },
	{ 0x0f },
	{ 0x10 },
	{ 0x11 },
	{ 0x12 },
	{ 0x24 },
	/* 0x25 bit 7 */
	{ 0x00, { 1152, 870, 192, 145, 100000, 0, false,
	          48, 128, 128, true, 3, 3, 39, true }, "Apple" },
};

// The bits in the Established Timings III map to DMT timings,
// this array has the DMT IDs.
static const unsigned char established_timings3_dmt_ids[] = {
	/* 0x06 bit 7 - 0 */
	0x01, // 640x350@85
	0x02, // 640x400@85
	0x03, // 720x400@85
	0x07, // 640x480@85
	0x0e, // 848x480@60
	0x0c, // 800x600@85
	0x13, // 1024x768@85
	0x15, // 1152x864@75
	/* 0x07 bit 7 - 0 */
	0x16, // 1280x768@60 RB
	0x17, // 1280x768@60
	0x18, // 1280x768@75
	0x19, // 1280x768@85
	0x20, // 1280x960@60
	0x21, // 1280x960@85
	0x23, // 1280x1024@60
	0x25, // 1280x1024@85
	/* 0x08 bit 7 - 0 */
	0x27, // 1360x768@60
	0x2e, // 1440x900@60 RB
	0x2f, // 1440x900@60
	0x30, // 1440x900@75
	0x31, // 1440x900@85
	0x29, // 1400x1050@60 RB
	0x2a, // 1400x1050@60
	0x2b, // 1400x1050@75
	/* 0x09 bit 7 - 0 */
	0x2c, // 1400x1050@85
	0x39, // 1680x1050@60 RB
	0x3a, // 1680x1050@60
	0x3b, // 1680x1050@75
	0x3c, // 1680x1050@85
	0x33, // 1600x1200@60
	0x34, // 1600x1200@65
	0x35, // 1600x1200@70
	/* 0x0a bit 7 - 0 */
	0x36, // 1600x1200@75
	0x37, // 1600x1200@85
	0x3e, // 1792x1344@60
	0x3f, // 1792x1344@75
	0x41, // 1856x1392@60
	0x42, // 1856x1392@75
	0x44, // 1920x1200@60 RB
	0x45, // 1920x1200@60
	/* 0x0b bit 7 - 4 */
	0x46, // 1920x1200@75
	0x47, // 1920x1200@85
	0x49, // 1920x1440@60
	0x4a, // 1920x1440@75
};

const struct timings *find_dmt_id(unsigned char dmt_id)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(dmt_timings); i++)
		if (dmt_timings[i].dmt_id == dmt_id)
			return &dmt_timings[i].t;
	return NULL;
}

static const struct timings *find_std_id(unsigned short std_id, unsigned char &dmt_id)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(dmt_timings); i++)
		if (dmt_timings[i].std_id == std_id) {
			dmt_id = dmt_timings[i].dmt_id;
			return &dmt_timings[i].t;
		}
	return NULL;
}

void edid_state::list_established_timings()
{
	printf("Established Timings I & II, 'Byte' is the EDID address:\n\n");
	for (unsigned i = 0; i < ARRAY_SIZE(established_timings12); i++) {
		unsigned char dmt_id = established_timings12[i].dmt_id;
		const struct timings *t;
		char type[16];

		if (dmt_id) {
			sprintf(type, "DMT 0x%02x", dmt_id);
			t = find_dmt_id(dmt_id);
		} else {
			t = &established_timings12[i].t;
			sprintf(type, "%-8s", established_timings12[i].type);
		}
		printf("Byte 0x%02x, Bit %u: ", 0x23 + i / 8, 7 - i % 8);
		print_timings("", t, type, "", false, false);
	}
	printf("\nEstablished timings III, 'Byte' is the offset from the start of the descriptor:\n\n");
	for (unsigned i = 0; i < ARRAY_SIZE(established_timings3_dmt_ids); i++) {
		unsigned char dmt_id = established_timings3_dmt_ids[i];
		char type[16];

		sprintf(type, "DMT 0x%02x", dmt_id);
		printf("Byte 0x%02x, Bit %u: ", 6 + i / 8, 7 - i % 8);
		print_timings("", find_dmt_id(dmt_id), type, "", false, false);
	}
}

const struct timings *close_match_to_dmt(const timings &t, unsigned &dmt)
{
	for (unsigned i = 0; i < ARRAY_SIZE(dmt_timings); i++) {
		if (timings_close_match(t, dmt_timings[i].t)) {
			dmt = dmt_timings[i].dmt_id;
			return &dmt_timings[i].t;
		}
	}
	dmt = 0;
	return NULL;
}

void edid_state::list_dmts()
{
	char type[16];

	for (unsigned i = 0; i < ARRAY_SIZE(dmt_timings); i++) {
		sprintf(type, "DMT 0x%02x", dmt_timings[i].dmt_id);
		std::string flags;
		if (dmt_timings[i].std_id)
			flags += std::string("STD: ") +
				utohex(dmt_timings[i].std_id >> 8) + " " +
				utohex(dmt_timings[i].std_id & 0xff);
		if (dmt_timings[i].cvt_id)
			add_str(flags, std::string("CVT: ") +
				utohex(dmt_timings[i].cvt_id >> 16) + " " +
				utohex((dmt_timings[i].cvt_id >> 8) & 0xff) + " " +
				utohex(dmt_timings[i].cvt_id & 0xff));
		print_timings("", &dmt_timings[i].t, type, flags.c_str(), false, false);
	}
}

void edid_state::detailed_cvt_descriptor(const char *prefix, const unsigned char *x, bool first)
{
	static const unsigned char empty[3] = { 0, 0, 0 };
	struct timings cvt_t = {};
	unsigned char preferred;

	if (!first && !memcmp(x, empty, 3))
		return;

	cvt_t.vact = x[0];
	if (!cvt_t.vact)
		fail("CVT byte 0 is 0, which is a reserved value.\n");
	cvt_t.vact |= (x[1] & 0xf0) << 4;
	cvt_t.vact++;
	cvt_t.vact *= 2;

	switch (x[1] & 0x0c) {
	case 0x00:
	default: /* avoids 'width/ratio may be used uninitialized' warnings */
		cvt_t.hratio = 4;
		cvt_t.vratio = 3;
		break;
	case 0x04:
		cvt_t.hratio = 16;
		cvt_t.vratio = 9;
		break;
	case 0x08:
		cvt_t.hratio = 16;
		cvt_t.vratio = 10;
		break;
	case 0x0c:
		cvt_t.hratio = 15;
		cvt_t.vratio = 9;
		break;
	}
	cvt_t.hact = 8 * (((cvt_t.vact * cvt_t.hratio) / cvt_t.vratio) / 8);

	if (x[1] & 0x03)
		fail("Reserved bits of CVT byte 1 are non-zero.\n");
	if (x[2] & 0x80)
		fail("Reserved bit of CVT byte 2 is non-zero.\n");
	if (!(x[2] & 0x1f))
		fail("CVT byte 2 does not support any vertical rates.\n");
	preferred = (x[2] & 0x60) >> 5;
	if (preferred == 1 && (x[2] & 0x01))
		preferred = 4;
	if (!(x[2] & (1 << (4 - preferred))))
		fail("The preferred CVT Vertical Rate is not supported.\n");

	static const char *s_pref = "preferred vertical rate";

	if (x[2] & 0x10) {
		edid_cvt_mode(50, cvt_t);
		print_timings(prefix, &cvt_t, "CVT", preferred == 0 ? s_pref : "");
	}
	if (x[2] & 0x08) {
		edid_cvt_mode(60, cvt_t);
		print_timings(prefix, &cvt_t, "CVT", preferred == 1 ? s_pref : "");
	}
	if (x[2] & 0x04) {
		edid_cvt_mode(75, cvt_t);
		print_timings(prefix, &cvt_t, "CVT", preferred == 2 ? s_pref : "");
	}
	if (x[2] & 0x02) {
		edid_cvt_mode(85, cvt_t);
		print_timings(prefix, &cvt_t, "CVT", preferred == 3 ? s_pref : "");
	}
	if (x[2] & 0x01) {
		cvt_t.rb = RB_CVT_V1;
		edid_cvt_mode(60, cvt_t);
		print_timings(prefix, &cvt_t, "CVT", preferred == 4 ? s_pref : "");
	}
}

// Base Block uses Code Page 437, unprintable characters are represented by ▯
static const char *cp437[256] = {
"▯", "☺", "☻", "♥", "♦", "♣", "♠", "•", "◘", "○", "◙", "♂", "♀", "♪", "♫", "☼",
"►", "◄", "↕", "‼", "¶", "§", "▬", "↨", "↑", "↓", "→", "←", "∟", "↔", "▲", "▼",
" ", "!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+", ",", "-", ".", "/",
"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?",
"@", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "\\", "]", "^", "_",
"`", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
"p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "{", "|", "}", "~", "⌂",
"Ç", "ü", "é", "â", "ä", "à", "å", "ç", "ê", "ë", "è", "ï", "î", "ì", "Ä", "Å",
"É", "æ", "Æ", "ô", "ö", "ò", "û", "ù", "ÿ", "Ö", "Ü", "¢", "£", "¥", "₧", "ƒ",
"á", "í", "ó", "ú", "ñ", "Ñ", "ª", "º", "¿", "⌐", "¬", "½", "¼", "¡", "«", "»",
"░", "▒", "▓", "│", "┤", "╡", "╢", "╖", "╕", "╣", "║", "╗", "╝", "╜", "╛", "┐",
"└", "┴", "┬", "├", "─", "┼", "╞", "╟", "╚", "╔", "╩", "╦", "╠", "═", "╬", "╧",
"╨", "╤", "╥", "╙", "╘", "╒", "╓", "╫", "╪", "┘", "┌", "█", "▄", "▌", "▐", "▀",
"α", "ß", "Γ", "π", "Σ", "σ", "µ", "τ", "Φ", "Θ", "Ω", "δ", "∞", "φ", "ε", "∩",
"≡", "±", "≥", "≤", "⌠", "⌡", "÷", "≈", "°", "∙", "·", "√", "ⁿ", "²", "■", "▯"
};

// DisplayID uses ISO 8859-1, unprintable chararcters are represented by ▯
static const char *ascii[256] = {
"▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯",
"▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯",
" ", "!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+", ",", "-", ".", "/",
"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?",
"@", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "\\", "]", "^", "_",
"`", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
"p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "{", "|", "}", "~", "▯",
"▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯",
"▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯", "▯",
"▯", "¡", "¢", "£", "¤", "¥", "¦", "§", "¨", "©", "ª", "«", "¬", "▯", "®", "¯",
"°", "±", "²", "³", "´", "µ", "¶", "·", "¸", "¹", "º", "»", "¼", "½", "¾", "¿",
"À", "Á", "Â", "Ã", "Ä", "Å", "Æ", "Ç", "È", "É", "Ê", "Ë", "Ì", "Í", "Î", "Ï",
"Ð", "Ñ", "Ò", "Ó", "Ô", "Õ", "Ö", "×", "Ø", "Ù", "Ú", "Û", "Ü", "Ý", "Þ", "ß",
"à", "á", "â", "ã", "ä", "å", "æ", "ç", "è", "é", "ê", "ë", "ì", "í", "î", "ï",
"ð", "ñ", "ò", "ó", "ô", "õ", "ö", "÷", "ø", "ù", "ú", "û", "ü", "ý", "þ", "ÿ"
};

bool to_utf8 = false;

/* extract a string from a detailed subblock, checking for termination */
char *extract_string(const unsigned char *x, unsigned len, bool is_cp437)
{
	static char s[1025];
	const char **conv = is_cp437 ? cp437 : ascii;
	bool seen_newline = false;
	bool added_space = false;
	unsigned i;

	memset(s, 0, sizeof(s));

	for (i = 0; i < len; i++) {
		// The encoding is cp437, so any character is allowed,
		// but in practice it is unwise to use a non-ASCII character.
		bool non_ascii = (x[i] >= 1 && x[i] < 0x20 && x[i] != 0x0a) || x[i] >= 0x7f;

		if (seen_newline) {
			if (x[i] != 0x20) {
				fail("Non-space after newline.\n");
				return s;
			}
		} else if (x[i] == 0x0a) {
			seen_newline = true;
			if (!i)
				fail("Empty string.\n");
			else if (added_space)
				fail("One or more trailing spaces before newline.\n");
			added_space = false;
		} else if (!x[i]) {
			// While incorrect, a \0 is often used to end the string
			fail("NUL byte at position %u.\n", i);
			return s;
		} else if (x[i] == 0xff) {
			// 0xff is sometimes (incorrectly) used to pad the remainder
			// of the string
			fail("0xff byte at position %u.\n", i);
			return s;
		} else if (!non_ascii) {
			added_space = x[i] == ' ';
			if (to_utf8)
				strcat(s, conv[x[i]]);
			else
				s[i] = x[i];
		} else {
			if (to_utf8) {
				warn("Non-ASCII character 0x%02x (%s) at position %u, can cause problems.\n",
				     x[i], conv[x[i]], i);
				strcat(s, conv[x[i]]);
			} else {
				warn("Non-ASCII character 0x%02x at position %u, can cause problems.\n",
				     x[i], i);
				s[i] = '.';
			}
			added_space = false;
		}
	}
	/* Does the string end with a space? */
	if (!seen_newline && added_space)
		fail("No newline, but one or more trailing spaces.\n");

	return s;
}

void edid_state::print_standard_timing(const char *prefix, unsigned char b1, unsigned char b2,
				       bool gtf_only, bool show_both)
{
	const struct timings *t;
	struct timings formula = {};
	unsigned hratio, vratio;
	unsigned hact, vact, refresh;
	unsigned char dmt_id = 0;

	if (b1 <= 0x01) {
		if (b1 != 0x01 || b2 != 0x01)
			fail("Use 0x0101 as the invalid Standard Timings code, not 0x%02x%02x.\n", b1, b2);
		return;
	}

	t = find_std_id((b1 << 8) | b2, dmt_id);
	if (t) {
		char type[16];
		sprintf(type, "DMT 0x%02x", dmt_id);
		print_timings(prefix, t, type);
		return;
	}
	hact = (b1 + 31) * 8;
	switch ((b2 >> 6) & 0x3) {
	case 0x00:
		if (gtf_only || show_both || base.edid_minor >= 3) {
			hratio = 16;
			vratio = 10;
		} else {
			hratio = 1;
			vratio = 1;
		}
		break;
	case 0x01:
		hratio = 4;
		vratio = 3;
		break;
	case 0x02:
		hratio = 5;
		vratio = 4;
		break;
	case 0x03:
		hratio = 16;
		vratio = 9;
		break;
	}
	vact = (double)hact * vratio / hratio;
	refresh = (b2 & 0x3f) + 60;

	formula.hact = hact;
	formula.vact = vact;
	formula.hratio = hratio;
	formula.vratio = vratio;

	if (!gtf_only && (show_both || base.edid_minor >= 4)) {
		if (show_both || base.supports_cvt) {
			edid_cvt_mode(refresh, formula);
			print_timings(prefix, &formula, "CVT     ",
				      show_both ? "" : "EDID 1.4 source");
		}
		/*
		 * An EDID 1.3 source will assume GTF, so both GTF and CVT
		 * have to be supported.
		 */
		edid_gtf_mode(refresh, formula);
		if (base.supports_cvt)
			print_timings(prefix, &formula, "GTF     ", "EDID 1.3 source");
		else
			print_timings(prefix, &formula, "GTF     ");
	} else if (gtf_only || base.edid_minor >= 2) {
		edid_gtf_mode(refresh, formula);
		print_timings(prefix, &formula, "GTF     ");
	} else {
		printf("%sUnknown : %5ux%-5u %3u.000 Hz %3u:%u\n",
		       prefix, hact, vact, refresh, hratio, vratio);
		min_vert_freq_hz = min(min_vert_freq_hz, refresh);
		max_vert_freq_hz = max(max_vert_freq_hz, refresh);
	}

	// See Ref. D-8 in the EDID-1.4 spec
	if (vact & 1)
		warn("Standard Timing %ux%u has a dubious odd vertical resolution.\n", hact, vact);
}

void edid_state::detailed_display_range_limits(const unsigned char *x)
{
	int h_max_offset = 0, h_min_offset = 0;
	int v_max_offset = 0, v_min_offset = 0;
	int is_cvt = 0;
	bool has_sec_gtf = false;
	std::string range_class;

	data_block = "Display Range Limits";
	printf("    %s:\n", data_block.c_str());
	base.has_display_range_descriptor = 1;

	if (base.edid_minor >= 4) {
		if (x[4] & 0x02) {
			v_max_offset = 255;
			if (x[4] & 0x01) {
				v_min_offset = 255;
			}
		}
		if (x[4] & 0x08) {
			h_max_offset = 255;
			if (x[4] & 0x04) {
				h_min_offset = 255;
			}
		}
	}

	/*
	 * despite the values, this is not a bitfield.
	 */
	switch (x[10]) {
	case 0x00: /* default gtf */
		range_class = "GTF";
		if (base.edid_minor >= 4 && !base.supports_continuous_freq)
			fail("GTF is supported, but the display does not support continuous frequencies.\n");
		if (base.edid_minor >= 4)
			warn("GTF support is deprecated in EDID 1.4.\n");
		break;
	case 0x01: /* range limits only */
		range_class = "Range Limits Only";
		if (base.edid_minor < 4)
			fail("'%s' is not allowed for EDID < 1.4.\n", range_class.c_str());
		break;
	case 0x02: /* secondary gtf curve */
		range_class = "Secondary GTF";
		if (base.edid_minor >= 4 && !base.supports_continuous_freq)
			fail("Secondary GTF is supported, but the display does not support continuous frequencies.\n");
		if (base.edid_minor >= 4)
			warn("GTF support is deprecated in EDID 1.4.\n");
		has_sec_gtf = true;
		break;
	case 0x04: /* cvt */
		range_class = "CVT";
		is_cvt = 1;
		if (base.edid_minor < 4)
			fail("'%s' is not allowed for EDID < 1.4.\n", range_class.c_str());
		else if (!base.supports_continuous_freq)
			fail("CVT is supported, but the display does not support continuous frequencies.\n");
		break;
	default: /* invalid */
		fail("Unknown range class (0x%02x).\n", x[10]);
		range_class = std::string("Unknown (") + utohex(x[10]) + ")";
		break;
	}

	if (x[5] + v_min_offset > x[6] + v_max_offset)
		fail("Min vertical rate > max vertical rate.\n");
	base.min_display_vert_freq_hz = x[5] + v_min_offset;
	base.max_display_vert_freq_hz = x[6] + v_max_offset;
	if (x[7] + h_min_offset > x[8] + h_max_offset)
		fail("Min horizontal freq > max horizontal freq.\n");
	base.min_display_hor_freq_hz = (x[7] + h_min_offset) * 1000;
	base.max_display_hor_freq_hz = (x[8] + h_max_offset) * 1000;
	printf("      Monitor ranges (%s): %d-%d Hz V, %d-%d kHz H",
	       range_class.c_str(),
	       x[5] + v_min_offset, x[6] + v_max_offset,
	       x[7] + h_min_offset, x[8] + h_max_offset);

	// For EDID 1.3 the horizontal frequency maxes out at 255 kHz.
	// So to avoid false range-check warnings due to this limitation,
	// just double the max_display_hor_freq_hz in this case.
	if (base.edid_minor < 4 && x[8] == 0xff)
		base.max_display_hor_freq_hz *= 2;

	// For EDID 1.3 the vertical frequency maxes out at 255 Hz.
	// So to avoid false range-check warnings due to this limitation,
	// just double the max_display_vert_freq_hz in this case.
	if (base.edid_minor < 4 && x[6] == 0xff)
		base.max_display_vert_freq_hz *= 2;

	if (x[9]) {
		base.max_display_pixclk_khz = x[9] * 10000;
		printf(", max dotclock %d MHz\n", x[9] * 10);
	} else {
		printf("\n");
		if (base.edid_minor >= 4)
			fail("EDID 1.4 block does not set max dotclock.\n");
	}

	if (has_sec_gtf) {
		if (x[11])
			fail("Byte 11 is 0x%02x instead of 0x00.\n", x[11]);
		if (memchk(x + 12, 6)) {
			fail("Zeroed Secondary Curve Block.\n");
		} else {
			printf("      GTF Secondary Curve Block:\n");
			printf("        Start frequency: %u kHz\n", x[12] * 2);
			printf("        C: %.1f%%\n", x[13] / 2.0);
			printf("        M: %u%%/kHz\n", (x[15] << 8) | x[14]);
			printf("        K: %u\n", x[16]);
			printf("        J: %.1f%%\n", x[17] / 2.0);
		}
	} else if (is_cvt) {
		int max_h_pixels = 0;

		printf("      CVT version %d.%d\n", (x[11] & 0xf0) >> 4, x[11] & 0x0f);

		if (x[12] & 0xfc) {
			unsigned raw_offset = (x[12] & 0xfc) >> 2;

			printf("      Real max dotclock: %.2f MHz\n",
			       (x[9] * 10) - (raw_offset * 0.25));
			if (raw_offset >= 40)
				warn("CVT block corrects dotclock by more than 9.75 MHz.\n");
		}

		max_h_pixels = x[12] & 0x03;
		max_h_pixels <<= 8;
		max_h_pixels |= x[13];
		max_h_pixels *= 8;
		if (max_h_pixels)
			printf("      Max active pixels per line: %d\n", max_h_pixels);

		printf("      Supported aspect ratios:%s%s%s%s%s\n",
		       x[14] & 0x80 ? " 4:3" : "",
		       x[14] & 0x40 ? " 16:9" : "",
		       x[14] & 0x20 ? " 16:10" : "",
		       x[14] & 0x10 ? " 5:4" : "",
		       x[14] & 0x08 ? " 15:9" : "");
		if (x[14] & 0x07)
			fail("Reserved bits of byte 14 are non-zero.\n");

		printf("      Preferred aspect ratio: ");
		switch ((x[15] & 0xe0) >> 5) {
		case 0x00:
			printf("4:3");
			break;
		case 0x01:
			printf("16:9");
			break;
		case 0x02:
			printf("16:10");
			break;
		case 0x03:
			printf("5:4");
			break;
		case 0x04:
			printf("15:9");
			break;
		default:
			printf("Unknown (0x%02x)", (x[15] & 0xe0) >> 5);
			fail("Invalid preferred aspect ratio 0x%02x.\n",
			     (x[15] & 0xe0) >> 5);
			break;
		}
		printf("\n");

		if (x[15] & 0x08)
			printf("      Supports CVT standard blanking\n");
		if (x[15] & 0x10)
			printf("      Supports CVT reduced blanking\n");

		if (x[15] & 0x07)
			fail("Reserved bits of byte 15 are non-zero.\n");

		if (x[16] & 0xf0) {
			printf("      Supported display scaling:\n");
			if (x[16] & 0x80)
				printf("        Horizontal shrink\n");
			if (x[16] & 0x40)
				printf("        Horizontal stretch\n");
			if (x[16] & 0x20)
				printf("        Vertical shrink\n");
			if (x[16] & 0x10)
				printf("        Vertical stretch\n");
		}

		if (x[16] & 0x0f)
			fail("Reserved bits of byte 16 are non-zero.\n");

		if (x[17])
			printf("      Preferred vertical refresh: %d Hz\n", x[17]);
		else
			warn("CVT block does not set preferred refresh rate.\n");
	} else {
		if (x[11] != 0x0a)
			fail("Byte 11 is 0x%02x instead of 0x0a.\n", x[11]);
		for (unsigned i = 12; i <= 17; i++) {
			if (x[i] != 0x20) {
				fail("Bytes 12-17 must be 0x20.\n");
				break;
			}
		}
	}
}

void edid_state::detailed_epi(const unsigned char *x)
{
	data_block = "EPI Descriptor";
	printf("    %s:\n", data_block.c_str());

	unsigned v = x[5] & 0x07;

	printf("      Bits per pixel: %u\n", 18 + v * 6);
	if (v > 2)
		fail("Invalid bits per pixel.\n");
	v = (x[5] & 0x18) >> 3;
	printf("      Pixels per clock: %u\n", 1 << v);
	if (v > 2)
		fail("Invalid pixels per clock.\n");
	v = (x[5] & 0x60) >> 5;
	printf("      Data color mapping: %sconventional\n", v ? "non-" : "");
	if (v > 1)
		fail("Unknown data color mapping (0x%02x).\n", v);
	if (x[5] & 0x80)
		fail("Non-zero reserved field in byte 5.\n");

	v = x[6] & 0x0f;
	printf("      Interface type: ");
	switch (v) {
	case 0x00: printf("LVDS TFT\n"); break;
	case 0x01: printf("monoSTN 4/8 Bit\n"); break;
	case 0x02: printf("colorSTN 8/16 Bit\n"); break;
	case 0x03: printf("18 Bit TFT\n"); break;
	case 0x04: printf("24 Bit TFT\n"); break;
	case 0x05: printf("TMDS\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", v);
		   fail("Invalid interface type 0x%02x.\n", v);
		   break;
	}
	printf("      DE polarity: DE %s active\n",
	       (x[6] & 0x10) ? "low" : "high");
	printf("      FPSCLK polarity: FPSCLK %sinverted\n",
	       (x[6] & 0x20) ? "" : "not ");
	if (x[6] & 0xc0)
		fail("Non-zero reserved field in byte 6.\n");

	printf("      Vertical display mode: %s\n",
	       (x[7] & 0x01) ? "Up/Down reverse mode" : "normal");
	printf("      Horizontal display mode: %s\n",
	       (x[7] & 0x02) ? "Left/Right reverse mode" : "normal");
	if (x[7] & 0xfc)
		fail("Non-zero reserved field in byte 7.\n");

	v = x[8] & 0x0f;
	printf("      Total power on sequencing delay: ");
	if (v)
		printf("%u ms\n", v * 10);
	else
		printf("VGA controller default\n");
	v = (x[8] & 0xf0) >> 4;
	printf("      Total power off sequencing delay: ");
	if (v)
		printf("%u ms\n", v * 10);
	else
		printf("VGA controller default\n");

	v = x[9] & 0x0f;
	printf("      Contrast power on sequencing delay: ");
	if (v)
		printf("%u ms\n", v * 10);
	else
		printf("VGA controller default\n");
	v = (x[9] & 0xf0) >> 4;
	printf("      Contrast power off sequencing delay: ");
	if (v)
		printf("%u ms\n", v * 10);
	else
		printf("VGA controller default\n");

	v = x[10] & 0x2f;
	const char *s = (x[10] & 0x80) ? "" : " (ignored)";

	printf("      Backlight brightness control: %u steps%s\n", v, s);
	printf("      Backlight enable at boot: %s%s\n",
	       (x[10] & 0x40) ? "off" : "on", s);
	printf("      Backlight control enable: %s\n",
	       (x[10] & 0x80) ? "enabled" : "disabled");

	v = x[11] & 0x2f;
	s = (x[11] & 0x80) ? "" : " (ignored)";

	printf("      Contrast voltable control: %u steps%s\n", v, s);
	if (x[11] & 0x40)
		fail("Non-zero reserved field in byte 11.\n");
	printf("      Contrast control enable: %s\n",
	       (x[11] & 0x80) ? "enabled" : "disabled");

	if (x[12] || x[13] || x[14] || x[15] || x[16])
		fail("Non-zero values in reserved bytes 12-16.\n");

	printf("      EPI Version: %u.%u\n", (x[17] & 0xf0) >> 4, x[17] & 0x0f);
}

void edid_state::detailed_timings(const char *prefix, const unsigned char *x,
				  bool base_or_cta)
{
	struct timings t = {};
	unsigned hbl, vbl;
	std::string s_sync, s_flags;

	// Only count DTDs in base block 0 or CTA-861 extension blocks
	if (base_or_cta)
		base.dtd_cnt++;
	data_block = "Detailed Timing Descriptor #" + std::to_string(base.dtd_cnt);
	t.pixclk_khz = (x[0] + (x[1] << 8)) * 10;
	if (t.pixclk_khz < 10000) {
		printf("%sDetailed mode: ", prefix);
		hex_block("", x, 18, true, 18);
		if (!t.pixclk_khz)
			fail("First two bytes are 0, invalid data.\n");
		else
			fail("Pixelclock < 10 MHz, assuming invalid data 0x%02x 0x%02x.\n",
			     x[0], x[1]);
		return;
	}

	/*
	 * If the borders are non-zero, then it is unclear how to interpret
	 * the DTD blanking parameters.
	 *
	 * According to EDID 1.3 (3.12) the Hor/Vert Blanking includes the
	 * borders, and so does the Hor/Vert Sync Offset.
	 *
	 * According to EDID 1.4 (3.12) the Hor/Vert Blanking excludes the
	 * borders, and they are also excluded from the Hor/Vert Front Porch.
	 *
	 * But looking at what is really done in EDIDs is that the Hor/Vert
	 * Blanking follows EDID 1.3, but the Hor/Vert Front Porch does not
	 * include the border.
	 *
	 * So hbl/vbl includes the borders, so those need to be subtracted,
	 * but hfp/vfp is used as-is.
	 *
	 * In practice you really shouldn't use non-zero borders in DTDs
	 * since clearly nobody knows how to interpret the timing.
	 */
	t.hact = (x[2] + ((x[4] & 0xf0) << 4));
	t.hborder = x[15];
	hbl = (x[3] + ((x[4] & 0x0f) << 8)) - t.hborder * 2;
	t.hfp = (x[8] + ((x[11] & 0xc0) << 2));
	t.hsync = (x[9] + ((x[11] & 0x30) << 4));
	t.hbp = hbl - t.hsync - t.hfp;
	t.vact = (x[5] + ((x[7] & 0xf0) << 4));
	t.vborder = x[16];
	vbl = (x[6] + ((x[7] & 0x0f) << 8)) - t.vborder * 2;
	t.vfp = ((x[10] >> 4) + ((x[11] & 0x0c) << 2));
	t.vsync = ((x[10] & 0x0f) + ((x[11] & 0x03) << 4));
	t.vbp = vbl - t.vsync - t.vfp;

	unsigned char flags = x[17];

	if (base.has_spwg && base.detailed_block_cnt == 2)
		flags = *(x - 1);

	switch ((flags & 0x18) >> 3) {
	case 0x00:
		s_flags = "analog composite";
#ifdef __EMSCRIPTEN__
		[[clang::fallthrough]];
#endif
		/* fall-through */
	case 0x01:
		if (s_flags.empty())
			s_flags = "bipolar analog composite";
		switch ((flags & 0x06) >> 1) {
		case 0x00:
			add_str(s_flags, "sync-on-green");
			break;
		case 0x01:
			break;
		case 0x02:
			add_str(s_flags, "serrate, sync-on-green");
			break;
		case 0x03:
			add_str(s_flags, "serrate");
			break;
		}
		break;
	case 0x02:
		if (flags & (1 << 1))
			t.pos_pol_hsync = true;
		t.no_pol_vsync = true;
		s_flags = "digital composite";
		if (flags & (1 << 2))
		    add_str(s_flags, "serrate");
		break;
	case 0x03:
		if (flags & (1 << 1))
			t.pos_pol_hsync = true;
		if (flags & (1 << 2))
			t.pos_pol_vsync = true;
		s_sync = t.pos_pol_hsync ? "+hsync " : "-hsync ";
		s_sync += t.pos_pol_vsync ? "+vsync " : "-vsync ";
		if (base.has_spwg && (flags & 0x01))
			s_flags = "DE timing only";
		break;
	}
	if (flags & 0x80) {
		t.interlaced = true;
		t.vact *= 2;
		/*
		 * Check if this DTD matches VIC code 39 with special
		 * interlaced timings.
		 */
		if (t.hact == 1920 && t.vact == 1080 && t.pixclk_khz == 72000 &&
		    t.hfp == 32 && t.hsync == 168 && t.hbp == 184 && !t.hborder &&
		    t.vfp == 23 && t.vsync == 5 && t.vbp == 57 && !t.vborder &&
		    !base.has_spwg && cta.preparsed_has_vic[0][39] && (flags & 0x1e) == 0x1a)
			t.even_vtotal = true;
	}
	switch (flags & 0x61) {
	case 0x20:
		add_str(s_flags, "field sequential L/R");
		break;
	case 0x40:
		add_str(s_flags, "field sequential R/L");
		break;
	case 0x21:
		add_str(s_flags, "interleaved right even");
		break;
	case 0x41:
		add_str(s_flags, "interleaved left even");
		break;
	case 0x60:
		add_str(s_flags, "four way interleaved");
		break;
	case 0x61:
		add_str(s_flags, "side by side interleaved");
		break;
	default:
		break;
	}

	t.hsize_mm = x[12] + ((x[14] & 0xf0) << 4);
	t.vsize_mm = x[13] + ((x[14] & 0x0f) << 8);

	calc_ratio(&t);

	std::string s_type = base_or_cta ? dtd_type() : "DTD";
	bool ok = print_timings(prefix, &t, s_type.c_str(), s_flags.c_str(), true);
	timings_ext te(t, s_type, s_flags);

	if (block_nr == 0 && base.dtd_cnt == 1) {
		te.type = "DTD   1";
		base.preferred_timing = te;
		if (has_cta) {
			cta.preferred_timings.push_back(te);
			if (cta.byte3 & 0xf) {
				cta.native_timings.push_back(te);
			}
		}
	}
	if (base_or_cta)
		cta.vec_dtds.push_back(te);

	if (t.hborder || t.vborder)
		warn("The use of non-zero borders in a DTD is not recommended.\n");
	if ((base.max_display_width_mm && !t.hsize_mm) ||
	    (base.max_display_height_mm && !t.vsize_mm)) {
		fail("Mismatch of image size vs display size: image size is not set, but display size is.\n");
	}
	if (base.has_spwg && base.detailed_block_cnt == 2)
		printf("%sSPWG Module Revision: %hhu\n", prefix, x[17]);
	if (!ok) {
		std::string s = prefix;

		s += "               ";
		hex_block(s.c_str(), x, 18, true, 18);
	}
}

bool edid_state::preparse_detailed_block(unsigned char *x)
{
	if (x[0] || x[1])
		return false;

	switch (x[3]) {
	case 0xfd:
		switch (x[10]) {
		case 0x00: /* default gtf */
			base.supports_gtf = true;
			break;
		case 0x02: /* secondary gtf curve */
			base.supports_gtf = true;
			base.supports_sec_gtf = !memchk(x + 12, 6);
			base.sec_gtf_start_freq = x[12] * 2;
			base.C = x[13] / 2.0;
			base.M = (x[15] << 8) | x[14];
			base.K = x[16];
			base.J = x[17] / 2.0;
			break;
		case 0x04: /* cvt */
			if (base.edid_minor >= 4) {
				/* GTF is implied if CVT is signaled */
				base.supports_gtf = true;
				base.supports_cvt = true;
			}
			break;
		}
		break;
	case 0xff:
		data_block = "Display Product Serial Number";
		serial_strings.push_back(extract_string(x + 5, 13, true));
		data_block.clear();
		if (replace_unique_ids) {
			// Replace with 123456
			static const unsigned char sernum[13] = {
				'1', '2', '3', '4', '5', '6',
				'\n', ' ', ' ', ' ', ' ', ' ', ' '
			};
			memcpy(x + 5, sernum, sizeof(sernum));
			return true;
		}
		break;
	}
	return false;
}

void edid_state::detailed_block(const unsigned char *x)
{
	static const unsigned char zero_descr[18] = { 0 };
	unsigned cnt;
	unsigned i;

	base.detailed_block_cnt++;
	if (x[0] || x[1]) {
		detailed_timings("    ", x);
		if (base.seen_non_detailed_descriptor)
			fail("Invalid detailed timing descriptor ordering.\n");
		return;
	}

	data_block = "Display Descriptor #" + std::to_string(base.detailed_block_cnt);
	/* Monitor descriptor block, not detailed timing descriptor. */
	if (x[2] != 0) {
		/* 1.3, 3.10.3 */
		fail("Monitor descriptor block has byte 2 nonzero (0x%02x).\n", x[2]);
	}
	if ((base.edid_minor < 4 || x[3] != 0xfd) && x[4] != 0x00) {
		/* 1.3, 3.10.3 */
		fail("Monitor descriptor block has byte 4 nonzero (0x%02x).\n", x[4]);
	}

	base.seen_non_detailed_descriptor = true;
	if (base.edid_minor == 0)
		fail("Has descriptor blocks other than detailed timings.\n");

	if (!memcmp(x, zero_descr, sizeof(zero_descr))) {
		data_block = "Empty Descriptor";
		printf("    %s\n", data_block.c_str());
		fail("Use Dummy Descriptor instead of all zeroes.\n");
		return;
	}

	switch (x[3]) {
	case 0x0e:
		detailed_epi(x);
		return;
	case 0x10:
		data_block = "Dummy Descriptor";
		printf("    %s:\n", data_block.c_str());
		for (i = 5; i < 18; i++) {
			if (x[i]) {
				fail("Dummy block filled with garbage.\n");
				break;
			}
		}
		return;
	case 0xf7:
		data_block = "Established timings III";
		printf("    %s:\n", data_block.c_str());
		for (i = 0; i < ARRAY_SIZE(established_timings3_dmt_ids); i++)
			if (x[6 + i / 8] & (1 << (7 - i % 8))) {
				unsigned char dmt_id = established_timings3_dmt_ids[i];
				char type[16];

				sprintf(type, "DMT 0x%02x", dmt_id);
				print_timings("      ", find_dmt_id(dmt_id), type);
			}
		if (base.edid_minor < 4)
			fail("Not allowed for EDID < 1.4.\n");
		return;
	case 0xf8:
		data_block = "CVT 3 Byte Timing Codes";
		printf("    %s:\n", data_block.c_str());
		if (x[5] != 0x01) {
			fail("Invalid version number %u.\n", x[5]);
			return;
		}
		for (i = 0; i < 4; i++)
			detailed_cvt_descriptor("      ", x + 6 + (i * 3), !i);
		if (base.edid_minor < 4)
			fail("Not allowed for EDID < 1.4.\n");
		return;
	case 0xf9:
		data_block = "Display Color Management Data";
		printf("    %s:\n", data_block.c_str());
		printf("      Version : %d\n", x[5]);
		printf("      Red a3  : %.2f\n", (short)(x[6] | (x[7] << 8)) / 100.0);
		printf("      Red a2  : %.2f\n", (short)(x[8] | (x[9] << 8)) / 100.0);
		printf("      Green a3: %.2f\n", (short)(x[10] | (x[11] << 8)) / 100.0);
		printf("      Green a2: %.2f\n", (short)(x[12] | (x[13] << 8)) / 100.0);
		printf("      Blue a3 : %.2f\n", (short)(x[14] | (x[15] << 8)) / 100.0);
		printf("      Blue a2 : %.2f\n", (short)(x[16] | (x[17] << 8)) / 100.0);
		return;
	case 0xfa:
		data_block = "Standard Timing Identifications";
		printf("    %s:\n", data_block.c_str());
		for (cnt = i = 0; i < 6; i++) {
			if (x[5 + i * 2] != 0x01 || x[5 + i * 2 + 1] != 0x01)
				cnt++;
			print_standard_timing("      ", x[5 + i * 2], x[5 + i * 2 + 1]);
		}
		if (!cnt)
			warn("%s block without any timings.\n", data_block.c_str());
		return;
	case 0xfb: {
		unsigned w_x, w_y;
		unsigned gamma;

		data_block = "Color Point Data";
		printf("    %s:\n", data_block.c_str());
		w_x = (x[7] << 2) | ((x[6] >> 2) & 3);
		w_y = (x[8] << 2) | (x[6] & 3);
		gamma = x[9];
		printf("      Index: %u White: 0.%04u, 0.%04u", x[5],
		       (w_x * 10000) / 1024, (w_y * 10000) / 1024);
		if (gamma == 0xff)
			printf(" Gamma: is defined in an extension block");
		else
			printf(" Gamma: %.2f", ((gamma + 100.0) / 100.0));
		printf("\n");
		if (x[10] == 0)
			return;
		w_x = (x[12] << 2) | ((x[11] >> 2) & 3);
		w_y = (x[13] << 2) | (x[11] & 3);
		gamma = x[14];
		printf("      Index: %u White: 0.%04u, 0.%04u", x[10],
		       (w_x * 10000) / 1024, (w_y * 10000) / 1024);
		if (gamma == 0xff)
			printf(" Gamma: is defined in an extension block");
		else
			printf(" Gamma: %.2f", ((gamma + 100.0) / 100.0));
		printf("\n");
		return;
	}
	case 0xfc:
		data_block = "Display Product Name";
		base.has_name_descriptor = 1;
		printf("    %s: '%s'\n", data_block.c_str(), extract_string(x + 5, 13, true));
		return;
	case 0xfd:
		detailed_display_range_limits(x);
		return;
	case 0xfe:
		if (!base.has_spwg || base.detailed_block_cnt < 3) {
			data_block = "Alphanumeric Data String";
			printf("    %s: '%s'\n", data_block.c_str(),
			       extract_string(x + 5, 13, true));
			return;
		}
		if (base.detailed_block_cnt == 3) {
			char buf[6] = { 0 };

			data_block = "SPWG Descriptor #3";
			printf("    %s:\n", data_block.c_str());
			memcpy(buf, x + 5, 5);
			if (strlen(buf) != 5)
				fail("Invalid PC Maker P/N length.\n");
			printf("      SPWG PC Maker P/N: '%s'\n", buf);
			printf("      SPWG LCD Supplier EEDID Revision: %hhu\n", x[10]);
			printf("      SPWG Manufacturer P/N: '%s'\n", extract_string(x + 11, 7, true));
		} else {
			data_block = "SPWG Descriptor #4";
			printf("    %s:\n", data_block.c_str());
			printf("      SMBUS Values: 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx"
			       " 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx\n",
			       x[5], x[6], x[7], x[8], x[9], x[10], x[11], x[12]);
			printf("      LVDS Channels: %hhu\n", x[13]);
			printf("      Panel Self Test %sPresent\n", x[14] ? "" : "Not ");
			if (x[15] != 0x0a || x[16] != 0x20 || x[17] != 0x20)
				fail("Invalid trailing data.\n");
		}
		return;
	case 0xff: {
		static const char * const dummy_sn[] = {
			"na",
			"n/a",
			"NA",
			"Serial Number",
			"SerialNumber",
			"Serial_Number",
			"121212121212",
			"1234567890123",
			"20000080",
			"SN-000000001",
			"demoset-1",
			"H1AK500000", // Often used with Samsung displays
			NULL
		};

		data_block = "Display Product Serial Number";
		const char *sn = serial_strings[serial_string_cnt++].c_str();
		if (hide_serial_numbers)
			printf("    %s: ...\n", data_block.c_str());
		else if (replace_unique_ids)
			printf("    %s: '123456'\n", data_block.c_str());
		else
			printf("    %s: '%s'\n", data_block.c_str(), sn);
		bool dummy = true;
		// Any serial numbers consisting only of spaces, 0, and/or 1
		// characters are always considered dummy values.
		for (unsigned i = 0; i < strlen(sn); i++) {
			if (!strchr(" 01", sn[i])) {
				dummy = false;
				break;
			}
		}
		// In addition, check against a list of known dummy S/Ns
		for (unsigned i = 0; !dummy && dummy_sn[i]; i++) {
			if (!strcmp(sn, dummy_sn[i])) {
				dummy = true;
				break;
			}
		}
		if (dummy && sn[0])
			warn("The serial number is one of the known dummy values, is that intended?\n");
		return;
	}
	default:
		printf("    %s Display Descriptor (0x%02hhx):",
		       x[3] <= 0x0f ? "Manufacturer-Specified" : "Unknown", x[3]);
		hex_block(" ", x + 2, 16);
		if (x[3] > 0x0f)
			fail("Unknown Type 0x%02hhx.\n", x[3]);
		return;
	}
}

/*
 * The sRGB chromaticities are (x, y):
 * red:   0.640,  0.330
 * green: 0.300,  0.600
 * blue:  0.150,  0.060
 * white: 0.3127, 0.3290
 */
static const unsigned char srgb_chromaticity[10] = {
	0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26, 0x0f, 0x50, 0x54
};

void edid_state::preparse_base_block(unsigned char *x)
{
	bool update_checksum = false;

	base.serial_number = x[0x0c] + (x[0x0d] << 8) +
			(x[0x0e] << 16) + (x[0x0f] << 24);

	if (base.serial_number && replace_unique_ids) {
		// Replace by 123456
		x[0x0c] = 0x40;
		x[0x0d] = 0xe2;
		x[0x0e] = 0x01;
		x[0x0f] = 0x00;
		update_checksum = true;
	}

	base.week = x[0x10];
	base.year = x[0x11];
	if (replace_unique_ids && base.week != 0xff) {
		x[0x10] = 0;
		x[0x11] = 10;
		update_checksum = true;
	}

	/*
	 * Need to find the Display Range Limit info before reading
	 * the standard timings.
	 */
	update_checksum |= preparse_detailed_block(x + 0x36);
	update_checksum |= preparse_detailed_block(x + 0x48);
	update_checksum |= preparse_detailed_block(x + 0x5a);
	update_checksum |= preparse_detailed_block(x + 0x6c);

	if (update_checksum)
		replace_checksum(x, EDID_PAGE_SIZE);
}

void edid_state::parse_base_block(const unsigned char *x)
{
	time_t the_time;
	struct tm *ptm;
	unsigned col_x, col_y;
	bool has_preferred_timing = false;
	char *manufacturer;

	data_block = "EDID Structure Version & Revision";
	printf("  %s: %hhu.%hhu\n", data_block.c_str(), x[0x12], x[0x13]);
	if (x[0x12] == 1) {
		base.edid_minor = x[0x13];
		if (base.edid_minor > 4)
			warn("Unknown EDID minor version %u, assuming 1.4 conformance.\n", base.edid_minor);
		if (base.edid_minor < 3)
			warn("EDID 1.%u is deprecated, do not use.\n", base.edid_minor);
	} else {
		fail("Unknown EDID major version.\n");
	}

	data_block = "Vendor & Product Identification";
	manufacturer = manufacturer_name(x + 0x08);
	printf("  %s:\n", data_block.c_str());
	printf("    Manufacturer: %s\n    Model: %u\n",
	       manufacturer,
	       (unsigned short)(x[0x0a] + (x[0x0b] << 8)));
	if (!strcmp(manufacturer, "CID")) {
		if (has_cta && !cta.has_pidb)
			fail("Manufacturer name is set to CID, but there is no CTA-861 Product Information Data Block.\n");
		if (has_dispid && !has_cta && !dispid.has_product_identification)
			fail("Manufacturer name is set to CID, but there is no DisplayID Product Identification Data Block.\n");
	}
	if (base.serial_number) {
		unsigned sn = base.serial_number;

		if (hide_serial_numbers)
			printf("    Serial Number: ...\n");
		else
			printf("    Serial Number: %u (0x%08x)\n", sn, sn);

		// This is a list of known dummy values that are often used in EDIDs:
		switch (sn) {
		case 1:
		case 0x01010101:
		case 1010101:
		case 0x5445:
		case 0x80000000:
		case 20000080:
		case 8888:
		case 6666:
			warn("The serial number is one of the known dummy values, it should probably be set to 0.\n");
			break;
		}
	}

	time(&the_time);
	ptm = localtime(&the_time);

	unsigned char week = base.week;
	int year = 1990 + base.year;

	if (week) {
		if (base.edid_minor <= 3 && week == 0xff)
			fail("EDID 1.3 does not support week 0xff.\n");
		// The max week is 53 in EDID 1.3 and 54 in EDID 1.4.
		// No idea why there is a difference.
		if (base.edid_minor <= 3 && week == 54)
			fail("EDID 1.3 does not support week 54.\n");
		if (week != 0xff && week > 54)
			fail("Invalid week of manufacture (> 54).\n");
	}
	if (year - 1 > ptm->tm_year + 1900)
		fail("The year is more than one year in the future.\n");

	if (week == 0xff)
		printf("    Model year: %d\n", year);
	else if (replace_unique_ids)
		printf("    Made in: 2000\n");
	else if (week)
		printf("    Made in: week %hhu of %d\n", week, year);
	else
		printf("    Made in: %d\n", year);

	/* display section */

	data_block = "Basic Display Parameters & Features";
	printf("  %s:\n", data_block.c_str());
	if (x[0x14] & 0x80) {
		base.is_analog = false;
		printf("    Digital display\n");
		if (base.edid_minor >= 4) {
			if ((x[0x14] & 0x70) == 0x00)
				printf("    Color depth is undefined\n");
			else if ((x[0x14] & 0x70) == 0x70)
				fail("Color Bit Depth set to reserved value.\n");
			else
				printf("    Bits per primary color channel: %u\n",
				       ((x[0x14] & 0x70) >> 3) + 4);

			printf("    ");
			switch (x[0x14] & 0x0f) {
			case 0x00: printf("Digital interface is not defined\n"); break;
			case 0x01: printf("DVI interface\n"); break;
			case 0x02: printf("HDMI-a interface\n"); break;
			case 0x03: printf("HDMI-b interface\n"); break;
			case 0x04: printf("MDDI interface\n"); break;
			case 0x05: printf("DisplayPort interface\n"); break;
			default:
				   printf("Unknown interface: 0x%02x\n", x[0x14] & 0x0f);
				   fail("Digital Video Interface Standard set to reserved value 0x%02x.\n", x[0x14] & 0x0f);
				   break;
			}
		} else if (base.edid_minor >= 2) {
			if (x[0x14] & 0x01) {
				printf("    DFP 1.x compatible TMDS\n");
			}
			if (x[0x14] & 0x7e)
				fail("Digital Video Interface Standard set to reserved value 0x%02x.\n", x[0x14] & 0x7e);
		} else if (x[0x14] & 0x7f) {
			fail("Digital Video Interface Standard set to reserved value 0x%02x.\n", x[0x14] & 0x7f);
		}
	} else {
		static const char * const voltages[] = {
			"0.700 : 0.300 : 1.000 V p-p",
			"0.714 : 0.286 : 1.000 V p-p",
			"1.000 : 0.400 : 1.400 V p-p",
			"0.700 : 0.000 : 0.700 V p-p"
		};
		unsigned voltage = (x[0x14] & 0x60) >> 5;
		unsigned sync = (x[0x14] & 0x0f);

		base.is_analog = true;
		printf("    Analog display\n");
		printf("    Signal Level Standard: %s\n", voltages[voltage]);

		if (x[0x14] & 0x10)
			printf("    Blank-to-black setup/pedestal\n");
		else
			printf("    Blank level equals black level\n");

		if (sync)
			printf("    Sync:%s%s%s%s\n",
			       sync & 0x08 ? " Separate" : "",
			       sync & 0x04 ? " Composite" : "",
			       sync & 0x02 ? " SyncOnGreen" : "",
			       sync & 0x01 ? " Serration" : "");
	}

	if (x[0x15] && x[0x16]) {
		unsigned factor = cta.preparsed_image_size == hdmi_image_size_5cm ? 5 : 1;

		printf("    Maximum image size: %u cm x %u cm%s\n",
		       x[0x15] * factor, x[0x16] * factor,
		       factor == 5 ? " (HDMI VSDB indicates 5 cm units)" : "");
		base.max_display_width_mm = x[0x15] * 10 * factor;
		base.max_display_height_mm = x[0x16] * 10 * factor;
		image_width = base.max_display_width_mm * 10;
		image_height = base.max_display_height_mm * 10;
		if (x[0x15] < 10 || x[0x16] < 10)
			warn("Dubious maximum image size (%ux%u is smaller than %ux%u cm).\n",
			     x[0x15] * factor, x[0x16] * factor,
			     10 * factor, 10 * factor);
	}
	else if (base.edid_minor >= 4 && (x[0x15] || x[0x16])) {
		if (x[0x15])
			printf("    Aspect ratio: %.2f (landscape)\n", (x[0x15] + 99) / 100.0);
		else
			printf("    Aspect ratio: %.2f (portrait)\n", 100.0 / (x[0x16] + 99));
	} else {
		/* Either or both can be zero for 1.3 and before */
		printf("    Image size is variable\n");
	}

	if (x[0x17] == 0xff)
		printf("    Gamma is defined in an extension block\n");
	else
		printf("    Gamma: %.2f\n", (x[0x17] + 100.0) / 100.0);

	if (x[0x18] & 0xe0) {
		printf("    DPMS levels:");
		if (x[0x18] & 0x80) printf(" Standby");
		if (x[0x18] & 0x40) printf(" Suspend");
		if (x[0x18] & 0x20) printf(" Off");
		printf("\n");
	}

	if (base.is_analog || base.edid_minor < 4) {
		printf("    ");
		switch (x[0x18] & 0x18) {
		case 0x00: printf("Monochrome or grayscale display\n"); break;
		case 0x08: printf("RGB color display\n"); break;
		case 0x10: printf("Non-RGB color display\n"); break;
		case 0x18: printf("Undefined display color type\n"); break;
		}
	} else {
		printf("    Supported color formats: RGB 4:4:4");
		if (x[0x18] & 0x08)
			printf(", YCrCb 4:4:4");
		if (x[0x18] & 0x10)
			printf(", YCrCb 4:2:2");
		printf("\n");
	}

	if (x[0x18] & 0x04) {
		printf("    Default (sRGB) color space is primary color space\n");
		if (memcmp(x + 0x19, srgb_chromaticity, sizeof(srgb_chromaticity)))
			fail("sRGB is signaled, but the chromaticities do not match.\n");
		if (x[0x17] != 120)
			warn("sRGB is signaled, but the gamma != 2.2.\n");
		base.uses_srgb = true;
	} else if (!memcmp(x + 0x19, srgb_chromaticity, sizeof(srgb_chromaticity))) {
		fail("The chromaticities match sRGB, but sRGB is not signaled.\n");
		base.uses_srgb = true;
	}

	if (base.edid_minor >= 4) {
		/* 1.4 always has a preferred timing and this bit means something else. */
		has_preferred_timing = true;
		base.preferred_is_also_native = x[0x18] & 0x02;
		printf("    First detailed timing %s the native pixel format and preferred refresh rate\n",
		       base.preferred_is_also_native ? "includes" : "does not include");
	} else {
		if (x[0x18] & 0x02) {
			printf("    First detailed timing is the preferred timing\n");
			has_preferred_timing = true;
			// 1.3 recommends that the preferred timing corresponds to the
			// native timing, but it is not a requirement.
			// That said, we continue with the assumption that it actually
			// is the native timing.
			base.preferred_is_also_native = true;
		} else if (base.edid_minor == 3) {
			fail("EDID 1.3 requires that the first detailed timing is the preferred timing.\n");
		}
	}

	if (x[0x18] & 0x01) {
		if (base.edid_minor >= 4) {
			base.supports_continuous_freq = true;
			printf("    Display supports continuous frequencies\n");
		} else {
			printf("    Supports GTF timings within operating range\n");
			base.supports_gtf = true;
		}
	}

	data_block = "Color Characteristics";
	printf("  %s:\n", data_block.c_str());
	col_x = (x[0x1b] << 2) | (x[0x19] >> 6);
	col_y = (x[0x1c] << 2) | ((x[0x19] >> 4) & 3);
	printf("    Red  : 0.%04u, 0.%04u\n",
	       (col_x * 10000) / 1024, (col_y * 10000) / 1024);
	col_x = (x[0x1d] << 2) | ((x[0x19] >> 2) & 3);
	col_y = (x[0x1e] << 2) | (x[0x19] & 3);
	printf("    Green: 0.%04u, 0.%04u\n",
	       (col_x * 10000) / 1024, (col_y * 10000) / 1024);
	col_x = (x[0x1f] << 2) | (x[0x1a] >> 6);
	col_y = (x[0x20] << 2) | ((x[0x1a] >> 4) & 3);
	printf("    Blue : 0.%04u, 0.%04u\n",
	       (col_x * 10000) / 1024, (col_y * 10000) / 1024);
	col_x = (x[0x21] << 2) | ((x[0x1a] >> 2) & 3);
	col_y = (x[0x22] << 2) | (x[0x1a] & 3);
	printf("    White: 0.%04u, 0.%04u\n",
	       (col_x * 10000) / 1024, (col_y * 10000) / 1024);

	data_block = "Established Timings I & II";
	if (x[0x23] || x[0x24] || x[0x25]) {
		printf("  %s:\n", data_block.c_str());
		for (unsigned i = 0; i < ARRAY_SIZE(established_timings12); i++) {
			if (x[0x23 + i / 8] & (1 << (7 - i % 8))) {
				unsigned char dmt_id = established_timings12[i].dmt_id;
				const struct timings *t;
				char type[16];

				if (dmt_id) {
					sprintf(type, "DMT 0x%02x", dmt_id);
					t = find_dmt_id(dmt_id);
				} else {
					t = &established_timings12[i].t;
					sprintf(type, "%-8s", established_timings12[i].type);
				}
				print_timings("    ", t, type);
			}
		}
	} else {
		printf("  %s: none\n", data_block.c_str());
	}
	base.has_640x480p60_est_timing = x[0x23] & 0x20;

	data_block = "Standard Timings";
	bool found = false;
	for (unsigned i = 0; i < 8; i++) {
		if (x[0x26 + i * 2] != 0x01 || x[0x26 + i * 2 + 1] != 0x01) {
			found = true;
			break;
		}
	}
	if (found) {
		printf("  %s:\n", data_block.c_str());
		for (unsigned i = 0; i < 8; i++)
			print_standard_timing("    ", x[0x26 + i * 2], x[0x26 + i * 2 + 1]);
	} else {
		printf("  %s: none\n", data_block.c_str());
	}

	/* 18 byte descriptors */
	if (has_preferred_timing && !x[0x36] && !x[0x37])
		fail("Missing preferred timing.\n");

	/* Look for SPWG Noteboook Panel EDID data blocks */
	if ((x[0x36] || x[0x37]) &&
	    (x[0x48] || x[0x49]) &&
	    !x[0x5a] && !x[0x5b] && x[0x5d] == 0xfe &&
	    !x[0x6c] && !x[0x6d] && x[0x6f] == 0xfe &&
	    (x[0x79] == 1 || x[0x79] == 2) && x[0x7a] <= 1)
		base.has_spwg = true;

	for (unsigned i = 0; i < (base.has_spwg ? 2 : 4); i++)
		if (x[0x36 + i * 18] || x[0x37 + i * 18])
			cta.preparsed_total_dtds++;

	data_block = "Detailed Timing Descriptors";
	printf("  %s:\n", data_block.c_str());
	detailed_block(x + 0x36);
	detailed_block(x + 0x48);
	detailed_block(x + 0x5a);
	detailed_block(x + 0x6c);
	base.has_spwg = false;
	if (!has_preferred_timing) {
		base.preferred_timing = timings_ext();
	}

	data_block = block;
	if (x[0x7e])
		printf("  Extension blocks: %u\n", x[0x7e]);

	block = block_name(0x00);
	data_block.clear();
	do_checksum("", x, EDID_PAGE_SIZE, EDID_PAGE_SIZE - 1);
	if (base.edid_minor >= 3) {
		if (!base.has_name_descriptor)
			msg(base.edid_minor >= 4, "Missing Display Product Name.\n");
		if ((base.edid_minor == 3 || base.supports_continuous_freq) &&
		    !base.has_display_range_descriptor)
			fail("Missing Display Range Limits Descriptor.\n");
	}
}

void edid_state::check_base_block(const unsigned char *x)
{
	data_block = "Base EDID";

	/*
	 * Allow for regular rounding of vertical and horizontal frequencies.
	 * The spec says that the pixelclock shall be rounded up, so there is
	 * no need to take rounding into account.
	 */
	if (base.has_display_range_descriptor &&
	    (min_vert_freq_hz + 0.5 < base.min_display_vert_freq_hz ||
	     (max_vert_freq_hz >= base.max_display_vert_freq_hz + 0.5 && base.max_display_vert_freq_hz) ||
	     min_hor_freq_hz + 500 < base.min_display_hor_freq_hz ||
	     (max_hor_freq_hz >= base.max_display_hor_freq_hz + 500 && base.max_display_hor_freq_hz) ||
	     (max_pixclk_khz > base.max_display_pixclk_khz && base.max_display_pixclk_khz))) {
		/*
		 * Check if it is really out of range, or if it could be a rounding error.
		 * The EDID spec is not very clear about rounding.
		 */
		bool out_of_range =
			min_vert_freq_hz + 1.0 <= base.min_display_vert_freq_hz ||
			(max_vert_freq_hz >= base.max_display_vert_freq_hz + 1.0 && base.max_display_vert_freq_hz) ||
			min_hor_freq_hz + 1000 <= base.min_display_hor_freq_hz ||
			(max_hor_freq_hz >= base.max_display_hor_freq_hz + 1000 && base.max_display_hor_freq_hz) ||
			(max_pixclk_khz >= base.max_display_pixclk_khz + 10000 && base.max_display_pixclk_khz);

		std::string err("Some timings are out of range of the Monitor Ranges:\n");
		char buf[512];

		if (min_vert_freq_hz + 0.5 < base.min_display_vert_freq_hz ||
		    (max_vert_freq_hz >= base.max_display_vert_freq_hz + 0.5 && base.max_display_vert_freq_hz)) {
			sprintf(buf, "    Vertical Freq: %.3f - %.3f Hz (Monitor: %u.000 - %u.000 Hz)\n",
				min_vert_freq_hz, max_vert_freq_hz,
				base.min_display_vert_freq_hz, base.max_display_vert_freq_hz);
			err += buf;
		}

		if (min_hor_freq_hz + 500 < base.min_display_hor_freq_hz ||
		    (max_hor_freq_hz >= base.max_display_hor_freq_hz + 500 && base.max_display_hor_freq_hz)) {
			sprintf(buf, "    Horizontal Freq: %.3f - %.3f kHz (Monitor: %.3f - %.3f kHz)\n",
				min_hor_freq_hz / 1000.0, max_hor_freq_hz / 1000.0,
				base.min_display_hor_freq_hz / 1000.0, base.max_display_hor_freq_hz / 1000.0);
			err += buf;
		}

		if (max_pixclk_khz >= base.max_display_pixclk_khz && base.max_display_pixclk_khz) {
			sprintf(buf, "    Maximum Clock: %.3f MHz (Monitor: %.3f MHz)\n",
				max_pixclk_khz / 1000.0, base.max_display_pixclk_khz / 1000.0);
			err += buf;
		}

		if (!out_of_range)
			err += "    Could be due to a Monitor Range off-by-one rounding issue\n";

		warn("%s", err.c_str());
	}

	if ((image_width && dtd_max_hsize_mm >= 10 + image_width / 10) ||
	    (image_height && dtd_max_vsize_mm >= 10 + image_height / 10))
		fail("The DTD max image size is %ux%umm, which is larger than the display size %.1fx%.1fmm.\n",
		     dtd_max_hsize_mm, dtd_max_vsize_mm,
		     image_width / 10.0, image_height / 10.0);
	if ((!image_width && dtd_max_hsize_mm) || (!image_height && dtd_max_vsize_mm))
		fail("The DTD max image size is %ux%umm, but the display size is not specified anywhere.\n",
		     dtd_max_hsize_mm, dtd_max_vsize_mm);

	// Secondary GTF curves start at a specific frequency. Any legacy timings
	// that have a positive hsync and negative vsync must be less than that
	// frequency to avoid confusion.
	if (base.supports_sec_gtf && base.max_pos_neg_hor_freq_khz >= base.sec_gtf_start_freq)
		fail("Second GTF start frequency %u is less than the highest P/N frequency %u.\n",
		     base.sec_gtf_start_freq, base.max_pos_neg_hor_freq_khz);
	if (x[0x7e] + 1U != num_blocks && !cta.hf_eeodb_blocks)
		fail("EDID specified %u extension block(s), but found %u extension block(s).\n",
		     x[0x7e], num_blocks - 1);
	else if (x[0x7e] != 1 && cta.hf_eeodb_blocks)
		fail("HDMI Forum EDID Extension Override Data Block is present, but Block 0 defined %u instead of 1 Extension Blocks.\n",
		     x[0x7e]);
	else if (x[0x7e] == 1 && cta.hf_eeodb_blocks && cta.hf_eeodb_blocks + 1 != num_blocks)
		fail("HDMI Forum EDID Extension Override Data Block specified %u extension block(s), but found %u extension block(s).\n",
		     cta.hf_eeodb_blocks, num_blocks - 1);

	if (base.edid_minor == 3 && num_blocks > 2 && !block_map.saw_block_1 && !cta.hf_eeodb_blocks)
		fail("EDID 1.3 requires a Block Map Extension in Block 1 if there are more than 2 blocks in the EDID.\n");
	if (base.edid_minor == 3 && num_blocks > 128 && !block_map.saw_block_128 && !cta.hf_eeodb_blocks)
		fail("EDID 1.3 requires a Block Map Extension in Block 128 if there are more than 128 blocks in the EDID.\n");
	if (block_map.saw_block_128 && num_blocks > 255)
		fail("If there is a Block Map Extension in Block 128 then the maximum number of blocks is 255.\n");

	if (serial_strings.size() > 1)
		warn("Multiple Display Product Serial Numbers are specified, is that intended?\n");
}
