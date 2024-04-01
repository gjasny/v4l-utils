/*
    V4L2 API subdev ioctl tests.

    Copyright (C) 2018  Hans Verkuil <hverkuil-cisco@xs4all.nl>

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

#include <map>
#include <set>

#include <sys/types.h>

#include "v4l2-compliance.h"

#define VALID_SUBDEV_CAPS (V4L2_SUBDEV_CAP_RO_SUBDEV | V4L2_SUBDEV_CAP_STREAMS)

int testSubDevCap(struct node *node)
{
	v4l2_subdev_capability caps;

	memset(&caps, 0xff, sizeof(caps));
	// Must always be there
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_QUERYCAP, &caps));
	if (has_mmu)
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_QUERYCAP, nullptr) != EFAULT);
	fail_on_test(check_0(caps.reserved, sizeof(caps.reserved)));
	fail_on_test((caps.version >> 16) < 5);
	fail_on_test(caps.capabilities & ~VALID_SUBDEV_CAPS);
	node->is_ro_subdev = caps.capabilities & V4L2_SUBDEV_CAP_RO_SUBDEV;
	return 0;
}

static int testSubDevEnumFrameInterval(struct node *node, unsigned which,
				       unsigned pad, unsigned code,
				       unsigned width, unsigned height)
{
	struct v4l2_subdev_frame_interval_enum fie;
	unsigned num_ivals;
	int ret;

	memset(&fie, 0, sizeof(fie));
	fie.which = which;
	fie.pad = pad;
	fie.stream = 0;
	fie.code = code;
	fie.width = width;
	fie.height = height;
	ret = doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fie);
	node->has_subdev_enum_fival |= (ret != ENOTTY) << which;
	if (ret == ENOTTY)
		return ret;
	if (which)
		fail_on_test(node->enum_frame_interval_pad != (int)pad);
	else
		fail_on_test(node->enum_frame_interval_pad >= 0);
	node->enum_frame_interval_pad = pad;
	fie.which = ~0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fie) != EINVAL);
	fie.which = which;
	fie.index = ~0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fie) != EINVAL);
	fie.pad = node->entity.pads;
	fie.index = 0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fie) != EINVAL);
	fie.width = ~0;
	fie.pad = pad;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fie) != EINVAL);
	fie.height = ~0;
	fie.width = width;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fie) != EINVAL);
	memset(&fie, 0xff, sizeof(fie));
	fie.which = which;
	fie.pad = pad;
	fie.stream = 0;
	fie.code = code;
	fie.width = width;
	fie.height = height;
	fie.index = 0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fie));
	fail_on_test(check_0(fie.reserved, sizeof(fie.reserved)));
	fail_on_test(fie.which != which);
	fail_on_test(fie.pad != pad);
	fail_on_test(fie.code != code);
	fail_on_test(fie.width != width);
	fail_on_test(fie.height != height);
	fail_on_test(fie.index);
	fail_on_test(fie.interval.numerator == ~0U || fie.interval.denominator == ~0U);
	do {
		fie.index++;
		ret = doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fie);
	} while (!ret);
	fail_on_test(ret != EINVAL);

	num_ivals = fie.index;
	for (unsigned i = 0; i < num_ivals; i++) {
		fie.index = i;
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fie));
		fail_on_test(fie.which != which);
		fail_on_test(fie.pad != pad);
		fail_on_test(fie.code != code);
		fail_on_test(fie.width != width);
		fail_on_test(fie.height != height);
		fail_on_test(fie.index != i);
		fail_on_test(!fie.interval.numerator);
		fail_on_test(!fie.interval.denominator);
	}
	return 0;
}

static int testSubDevEnumFrameSize(struct node *node, unsigned which,
				   unsigned pad, unsigned stream, unsigned code)
{
	struct v4l2_subdev_frame_size_enum fse;
	unsigned num_sizes;
	int ret;

	memset(&fse, 0, sizeof(fse));
	fse.which = which;
	fse.pad = pad;
	fse.stream = stream;
	fse.code = code;
	ret = doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &fse);
	node->has_subdev_enum_fsize |= (ret != ENOTTY) << which;
	if (ret == ENOTTY) {
		struct v4l2_subdev_frame_interval_enum fie;

		memset(&fie, 0, sizeof(fie));
		fie.which = which;
		fie.pad = pad;
		fie.stream = stream;
		fie.code = code;
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fie) != ENOTTY);
		return ret;
	}
	fse.which = ~0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &fse) != EINVAL);
	fse.which = which;
	fse.index = ~0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &fse) != EINVAL);
	fse.pad = node->entity.pads;
	fse.index = 0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &fse) != EINVAL);
	memset(&fse, 0xff, sizeof(fse));
	fse.which = which;
	fse.pad = pad;
	fse.stream = stream;
	fse.code = code;
	fse.index = 0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &fse));
	fail_on_test(check_0(fse.reserved, sizeof(fse.reserved)));
	fail_on_test(fse.which != which);
	fail_on_test(fse.pad != pad);
	fail_on_test(fse.code != code);
	fail_on_test(fse.index);
	fail_on_test(!fse.min_width || !fse.min_height);
	fail_on_test(fse.min_width == ~0U || fse.min_height == ~0U);
	fail_on_test(!fse.max_width || !fse.max_height);
	fail_on_test(fse.max_width == ~0U || fse.max_height == ~0U);
	fail_on_test(fse.min_width > fse.max_width);
	fail_on_test(fse.min_height > fse.max_height);
	do {
		fse.index++;
		ret = doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &fse);
	} while (!ret);
	fail_on_test(ret != EINVAL);

	num_sizes = fse.index;
	for (unsigned i = 0; i < num_sizes; i++) {
		fse.index = i;
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &fse));
		fail_on_test(fse.which != which);
		fail_on_test(fse.pad != pad);
		fail_on_test(fse.code != code);
		fail_on_test(fse.index != i);
		fail_on_test(!fse.min_width || !fse.min_height);
		fail_on_test(!fse.max_width || !fse.max_height);
		fail_on_test(fse.min_width > fse.max_width);
		fail_on_test(fse.min_height > fse.max_height);

		ret = testSubDevEnumFrameInterval(node, which, pad, code,
						  fse.min_width, fse.min_height);
		fail_on_test(ret && ret != ENOTTY);
		ret = testSubDevEnumFrameInterval(node, which, pad, code,
						  fse.max_width, fse.max_height);
		fail_on_test(ret && ret != ENOTTY);
	}
	return 0;
}

int testSubDevEnum(struct node *node, unsigned which, unsigned pad, unsigned stream)
{
	struct v4l2_subdev_mbus_code_enum mbus_core_enum;
	unsigned num_codes;
	int ret;

	memset(&mbus_core_enum, 0, sizeof(mbus_core_enum));
	mbus_core_enum.which = which;
	mbus_core_enum.pad = pad;
	mbus_core_enum.stream = stream;
	ret = doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum);
	node->has_subdev_enum_code |= (ret != ENOTTY) << which;
	if (ret == ENOTTY) {
		struct v4l2_subdev_frame_size_enum fse;
		struct v4l2_subdev_frame_interval_enum fie;

		memset(&fse, 0, sizeof(fse));
		memset(&fie, 0, sizeof(fie));
		fse.which = which;
		fse.pad = pad;
		fse.stream = stream;
		fie.which = which;
		fie.pad = pad;
		fie.stream = stream;
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &fse) != ENOTTY);
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fie) != ENOTTY);
		return ret;
	}
	mbus_core_enum.which = ~0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum) != EINVAL);
	mbus_core_enum.which = which;
	mbus_core_enum.index = ~0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum) != EINVAL);
	mbus_core_enum.pad = node->entity.pads;
	mbus_core_enum.stream = stream;
	mbus_core_enum.index = 0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum) != EINVAL);
	memset(&mbus_core_enum, 0xff, sizeof(mbus_core_enum));
	mbus_core_enum.which = which;
	mbus_core_enum.pad = pad;
	mbus_core_enum.stream = stream;
	mbus_core_enum.index = 0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum));
	fail_on_test(check_0(mbus_core_enum.reserved, sizeof(mbus_core_enum.reserved)));
	fail_on_test(mbus_core_enum.code == ~0U);
	fail_on_test(mbus_core_enum.pad != pad);
	fail_on_test(mbus_core_enum.stream != stream);
	fail_on_test(mbus_core_enum.index);
	fail_on_test(mbus_core_enum.which != which);
	do {
		mbus_core_enum.index++;
		ret = doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum);
	} while (!ret);
	fail_on_test(ret != EINVAL);

	num_codes = mbus_core_enum.index;
	for (unsigned i = 0; i < num_codes; i++) {
		mbus_core_enum.index = i;
		mbus_core_enum.code = 0;
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum));
		fail_on_test(!mbus_core_enum.code);
		fail_on_test(mbus_core_enum.which != which);
		fail_on_test(mbus_core_enum.pad != pad);
		fail_on_test(mbus_core_enum.stream != stream);
		fail_on_test(mbus_core_enum.index != i);

		ret = testSubDevEnumFrameSize(node, which, pad, stream, mbus_core_enum.code);
		fail_on_test(ret && ret != ENOTTY);
	}
	return 0;
}

int testSubDevFrameInterval(struct node *node, unsigned which, unsigned pad, unsigned stream)
{
	struct v4l2_subdev_frame_interval fival;
	struct v4l2_fract ival;
	bool has_which = node->has_ival_uses_which();
	int ret;

	if (!has_which)
		which = V4L2_SUBDEV_FORMAT_ACTIVE;
	memset(&fival, 0xff, sizeof(fival));
	fival.pad = pad;
	fival.stream = stream;
	if (has_which)
		fival.which = which;
	ret = doioctl(node, VIDIOC_SUBDEV_G_FRAME_INTERVAL, &fival);
	if (has_which)
		node->has_subdev_frame_interval |= (ret != ENOTTY) << which;
	if (ret == ENOTTY) {
		fail_on_test(node->enum_frame_interval_pad >= 0);
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &fival) != ENOTTY);
		return ret;
	}

	if (!has_which)
		warn_once("V4L2_SUBDEV_CLIENT_CAP_INTERVAL_USES_WHICH is not supported\n");

	fail_on_test(node->frame_interval_pad >= 0);
	fail_on_test(node->enum_frame_interval_pad != (int)pad);
	node->frame_interval_pad = pad;
	fail_on_test(check_0(fival.reserved, sizeof(fival.reserved)));
	if (has_which)
		fail_on_test(fival.which != which);
	fail_on_test(fival.pad != pad);
	fail_on_test(fival.stream != stream);
	fail_on_test(!fival.interval.numerator);
	fail_on_test(!fival.interval.denominator);
	fail_on_test(fival.interval.numerator == ~0U || fival.interval.denominator == ~0U);
	ival = fival.interval;
	memset(fival.reserved, 0xff, sizeof(fival.reserved));
	if (node->is_ro_subdev) {
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &fival) != EPERM);
		return 0;
	}
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &fival));
	if (has_which)
		fail_on_test(fival.which != which);
	fail_on_test(fival.pad != pad);
	fail_on_test(fival.stream != stream);
	fail_on_test(ival.numerator != fival.interval.numerator);
	fail_on_test(ival.denominator != fival.interval.denominator);
	fail_on_test(check_0(fival.reserved, sizeof(fival.reserved)));
	memset(&fival, 0, sizeof(fival));
	fival.pad = pad;
	fival.stream = stream;
	if (has_which)
		fival.which = which;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_G_FRAME_INTERVAL, &fival));
	if (has_which)
		fail_on_test(fival.which != which);
	fail_on_test(fival.pad != pad);
	fail_on_test(fival.stream != stream);
	fail_on_test(ival.numerator != fival.interval.numerator);
	fail_on_test(ival.denominator != fival.interval.denominator);

	if (has_which) {
		fival.which = ~0;
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_G_FRAME_INTERVAL, &fival) != EINVAL);
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &fival) != EINVAL);
		fival.which = which;
	}
	fival.pad = node->entity.pads;
	fival.stream = stream;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_G_FRAME_INTERVAL, &fival) != EINVAL);
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &fival) != EINVAL);
	fival.pad = pad;
	fival.stream = stream;
	fival.interval = ival;
	fival.interval.numerator = 0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &fival));
	fail_on_test(!fival.interval.numerator);
	fail_on_test(!fival.interval.denominator);
	fival.interval = ival;
	fival.interval.denominator = 0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &fival));
	fail_on_test(!fival.interval.numerator);
	fail_on_test(!fival.interval.denominator);
	fival.interval = ival;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &fival));
	return 0;
}

static int checkMBusFrameFmt(struct node *node, struct v4l2_mbus_framefmt &fmt)
{
	fail_on_test(check_0(fmt.reserved, sizeof(fmt.reserved)));
	fail_on_test(fmt.width == 0 || fmt.width > 65536);
	fail_on_test(fmt.height == 0 || fmt.height > 65536);
	fail_on_test(fmt.code == 0 || fmt.code == ~0U);
	fail_on_test(fmt.field == ~0U);
	if (!node->is_passthrough_subdev) {
		// Passthrough subdevs just copy this info, they don't validate
		// it. TODO: this does not take colorspace converters into account!
		fail_on_test(fmt.colorspace == ~0U);
		//TBD fail_on_test(!fmt.colorspace);
		fail_on_test(fmt.ycbcr_enc == 0xffff);
		fail_on_test(fmt.quantization == 0xffff);
		fail_on_test(fmt.xfer_func == 0xffff);
		fail_on_test(!fmt.colorspace &&
			     (fmt.ycbcr_enc || fmt.quantization || fmt.xfer_func));
	}
	return 0;
}

int testSubDevFormat(struct node *node, unsigned which, unsigned pad, unsigned stream)
{
	struct v4l2_subdev_format fmt;
	struct v4l2_subdev_format s_fmt;
	int ret;

	memset(&fmt, 0, sizeof(fmt));
	fmt.which = which;
	fmt.pad = pad;
	fmt.stream = stream;
	ret = doioctl(node, VIDIOC_SUBDEV_G_FMT, &fmt);
	node->has_subdev_fmt |= (ret != ENOTTY) << which;
	if (ret == ENOTTY) {
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_FMT, &fmt) != ENOTTY);
		return ret;
	}
	fmt.which = ~0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_G_FMT, &fmt) != EINVAL);
	fmt.which = 0;
	fmt.pad = node->entity.pads;
	fmt.stream = stream;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_G_FMT, &fmt) != EINVAL);
	memset(&fmt, 0xff, sizeof(fmt));
	fmt.which = which;
	fmt.pad = pad;
	fmt.stream = stream;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_G_FMT, &fmt));
	fail_on_test(check_0(fmt.reserved, sizeof(fmt.reserved)));
	fail_on_test(fmt.which != which);
	fail_on_test(fmt.pad != pad);
	fail_on_test(fmt.stream != stream);
	fail_on_test(checkMBusFrameFmt(node, fmt.format));
	s_fmt = fmt;
	memset(s_fmt.reserved, 0xff, sizeof(s_fmt.reserved));
	memset(s_fmt.format.reserved, 0xff, sizeof(s_fmt.format.reserved));
	ret = doioctl(node, VIDIOC_SUBDEV_S_FMT, &s_fmt);
	if (node->is_ro_subdev && which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		fail_on_test(ret != EPERM);
		return 0;
	}
	fail_on_test(ret && ret != ENOTTY);
	fail_on_test(s_fmt.which != which);
	fail_on_test(s_fmt.pad != pad);
	fail_on_test(s_fmt.stream != stream);
	if (ret) {
		warn("VIDIOC_SUBDEV_G_FMT is supported but not VIDIOC_SUBDEV_S_FMT\n");
		return 0;
	}
	fail_on_test(check_0(s_fmt.reserved, sizeof(s_fmt.reserved)));
	fail_on_test(checkMBusFrameFmt(node, s_fmt.format));
	fail_on_test(s_fmt.format.width != fmt.format.width);
	fail_on_test(s_fmt.format.height != fmt.format.height);
	fail_on_test(s_fmt.format.code != fmt.format.code);
	fail_on_test(s_fmt.format.field != fmt.format.field);
	fail_on_test(s_fmt.format.colorspace != fmt.format.colorspace);
	fail_on_test(s_fmt.format.ycbcr_enc != fmt.format.ycbcr_enc);
	fail_on_test(s_fmt.format.quantization != fmt.format.quantization);
	fail_on_test(s_fmt.format.xfer_func != fmt.format.xfer_func);

	s_fmt.format.code = ~0U;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_FMT, &s_fmt));
	fail_on_test(s_fmt.format.code == ~0U);
	fail_on_test(!s_fmt.format.code);

	// Restore fmt
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_FMT, &fmt));
	return 0;
}

struct target_info {
	__u32 target;
	bool allowed;
	bool readonly;
	bool found;
};

static target_info targets[] = {
	{ V4L2_SEL_TGT_CROP, true },
	{ V4L2_SEL_TGT_CROP_DEFAULT, true, true },
	{ V4L2_SEL_TGT_CROP_BOUNDS, true, true },
	{ V4L2_SEL_TGT_NATIVE_SIZE, true },
	{ V4L2_SEL_TGT_COMPOSE, true },
	{ V4L2_SEL_TGT_COMPOSE_DEFAULT, false, true },
	{ V4L2_SEL_TGT_COMPOSE_BOUNDS, true, true },
	{ V4L2_SEL_TGT_COMPOSE_PADDED, false, true },
	{ ~0U },
};

int testSubDevSelection(struct node *node, unsigned which, unsigned pad, unsigned stream)
{
	struct v4l2_subdev_selection sel;
	struct v4l2_subdev_selection s_sel;
	struct v4l2_subdev_crop crop;
	bool is_sink = node->pads[pad].flags & MEDIA_PAD_FL_SINK;
	bool have_sel = false;
	int ret;

	targets[V4L2_SEL_TGT_NATIVE_SIZE].readonly = is_sink;
	memset(&crop, 0, sizeof(crop));
	crop.pad = pad;
	crop.stream = stream;
	crop.which = which;
	memset(&sel, 0, sizeof(sel));
	sel.which = which;
	sel.pad = pad;
	sel.stream = stream;
	sel.target = V4L2_SEL_TGT_CROP;
	ret = doioctl(node, VIDIOC_SUBDEV_G_SELECTION, &sel);
	node->has_subdev_selection |= (ret != ENOTTY) << which;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_G_CROP, &crop) != ret);
	if (ret == ENOTTY) {
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_SELECTION, &sel) != ENOTTY);
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_CROP, &crop) != ENOTTY);
		return ret;
	}
	fail_on_test(check_0(crop.reserved, sizeof(crop.reserved)));
	fail_on_test(crop.which != which);
	fail_on_test(crop.pad != pad);
	fail_on_test(crop.stream != stream);
	fail_on_test(memcmp(&crop.rect, &sel.r, sizeof(sel.r)));

	for (unsigned tgt = 0; targets[tgt].target != ~0U; tgt++) {
		targets[tgt].found = false;
		memset(&sel, 0xff, sizeof(sel));
		sel.which = which;
		sel.pad = pad;
		sel.stream = stream;
		sel.target = tgt;
		ret = doioctl(node, VIDIOC_SUBDEV_G_SELECTION, &sel);
		targets[tgt].found = !ret;
		fail_on_test(ret && ret != EINVAL);
		if (ret)
			continue;
		have_sel = true;
		fail_on_test(!targets[tgt].allowed);
		fail_on_test(check_0(sel.reserved, sizeof(sel.reserved)));
		fail_on_test(sel.which != which);
		fail_on_test(sel.pad != pad);
		fail_on_test(sel.stream != stream);
		fail_on_test(sel.target != tgt);
		fail_on_test(!sel.r.width);
		fail_on_test(sel.r.width == ~0U);
		fail_on_test(!sel.r.height);
		fail_on_test(sel.r.height == ~0U);
		fail_on_test(sel.r.top == ~0);
		fail_on_test(sel.r.left == ~0);
		sel.which = ~0;
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_G_SELECTION, &sel) != EINVAL);
		sel.which = 0;
		sel.pad = node->entity.pads;
		sel.stream = stream;
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_G_SELECTION, &sel) != EINVAL);
		sel.which = which;
		sel.pad = pad;
		sel.stream = stream;
		s_sel = sel;
		memset(s_sel.reserved, 0xff, sizeof(s_sel.reserved));
		ret = doioctl(node, VIDIOC_SUBDEV_S_SELECTION, &s_sel);
		if (node->is_ro_subdev && which == V4L2_SUBDEV_FORMAT_ACTIVE)
			fail_on_test(ret != EPERM);
		if (tgt == V4L2_SEL_TGT_CROP) {
			crop.rect = sel.r;
			memset(crop.reserved, 0xff, sizeof(crop.reserved));
			fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_CROP, &crop) != ret);
			if (!ret) {
				fail_on_test(check_0(crop.reserved, sizeof(crop.reserved)));
				fail_on_test(crop.which != which);
				fail_on_test(crop.pad != pad);
				fail_on_test(crop.stream != stream);
				fail_on_test(memcmp(&crop.rect, &sel.r, sizeof(sel.r)));
			}
		}
		if (node->is_ro_subdev && which == V4L2_SUBDEV_FORMAT_ACTIVE)
			continue;
		fail_on_test(!ret && targets[tgt].readonly);
		fail_on_test(s_sel.which != which);
		fail_on_test(s_sel.pad != pad);
		fail_on_test(s_sel.stream != stream);
		if (ret && !targets[tgt].readonly && tgt != V4L2_SEL_TGT_NATIVE_SIZE)
			warn("VIDIOC_SUBDEV_G_SELECTION is supported for target %u but not VIDIOC_SUBDEV_S_SELECTION\n", tgt);
		if (ret)
			continue;
		fail_on_test(check_0(s_sel.reserved, sizeof(s_sel.reserved)));
		fail_on_test(s_sel.flags != sel.flags);
		fail_on_test(s_sel.r.top != sel.r.top);
		fail_on_test(s_sel.r.left != sel.r.left);
		fail_on_test(s_sel.r.width != sel.r.width);
		fail_on_test(s_sel.r.height != sel.r.height);
	}

	return have_sel ? 0 : ENOTTY;
}

int testSubDevRouting(struct node *node, unsigned which)
{
	const uint32_t all_route_flags_mask = V4L2_SUBDEV_ROUTE_FL_ACTIVE;
	struct v4l2_subdev_routing routing = {};
	struct v4l2_subdev_route routes[NUM_ROUTES_MAX] = {};
	unsigned int i;
	int ret;

	routing.which = which;
	routing.routes = (uintptr_t)&routes;
	routing.len_routes = 0;
	routing.num_routes = 0;
	memset(routing.reserved, 0xff, sizeof(routing.reserved));

	/* First test that G_ROUTING returns success even when len_routes is 0. */

	ret = doioctl(node, VIDIOC_SUBDEV_G_ROUTING, &routing);
	fail_on_test(ret);
	fail_on_test(routing.num_routes > NUM_ROUTES_MAX);
	fail_on_test(check_0(routing.reserved, sizeof(routing.reserved)));

	if (!routing.num_routes)
		return 0;

	/*
	 * Then get the actual routes, and verify that the num_routes gets
	 * updated to the correct number.
	 */

	uint32_t num_routes = routing.num_routes;
	routing.len_routes = NUM_ROUTES_MAX;
	routing.num_routes = 0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_G_ROUTING, &routing));
	fail_on_test(routing.num_routes != num_routes);

	/* Check the validity of route pads and flags */

	if (node->pads) {
		for (i = 0; i < routing.num_routes; ++i) {
			const struct v4l2_subdev_route *route = &routes[i];
			const struct media_pad_desc *sink;
			const struct media_pad_desc *source;

			fail_on_test(route->sink_pad >= node->entity.pads);
			fail_on_test(route->source_pad >= node->entity.pads);

			sink = &node->pads[route->sink_pad];
			source = &node->pads[route->source_pad];

			fail_on_test(!(sink->flags & MEDIA_PAD_FL_SINK));
			fail_on_test(!(source->flags & MEDIA_PAD_FL_SOURCE));
			fail_on_test(route->flags & ~all_route_flags_mask);
		}
	}

	/*
	 * Set the same routes back, which should always succeed and report the
	 * same number of routes.
	 */

	memset(routing.reserved, 0xff, sizeof(routing.reserved));
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_ROUTING, &routing));
	fail_on_test(routing.num_routes != num_routes);
	fail_on_test(check_0(routing.reserved, sizeof(routing.reserved)));

	/* Test setting invalid pads */

	if (node->pads) {
		for (i = 0; i < routing.num_routes; ++i) {
			struct v4l2_subdev_route *route = &routes[i];

			route->sink_pad = node->entity.pads + 1;
		}

		memset(routing.reserved, 0xff, sizeof(routing.reserved));
		fail_on_test(doioctl(node, VIDIOC_SUBDEV_S_ROUTING, &routing) != EINVAL);
	}

	return 0;
}
