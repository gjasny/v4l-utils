/*
    V4L2 API compliance buffer ioctl tests.

    Copyright (C) 2012  Hans Verkuil <hverkuil@xs4all.nl>

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

int testReqBufs(struct node *node)
{
	struct v4l2_requestbuffers bufs;
	struct v4l2_create_buffers cbufs;
	struct v4l2_format fmt;
	bool can_stream = node->caps & V4L2_CAP_STREAMING;
	bool can_rw = node->caps & V4L2_CAP_READWRITE;
	bool mmap_valid;
	bool userptr_valid;
	int ret;
	unsigned i;
	
	reopen(node);
	memset(&bufs, 0, sizeof(bufs));
	memset(&cbufs, 0, sizeof(cbufs));
	ret = doioctl(node, VIDIOC_REQBUFS, &bufs);
	if (ret == ENOTTY) {
		fail_on_test(can_stream);
		return ret;
	}
	fail_on_test(ret != EINVAL);
	fail_on_test(node->node2 == NULL);
	for (i = 1; i <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; i++) {
		if (node->buftype_pixfmts[i].empty())
			continue;
		info("test buftype %d\n", i);
		memset(&bufs, 0, sizeof(bufs));
		memset(&cbufs, 0, sizeof(cbufs));
		if (node->valid_buftype == 0)
			node->valid_buftype = i;
		fmt.type = i;
		fail_on_test(doioctl(node, VIDIOC_G_FMT, &fmt));
		bufs.type = fmt.type;
		fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs) != EINVAL);
		bufs.memory = V4L2_MEMORY_MMAP;
		ret = doioctl(node, VIDIOC_REQBUFS, &bufs);
		fail_on_test(ret && ret != EINVAL);
		mmap_valid = !ret;
		bufs.memory = V4L2_MEMORY_USERPTR;
		ret = doioctl(node, VIDIOC_REQBUFS, &bufs);
		fail_on_test(ret && ret != EINVAL);
		userptr_valid = !ret;
		fail_on_test(can_stream && !mmap_valid && !userptr_valid);
		fail_on_test(!can_stream && (mmap_valid || userptr_valid));
		if (!can_stream)
			continue;

		if (mmap_valid) {
			bufs.count = 1;
			bufs.memory = V4L2_MEMORY_MMAP;
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
			fail_on_test(bufs.count == 0);
			fail_on_test(bufs.memory != V4L2_MEMORY_MMAP);
			fail_on_test(bufs.type != i);
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
		}
		if (userptr_valid) {
			bufs.count = 1;
			bufs.memory = V4L2_MEMORY_USERPTR;
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
			fail_on_test(bufs.count == 0);
			fail_on_test(bufs.memory != V4L2_MEMORY_USERPTR);
			fail_on_test(bufs.type != i);
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
		}

		if (can_rw) {
			char buf = 0;

			if (node->can_capture)
				ret = read(node->fd, &buf, 1);
			else
				ret = write(node->fd, &buf, 1);
			fail_on_test(ret != -1);
			fail_on_test(errno != EBUSY);
		}
		if (!node->is_m2m) {
			bufs.count = 1;
			fail_on_test(doioctl(node->node2, VIDIOC_REQBUFS, &bufs) != EBUSY);
			bufs.count = 0;
			fail_on_test(doioctl(node->node2, VIDIOC_REQBUFS, &bufs) != EBUSY);
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
			bufs.count = 1;
			fail_on_test(doioctl(node->node2, VIDIOC_REQBUFS, &bufs));
			bufs.count = 0;
			fail_on_test(doioctl(node->node2, VIDIOC_REQBUFS, &bufs));
		}
		cbufs.format = fmt;
		cbufs.count = 1;
		cbufs.memory = bufs.memory;
		ret = doioctl(node, VIDIOC_CREATE_BUFS, &cbufs);
		if (ret == ENOTTY) {
			warn("VIDIOC_CREATE_BUFS not supported\n");
			continue;
		}
		fail_on_test(cbufs.count == 0);
		fail_on_test(cbufs.memory != bufs.memory);
		fail_on_test(cbufs.format.type != i);
		cbufs.count = 1;
		fail_on_test(doioctl(node, VIDIOC_CREATE_BUFS, &cbufs));
		if (!node->is_m2m) {
			bufs.count = 1;
			fail_on_test(doioctl(node->node2, VIDIOC_REQBUFS, &bufs) != EBUSY);
		}
		bufs.count = 0;
		fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
	}
	return 0;
}

int testReadWrite(struct node *node)
{
	bool can_rw = node->caps & V4L2_CAP_READWRITE;
	char buf = 0;
	int ret;

	if (node->can_capture)
		ret = read(node->fd, &buf, 1);
	else
		ret = write(node->fd, &buf, 1);
	// Note: RDS can only return multiples of 3, so we accept
	// both 0 and 1 as return code.
	if (can_rw)
		fail_on_test(ret != 0 && ret != 1);
	else
		fail_on_test(ret < 0 && errno != EINVAL);
	if (!can_rw)
		return 0;

	reopen(node);

	/* check that the close cleared the busy flag */
	if (node->can_capture)
		ret = read(node->fd, &buf, 1);
	else
		ret = write(node->fd, &buf, 1);
	fail_on_test(ret != 0 && ret != 1);
	reopen(node);
	return 0;
}
