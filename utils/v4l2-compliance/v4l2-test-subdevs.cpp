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
#include <linux/v4l2-subdev.h>
#include "v4l2-compliance.h"

int testSubDevEnum(struct node *node, unsigned pad)
{
	struct v4l2_subdev_mbus_code_enum mbus_core_enum;
	int ret;

	memset(&mbus_core_enum, 0, sizeof(mbus_core_enum));
	ret = doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum);
	if (ret == ENOTTY)
		return ret;
	mbus_core_enum.which = ~0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum) != EINVAL);
	mbus_core_enum.which = V4L2_SUBDEV_FORMAT_TRY;
	mbus_core_enum.index = ~0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum) != EINVAL);
	mbus_core_enum.pad = ~0;
	mbus_core_enum.index = 0;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum) != EINVAL);
	memset(&mbus_core_enum, 0xff, sizeof(mbus_core_enum));
	mbus_core_enum.pad = pad;
	mbus_core_enum.index = 0;
	mbus_core_enum.which = V4L2_SUBDEV_FORMAT_TRY;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum));
	fail_on_test(check_0(mbus_core_enum.reserved, sizeof(mbus_core_enum.reserved)));
	fail_on_test(mbus_core_enum.code == ~0U);
	fail_on_test(mbus_core_enum.pad != pad);
	fail_on_test(mbus_core_enum.index);
	fail_on_test(mbus_core_enum.which != V4L2_SUBDEV_FORMAT_TRY);
	mbus_core_enum.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	fail_on_test(doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum));
	fail_on_test(mbus_core_enum.which != V4L2_SUBDEV_FORMAT_ACTIVE);
	do {
		mbus_core_enum.index++;
		ret = doioctl(node, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_core_enum);
	} while (!ret);
	fail_on_test(ret != EINVAL);
	
	return 0;
}
