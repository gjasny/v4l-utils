// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2020 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil+cisco@kernel.org>
 */

#include <algorithm>
#include <stdio.h>
#include <math.h>

#include "edid-decode.h"

static const struct timings edid_cta_modes1[] = {
	/* VIC 1 */
	{  640,  480,   4,   3,   25175, 0, false,   16,  96,  48, false, 10,  2,  33, false },
	{  720,  480,   4,   3,   27000, 0, false,   16,  62,  60, false,  9,  6,  30, false },
	{  720,  480,  16,   9,   27000, 0, false,   16,  62,  60, false,  9,  6,  30, false },
	{ 1280,  720,  16,   9,   74250, 0, false,  110,  40, 220, true,   5,  5,  20, true  },
	{ 1920, 1080,  16,   9,   74250, 0, true,    88,  44, 148, true,   2,  5,  15, true  },
	{ 1440,  480,   4,   3,   27000, 0, true,    38, 124, 114, false,  4,  3,  15, false },
	{ 1440,  480,  16,   9,   27000, 0, true,    38, 124, 114, false,  4,  3,  15, false },
	{ 1440,  240,   4,   3,   27000, 0, false,   38, 124, 114, false,  4,  3,  15, false },
	{ 1440,  240,  16,   9,   27000, 0, false,   38, 124, 114, false,  4,  3,  15, false },
	{ 2880,  480,   4,   3,   54000, 0, true,    76, 248, 228, false,  4,  3,  15, false },
	/* VIC 11 */
	{ 2880,  480,  16,   9,   54000, 0, true,    76, 248, 228, false,  4,  3,  15, false },
	{ 2880,  240,   4,   3,   54000, 0, false,   76, 248, 228, false,  4,  3,  15, false },
	{ 2880,  240,  16,   9,   54000, 0, false,   76, 248, 228, false,  4,  3,  15, false },
	{ 1440,  480,   4,   3,   54000, 0, false,   32, 124, 120, false,  9,  6,  30, false },
	{ 1440,  480,  16,   9,   54000, 0, false,   32, 124, 120, false,  9,  6,  30, false },
	{ 1920, 1080,  16,   9,  148500, 0, false,   88,  44, 148, true,   4,  5,  36, true  },
	{  720,  576,   4,   3,   27000, 0, false,   12,  64,  68, false,  5,  5,  39, false },
	{  720,  576,  16,   9,   27000, 0, false,   12,  64,  68, false,  5,  5,  39, false },
	{ 1280,  720,  16,   9,   74250, 0, false,  440,  40, 220, true,   5,  5,  20, true  },
	{ 1920, 1080,  16,   9,   74250, 0, true,   528,  44, 148, true,   2,  5,  15, true  },
	/* VIC 21 */
	{ 1440,  576,   4,   3,   27000, 0, true,    24, 126, 138, false,  2,  3,  19, false },
	{ 1440,  576,  16,   9,   27000, 0, true,    24, 126, 138, false,  2,  3,  19, false },
	{ 1440,  288,   4,   3,   27000, 0, false,   24, 126, 138, false,  2,  3,  19, false },
	{ 1440,  288,  16,   9,   27000, 0, false,   24, 126, 138, false,  2,  3,  19, false },
	{ 2880,  576,   4,   3,   54000, 0, true,    48, 252, 276, false,  2,  3,  19, false },
	{ 2880,  576,  16,   9,   54000, 0, true,    48, 252, 276, false,  2,  3,  19, false },
	{ 2880,  288,   4,   3,   54000, 0, false,   48, 252, 276, false,  2,  3,  19, false },
	{ 2880,  288,  16,   9,   54000, 0, false,   48, 252, 276, false,  2,  3,  19, false },
	{ 1440,  576,   4,   3,   54000, 0, false,   24, 128, 136, false,  5,  5,  39, false },
	{ 1440,  576,  16,   9,   54000, 0, false,   24, 128, 136, false,  5,  5,  39, false },
	/* VIC 31 */
	{ 1920, 1080,  16,   9,  148500, 0, false,  528,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  16,   9,   74250, 0, false,  638,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  16,   9,   74250, 0, false,  528,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  16,   9,   74250, 0, false,   88,  44, 148, true,   4,  5,  36, true  },
	{ 2880,  480,   4,   3,  108000, 0, false,   64, 248, 240, false,  9,  6,  30, false },
	{ 2880,  480,  16,   9,  108000, 0, false,   64, 248, 240, false,  9,  6,  30, false },
	{ 2880,  576,   4,   3,  108000, 0, false,   48, 256, 272, false,  5,  5,  39, false },
	{ 2880,  576,  16,   9,  108000, 0, false,   48, 256, 272, false,  5,  5,  39, false },
	{ 1920, 1080,  16,   9,   72000, 0, true,    32, 168, 184, true,  23,  5,  57, false, 0, 0, true },
	{ 1920, 1080,  16,   9,  148500, 0, true,   528,  44, 148, true,   2,  5,  15, true  },
	/* VIC 41 */
	{ 1280,  720,  16,   9,  148500, 0, false,  440,  40, 220, true,   5,  5,  20, true  },
	{  720,  576,   4,   3,   54000, 0, false,   12,  64,  68, false,  5,  5,  39, false },
	{  720,  576,  16,   9,   54000, 0, false,   12,  64,  68, false,  5,  5,  39, false },
	{ 1440,  576,   4,   3,   54000, 0, true,    24, 126, 138, false,  2,  3,  19, false },
	{ 1440,  576,  16,   9,   54000, 0, true,    24, 126, 138, false,  2,  3,  19, false },
	{ 1920, 1080,  16,   9,  148500, 0, true,    88,  44, 148, true,   2,  5,  15, true  },
	{ 1280,  720,  16,   9,  148500, 0, false,  110,  40, 220, true,   5,  5,  20, true  },
	{  720,  480,   4,   3,   54000, 0, false,   16,  62,  60, false,  9,  6,  30, false },
	{  720,  480,  16,   9,   54000, 0, false,   16,  62,  60, false,  9,  6,  30, false },
	{ 1440,  480,   4,   3,   54000, 0, true,    38, 124, 114, false,  4,  3,  15, false },
	/* VIC 51 */
	{ 1440,  480,  16,   9,   54000, 0, true,    38, 124, 114, false,  4,  3,  15, false },
	{  720,  576,   4,   3,  108000, 0, false,   12,  64,  68, false,  5,  5,  39, false },
	{  720,  576,  16,   9,  108000, 0, false,   12,  64,  68, false,  5,  5,  39, false },
	{ 1440,  576,   4,   3,  108000, 0, true,    24, 126, 138, false,  2,  3,  19, false },
	{ 1440,  576,  16,   9,  108000, 0, true,    24, 126, 138, false,  2,  3,  19, false },
	{  720,  480,   4,   3,  108000, 0, false,   16,  62,  60, false,  9,  6,  30, false },
	{  720,  480,  16,   9,  108000, 0, false,   16,  62,  60, false,  9,  6,  30, false },
	{ 1440,  480,   4,   3,  108000, 0, true,    38, 124, 114, false,  4,  3,  15, false },
	{ 1440,  480,  16,   9,  108000, 0, true,    38, 124, 114, false,  4,  3,  15, false },
	{ 1280,  720,  16,   9,   59400, 0, false, 1760,  40, 220, true,   5,  5,  20, true  },
	/* VIC 61 */
	{ 1280,  720,  16,   9,   74250, 0, false, 2420,  40, 220, true,   5,  5,  20, true  },
	{ 1280,  720,  16,   9,   74250, 0, false, 1760,  40, 220, true,   5,  5,  20, true  },
	{ 1920, 1080,  16,   9,  297000, 0, false,   88,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  16,   9,  297000, 0, false,  528,  44, 148, true,   4,  5,  36, true  },
	{ 1280,  720,  64,  27,   59400, 0, false, 1760,  40, 220, true,   5,  5,  20, true  },
	{ 1280,  720,  64,  27,   74250, 0, false, 2420,  40, 220, true,   5,  5,  20, true  },
	{ 1280,  720,  64,  27,   74250, 0, false, 1760,  40, 220, true,   5,  5,  20, true  },
	{ 1280,  720,  64,  27,   74250, 0, false,  440,  40, 220, true,   5,  5,  20, true  },
	{ 1280,  720,  64,  27,   74250, 0, false,  110,  40, 220, true,   5,  5,  20, true  },
	{ 1280,  720,  64,  27,  148500, 0, false,  440,  40, 220, true,   5,  5,  20, true  },
	/* VIC 71 */
	{ 1280,  720,  64,  27,  148500, 0, false,  110,  40, 220, true,   5,  5,  20, true  },
	{ 1920, 1080,  64,  27,   74250, 0, false,  638,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  64,  27,   74250, 0, false,  528,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  64,  27,   74250, 0, false,   88,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  64,  27,  148500, 0, false,  528,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  64,  27,  148500, 0, false,   88,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  64,  27,  297000, 0, false,  528,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  64,  27,  297000, 0, false,   88,  44, 148, true,   4,  5,  36, true  },
	{ 1680,  720,  64,  27,   59400, 0, false, 1360,  40, 220, true,   5,  5,  20, true  },
	{ 1680,  720,  64,  27,   59400, 0, false, 1228,  40, 220, true,   5,  5,  20, true  },
	/* VIC 81 */
	{ 1680,  720,  64,  27,   59400, 0, false,  700,  40, 220, true,   5,  5,  20, true  },
	{ 1680,  720,  64,  27,   82500, 0, false,  260,  40, 220, true,   5,  5,  20, true  },
	{ 1680,  720,  64,  27,   99000, 0, false,  260,  40, 220, true,   5,  5,  20, true  },
	{ 1680,  720,  64,  27,  165000, 0, false,   60,  40, 220, true,   5,  5,  95, true  },
	{ 1680,  720,  64,  27,  198000, 0, false,   60,  40, 220, true,   5,  5,  95, true  },
	{ 2560, 1080,  64,  27,   99000, 0, false,  998,  44, 148, true,   4,  5,  11, true  },
	{ 2560, 1080,  64,  27,   90000, 0, false,  448,  44, 148, true,   4,  5,  36, true  },
	{ 2560, 1080,  64,  27,  118800, 0, false,  768,  44, 148, true,   4,  5,  36, true  },
	{ 2560, 1080,  64,  27,  185625, 0, false,  548,  44, 148, true,   4,  5,  36, true  },
	{ 2560, 1080,  64,  27,  198000, 0, false,  248,  44, 148, true,   4,  5,  11, true  },
	/* VIC 91 */
	{ 2560, 1080,  64,  27,  371250, 0, false,  218,  44, 148, true,   4,  5, 161, true  },
	{ 2560, 1080,  64,  27,  495000, 0, false,  548,  44, 148, true,   4,  5, 161, true  },
	{ 3840, 2160,  16,   9,  297000, 0, false, 1276,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  16,   9,  297000, 0, false, 1056,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  16,   9,  297000, 0, false,  176,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  16,   9,  594000, 0, false, 1056,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  16,   9,  594000, 0, false,  176,  88, 296, true,   8, 10,  72, true  },
	{ 4096, 2160, 256, 135,  297000, 0, false, 1020,  88, 296, true,   8, 10,  72, true  },
	{ 4096, 2160, 256, 135,  297000, 0, false,  968,  88, 128, true,   8, 10,  72, true  },
	{ 4096, 2160, 256, 135,  297000, 0, false,   88,  88, 128, true,   8, 10,  72, true  },
	/* VIC 101 */
	{ 4096, 2160, 256, 135,  594000, 0, false,  968,  88, 128, true,   8, 10,  72, true  },
	{ 4096, 2160, 256, 135,  594000, 0, false,   88,  88, 128, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27,  297000, 0, false, 1276,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27,  297000, 0, false, 1056,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27,  297000, 0, false,  176,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27,  594000, 0, false, 1056,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27,  594000, 0, false,  176,  88, 296, true,   8, 10,  72, true  },
	{ 1280,  720,  16,   9,   90000, 0, false,  960,  40, 220, true,   5,  5,  20, true  },
	{ 1280,  720,  64,  27,   90000, 0, false,  960,  40, 220, true,   5,  5,  20, true  },
	{ 1680,  720,  64,  27,   99000, 0, false,  810,  40, 220, true,   5,  5,  20, true  },
	/* VIC 111 */
	{ 1920, 1080,  16,   9,  148500, 0, false,  638,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  64,  27,  148500, 0, false,  638,  44, 148, true,   4,  5,  36, true  },
	{ 2560, 1080,  64,  27,  198000, 0, false,  998,  44, 148, true,   4,  5,  11, true  },
	{ 3840, 2160,  16,   9,  594000, 0, false, 1276,  88, 296, true,   8, 10,  72, true  },
	{ 4096, 2160, 256, 135,  594000, 0, false, 1020,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27,  594000, 0, false, 1276,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  16,   9, 1188000, 0, false, 1056,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  16,   9, 1188000, 0, false,  176,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27, 1188000, 0, false, 1056,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27, 1188000, 0, false,  176,  88, 296, true,   8, 10,  72, true  },
	/* VIC 121 */
	{ 5120, 2160,  64,  27,  396000, 0, false, 1996,  88, 296, true,   8, 10,  22, true  },
	{ 5120, 2160,  64,  27,  396000, 0, false, 1696,  88, 296, true,   8, 10,  22, true  },
	{ 5120, 2160,  64,  27,  396000, 0, false,  664,  88, 128, true,   8, 10,  22, true  },
	{ 5120, 2160,  64,  27,  742500, 0, false,  746,  88, 296, true,   8, 10, 297, true  },
	{ 5120, 2160,  64,  27,  742500, 0, false, 1096,  88, 296, true,   8, 10,  72, true  },
	{ 5120, 2160,  64,  27,  742500, 0, false,  164,  88, 128, true,   8, 10,  72, true  },
	{ 5120, 2160,  64,  27, 1485000, 0, false, 1096,  88, 296, true,   8, 10,  72, true  },
};

static const struct timings edid_cta_modes2[] = {
	/* VIC 193 */
	{  5120, 2160,  64,  27, 1485000, 0, false,  164,  88, 128, true,   8, 10,  72, true  },
	{  7680, 4320,  16,   9, 1188000, 0, false, 2552, 176, 592, true,  16, 20, 144, true  },
	{  7680, 4320,  16,   9, 1188000, 0, false, 2352, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  16,   9, 1188000, 0, false,  552, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  16,   9, 2376000, 0, false, 2552, 176, 592, true,  16, 20, 144, true  },
	{  7680, 4320,  16,   9, 2376000, 0, false, 2352, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  16,   9, 2376000, 0, false,  552, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  16,   9, 4752000, 0, false, 2112, 176, 592, true,  16, 20, 144, true  },
	/* VIC 201 */
	{  7680, 4320,  16,   9, 4752000, 0, false,  352, 176, 592, true,  16, 20, 144, true  },
	{  7680, 4320,  64,  27, 1188000, 0, false, 2552, 176, 592, true,  16, 20, 144, true  },
	{  7680, 4320,  64,  27, 1188000, 0, false, 2352, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  64,  27, 1188000, 0, false,  552, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  64,  27, 2376000, 0, false, 2552, 176, 592, true,  16, 20, 144, true  },
	{  7680, 4320,  64,  27, 2376000, 0, false, 2352, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  64,  27, 2376000, 0, false,  552, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  64,  27, 4752000, 0, false, 2112, 176, 592, true,  16, 20, 144, true  },
	{  7680, 4320,  64,  27, 4752000, 0, false,  352, 176, 592, true,  16, 20, 144, true  },
	{ 10240, 4320,  64,  27, 1485000, 0, false, 1492, 176, 592, true,  16, 20, 594, true  },
	/* VIC 211 */
	{ 10240, 4320,  64,  27, 1485000, 0, false, 2492, 176, 592, true,  16, 20,  44, true  },
	{ 10240, 4320,  64,  27, 1485000, 0, false,  288, 176, 296, true,  16, 20, 144, true  },
	{ 10240, 4320,  64,  27, 2970000, 0, false, 1492, 176, 592, true,  16, 20, 594, true  },
	{ 10240, 4320,  64,  27, 2970000, 0, false, 2492, 176, 592, true,  16, 20,  44, true  },
	{ 10240, 4320,  64,  27, 2970000, 0, false,  288, 176, 296, true,  16, 20, 144, true  },
	{ 10240, 4320,  64,  27, 5940000, 0, false, 2192, 176, 592, true,  16, 20, 144, true  },
	{ 10240, 4320,  64,  27, 5940000, 0, false,  288, 176, 296, true,  16, 20, 144, true  },
	{  4096, 2160, 256, 135, 1188000, 0, false,  800,  88, 296, true,   8, 10,  72, true  },
	{  4096, 2160, 256, 135, 1188000, 0, false,   88,  88, 128, true,   8, 10,  72, true  },
};

static const cta_rid rids[] = {
	/* RID 0-9 */
	{     0,    0,  0,  0 },
	{  1280,  720, 16,  9 },
	{  1280,  720, 64, 27 },
	{  1680,  720, 64, 27 },
	{  1920, 1080, 16,  9 },
	{  1920, 1080, 64, 27 },
	{  2560, 1080, 64, 27 },
	{  3840, 1080, 32,  9 },
	{  2560, 1440, 16,  9 },
	{  3440, 1440, 64, 27 },
	/* RID 10-19 */
	{  5120, 1440, 32,  9 },
	{  3840, 2160, 16,  9 },
	{  3840, 2160, 64, 27 },
	{  5120, 2160, 64, 27 },
	{  7680, 2160, 32,  9 },
	{  5120, 2880, 16,  9 },
	{  5120, 2880, 64, 27 },
	{  6880, 2880, 64, 27 },
	{ 10240, 2880, 32,  9 },
	{  7680, 4320, 16,  9 },
	/* RID 20-28 */
	{  7680, 4320, 64, 27 },
	{ 10240, 4320, 64, 27 },
	{ 15360, 4320, 32,  9 },
	{ 11520, 6480, 16,  9 },
	{ 11520, 6480, 64, 27 },
	{ 15360, 6480, 64, 27 },
	{ 15360, 8640, 16,  9 },
	{ 15360, 8640, 64, 27 },
	{ 20480, 8640, 64, 27 },
};

static const unsigned char rid2vic[ARRAY_SIZE(rids)][8] = {
	/* RID 0-9 */
	{},
	{  60,  61,  62, 108,  19,   4,  41,  47 },
	{  65,  66,  67, 109,  68,  69,  70,  71 },
	{  79,  80,  81, 110,  82,  83,  84,  85 },
	{  32,  33,  34, 111,  31,  16,  64,  63 },
	{  72,  73,  74, 112,  75,  76,  77,  78 },
	{  86,  87,  88, 113,  89,  90,  91,  92 },
	{},
	{},
	{},
	/* RID 10-19 */
	{},
	{  93,  94,  95, 114,  96,  97, 117, 118 },
	{ 103, 104, 105, 116, 106, 107, 119, 120 },
	{ 121, 122, 123, 124, 125, 126, 127, 193 },
	{},
	{},
	{},
	{},
	{},
	{ 194, 195, 196, 197, 198, 199, 200, 201 },
	/* RID 20-28 */
	{ 202, 203, 204, 205, 206, 207, 208, 209 },
	{ 210, 211, 212, 213, 214, 215, 216, 217 },
	{},
	{},
	{},
	{},
	{},
	{},
	{},
};

static const unsigned vf_rate_values[] = {
	/* Rate Index 0-7 */
	  0,  24,  25,  30,  48,  50,  60, 100,
	/* Rate Index 8-15 */
	120, 144, 200, 240, 300, 360, 400, 480,
};

static const unsigned char edid_hdmi_mode_map[] = { 95, 94, 93, 98 };

unsigned char hdmi_vic_to_vic(unsigned char hdmi_vic)
{
	if (hdmi_vic > 0 && hdmi_vic <= ARRAY_SIZE(edid_hdmi_mode_map))
		return edid_hdmi_mode_map[hdmi_vic - 1];
	return 0;
}

const struct timings *find_vic_id(unsigned char vic)
{
	if (vic > 0 && vic <= ARRAY_SIZE(edid_cta_modes1))
		return edid_cta_modes1 + vic - 1;
	if (vic >= 193 && vic < ARRAY_SIZE(edid_cta_modes2) + 193)
		return edid_cta_modes2 + vic - 193;
	return NULL;
}

const struct timings *find_hdmi_vic_id(unsigned char hdmi_vic)
{
	if (hdmi_vic > 0 && hdmi_vic <= ARRAY_SIZE(edid_hdmi_mode_map))
		return find_vic_id(edid_hdmi_mode_map[hdmi_vic - 1]);
	return NULL;
}

const struct cta_rid *find_rid(unsigned char rid)
{
	if (rid > 0 && rid < ARRAY_SIZE(rids))
		return &rids[rid];
	return NULL;
}

static unsigned char rid_to_vic(unsigned char rid, unsigned char rate_index)
{
	if (vf_rate_values[rate_index] > 120)
		return 0;
	return rid2vic[rid][rate_index - 1];
}

unsigned char rid_fps_to_vic(unsigned char rid, unsigned fps)
{
	for (unsigned i = 1; i < ARRAY_SIZE(vf_rate_values); i++) {
		if (vf_rate_values[i] == fps)
			return rid2vic[rid][i - 1];
	}
	return 0;
}

const struct timings *cta_close_match_to_vic(const timings &t, unsigned &vic)
{
	for (vic = 1; vic <= ARRAY_SIZE(edid_cta_modes1); vic++) {
		if (timings_close_match(t, edid_cta_modes1[vic - 1]))
			return &edid_cta_modes1[vic - 1];
	}
	for (vic = 193; vic < ARRAY_SIZE(edid_cta_modes2) + 193; vic++) {
		if (timings_close_match(t, edid_cta_modes1[vic - 193]))
			return &edid_cta_modes1[vic - 193];
	}
	vic = 0;
	return NULL;
}

bool cta_matches_vic(const timings &t, unsigned &vic)
{
	for (vic = 1; vic <= ARRAY_SIZE(edid_cta_modes1); vic++) {
		if (match_timings(t, edid_cta_modes1[vic - 1]))
			return true;
	}
	for (vic = 193; vic < ARRAY_SIZE(edid_cta_modes2) + 193; vic++) {
		if (match_timings(t, edid_cta_modes1[vic - 193]))
			return true;
	}
	vic = 0;
	return false;
}

void edid_state::cta_list_vics()
{
	char type[16];
	for (unsigned vic = 1; vic <= ARRAY_SIZE(edid_cta_modes1); vic++) {
		sprintf(type, "VIC %3u", vic);
		print_timings("", &edid_cta_modes1[vic - 1], type, "", false, false);
	}
	for (unsigned vic = 193; vic < ARRAY_SIZE(edid_cta_modes2) + 193; vic++) {
		sprintf(type, "VIC %3u", vic);
		print_timings("", &edid_cta_modes2[vic - 193], type, "", false, false);
	}
}

void edid_state::cta_list_hdmi_vics()
{
	for (unsigned i = 0; i < ARRAY_SIZE(edid_hdmi_mode_map); i++) {
		unsigned vic = edid_hdmi_mode_map[i];
		char type[16];

		sprintf(type, "HDMI VIC %u", i + 1);
		print_timings("", find_vic_id(vic), type, "", false, false);
	}
}

void edid_state::cta_list_rids()
{
	for (unsigned i = 1; i < ARRAY_SIZE(rids); i++) {
		printf("RID %2u: %5ux%-4u %2u:%-2u\n", i,
		       rids[i].hact, rids[i].vact,
		       rids[i].hratio, rids[i].vratio);
	}
}

void edid_state::cta_list_rid_timings(unsigned list_rid)
{
	for (unsigned rid = 1; rid < ARRAY_SIZE(rids); rid++) {
		char type[16];

		if (list_rid && rid != list_rid)
			continue;

		sprintf(type, "RID %u", rid);
		for (unsigned i = 1; i < ARRAY_SIZE(vf_rate_values); i++) {
			unsigned fps = vf_rate_values[i];

			if (rid_to_vic(rid, i)) {
				printf("%s: %5ux%-4u  %7.3f Hz %3u:%-2u maps to VIC %u\n", type,
				       rids[rid].hact, rids[rid].vact, (double)fps,
				       rids[rid].hratio, rids[rid].vratio,
				       rid_to_vic(rid, i));
				continue;
			}
			timings t = calc_ovt_mode(rids[rid].hact, rids[rid].vact,
						  rids[rid].hratio, rids[rid].vratio, fps);
			print_timings("", &t, type, "", false, false);
		}
	}
}

static std::string audio_ext_format(unsigned char x)
{
	if (x >= 1 && x <= 3)
		fail("Obsolete Audio Ext Format 0x%02x.\n", x);
	switch (x) {
	case 1: return "HE AAC (Obsolete)";
	case 2: return "HE AAC v2 (Obsolete)";
	case 3: return "MPEG Surround (Obsolete)";
	case 4: return "MPEG-4 HE AAC";
	case 5: return "MPEG-4 HE AAC v2";
	case 6: return "MPEG-4 AAC LC";
	case 7: return "DRA";
	case 8: return "MPEG-4 HE AAC + MPEG Surround";
	case 10: return "MPEG-4 AAC LC + MPEG Surround";
	case 11: return "MPEG-H 3D Audio";
	case 12: return "AC-4";
	case 13: return "L-PCM 3D Audio";
	case 14: return "Auro-Cx";
	case 15: return "MPEG-D USAC";
	default: break;
	}
	fail("Unknown Audio Ext Format 0x%02x.\n", x);
	return std::string("Unknown Audio Ext Format (") + utohex(x) + ")";
}

static std::string audio_format(unsigned char x)
{
	switch (x) {
	case 1: return "Linear PCM";
	case 2: return "AC-3";
	case 3: return "MPEG 1 (Layers 1 & 2)";
	case 4: return "MPEG 1 Layer 3 (MP3)";
	case 5: return "MPEG2 (multichannel)";
	case 6: return "AAC LC";
	case 7: return "DTS";
	case 8: return "ATRAC";
	case 9: return "One Bit Audio";
	case 10: return "Enhanced AC-3 (DD+)";
	case 11: return "DTS-HD";
	case 12: return "MAT (MLP)";
	case 13: return "DST";
	case 14: return "WMA Pro";
	default: break;
	}
	fail("Unknown Audio Format 0x%02x.\n", x);
	return std::string("Unknown Audio Format (") + utohex(x) + ")";
}

static std::string mpeg_h_3d_audio_level(unsigned char x)
{
	switch (x) {
	case 0: return "Unspecified";
	case 1: return "Level 1";
	case 2: return "Level 2";
	case 3: return "Level 3";
	case 4: return "Level 4";
	case 5: return "Level 5";
	default: break;
	}
	fail("Unknown MPEG-H 3D Audio Level 0x%02x.\n", x);
	return std::string("Unknown MPEG-H 3D Audio Level (") + utohex(x) + ")";
}

void edid_state::cta_audio_block(const unsigned char *x, unsigned length)
{
	unsigned i, format, ext_format;

	if (length % 3) {
		fail("Broken CTA-861 audio block length %d.\n", length);
		return;
	}
	if (!length)
		fail("This Data Block is empty.\n");

	for (i = 0; i < length; i += 3) {
		format = (x[i] & 0x78) >> 3;
		if (format == 0) {
			printf("    Reserved (0x00)\n");
			fail("Audio Format Code 0x00 is reserved.\n");
			continue;
		}
		if (format != 15) {
			ext_format = 0;
			printf("    %s:\n", audio_format(format).c_str());
		} else {
			ext_format = (x[i + 2] & 0xf8) >> 3;
			printf("    %s:\n", audio_ext_format(ext_format).c_str());
		}
		if (format != 15)
			printf("      Max channels: %u\n", (x[i] & 0x07)+1);
		else if (ext_format == 11)
			printf("      MPEG-H 3D Audio Level: %s\n",
			       mpeg_h_3d_audio_level(x[i] & 0x07).c_str());
		else if (ext_format == 13)
			printf("      Max channels: %u\n",
			       (((x[i + 1] & 0x80) >> 3) | ((x[i] & 0x80) >> 4) |
				(x[i] & 0x07))+1);
		else if ((ext_format == 12 || ext_format == 14) && (x[i] & 0x07))
			fail("Bits F10-F12 must be 0.\n");
		else
			printf("      Max channels: %u\n", (x[i] & 0x07)+1);

		if ((format == 1 || format == 14) && (x[i + 2] & 0xf8))
			fail("Bits F33-F37 must be 0.\n");
		if (ext_format != 13 && (x[i+1] & 0x80))
			fail("Bit F27 must be 0.\n");

		// Several sample rates are not supported in certain formats
		if (ext_format == 12 && (x[i+1] & 0x29))
			fail("Bits F20, F23 and F25 must be 0.\n");
		if (ext_format >= 4 && ext_format <= 6 && (x[i+1] & 0x60))
			fail("Bits F25 and F26 must be 0.\n");
		if ((ext_format == 8 || ext_format == 10 || ext_format == 15) && (x[i+1] & 0x60))
			fail("Bits F25 and F26 must be 0.\n");

		printf("      Supported sample rates (kHz):%s%s%s%s%s%s%s\n",
		       (x[i+1] & 0x40) ? " 192" : "",
		       (x[i+1] & 0x20) ? " 176.4" : "",
		       (x[i+1] & 0x10) ? " 96" : "",
		       (x[i+1] & 0x08) ? " 88.2" : "",
		       (x[i+1] & 0x04) ? " 48" : "",
		       (x[i+1] & 0x02) ? " 44.1" : "",
		       (x[i+1] & 0x01) ? " 32" : "");
		if (format == 1 || ext_format == 13) {
			printf("      Supported sample sizes (bits):%s%s%s\n",
			       (x[i+2] & 0x04) ? " 24" : "",
			       (x[i+2] & 0x02) ? " 20" : "",
			       (x[i+2] & 0x01) ? " 16" : "");
		} else if (format <= 8) {
			printf("      Maximum bit rate: %u kb/s\n", x[i+2] * 8);
		} else if (format == 10) {
			// As specified by the "Dolby Audio and Dolby Atmos over HDMI"
			// specification (v1.0).
			if (x[i+2] & 1)
				printf("      Supports Joint Object Coding\n");
			if (x[i+2] & 2)
				printf("      Supports Joint Object Coding with ACMOD28\n");
		} else if (format == 11) {
			// Reverse engineering, see:
			// https://www.avsforum.com/threads/lg-c9-earc-info-thread.3072900/post-61795538
			if (x[i+2] & 2)
				printf("      Supports DTS:X\n");
			// Note: I strongly suspect that bit 0 indicates DTS-HD MA support.
			printf("      Audio Format Code dependent value: 0x%02x\n", x[i+2]);
		} else if (format == 12) {
			if (x[i+2] & 1) {
				printf("      Supports Dolby TrueHD, object audio PCM and channel-based PCM\n");
				printf("      Hash calculation %srequired for object audio PCM or channel-based PCM\n",
				       (x[i+2] & 2) ? "not " : "");
			} else {
				printf("      Supports only Dolby TrueHD\n");
			}
		} else if (format == 14) {
			printf("      Profile: %u\n", x[i+2] & 7);
		} else if (format >= 9 && format <= 13) {
			printf("      Audio Format Code dependent value: 0x%02x\n", x[i+2]);
		} else if (ext_format == 11 && (x[i+2] & 1)) {
			printf("      Supports MPEG-H 3D Audio Low Complexity Profile\n");
		} else if ((ext_format >= 4 && ext_format <= 6) ||
			   ext_format == 8 || ext_format == 10) {
			printf("      AAC audio frame lengths:%s%s\n",
			       (x[i+2] & 4) ? " 1024_TL" : "",
			       (x[i+2] & 2) ? " 960_TL" : "");
			if (ext_format >= 8 && (x[i+2] & 1))
				printf("      Supports %s signaled MPEG Surround data\n",
				       (x[i+2] & 1) ? "implicitly and explicitly" : "only implicitly");
			if (ext_format == 6 && (x[i+2] & 1))
				printf("      Supports 22.2ch System H\n");
		} else if (ext_format == 12 || ext_format == 14) {
			printf("      Audio Format Code dependent value: %u\n", x[i+2] & 7);
		}
	}
}

void edid_state::cta_svd(const unsigned char *x, unsigned n, bool for_ycbcr420)
{
	bool ascending = !for_ycbcr420;
	unsigned char last_vic = 0;
	bool first_vic_is_1_to_4 = false;
	bool have_vics_5_and_up = false;
	unsigned i;

	if (!n)
		fail("This Data Block is empty.\n");
	for (i = 0; i < n; i++)  {
		const struct timings *t = NULL;
		unsigned char svd = x[i];
		unsigned char native;
		unsigned char vic;

		if ((svd & 0x7f) == 0)
			continue;

		if ((svd - 1) & 0x40) {
			vic = svd;
			native = 0;
			if (cta.avi_version == 2)
				cta.avi_version = 3;
		} else {
			vic = svd & 0x7f;
			native = svd & 0x80;
		}

		if (i == 0)
			first_vic_is_1_to_4 = vic <= 4;
		if (vic > 4)
			have_vics_5_and_up = true;
		if (vic < last_vic)
			ascending = false;
		last_vic = vic;

		t = find_vic_id(vic);
		if (t) {
			bool first_svd = cta.first_svd && !for_ycbcr420;

			char type[16];
			sprintf(type, "VIC %3u", vic);
			const char *flags = native ? "native" : "";

			if (for_ycbcr420) {
				struct timings tmp = *t;
				tmp.ycbcr420 = true;
				print_timings("    ", &tmp, type, flags);
				cta.has_ycbcr420 = true;
			} else {
				print_timings("    ", t, type, flags);
			}
			if (first_svd && !cta.preferred_timings.empty()) {
				if (!match_timings(cta.preferred_timings[0].t, *t)) {
					// The DTD can't handle width or height >= 4096. If the first
					// VIC has a width or height >= 4096, then adding VFPDB and
					// NVRDB data blocks is strongly recommended.
					if (t->vact < 4096 && t->hact < 4096)
						warn("VIC %u and the first DTD are not identical. Is this intended?\n", vic);
					else if (!cta.has_vfpdb)
						warn("The first VIC %u has width or height >= 4096, adding a VFPDB and NVRDB is strongly recommended.\n", vic);
				} else if (cta.first_svd_might_be_preferred) {
					warn("For improved preferred timing interoperability, set 'Native detailed modes' to 1.\n");
				}
			}
			if (first_svd) {
				if (cta.first_svd_might_be_preferred)
					cta.preferred_timings.insert(cta.preferred_timings.begin(),
								     timings_ext(*t, type, flags));
				else
					cta.preferred_timings.push_back(timings_ext(*t, type, flags));
			}
			if (first_svd) {
				cta.first_svd = false;
				cta.first_svd_might_be_preferred = false;
			}
			if (native)
				cta.native_timings.push_back(timings_ext(*t, type, flags));
		} else {
			printf("    Unknown (VIC %3u)\n", vic);
			fail("Unknown VIC %u.\n", vic);
		}

		if (vic == 1 && !for_ycbcr420)
			cta.has_vic_1 = 1;
		if (++cta.vics[vic][for_ycbcr420] == 2)
			fail("Duplicate %sVIC %u.\n", for_ycbcr420 ? "YCbCr 4:2:0 " : "", vic);
		if (for_ycbcr420 && cta.preparsed_has_vic[0][vic])
			fail("YCbCr 4:2:0-only VIC %u is also a regular VIC.\n", vic);
	}
	if (n > 1 && ascending && first_vic_is_1_to_4 && have_vics_5_and_up)
		warn("All VICs are in ascending order, and the first (preferred) VIC <= 4, is that intended?\n");
}

cta_vfd edid_state::cta_parse_vfd(const unsigned char *x, unsigned lvfd)
{
	cta_vfd vfd = {};

	cta.avi_version = 4;
	if (cta.avi_v4_length < 15)
		cta.avi_v4_length = 15;
	vfd.rid = x[0] & 0x3f;
	if (vfd.rid >= ARRAY_SIZE(rids)) {
		vfd.rid = 0;
		return vfd;
	}
	vfd.bfr50 = !!(x[0] & 0x80);
	vfd.fr24 = !!(x[0] & 0x40);
	vfd.bfr60 = lvfd > 1 ? !!(x[1] & 0x80) : 1;
	vfd.fr144 = lvfd > 1 ? !!(x[1] & 0x40) : 0;
	vfd.fr_factor = lvfd > 1 ? (x[1] & 0x3f) : 3;
	vfd.fr48 = lvfd > 2 ? !!(x[2] & 0x01) : 0;
	return vfd;
}

static bool vfd_has_rate(cta_vfd &vfd, unsigned rate_index)
{
	static const unsigned factors[6] = {
		1, 2, 4, 8, 12, 16
	};
	unsigned rate = vf_rate_values[rate_index];
	unsigned factor = 0;

	if (!vfd.rid)
		return false;
	if (rate == 24)
		return vfd.fr24;
	if (rate == 48)
		return vfd.fr48;
	if (rate == 144)
		return vfd.fr144;

	if (!(rate % 30)) {
		if (!vfd.bfr60)
			return false;
		factor = rate / 30;
	}
	if (!(rate % 25)) {
		if (!vfd.bfr50)
			return false;
		factor = rate / 25;
	}

	for (unsigned i = 0; i < ARRAY_SIZE(factors); i++)
		if (factors[i] == factor && (vfd.fr_factor & (1 << i)))
			return true;
	return false;
}

void edid_state::cta_vfdb(const unsigned char *x, unsigned n)
{
	if (n-- == 0) {
		fail("Length is 0.\n");
		return;
	}
	unsigned char flags = *x++;
	unsigned lvfd = (flags & 3) + 1;

	if (n % lvfd) {
		fail("Length - 1 is not a multiple of Lvfd (%u).\n", lvfd);
		return;
	}
	if (flags & 0x80)
		printf("    Supports YCbCr 4:2:0\n");
	if (flags & 0x40)
		printf("    NTSC fractional frame rates are preferred\n");
	for (unsigned i = 0; i < n; i += lvfd, x += lvfd)  {
		unsigned char rid = x[0] & 0x3f;
		cta_vfd vfd = cta_parse_vfd(x, lvfd);

		if (lvfd > 2 && (x[2] & 0xfe))
			fail("Bits F31-F37 must be 0.\n");
		if (lvfd > 3 && x[3])
			fail("Bits F40-F47 must be 0.\n");
		if (rid == 0 || rid >= ARRAY_SIZE(rids)) {
			fail("Unknown RID %u.\n", rid);
			continue;
		}
		for (unsigned rate_index = 1; rate_index < ARRAY_SIZE(vf_rate_values); rate_index++) {
			if (!vfd_has_rate(vfd, rate_index))
				continue;
			struct timings t = calc_ovt_mode(rids[vfd.rid].hact,
							 rids[vfd.rid].vact,
							 rids[vfd.rid].hratio,
							 rids[vfd.rid].vratio,
							 vf_rate_values[rate_index]);
			char type[16];
			sprintf(type, "RID %u@%up", rid, vf_rate_values[rate_index]);
			print_timings("    ", &t, type);
			if (rid_to_vic(vfd.rid, rate_index))
				fail("%s not allowed since it maps to VIC %u.\n",
				     type, rid_to_vic(vfd.rid, rate_index));
		}
	}
}

void edid_state::print_vic_index(const char *prefix, unsigned idx, const char *suffix, bool ycbcr420)
{
	if (!suffix)
		suffix = "";
	if (idx < cta.preparsed_svds[0].size()) {
		unsigned char vic = cta.preparsed_svds[0][idx];
		const struct timings *t = find_vic_id(vic);
		char buf[16];

		sprintf(buf, "VIC %3u", vic);

		if (t) {
			struct timings tmp = *t;
			tmp.ycbcr420 = ycbcr420;
			print_timings(prefix, &tmp, buf, suffix);
		} else {
			printf("%sUnknown (%s%s%s)\n", prefix, buf,
			       *suffix ? ", " : "", suffix);
		}
	} else {
		// Should not happen!
		printf("%sSVD Index %u is out of range", prefix, idx + 1);
		if (*suffix)
			printf(" (%s)", suffix);
		printf("\n");
	}
}

void edid_state::cta_y420cmdb(const unsigned char *x, unsigned length)
{
	unsigned max_idx = 0;
	unsigned i;

	if (!length) {
		printf("    All VDB SVDs\n");
		return;
	}

	if (memchk(x, length)) {
		printf("    Empty Capability Map\n");
		fail("Empty Capability Map.\n");
		return;
	}

	for (i = 0; i < length; i++) {
		unsigned char v = x[i];
		unsigned j;

		for (j = 0; j < 8; j++) {
			if (!(v & (1 << j)))
				continue;

			print_vic_index("    ", i * 8 + j, "", true);
			max_idx = i * 8 + j;
			if (max_idx < cta.preparsed_svds[0].size()) {
				unsigned vic = cta.preparsed_svds[0][max_idx];
				if (cta.preparsed_has_vic[1][vic])
					fail("VIC %u is also a YCbCr 4:2:0-only VIC.\n", vic);
			}
			cta.has_ycbcr420 = true;
		}
	}
	if (max_idx >= cta.preparsed_svds[0].size())
		fail("Max index %u > %u (#SVDs).\n",
		     max_idx + 1, cta.preparsed_svds[0].size());
}

void edid_state::cta_print_svr(unsigned char svr, vec_timings_ext &vec_tim)
{
	char suffix[24];

	if ((svr > 0 && svr < 128) || (svr > 192 && svr < 254)) {
		const struct timings *t;
		unsigned char vic = svr;

		sprintf(suffix, "VIC %3u", vic);

		t = find_vic_id(vic);
		if (t) {
			print_timings("    ", t, suffix);
			vec_tim.push_back(timings_ext(*t, suffix, ""));
		} else {
			printf("    %s: Unknown\n", suffix);
			fail("Unknown VIC %u.\n", vic);
		}

	} else if (svr >= 129 && svr <= 144) {
		sprintf(suffix, "DTD %3u", svr - 128);
		if (svr >= cta.preparsed_total_dtds + 129) {
			printf("    %s: Invalid\n", suffix);
			fail("Invalid DTD %u.\n", svr - 128);
		} else {
			printf("    %s\n", suffix);
			vec_tim.push_back(timings_ext(svr, suffix));
			cta.has_svrs = true;
		}
	} else if (svr >= 145 && svr <= 160) {
		sprintf(suffix, "VTDB %3u", svr - 144);
		if (svr >= cta.preparsed_total_vtdbs + 145) {
			printf("    %s: Invalid\n", suffix);
			fail("Invalid VTDB %u.\n", svr - 144);
		} else {
			printf("    %s\n", suffix);
			vec_tim.push_back(timings_ext(svr, suffix));
			cta.has_svrs = true;
		}
	} else if (svr >= 161 && svr <= 175) {
		sprintf(suffix, "RID %u@%up",
			cta.preparsed_first_vfd.rid, vf_rate_values[svr - 160]);
		if (!vfd_has_rate(cta.preparsed_first_vfd, svr - 160)) {
			printf("    %s: Invalid\n", suffix);
			fail("Invalid %s.\n", suffix);
		} else {
			printf("    %s\n", suffix);
			vec_tim.push_back(timings_ext(svr, suffix));
			cta.has_svrs = true;
		}
	} else if (svr == 254) {
		sprintf(suffix, "T8VTDB");
		if (!cta.preparsed_has_t8vtdb) {
			printf("    %s: Invalid\n", suffix);
			fail("Invalid T8VTDB.\n");
		} else {
			sprintf(suffix, "DMT 0x%02x", cta.preparsed_t8vtdb_dmt);
			printf("    %s\n", suffix);
			vec_tim.push_back(timings_ext(svr, suffix));
			cta.has_svrs = true;
		}
	}
}

void edid_state::cta_vfpdb(const unsigned char *x, unsigned length)
{
	unsigned i;

	if (length == 0) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}
	cta.preferred_timings_vfpdb.clear();
	for (i = 0; i < length; i++)
		cta_print_svr(x[i], cta.preferred_timings_vfpdb);
	if (cta.has_nvrdb)
		return;

	for (const auto &n : cta.preferred_timings_vfpdb) {
		if (n.t.vact >= 4096 || n.t.hact >= 4096) {
			warn("One of the VFPDB timings has width or height >= 4096, adding an NVRDB is strongly recommended.\n");
			break;
		}
	}
}

void edid_state::cta_nvrdb(const unsigned char *x, unsigned length)
{
	if (length == 0) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}

	unsigned char flags = length == 1 ? 0 : x[1];

	cta.native_timing_nvrdb.clear();
	cta_print_svr(x[0], cta.native_timing_nvrdb);
	if ((flags & 1) && length < 6) {
		fail("Data Block too short for Image Size (length = %u).\n", length);
		return;
	}
	if (flags & 0x7e)
		fail("Bits F41-F46 must be 0.\n");
	if (!(flags & 1))
		return;

	unsigned w = (x[3] << 8) | x[2];
	unsigned h = (x[5] << 8) | x[4];

	if (!w || !h) {
		fail("Image Size has a zero width and/or height.\n");
		return;
	}

	if (flags & 0x80) {
		w *= 10;
		h *= 10;
	}
	printf("    Image Size: %.1fx%.1f mm\n", w / 10.0, h / 10.0);
	image_width = w;
	image_height = h;
	if (w <= 25500 && h <= 25500)
		warn("Image Size should only be used for large displays with width and/or height > 255 cm\n");
	cta.nvrdb_has_size = true;
}

static std::string hdmi_latency2s(unsigned char l, bool is_video)
{
	if (!l)
		return "Unknown";
	if (l == 0xff)
		return is_video ? "Video not supported" : "Audio not supported";
	return std::to_string(2 * (l - 1)) + " ms";
}

void edid_state::hdmi_latency(unsigned char vid_lat, unsigned char aud_lat,
			      bool is_ilaced)
{
	const char *vid = is_ilaced ? "Interlaced video" : "Video";
	const char *aud = is_ilaced ? "Interlaced audio" : "Audio";

	printf("    %s latency: %s\n", vid, hdmi_latency2s(vid_lat, true).c_str());
	printf("    %s latency: %s\n", aud, hdmi_latency2s(aud_lat, false).c_str());

	if (vid_lat > 251 && vid_lat != 0xff)
		fail("Invalid %s latency value %u.\n", vid, vid_lat);
	if (aud_lat > 251 && aud_lat != 0xff)
		fail("Invalid %s latency value %u.\n", aud, aud_lat);

	if (!vid_lat || vid_lat > 251)
		return;
	if (!aud_lat || aud_lat > 251)
		return;

	unsigned vid_ms = 2 * (vid_lat - 1);
	unsigned aud_ms = 2 * (aud_lat - 1);

	// HDMI 2.0 latency checks for devices without HDMI output
	if (aud_ms < vid_ms)
		warn("%s latency < %s latency (%u ms < %u ms). This is discouraged for devices without HDMI output.\n",
		     aud, vid, aud_ms, vid_ms);
	else if (vid_ms + 20 < aud_ms)
		warn("%s latency + 20 < %s latency (%u + 20 ms < %u ms). This is forbidden for devices without HDMI output.\n",
		     vid, aud, vid_ms, aud_ms);
	else if (vid_ms < aud_ms)
		warn("%s latency < %s latency (%u ms < %u ms). This is discouraged for devices without HDMI output.\n",
		     vid, aud, vid_ms, aud_ms);
}

void edid_state::cta_hdmi_block(const unsigned char *x, unsigned length)
{
	unsigned len_vic, len_3d;

	if (length < 1) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}
	printf("    Source physical address: %x.%x.%x.%x\n", x[0] >> 4, x[0] & 0x0f,
	       x[1] >> 4, x[1] & 0x0f);

	if (length < 3)
		return;

	if (x[2] & 0x80)
		printf("    Supports_AI\n");
	if (x[2] & 0x40)
		printf("    DC_48bit\n");
	if (x[2] & 0x20)
		printf("    DC_36bit\n");
	if (x[2] & 0x10)
		printf("    DC_30bit\n");
	if (x[2] & 0x08)
		printf("    DC_Y444\n");
	/* two reserved bits */
	if (x[2] & 0x01)
		printf("    DVI_Dual\n");

	if (length < 4)
		return;

	unsigned rate = x[3] * 5;
	printf("    Maximum TMDS clock: %u MHz\n", rate);
	cta.hdmi_max_rate = rate;
	if (rate > 340)
		fail("HDMI VSDB Max TMDS rate is > 340.\n");

	if (length < 5)
		return;

	if (x[4] & 0x0f) {
		printf("    Supported Content Types:\n");
		if (x[4] & 0x01)
			printf("      Graphics\n");
		if (x[4] & 0x02)
			printf("      Photo\n");
		if (x[4] & 0x04)
			printf("      Cinema\n");
		if (x[4] & 0x08)
			printf("      Game\n");
	}

	unsigned b = 5;
	if (x[4] & 0x80) {
		hdmi_latency(x[b], x[b + 1], false);

		if (x[4] & 0x40) {
			if (x[b] == x[b + 2] &&
			    x[b + 1] == x[b + 3])
				warn("Progressive and Interlaced latency values are identical, no need for both.\n");
			b += 2;
			hdmi_latency(x[b], x[b + 1], true);
		}
		b += 2;
	}

	if (!(x[4] & 0x20))
		return;

	bool mask = false;
	bool formats = false;

	printf("    Extended HDMI video details:\n");
	if (x[b] & 0x80)
		printf("      3D present\n");
	if ((x[b] & 0x60) == 0x20) {
		printf("      All advertised VICs are 3D-capable\n");
		formats = true;
	}
	if ((x[b] & 0x60) == 0x40) {
		printf("      3D-capable-VIC mask present\n");
		formats = true;
		mask = true;
	}
	switch (x[b] & 0x18) {
	case 0x00: break;
	case 0x08:
		   printf("      Base EDID image size is aspect ratio\n");
		   break;
	case 0x10:
		   printf("      Base EDID image size is in units of 1 cm\n");
		   break;
	case 0x18:
		   printf("      Base EDID image size is in units of 5 cm\n");
		   if (base.max_display_width_mm < 2550 &&
		       base.max_display_height_mm < 2550)
			   fail("5 cm units should not be used for displays smaller than 255x255 cm\n");
		   break;
	}
	b++;
	len_vic = (x[b] & 0xe0) >> 5;
	len_3d = (x[b] & 0x1f) >> 0;
	b++;

	if (len_vic) {
		unsigned i;

		printf("      HDMI VICs:\n");
		cta.hdmi_vic_codes = 0;
		for (i = 0; i < len_vic; i++) {
			unsigned char vic = x[b + i];
			const struct timings *t;

			if (vic && vic <= ARRAY_SIZE(edid_hdmi_mode_map)) {
				std::string suffix = "HDMI VIC " + std::to_string(vic);
				cta.hdmi_vic_codes |= 1 << (vic - 1);
				t = find_vic_id(edid_hdmi_mode_map[vic - 1]);
				print_timings("        ", t, suffix.c_str());
			} else {
				printf("         Unknown (HDMI VIC %u)\n", vic);
				fail("Unknown HDMI VIC %u.\n", vic);
			}
		}

		b += len_vic;

		if ((cta.preparsed_hdmi_vic_vsb_codes & cta.hdmi_vic_codes) != cta.hdmi_vic_codes)
			fail("HDMI VIC Codes must have their CTA-861 VIC equivalents in the VSB.\n");
	}

	if (!len_3d)
		return;

	if (formats) {
		/* 3D_Structure_ALL_15..8 */
		if (x[b] & 0x80)
			printf("      3D: Side-by-side (half, quincunx)\n");
		if (x[b] & 0x01)
			printf("      3D: Side-by-side (half, horizontal)\n");
		/* 3D_Structure_ALL_7..0 */
		b++;
		if (x[b] & 0x40)
			printf("      3D: Top-and-bottom\n");
		if (x[b] & 0x20)
			printf("      3D: L + depth + gfx + gfx-depth\n");
		if (x[b] & 0x10)
			printf("      3D: L + depth\n");
		if (x[b] & 0x08)
			printf("      3D: Side-by-side (full)\n");
		if (x[b] & 0x04)
			printf("      3D: Line-alternative\n");
		if (x[b] & 0x02)
			printf("      3D: Field-alternative\n");
		if (x[b] & 0x01)
			printf("      3D: Frame-packing\n");
		b++;
		len_3d -= 2;
	}

	if (mask) {
		int max_idx = -1;
		unsigned i;

		printf("      3D VIC indices that support these capabilities:\n");
		/* worst bit ordering ever */
		for (i = 0; i < 8; i++)
			if (x[b + 1] & (1 << i)) {
				print_vic_index("        ", i, "");
				max_idx = i;
			}
		for (i = 0; i < 8; i++)
			if (x[b] & (1 << i)) {
				print_vic_index("        ", i + 8, "");
				max_idx = i + 8;
			}
		b += 2;
		len_3d -= 2;
		if (max_idx >= (int)cta.preparsed_svds[0].size())
			fail("HDMI 3D VIC indices max index %d > %u (#SVDs).\n",
			     max_idx + 1, cta.preparsed_svds[0].size());
	}

	/*
	 * list of nibbles:
	 * 2D_VIC_Order_X
	 * 3D_Structure_X
	 * (optionally: 3D_Detail_X and reserved)
	 */
	if (!len_3d)
		return;

	unsigned end = b + len_3d;
	int max_idx = -1;

	printf("      3D VIC indices with specific capabilities:\n");
	while (b < end) {
		unsigned char idx = x[b] >> 4;
		std::string s;

		if (idx > max_idx)
			max_idx = idx;
		switch (x[b] & 0x0f) {
		case 0: s = "frame packing"; break;
		case 1: s = "field alternative"; break;
		case 2: s = "line alternative"; break;
		case 3: s = "side-by-side (full)"; break;
		case 4: s = "L + depth"; break;
		case 5: s = "L + depth + gfx + gfx-depth"; break;
		case 6: s = "top-and-bottom"; break;
		case 8:
			s = "side-by-side";
			switch (x[b + 1] >> 4) {
			case 0x00: s += ", any subsampling"; break;
			case 0x01: s += ", horizontal"; break;
			case 0x02: case 0x03: case 0x04: case 0x05:
				   s += ", not in use";
				   fail("not-in-use 3D_Detail_X value 0x%02x.\n",
					x[b + 1] >> 4);
				   break;
			case 0x06: s += ", all quincunx combinations"; break;
			case 0x07: s += ", quincunx odd/left, odd/right"; break;
			case 0x08: s += ", quincunx odd/left, even/right"; break;
			case 0x09: s += ", quincunx even/left, odd/right"; break;
			case 0x0a: s += ", quincunx even/left, even/right"; break;
			default:
				   s += ", reserved";
				   fail("reserved 3D_Detail_X value 0x%02x.\n",
					x[b + 1] >> 4);
				   break;
			}
			break;
		default:
			s = "unknown (";
			s += utohex(x[b] & 0x0f) + ")";
			fail("Unknown 3D_Structure_X value 0x%02x.\n", x[b] & 0x0f);
			break;
		}
		print_vic_index("        ", idx, s.c_str());
		if ((x[b] & 0x0f) >= 8)
			b++;
		b++;
	}
	if (max_idx >= (int)cta.preparsed_svds[0].size())
		fail("HDMI 2D VIC indices max index %d > %u (#SVDs).\n",
		     max_idx + 1, cta.preparsed_svds[0].size());
}

static const char *max_frl_rates[] = {
	"Not Supported",
	"3 Gbps per lane on 3 lanes",
	"3 and 6 Gbps per lane on 3 lanes",
	"3 and 6 Gbps per lane on 3 lanes, 6 Gbps on 4 lanes",
	"3 and 6 Gbps per lane on 3 lanes, 6 and 8 Gbps on 4 lanes",
	"3 and 6 Gbps per lane on 3 lanes, 6, 8 and 10 Gbps on 4 lanes",
	"3 and 6 Gbps per lane on 3 lanes, 6, 8, 10 and 12 Gbps on 4 lanes",
};

static const char *dsc_max_slices[] = {
	"Not Supported",
	"up to 1 slice and up to (340 MHz/Ksliceadjust) pixel clock per slice",
	"up to 2 slices and up to (340 MHz/Ksliceadjust) pixel clock per slice",
	"up to 4 slices and up to (340 MHz/Ksliceadjust) pixel clock per slice",
	"up to 8 slices and up to (340 MHz/Ksliceadjust) pixel clock per slice",
	"up to 8 slices and up to (400 MHz/Ksliceadjust) pixel clock per slice",
	"up to 12 slices and up to (400 MHz/Ksliceadjust) pixel clock per slice",
	"up to 12 slices and up to (600 MHz/Ksliceadjust) pixel clock per slice",
};

static void cta_hf_eeodb(const unsigned char *x, unsigned length)
{
	printf("    EDID Extension Block Count: %u\n", x[0]);
	if (length != 1)
		fail("Block is too long.\n");
	if (x[0] <= 1)
		fail("Extension Block Count == %u.\n", x[0]);
}

void edid_state::cta_hf_scdb(const unsigned char *x, unsigned length)
{
	unsigned rate = x[1] * 5;
	unsigned v;

	printf("    Version: %u\n", x[0]);
	if (rate) {
		printf("    Maximum TMDS Character Rate: %u MHz\n", rate);
		if (rate <= 340 || rate > 600)
			fail("Max TMDS rate is > 0 and <= 340 or > 600.\n");
		if (rate < cta.hdmi_max_rate)
			fail("HDMI Forum VSDB rate < HDMI VSDB rate.\n");
		else
			cta.hdmi_max_rate = rate;
	}
	if (x[2] & 0x80)
		printf("    SCDC Present\n");
	if (x[2] & 0x40)
		printf("    SCDC Read Request Capable\n");
	if (x[2] & 0x20)
		printf("    Supports Cable Status\n");
	if (x[2] & 0x10)
		printf("    Supports Color Content Bits Per Component Indication\n");
	if (x[2] & 0x08)
		printf("    Supports scrambling for <= 340 Mcsc\n");
	if (x[2] & 0x04)
		printf("    Supports 3D Independent View signaling\n");
	if (x[2] & 0x02)
		printf("    Supports 3D Dual View signaling\n");
	if (x[2] & 0x01)
		printf("    Supports 3D OSD Disparity signaling\n");
	if (x[3] & 0xf0) {
		unsigned max_frl_rate = x[3] >> 4;

		printf("    Max Fixed Rate Link: ");
		if (max_frl_rate < ARRAY_SIZE(max_frl_rates)) {
			printf("%s\n", max_frl_rates[max_frl_rate]);
		} else {
			printf("Unknown (0x%02x)\n", max_frl_rate);
			fail("Unknown Max Fixed Rate Link (0x%02x).\n", max_frl_rate);
		}
		if (max_frl_rate == 1 && rate < 300)
			fail("Max Fixed Rate Link is 1, but Max TMDS rate < 300.\n");
		else if (max_frl_rate >= 2 && rate < 600)
			fail("Max Fixed Rate Link is >= 2, but Max TMDS rate < 600.\n");

		// FIXME:
		// Currently I do not really know how to translate the
		// Max FRL value to an equivalent max clock frequency.
		// So reset this field to 0 to skip any clock rate checks.
		cta.hdmi_max_rate = 0;
	}
	if (x[3] & 0x08) {
		printf("    Supports UHD VIC\n");
		if (!(cta.preparsed_hdmi_vic_vsb_codes & cta.hdmi_vic_codes))
			fail("UHD VIC bit is 1, but no related VIC codes are present.\n");
	} else if (cta.preparsed_hdmi_vic_vsb_codes & cta.hdmi_vic_codes) {
		fail("HDMI VIC and related CTA VIC codes are present, but the UHD VIC bit is 0.\n");
	}
	if (x[3] & 0x04)
		printf("    Supports 16-bits/component Deep Color 4:2:0 Pixel Encoding\n");
	if (x[3] & 0x02)
		printf("    Supports 12-bits/component Deep Color 4:2:0 Pixel Encoding\n");
	if (x[3] & 0x01)
		printf("    Supports 10-bits/component Deep Color 4:2:0 Pixel Encoding\n");

	if (length <= 4)
		return;

	if (x[4] & 0x80)
		printf("    Supports FAPA End Extended\n");
	if (x[4] & 0x40)
		printf("    Supports QMS\n");
	if (x[4] & 0x20)
		printf("    Supports Mdelta\n");
	if (x[4] & 0x10) {
		printf("    Supports media rates below VRRmin (CinemaVRR, deprecated)\n");
		warn("CinemaVRR is deprecated and must be cleared.\n");
	}
	if (x[4] & 0x08)
		printf("    Supports negative Mvrr values\n");
	if (x[4] & 0x04)
		printf("    Supports Fast Vactive\n");
	if (x[4] & 0x02)
		printf("    Supports Auto Low-Latency Mode\n");
	if (x[4] & 0x01)
		printf("    Supports a FAPA in blanking after first active video line\n");

	if (length <= 5)
		return;

	v = x[5] & 0x3f;
	printf("    VRRmin: %u Hz\n", v);
	if (v > 48)
		fail("VRRmin > 48.\n");
	v = (x[5] & 0xc0) << 2 | x[6];
	printf("    VRRmax: %u Hz\n", v);
	if (v) {
		if (!(x[5] & 0x3f))
			fail("VRRmin == 0, but VRRmax isn't.\n");
		else if (v < 100)
			fail("0 < VRRmax < 100.\n");
	}

	if (length <= 7)
		return;

	if (x[7] & 0x80)
		printf("    Supports VESA DSC 1.2a compression\n");
	if (x[7] & 0x40)
		printf("    Supports Compressed Video Transport for 4:2:0 Pixel Encoding\n");
	if (x[7] & 0x20)
		printf("    Supports QMS TFRmax\n");
	if (x[7] & 0x10)
		printf("    Supports QMS TFRmin\n");
	if (x[7] & 0x08)
		printf("    Supports Compressed Video Transport at any valid 1/16th bit bpp\n");
	if (x[7] & 0x04)
		printf("    Supports 16 bpc Compressed Video Transport\n");
	if (x[7] & 0x02)
		printf("    Supports 12 bpc Compressed Video Transport\n");
	if (x[7] & 0x01)
		printf("    Supports 10 bpc Compressed Video Transport\n");
	if (x[8] & 0xf) {
		unsigned max_slices = x[8] & 0xf;

		printf("    DSC Max Slices: ");
		if (max_slices < ARRAY_SIZE(dsc_max_slices)) {
			printf("%s\n", dsc_max_slices[max_slices]);
		} else {
			printf("Unknown (%u), interpreted as: %s\n", max_slices,
			       dsc_max_slices[7]);
			warn("Unknown DSC Max Slices (%u).\n", max_slices);
		}
	}
	if (x[8] & 0xf0) {
		unsigned max_frl_rate = x[8] >> 4;

		printf("    DSC Max Fixed Rate Link: ");
		if (max_frl_rate < ARRAY_SIZE(max_frl_rates)) {
			printf("%s\n", max_frl_rates[max_frl_rate]);
		} else {
			printf("Unknown (0x%02x)\n", max_frl_rate);
			fail("Unknown DSC Max Fixed Rate Link (0x%02x).\n", max_frl_rate);
		}
	}
	if (x[9] & 0x3f)
		printf("    Maximum number of bytes in a line of chunks: %u\n",
		       1024 * (1 + (x[9] & 0x3f)));
}

// Convert a PQ value (0-1) to cd/m^2 aka nits (0-10000)
static double pq2nits(double pq)
{
	const double m1 = 2610.0 / 16384.0;
	const double m2 = 128.0 * (2523.0 / 4096.0);
	const double c1 = 3424.0 / 4096.0;
	const double c2 = 32.0 * (2413.0 / 4096.0);
	const double c3 = 32.0 * (2392.0 / 4096.0);
	double e = pow(pq, 1.0 / m2);
	double v = e - c1;

	if (v < 0)
		v = 0;
	v /= c2 - c3 * e;
	v = pow(v, 1.0 / m1);
	return v * 10000.0;
}

static double perc2d(unsigned char x)
{
	double m = x >> 2;
	double e = x & 3;

	return 100.0 * (m / 64.0) * pow(10, -e);
}

static void cta_hf_sbtmdb(const unsigned char *x, unsigned length)
{
	int len = length;

	if (!length)
		fail("Block is too short.\n");
	printf("    Version: %d\n", x[0] & 0xf);
	switch ((x[0] >> 5) & 3) {
	case 0:
		printf("    Does not support a General RDM format\n");
		break;
	case 1:
		printf("    Supports an SDR-range General RDM format\n");
		break;
	case 2:
		printf("    Supports an HDR-range General RDM format\n");
		break;
	default:
		fail("Invalid GRDM Support value.\n");
		break;
	}
	if (!(x[0] & 0x80))
		return;

	bool uses_hgig_drdm = true;

	printf("    Supports a D-RDM format\n");
	if (x[1] & 0x10)
		printf("    Use HGIG D-RDM\n");
	switch (x[1] & 7) {
	case 0:
		printf("    HGIG D-RDM is not used\n");
		uses_hgig_drdm = false;
		break;
	case 1:
		printf("    PBnits[0] = 600 cd/m^2\n");
		break;
	case 2:
		printf("    PBnits[0] = 1000 cd/m^2\n");
		break;
	case 3:
		printf("    PBnits[0] = 4000 cd/m^2\n");
		break;
	case 4:
		printf("    PBnits[0] = 10000 cd/m^2\n");
		break;
	default:
		fail("Invalid HGIG D-DRM value.\n");
		break;
	}

	bool has_chromaticities = false;

	if (x[1] & 0x20)
		printf("    MaxRGB\n");
	switch (x[1] >> 6) {
	case 0:
		printf("    Gamut is explicit\n");
		has_chromaticities = true;
		break;
	case 1:
		printf("    Gamut is Rec. ITU-R BT.709\n");
		break;
	case 2:
		printf("    Gamut is SMPTE ST 2113\n");
		break;
	default:
		printf("    Gamut is Rec. ITU-R BT.2020\n");
		break;
	}
	x += 2;
	len -= 2;
	if (has_chromaticities) {
		printf("    Red: (%.5f, %.5f)\n", chrom2d(x), chrom2d(x + 2));
		printf("    Green: (%.5f, %.5f)\n", chrom2d(x + 4), chrom2d(x + 6));
		printf("    Blue: (%.5f, %.5f)\n", chrom2d(x + 8), chrom2d(x + 10));
		printf("    White: (%.5f, %.5f)\n", chrom2d(x + 12), chrom2d(x + 14));
		x += 16;
		len -= 16;
	}
	if (uses_hgig_drdm)
		return;
	printf("    Min Brightness 10: %.8f cd/m^2\n", pq2nits((x[0] << 1) / 4095.0));
	printf("    Peak Brightness 100: %u cd/m^2\n", (unsigned)pq2nits((x[1] << 4) / 4095.0));
	x += 2;
	len -= 2;
	if (len <= 0)
		return;
	printf("    Percentage of Peak Brightness P0: %.2f%%\n", perc2d(x[0]));
	printf("    Peak Brightness P0: %.8f cd/m^2\n", pq2nits((x[1] << 1) / 4095.0));
	x += 2;
	len -= 2;
	if (len <= 0)
		return;
	printf("    Percentage of Peak Brightness P1: %.2f%%\n", perc2d(x[0]));
	printf("    Peak Brightness P1: %.8f cd/m^2\n", pq2nits((x[1] << 1) / 4095.0));
	x += 2;
	len -= 2;
	if (len <= 0)
		return;
	printf("    Percentage of Peak Brightness P2: %.2f%%\n", perc2d(x[0]));
	printf("    Peak Brightness P2: %.8f cd/m^2\n", pq2nits((x[1] << 1) / 4095.0));
	x += 2;
	len -= 2;
	if (len <= 0)
		return;
	printf("    Percentage of Peak Brightness P3: %.2f%%\n", perc2d(x[0]));
	printf("    Peak Brightness P3: %.8f cd/m^2\n", pq2nits((x[1] << 1) / 4095.0));
}

static void cta_amd(const unsigned char *x, unsigned length)
{
    // x[00]          - major version
    // x[01] & 0x01   - unknown, set in almost all EDIDs
    // x[01] & 0x02   - set if x[05..=09] are valid
    // x[01] & 0x04   - global backlight control support
    // x[01] & 0x08   - local dimming support
    // x[01] & 0x10   - FreeSync Panel Replay/PSR switch support
    // x[01] & 0x20   - called the SPRS bit by AMD, related to Replay
    // x[01] & 0x40   - FreeSync Panel Replay support
    // x[01] & 0x80   - set if x[12..=14] are valid
    // x[02]          - min refresh rate
    // x[03]          - max refresh rate in versions < 3
    // x[04]          - MCCS flags
    // -- start of version 2 fields
    // x[05] & 0x01   - unknown
    // x[05] & 0x02   - unknown
    // x[05] & 0x04   - PQ EOTF support
    // x[05] & 0x08   - unknown
    // x[05] & 0x10   - PQ-Interim EOTF support (unknown what that is but enumerated in ADL)
    // x[05] & 0x20   - unknown but see the calculation of supported_tf below
    // x[05] & 0xc0   - set to 1 if the display is Mini LED
    //                  set to 2 if the display is OLED
    // x[06]          - max luminance
    // x[07]          - min luminance
    // x[08]          -      if x[01] & 0x08 or display is OLED: max luminance without local dimming
    //                  else if x[01] & 0x04                   : max luminance at min backlight
    // x[09]          -      if x[01] & 0x08 or display is OLED: min luminance without local dimming
    //                  else if x[01] & 0x04                   : min luminance at min backlight
    // -- start of version 3 fields
    // x[10]          - max refresh rate lower 8 bits in version >= 3
    // x[11] & 0x03   - max refresh rate upper 2 bits in version >= 3
    // x[11] & 0xfc   - unused
    // -- end of version 3 mandatory fields, fields below will be parsed only if the VSDB
    //    block is large enough
    // x[12]          - unknown
    // x[13]          - unknown
    // x[14]          - unknown
    // x[15] & 0x80   - x[15] & 0x7f is valid
    // x[15] & 0x7f   - unknown
    // x[16] & 0x80   - x[16] & 0x7f is valid
    // x[16] & 0x7f   - DPCD (DisplayPort configuration data) register offset for
    //                  proprietary AMD settings

    if (length < 5) {
        printf("    Data block is truncated (length = %d)\n", length);
        return;
    }

    unsigned version = x[0];
	printf("    Version: %u\n", version);

	printf("    Feature Caps: 0x%02x\n", x[1]);
    bool hdr_fields_valid = false;
    bool supports_local_dimming = false;
    bool has_global_backlight_control = false;
    if (version > 1) {
        hdr_fields_valid = x[1] & 0x02;
        has_global_backlight_control = x[1] & 0x04;
        supports_local_dimming = x[1] & 0x08;
        if (has_global_backlight_control)
            printf("      Global Backlight Control Supported\n");
        if (supports_local_dimming)
            printf("      Local Dimming Supported\n");
        if (version > 2) {
            // Obtained from:
            // https://github.com/torvalds/linux/commit/ec8e59cb4e0c1a52d5a541fff9dcec398b48f7b4
            if (x[1] & 0x40)
                printf("      FreeSync Panel Replay Supported\n");
        }
    }

    unsigned short max_refresh_rate;
    if (version > 2 && length > 0xb) {
        max_refresh_rate = (x[0xb] & 3) << 8 | x[0xa];
    } else {
        max_refresh_rate = x[3];
    }
	printf("    Minimum Refresh Rate: %u Hz\n", x[2]);
	printf("    Maximum Refresh Rate: %u Hz\n", max_refresh_rate);
	// Freesync 1.x flags
	// One or more of the 0xe6 bits signal that the VESA MCCS
	// protocol is used to switch the Freesync range
	printf("    Flags 1.x: 0x%02x%s\n", x[4],
	       (x[4] & 0xe6) ? " (MCCS)" : "");

    if (version < 2)
        return;
    if (length < 10) {
        printf("    Data block is truncated (length = %d)\n", length);
        return;
    }

    printf("    Flags 2.x: 0x%02x\n", x[5]);

    if (!hdr_fields_valid)
        return;

    const unsigned TF_PQ2084         = 0x0004;
    const unsigned TF_LINEAR_0_125   = 0x0020;
    const unsigned TF_GAMMA_22       = 0x0080;

    const unsigned CS_BT2020         = 0x0008;

    // the calculation of supported_tf is a bit weird because it doesn't correspond to
    // the description in the comment at the start of the function. but this is what ADL
    // (AMD Display Library) reports
    unsigned supported_tf = 0;
    unsigned supported_cs = 0;
    bool supports_hdr10 = x[5] & 0x34;
    if (supports_hdr10) {
        supported_tf |= TF_LINEAR_0_125 | TF_PQ2084;
        supported_cs |= CS_BT2020;
    }
    if (x[5] & 0x04)
        supported_tf |= TF_GAMMA_22;

    if (supported_tf & TF_PQ2084)
        printf("      ST 2084 (PQ) EOTF Supported\n");
    if (supported_tf & TF_LINEAR_0_125)
        printf("      Linear EOTF (Windows scRGB, 0.0 - 125.0) Supported\n");
    if (supported_tf & TF_GAMMA_22)
        printf("      Gamma 2.2 EOTF Supported\n");

    if (supported_cs & CS_BT2020)
        printf("      BT.2020 Gamut Supported\n");

    bool is_mini_led = x[5] >> 5 == 1;
    bool is_oled     = x[5] >> 5 == 2;
    if (is_mini_led)
        printf("      Display is Mini LED\n");
    if (is_oled)
        printf("      Display is OLED\n");

    printf("    Maximum luminance: %u (%.3f cd/m^2)\n",
           x[6], 50.0 * pow(2, x[6] / 32.0));
    printf("    Minimum luminance: %u (%.3f cd/m^2)\n",
           x[7], (50.0 * pow(2, x[6] / 32.0)) * pow(x[7] / 255.0, 2) / 100.0);
    if (supports_local_dimming || is_oled || has_global_backlight_control) {
        const char *type = "minimum backlight";
        if (supports_local_dimming || is_oled)
            type = "without local dimming";
        printf("    Maximum luminance (%s): %u (%.3f cd/m^2)\n",
               type, x[8], 50.0 * pow(2, x[8] / 32.0));
        printf("    Minimum luminance (%s): %u (%.3f cd/m^2)\n",
               type, x[9], (50.0 * pow(2, x[8] / 32.0)) * pow(x[9] / 255.0, 2) / 100.0);
    }
}

static std::string display_use_case(unsigned char x)
{
	switch (x) {
	case 1: return "Test equipment";
	case 2: return "Generic display";
	case 3: return "Television display";
	case 4: return "Desktop productivity display";
	case 5: return "Desktop gaming display";
	case 6: return "Presentation display";
	case 7: return "Virtual reality headset";
	case 8: return "Augmented reality";
	case 16: return "Video wall display";
	case 17: return "Medical imaging display";
	case 18: return "Dedicated gaming display";
	case 19: return "Dedicated video monitor display";
	case 20: return "Accessory display";
	default: break;
	}
	fail("Unknown Display product primary use case 0x%02x.\n", x);
	return "Unknown";
}

static void cta_microsoft(const unsigned char *x, unsigned length)
{
	// This VSDB is documented at:
	// https://docs.microsoft.com/en-us/windows-hardware/drivers/display/specialized-monitors-edid-extension
	printf("    Version: %u\n", x[0]);
	if (x[0] > 2) {
		// In version 1 and 2 these bits should always be set to 0.
		printf("    Desktop Usage: %u\n", (x[1] >> 6) & 1);
		printf("    Third-Party Usage: %u\n", (x[1] >> 5) & 1);
	}
	printf("    Display Product Primary Use Case: %s\n",
	       display_use_case(x[1] & 0x1f).c_str());
	printf("    Container ID: %s\n", containerid2s(x + 2).c_str());
}

static void cta_hdr10plus(const unsigned char *x, unsigned length)
{
	if (length == 0) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}
	printf("    Application Version: %u\n", x[0] & 3);
	printf("    Full Frame Peak Luminance Index: %u\n", (x[0] >> 2) & 3);
	printf("    Peak Luminance Index: %u\n", x[0] >> 4);
	hex_block("    ", x + 1, length - 1);
}

static void cta_dolby_video(const unsigned char *x, unsigned length)
{
	unsigned char version = (x[0] >> 5) & 0x07;

	printf("    Version: %u (%u bytes)\n", version, length + 5);
	if (x[0] & 0x01)
		printf("    Supports YUV422 12 bit\n");

	if (version == 0) {
		if (x[0] & 0x02)
			printf("    Supports 2160p60\n");
		if (x[0] & 0x04)
			printf("    Supports global dimming\n");
		unsigned char dm_version = x[16];
		printf("    DM Version: %u.%u\n", dm_version >> 4, dm_version & 0xf);
		unsigned pq = (x[14] << 4) | (x[13] >> 4);
		printf("    Target Min PQ: %u (%.8f cd/m^2)\n", pq, pq2nits(pq / 4095.0));
		pq = (x[15] << 4) | (x[13] & 0xf);
		printf("    Target Max PQ: %u (%u cd/m^2)\n", pq, (unsigned)pq2nits(pq / 4095.0));
		printf("    Rx, Ry: %.8f, %.8f\n",
		       ((x[1] >> 4) | (x[2] << 4)) / 4096.0,
		       ((x[1] & 0xf) | (x[3] << 4)) / 4096.0);
		printf("    Gx, Gy: %.8f, %.8f\n",
		       ((x[4] >> 4) | (x[5] << 4)) / 4096.0,
		       ((x[4] & 0xf) | (x[6] << 4)) / 4096.0);
		printf("    Bx, By: %.8f, %.8f\n",
		       ((x[7] >> 4) | (x[8] << 4)) / 4096.0,
		       ((x[7] & 0xf) | (x[9] << 4)) / 4096.0);
		printf("    Wx, Wy: %.8f, %.8f\n",
		       ((x[10] >> 4) | (x[11] << 4)) / 4096.0,
		       ((x[10] & 0xf) | (x[12] << 4)) / 4096.0);
		return;
	}

	if (version == 1) {
		if (x[0] & 0x02)
			printf("    Supports 2160p60\n");
		if (x[1] & 0x01)
			printf("    Supports global dimming\n");
		unsigned char dm_version = (x[0] >> 2) & 0x07;
		printf("    DM Version: %u.x\n", dm_version + 2);
		printf("    Colorimetry: %s\n", (x[2] & 0x01) ? "P3-D65" : "ITU-R BT.709");
		printf("    Low Latency: %s\n", (x[3] & 0x01) ? "Standard + Low Latency" : "Only Standard");
		double lm = (x[2] >> 1) / 127.0;
		printf("    Target Min Luminance: %.8f cd/m^2\n", lm * lm);
		printf("    Target Max Luminance: %u cd/m^2\n", 100 + (x[1] >> 1) * 50);
		if (length == 10) {
			printf("    Rx, Ry: %.8f, %.8f\n", x[4] / 256.0, x[5] / 256.0);
			printf("    Gx, Gy: %.8f, %.8f\n", x[6] / 256.0, x[7] / 256.0);
			printf("    Bx, By: %.8f, %.8f\n", x[8] / 256.0, x[9] / 256.0);
		} else {
			double xmin = 0.625;
			double xstep = (0.74609375 - xmin) / 31.0;
			double ymin = 0.25;
			double ystep = (0.37109375 - ymin) / 31.0;

			printf("    Unique Rx, Ry: %.8f, %.8f\n",
			       xmin + xstep * (x[6] >> 3),
			       ymin + ystep * (((x[6] & 0x7) << 2) | (x[4] & 0x01) | ((x[5] & 0x01) << 1)));
			xstep = 0.49609375 / 127.0;
			ymin = 0.5;
			ystep = (0.99609375 - ymin) / 127.0;
			printf("    Unique Gx, Gy: %.8f, %.8f\n",
			       xstep * (x[4] >> 1), ymin + ystep * (x[5] >> 1));
			xmin = 0.125;
			xstep = (0.15234375 - xmin) / 7.0;
			ymin = 0.03125;
			ystep = (0.05859375 - ymin) / 7.0;
			printf("    Unique Bx, By: %.8f, %.8f\n",
			       xmin + xstep * (x[3] >> 5),
			       ymin + ystep * ((x[3] >> 2) & 0x07));
		}
		return;
	}

	if (version == 2) {
		if (x[0] & 0x02)
			printf("    Supports Backlight Control\n");
		if (x[1] & 0x04)
			printf("    Supports global dimming\n");
		unsigned char dm_version = (x[0] >> 2) & 0x07;
		printf("    DM Version: %u.x\n", dm_version + 2);
		printf("    Backlt Min Luma: %u cd/m^2\n", 25 + (x[1] & 0x03) * 25);
		printf("    Interface: ");
		switch (x[2] & 0x03) {
		case 0: printf("Low-Latency\n"); break;
		case 1: printf("Low-Latency + Low-Latency-HDMI\n"); break;
		case 2: printf("Standard + Low-Latency\n"); break;
		case 3: printf("Standard + Low-Latency + Low-Latency-HDMI\n"); break;
		}
		printf("    Supports 10b 12b 444: ");
		switch ((x[3] & 0x01) << 1 | (x[4] & 0x01)) {
		case 0: printf("Not supported\n"); break;
		case 1: printf("10 bit\n"); break;
		case 2: printf("12 bit\n"); break;
		case 3: printf("Reserved\n"); break;
		}

		unsigned pq = 20 * (x[1] >> 3);
		printf("    Target Min PQ v2: %u (%.8f cd/m^2)\n", pq, pq2nits(pq / 4095.0));
		pq = 2055 + 65 * (x[2] >> 3);
		printf("    Target Max PQ v2: %u (%u cd/m^2)\n", pq, (unsigned)pq2nits(pq / 4095.0));

		printf("    Unique Rx, Ry: %.8f, %.8f\n",
		       0.625 + (x[5] >> 3) / 256.0,
		       0.25 + (x[6] >> 3) / 256.0);
		printf("    Unique Gx, Gy: %.8f, %.8f\n",
		       (x[3] >> 1) / 256.0,
		       0.5 + (x[4] >> 1) / 256.0);
		printf("    Unique Bx, By: %.8f, %.8f\n",
		       0.125 + (x[5] & 0x07) / 256.0,
		       0.03125 + (x[6] & 0x07) / 256.0);
	}
}

static void cta_dolby_audio(const unsigned char *x, unsigned length)
{
	unsigned char version = 1 + (x[0] & 0x07);

	printf("    Version: %u (%u bytes)\n", version, length + 5);
	if (x[0] & 0x80)
		printf("    Headphone playback only\n");
	if (x[0] & 0x40)
		printf("    Height speaker zone present\n");
	if (x[0] & 0x20)
		printf("    Surround speaker zone present\n");
	if (x[0] & 0x10)
		printf("    Center speaker zone present\n");
	if (x[1] & 0x01)
		printf("    Supports Dolby MAT PCM decoding at 48 kHz only, does not support TrueHD\n");
}

static void cta_uhda_fmm(const unsigned char *x, unsigned length)
{
	printf("    Filmmaker Mode Content Type: %u\n", x[0]);
	printf("    Filmmaker Mode Content Subtype: %u\n", x[1]);
}

const char *cta_speaker_map[] = {
	"FL/FR - Front Left/Right",
	"LFE1 - Low Frequency Effects 1",
	"FC - Front Center",
	"LS/RS - Left/Right Surround",
	"BC - Back Center",
	"FLc/FRc - Front Left/Right of Center",
	"BL/BR - Back Left/Right",
	"FLw/FRw - Front Left/Right Wide",

	"TpFL/TpFR - Top Front Left/Right",
	"TpC - Top Center",
	"TpFC - Top Front Center",

	// The following speakers are Deprecated in the SADB
	"LS/RS - Left/Right Surround",
	"LFE2 - Low Frequency Effects 2",
	"TpBC - Top Back Center",
	"SiL/SiR - Side Left/Right",
	"TpSiL/TpSiR - Top Side Left/Right",

	"TpBL/TpBR - Top Back Left/Right",
	"BtFC - Bottom Front Center",
	"BtFL/BtFR - Bottom Front Left/Right",
	"TpLS/TpRS - Top Left/Right Surround",
	NULL
};

void edid_state::cta_sadb(const unsigned char *x, unsigned length)
{
	unsigned sad_valid = 0x7f;
	unsigned sad;
	unsigned i;

	if (length < 3) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}

	sad = ((x[2] << 16) | (x[1] << 8) | x[0]);

	for (i = 0; cta_speaker_map[i]; i++) {
		bool valid = sad_valid & (1 << i);

		if ((sad >> i) & 1)
			printf("    %s%s\n", cta_speaker_map[i],
			       valid ? "" : " (Deprecated, use the RCDB)");
	}
	if (sad & ~sad_valid)
		warn("Specifies deprecated speakers.\n");
}

static void cta_vesa_dtcdb(const unsigned char *x, unsigned length)
{
	if (length != 7 && length != 15 && length != 31) {
		fail("Invalid length %u.\n", length);
		return;
	}

	switch (x[0] >> 6) {
	case 0: printf("    White"); break;
	case 1: printf("    Red"); break;
	case 2: printf("    Green"); break;
	case 3: printf("    Blue"); break;
	}
	unsigned v = x[0] & 0x3f;
	printf(" transfer characteristics: %u", v);
	for (unsigned i = 1; i < length; i++)
		printf(" %u", v += x[i]);
	printf(" 1023\n");
}

static void cta_vesa_vdddb(const unsigned char *x, unsigned length)
{
	if (length != 30) {
		fail("Invalid length %u.\n", length);
		return;
	}

	printf("    Interface Type: ");
	unsigned char v = x[0];
	switch (v >> 4) {
	case 0: printf("Analog (");
		switch (v & 0xf) {
		case 0: printf("15HD/VGA"); break;
		case 1: printf("VESA NAVI-V (15HD)"); break;
		case 2: printf("VESA NAVI-D"); break;
		default: printf("Reserved"); break;
		}
		printf(")\n");
		break;
	case 1: printf("LVDS %u lanes", v & 0xf); break;
	case 2: printf("RSDS %u lanes", v & 0xf); break;
	case 3: printf("DVI-D %u channels", v & 0xf); break;
	case 4: printf("DVI-I analog"); break;
	case 5: printf("DVI-I digital %u channels", v & 0xf); break;
	case 6: printf("HDMI-A"); break;
	case 7: printf("HDMI-B"); break;
	case 8: printf("MDDI %u channels", v & 0xf); break;
	case 9: printf("DisplayPort %u channels", v & 0xf); break;
	case 10: printf("IEEE-1394"); break;
	case 11: printf("M1 analog"); break;
	case 12: printf("M1 digital %u channels", v & 0xf); break;
	default: printf("Reserved"); break;
	}
	printf("\n");

	printf("    Interface Standard Version: %u.%u\n", x[1] >> 4, x[1] & 0xf);
	printf("    Content Protection Support: ");
	switch (x[2]) {
	case 0: printf("None\n"); break;
	case 1: printf("HDCP\n"); break;
	case 2: printf("DTCP\n"); break;
	case 3: printf("DPCP\n"); break;
	default: printf("Reserved\n"); break;
	}

	printf("    Minimum Clock Frequency: %u MHz\n", x[3] >> 2);
	printf("    Maximum Clock Frequency: %u MHz\n", ((x[3] & 0x03) << 8) | x[4]);
	printf("    Device Native Pixel Format: %ux%u\n",
	       x[5] | (x[6] << 8), x[7] | (x[8] << 8));
	printf("    Aspect Ratio: %.2f\n", (100 + x[9]) / 100.0);
	v = x[0x0a];
	printf("    Default Orientation: ");
	switch ((v & 0xc0) >> 6) {
	case 0x00: printf("Landscape\n"); break;
	case 0x01: printf("Portrait\n"); break;
	case 0x02: printf("Not Fixed\n"); break;
	case 0x03: printf("Undefined\n"); break;
	}
	printf("    Rotation Capability: ");
	switch ((v & 0x30) >> 4) {
	case 0x00: printf("None\n"); break;
	case 0x01: printf("Can rotate 90 degrees clockwise\n"); break;
	case 0x02: printf("Can rotate 90 degrees counterclockwise\n"); break;
	case 0x03: printf("Can rotate 90 degrees in either direction)\n"); break;
	}
	printf("    Zero Pixel Location: ");
	switch ((v & 0x0c) >> 2) {
	case 0x00: printf("Upper Left\n"); break;
	case 0x01: printf("Upper Right\n"); break;
	case 0x02: printf("Lower Left\n"); break;
	case 0x03: printf("Lower Right\n"); break;
	}
	printf("    Scan Direction: ");
	switch (v & 0x03) {
	case 0x00: printf("Not defined\n"); break;
	case 0x01: printf("Fast Scan is on the Major (Long) Axis and Slow Scan is on the Minor Axis\n"); break;
	case 0x02: printf("Fast Scan is on the Minor (Short) Axis and Slow Scan is on the Major Axis\n"); break;
	case 0x03: printf("Reserved\n");
		   fail("Scan Direction used the reserved value 0x03.\n");
		   break;
	}
	printf("    Subpixel Information: ");
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
	printf("    Horizontal and vertical dot/pixel pitch: %.2f x %.2f mm\n",
	       (double)(x[0x0c]) / 100.0, (double)(x[0x0d]) / 100.0);
	v = x[0x0e];
	printf("    Dithering: ");
	switch (v >> 6) {
	case 0: printf("None\n"); break;
	case 1: printf("Spatial\n"); break;
	case 2: printf("Temporal\n"); break;
	case 3: printf("Spatial and Temporal\n"); break;
	}
	printf("    Direct Drive: %s\n", (v & 0x20) ? "Yes" : "No");
	printf("    Overdrive %srecommended\n", (v & 0x10) ? "not " : "");
	printf("    Deinterlacing: %s\n", (v & 0x08) ? "Yes" : "No");

	v = x[0x0f];
	printf("    Audio Support: %s\n", (v & 0x80) ? "Yes" : "No");
	printf("    Separate Audio Inputs Provided: %s\n", (v & 0x40) ? "Yes" : "No");
	printf("    Audio Input Override: %s\n", (v & 0x20) ? "Yes" : "No");
	v = x[0x10];
	if (v)
		printf("    Audio Delay: %s%u ms\n", (v & 0x80) ? "" : "-", (v & 0x7f) * 2);
	else
		printf("    Audio Delay: no information provided\n");
	v = x[0x11];
	printf("    Frame Rate/Mode Conversion: ");
	switch (v >> 6) {
	case 0: printf("None\n"); break;
	case 1: printf("Single Buffering\n"); break;
	case 2: printf("Double Buffering\n"); break;
	case 3: printf("Advanced Frame Rate Conversion\n"); break;
	}
	if (v & 0x3f)
		printf("    Frame Rate Range: %u fps +/- %u fps\n",
		       x[0x12], v & 0x3f);
	else
		printf("    Nominal Frame Rate: %u fps\n", x[0x12]);
	printf("    Color Bit Depth: %u @ interface, %u @ display\n",
	       (x[0x13] >> 4) + 1, (x[0x13] & 0xf) + 1);
	v = x[0x15] & 3;
	if (v) {
		printf("    Additional Primary Chromaticities:\n");
		unsigned col_x = (x[0x16] << 2) | (x[0x14] >> 6);
		unsigned col_y = (x[0x17] << 2) | ((x[0x14] >> 4) & 3);
		printf("      Primary 4:   0.%04u, 0.%04u\n",
		       (col_x * 10000) / 1024, (col_y * 10000) / 1024);
		if (v > 1) {
			col_x = (x[0x18] << 2) | ((x[0x14] >> 2) & 3);
			col_y = (x[0x19] << 2) | (x[0x14] & 3);
			printf("      Primary 5:   0.%04u, 0.%04u\n",
			       (col_x * 10000) / 1024, (col_y * 10000) / 1024);
			if (v > 2) {
				col_x = (x[0x1a] << 2) | (x[0x15] >> 6);
				col_y = (x[0x1b] << 2) | ((x[0x15] >> 4) & 3);
				printf("      Primary 6:   0.%04u, 0.%04u\n",
				       (col_x * 10000) / 1024, (col_y * 10000) / 1024);
			}
		}
	}

	v = x[0x1c];
	printf("    Response Time %s: %u ms\n",
	       (v & 0x80) ? "White -> Black" : "Black -> White", v & 0x7f);
	v = x[0x1d];
	printf("    Overscan: %u%% x %u%%\n", v >> 4, v & 0xf);
}

static double decode_uchar_as_double(unsigned char x)
{
	signed char s = (signed char)x;

	return s / 64.0;
}

const char *cta_rcdb_speaker_map[] = {
	"FL/FR - Front Left/Right",
	"LFE1 - Low Frequency Effects 1",
	"FC - Front Center",
	"BL/BR - Back Left/Right",
	"BC - Back Center",
	"FLc/FRc - Front Left/Right of Center",
	"RLC/RRC - Left/Right Rear Surround (Deprecated)",
	"FLw/FRw - Front Left/Right Wide",

	"TpFL/TpFR - Top Front Left/Right",
	"TpC - Top Center",
	"TpFC - Top Front Center",
	"LS/RS - Left/Right Surround",
	"LFE2 - Low Frequency Effects 2",
	"TpBC - Top Back Center",
	"SiL/SiR - Side Left/Right",
	"TpSiL/TpSiR - Top Side Left/Right",

	"TpBL/TpBR - Top Back Left/Right",
	"BtFC - Bottom Front Center",
	"BtFL/BtFR - Bottom Front Left/Right",
	"TpLS/TpRS - Top Left/Right Surround (Deprecated)",
	NULL
};


void edid_state::cta_rcdb(const unsigned char *x, unsigned length)
{
	unsigned spm = ((x[3] << 16) | (x[2] << 8) | x[1]);
	unsigned i;

	if (length < 4) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}

	if ((x[0] & 0x20) && !cta.has_sldb)
		fail("'SLD' flag is 1, but no Speaker Location Data Block is found.\n");
	else if (!(x[0] & 0x20) && cta.has_sldb)
		fail("'SLD' flag is 0, but a Speaker Location Data Block is present.\n");

	if (x[0] & 0x40) {
		printf("    Speaker count: %u\n", (x[0] & 0x1f) + 1);
	} else {
		if (x[0] & 0x1f)
			fail("'Speaker' flag is 0, but 'Speaker Count' is != 0.\n");
		if (x[0] & 0x20)
			fail("'SLD' flag is 1, but 'Speaker' is 0.\n");
	}

	printf("    Speaker Presence Mask:\n");
	for (i = 0; cta_rcdb_speaker_map[i]; i++) {
		if ((spm >> i) & 1)
			printf("      %s\n", cta_rcdb_speaker_map[i]);
	}

	if ((x[0] & 0xa0) == 0x80)
		fail("'Display' flag set, but not the 'SLD' flag.\n");

	bool valid_max = cta.preparsed_sld_has_coord || (x[0] & 0x80);

	if (valid_max && length >= 7) {
		printf("    Xmax: %u dm\n", x[4]);
		printf("    Ymax: %u dm\n", x[5]);
		printf("    Zmax: %u dm\n", x[6]);
	} else if (!valid_max && length >= 7) {
		// The RCDB should have been truncated.
		warn("'Display' flag is 0 and 'Coord' is 0 for all SLDs, but the Max coordinates are still present.\n");
	}
	if ((x[0] & 0x80) && length >= 10) {
		printf("    DisplayX: %.3f * Xmax\n", decode_uchar_as_double(x[7]));
		printf("    DisplayY: %.3f * Ymax\n", decode_uchar_as_double(x[8]));
		printf("    DisplayZ: %.3f * Zmax\n", decode_uchar_as_double(x[9]));
	} else if (!(x[0] & 0x80) && length >= 10) {
		// The RCDB should have been truncated.
		warn("'Display' flag is 0, but the Display coordinates are still present.\n");
	}
}

static const struct {
	const char *name;
	double x, y, z;
} speaker_location[] = {
	{ "FL - Front Left",		    -1,    1,  0 },
	{ "FR - Front Right",		     1,    1,  0 },
	{ "FC - Front Center",		     0,    1,  0 },
	{ "LFE1 - Low Frequency Effects 1", -0.5,  1, -1 },
	{ "BL - Back Left",		    -1,   -1,  0 },
	{ "BR - Back Right",		     1,   -1,  0 },
	{ "FLC - Front Left of Center",	    -0.5,  1,  0 },
	{ "FRC - Front Right of Center",     0.5,  1,  0 },
	{ "BC - Back Center",		     0,   -1,  0 },
	{ "LFE2 - Low Frequency Effects 2",  0.5,  1, -1 },
	{ "SiL - Side Left",		    -1, 1.0/3.0, 0 },
	{ "SiR - Side Right",		     1, 1.0/3.0, 0 },
	{ "TpFL - Top Front Left",	    -1,    1,  1 },
	{ "TpFR - Top Front Right",	     1,    1,  1 },
	{ "TpFC - Top Front Center",	     0,    1,  1 },
	{ "TpC - Top Center",		     0,    0,  1 },
	{ "TpBL - Top Back Left",	    -1,   -1,  1 },
	{ "TpBR - Top Back Right",	     1,   -1,  1 },
	{ "TpSiL - Top Side Left",	    -1,    0,  1 },
	{ "TpSiR - Top Side Right",	     1,    0,  1 },
	{ "TpBC - Top Back Center",	     0,   -1,  1 },
	{ "BtFC - Bottom Front Center",	     0,    1, -1 },
	{ "BtFL - Bottom Front Left",	    -1,    1, -1 },
	{ "BtFR - Bottom Front Right",	     1,    1, -1 },
	{ "FLW - Front Left Wide",	    -1, 2.0/3.0, 0 },
	{ "FRW - Front Right Wide",	     1, 2.0/3.0, 0 },
	{ "LS - Left Surround",		    -1,    0,  0 },
	{ "RS - Right Surround",	     1,    0,  0 },
};

void edid_state::cta_sldb(const unsigned char *x, unsigned length)
{
	if (length < 2) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}

	unsigned active_cnt = 0;
	unsigned channel_is_active = 0;

	while (length >= 2) {
		printf("    Channel: %u (%sactive)\n", x[0] & 0x1f,
		       (x[0] & 0x20) ? "" : "not ");
		if (x[0] & 0x20) {
			if (channel_is_active & (1U << (x[0] & 0x1f)))
				fail("Channel Index %u was already marked 'Active'.\n",
				     x[0] & 0x1f);
			channel_is_active |= 1U << (x[0] & 0x1f);
			active_cnt++;
		}

		unsigned speaker_id = x[1] & 0x1f;

		if (speaker_id < ARRAY_SIZE(speaker_location)) {
			printf("      Speaker ID: %s\n", speaker_location[speaker_id].name);
		} else if (speaker_id == 0x1f) {
			printf("      Speaker ID: None Specified\n");
		} else {
			printf("      Speaker ID: Reserved (%u)\n", speaker_id);
			fail("Reserved Speaker ID specified.\n");
		}
		if (length >= 5 && (x[0] & 0x40)) {
			printf("      X: %.3f * Xmax\n", decode_uchar_as_double(x[2]));
			printf("      Y: %.3f * Ymax\n", decode_uchar_as_double(x[3]));
			printf("      Z: %.3f * Zmax\n", decode_uchar_as_double(x[4]));
			length -= 3;
			x += 3;
		} else if (speaker_id < ARRAY_SIZE(speaker_location)) {
			printf("      X: %.3f * Xmax (approximately)\n", speaker_location[speaker_id].x);
			printf("      Y: %.3f * Ymax (approximately)\n", speaker_location[speaker_id].y);
			printf("      Z: %.3f * Zmax (approximately)\n", speaker_location[speaker_id].z);
		}

		length -= 2;
		x += 2;
	}
	if (active_cnt != cta.preparsed_speaker_count)
		fail("There are %u active speakers, but 'Speaker Count' is %u.\n",
		     active_cnt, cta.preparsed_speaker_count);
}

void edid_state::cta_preparse_sldb(const unsigned char *x, unsigned length)
{
	cta.has_sldb = true;
	while (length >= 2) {
		if (length >= 5 && (x[0] & 0x40)) {
			cta.preparsed_sld_has_coord = true;
			return;
		}
		length -= 2;
		x += 2;
	}
}

void edid_state::cta_vcdb(const unsigned char *x, unsigned length)
{
	unsigned char d = x[0];

	cta.has_vcdb = true;
	if (length < 1) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}
	printf("    YCbCr quantization: %s\n",
	       (d & 0x80) ? "Selectable (via AVI YQ)" : "No Data");
	printf("    RGB quantization: %s\n",
	       (d & 0x40) ? "Selectable (via AVI Q)" : "No Data");
	/*
	 * If this bit is not set then that will result in interoperability
	 * problems (specifically with PCs/laptops) that quite often do not
	 * follow the default rules with respect to RGB Quantization Range
	 * handling.
	 *
	 * Starting with the CTA-861-H spec this bit is now required to be
	 * 1 for new designs.
	 */
	if (!(d & 0x40))
		fail("Set Selectable RGB Quantization to avoid interop issues.\n");
	/*
	 * Since most YCbCr formats use limited range, the interop issues are
	 * less noticable than for RGB formats.
	 *
	 * Starting with the CTA-861-H spec this bit is now required to be
	 * 1 for new designs, but just warn about it (for now).
	 */
	if ((cta.byte3 & 0x30) && !(d & 0x80))
		warn("Set Selectable YCbCr Quantization to avoid interop issues.\n");

	unsigned char s_pt = (d >> 4) & 0x03;
	unsigned char s_it = (d >> 2) & 0x03;
	unsigned char s_ce = d & 0x03;

	printf("    PT scan behavior: ");
	switch (s_pt) {
	case 0: printf("No Data\n"); break;
	case 1: printf("Always Overscanned\n"); break;
	case 2: printf("Always Underscanned\n"); break;
	case 3: printf("Supports both over- and underscan\n"); break;
	}
	printf("    IT scan behavior: ");
	switch (s_it) {
	case 0: printf("IT video formats not supported\n"); break;
	case 1:
		printf("Always Overscanned\n");
		// See Table 52 of CTA-861-G for a description of Byte 3
		if (cta.byte3 & 0x80)
			fail("IT video formats are always overscanned, but bit 7 of Byte 3 of the CTA-861 Extension header is set to underscanned.\n");
		break;
	case 2:
		printf("Always Underscanned\n");
		// See Table 52 of CTA-861-G for a description of Byte 3
		if (!(cta.byte3 & 0x80))
			fail("IT video formats are always underscanned, but bit 7 of Byte 3 of the CTA-861 Extension header is set to overscanned.\n");
		break;
	case 3: printf("Supports both over- and underscan\n"); break;
	}
	if (s_it < 2)
		warn("IT scan behavior is expected to support underscanned.\n");
	printf("    CE scan behavior: ");
	switch (s_ce) {
	case 0: printf("CE video formats not supported\n"); break;
	case 1: printf("Always Overscanned\n"); break;
	case 2: printf("Always Underscanned\n"); break;
	case 3: printf("Supports both over- and underscan\n"); break;
	}
	if (s_ce == 0)
		warn("'CE video formats not supported' makes no sense.\n");
	else if (s_pt == s_it && s_pt == s_ce)
		warn("S_PT is equal to S_IT and S_CE, so should be set to 0 instead.\n");
}

static const char *colorimetry1_map[] = {
	"xvYCC601",
	"xvYCC709",
	"sYCC601",
	"opYCC601",
	"opRGB",
	"BT2020cYCC",
	"BT2020YCC",
	"BT2020RGB",
};

static const char *colorimetry2_map[] = {
	"Gamut Boundary Description Metadata Profile P0",
	"Reserved F41",
	"Reserved F42",
	"Reserved F43",
	"Default",
	"sRGB",
	"ICtCp",
	"ST2113RGB",
};

void edid_state::cta_colorimetry_block(const unsigned char *x, unsigned length)
{
	unsigned i;

	if (length < 2) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}
	for (i = 0; i < ARRAY_SIZE(colorimetry1_map); i++)
		if (x[0] & (1 << i))
			printf("    %s\n", colorimetry1_map[i]);
	// Bit MD0 is used to indicate if HDMI Gamut Boundary Description
	// Metadata Profile P0 is supported. Bits F41-F43 are reserved
	// and must be set to 0.
	if (x[1] & 0xe)
		fail("Reserved bits F41-F43 must be 0.\n");
	for (i = 0; i < ARRAY_SIZE(colorimetry2_map); i++)
		if (x[1] & (1 << i))
			printf("    %s\n", colorimetry2_map[i]);
	// The sRGB bit (added in CTA-861.6) allows sources to explicitly
	// signal sRGB colorimetry. Without this the default colorimetry
	// of an RGB video is either sRGB or defaultRGB. It depends on the
	// Source which is used, and the Sink has no idea what it is getting.
	//
	// For proper compatibility with PCs enabling sRGB support is
	// desirable.
	if (!base.uses_srgb && !(x[1] & 0x20))
		warn("Set the sRGB colorimetry bit to avoid interop issues.\n");
	if (x[1] & 0xf0)
		cta.avi_version = 4;
}

static const char *eotf_map[] = {
	"Traditional gamma - SDR luminance range",
	"Traditional gamma - HDR luminance range",
	"SMPTE ST2084",
	"Hybrid Log-Gamma",
};

static void cta_hdr_static_metadata_block(const unsigned char *x, unsigned length)
{
	unsigned i;

	if (length < 2) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}
	printf("    Electro optical transfer functions:\n");
	for (i = 0; i < 6; i++) {
		if (x[0] & (1 << i)) {
			if (i < ARRAY_SIZE(eotf_map)) {
				printf("      %s\n", eotf_map[i]);
			} else {
				printf("      Unknown (%u)\n", i);
				fail("Unknown EOTF (%u).\n", i);
			}
		}
	}
	printf("    Supported static metadata descriptors:\n");
	for (i = 0; i < 8; i++) {
		if (x[1] & (1 << i))
			printf("      Static metadata type %u\n", i + 1);
	}

	if (length >= 3)
		printf("    Desired content max luminance: %u (%.3f cd/m^2)\n",
		       x[2], 50.0 * pow(2, x[2] / 32.0));

	if (length >= 4)
		printf("    Desired content max frame-average luminance: %u (%.3f cd/m^2)\n",
		       x[3], 50.0 * pow(2, x[3] / 32.0));

	if (length >= 5)
		printf("    Desired content min luminance: %u (%.3f cd/m^2)\n",
		       x[4], (50.0 * pow(2, x[2] / 32.0)) * pow(x[4] / 255.0, 2) / 100.0);
}

static void cta_hdr_dyn_metadata_block(const unsigned char *x, unsigned length)
{
	if (length < 3) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}
	while (length >= 3) {
		unsigned type_len = x[0];
		unsigned type = x[1] | (x[2] << 8);

		if (length < type_len + 1)
			return;
		printf("    HDR Dynamic Metadata Type %u\n", type);
		switch (type) {
		case 1:
		case 4:
			if (type_len > 2)
				printf("      Version: %u\n", x[3] & 0xf);
			break;
		case 2:
			if (type_len > 2) {
				unsigned version = x[3] & 0xf;
				printf("      Version: %u\n", version);
				if (version >= 1) {
					if (x[3] & 0x10) printf("      Supports SL-HDR1 (ETSI TS 103 433-1)\n");
					if (x[3] & 0x20) printf("      Supports SL-HDR2 (ETSI TS 103 433-2)\n");
					if (x[3] & 0x40) printf("      Supports SL-HDR3 (ETSI TS 103 433-3)\n");
				}
			}
			break;
		default:
			break;
		}
		length -= type_len + 1;
		x += type_len + 1;
	}
}

static const char *infoframe_types[] = {
	NULL,
	"Vendor-Specific",
	"Auxiliary Video Information",
	"Source Product Description",
	"Audio",
	"MPEG Source",
	"NTSC VBI",
	"Dynamic Range and Mastering",
};

static void cta_ifdb(const unsigned char *x, unsigned length)
{
	unsigned len_hdr = x[0] >> 5;

	if (length < 2) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}
	if (x[0] & 0x1f)
		fail("Bits F14-F10 are 0x%02x instead of 0x00.\n", x[0] & 0x1f);
	printf("    VSIFs: %u\n", x[1]);
	if (length < len_hdr + 2)
		return;
	length -= len_hdr + 2;
	x += len_hdr + 2;
	while (length > 0) {
		unsigned payload_len = x[0] >> 5;
		unsigned char type = x[0] & 0x1f;

		if (payload_len) {
			fail("Payload size must be 0, but it is %u.\n", payload_len);
			break;
		}

		const char *name = NULL;
		if (type < ARRAY_SIZE(infoframe_types))
			name = infoframe_types[type];
		if (!name)
			name = "Unknown";
		printf("    %s InfoFrame (%u)", name, type);

		if (type == 1) {
			if (length < 4) {
				fail("Remaining length %u < 4.\n");
				break;
			}

			unsigned oui = (x[3] << 16) | (x[2] << 8) | x[1];

			printf(", OUI %s\n", ouitohex(oui).c_str());
			x += 4;
			length -= 4;
		} else {
			printf("\n");
			x++;
			length--;
		}
	}
}

void edid_state::cta_pidb(const unsigned char *x, unsigned length)
{
	if (length < 4) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}
	unsigned oui = (x[0] << 16) | (x[1] << 8) | x[2];
	printf("    IEEE CID/OUI: %s\n", ouitohex(oui).c_str());
	if (length == 4)
		return;
	printf("    Version: %u\n", x[3]);
	if (x[3])
		fail("Unsupported version %u.\n", x[3]);
	if (length == 5)
		return;
	char pn[26];
	memcpy(pn, x + 4, length - 5);
	pn[length - 5] = 0;
	for (unsigned i = 0; i < length - 5; i++)
		if (x[4 + i] < 0x20 || x[4 + i] >= 0x80)
			fail("Product Name: invalid ASCII value at position %u.\n", i);
	printf("    Product Name: %s\n", pn);
}

void edid_state::cta_displayid_type_7(const unsigned char *x, unsigned length)
{
	check_displayid_datablock_revision(x[0], 0x00, 2);

	if (length < 21U + ((x[0] & 0x70) >> 4)) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}
	parse_displayid_type_1_7_timing(x + 1, true, 2, true);
}

void edid_state::cta_displayid_type_8(const unsigned char *x, unsigned length)
{
	check_displayid_datablock_revision(x[0], 0xe8, 1);
	if (length < ((x[0] & 0x08) ? 3 : 2)) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}

	unsigned sz = (x[0] & 0x08) ? 2 : 1;
	unsigned type = x[0] >> 6;

	if (type) {
		fail("Only code type 0 is supported.\n");
		return;
	}

	if (x[0] & 0x20)
		printf("    Also supports YCbCr 4:2:0\n");

	x++;
	length--;
	for (unsigned i = 0; i < length / sz; i++) {
		unsigned id = x[i * sz];

		if (sz == 2)
			id |= x[i * sz + 1] << 8;
		parse_displayid_type_4_8_timing(type, id, true);
	}
}

void edid_state::cta_displayid_type_10(const unsigned char *x, unsigned length)
{
	check_displayid_datablock_revision(x[0], 0x70);
	if (length < 7U + ((x[0] & 0x70) >> 4)) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}

	unsigned sz = 6U + ((x[0] & 0x70) >> 4);
	x++;
	length--;
	for (unsigned i = 0; i < length / sz; i++)
		parse_displayid_type_10_timing(x + i * sz, sz, true);
}

const char *cta_hdmi_speaker_map[] = {
	"FL/FR - Front Left/Right",
	"LFE1 - Low Frequency Effects 1",
	"FC - Front Center",
	"BL/BR - Back Left/Right",
	"BC - Back Center",
	"FLc/FRc - Front Left/Right of Center",
	"Reserved",
	"FLw/FRw - Front Left/Right Wide",

	"TpFL/TpFR - Top Front Left/Right",
	"TpC - Top Center",
	"TpFC - Top Front Center",
	"LS/RS - Left/Right Surround",
	"LFE2 - Low Frequency Effects 2",
	"TpBC - Top Back Center",
	"SiL/SiR - Side Left/Right",
	"TpSiL/TpSiR - Top Side Left/Right",

	"TpBL/TpBR - Top Back Left/Right",
	"BtFC - Bottom Front Center",
	"BtFL/BtFR - Bottom Front Left/Right",
	"TpLS/TpRS - Top Left/Right Surround",
	"LSd/RSd - Left/Right Surround Direct",
	NULL
};

static void cta_hdmi_audio_block(const unsigned char *x, unsigned length)
{
	unsigned num_descs;

	if (length < 2) {
		fail("Empty Data Block with length %u.\n", length);
		return;
	}
	if (x[0] & 3) {
		printf("    Max Stream Count: %u\n", (x[0] & 3) + 1);
		if (x[0] & 4)
			printf("    Supports MS NonMixed\n");
	} else if (x[0] & 4) {
		fail("MS NonMixed support indicated but Max Stream Count == 0.\n");
	}

	num_descs = x[1] & 7;
	if (num_descs == 0)
		return;
	warn("Support for HDMI 3D Audio is deprecated since HDMI 2.2.\n"); 
	length -= 2;
	x += 2;
	while (length >= 4) {
		if (length > 4) {
			unsigned format = x[0] & 0xf;

			printf("    %s:\n", audio_format(format).c_str());
			printf("      Max channels: %u\n", (x[1] & 0x1f)+1);
			printf("      Supported sample rates (kHz):%s%s%s%s%s%s%s\n",
			       (x[2] & 0x40) ? " 192" : "",
			       (x[2] & 0x20) ? " 176.4" : "",
			       (x[2] & 0x10) ? " 96" : "",
			       (x[2] & 0x08) ? " 88.2" : "",
			       (x[2] & 0x04) ? " 48" : "",
			       (x[2] & 0x02) ? " 44.1" : "",
			       (x[2] & 0x01) ? " 32" : "");
			if (format == 1)
				printf("      Supported sample sizes (bits):%s%s%s\n",
				       (x[3] & 0x04) ? " 24" : "",
				       (x[3] & 0x02) ? " 20" : "",
				       (x[3] & 0x01) ? " 16" : "");
		} else {
			unsigned sad = ((x[2] << 16) | (x[1] << 8) | x[0]);
			unsigned i;

			switch (x[3] >> 4) {
			case 1:
				printf("    Speaker Allocation for 10.2 channels:\n");
				break;
			case 2:
				printf("    Speaker Allocation for 22.2 channels:\n");
				break;
			case 3:
				printf("    Speaker Allocation for 30.2 channels:\n");
				break;
			default:
				printf("    Unknown Speaker Allocation (0x%02x)\n", x[3] >> 4);
				return;
			}

			for (i = 0; cta_hdmi_speaker_map[i]; i++) {
				if ((sad >> i) & 1)
					printf("      %s\n", cta_hdmi_speaker_map[i]);
			}
		}
		length -= 4;
		x += 4;
	}
}

void edid_state::cta_block(const unsigned char *x, std::vector<unsigned> &found_tags)
{
	unsigned length = x[0] & 0x1f;
	unsigned tag = (x[0] & 0xe0) >> 5;
	unsigned extended = (tag == 0x07) ? 1 : 0;

	x++;
	if (extended && length) {
		tag <<= 8;
		tag |= x[0];
		length--;
		x++;
	}

	bool dooutputname = true;
	bool audio_block = false;
	data_block.clear();

	switch (tag) {
	case 0x01: data_block = "Audio Data Block"; audio_block = true; break;
	case 0x02: data_block = "Video Data Block"; break;
	case 0x03: data_block = "Vendor-Specific Data Block"; break;
	case 0x04: data_block = "Speaker Allocation Data Block"; audio_block = true; break;
	case 0x05: data_block = "VESA Display Transfer Characteristics Data Block"; break;
	case 0x06: data_block = "Video Format Data Block"; break;
	case 0x07: data_block = "Unknown CTA-861 Data Block (extended tag truncated)"; break;

	case 0x700: data_block = "Video Capability Data Block"; break;
	case 0x701: data_block = "Vendor-Specific Video Data Block"; break;
	case 0x702: data_block = "VESA Video Display Device Data Block"; break;
	case 0x703: data_block = "VESA Video Timing Block Extension"; break;
	case 0x704: data_block = "Reserved for HDMI Video Data Block"; break;
	case 0x705: data_block = "Colorimetry Data Block"; break;
	case 0x706: data_block = "HDR Static Metadata Data Block"; break;
	case 0x707: data_block = "HDR Dynamic Metadata Data Block"; break;
	case 0x708: data_block = "Native Video Resolution Data Block"; break;

	case 0x70d: data_block = "Video Format Preference Data Block"; break;
	case 0x70e: data_block = "YCbCr 4:2:0 Video Data Block"; break;
	case 0x70f: data_block = "YCbCr 4:2:0 Capability Map Data Block"; break;
	case 0x710: data_block = "Reserved for CTA-861 Miscellaneous Audio Fields"; break;
	case 0x711: data_block = "Vendor-Specific Audio Data Block"; audio_block = true; break;
	case 0x712: data_block = "HDMI Audio Data Block"; audio_block = true; break;
	case 0x713: data_block = "Room Configuration Data Block"; audio_block = true; break;
	case 0x714: data_block = "Speaker Location Data Block"; audio_block = true; break;

	case 0x720: data_block = "InfoFrame Data Block"; break;
	case 0x721: data_block = "Product Information Data Block"; break;

	case 0x722: data_block = "DisplayID Type VII Video Timing Data Block"; break;
	case 0x723: data_block = "DisplayID Type VIII Video Timing Data Block"; break;
	case 0x72a: data_block = "DisplayID Type X Video Timing Data Block"; break;

	case 0x778: data_block = "HDMI Forum EDID Extension Override Data Block"; break;
	case 0x779: data_block = "HDMI Forum Sink Capability Data Block"; break;
	case 0x77a: data_block = "HDMI Forum Source-Based Tone Mapping Data Block"; break;

	default:
		std::string unknown_name;
		     if (tag < 0x700) unknown_name = "Unknown CTA-861 Data Block";
		else if (tag < 0x70d) unknown_name = "Unknown CTA-861 Video-Related Data Block";
		else if (tag < 0x720) unknown_name = "Unknown CTA-861 Audio-Related Data Block";
		else if (tag < 0x778) unknown_name = "Unknown CTA-861 Data Block";
		else if (tag < 0x780) unknown_name = "Unknown CTA-861 HDMI-Related Data Block";
		else                  unknown_name = "Unknown CTA-861 Data Block";
		unknown_name += std::string(" (") + (extended ? "extended " : "") + "tag " + utohex(tag & 0xff) + ", length " + std::to_string(length) + ")";
		printf("  %s:\n", unknown_name.c_str());
		warn("%s.\n", unknown_name.c_str());
		break;
	}

	switch (tag) {
	case 0x03:
	case 0x701:
	case 0x711: {
		unsigned ouinum;

		data_block_oui(data_block, x, length, &ouinum);
		x += (length < 3) ? length : 3;
		length -= (length < 3) ? length : 3;
		dooutputname = false;
		tag |= ouinum;
		break;
	}
	}

	if (dooutputname && data_block.length())
		printf("  %s:\n", data_block.c_str());

	switch (tag) {
	case 0x04:
	case 0x05:
	case 0x700:
	case 0x702:
	case 0x705:
	case 0x706:
	case 0x708:
	case 0x70d:
	case 0x70f:
	case 0x712:
	case 0x713:
	case 0x721:
	case 0x778:
	case 0x779:
	case 0x77a:
		if (std::find(found_tags.begin(), found_tags.end(), tag) != found_tags.end())
			fail("Only one instance of this Data Block is allowed.\n");
		break;
	}

	// See Table 52 of CTA-861-G for a description of Byte 3
	if (audio_block && !(cta.byte3 & 0x40))
		fail("Audio information is present, but bit 6 of Byte 3 of the CTA-861 Extension header indicates no Basic Audio support.\n");

	switch (tag) {
	case 0x01: cta_audio_block(x, length); break;
	case 0x02: cta_svd(x, length, false); break;
	case 0x03|kOUI_HDMI:
		cta_hdmi_block(x, length);
		// The HDMI OUI is present, so this EDID represents an HDMI
		// interface. And HDMI interfaces must use EDID version 1.3
		// according to the HDMI Specification, so check for this.
		if (base.edid_minor != 3)
			fail("The HDMI Specification requires EDID 1.3 instead of 1.%u.\n",
				 base.edid_minor);
		break;
	case 0x03|kOUI_HDMIForum:
		if (cta.previous_cta_tag != (0x03|kOUI_HDMI))
			fail("HDMI Forum VSDB did not immediately follow the HDMI VSDB.\n");
		if (cta.have_hf_scdb || cta.have_hf_vsdb)
			fail("Duplicate HDMI Forum VSDB/SCDB.\n");
		cta_hf_scdb(x, length);
		cta.have_hf_vsdb = true;
		break;
	case 0x03|kOUI_AMD: cta_amd(x, length); break;
	case 0x03|kOUI_Microsoft: if (length != 0x12) goto dodefault; cta_microsoft(x, length); break;
	case 0x03|kOUI_UHDA: cta_uhda_fmm(x, length); break;
	case 0x04: cta_sadb(x, length); break;
	case 0x05: cta_vesa_dtcdb(x, length); break;
	case 0x06: cta_vfdb(x, length); break;
	case 0x07: fail("Extended tag cannot have zero length.\n"); break;
	case 0x700: cta_vcdb(x, length); break;
	case 0x701|kOUI_HDR10: cta_hdr10plus(x, length); break;
	case 0x701|kOUI_Dolby: cta_dolby_video(x, length); break;
	// 0x701|kOUI_Apple: this almost certainly contains 'BLC Info/Corrections',
	// since the data (spread out over two VSDBs) is very similar to what is seen
	// in DisplayID blocks. Since I don't know how to parse this data, we still
	// default to a hex dump, but I mention this here in case data on how to
	// parse this becomes available.
	case 0x702: cta_vesa_vdddb(x, length); break;
	case 0x705: cta_colorimetry_block(x, length); break;
	case 0x706: cta_hdr_static_metadata_block(x, length); break;
	case 0x707: cta_hdr_dyn_metadata_block(x, length); break;
	case 0x708: cta_nvrdb(x, length); return;
	case 0x70d: cta_vfpdb(x, length); break;
	case 0x70e: cta_svd(x, length, true); break;
	case 0x70f: cta_y420cmdb(x, length); break;
	case 0x711|kOUI_Dolby: cta_dolby_audio(x, length); break;
	case 0x712: cta_hdmi_audio_block(x, length); break;
	case 0x713: cta_rcdb(x, length); break;
	case 0x714: cta_sldb(x, length); break;
	case 0x720: cta_ifdb(x, length); break;
	case 0x721: cta_pidb(x, length); break;
	case 0x722: cta_displayid_type_7(x, length); break;
	case 0x723: cta_displayid_type_8(x, length); break;
	case 0x72a: cta_displayid_type_10(x, length); break;
	case 0x778:
		cta_hf_eeodb(x, length);
		if (block_nr != 1)
			fail("Data Block can only be present in Block 1.\n");
		// This must be the first CTA-861 block
		if (cta.block_number > 0)
			fail("Data Block starts at a wrong offset.\n");
		break;
	case 0x779:
		if (cta.previous_cta_tag != (0x03|kOUI_HDMI))
			fail("HDMI Forum SCDB did not immediately follow the HDMI VSDB.\n");
		if (cta.have_hf_scdb || cta.have_hf_vsdb)
			fail("Duplicate HDMI Forum VSDB/SCDB.\n");
		if (block_nr != 1)
			fail("Data Block can only be present in Block 1.\n");
		if (length < 2) {
			data_block = std::string("HDMI Forum SCDB");
			fail("Invalid length %u < 2.\n", length);
			break;
		}
		if (x[0] || x[1])
			printf("  Non-zero SCDB reserved fields!\n");
		cta_hf_scdb(x + 2, length - 2);
		cta.have_hf_scdb = true;
		break;
	case 0x77a:
		cta_hf_sbtmdb(x, length);
		break;
dodefault:
	default:
		hex_block("    ", x, length);
		break;
	}

	cta.block_number++;
	cta.previous_cta_tag = tag;
	found_tags.push_back(tag);
}

void edid_state::preparse_cta_block(unsigned char *x)
{
	unsigned version = x[1];
	unsigned offset = x[2];

	if (offset >= 4) {
		unsigned char *detailed;
		bool update_checksum = false;

		for (detailed = x + offset; detailed + 17 < x + 127; detailed += 18) {
			if (memchk(detailed, 18))
				break;
			update_checksum |= preparse_detailed_block(detailed);
			if (detailed[0] || detailed[1])
				cta.preparsed_total_dtds++;
		}
		if (update_checksum)
			replace_checksum(x, EDID_PAGE_SIZE);
	}

	if (version < 3)
		return;

	if (!cta.byte3)
		cta.byte3 = x[3];

	for (unsigned i = 4; i < offset; i += (x[i] & 0x1f) + 1) {
		bool for_ycbcr420 = false;
		unsigned oui;

		switch ((x[i] & 0xe0) >> 5) {
		case 0x03:
			if ((x[i] & 0x1f) < 5)
				continue;
			oui = (x[i + 3] << 16) + (x[i + 2] << 8) + x[i + 1];
			if (oui == 0x000c03) {
				unsigned length = x[i] & 0x1f;
				unsigned b = i + 8;

				cta.has_hdmi = true;
				cta.preparsed_phys_addr = (x[i + 4] << 8) | x[i + 5];
				if (length < 9 || !(x[b] & 0x20))
					continue;
				if (x[b] & 0x80) {
					if (length < 11)
						continue;
					if (x[b] & 0x40) {
						if (length < 13)
							continue;
						b += 2;
					}
					b += 2;
				}
				cta.preparsed_image_size = (enum hdmi_image_size)((x[b + 1] & 0x18) >> 3);
			} else if ((oui == 0xca125c || oui == 0x5c12ca) &&
				   (x[i] & 0x1f) == 0x15 && replace_unique_ids) {
				memset(x + i + 6, 0, 16);
				replace_checksum(x, EDID_PAGE_SIZE);
			}
			break;
		case 0x06:
			if (!(x[i] & 0x1f) || cta.preparsed_first_vfd.rid)
				break;
			cta.preparsed_first_vfd = cta_parse_vfd(x + i + 2, (x[i + 1] & 3) + 1);
			break;
		case 0x07:
			if (x[i + 1] == 0x0d)
				cta.has_vfpdb = true;
			else if (x[i + 1] == 0x05)
				cta.has_cdb = true;
			else if (x[i + 1] == 0x08)
				cta.has_nvrdb = true;
			else if (x[i + 1] == 0x21)
				cta.has_pidb = true;
			else if (x[i + 1] == 0x13 && (x[i + 2] & 0x40)) {
				cta.preparsed_speaker_count = 1 + (x[i + 2] & 0x1f);
				cta.preparsed_sld = x[i + 2] & 0x20;
			} else if (x[i + 1] == 0x14)
				cta_preparse_sldb(x + i + 2, (x[i] & 0x1f) - 1);
			else if (x[i + 1] == 0x22)
				cta.preparsed_total_vtdbs++;
			else if (x[i + 1] == 0x23) {
				cta.preparsed_has_t8vtdb = true;
				cta.preparsed_t8vtdb_dmt = x[i + 3];
				if (x[i + 2] & 0x08)
					cta.preparsed_t8vtdb_dmt |= x[i + 4] << 8;
			} else if (x[i + 1] == 0x2a)
				cta.preparsed_total_vtdbs +=
					((x[i] & 0x1f) - 2) / (6 + ((x[i + 2] & 0x70) >> 4));
			else if (x[i + 1] == 0x78)
				cta.hf_eeodb_blocks = x[i + 2];
			if (x[i + 1] != 0x0e)
				continue;
			for_ycbcr420 = true;
#ifdef __EMSCRIPTEN__
			[[clang::fallthrough]];
#endif
			/* fall-through */
		case 0x02:
			for (unsigned j = 1 + for_ycbcr420; j <= (x[i] & 0x1f); j++) {
				unsigned char vic = x[i + j];

				if ((vic & 0x7f) <= 64)
					vic &= 0x7f;
				cta.preparsed_svds[for_ycbcr420].push_back(vic);
				cta.preparsed_has_vic[for_ycbcr420][vic] = true;

				if (for_ycbcr420)
					continue;

				const struct timings *t = find_vic_id(vic);

				if (t && t->pixclk_khz > cta.preparsed_max_vic_pixclk_khz)
					cta.preparsed_max_vic_pixclk_khz = t->pixclk_khz;

				switch (vic) {
				case 95:
					cta.preparsed_hdmi_vic_vsb_codes |= 1 << 0;
					break;
				case 94:
					cta.preparsed_hdmi_vic_vsb_codes |= 1 << 1;
					break;
				case 93:
					cta.preparsed_hdmi_vic_vsb_codes |= 1 << 2;
					break;
				case 98:
					cta.preparsed_hdmi_vic_vsb_codes |= 1 << 3;
					break;
				}
			}
			break;
		}
	}
}

void edid_state::parse_cta_block(const unsigned char *x)
{
	unsigned version = x[1];
	unsigned offset = x[2];
	const unsigned char *detailed;

	// See Table 52 of CTA-861-G for a description of Byte 3

	printf("  Revision: %u\n", version);
	if (version == 0)
		fail("Invalid CTA-861 Extension revision 0.\n");
	if (version == 2)
		fail("Deprecated CTA-861 Extension revision 2.\n");
	if (cta.has_hdmi && version != 3)
		fail("The HDMI Specification requires CTA Extension revision 3.\n");
	if (version > 3)
		warn("Unknown CTA-861 Extension revision %u.\n", version);
	if (offset > 0 && offset < 4)
		fail("Invalid CTA-861 Extension offset value (byte 2).\n");

	if (version >= 1) do {
		if (version == 1 && x[3] != 0)
			fail("Non-zero byte 3.\n");

		if (version < 3 && offset >= 4 && ((offset - 4) / 8)) {
			printf("  8-byte timing descriptors: %u\n", (offset - 4) / 8);
			fail("8-byte descriptors were never used.\n");
		}

		if (version >= 2) {
			if (x[3] & 0x80)
				printf("  Underscans IT Video Formats by default\n");
			else
				warn("IT Video Formats are overscanned by default, but normally this should be underscanned.\n");
			if (x[3] & 0x40)
				printf("  Basic audio support\n");
			if (x[3] & 0x20) {
				printf("  Supports YCbCr 4:4:4\n");
				cta.has_ycbcr444 = true;
			}
			if (x[3] & 0x10) {
				printf("  Supports YCbCr 4:2:2\n");
				cta.has_ycbcr422 = true;
			}
			// Disable this test: this fails a lot of EDIDs, and there are
			// also some corner cases where you only want to receive 4:4:4
			// and refuse a fallback to 4:2:2.
//			if ((x[3] & 0x30) && (x[3] & 0x30) != 0x30)
//				msg(!cta.has_hdmi, "If YCbCr support is indicated, then both 4:2:2 and 4:4:4 %s be supported.\n",
//				    cta.has_hdmi ? "shall" : "should");
			printf("  Native detailed modes: %u\n", x[3] & 0x0f);
			if (cta.block_number == 0)
				cta.byte3 = x[3];
			else if (x[3] != cta.byte3)
				fail("Byte 3 must be the same for all CTA-861 Extension Blocks.\n");
			if (cta.block_number == 0) {
				unsigned native_dtds = x[3] & 0x0f;

				cta.native_timings.clear();
				if (!native_dtds && !cta.has_vfpdb) {
					cta.first_svd_might_be_preferred = true;
				} else if (native_dtds > cta.preparsed_total_dtds) {
					fail("There are more Native DTDs (%u) than DTDs (%u).\n",
					     native_dtds, cta.preparsed_total_dtds);
				}
				if (native_dtds > cta.preparsed_total_dtds)
					native_dtds = cta.preparsed_total_dtds;
				for (unsigned i = 0; i < native_dtds; i++) {
					char type[16];

					sprintf(type, "DTD %3u", i + 1);
					cta.native_timings.push_back(timings_ext(i + 129, type));
					cta.has_svrs = true;
				}
				if (cta.has_hdmi && block_nr != (block_map.saw_block_1 ? 2 : 1))
					fail("The HDMI Specification requires that the first Extension Block (that is not a Block Map) is an CTA-861 Extension Block.\n");
			}
		}

		if (offset < 4) {
			// Offset 0 means that there are no data blocks or DTDs,
			// so the remainder must be padding.
			if (!memchk(x + 4, 127 - 4)) {
				data_block = "Padding";
				fail("Contains non-zero bytes.\n");
			}
			break;
		}

		if (offset > 127) {
			fail("Offset %u is larger than EDID block size-1 (%d).\n", offset, 127);
			break;
		}
		if (version >= 3) {
			unsigned i;

			for (i = 4; i < offset; i += (x[i] & 0x1f) + 1) {
				cta_block(x + i, cta.found_tags);
			}

			data_block.clear();
			if (i != offset)
				fail("Offset is %u, but should be %u.\n", offset, i);
		}

		data_block = "Detailed Timing Descriptors";
		base.seen_non_detailed_descriptor = false;
		bool first = true;
		for (detailed = x + offset; detailed + 17 < x + 127; detailed += 18) {
			if (memchk(detailed, 18))
				break;
			if (first) {
				first = false;
				printf("  %s:\n", data_block.c_str());
			}
			detailed_block(detailed);
		}
		unused_bytes = x + 127 - detailed;
		if (!memchk(detailed, unused_bytes)) {
			data_block = "Padding";
			fail("Contains non-zero bytes.\n");
		}
	} while (0);

	data_block.clear();
	if (!cta.first_cta)
		return;

	cta.first_cta = false;
	if (base.serial_number && serial_strings.size())
		warn("Display Product Serial Number is set, so the Serial Number in the Base EDID should be 0.\n");
	if (!cta.has_vic_1 && !base.has_640x480p60_est_timing)
		fail("Required 640x480p60 timings are missing in the established timings"
		     " and the SVD list (VIC 1).\n");
	if (!cta.has_vcdb)
		fail("Missing VCDB, needed for Set Selectable RGB Quantization to avoid interop issues.\n");
	if (!base.uses_srgb && !cta.has_cdb)
		warn("Add a Colorimetry Data Block with the sRGB colorimetry bit set to avoid interop issues.\n");
}

void edid_state::cta_resolve_svr(timings_ext &t_ext)
{
	if (t_ext.svr() == 254) {
		t_ext.flags = cta.t8vtdb.flags;
		add_str(t_ext.flags, ">=CTA-861-H");
		t_ext.t = cta.t8vtdb.t;
	} else if (t_ext.svr() <= 144) {
		if (t_ext.svr() < 129 || t_ext.svr() - 129 >= cta.vec_dtds.size())
			return;
		t_ext.flags = cta.vec_dtds[t_ext.svr() - 129].flags;
		t_ext.t = cta.vec_dtds[t_ext.svr() - 129].t;
	} else if (t_ext.svr() <= 160) {
		if (t_ext.svr() - 145 >= cta.vec_vtdbs.size())
			return;
		t_ext.flags = cta.vec_vtdbs[t_ext.svr() - 145].flags;
		add_str(t_ext.flags, ">=CTA-861-H");
		t_ext.t = cta.vec_vtdbs[t_ext.svr() - 145].t;
	} else if (t_ext.svr() <= 175) {
		t_ext.flags.clear();
		unsigned char rid = cta.preparsed_first_vfd.rid;
		t_ext.t = calc_ovt_mode(rids[rid].hact, rids[rid].vact,
					rids[rid].hratio, rids[rid].vratio,
					vf_rate_values[t_ext.svr() - 160]);
		t_ext.flags = ">=CTA-861.6";
	}
}

void edid_state::cta_resolve_svrs()
{
	for (vec_timings_ext::iterator iter = cta.preferred_timings_vfpdb.begin();
	     iter != cta.preferred_timings_vfpdb.end(); ++iter) {
		if (iter->has_svr())
			cta_resolve_svr(*iter);
	}

	for (vec_timings_ext::iterator iter = cta.native_timings.begin();
	     iter != cta.native_timings.end(); ++iter) {
		if (iter->has_svr())
			cta_resolve_svr(*iter);
	}

	for (vec_timings_ext::iterator iter = cta.native_timing_nvrdb.begin();
	     iter != cta.native_timing_nvrdb.end(); ++iter) {
		if (iter->has_svr())
			cta_resolve_svr(*iter);
	}
}

void edid_state::check_cta_blocks()
{
	unsigned max_pref_prog_hact = 0;
	unsigned max_pref_prog_vact = 0;
	unsigned max_pref_ilace_hact = 0;
	unsigned max_pref_ilace_vact = 0;

	data_block = "CTA-861";

	// HDMI 1.4 goes up to 340 MHz. Dubious to have a DTD above that,
	// but no VICs. Displays often have a setting to turn off HDMI 2.x
	// support, dropping any HDMI 2.x VICs, but they sometimes forget
	// to replace the DTD in the base block as well.
	if (cta.warn_about_hdmi_2x_dtd)
		warn("DTD pixelclock indicates HDMI 2.x support, VICs indicate HDMI 1.x.\n");

	if (cta.hdmi_max_rate && max_pixclk_khz > cta.hdmi_max_rate * 1000)
		fail("The maximum HDMI TMDS clock is %u kHz, but one or more video timings go up to %u kHz.\n",
		     cta.hdmi_max_rate * 1000, max_pixclk_khz);

	for (vec_timings_ext::iterator iter = cta.preferred_timings.begin();
	     iter != cta.preferred_timings.end(); ++iter) {
		if (iter->t.interlaced &&
		    (iter->t.vact > max_pref_ilace_vact ||
		     (iter->t.vact == max_pref_ilace_vact && iter->t.hact >= max_pref_ilace_hact))) {
			max_pref_ilace_hact = iter->t.hact;
			max_pref_ilace_vact = iter->t.vact;
		}
		if (!iter->t.interlaced &&
		    (iter->t.vact > max_pref_prog_vact ||
		     (iter->t.vact == max_pref_prog_vact && iter->t.hact >= max_pref_prog_hact))) {
			max_pref_prog_hact = iter->t.hact;
			max_pref_prog_vact = iter->t.vact;
		}
	}
	for (vec_timings_ext::iterator iter = cta.preferred_timings_vfpdb.begin();
	     iter != cta.preferred_timings_vfpdb.end(); ++iter) {
		if (iter->t.interlaced &&
		    (iter->t.vact > max_pref_ilace_vact ||
		     (iter->t.vact == max_pref_ilace_vact && iter->t.hact >= max_pref_ilace_hact))) {
			max_pref_ilace_hact = iter->t.hact;
			max_pref_ilace_vact = iter->t.vact;
		}
		if (!iter->t.interlaced &&
		    (iter->t.vact > max_pref_prog_vact ||
		     (iter->t.vact == max_pref_prog_vact && iter->t.hact >= max_pref_prog_hact))) {
			max_pref_prog_hact = iter->t.hact;
			max_pref_prog_vact = iter->t.vact;
		}
	}

	unsigned native_prog = 0;
	unsigned native_prog_hact = 0;
	unsigned native_prog_vact = 0;
	bool native_prog_mixed_resolutions = false;
	unsigned native_ilace = 0;
	unsigned native_ilace_hact = 0;
	unsigned native_ilace_vact = 0;
	bool native_ilace_mixed_resolutions = false;
	unsigned native_nvrdb_hact = 0;
	unsigned native_nvrdb_vact = 0;

	for (vec_timings_ext::iterator iter = cta.native_timings.begin();
	     iter != cta.native_timings.end(); ++iter) {
		if (iter->t.interlaced) {
			native_ilace++;
			if (!native_ilace_hact) {
				native_ilace_hact = iter->t.hact;
				native_ilace_vact = iter->t.vact;
			} else if (native_ilace_hact != iter->t.hact ||
				   native_ilace_vact != iter->t.vact) {
				native_ilace_mixed_resolutions = true;
			}
		} else {
			native_prog++;
			if (!native_prog_hact) {
				native_prog_hact = iter->t.hact;
				native_prog_vact = iter->t.vact;
			} else if (native_prog_hact != iter->t.hact ||
				   native_prog_vact != iter->t.vact) {
				native_prog_mixed_resolutions = true;
			}
		}
	}

	for (vec_timings_ext::iterator iter = cta.native_timing_nvrdb.begin();
	     iter != cta.native_timing_nvrdb.end(); ++iter) {
			native_nvrdb_hact = iter->t.hact;
			native_nvrdb_vact = iter->t.vact;
	}

	if (native_prog_mixed_resolutions)
		fail("Native progressive timings are a mix of several resolutions.\n");
	if (native_ilace_mixed_resolutions)
		fail("Native interlaced timings are a mix of several resolutions.\n");
	if (native_ilace && !native_prog)
		fail("A native interlaced timing is present, but not a native progressive timing.\n");
	if (!native_prog_mixed_resolutions && native_prog > 1)
		warn("Multiple native progressive timings are defined.\n");
	if (!native_ilace_mixed_resolutions && native_ilace > 1)
		warn("Multiple native interlaced timings are defined.\n");

	if (native_nvrdb_vact &&
	    (max_pref_prog_vact > native_nvrdb_vact ||
	     (max_pref_prog_vact == native_nvrdb_vact && max_pref_prog_hact > native_nvrdb_hact)))
		warn("Native video resolution of %ux%u is smaller than the max preferred progressive resolution %ux%u.\n",
		     native_nvrdb_hact, native_nvrdb_vact,
		     max_pref_prog_hact, max_pref_prog_vact);
	else if (!native_nvrdb_vact && !native_prog_mixed_resolutions && native_prog_vact &&
	    (max_pref_prog_vact > native_prog_vact ||
	     (max_pref_prog_vact == native_prog_vact && max_pref_prog_hact > native_prog_hact)))
		warn("Native progressive resolution of %ux%u is smaller than the max preferred progressive resolution %ux%u.\n",
		     native_prog_hact, native_prog_vact,
		     max_pref_prog_hact, max_pref_prog_vact);
	if (!native_ilace_mixed_resolutions && native_ilace_vact &&
	    (max_pref_ilace_vact > native_ilace_vact ||
	     (max_pref_ilace_vact == native_ilace_vact && max_pref_ilace_hact > native_ilace_hact)))
		warn("Native interlaced resolution of %ux%u is smaller than the max preferred interlaced resolution %ux%u.\n",
		     native_ilace_hact, native_ilace_vact,
		     max_pref_ilace_hact, max_pref_ilace_vact);

	if (dispid.native_width && native_prog_hact &&
	    !native_prog_mixed_resolutions) {
		if (dispid.native_width != native_prog_hact ||
		    dispid.native_height != native_prog_vact)
			fail("Mismatch between CTA-861 and DisplayID native progressive resolution.\n");
	}
}
