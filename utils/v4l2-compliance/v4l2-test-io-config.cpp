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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

static int checkStd(struct node *node, bool has_std, v4l2_std_id mask)
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
	if (!ret && has_std) {
		if (std & ~mask)
			warn("current standard is invalid according to the input standard mask\n");
		if (std == 0)
			return fail("Standard == 0?!\n");
	}
	if (std & V4L2_STD_ATSC)
		return fail("Current standard contains ATSC standards. This is no longer supported\n");
	ret = doioctl(node, VIDIOC_S_STD, &std);
	if (ret && has_std)
		return fail("STD cap set, but could not set standard\n");
	if (!ret && !has_std)
		return fail("STD cap not set, but could still set a standard\n");
	std = V4L2_STD_ATSC;
	ret = doioctl(node, VIDIOC_S_STD, &std);
	if (ret != EINVAL)
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
	if (std_mask & V4L2_STD_ATSC)
		return fail("STD mask contains ATSC standards. This is no longer supported\n");
	ret = doioctl(node, VIDIOC_QUERYSTD, &std);
	if (!ret && !has_std)
		return fail("STD cap was not set, but could still query standard\n");
	if (!ret && (std & ~std_mask))
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
		if (checkStd(node, input.capabilities & V4L2_IN_CAP_STD, input.std))
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
		if (checkStd(node, output.capabilities & V4L2_OUT_CAP_STD, output.std))
			return fail("STD failed for output %d.\n", o);
	}
	return has_std ? 0 : -ENOSYS;
}

static int checkPresets(struct node *node, bool has_presets)
{
	struct v4l2_dv_enum_preset enumpreset;
	struct v4l2_dv_preset preset;
	unsigned i;
	int ret;

	memset(&preset, 0xff, sizeof(preset));
	ret = doioctl(node, VIDIOC_G_DV_PRESET, &preset);
	if (!ret && check_0(preset.reserved, sizeof(preset.reserved)))
		return fail("reserved not zeroed\n");
	if (ret && has_presets)
		return fail("PRESET cap set, but could not get current preset\n");
	if (!ret && !has_presets)
		return fail("PRESET cap not set, but could still get a preset\n");
	if (preset.preset != V4L2_DV_INVALID) {
		ret = doioctl(node, VIDIOC_S_DV_PRESET, &preset);
		if (ret && has_presets)
			return fail("PRESET cap set, but could not set preset\n");
		if (!ret && !has_presets)
			return fail("PRESET cap not set, but could still set a preset\n");
	}
	preset.preset = V4L2_DV_INVALID;
	ret = doioctl(node, VIDIOC_S_DV_PRESET, &preset);
	if (ret != EINVAL)
		return fail("could set preset V4L2_DV_INVALID\n");

	for (i = 0; ; i++) {
		memset(&enumpreset, 0xff, sizeof(enumpreset));

		enumpreset.index = i;
		ret = doioctl(node, VIDIOC_ENUM_DV_PRESETS, &enumpreset);
		if (ret)
			break;
		if (check_ustring(enumpreset.name, sizeof(enumpreset.name)))
			return fail("invalid preset name\n");
		if (check_0(enumpreset.reserved, sizeof(enumpreset.reserved)))
			return fail("reserved not zeroed\n");
		if (enumpreset.index != i)
			return fail("index changed!\n");
		if (enumpreset.preset == V4L2_DV_INVALID)
			return fail("invalid preset!\n");
		if (enumpreset.width == 0 || enumpreset.height == 0)
			return fail("width or height not set\n");
	}
	if (i == 0 && has_presets)
		return fail("PRESET cap set, but no presets can be enumerated\n");
	if (i && !has_presets)
		return fail("PRESET cap was not set, but presets can be enumerated\n");
	ret = doioctl(node, VIDIOC_QUERY_DV_PRESET, &preset);
	if (!ret && !has_presets)
		return fail("PRESET cap was not set, but could still query preset\n");
	return 0;
}

int testPresets(struct node *node)
{
	int ret;
	unsigned i, o;
	bool has_presets = false;

	for (i = 0; i < node->inputs; i++) {
		struct v4l2_input input;

		input.index = i;
		ret = doioctl(node, VIDIOC_ENUMINPUT, &input);
		if (ret)
			return fail("could not enumerate input %d?!\n", i);
		ret = doioctl(node, VIDIOC_S_INPUT, &input.index);
		if (ret)
			return fail("could not select input %d.\n", i);
		if (input.capabilities & V4L2_IN_CAP_PRESETS)
			has_presets = true;
		if (checkPresets(node, input.capabilities & V4L2_IN_CAP_PRESETS))
			return fail("Presets failed for input %d.\n", i);
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
		if (output.capabilities & V4L2_OUT_CAP_PRESETS)
			has_presets = true;
		if (checkPresets(node, output.capabilities & V4L2_OUT_CAP_PRESETS))
			return fail("Presets check failed for output %d.\n", o);
	}
	return has_presets ? 0 : -ENOSYS;
}

static int checkTimings(struct node *node, bool has_timings)
{
	struct v4l2_dv_timings timings;
	int ret;

	memset(&timings, 0xff, sizeof(timings));
	ret = doioctl(node, VIDIOC_G_DV_TIMINGS, &timings);
	if (!ret && check_0(timings.reserved, sizeof(timings.reserved)))
		return fail("reserved not zeroed\n");
	if (ret && has_timings)
		return fail("TIMINGS cap set, but could not get current custom timings\n");
	if (!ret && !has_timings)
		return fail("TIMINGS cap not set, but could still get custom timings\n");
	/* I can't really test anything else here since due to the nature of these
	   ioctls there isn't anything 'generic' that I can test. If you use this,
	   then you are supposed to know what you are doing. */
	return 0;
}

int testCustomTimings(struct node *node)
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
		if (input.capabilities & V4L2_IN_CAP_CUSTOM_TIMINGS)
			has_timings = true;
		if (checkTimings(node, input.capabilities & V4L2_IN_CAP_CUSTOM_TIMINGS))
			return fail("Custom timings failed for input %d.\n", i);
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
		if (output.capabilities & V4L2_OUT_CAP_CUSTOM_TIMINGS)
			has_timings = true;
		if (checkTimings(node, output.capabilities & V4L2_OUT_CAP_CUSTOM_TIMINGS))
			return fail("Custom timings check failed for output %d.\n", o);
	}
	return has_timings ? 0 : -ENOSYS;
}
