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
	v4l2_std_id std = 0;
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
	struct v4l2_format fmt = { 0 };
	bool is_mplane = node->g_caps() & (V4L2_CAP_VIDEO_CAPTURE_MPLANE |
				       V4L2_CAP_VIDEO_OUTPUT_MPLANE |
				       V4L2_CAP_VIDEO_M2M_MPLANE);
	unsigned type;
	unsigned i;
	int ret;

	memset(&timings, 0xff, sizeof(timings));
	if (node->can_capture)
		type = is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
	else
		type = is_mplane ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT;
	ret = doioctl(node, VIDIOC_G_DV_TIMINGS, &timings);
	if (ret && has_timings)
		return fail("TIMINGS cap set, but could not get current timings\n");
	if (!ret && !has_timings)
		return fail("TIMINGS cap not set, but could still get timings\n");
	if (ret != ENOTTY && ret != ENODATA && !has_timings)
		return fail("TIMINGS cap not set, but got wrong error code (%d)\n", ret);

	if (!ret) {
		fail_on_test(timings.type != V4L2_DV_BT_656_1120);
		fail_on_test(check_0(timings.bt.reserved, sizeof(timings.bt.reserved)));
	}

	for (i = 0; ; i++) {
		memset(&enumtimings, 0xff, sizeof(enumtimings));

		enumtimings.index = i;
		enumtimings.pad = 0;
		ret = doioctl(node, VIDIOC_ENUM_DV_TIMINGS, &enumtimings);
		if (ret)
			break;
		if (check_0(enumtimings.reserved, sizeof(enumtimings.reserved)))
			return fail("reserved not zeroed\n");
		fail_on_test(check_0(enumtimings.timings.bt.reserved,
				     sizeof(enumtimings.timings.bt.reserved)));
		if (enumtimings.index != i)
			return fail("index changed!\n");
		memset(enumtimings.timings.bt.reserved, 0xff,
		       sizeof(enumtimings.timings.bt.reserved));
		fail_on_test(doioctl(node, VIDIOC_S_DV_TIMINGS, &enumtimings.timings));
		fail_on_test(check_0(enumtimings.timings.bt.reserved,
				     sizeof(enumtimings.timings.bt.reserved)));
		
		struct v4l2_dv_timings g_timings;
		fail_on_test(doioctl(node, VIDIOC_G_DV_TIMINGS, &g_timings));
		fail_on_test(g_timings.bt.width != enumtimings.timings.bt.width);
		fail_on_test(g_timings.bt.height != enumtimings.timings.bt.height);

		if (node->is_vbi)
			continue;
		fmt.type = type;
		fail_on_test(doioctl(node, VIDIOC_G_FMT, &fmt));

		unsigned field = is_mplane ? fmt.fmt.pix_mp.field : fmt.fmt.pix.field;

		if (enumtimings.timings.bt.interlaced)
			fail_on_test(field == V4L2_FIELD_NONE);
		else
			fail_on_test(field != V4L2_FIELD_NONE);

		unsigned factor = V4L2_FIELD_HAS_T_OR_B(field) ? 2 : 1;

		// Test if the new format after setting new timings is either too small
		// or more than 1.5 times the width/height. This allows for some
		// adjustments to be made to the format, but still detect if the
		// format isn't updated by the driver.
		if (is_mplane) {
			fail_on_test(fmt.fmt.pix_mp.width < enumtimings.timings.bt.width);
			fail_on_test(fmt.fmt.pix_mp.width >= enumtimings.timings.bt.width * 1.5);
			fail_on_test(fmt.fmt.pix_mp.height * factor < enumtimings.timings.bt.height);
			fail_on_test(fmt.fmt.pix_mp.height * factor >= enumtimings.timings.bt.height * 1.5);
		} else {
			fail_on_test(fmt.fmt.pix.width < enumtimings.timings.bt.width);
			fail_on_test(fmt.fmt.pix.width >= enumtimings.timings.bt.width * 1.5);
			fail_on_test(fmt.fmt.pix.height * factor < enumtimings.timings.bt.height);
			fail_on_test(fmt.fmt.pix.height * factor >= enumtimings.timings.bt.height * 1.5);
		}
	}
	if (i == 0 && has_timings)
		return fail("TIMINGS cap set, but no timings can be enumerated\n");
	if (i && !has_timings)
		return fail("TIMINGS cap was not set, but timings can be enumerated\n");
	if (ret != ENOTTY && ret != ENODATA && !has_timings)
		return fail("TIMINGS cap not set, but got wrong error code for enumeration (%d)\n", ret);
	if (has_timings)
		fail_on_test(doioctl(node, VIDIOC_S_DV_TIMINGS, &timings));
	ret = doioctl(node, VIDIOC_QUERY_DV_TIMINGS, &timings);
	if (!ret)
		fail_on_test(check_0(timings.bt.reserved, sizeof(timings.bt.reserved)));
	if (!ret && !has_timings)
		return fail("TIMINGS cap was not set, but could still query timings\n");
	if (ret != ENOTTY && ret != ENODATA && !has_timings)
		return fail("TIMINGS cap not set, but got wrong error code for query (%d)\n", ret);
	if (ret != ENOTTY && !is_input)
		return fail("this is an output, but could still query timings\n");
	return 0;
}

static int checkSubDevEnumTimings(struct node *node, __u32 pad)
{
	struct v4l2_enum_dv_timings enumtimings;
	struct v4l2_dv_timings timings;
	bool found_fmt_mismatch = false;
	bool has_timings;
	int ret;

	memset(&timings, 0xff, sizeof(timings));
	ret = doioctl(node, VIDIOC_G_DV_TIMINGS, &timings);
	has_timings = ret != ENOTTY;
	fail_on_test(has_timings && ret);
	if (has_timings) {
		fail_on_test(check_0(timings.bt.reserved, sizeof(timings.bt.reserved)));
		memset(timings.bt.reserved, 0xff, sizeof(timings.bt.reserved));
		fail_on_test(doioctl(node, VIDIOC_S_DV_TIMINGS, &timings));
		fail_on_test(check_0(timings.bt.reserved, sizeof(timings.bt.reserved)));
	} else {
		fail_on_test(doioctl(node, VIDIOC_S_DV_TIMINGS, &timings) != ENOTTY);
		fail_on_test(doioctl(node, VIDIOC_QUERY_DV_TIMINGS, &timings) != ENOTTY);
	}

	for (unsigned i = 0; ; i++) {
		memset(&enumtimings, 0xff, sizeof(enumtimings));

		enumtimings.index = i;
		enumtimings.pad = pad;
		ret = doioctl(node, VIDIOC_ENUM_DV_TIMINGS, &enumtimings);
		fail_on_test(ret != ENOTTY && !has_timings);
		fail_on_test(ret == ENOTTY && has_timings);
		if (ret == ENOTTY)
			return ret;
		fail_on_test(ret && ret != EINVAL);
		fail_on_test(ret && !i);
		if (ret)
			break;
		fail_on_test(check_0(enumtimings.reserved, sizeof(enumtimings.reserved)));
		fail_on_test(check_0(enumtimings.timings.bt.reserved,
				     sizeof(enumtimings.timings.bt.reserved)));
		fail_on_test(enumtimings.index != i);
		fail_on_test(enumtimings.pad != pad);
		memset(enumtimings.timings.bt.reserved, 0xff,
		       sizeof(enumtimings.timings.bt.reserved));
		fail_on_test(doioctl(node, VIDIOC_S_DV_TIMINGS, &enumtimings.timings));
		fail_on_test(check_0(enumtimings.timings.bt.reserved,
				     sizeof(enumtimings.timings.bt.reserved)));

		struct v4l2_dv_timings g_timings;

		fail_on_test(doioctl(node, VIDIOC_G_DV_TIMINGS, &g_timings));
		fail_on_test(g_timings.bt.width != enumtimings.timings.bt.width);
		fail_on_test(g_timings.bt.height != enumtimings.timings.bt.height);

		if (found_fmt_mismatch)
			continue;

		struct v4l2_subdev_format fmt;

		memset(&fmt, 0, sizeof(fmt));
		fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;

		// Test if the new format after setting new timings is either too small
		// or more than 1.5 times the width/height. This allows for some
		// adjustments to be made to the format, but still detect if the
		// format isn't updated by the driver.
		if (!doioctl(node, VIDIOC_SUBDEV_G_FMT, &fmt)) {
			if (V4L2_FIELD_HAS_T_OR_B(fmt.format.field))
				fmt.format.height *= 2;

			if (fmt.format.width < enumtimings.timings.bt.width) {
				warn("Active width for pad 0 is %u, which is less than the timings width of %u\n",
				     fmt.format.width, enumtimings.timings.bt.width);
				found_fmt_mismatch = true;
			}
			if (fmt.format.width >= enumtimings.timings.bt.width * 1.5) {
				warn("Active width for pad 0 is %u, which is more than 1.5 * the timings width of %u\n",
				     fmt.format.width, enumtimings.timings.bt.width);
				found_fmt_mismatch = true;
			}
			if (fmt.format.height < enumtimings.timings.bt.height) {
				warn("Active height for pad 0 is %u, which is less than the timings height of %u\n",
				     fmt.format.height, enumtimings.timings.bt.height);
				found_fmt_mismatch = true;
			}
			if (fmt.format.height >= enumtimings.timings.bt.height * 1.5) {
				warn("Active height for pad 0 is %u, which is more than 1.5 * the timings height of %u\n",
				     fmt.format.height, enumtimings.timings.bt.height);
				found_fmt_mismatch = true;
			}
		}
	}
	enumtimings.pad = node->entity.pads;
	enumtimings.index = 0;
	fail_on_test(doioctl(node, VIDIOC_ENUM_DV_TIMINGS, &enumtimings) != EINVAL);

	fail_on_test(doioctl(node, VIDIOC_S_DV_TIMINGS, &timings));
	memset(&timings, 0xff, sizeof(timings));
	ret = doioctl(node, VIDIOC_QUERY_DV_TIMINGS, &timings);
	fail_on_test(ret == ENOTTY);
	fail_on_test(ret == ENODATA);
	if (!ret || ret == ERANGE)
		fail_on_test(check_0(timings.bt.reserved, sizeof(timings.bt.reserved)));
	return 0;
}

int testTimings(struct node *node)
{
	int ret;
	unsigned i, o;
	bool has_timings = false;

	if (node->is_subdev()) {
		for (unsigned pad = 0; pad < node->entity.pads; pad++) {
			ret = checkSubDevEnumTimings(node, pad);
			if (!ret)
				has_timings = true;
			fail_on_test(ret && ret != ENOTTY);
		}
		return has_timings ? 0 : ENOTTY;
	}

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
			return fail("Timings check failed for input %d.\n", i);
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

static int checkTimingsCap(struct node *node, unsigned pad, bool has_timings)
{
	struct v4l2_dv_timings_cap timingscap;
	int ret;

	memset(&timingscap, 0xff, sizeof(timingscap));
	timingscap.pad = pad;
	ret = doioctl(node, VIDIOC_DV_TIMINGS_CAP, &timingscap);
	if (node->is_subdev() && ret == ENOTTY)
		return ENOTTY;
	if (ret && has_timings)
		return fail("TIMINGS cap set, but could not get timings caps\n");
	if (!ret && !has_timings)
		return fail("TIMINGS cap not set, but could still get timings caps\n");
	if (ret && !has_timings)
		return 0;
	if (check_0(timingscap.reserved, sizeof(timingscap.reserved)))
		return fail("reserved not zeroed\n");
	fail_on_test(timingscap.pad != pad);
	fail_on_test(timingscap.type != V4L2_DV_BT_656_1120);
	if (check_0(timingscap.bt.reserved, sizeof(timingscap.bt.reserved)))
		return fail("reserved not zeroed\n");
	fail_on_test(timingscap.bt.min_width > timingscap.bt.max_width);
	fail_on_test(timingscap.bt.min_height > timingscap.bt.max_height);
	fail_on_test(timingscap.bt.min_pixelclock > timingscap.bt.max_pixelclock);

	if (!node->is_subdev())
		return 0;

	memset(&timingscap, 0, sizeof(timingscap));
	timingscap.pad = node->is_subdev() ? node->entity.pads : 1;
	fail_on_test(doioctl(node, VIDIOC_DV_TIMINGS_CAP, &timingscap) != EINVAL);
	return 0;
}

int testTimingsCap(struct node *node)
{
	int ret;
	unsigned i, o;
	bool has_timings = false;

	if (node->is_subdev()) {
		for (unsigned pad = 0; pad < node->entity.pads; pad++) {
			bool is_input = node->pads[pad].flags & MEDIA_PAD_FL_SINK;

			ret = checkTimingsCap(node, pad, true);
			if (ret && ret != ENOTTY)
				return fail("EDID check failed for %s pad %u.\n",
					    is_input ? "sink" : "source", pad);
			if (!ret)
				has_timings = true;
		}
		return has_timings ? 0 : ENOTTY;
	}

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
		if (checkTimingsCap(node, 0, input.capabilities & V4L2_IN_CAP_DV_TIMINGS))
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
		if (checkTimingsCap(node, 0, output.capabilities & V4L2_OUT_CAP_DV_TIMINGS))
			return fail("Timings cap check failed for output %d.\n", o);
	}
	return has_timings ? 0 : ENOTTY;
}

static int checkEdid(struct node *node, unsigned pad, bool is_input)
{
	struct v4l2_edid edid;
	__u8 data[256 * 128] = { 0 };
	unsigned blocks;
	bool has_edid;
	int ret;

	memset(edid.reserved, 0xff, sizeof(edid.reserved));
	edid.pad = pad;
	edid.start_block = 0;
	edid.blocks = 0;
	edid.edid = (__u8 *)0xdeadbeef;
	ret = doioctl(node, VIDIOC_G_EDID, &edid);
	if (ret == ENOTTY) {
		memset(&edid, 0, sizeof(edid));
		edid.pad = pad;
		fail_on_test(doioctl(node, VIDIOC_S_EDID, &edid) != ENOTTY);
		return ENOTTY;
	}
	has_edid = ret == 0;
	fail_on_test(ret && ret != EINVAL);
	fail_on_test(!ret && check_0(edid.reserved, sizeof(edid.reserved)));
	fail_on_test(edid.start_block);
	fail_on_test(edid.blocks > 256);
	fail_on_test(!has_edid && edid.blocks);
	fail_on_test(edid.pad != pad);
	blocks = edid.blocks;
	edid.edid = data;
	ret = doioctl(node, VIDIOC_G_EDID, &edid);
	if (!has_edid)
		fail_on_test(ret != EINVAL);
	else
		fail_on_test(ret != 0 && ret != ENODATA);
	fail_on_test(edid.blocks != blocks);

	if (has_edid) {
		edid.start_block = edid.blocks ? edid.blocks : 1;
		ret = doioctl(node, VIDIOC_G_EDID, &edid);
		fail_on_test(ret != EINVAL && ret != ENODATA);
		if (blocks > 1) {
			edid.start_block = 1;
			edid.blocks = blocks;
			fail_on_test(doioctl(node, VIDIOC_G_EDID, &edid));
			fail_on_test(edid.blocks != blocks - 1);
		}

		edid.start_block = 0;
		edid.blocks = 256;
		ret = doioctl(node, VIDIOC_G_EDID, &edid);
		fail_on_test(ret && ret != ENODATA);
		if (!ret)
			fail_on_test(edid.blocks != blocks);
		edid.pad = ~0;
		fail_on_test(doioctl(node, VIDIOC_G_EDID, &edid) != EINVAL);
		edid.pad = pad;
	}

	memset(edid.reserved, 0xff, sizeof(edid.reserved));
	edid.blocks = blocks;
	ret = doioctl(node, VIDIOC_S_EDID, &edid);
	if (ret == ENOTTY) {
		fail_on_test(is_input);
		return 0;
	}
	fail_on_test(!is_input);
	if (!has_edid) {
		fail_on_test(ret != EINVAL);
		return 0;
	}
	fail_on_test(ret);
	fail_on_test(check_0(edid.reserved, sizeof(edid.reserved)));
	edid.blocks = blocks;
	edid.pad = ~0;
	ret = doioctl(node, VIDIOC_S_EDID, &edid);
	fail_on_test(ret != EINVAL);
	if (blocks < 256) {
		edid.blocks = 256;
		edid.pad = pad;
		ret = doioctl(node, VIDIOC_S_EDID, &edid);
		fail_on_test(ret != E2BIG);
		fail_on_test(edid.blocks == 0 || edid.blocks >= 256);
		fail_on_test(edid.pad != pad);
	}
	edid.blocks = blocks;
	edid.pad = pad;
	edid.edid = NULL;
	ret = doioctl(node, VIDIOC_S_EDID, &edid);
	fail_on_test(ret != EFAULT);
	edid.edid = (__u8 *)0xdeadbeef;
	ret = doioctl(node, VIDIOC_S_EDID, &edid);
	fail_on_test(ret != EFAULT);
	return 0;
}

int testEdid(struct node *node)
{
	bool has_edid = false;
	unsigned i, o;
	int ret;

	if (node->is_subdev()) {
		for (unsigned pad = 0; pad < node->entity.pads; pad++) {
			bool is_input = node->pads[pad].flags & MEDIA_PAD_FL_SOURCE;

			ret = checkEdid(node, pad, is_input);
			if (ret && ret != ENOTTY)
				return fail("EDID check failed for %s pad %u.\n",
					    is_input ? "source" : "sink", pad);
			if (!ret)
				has_edid = true;
		}
		return has_edid ? 0 : ENOTTY;
	}

	for (i = 0; i < node->inputs; i++) {
		struct v4l2_input input;

		input.index = i;
		ret = doioctl(node, VIDIOC_ENUMINPUT, &input);
		if (ret)
			return fail("could not enumerate input %d?!\n", i);
		ret = doioctl(node, VIDIOC_S_INPUT, &input.index);
		if (ret)
			return fail("could not select input %d.\n", i);
		ret = checkEdid(node, i, true);
		if (ret && ret != ENOTTY)
			return fail("EDID check failed for input %d.\n", i);
		if (input.capabilities & V4L2_IN_CAP_DV_TIMINGS)
			has_edid = !ret ? true : has_edid;
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
		ret = checkEdid(node, o, false);
		if (ret && ret != ENOTTY)
			return fail("EDID check failed for output %d.\n", o);
		if (output.capabilities & V4L2_OUT_CAP_DV_TIMINGS)
			has_edid = !ret ? true : has_edid;
	}
	return has_edid ? 0 : ENOTTY;
}
