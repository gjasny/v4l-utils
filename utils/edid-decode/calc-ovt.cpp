// SPDX-License-Identifier: MIT
/*
 * Copyright 2021 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <numeric>

#include "edid-decode.h"

#define MinVblankDuration 460
#define MinVblankLines 20
#define MinVsyncLeadingEdge 400
#define MinVsyncLELines 14
#define MinClockRate420 590000000
#define PixelFactor420 2
#define MinHblank444 80
#define MinHblank420 128
#define PixelClockGranularity 1000
#define MinHtotalGranularity 8
#define MaxChunkRate 650000000
#define AudioPacketRate 195000
#define AudioPacketSize 32
#define LineOverhead 32

static unsigned roundup_to_power_of_two(unsigned v)
{
	unsigned shift = 1;
	unsigned mask = 0;

	if (!v || v > 0x80000000) {
		fprintf(stderr, "roundup_to_power_of_two: invalid input %u.\n", v);
		exit(1);
	}

	v--;
	do {
		mask = v >> shift;
		v |= mask;
		shift <<= 1;
	} while (mask);
	return v + 1;
}

static unsigned greatest_power_of_two_divider(unsigned x)
{
	return x & ~(x - 1);
}

timings edid_state::calc_ovt_mode(unsigned Hactive, unsigned Vactive,
				  unsigned Hratio, unsigned Vratio,
				  unsigned Vrate)
{
	timings t = {};

	t.hact = Hactive;
	t.vact = Vactive;
	t.hratio = Hratio;
	t.vratio = Vratio;

	unsigned MaxVrate = Vrate;
	unsigned VtotalGranularity = 1;

	// Step 1
	switch (Vrate) {
	case 24: case 25: case 30:
		MaxVrate = 30;
		VtotalGranularity = 20;
		break;
	case 48: case 50: case 60:
		MaxVrate = 60;
		VtotalGranularity = 20;
		break;
	case 100: case 120:
		MaxVrate = 120;
		VtotalGranularity = 5;
		break;
	case 200: case 240:
		MaxVrate = 240;
		VtotalGranularity = 5;
		break;
	case 300: case 360:
		MaxVrate = 360;
		VtotalGranularity = 5;
		break;
	case 400: case 480:
		MaxVrate = 480;
		VtotalGranularity = 5;
		break;
	}

	// Step 2
	double MaxActiveTime = (1000000.0 / MaxVrate) - MinVblankDuration;
	double MinLineTime = MaxActiveTime / Vactive;
	unsigned MinVblank = max(MinVblankLines, ceil(MinVblankDuration / MinLineTime));
	unsigned MinVtotal = Vactive + MinVblank;

	if (MinVtotal % VtotalGranularity)
		MinVtotal += VtotalGranularity - (MinVtotal % VtotalGranularity);

	// Step 3
	unsigned MinLineRate = MaxVrate * MinVtotal;
	unsigned MaxAudioPacketsPerLine = ceil((double)AudioPacketRate / MinLineRate);

	// Step 4
	unsigned MinHtotal = Hactive +
		max(MinHblank444, LineOverhead + AudioPacketSize * MaxAudioPacketsPerLine);
	double MinPixelClockRate = (double)MaxVrate * MinHtotal * MinVtotal;
	double HCalcGranularity = roundup_to_power_of_two(ceil(MinPixelClockRate / MaxChunkRate));
	unsigned HtotalGranularity = max(MinHtotalGranularity, HCalcGranularity);

	if (MinHtotal % HtotalGranularity)
		MinHtotal += HtotalGranularity - (MinHtotal % HtotalGranularity);

	unsigned ResolutionGranularity = PixelClockGranularity /
		gcd(PixelClockGranularity, MaxVrate);
	unsigned Htotal = 0;
	unsigned Vtotal = 0;
	unsigned long long PixelClockRate = 0;

	for (;;) {
		// Step 5
		unsigned long long RMin = 0;
		unsigned V = MinVtotal;

		for (;;) {
			unsigned H = MinHtotal;
			unsigned long long R = H * V;
			if (RMin && R > RMin)
				break;
			while (R % ResolutionGranularity ||
			       MaxVrate * R / greatest_power_of_two_divider(H) > MaxChunkRate) {
				H += HtotalGranularity;
				R = H * V;
			}
			if (!RMin || R < RMin) {
				Htotal = H;
				Vtotal = V;
				RMin = R;
			}
			V += VtotalGranularity;
		}
		PixelClockRate = MaxVrate * RMin;

		// Step 6
		MinHtotal = Hactive + max(MinHblank420, PixelFactor420 *
					  (LineOverhead + AudioPacketSize * MaxAudioPacketsPerLine));
		if (PixelClockRate >= MinClockRate420 &&
		    Htotal < MinHtotal)
			continue;
		break;
	}

	// Step 7
	Vtotal = Vtotal * MaxVrate / Vrate;

	// Step 8
	unsigned Vblank = Vtotal - Vactive;
	unsigned VsyncPosition = max(MinVsyncLELines,
				     ceil((double)MinVsyncLeadingEdge * PixelClockRate / (1000000.0 * Htotal)));
	t.vfp = Vblank - VsyncPosition;

	t.vsync = 8;
	t.vbp = Vblank - t.vfp - t.vsync;
	t.pos_pol_vsync = true;
	unsigned Hblank = Htotal - Hactive;
	t.hsync = 32;
	t.hbp = 32;
	t.hfp = Hblank - t.hsync - t.hbp;
	t.pos_pol_hsync = true;
	t.pixclk_khz = PixelClockRate / 1000;
	if (!t.hratio || !t.vratio)
		calc_ratio(&t);
	return t;
}
