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
#include <sys/mman.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <map>
#include "v4l2-compliance.h"

static inline void *test_mmap(void *start, size_t length, int prot, int flags,
		int fd, int64_t offset)
{
 	return wrapper ? v4l2_mmap(start, length, prot, flags, fd, offset) :
		mmap(start, length, prot, flags, fd, offset);
}

static void *ptrs[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];
static int dmabufs[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];
static struct v4l2_format cur_fmt;

static const unsigned valid_output_flags =
	V4L2_BUF_FLAG_TIMECODE | V4L2_BUF_FLAG_TSTAMP_SRC_MASK |
	V4L2_BUF_FLAG_KEYFRAME | V4L2_BUF_FLAG_PFRAME | V4L2_BUF_FLAG_BFRAME;

bool operator<(struct timeval const& n1, struct timeval const& n2)
{
	return n1.tv_sec < n2.tv_sec ||
		(n1.tv_sec == n2.tv_sec && n1.tv_usec < n2.tv_usec);
}

typedef std::map<struct timeval, struct v4l2_buffer> buf_info_map;
static buf_info_map buffer_info;

struct buf_seq {
	void init()
	{
		last_seq = -1;
		field_nr = 1;
	}

	int last_seq;
	unsigned last_field;
	unsigned field_nr;
};

static struct buf_seq last_seq, last_m2m_seq;

enum QueryBufMode {
	Unqueued,
	Prepared,
	Queued,
	Dequeued
};

static std::string num2s(unsigned num)
{
	char buf[10];

	sprintf(buf, "%08x", num);
	return buf;
}

static std::string field2s(unsigned val)
{
	switch (val) {
	case V4L2_FIELD_ANY:
		return "Any";
	case V4L2_FIELD_NONE:
		return "None";
	case V4L2_FIELD_TOP:
		return "Top";
	case V4L2_FIELD_BOTTOM:
		return "Bottom";
	case V4L2_FIELD_INTERLACED:
		return "Interlaced";
	case V4L2_FIELD_SEQ_TB:
		return "Sequential Top-Bottom";
	case V4L2_FIELD_SEQ_BT:
		return "Sequential Bottom-Top";
	case V4L2_FIELD_ALTERNATE:
		return "Alternating";
	case V4L2_FIELD_INTERLACED_TB:
		return "Interlaced Top-Bottom";
	case V4L2_FIELD_INTERLACED_BT:
		return "Interlaced Bottom-Top";
	default:
		return "Unknown (" + num2s(val) + ")";
	}
}

static unsigned process_buf(const v4l2_buffer &buf, v4l2_plane *planes)
{
	if (V4L2_TYPE_IS_MULTIPLANAR(buf.type))
		return buf.length;
	planes->bytesused = buf.bytesused;
	planes->length = buf.length;
	planes->m.userptr = buf.m.userptr;
	return 1;
}

static int checkQueryBuf(struct node *node, const struct v4l2_buffer &buf,
		__u32 type, __u32 memory, unsigned index, enum QueryBufMode mode,
		struct buf_seq &seq)
{
	unsigned timestamp = buf.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK;
	bool ts_copy = timestamp == V4L2_BUF_FLAG_TIMESTAMP_COPY;
	unsigned timestamp_src = buf.flags & V4L2_BUF_FLAG_TSTAMP_SRC_MASK;
	unsigned frame_types = 0;
	unsigned buf_states = 0;

	fail_on_test(buf.type != type);
	fail_on_test(buf.memory == 0);
	fail_on_test(buf.memory != memory);
	fail_on_test(buf.index >= VIDEO_MAX_FRAME);
	fail_on_test(buf.index != index);
	fail_on_test(buf.reserved2 || buf.reserved);
	fail_on_test(timestamp != V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC &&
		     timestamp != V4L2_BUF_FLAG_TIMESTAMP_COPY);
	fail_on_test(timestamp_src != V4L2_BUF_FLAG_TSTAMP_SRC_SOE &&
		     timestamp_src != V4L2_BUF_FLAG_TSTAMP_SRC_EOF);
	fail_on_test(!ts_copy && V4L2_TYPE_IS_OUTPUT(buf.type) &&
		     timestamp_src == V4L2_BUF_FLAG_TSTAMP_SRC_SOE);
	if (buf.flags & V4L2_BUF_FLAG_KEYFRAME)
		frame_types++;
	if (buf.flags & V4L2_BUF_FLAG_PFRAME)
		frame_types++;
	if (buf.flags & V4L2_BUF_FLAG_BFRAME)
		frame_types++;
	fail_on_test(frame_types > 1);
	fail_on_test(buf.flags & (V4L2_BUF_FLAG_NO_CACHE_INVALIDATE |
				  V4L2_BUF_FLAG_NO_CACHE_CLEAN));
	if (buf.flags & V4L2_BUF_FLAG_QUEUED)
		buf_states++;
	if (buf.flags & V4L2_BUF_FLAG_DONE)
		buf_states++;
	if (buf.flags & V4L2_BUF_FLAG_ERROR)
		buf_states++;
	if (buf.flags & V4L2_BUF_FLAG_PREPARED)
		buf_states++;
	fail_on_test(buf_states > 1);
	fail_on_test(buf.length == 0);
	if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
		fail_on_test(buf.length > VIDEO_MAX_PLANES);
		for (unsigned p = 0; p < buf.length; p++) {
			struct v4l2_plane *vp = buf.m.planes + p;

			fail_on_test(check_0(vp->reserved, sizeof(vp->reserved)));
			fail_on_test(vp->length == 0);
		}
	}

	if (mode == Dequeued || mode == Prepared) {
		if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
			for (unsigned p = 0; p < buf.length; p++) {
				struct v4l2_plane *vp = buf.m.planes + p;

				if (buf.memory == V4L2_MEMORY_USERPTR)
					fail_on_test((void *)vp->m.userptr != ptrs[buf.index][p]);
				else if (buf.memory == V4L2_MEMORY_DMABUF)
					fail_on_test(vp->m.fd != dmabufs[buf.index][p]);
			}
		} else {
			if (buf.memory == V4L2_MEMORY_USERPTR)
				fail_on_test((void *)buf.m.userptr != ptrs[buf.index][0]);
			else if (buf.memory == V4L2_MEMORY_DMABUF)
				fail_on_test(buf.m.fd != dmabufs[buf.index][0]);
		}
	}

	if (!V4L2_TYPE_IS_OUTPUT(buf.type) && !ts_copy &&
	    (buf.flags & V4L2_BUF_FLAG_TIMECODE))
		warn("V4L2_BUF_FLAG_TIMECODE was used!\n");

	if (mode == Dequeued) {
		if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
			for (unsigned p = 0; p < buf.length; p++) {
				struct v4l2_plane *vp = buf.m.planes + p;

				fail_on_test(!vp->bytesused);
				fail_on_test(vp->data_offset >= vp->bytesused);
				fail_on_test(vp->bytesused > vp->length);
			}
		} else {
			fail_on_test(!buf.bytesused);
			fail_on_test(buf.bytesused > buf.length);
		}
		fail_on_test(!buf.timestamp.tv_sec && !buf.timestamp.tv_usec);
		fail_on_test(!(buf.flags & (V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_ERROR)));
		if (node->is_video) {
			fail_on_test(buf.field == V4L2_FIELD_ALTERNATE);
			fail_on_test(buf.field == V4L2_FIELD_ANY);
			if (cur_fmt.fmt.pix.field == V4L2_FIELD_ALTERNATE) {
				fail_on_test(buf.field != V4L2_FIELD_BOTTOM &&
						buf.field != V4L2_FIELD_TOP);
				fail_on_test(buf.field == seq.last_field);
				seq.field_nr ^= 1;
				if (seq.field_nr)
					fail_on_test((int)buf.sequence != seq.last_seq);
				else
					fail_on_test((int)buf.sequence != seq.last_seq + 1);
			} else {
				fail_on_test(buf.field != cur_fmt.fmt.pix.field);
				fail_on_test((int)buf.sequence != seq.last_seq + 1);
			}
		} else {
			fail_on_test((int)buf.sequence != seq.last_seq + 1);
		}
		seq.last_seq = (int)buf.sequence;
		seq.last_field = buf.field;
	} else {
		fail_on_test(buf.sequence);
		if (mode == Queued && ts_copy && V4L2_TYPE_IS_OUTPUT(buf.type)) {
			fail_on_test(!buf.timestamp.tv_sec && !buf.timestamp.tv_usec);
		} else {
			fail_on_test(buf.timestamp.tv_sec || buf.timestamp.tv_usec);
			fail_on_test(buf.flags & V4L2_BUF_FLAG_TIMECODE);
		}
		if (!V4L2_TYPE_IS_OUTPUT(buf.type) || mode == Unqueued)
			fail_on_test(frame_types);
		if (mode == Unqueued)
			fail_on_test(buf.flags & (V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_PREPARED |
						  V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_ERROR));
		else if (mode == Prepared)
			fail_on_test((buf.flags & (V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_PREPARED |
						   V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_ERROR)) !=
					V4L2_BUF_FLAG_PREPARED);
		else
			fail_on_test(!(buf.flags & (V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_PREPARED)));
	}
	return 0;
}

static int testQueryBuf(struct node *node, unsigned type, unsigned count)
{
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buf;
	int ret;
	unsigned i;

	memset(&buf, 0, sizeof(buf));
	buf.type = type;
	if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
		buf.m.planes = planes;
		buf.length = VIDEO_MAX_PLANES;
	}
	for (i = 0; i < count; i++) {
		buf.index = i;
		fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
		if (V4L2_TYPE_IS_MULTIPLANAR(type))
			fail_on_test(buf.m.planes != planes);
		fail_on_test(checkQueryBuf(node, buf, type, buf.memory, i, Unqueued, last_seq));
	}
	buf.index = count;
	ret = doioctl(node, VIDIOC_QUERYBUF, &buf);
	fail_on_test(ret != EINVAL);
	return 0;
}


int testReqBufs(struct node *node)
{
	struct v4l2_requestbuffers bufs;
	struct v4l2_create_buffers cbufs;
	struct v4l2_format fmt;
	bool can_stream = node->caps & V4L2_CAP_STREAMING;
	bool can_rw = node->caps & V4L2_CAP_READWRITE;
	bool mmap_valid;
	bool userptr_valid;
	bool dmabuf_valid;
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
		bool is_overlay = i == V4L2_BUF_TYPE_VIDEO_OVERLAY ||
				  i == V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;

		if (!(node->valid_buftypes & (1 << i)))
			continue;
		info("test buftype %s\n", buftype2s(i).c_str());
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

		bufs.memory = V4L2_MEMORY_DMABUF;
		ret = doioctl(node, VIDIOC_REQBUFS, &bufs);
		fail_on_test(ret && ret != EINVAL);
		dmabuf_valid = !ret;
		fail_on_test((can_stream && !is_overlay) && !mmap_valid && !userptr_valid && !dmabuf_valid);
		fail_on_test((!can_stream || is_overlay) && (mmap_valid || userptr_valid || dmabuf_valid));
		if (!can_stream || is_overlay)
			continue;

		if (mmap_valid) {
			bufs.count = 1;
			bufs.memory = V4L2_MEMORY_MMAP;
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
			fail_on_test(bufs.count == 0);
			fail_on_test(bufs.memory != V4L2_MEMORY_MMAP);
			fail_on_test(bufs.type != i);
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
			fail_on_test(testQueryBuf(node, i, bufs.count));
			node->valid_memorytype |= 1 << V4L2_MEMORY_MMAP;
		}

		if (userptr_valid) {
			bufs.count = 1;
			bufs.memory = V4L2_MEMORY_USERPTR;
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
			fail_on_test(bufs.count == 0);
			fail_on_test(bufs.memory != V4L2_MEMORY_USERPTR);
			fail_on_test(bufs.type != i);
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
			fail_on_test(testQueryBuf(node, i, bufs.count));
			node->valid_memorytype |= 1 << V4L2_MEMORY_USERPTR;
		}

		if (dmabuf_valid) {
			bufs.count = 1;
			bufs.memory = V4L2_MEMORY_DMABUF;
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
			fail_on_test(bufs.count == 0);
			fail_on_test(bufs.memory != V4L2_MEMORY_DMABUF);
			fail_on_test(bufs.type != i);
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
			fail_on_test(testQueryBuf(node, i, bufs.count));
			node->valid_memorytype |= 1 << V4L2_MEMORY_DMABUF;
		}

		if (can_rw) {
			char buf = 0;

			if (node->can_capture)
				ret = read(node->fd, &buf, 1);
			else
				ret = write(node->fd, &buf, 1);
			if (ret != -1)
				return fail("Expected -1, got %d\n", ret);
			if (errno != EBUSY)
				return fail("Expected EBUSY, got %d\n", errno);
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
		fail_on_test(ret);
		fail_on_test(cbufs.count == 0);
		fail_on_test(cbufs.memory != bufs.memory);
		fail_on_test(cbufs.format.type != i);
		fail_on_test(testQueryBuf(node, i, cbufs.count));
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

int testExpBuf(struct node *node)
{
	struct v4l2_requestbuffers bufs;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buf;
	int type;
	int err;

	if (!(node->valid_memorytype & (1 << V4L2_MEMORY_MMAP))) {
		struct v4l2_exportbuffer expbuf;

		memset(&expbuf, 0, sizeof(expbuf));
		err = doioctl(node, VIDIOC_EXPBUF, &expbuf);
		fail_on_test(err != ENOTTY);
		return ENOTTY;
	}

	for (type = 0; type <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;
		if (type == V4L2_BUF_TYPE_VIDEO_OVERLAY ||
		    type == V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY)
			continue;

		err = 0;
		memset(planes, 0, sizeof(planes));
		memset(&bufs, 0, sizeof(bufs));
		bufs.count = 1;
		bufs.memory = V4L2_MEMORY_MMAP;
		bufs.type = type;
		fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));

		memset(ptrs, 0, sizeof(ptrs));

		for (unsigned i = 0; i < bufs.count; i++) {
			struct v4l2_exportbuffer expbuf;

			memset(&buf, 0, sizeof(buf));
			buf.type = bufs.type;
			buf.memory = bufs.memory;
			buf.index = i;
			if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
				buf.m.planes = planes;
				buf.length = VIDEO_MAX_PLANES;
			}
			fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
			unsigned num_planes = process_buf(buf, planes);

			for (unsigned p = 0; p < num_planes; p++) {
				memset(&expbuf, 0, sizeof(expbuf));
				expbuf.type = bufs.type;
				expbuf.index = i;
				expbuf.plane = p;
				expbuf.flags = O_RDWR;
				err = doioctl(node, VIDIOC_EXPBUF, &expbuf);
				if (err == ENOTTY)
					break;

				dmabufs[i][p] = expbuf.fd;
	 
				ptrs[i][p] = mmap(NULL, planes[p].length,
						PROT_READ | PROT_WRITE, MAP_SHARED, expbuf.fd, 0);
				if (ptrs[i][p] == MAP_FAILED) {
					close(dmabufs[i][p]);
					ptrs[i][p] = NULL;
					fail("mmap on DMABUF file descriptor failed\n");
					err = ENOMEM;
					break;
				}
			}
		}

		for (unsigned i = 0; i < bufs.count; i++) {
			for (unsigned p = 0; p < VIDEO_MAX_PLANES; p++) {
				if (ptrs[i][p]) {
					munmap(ptrs[i][p], planes[p].length);
					close(dmabufs[i][p]);
				}
			}
		}

		bufs.count = 0;
		fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
		if (err)
			return err;
	}
	return 0;
}

static void fillOutputBuf(struct v4l2_buffer &buf)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	if ((buf.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK) == V4L2_BUF_FLAG_TIMESTAMP_COPY) {
		buf.timestamp.tv_sec = ts.tv_sec;
		buf.timestamp.tv_usec = ts.tv_nsec / 1000;
		buf.flags |= V4L2_BUF_FLAG_TSTAMP_SRC_SOE;
	}
	buf.flags |= V4L2_BUF_FLAG_TIMECODE | V4L2_BUF_FLAG_KEYFRAME;
	buf.timecode.type = V4L2_TC_TYPE_30FPS;
	buf.timecode.flags = V4L2_TC_USERBITS_8BITCHARS;
	buf.timecode.frames = ts.tv_nsec * 30 / 1000000000;
	buf.timecode.seconds = ts.tv_sec % 60;
	buf.timecode.minutes = (ts.tv_sec / 60) % 60;
	buf.timecode.hours = (ts.tv_sec / 3600) % 30;
	buf.timecode.userbits[0] = 't';
	buf.timecode.userbits[1] = 'e';
	buf.timecode.userbits[2] = 's';
	buf.timecode.userbits[3] = 't';
	buf.field = V4L2_FIELD_ANY;
	if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
		for (unsigned p = 0; p < buf.length; p++) {
			buf.m.planes[p].bytesused = buf.m.planes[p].length;
			buf.m.planes[p].data_offset = 0;
		}
	} else {
		buf.bytesused = buf.length;
	}
}

int testReadWrite(struct node *node)
{
	bool can_rw = node->caps & V4L2_CAP_READWRITE;
	int fd_flags = fcntl(node->fd, F_GETFL);
	char buf = 0;
	int ret;

	fcntl(node->fd, F_SETFL, fd_flags | O_NONBLOCK);
	if (node->can_capture)
		ret = read(node->fd, &buf, 1);
	else
		ret = write(node->fd, &buf, 1);
	// Note: RDS can only return multiples of 3, so we accept
	// both 0 and 1 as return code.
	// EBUSY can be returned when attempting to read/write to a
	// multiplanar format.
	if (can_rw)
		fail_on_test((ret < 0 && errno != EAGAIN && errno != EBUSY) || ret > 1);
	else
		fail_on_test(ret >= 0 || errno != EINVAL);
	if (!can_rw)
		return ENOTTY;

	reopen(node);
	fcntl(node->fd, F_SETFL, fd_flags | O_NONBLOCK);

	/* check that the close cleared the busy flag */
	if (node->can_capture)
		ret = read(node->fd, &buf, 1);
	else
		ret = write(node->fd, &buf, 1);
	fail_on_test((ret < 0 && errno != EAGAIN && errno != EBUSY) || ret > 1);
	return 0;
}

static int captureBufs(struct node *node, const struct v4l2_requestbuffers &bufs,
		       const struct v4l2_requestbuffers &m2m_bufs,
		       unsigned frame_count, bool use_poll)
{
	int fd_flags = fcntl(node->fd, F_GETFL);
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buf;
	unsigned count = frame_count;
	unsigned timestamp;
	int ret;

	if (show_info) {
		printf("\t    %s%s:\n",
			buftype2s(bufs.type).c_str(),
			use_poll ? " (polling)" : "");
	}

	memset(planes, 0, sizeof(planes));
	memset(&buf, 0, sizeof(buf));
	if (use_poll)
		fcntl(node->fd, F_SETFL, fd_flags | O_NONBLOCK);
	for (;;) {
		if (use_poll) {
			struct timeval tv = { 2, 0 };
			fd_set fds;

			FD_ZERO(&fds);
			FD_SET(node->fd, &fds);
			if (node->is_m2m)
				ret = select(node->fd + 1, &fds, &fds, NULL, &tv);
			else if (V4L2_TYPE_IS_OUTPUT(bufs.type))
				ret = select(node->fd + 1, NULL, &fds, NULL, &tv);
			else
				ret = select(node->fd + 1, &fds, NULL, NULL, &tv);
			fail_on_test(ret <= 0);
			fail_on_test(!FD_ISSET(node->fd, &fds));
		}

		buf.type = bufs.type;
		buf.memory = bufs.memory;
		if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
			buf.m.planes = planes;
			buf.length = VIDEO_MAX_PLANES;
		}

		ret = doioctl(node, VIDIOC_DQBUF, &buf);
		if (ret != EAGAIN) {
			fail_on_test(ret);
			if (show_info)
				printf("\t\tBuffer: %d Sequence: %d Field: %s Timestamp: %ld.%06lds\n",
						buf.index, buf.sequence, field2s(buf.field).c_str(),
						buf.timestamp.tv_sec, buf.timestamp.tv_usec);
			fail_on_test(checkQueryBuf(node, buf, bufs.type, bufs.memory, buf.index, Dequeued, last_seq));
			if (!show_info) {
				printf("\r\t%s: Frame #%03d%s",
						buftype2s(bufs.type).c_str(),
						frame_count - count, use_poll ? " (polling)" : "");
				fflush(stdout);
			}
			timestamp = buf.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK;
			if (V4L2_TYPE_IS_OUTPUT(buf.type)) {
				fillOutputBuf(buf);
			} else if (node->is_m2m && timestamp == V4L2_BUF_FLAG_TIMESTAMP_COPY) {
				fail_on_test(buffer_info.find(buf.timestamp) == buffer_info.end());
				struct v4l2_buffer &orig_buf = buffer_info[buf.timestamp];
				fail_on_test(buf.field != orig_buf.field);
				fail_on_test((buf.flags & valid_output_flags) !=
					     (orig_buf.flags & valid_output_flags));
				if (buf.flags & V4L2_BUF_FLAG_TIMECODE)
					fail_on_test(memcmp(&buf.timecode, &orig_buf.timecode,
								sizeof(buf.timecode)));
			}
			fail_on_test(doioctl(node, VIDIOC_QBUF, &buf));
			if (V4L2_TYPE_IS_OUTPUT(buf.type))
				buffer_info[buf.timestamp] = buf;
			if (--count == 0)
				break;
		}
		if (!node->is_m2m)
			continue;

		buf.type = m2m_bufs.type;
		buf.memory = m2m_bufs.memory;
		if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
			buf.m.planes = planes;
			buf.length = VIDEO_MAX_PLANES;
		}
		ret = doioctl(node, VIDIOC_DQBUF, &buf);
		if (ret == EAGAIN)
			continue;
		if (show_info)
			printf("\t\tBuffer: %d Sequence: %d Field: %s Timestamp: %ld.%06lds\n",
				buf.index, buf.sequence, field2s(buf.field).c_str(),
				buf.timestamp.tv_sec, buf.timestamp.tv_usec);
		fail_on_test(ret);
		fail_on_test(checkQueryBuf(node, buf, m2m_bufs.type, m2m_bufs.memory, buf.index, Dequeued, last_m2m_seq));
		timestamp = buf.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK;
		if (V4L2_TYPE_IS_OUTPUT(buf.type)) {
			fillOutputBuf(buf);
		} else if (timestamp == V4L2_BUF_FLAG_TIMESTAMP_COPY) {
			fail_on_test(buffer_info.find(buf.timestamp) == buffer_info.end());
			struct v4l2_buffer &orig_buf = buffer_info[buf.timestamp];
			fail_on_test(buf.field != orig_buf.field);
			fail_on_test((buf.flags & valid_output_flags) !=
					(orig_buf.flags & valid_output_flags));
			if (buf.flags & V4L2_BUF_FLAG_TIMECODE)
				fail_on_test(memcmp(&buf.timecode, &orig_buf.timecode,
							sizeof(buf.timecode)));
		}
		fail_on_test(doioctl(node, VIDIOC_QBUF, &buf));
		if (V4L2_TYPE_IS_OUTPUT(buf.type))
			buffer_info[buf.timestamp] = buf;
	}
	if (use_poll)
		fcntl(node->fd, F_SETFL, fd_flags);
	if (!show_info) {
		printf("\r\t                                                  \r");
		fflush(stdout);
	}
	return 0;
}

static int setupM2M(struct node *node, struct v4l2_requestbuffers &bufs, int type)
{
	memset(&bufs, 0, sizeof(bufs));
	if (V4L2_TYPE_IS_MULTIPLANAR(type))
		bufs.type = V4L2_TYPE_IS_OUTPUT(type) ?
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	else
		bufs.type = V4L2_TYPE_IS_OUTPUT(type) ?
			V4L2_BUF_TYPE_VIDEO_CAPTURE :
			V4L2_BUF_TYPE_VIDEO_OUTPUT;

	last_m2m_seq.init();
	if (node->is_video) {
		struct v4l2_format fmt;

		fmt.type = bufs.type;
		fail_on_test(doioctl(node, VIDIOC_G_FMT, &fmt));
		if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type))
			last_m2m_seq.last_field = cur_fmt.fmt.pix_mp.field;
		else
			last_m2m_seq.last_field = cur_fmt.fmt.pix.field;
	}

	bufs.memory = V4L2_MEMORY_MMAP;
	bufs.count = 1;
	fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
	for (unsigned i = 0; i < bufs.count; i++) {
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		struct v4l2_buffer buf;

		memset(planes, 0, sizeof(planes));
		memset(&buf, 0, sizeof(buf));
		buf.type = bufs.type;
		buf.memory = bufs.memory;
		buf.index = i;
		if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
			buf.m.planes = planes;
			buf.length = VIDEO_MAX_PLANES;
		}
		fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
		if (V4L2_TYPE_IS_OUTPUT(buf.type))
			fillOutputBuf(buf);
		fail_on_test(doioctl(node, VIDIOC_QBUF, &buf));
		if (V4L2_TYPE_IS_OUTPUT(buf.type))
			buffer_info[buf.timestamp] = buf;
	}
	fail_on_test(doioctl(node, VIDIOC_STREAMON, &bufs.type));
	return 0;
}

static int bufferOutputErrorTest(struct node *node, const struct v4l2_buffer &orig_buf,
				 const struct v4l2_plane orig_planes[VIDEO_MAX_PLANES])
{
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buf = orig_buf;
	bool have_prepare = false;
	int ret;

	memcpy(planes, orig_planes, sizeof(planes));
	if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
		buf.m.planes = planes;
		for (unsigned p = 0; p < buf.length; p++) {
			buf.m.planes[p].bytesused = buf.m.planes[p].length + 1;
			buf.m.planes[p].data_offset = 0;
		}
	} else {
		buf.bytesused = buf.length + 1;
	}
	ret = doioctl(node, VIDIOC_PREPARE_BUF, &buf);
	fail_on_test(ret != EINVAL && ret != ENOTTY);
	have_prepare = ret != ENOTTY;
	fail_on_test(doioctl(node, VIDIOC_QBUF, &buf) != EINVAL);

	if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
		for (unsigned p = 0; p < buf.length; p++) {
			buf.m.planes[p].bytesused = buf.m.planes[p].length / 2;
			buf.m.planes[p].data_offset = buf.m.planes[p].bytesused;
		}
		if (have_prepare)
			fail_on_test(doioctl(node, VIDIOC_PREPARE_BUF, &buf) != EINVAL);
		fail_on_test(doioctl(node, VIDIOC_QBUF, &buf) != EINVAL);
	}
	buf = orig_buf;
	if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
		buf.m.planes = planes;
		for (unsigned p = 0; p < buf.length; p++) {
			buf.m.planes[p].bytesused = 0;
			buf.m.planes[p].data_offset = 0;
		}
	} else {
		buf.bytesused = 0;
	}
	if (have_prepare) {
		fail_on_test(doioctl(node, VIDIOC_PREPARE_BUF, &buf));
		fail_on_test(checkQueryBuf(node, buf, buf.type, buf.memory, 0, Prepared, last_seq));
		buf = orig_buf;
		if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
			buf.m.planes = planes;
			for (unsigned p = 0; p < buf.length; p++) {
				buf.m.planes[p].bytesused = 0xdeadbeef;
				buf.m.planes[p].data_offset = 0xdeadbeef;
			}
		} else {
			buf.bytesused = 0xdeadbeef;
		}
	}
	fail_on_test(doioctl(node, VIDIOC_QBUF, &buf));
	if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
		for (unsigned p = 0; p < buf.length; p++) {
			fail_on_test(buf.m.planes[p].bytesused != buf.m.planes[p].length);
			fail_on_test(buf.m.planes[p].data_offset);
		}
	} else {
		fail_on_test(buf.bytesused != buf.length);
	}
	return 0;
}

static int setupMmap(struct node *node, struct v4l2_requestbuffers &bufs)
{
	for (unsigned i = 0; i < bufs.count; i++) {
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		struct v4l2_buffer buf;
		int ret;

		memset(planes, 0, sizeof(planes));
		memset(&buf, 0, sizeof(buf));
		buf.type = bufs.type;
		buf.memory = bufs.memory;
		buf.index = i;
		if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
			buf.m.planes = planes;
			buf.length = VIDEO_MAX_PLANES;
		}
		fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
		fail_on_test(checkQueryBuf(node, buf, bufs.type, bufs.memory, i, Unqueued, last_seq));
		unsigned num_planes = process_buf(buf, planes);

		for (unsigned p = 0; p < num_planes; p++) {
			// Try a random offset
			ptrs[i][p] = test_mmap(NULL, planes[p].length,
					PROT_READ | PROT_WRITE, MAP_SHARED, node->fd, planes[p].m.mem_offset + 0xdeadbeef);
			fail_on_test(ptrs[i][p] != MAP_FAILED);

			// Now with the proper offset
			ptrs[i][p] = test_mmap(NULL, planes[p].length,
					PROT_READ | PROT_WRITE, MAP_SHARED, node->fd, planes[p].m.mem_offset);
			fail_on_test(ptrs[i][p] == MAP_FAILED);
		}
		fail_on_test(!doioctl(node, VIDIOC_DQBUF, &buf));
		if (V4L2_TYPE_IS_OUTPUT(buf.type))
			fillOutputBuf(buf);
		if (V4L2_TYPE_IS_OUTPUT(buf.type) && i == 0) {
			fail_on_test(bufferOutputErrorTest(node, buf, planes));
			fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
			fail_on_test(checkQueryBuf(node, buf, buf.type, buf.memory, i, Queued, last_seq));
		} else {
			ret = doioctl(node, VIDIOC_PREPARE_BUF, &buf);
			fail_on_test(ret && ret != ENOTTY);
			if (ret == 0) {
				fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
				fail_on_test(checkQueryBuf(node, buf, bufs.type, bufs.memory, i, Prepared, last_seq));
				fail_on_test(!doioctl(node, VIDIOC_PREPARE_BUF, &buf));
			}

			if (V4L2_TYPE_IS_OUTPUT(buf.type))
				fillOutputBuf(buf);
			fail_on_test(doioctl(node, VIDIOC_QBUF, &buf));
			fail_on_test(!doioctl(node, VIDIOC_QBUF, &buf));
			fail_on_test(!doioctl(node, VIDIOC_PREPARE_BUF, &buf));
			// Test with invalid buffer index
			buf.index += VIDEO_MAX_FRAME;
			fail_on_test(!doioctl(node, VIDIOC_PREPARE_BUF, &buf));
			fail_on_test(!doioctl(node, VIDIOC_QBUF, &buf));
			fail_on_test(!doioctl(node, VIDIOC_QUERYBUF, &buf));
			buf.index -= VIDEO_MAX_FRAME;
			fail_on_test(buf.index != i);
		}
		if (V4L2_TYPE_IS_OUTPUT(buf.type))
			buffer_info[buf.timestamp] = buf;
		fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
		fail_on_test(checkQueryBuf(node, buf, bufs.type, bufs.memory, i, Queued, last_seq));
		fail_on_test(!doioctl(node, VIDIOC_DQBUF, &buf));
	}
	return 0;
}

static int releaseMmap(struct node *node, struct v4l2_requestbuffers &bufs)
{
	for (unsigned i = 0; i < bufs.count; i++) {
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));
		buf.type = bufs.type;
		buf.memory = bufs.memory;
		buf.index = i;
		if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
			buf.m.planes = planes;
			buf.length = VIDEO_MAX_PLANES;
		}
		fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
		unsigned num_planes = process_buf(buf, planes);

		for (unsigned p = 0; p < num_planes; p++)
			munmap(ptrs[i][p], planes[p].length);
	}
	return 0;
}

int testMmap(struct node *node, unsigned frame_count)
{
	struct v4l2_requestbuffers bufs;
	struct v4l2_requestbuffers m2m_bufs;
	struct v4l2_create_buffers cbufs;
	bool can_stream = node->caps & V4L2_CAP_STREAMING;
	bool have_createbufs = true;
	int type;
	int ret;

	if (!node->valid_buftypes)
		return ENOTTY;

	buffer_info.clear();
	for (type = 0; type <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;
		if (type == V4L2_BUF_TYPE_VIDEO_OVERLAY ||
		    type == V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY)
			continue;

		memset(&bufs, 0, sizeof(bufs));
		bufs.type = type;
		bufs.memory = V4L2_MEMORY_MMAP;
		ret = doioctl(node, VIDIOC_REQBUFS, &bufs);
		if (ret) {
			fail_on_test(can_stream);
			return ret;
		}
		fail_on_test(!can_stream);

		fail_on_test(doioctl(node, VIDIOC_STREAMON, &bufs.type) != EINVAL);
		fail_on_test(doioctl(node, VIDIOC_STREAMOFF, &bufs.type));

		cur_fmt.type = bufs.type;
		doioctl(node, VIDIOC_G_FMT, &cur_fmt);
		bufs.count = 1;
		fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
		fail_on_test(doioctl(node, VIDIOC_STREAMOFF, &bufs.type));
		last_seq.init();

		// Test queuing buffers...
		for (unsigned i = 0; i < bufs.count; i++) {
			struct v4l2_plane planes[VIDEO_MAX_PLANES];
			struct v4l2_buffer buf;

			memset(&buf, 0, sizeof(buf));
			buf.type = bufs.type;
			buf.memory = bufs.memory;
			buf.index = i;
			if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
				buf.m.planes = planes;
				buf.length = VIDEO_MAX_PLANES;
			}
			fail_on_test(doioctl(node, VIDIOC_QBUF, &buf));
		}
		// calling STREAMOFF...
		fail_on_test(doioctl(node, VIDIOC_STREAMOFF, &bufs.type));
		// and now we should be able to queue those buffers again since
		// STREAMOFF should return them back to the dequeued state.
		for (unsigned i = 0; i < bufs.count; i++) {
			struct v4l2_plane planes[VIDEO_MAX_PLANES];
			struct v4l2_buffer buf;

			memset(&buf, 0, sizeof(buf));
			buf.type = bufs.type;
			buf.memory = bufs.memory;
			buf.index = i;
			if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
				buf.m.planes = planes;
				buf.length = VIDEO_MAX_PLANES;
			}
			fail_on_test(doioctl(node, VIDIOC_QBUF, &buf));
		}
		// Now request buffers again, freeing the old buffers.
		// Good check for whether all the internal vb2 calls are in
		// balance.
		fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));

		cbufs.format = cur_fmt;
		cbufs.count = 0;
		cbufs.memory = bufs.memory;
		ret = doioctl(node, VIDIOC_CREATE_BUFS, &cbufs);
		fail_on_test(ret != ENOTTY && ret != 0);
		if (ret == ENOTTY)
			have_createbufs = false;
		else
			fail_on_test(cbufs.index != bufs.count);
		cbufs.count = 1;
		if (have_createbufs) {
			if (node->is_video) {
				if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
					last_seq.last_field = cur_fmt.fmt.pix_mp.field;
					cbufs.format.fmt.pix_mp.height /= 2;
					for (unsigned p = 0; p < cbufs.format.fmt.pix_mp.num_planes; p++)
						cbufs.format.fmt.pix_mp.plane_fmt[p].sizeimage /= 2;
				} else {
					last_seq.last_field = cur_fmt.fmt.pix.field;
					cbufs.format.fmt.pix.height /= 2;
					cbufs.format.fmt.pix.sizeimage /= 2;
				}
				ret = doioctl(node, VIDIOC_CREATE_BUFS, &cbufs);
				fail_on_test(ret != EINVAL);
				fail_on_test(testQueryBuf(node, cur_fmt.type, bufs.count));
				cbufs.format = cur_fmt;
				if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
					for (unsigned p = 0; p < cbufs.format.fmt.pix_mp.num_planes; p++)
						cbufs.format.fmt.pix_mp.plane_fmt[p].sizeimage *= 2;
				} else {
					cbufs.format.fmt.pix.sizeimage *= 2;
				}
			}
			cbufs.count = 1;
			cbufs.memory = bufs.memory;
			fail_on_test(doioctl(node, VIDIOC_CREATE_BUFS, &cbufs));
			fail_on_test(cbufs.index != bufs.count);
			bufs.count = cbufs.index + cbufs.count;
		}
		fail_on_test(setupMmap(node, bufs));

		fail_on_test(doioctl(node, VIDIOC_STREAMON, &bufs.type));
		fail_on_test(doioctl(node, VIDIOC_STREAMON, &bufs.type));

		if (node->is_m2m)
			fail_on_test(setupM2M(node, m2m_bufs, bufs.type));
		else
			fail_on_test(captureBufs(node, bufs, m2m_bufs, frame_count, false));
		fail_on_test(captureBufs(node, bufs, m2m_bufs, frame_count, true));
		fail_on_test(doioctl(node, VIDIOC_STREAMOFF, &bufs.type));
		fail_on_test(doioctl(node, VIDIOC_STREAMOFF, &bufs.type));
		fail_on_test(releaseMmap(node, bufs));
		bufs.count = m2m_bufs.count = 0;
		fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
		if (node->is_m2m) {
			fail_on_test(doioctl(node, VIDIOC_STREAMOFF, &m2m_bufs.type));
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &m2m_bufs));
		}
	}
	return 0;
}

static int setupUserPtr(struct node *node, struct v4l2_requestbuffers &bufs)
{
	for (unsigned i = 0; i < bufs.count; i++) {
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		struct v4l2_buffer buf;
		int ret;

		memset(planes, 0, sizeof(planes));
		memset(&buf, 0, sizeof(buf));
		buf.type = bufs.type;
		buf.memory = bufs.memory;
		buf.index = i;
		if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
			buf.m.planes = planes;
			buf.length = VIDEO_MAX_PLANES;
		}
		fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
		fail_on_test(checkQueryBuf(node, buf, bufs.type, bufs.memory, i, Unqueued, last_seq));
		unsigned num_planes = process_buf(buf, planes);

		for (unsigned p = 0; p < num_planes; p++) {
			// This should not work!
			ptrs[i][p] = test_mmap(NULL, planes[p].length,
					PROT_READ | PROT_WRITE, MAP_SHARED, node->fd, 0);
			fail_on_test(ptrs[i][p] != MAP_FAILED);
		}

		for (unsigned p = 0; p < num_planes; p++) {
			ptrs[i][p] = malloc(planes[p].length);
			fail_on_test(ptrs[i][p] == NULL);
		}
		if (V4L2_TYPE_IS_OUTPUT(buf.type))
			fillOutputBuf(buf);

		ret = ENOTTY;
		// Try to use VIDIOC_PREPARE_BUF for every other buffer
		if ((i & 1) == 0) {
			if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
				for (unsigned p = 0; p < buf.length; p++)
					planes[p].m.userptr = 0;
			} else {
				buf.m.userptr = 0;
			}
			ret = doioctl(node, VIDIOC_PREPARE_BUF, &buf);
			fail_on_test(!ret);

			if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
				for (unsigned p = 0; p < buf.length; p++)
					planes[p].m.userptr =
						(unsigned long)ptrs[i][p] + 0xdeadbeef;
			} else {
				buf.m.userptr = (unsigned long)ptrs[i][0] + 0xdeadbeef;
			}
			ret = doioctl(node, VIDIOC_PREPARE_BUF, &buf);
			fail_on_test(!ret);

			if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
				for (unsigned p = 0; p < buf.length; p++)
					planes[p].m.userptr = (unsigned long)ptrs[i][p];
			} else {
				buf.m.userptr = (unsigned long)ptrs[i][0];
			}
			ret = doioctl(node, VIDIOC_PREPARE_BUF, &buf);
			fail_on_test(ret && ret != ENOTTY);

			if (ret == 0) {
				fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
				fail_on_test(checkQueryBuf(node, buf, bufs.type, bufs.memory, i, Prepared, last_seq));
			}
		}
		if (ret == ENOTTY) {
			if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
				for (unsigned p = 0; p < buf.length; p++)
					planes[p].m.userptr = 0;
			} else {
				buf.m.userptr = 0;
			}
			ret = doioctl(node, VIDIOC_QBUF, &buf);
			fail_on_test(!ret);

			if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
				for (unsigned p = 0; p < buf.length; p++)
					planes[p].m.userptr =
						(unsigned long)ptrs[i][p] + 0xdeadbeef;
			} else {
				buf.m.userptr = (unsigned long)ptrs[i][0] + 0xdeadbeef;
			}
			ret = doioctl(node, VIDIOC_QBUF, &buf);
			fail_on_test(!ret);

			if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
				for (unsigned p = 0; p < buf.length; p++)
					planes[p].m.userptr = (unsigned long)ptrs[i][p];
			} else {
				buf.m.userptr = (unsigned long)ptrs[i][0];
			}
		}

		if (V4L2_TYPE_IS_OUTPUT(buf.type))
			fillOutputBuf(buf);
		fail_on_test(doioctl(node, VIDIOC_QBUF, &buf));
		if (V4L2_TYPE_IS_OUTPUT(buf.type))
			buffer_info[buf.timestamp] = buf;
		fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
		fail_on_test(checkQueryBuf(node, buf, bufs.type, bufs.memory, i, Queued, last_seq));
	}
	return 0;
}

static int releaseUserPtr(struct node *node, struct v4l2_requestbuffers &bufs)
{
	for (unsigned i = 0; i < bufs.count; i++)
		for (unsigned p = 0; p < VIDEO_MAX_PLANES; p++) {
			free(ptrs[i][p]);
			ptrs[i][p] = NULL;
		}
	return 0;
}

int testUserPtr(struct node *node, unsigned frame_count)
{
	struct v4l2_requestbuffers bufs;
	struct v4l2_requestbuffers m2m_bufs;
	bool can_stream = node->caps & V4L2_CAP_STREAMING;
	int type;
	int ret;

	if (!node->valid_buftypes)
		return ENOTTY;

	buffer_info.clear();
	for (type = 0; type <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;
		if (type == V4L2_BUF_TYPE_VIDEO_OVERLAY ||
		    type == V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY)
			continue;

		memset(&bufs, 0, sizeof(bufs));
		bufs.type = type;
		bufs.memory = V4L2_MEMORY_USERPTR;
		ret = doioctl(node, VIDIOC_REQBUFS, &bufs);
		if (ret) {
			fail_on_test(ret != EINVAL);
			return ENOTTY;
		}
		fail_on_test(!can_stream);

		bufs.count = 1;
		fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
		fail_on_test(doioctl(node, VIDIOC_STREAMOFF, &bufs.type));
		last_seq.init();
		if (node->is_video) {
			if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type))
				last_seq.last_field = cur_fmt.fmt.pix_mp.field;
			else
				last_seq.last_field = cur_fmt.fmt.pix.field;
		}

		fail_on_test(setupUserPtr(node, bufs));

		fail_on_test(doioctl(node, VIDIOC_STREAMON, &bufs.type));
		fail_on_test(doioctl(node, VIDIOC_STREAMON, &bufs.type));

		if (node->is_m2m)
			fail_on_test(setupM2M(node, m2m_bufs, bufs.type));
		else
			fail_on_test(captureBufs(node, bufs, m2m_bufs, frame_count, false));
		fail_on_test(captureBufs(node, bufs, m2m_bufs, frame_count, true));
		fail_on_test(doioctl(node, VIDIOC_STREAMOFF, &bufs.type));
		fail_on_test(doioctl(node, VIDIOC_STREAMOFF, &bufs.type));
		fail_on_test(releaseUserPtr(node, bufs));
		bufs.count = m2m_bufs.count = 0;
		fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
		if (node->is_m2m) {
			fail_on_test(doioctl(node, VIDIOC_STREAMOFF, &m2m_bufs.type));
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &m2m_bufs));
		}
	}
	return 0;
}

static int setupDmaBuf(struct node *expbuf_node, struct node *node,
		       struct v4l2_requestbuffers &bufs, int expbuf_type)
{
	struct v4l2_requestbuffers expbuf_bufs;
	struct v4l2_plane expbuf_planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer expbuf_buf;

	memset(expbuf_planes, 0, sizeof(expbuf_planes));
	memset(&expbuf_bufs, 0, sizeof(expbuf_bufs));
	expbuf_bufs.count = bufs.count;
	expbuf_bufs.memory = V4L2_MEMORY_MMAP;
	expbuf_bufs.type = expbuf_type;
	fail_on_test(doioctl_no_wrap(expbuf_node, VIDIOC_REQBUFS, &expbuf_bufs));
	fail_on_test(expbuf_bufs.count < bufs.count);

	memset(&expbuf_buf, 0, sizeof(expbuf_buf));
	expbuf_buf.type = expbuf_bufs.type;
	expbuf_buf.memory = expbuf_bufs.memory;
	if (V4L2_TYPE_IS_MULTIPLANAR(expbuf_bufs.type)) {
		expbuf_buf.m.planes = expbuf_planes;
		expbuf_buf.length = VIDEO_MAX_PLANES;
	}
	fail_on_test(doioctl_no_wrap(expbuf_node, VIDIOC_QUERYBUF, &expbuf_buf));

	for (unsigned i = 0; i < bufs.count; i++) {
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		struct v4l2_buffer buf;
		struct v4l2_exportbuffer expbuf;
		int ret;

		memset(planes, 0, sizeof(planes));
		memset(&buf, 0, sizeof(buf));
		buf.type = bufs.type;
		buf.memory = bufs.memory;
		buf.index = i;
		if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
			buf.m.planes = planes;
			buf.length = VIDEO_MAX_PLANES;
		}
		fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
		fail_on_test(checkQueryBuf(node, buf, bufs.type, bufs.memory, i, Unqueued, last_seq));
		unsigned num_planes = process_buf(buf, planes);
		fail_on_test(expbuf_buf.length < buf.length);
		if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type))
			for (unsigned p = 0; p < buf.length; p++)
				fail_on_test(expbuf_planes[p].length < planes[p].length);

		for (unsigned p = 0; p < num_planes; p++) {
			// This should not work!
			ptrs[i][p] = test_mmap(NULL, planes[p].length,
					PROT_READ | PROT_WRITE, MAP_SHARED, node->fd, 0);
			fail_on_test(ptrs[i][p] != MAP_FAILED);
		}
		if (V4L2_TYPE_IS_OUTPUT(buf.type))
			fillOutputBuf(buf);

		for (unsigned p = 0; p < num_planes; p++) {
			memset(&expbuf, 0, sizeof(expbuf));
			expbuf.type = expbuf_bufs.type;
			expbuf.index = i;
			expbuf.plane = p;
			expbuf.flags = O_RDWR;
			fail_on_test(doioctl_no_wrap(expbuf_node, VIDIOC_EXPBUF, &expbuf));

			dmabufs[i][p] = expbuf.fd;

			ptrs[i][p] = mmap(NULL, planes[p].length,
					PROT_READ | PROT_WRITE, MAP_SHARED, expbuf.fd, 0);
			fail_on_test(ptrs[i][p] == MAP_FAILED);
		}

		if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
			for (unsigned p = 0; p < buf.length; p++)
				planes[p].m.fd = 0xdeadbeef + dmabufs[i][p];
		} else {
			buf.m.fd = 0xdeadbeef + dmabufs[i][0];
		}
		ret = doioctl(node, VIDIOC_PREPARE_BUF, &buf);
		fail_on_test(!ret);
		if (ret != ENOTTY) {
			if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
				for (unsigned p = 0; p < buf.length; p++)
					planes[p].m.fd = dmabufs[i][p];
			} else {
				buf.m.fd = dmabufs[i][0];
			}
			ret = doioctl(node, VIDIOC_PREPARE_BUF, &buf);
			fail_on_test(ret);
			fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
			fail_on_test(checkQueryBuf(node, buf, bufs.type, bufs.memory, i, Prepared, last_seq));
		} else {
			fail_on_test(!doioctl(node, VIDIOC_QBUF, &buf));
			if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
				for (unsigned p = 0; p < buf.length; p++)
					planes[p].m.fd = dmabufs[i][p];
			} else {
				buf.m.fd = dmabufs[i][0];
			}
		}

		if (V4L2_TYPE_IS_OUTPUT(buf.type))
			fillOutputBuf(buf);
		fail_on_test(doioctl(node, VIDIOC_QBUF, &buf));
		if (V4L2_TYPE_IS_OUTPUT(buf.type))
			buffer_info[buf.timestamp] = buf;
		fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
		fail_on_test(checkQueryBuf(node, buf, bufs.type, bufs.memory, i, Queued, last_seq));
	}
	return 0;
}

static int releaseDmaBuf(struct node *expbuf_node, struct node *node,
			 struct v4l2_requestbuffers &bufs)
{
	for (unsigned i = 0; i < bufs.count; i++) {
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		struct v4l2_buffer buf;

		memset(planes, 0, sizeof(planes));
		memset(&buf, 0, sizeof(buf));
		buf.type = bufs.type;
		buf.memory = bufs.memory;
		buf.index = i;
		if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type)) {
			buf.m.planes = planes;
			buf.length = VIDEO_MAX_PLANES;
		}
		fail_on_test(doioctl(node, VIDIOC_QUERYBUF, &buf));
		unsigned num_planes = process_buf(buf, planes);

		for (unsigned p = 0; p < num_planes; p++) {
			munmap(ptrs[i][p], planes[p].length);
			close(dmabufs[i][p]);
		}
	}
	return 0;
}

int testDmaBuf(struct node *expbuf_node, struct node *node, unsigned frame_count)
{
	struct v4l2_requestbuffers bufs;
	struct v4l2_requestbuffers m2m_bufs;
	bool can_stream = node->caps & V4L2_CAP_STREAMING;
	int expbuf_type, type;
	int ret;

	if (!node->valid_buftypes)
		return ENOTTY;

	buffer_info.clear();
	for (type = 0; type <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;
		if (type == V4L2_BUF_TYPE_VIDEO_OVERLAY ||
		    type == V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY)
			continue;

		if (expbuf_node->caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
			expbuf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		else if (expbuf_node->caps & V4L2_CAP_VIDEO_CAPTURE)
			expbuf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		else if (expbuf_node->caps & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
			expbuf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		else 
			expbuf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

		memset(&bufs, 0, sizeof(bufs));
		bufs.type = type;
		bufs.memory = V4L2_MEMORY_DMABUF;
		ret = doioctl(node, VIDIOC_REQBUFS, &bufs);
		if (ret) {
			fail_on_test(ret != EINVAL);
			return ENOTTY;
		}
		fail_on_test(!can_stream);

		bufs.count = 1;
		fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
		fail_on_test(doioctl(node, VIDIOC_STREAMOFF, &bufs.type));
		last_seq.init();
		if (node->is_video) {
			if (V4L2_TYPE_IS_MULTIPLANAR(bufs.type))
				last_seq.last_field = cur_fmt.fmt.pix_mp.field;
			else
				last_seq.last_field = cur_fmt.fmt.pix.field;
		}

		fail_on_test(setupDmaBuf(expbuf_node, node, bufs, expbuf_type));

		fail_on_test(doioctl(node, VIDIOC_STREAMON, &bufs.type));
		fail_on_test(doioctl(node, VIDIOC_STREAMON, &bufs.type));

		if (node->is_m2m)
			fail_on_test(setupM2M(node, m2m_bufs, bufs.type));
		else
			fail_on_test(captureBufs(node, bufs, m2m_bufs, frame_count, false));
		fail_on_test(captureBufs(node, bufs, m2m_bufs, frame_count, true));
		fail_on_test(doioctl(node, VIDIOC_STREAMOFF, &bufs.type));
		fail_on_test(doioctl(node, VIDIOC_STREAMOFF, &bufs.type));
		fail_on_test(releaseDmaBuf(expbuf_node, node, bufs));
		bufs.count = m2m_bufs.count = 0;
		fail_on_test(doioctl(node, VIDIOC_REQBUFS, &bufs));
		if (node->is_m2m) {
			fail_on_test(doioctl(node, VIDIOC_STREAMOFF, &m2m_bufs.type));
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &m2m_bufs));
		}

		struct v4l2_requestbuffers expbuf_bufs;

		memset(&expbuf_bufs, 0, sizeof(expbuf_bufs));
		expbuf_bufs.memory = V4L2_MEMORY_MMAP;
		expbuf_bufs.type = expbuf_type;
		fail_on_test(doioctl_no_wrap(expbuf_node, VIDIOC_REQBUFS, &expbuf_bufs));
	}
	return 0;
}
