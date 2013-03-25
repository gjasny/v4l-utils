/*
    V4L2 API compliance input/output configuration ioctl tests.

    Copyright (C) 2011  Hans Verkuil <hverkuil@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "v4l2-compliance.h"

static int checkStd(struct node *node, bool has_std, v4l2_std_id mask, bool is_input)
{
	v4l2_std_id std_mask = 0;
	v4l2_std_id std;
	struct v4l2_standard enumstd;
	unsigned i;
	int ret;

	ret = doioctl(node, VIDIOC_G_STD, &std);
	if (ret && has_std)
		return fail("STD cap set, but could not get standard\n");
	if (!ret && !has_std)
		return fail("STD cap not set, but could still get a standard\n");
	if (ret != ENOTTY && ret != ENODATA && !has_std)
		return fail("STD cap not set, but got wrong error code (%d)\n", ret);
	if (!ret && has_std) {
		if (std & ~mask)
			warn("current standard is invalid according to the standard mask\n");
		if (std == 0)
			return fail("Standard == 0?!\n");
		if (std & V4L2_STD_ATSC)
			return fail("Current standard contains ATSC standards. This is no longer supported\n");
	}
	ret = doioctl(node, VIDIOC_S_STD, &std);
	if (ret && has_std)
		return fail("STD cap set, but could not set standard\n");
	if (!ret && !has_std)
		return fail("STD cap not set, but could still set a standard\n");
	std = V4L2_STD_ATSC;
	ret = doioctl(node, VIDIOC_S_STD, &std);
	if (ret != ENODATA && ret != EINVAL && ret != ENOTTY)
		return fail("could set standard to ATSC, which is not supported anymore\n");
	for (i = 0; ; i++) {
		memset(&enumstd, 0xff, sizeof(enumstd));

		enumstd.index = i;
		ret = doioctl(node, VIDIOC_ENUMSTD, &enumstd);
		if (ret)
			break;
		std_mask |= enumstd.id;
		if (check_ustring(enumstd.name, sizeof(enumstd.name)))
			return fail("invalid standard name\n");
		if (check_0(enumstd.reserved, sizeof(enumstd.reserved)))
			return fail("reserved not zeroed\n");
		if (enumstd.framelines == 0)
			return fail("framelines not filled\n");
		if (enumstd.framelines != 625 && enumstd.framelines != 525)
			warn("strange framelines value %d for standard %08llx\n",
					enumstd.framelines, enumstd.id);
		if (enumstd.frameperiod.numerator == 0 || enumstd.frameperiod.denominator == 0)
			return fail("frameperiod is not filled\n");
		if (enumstd.index != i)
			return fail("index changed!\n");
		if (enumstd.id == 0)
			return fail("empty standard returned\n");
	}
	if (i == 0 && has_std)
		return fail("STD cap set, but no standards can be enumerated\n");
	if (i && !has_std)
		return fail("STD cap was not set, but standards can be enumerated\n");
	if (ret != ENOTTY && ret != ENODATA && !has_std)
		return fail("STD cap not set, but got wrong error code for enumeration (%d)\n", ret);
	if (std_mask & V4L2_STD_ATSC)
		return fail("STD mask contains ATSC standards. This is no longer supported\n");
	if (has_std && std_mask != mask)
		return fail("the union of ENUMSTD does not match the standard mask (%llx != %llx)\n",
				std_mask, mask);
	ret = doioctl(node, VIDIOC_QUERYSTD, &std);
	if (!ret && !has_std)
		return fail("STD cap was not set, but could still query standard\n");
	if (ret != ENOTTY && ret != ENODATA && !has_std)
		return fail("STD cap not set, but got wrong error code for query (%d)\n", ret);
	if (ret != ENOTTY && !is_input)
		return fail("this is an output, but could still query standard\n");
	if (!ret && is_input && (std & ~std_mask))
		return fail("QUERYSTD gives back an unsupported standard\n");
	return 0;
}

int testStd(struct node *node)
{
	int ret;
	unsigned i, o;
	bool has_std = false;

	for (i = 0; i < node->inputs; i++) {
		struct v4l2_input input;

		input.index = i;
		ret = doioctl(node, VIDIOC_ENUMINPUT, &input);
		if (ret)
			return fail("could not enumerate input %d?!\n", i);
		ret = doioctl(node, VIDIOC_S_INPUT, &input.index);
		if (ret)
			return fail("could not select input %d.\n", i);
		if (input.capabilities & V4L2_IN_CAP_STD)
			has_std = true;
		if (checkStd(node, input.capabilities & V4L2_IN_CAP_STD, input.std, true))
			return fail("STD failed for input %d.\n", i);
	}

	for (o = 0; o < node->outputs; o++) {
		struct v4l2_output output;

		output.index = o;
		ret = doioctl(node, VIDIOC_ENUMOUTPUT, &output);
		if (ret)
			return fail("could not enumerate output %d?!\n", o);
		ret = doioctl(node, VIDIOC_S_OUTPUT, &output.index);
		if (ret)
			return fail("could not select output %d.\n", o);
		if (output.capabilities & V4L2_OUT_CAP_STD)
			has_std = true;
		if (checkStd(node, output.capabilities & V4L2_OUT_CAP_STD, output.std, false))
			return fail("STD failed for output %d.\n", o);
	}
	return has_std ? 0 : ENOTTY;
}

static int checkTimings(struct node *node, bool has_timings, bool is_input)
{
	struct v4l2_enum_dv_timings enumtimings;
	struct v4l2_dv_timings timings;
	int ret;
	unsigned i;

	memset(&timings, 0xff, sizeof(timings));
	ret = doioctl(node, VIDIOC_G_DV_TIMINGS, &timings);
	if (ret && has_timings)
		return fail("TIMINGS cap set, but could not get current timings\n");
	if (!ret && !has_timings)
		return fail("TIMINGS cap not set, but could still get timings\n");
	if (ret != ENOTTY && ret != ENODATA && !has_timings)
		return fail("TIMINGS cap not set, but got wrong error code (%d)\n", ret);

	for (i = 0; ; i++) {
		memset(&enumtimings, 0xff, sizeof(enumtimings));

		enumtimings.index = i;
		ret = doioctl(node, VIDIOC_ENUM_DV_TIMINGS, &enumtimings);
		if (ret)
			break;
		if (check_0(enumtimings.reserved, sizeof(enumtimings.reserved)))
			return fail("reserved not zeroed\n");
		if (enumtimings.index != i)
			return fail("index changed!\n");
	}
	if (i == 0 && has_timings)
		return fail("TIMINGS cap set, but no timings can be enumerated\n");
	if (i && !has_timings)
		return fail("TIMINGS cap was not set, but timings can be enumerated\n");
	if (ret != ENOTTY && ret != ENODATA && !has_timings)
		return fail("TIMINGS cap not set, but got wrong error code for enumeration (%d)\n", ret);
	ret = doioctl(node, VIDIOC_QUERY_DV_TIMINGS, &timings);
	if (!ret && !has_timings)
		return fail("TIMINGS cap was not set, but could still query timings\n");
	if (ret != ENOTTY && ret != ENODATA && !has_timings)
		return fail("TIMINGS cap not set, but got wrong error code for query (%d)\n", ret);
	if (ret != ENOTTY && !is_input)
		return fail("this is an output, but could still query timings\n");
	return 0;
}

int testTimings(struct node *node)
{
	int ret;
	unsigned i, o;
	bool has_timings = false;

	for (i = 0; i < node->inputs; i++) {
		struct v4l2_input input;

		input.index = i;
		ret = doioctl(node, VIDIOC_ENUMINPUT, &input);
		if (ret)
			return fail("could not enumerate input %d?!\n", i);
		ret = doioctl(node, VIDIOC_S_INPUT, &input.index);
		if (ret)
			return fail("could not select input %d.\n", i);
		if (input.capabilities & V4L2_IN_CAP_DV_TIMINGS)
			has_timings = true;
		if (checkTimings(node, input.capabilities & V4L2_IN_CAP_DV_TIMINGS, true))
			return fail("Timings failed for input %d.\n", i);
	}

	for (o = 0; o < node->outputs; o++) {
		struct v4l2_output output;

		output.index = o;
		ret = doioctl(node, VIDIOC_ENUMOUTPUT, &output);
		if (ret)
			return fail("could not enumerate output %d?!\n", o);
		ret = doioctl(node, VIDIOC_S_OUTPUT, &output.index);
		if (ret)
			return fail("could not select output %d.\n", o);
		if (output.capabilities & V4L2_OUT_CAP_DV_TIMINGS)
			has_timings = true;
		if (checkTimings(node, output.capabilities & V4L2_OUT_CAP_DV_TIMINGS, false))
			return fail("Timings check failed for output %d.\n", o);
	}
	return has_timings ? 0 : ENOTTY;
}

static int checkTimingsCap(struct node *node, bool has_timings)
{
	struct v4l2_dv_timings_cap timingscap;
	int ret;

	memset(&timingscap, 0xff, sizeof(timingscap));
	ret = doioctl(node, VIDIOC_DV_TIMINGS_CAP, &timingscap);
	if (ret && has_timings)
		return fail("TIMINGS cap set, but could not get timings caps\n");
	if (!ret && !has_timings)
		return fail("TIMINGS cap not set, but could still get timings caps\n");
	if (ret && !has_timings)
		return 0;
	if (check_0(timingscap.reserved, sizeof(timingscap.reserved)))
		return fail("reserved not zeroed\n");
	fail_on_test(timingscap.type != V4L2_DV_BT_656_1120);
	if (check_0(timingscap.bt.reserved, sizeof(timingscap.bt.reserved)))
		return fail("reserved not zeroed\n");
	fail_on_test(timingscap.bt.min_width > timingscap.bt.max_width);
	fail_on_test(timingscap.bt.min_height > timingscap.bt.max_height);
	fail_on_test(timingscap.bt.min_pixelclock > timingscap.bt.max_pixelclock);
	return 0;
}

int testTimingsCap(struct node *node)
{
	int ret;
	unsigned i, o;
	bool has_timings = false;

	for (i = 0; i < node->inputs; i++) {
		struct v4l2_input input;

		input.index = i;
		ret = doioctl(node, VIDIOC_ENUMINPUT, &input);
		if (ret)
			return fail("could not enumerate input %d?!\n", i);
		ret = doioctl(node, VIDIOC_S_INPUT, &input.index);
		if (ret)
			return fail("could not select input %d.\n", i);
		if (input.capabilities & V4L2_IN_CAP_DV_TIMINGS)
			has_timings = true;
		if (checkTimingsCap(node, input.capabilities & V4L2_IN_CAP_DV_TIMINGS))
			return fail("Timings cap failed for input %d.\n", i);
	}

	for (o = 0; o < node->outputs; o++) {
		struct v4l2_output output;

		output.index = o;
		ret = doioctl(node, VIDIOC_ENUMOUTPUT, &output);
		if (ret)
			return fail("could not enumerate output %d?!\n", o);
		ret = doioctl(node, VIDIOC_S_OUTPUT, &output.index);
		if (ret)
			return fail("could not select output %d.\n", o);
		if (output.capabilities & V4L2_OUT_CAP_DV_TIMINGS)
			has_timings = true;
		if (checkTimingsCap(node, output.capabilities & V4L2_OUT_CAP_DV_TIMINGS))
			return fail("Timings cap check failed for output %d.\n", o);
	}
	return has_timings ? 0 : ENOTTY;
}
