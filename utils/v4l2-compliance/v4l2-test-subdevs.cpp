/*
    V4L2 API subdev ioctl tests.

    Copyright (C) 2018  Hans Verkuil <hans.verkuil@cisco.com>

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
#include <assert.h>

#include "v4l2-compliance.h"

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
	fie.code = code;
	fie.width = width;
	fie.height = height;
	ret = doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fie);
	node->has_subdev_enum_fival |= (ret != ENOTTY) << which;
	if (ret == ENOTTY)
		return ret;
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
				   unsigned pad, unsigned code)
{
	struct v4l2_subdev_frame_size_enum fse;
	unsigned num_sizes;
	int ret;

	memset(&fse, 0, sizeof(fse));
	fse.which = which;
	fse.pad = pad;
	fse.code = code;
	ret = doioctl(node, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &fse);
	node->has_subdev_enum_fsize |= (ret != ENOTTY) << which;
	if (ret == ENOTTY) {
		struct v4l2_subdev_frame_interval_enum fie;

		memset(&fie, 0, sizeof(fie));
		fie.which = which;
		fie.pad = pad;
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

int testSubDevEnum(struct node *node, unsigned which, unsigned pad)
{
	struct v4l2_subdev_mbus_code_enum mbus_core_enum;
	unsigned num_codes;
	int ret;

	memset(&mbus_core_enum, 0, sizeof(mbus_core_enum));
	mbus_core_enum.which = which;
	mbus_core_enum.pad = pad;
	ret = doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum);
	node->has_subdev_enum_code |= (ret != ENOTTY) << which;
	if (ret == ENOTTY) {
		struct v4l2_subdev_frame_size_enum fse;
		struct v4l2_subdev_frame_interval_enum fie;

		memset(&fse, 0, sizeof(fse));
		memset(&fie, 0, sizeof(fie));
		fse.which = which;
		fse.pad = pad;
		fie.which = which;
		fie.pad = pad;
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
	mbus_core_enum.index = 0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum) != EINVAL);
	memset(&mbus_core_enum, 0xff, sizeof(mbus_core_enum));
	mbus_core_enum.which = which;
	mbus_core_enum.pad = pad;
	mbus_core_enum.index = 0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum));
	fail_on_test(check_0(mbus_core_enum.reserved, sizeof(mbus_core_enum.reserved)));
	fail_on_test(mbus_core_enum.code == ~0U);
	fail_on_test(mbus_core_enum.pad != pad);
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
		fail_on_test(mbus_core_enum.index != i);

		ret = testSubDevEnumFrameSize(node, which, pad, mbus_core_enum.code);
		fail_on_test(ret && ret != ENOTTY);
	}
	return 0;
}
