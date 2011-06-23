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


int testEnumFormatsType(struct node *node, enum v4l2_buf_type type)
{
	struct v4l2_fmtdesc fmtdesc;
	int f = 0;
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
		ret = check_ustring(fmtdesc.description, sizeof(fmtdesc.description));
		if (ret)
			return fail("fmtdesc.description not set\n");
		if (!fmtdesc.pixelformat)
			return fail("fmtdesc.pixelformat not set\n");
		if (!wrapper && (fmtdesc.flags & V4L2_FMT_FLAG_EMULATED))
			return fail("drivers must never set the emulated flag\n");
		if (fmtdesc.flags & ~(V4L2_FMT_FLAG_COMPRESSED | V4L2_FMT_FLAG_EMULATED))
			return fail("unknown flag %08x returned\n", fmtdesc.flags);
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
	return 0;
}
