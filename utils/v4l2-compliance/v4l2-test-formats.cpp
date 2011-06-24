/*
    V4L2 API format ioctl tests.

    Copyright (C) 2011  Hans Verkuil <hans.verkuil@cisco.com>

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


static int testEnumFrameIntervals(struct node *node, __u32 pixfmt, __u32 w, __u32 h, bool valid)
{
	struct v4l2_frmivalenum frmival;
	struct v4l2_frmival_stepwise *sw = &frmival.stepwise;
	bool found_stepwise = false;
	unsigned f = 0;
	int ret;

	for (;;) {
		memset(&frmival, 0xff, sizeof(frmival));
		frmival.index = f;
		frmival.pixel_format = pixfmt;
		frmival.width = w;
		frmival.height = h;

		ret = doioctl(node, VIDIOC_ENUM_FRAMEINTERVALS, &frmival);
		if (f == 0 && ret == EINVAL) {
			if (valid)
				warn("found framesize %dx%d, but no frame intervals\n", w, h);
			return -ENOSYS;
		}
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("expected EINVAL, but got %d when enumerating frameinterval %d\n", ret, f);
		ret = check_0(frmival.reserved, sizeof(frmival.reserved));
		if (ret)
			return fail("frmival.reserved not zeroed\n");
		if (frmival.pixel_format != pixfmt || frmival.index != f ||
				frmival.width != w || frmival.height != h)
			return fail("frmival.pixel_format, index, width or height changed\n");
		switch (frmival.type) {
		case V4L2_FRMIVAL_TYPE_DISCRETE:
			ret = check_fract(&frmival.discrete);
			if (found_stepwise)
				return fail("mixing discrete and stepwise is not allowed\n");
			break;
		case V4L2_FRMIVAL_TYPE_CONTINUOUS:
			if (sw->step.numerator != 1 || sw->step.denominator != 1)
				return fail("invalid step for continuous frameinterval\n");
			/* fallthrough */
		case V4L2_FRMIVAL_TYPE_STEPWISE:
			if (frmival.index)
				return fail("index must be 0 for stepwise/continuous frameintervals\n");
			found_stepwise = true;
			ret = check_fract(&sw->min);
			if (ret == 0)
				ret = check_fract(&sw->max);
			if (ret == 0)
				ret = check_fract(&sw->step);
			if (ret)
				return fail("invalid min, max or step for frameinterval %d\n", f);
			if (fract2f(&sw->min) > fract2f(&sw->max))
				return fail("min > max\n");
			if (fract2f(&sw->step) > fract2f(&sw->max) - fract2f(&sw->min))
				return fail("step > (max - min)\n");
			break;
		default:
			return fail("frmival.type is invalid\n");
		}
		
		f++;
	}
	if (!valid)
		return fail("found frame intervals for invalid size %dx%d\n", w, h);
	info("found %d frameintervals for pixel format %08x and size %dx%d\n", f, pixfmt, w, h);
	return 0;
}

static int testEnumFrameSizes(struct node *node, __u32 pixfmt)
{
	struct v4l2_frmsizeenum frmsize;
	struct v4l2_frmsize_stepwise *sw = &frmsize.stepwise;
	bool found_stepwise = false;
	unsigned f = 0;
	int ret;

	for (;;) {
		memset(&frmsize, 0xff, sizeof(frmsize));
		frmsize.index = f;
		frmsize.pixel_format = pixfmt;

		ret = doioctl(node, VIDIOC_ENUM_FRAMESIZES, &frmsize);
		if (f == 0 && ret == EINVAL)
			return -ENOSYS;
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("expected EINVAL, but got %d when enumerating framesize %d\n", ret, f);
		ret = check_0(frmsize.reserved, sizeof(frmsize.reserved));
		if (ret)
			return fail("frmsize.reserved not zeroed\n");
		if (frmsize.pixel_format != pixfmt || frmsize.index != f)
			return fail("frmsize.pixel_format or index changed\n");
		switch (frmsize.type) {
		case V4L2_FRMSIZE_TYPE_DISCRETE:
			if (frmsize.discrete.width == 0 || frmsize.discrete.height == 0)
				return fail("invalid width/height for discrete framesize\n");
			if (found_stepwise)
				return fail("mixing discrete and stepwise is not allowed\n");
			ret = testEnumFrameIntervals(node, pixfmt,
					frmsize.discrete.width, frmsize.discrete.height, true);
			if (ret > 0)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					frmsize.discrete.width + 1, frmsize.discrete.height, false);
			if (ret > 0)
				return ret;
			break;
		case V4L2_FRMSIZE_TYPE_CONTINUOUS:
			if (frmsize.stepwise.step_width != 1 || frmsize.stepwise.step_height != 1)
				return fail("invalid step_width/height for continuous framesize\n");
			/* fallthrough */
		case V4L2_FRMSIZE_TYPE_STEPWISE:
			if (frmsize.index)
				return fail("index must be 0 for stepwise/continuous framesizes\n");
			found_stepwise = true;
			if (!sw->min_width || !sw->min_height || !sw->step_width || !sw->step_height)
				return fail("0 for min_width/height or step_width/height\n");
			if (sw->min_width > sw->max_width || sw->min_height > sw->max_height)
				return fail("min_width/height > max_width/height\n");
			if (sw->step_width > sw->max_width - sw->min_width ||
			    sw->step_height > sw->max_height - sw->min_height)
				return fail("step > max - min for width or height\n");
			ret = testEnumFrameIntervals(node, pixfmt,
					sw->min_width, sw->min_height, true);
			if (ret > 0)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					sw->max_width, sw->max_height, true);
			if (ret > 0)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					sw->min_width - 1, sw->min_height, false);
			if (ret > 0)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					sw->max_width, sw->max_height + 1, false);
			if (ret > 0)
				return ret;
			break;
		default:
			return fail("frmsize.type is invalid\n");
		}
		
		f++;
	}
	info("found %d framesizes for pixel format %08x\n", f, pixfmt);
	return 0;
}

static int testEnumFormatsType(struct node *node, enum v4l2_buf_type type)
{
	struct v4l2_fmtdesc fmtdesc;
	unsigned f = 0;
	int ret;

	for (;;) {
		memset(&fmtdesc, 0xff, sizeof(fmtdesc));
		fmtdesc.type = type;
		fmtdesc.index = f;

		ret = doioctl(node, VIDIOC_ENUM_FMT, &fmtdesc);
		if (f == 0 && ret == EINVAL)
			return -ENOSYS;
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("expected EINVAL, but got %d when enumerating buftype %d\n", ret, type);
		ret = check_0(fmtdesc.reserved, sizeof(fmtdesc.reserved));
		if (ret)
			return fail("fmtdesc.reserved not zeroed\n");
		if (fmtdesc.index != f)
			return fail("fmtdesc.index was modified\n");
		ret = check_ustring(fmtdesc.description, sizeof(fmtdesc.description));
		if (ret)
			return fail("fmtdesc.description not set\n");
		if (!fmtdesc.pixelformat)
			return fail("fmtdesc.pixelformat not set\n");
		if (!wrapper && (fmtdesc.flags & V4L2_FMT_FLAG_EMULATED))
			return fail("drivers must never set the emulated flag\n");
		if (fmtdesc.flags & ~(V4L2_FMT_FLAG_COMPRESSED | V4L2_FMT_FLAG_EMULATED))
			return fail("unknown flag %08x returned\n", fmtdesc.flags);
		ret = testEnumFrameSizes(node, fmtdesc.pixelformat);
		if (ret > 0)
			return ret;
		if (ret == 0 && !(node->caps & (V4L2_CAP_VIDEO_CAPTURE | V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)))
			return fail("found framesizes when no video capture is supported\n");
		f++;
	}
	info("found %d formats for buftype %d\n", f, type);
	return 0;
}

int testEnumFormats(struct node *node)
{
	int ret;

	ret = testEnumFormatsType(node, (enum v4l2_buf_type)0);
	if (ret != -ENOSYS)
		return fail("Buffer type 0 accepted!\n");

	ret = testEnumFormatsType(node, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (ret > 0)
		return ret;
	if (ret && (node->caps & V4L2_CAP_VIDEO_CAPTURE))
		return fail("Video capture cap set, but no capture formats defined\n");
	if (!ret && !(node->caps & V4L2_CAP_VIDEO_CAPTURE))
		return fail("Video capture cap not set, but capture formats defined\n");

	ret = testEnumFormatsType(node, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (ret > 0)
		return ret;
	if (ret && (node->caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE))
		return fail("MPlane Video capture cap set, but no multiplanar capture formats defined\n");
	if (!ret && !(node->caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE))
		return fail("MPlane Video capture cap not set, but multiplanar capture formats defined\n");

	ret = testEnumFormatsType(node, V4L2_BUF_TYPE_VIDEO_OVERLAY);
	if (ret > 0)
		return ret;
	if (ret && (node->caps & V4L2_CAP_VIDEO_OVERLAY))
		return fail("Video overlay cap set, but no overlay formats defined\n");
	if (!ret && !(node->caps & V4L2_CAP_VIDEO_OVERLAY))
		return fail("Video overlay cap not set, but overlay formats defined\n");

	ret = testEnumFormatsType(node, V4L2_BUF_TYPE_VBI_CAPTURE);
	if (ret > 0)
		return ret;
	if (!ret)
		return fail("Buffer type VBI_CAPTURE allowed!\n");

	ret = testEnumFormatsType(node, V4L2_BUF_TYPE_SLICED_VBI_CAPTURE);
	if (ret > 0)
		return ret;
	if (!ret)
		return fail("Buffer type SLICED_VBI_CAPTURE allowed!\n");

	ret = testEnumFormatsType(node, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	if (ret > 0)
		return ret;
	if (ret && (node->caps & V4L2_CAP_VIDEO_OUTPUT))
		return fail("Video output cap set, but no output formats defined\n");
	if (!ret && !(node->caps & V4L2_CAP_VIDEO_OUTPUT))
		return fail("Video output cap not set, but output formats defined\n");

	ret = testEnumFormatsType(node, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	if (ret > 0)
		return ret;
	if (ret && (node->caps & V4L2_CAP_VIDEO_OUTPUT_MPLANE))
		return fail("MPlane Video output cap set, but no multiplanar output formats defined\n");
	if (!ret && !(node->caps & V4L2_CAP_VIDEO_OUTPUT_MPLANE))
		return fail("MPlane Video output cap not set, but multiplanar output formats defined\n");

	ret = testEnumFormatsType(node, V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY);
	if (ret > 0)
		return ret;
	if (!ret)
		return fail("Buffer type VIDEO_OUTPUT_OVERLAY allowed!\n");

	ret = testEnumFormatsType(node, V4L2_BUF_TYPE_VBI_OUTPUT);
	if (ret > 0)
		return ret;
	if (!ret)
		return fail("Buffer type VBI_OUTPUT allowed!\n");

	ret = testEnumFormatsType(node, V4L2_BUF_TYPE_SLICED_VBI_OUTPUT);
	if (ret > 0)
		return ret;
	if (!ret)
		return fail("Buffer type SLICED_VBI_OUTPUT allowed!\n");

	ret = testEnumFormatsType(node, V4L2_BUF_TYPE_PRIVATE);
	if (ret > 0)
		return ret;
	if (!ret)
		warn("Buffer type PRIVATE allowed!\n");
		
	ret = testEnumFrameSizes(node, 0x20202020);
	if (ret >= 0)
		return fail("Accepted framesize for invalid format\n");
	ret = testEnumFrameIntervals(node, 0x20202020, 640, 480, false);
	if (ret >= 0)
		return fail("Accepted frameinterval for invalid format\n");
	return 0;
}
