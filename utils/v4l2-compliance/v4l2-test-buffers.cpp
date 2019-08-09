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
#include <sys/wait.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <map>
#include <vector>
#include "v4l2-compliance.h"

#define V4L2_CTRL_CLASS_VIVID		0x00f00000
#define VIVID_CID_VIVID_BASE		(V4L2_CTRL_CLASS_VIVID | 0xf000)
#define VIVID_CID_DISCONNECT		(VIVID_CID_VIVID_BASE + 65)
#define VIVID_CID_DQBUF_ERROR		(VIVID_CID_VIVID_BASE + 66)
#define VIVID_CID_QUEUE_SETUP_ERROR	(VIVID_CID_VIVID_BASE + 67)
#define VIVID_CID_BUF_PREPARE_ERROR	(VIVID_CID_VIVID_BASE + 68)
#define VIVID_CID_START_STR_ERROR	(VIVID_CID_VIVID_BASE + 69)
#define VIVID_CID_QUEUE_ERROR		(VIVID_CID_VIVID_BASE + 70)
#define VIVID_CID_REQ_VALIDATE_ERROR	(VIVID_CID_VIVID_BASE + 72)

static struct cv4l_fmt cur_fmt;
static struct cv4l_fmt cur_m2m_fmt;
static int stream_from_fd = -1;
static bool stream_use_hdr;

bool operator<(struct timeval const& n1, struct timeval const& n2)
{
	return n1.tv_sec < n2.tv_sec ||
		(n1.tv_sec == n2.tv_sec && n1.tv_usec < n2.tv_usec);
}

struct buf_seq {
	buf_seq() { init(); }
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

static int buf_req_fds[VIDEO_MAX_FRAME * 2];

static inline int named_ioctl_fd(int fd, bool trace, const char *cmd_name, unsigned long cmd, void *arg)
{
	int retval;
	int e;

	retval = ioctl(fd, cmd, arg);
	e = retval == 0 ? 0 : errno;
	if (trace)
		fprintf(stderr, "\t\t%s returned %d (%s)\n",
				cmd_name, retval, strerror(e));
	return retval == -1 ? e : (retval ? -1 : 0);
}
#define doioctl_fd(fd, r, p) named_ioctl_fd((fd), node->g_trace(), #r, r, p)

enum QueryBufMode {
	Unqueued,
	Prepared,
	Queued,
	Dequeued
};

typedef std::map<struct timeval, struct v4l2_buffer> buf_info_map;
static buf_info_map buffer_info;

#define FILE_HDR_ID			v4l2_fourcc('V', 'h', 'd', 'r')

static void stream_close()
{
	if (stream_from_fd >= 0) {
		close(stream_from_fd);
		stream_from_fd = -1;
	}
}

static void stream_for_fmt(__u32 pixelformat)
{
	stream_close();

	std::string file = stream_from(fcc2s(pixelformat), stream_use_hdr);
	if (file.empty())
		return;

	stream_from_fd = open(file.c_str(), O_RDONLY);

	if (stream_from_fd < 0)
		fprintf(stderr, "cannot open file %s\n", file.c_str());
}

static void stream_reset()
{
	if (stream_from_fd >= 0)
		lseek(stream_from_fd, 0, SEEK_SET);
}

static bool fill_output_buffer(const cv4l_queue &q, cv4l_buffer &buf, bool first_run = true)
{
	bool seek = false;

	if (stream_from_fd < 0)
		return true;

	if (stream_use_hdr) {
		__u32 v;

		if (read(stream_from_fd, &v, sizeof(v)) != sizeof(v))
			seek = true;
		else if (ntohl(v) != FILE_HDR_ID) {
			fprintf(stderr, "Unknown header ID\n");
			return false;
		}
	}

	for (unsigned p = 0; !seek && p < buf.g_num_planes(); p++) {
		__u32 len = buf.g_length(p);

		buf.s_bytesused(len, p);
		buf.s_data_offset(0, p);
		if (stream_from_fd < 0)
			continue;

		if (!stream_use_hdr) {
			ssize_t sz = read(stream_from_fd, q.g_dataptr(buf.g_index(), p), len);

			if (sz < len) {
				seek = true;
				break;
			}
			continue;
		}
		__u32 bytesused;

		if (read(stream_from_fd, &bytesused, sizeof(bytesused)) != sizeof(bytesused)) {
			seek = true;
			break;
		}
		bytesused = ntohl(bytesused);
		if (bytesused > len) {
			fprintf(stderr, "plane size is too large (%u > %u)\n",
				bytesused, len);
			return false;
		}
		buf.s_bytesused(bytesused, p);

		ssize_t sz = read(stream_from_fd, q.g_dataptr(buf.g_index(), p), bytesused);

		if (sz < bytesused) {
			seek = true;
			break;
		}
	}
	if (!seek)
		return true;
	if (!first_run)
		return false;

	stream_reset();
	if (fill_output_buffer(q, buf, false))
		return true;

	close(stream_from_fd);
	stream_from_fd = -1;

	unsigned size = 0;

	for (unsigned p = 0; p < buf.g_num_planes(); p++)
		size += buf.g_length(p);

	if (stream_use_hdr)
		fprintf(stderr, "the input file is too small\n");
	else
		fprintf(stderr, "the input file is smaller than %d bytes\n", size);
	return false;
}

class buffer : public cv4l_buffer {
public:
	buffer(unsigned type = 0, unsigned memory = 0, unsigned index = 0) :
		cv4l_buffer(type, memory, index) {}
	buffer(const cv4l_queue &q, unsigned index = 0) :
		cv4l_buffer(q, index) {}
	buffer(const cv4l_buffer &b) : cv4l_buffer(b) {}

	int querybuf(node *node, unsigned index)
	{
		return node->querybuf(*this, index);
	}
	int prepare_buf(node *node, bool fill_bytesused = true)
	{
		if (v4l_type_is_output(g_type()))
			fill_output_buf(fill_bytesused);
		int err = node->prepare_buf(*this);
		if (err == 0 &&
		    v4l_type_is_video(g_type()) && v4l_type_is_output(g_type()))
			fail_on_test(g_field() == V4L2_FIELD_ANY);
		return err;
	}
	int prepare_buf(node *node, const cv4l_queue &q)
	{
		if (v4l_type_is_output(g_type()))
			fill_output_buffer(q, *this);
		return prepare_buf(node, false);
	}
	int dqbuf(node *node)
	{
		return node->dqbuf(*this);
	}
	int qbuf(node *node, bool fill_bytesused = true)
	{
		int err;

		if (v4l_type_is_output(g_type()))
			fill_output_buf(fill_bytesused);
		err = node->qbuf(*this);
		if (err == 0 &&
		    v4l_type_is_video(g_type()) && v4l_type_is_output(g_type())) {
			fail_on_test(g_field() == V4L2_FIELD_ANY);
			buffer_info[g_timestamp()] = buf;
		}
		return err;
	}
	int qbuf(node *node, const cv4l_queue &q)
	{
		if (v4l_type_is_output(g_type()))
			fill_output_buffer(q, *this);
		return qbuf(node, false);
	}
	int check(const cv4l_queue &q, enum QueryBufMode mode, bool is_m2m = false)
	{
		int ret = check(q.g_type(), q.g_memory(), g_index(), mode, last_seq, is_m2m);

		if (!ret)
			ret = check_planes(q, mode);
		return ret;
	}
	int check(const cv4l_queue &q, enum QueryBufMode mode, __u32 index, bool is_m2m = false)
	{
		int ret = check(q.g_type(), q.g_memory(), index, mode, last_seq, is_m2m);

		if (!ret)
			ret = check_planes(q, mode);
		return ret;
	}
	int check(const cv4l_queue &q, buf_seq &seq, bool is_m2m = false)
	{
		int ret = check(q.g_type(), q.g_memory(), g_index(), Dequeued, seq, is_m2m);

		if (!ret)
			ret = check_planes(q, Dequeued);
		return ret;
	}
	int check(enum QueryBufMode mode, __u32 index, bool is_m2m = false)
	{
		return check(g_type(), g_memory(), index, mode, last_seq, is_m2m);
	}
	int check(buf_seq &seq, bool is_m2m = false)
	{
		return check(g_type(), g_memory(), g_index(), Dequeued, seq, is_m2m);
	}

private:
	int check(unsigned type, unsigned memory, unsigned index,
			enum QueryBufMode mode, struct buf_seq &seq, bool is_m2m);
	int check_planes(const cv4l_queue &q, enum QueryBufMode mode);
	void fill_output_buf(bool fill_bytesused);
};

void buffer::fill_output_buf(bool fill_bytesused = true)
{
	timespec ts;
	v4l2_timecode tc;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	if (ts_is_copy()) {
		s_timestamp_ts(ts);
		s_timestamp_src(V4L2_BUF_FLAG_TSTAMP_SRC_SOE);
	}
	s_flags(g_flags() | V4L2_BUF_FLAG_TIMECODE);
	tc.type = V4L2_TC_TYPE_30FPS;
	tc.flags = V4L2_TC_USERBITS_8BITCHARS;
	tc.frames = ts.tv_nsec * 30 / 1000000000;
	tc.seconds = ts.tv_sec % 60;
	tc.minutes = (ts.tv_sec / 60) % 60;
	tc.hours = (ts.tv_sec / 3600) % 30;
	tc.userbits[0] = 't';
	tc.userbits[1] = 'e';
	tc.userbits[2] = 's';
	tc.userbits[3] = 't';
	s_timecode(tc);
	s_field(V4L2_FIELD_ANY);
	if (!fill_bytesused)
		return;
	for (unsigned p = 0; p < g_num_planes(); p++) {
		s_bytesused(g_length(p), p);
		s_data_offset(0, p);
	}
}

int buffer::check_planes(const cv4l_queue &q, enum QueryBufMode mode)
{
	if (mode == Dequeued || mode == Prepared) {
		for (unsigned p = 0; p < g_num_planes(); p++) {
			if (g_memory() == V4L2_MEMORY_USERPTR)
				fail_on_test(g_userptr(p) != q.g_userptr(g_index(), p));
			else if (g_memory() == V4L2_MEMORY_DMABUF)
				fail_on_test(g_fd(p) != q.g_fd(g_index(), p));
		}
	}
	return 0;
}

int buffer::check(unsigned type, unsigned memory, unsigned index,
		  enum QueryBufMode mode, struct buf_seq &seq, bool is_m2m)
{
	unsigned timestamp = g_timestamp_type();
	bool ts_copy = ts_is_copy();
	unsigned timestamp_src = g_timestamp_src();
	unsigned frame_types = 0;
	unsigned buf_states = 0;
	const struct cv4l_fmt &fmt = is_m2m ? cur_m2m_fmt : cur_fmt;

	fail_on_test(g_type() != type);
	fail_on_test(g_memory() == 0);
	fail_on_test(g_memory() != memory);
	fail_on_test(g_index() >= VIDEO_MAX_FRAME);
	fail_on_test(g_index() != index);
	fail_on_test(buf.reserved2);
	if (g_flags() & V4L2_BUF_FLAG_REQUEST_FD)
		fail_on_test(g_request_fd() < 0);
	else
		fail_on_test(g_request_fd());
	fail_on_test(timestamp != V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC &&
		     timestamp != V4L2_BUF_FLAG_TIMESTAMP_COPY);
	fail_on_test(timestamp_src != V4L2_BUF_FLAG_TSTAMP_SRC_SOE &&
		     timestamp_src != V4L2_BUF_FLAG_TSTAMP_SRC_EOF);
	fail_on_test(!ts_copy && v4l_type_is_output(g_type()) &&
		     timestamp_src == V4L2_BUF_FLAG_TSTAMP_SRC_SOE);
	if (g_flags() & V4L2_BUF_FLAG_KEYFRAME)
		frame_types++;
	if (g_flags() & V4L2_BUF_FLAG_PFRAME)
		frame_types++;
	if (g_flags() & V4L2_BUF_FLAG_BFRAME)
		frame_types++;
	fail_on_test(frame_types > 1);
	fail_on_test(g_flags() & (V4L2_BUF_FLAG_NO_CACHE_INVALIDATE |
				  V4L2_BUF_FLAG_NO_CACHE_CLEAN));
	if (g_flags() & V4L2_BUF_FLAG_QUEUED)
		buf_states++;
	if (g_flags() & V4L2_BUF_FLAG_DONE)
		buf_states++;
	if (g_flags() & V4L2_BUF_FLAG_PREPARED)
		buf_states++;
	if (g_flags() & V4L2_BUF_FLAG_IN_REQUEST) {
		fail_on_test(!(g_flags() & V4L2_BUF_FLAG_REQUEST_FD));
		if (!(g_flags() & V4L2_BUF_FLAG_PREPARED))
			buf_states++;
	}
	fail_on_test(buf_states > 1);
	fail_on_test(buf.length == 0);
	if (v4l_type_is_planar(g_type())) {
		fail_on_test(buf.length > VIDEO_MAX_PLANES);
		for (unsigned p = 0; p < buf.length; p++) {
			struct v4l2_plane *vp = buf.m.planes + p;

			fail_on_test(check_0(vp->reserved, sizeof(vp->reserved)));
			fail_on_test(vp->length == 0);
		}
	}

	if (v4l_type_is_capture(g_type()) && !ts_copy && !is_vivid &&
	    (g_flags() & V4L2_BUF_FLAG_TIMECODE))
		warn_once("V4L2_BUF_FLAG_TIMECODE was used!\n");

	if (mode == Dequeued) {
		for (unsigned p = 0; p < g_num_planes(); p++) {
			if (!(g_flags() & V4L2_BUF_FLAG_LAST))
				fail_on_test(!g_bytesused(p));
			if (!g_bytesused(p))
				fail_on_test(g_data_offset(p));
			else
				fail_on_test(g_data_offset(p) >= g_bytesused(p));
			fail_on_test(g_bytesused(p) > g_length(p));
		}
		fail_on_test(!g_timestamp().tv_sec && !g_timestamp().tv_usec);
		fail_on_test(g_flags() & V4L2_BUF_FLAG_DONE);
		fail_on_test((int)g_sequence() < seq.last_seq + 1);
		if (v4l_type_is_video(g_type())) {
			fail_on_test(g_field() == V4L2_FIELD_ALTERNATE);
			fail_on_test(g_field() == V4L2_FIELD_ANY);
			if (fmt.g_field() == V4L2_FIELD_ALTERNATE) {
				fail_on_test(g_field() != V4L2_FIELD_BOTTOM &&
						g_field() != V4L2_FIELD_TOP);
				fail_on_test(g_field() == seq.last_field);
				seq.field_nr ^= 1;
				if (seq.field_nr) {
					if ((int)g_sequence() != seq.last_seq)
						warn("got sequence number %u, expected %u\n",
							g_sequence(), seq.last_seq);
				} else {
					fail_on_test((int)g_sequence() == seq.last_seq + 1);
					if ((int)g_sequence() != seq.last_seq + 1)
						warn("got sequence number %u, expected %u\n",
							g_sequence(), seq.last_seq + 1);
				}
			} else {
				fail_on_test(g_field() != fmt.g_field());
				if ((int)g_sequence() != seq.last_seq + 1)
					warn("got sequence number %u, expected %u\n",
							g_sequence(), seq.last_seq + 1);
			}
		} else if ((int)g_sequence() != seq.last_seq + 1) {
			warn("got sequence number %u, expected %u\n",
					g_sequence(), seq.last_seq + 1);
		}
		seq.last_seq = (int)g_sequence();
		seq.last_field = g_field();
	} else {
		fail_on_test(g_sequence());
		if (mode == Queued && ts_copy && v4l_type_is_output(g_type())) {
			fail_on_test(!g_timestamp().tv_sec && !g_timestamp().tv_usec);
		} else {
			fail_on_test(g_timestamp().tv_sec || g_timestamp().tv_usec);
		}
		if (!v4l_type_is_output(g_type()) || mode == Unqueued)
			fail_on_test(frame_types);
		if (mode == Unqueued)
			fail_on_test(g_flags() & (V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_PREPARED |
						  V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_ERROR));
		else if (mode == Prepared)
			fail_on_test((g_flags() & (V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_PREPARED |
						   V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_ERROR)) !=
					V4L2_BUF_FLAG_PREPARED);
		else
			fail_on_test(!(g_flags() & (V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_PREPARED)));
	}
	return 0;
}

static int testQueryBuf(struct node *node, unsigned type, unsigned count)
{
	buffer buf(type);
	unsigned i;
	int ret;

	for (i = 0; i < count; i++) {
		fail_on_test(buf.querybuf(node, i));
		if (v4l_type_is_planar(buf.g_type()))
			fail_on_test(buf.buf.m.planes != buf.planes);
		fail_on_test(buf.check(Unqueued, i));
	}
	ret = buf.querybuf(node, count);
	fail_on_test(ret != EINVAL);
	return 0;
}

static int testSetupVbi(struct node *node, int type)
{
	if (!v4l_type_is_vbi(type))
		return 0;

	if (!(node->cur_io_caps & V4L2_IN_CAP_STD))
		return -1;

	node->s_type(type);
	cv4l_fmt vbi_fmt;

	if (!node->g_fmt(vbi_fmt))
		node->s_fmt(vbi_fmt);
	return 0;
}

static int testCanSetSameTimings(struct node *node)
{
	if (node->cur_io_caps & V4L2_IN_CAP_STD) {
		v4l2_std_id std;

		fail_on_test(node->g_std(std));
		fail_on_test(node->s_std(std));
	}
	if (node->cur_io_caps & V4L2_IN_CAP_DV_TIMINGS) {
		v4l2_dv_timings timings;

		fail_on_test(node->g_dv_timings(timings));
		fail_on_test(node->s_dv_timings(timings));
	}
	if (node->cur_io_caps & V4L2_IN_CAP_NATIVE_SIZE) {
		v4l2_selection sel = {
			node->g_selection_type(),
			V4L2_SEL_TGT_NATIVE_SIZE,
		};

		fail_on_test(node->g_selection(sel));
		fail_on_test(node->s_selection(sel));
	}
	return 0;
}

int testReqBufs(struct node *node)
{
	struct v4l2_create_buffers crbufs = { };
	struct v4l2_requestbuffers reqbufs = { };
	bool can_stream = node->g_caps() & V4L2_CAP_STREAMING;
	bool can_rw = node->g_caps() & V4L2_CAP_READWRITE;
	bool mmap_valid;
	bool userptr_valid;
	bool dmabuf_valid;
	int ret;
	unsigned i, m;
	
	node->reopen();

	cv4l_queue q(0, 0);

	ret = q.reqbufs(node, 0);
	if (ret == ENOTTY) {
		fail_on_test(can_stream);
		return ret;
	}
	fail_on_test(ret != EINVAL);
	fail_on_test(node->node2 == NULL);
	for (i = 1; i <= V4L2_BUF_TYPE_LAST; i++) {
		bool is_overlay = v4l_type_is_overlay(i);
		__u32 caps = 0;

		if (!(node->valid_buftypes & (1 << i)))
			continue;

		if (testSetupVbi(node, i))
			continue;

		info("test buftype %s\n", buftype2s(i).c_str());
		if (node->valid_buftype == 0)
			node->valid_buftype = i;

		q.init(0, 0);
		fail_on_test(q.reqbufs(node, i) != EINVAL);
		q.init(i, V4L2_MEMORY_MMAP);
		ret = q.reqbufs(node, 0);
		fail_on_test(ret && ret != EINVAL);
		mmap_valid = !ret;
		if (mmap_valid)
			node->buf_caps = caps = q.g_capabilities();
		if (caps) {
			fail_on_test(mmap_valid ^ !!(caps & V4L2_BUF_CAP_SUPPORTS_MMAP));
			if (caps & V4L2_BUF_CAP_SUPPORTS_ORPHANED_BUFS)
				node->supports_orphaned_bufs = true;
		}

		q.init(i, V4L2_MEMORY_USERPTR);
		ret = q.reqbufs(node, 0);
		fail_on_test(ret && ret != EINVAL);
		userptr_valid = !ret;
		fail_on_test(!mmap_valid && userptr_valid);
		if (caps)
			fail_on_test(userptr_valid ^ !!(caps & V4L2_BUF_CAP_SUPPORTS_USERPTR));

		q.init(i, V4L2_MEMORY_DMABUF);
		ret = q.reqbufs(node, 0);
		fail_on_test(ret && ret != EINVAL);
		dmabuf_valid = !ret;
		fail_on_test(!mmap_valid && dmabuf_valid);
		fail_on_test(dmabuf_valid && (caps != q.g_capabilities()));
		if (caps)
			fail_on_test(dmabuf_valid ^ !!(caps & V4L2_BUF_CAP_SUPPORTS_DMABUF));

		fail_on_test((can_stream && !is_overlay) && !mmap_valid && !userptr_valid && !dmabuf_valid);
		fail_on_test((!can_stream || is_overlay) && (mmap_valid || userptr_valid || dmabuf_valid));
		if (!can_stream || is_overlay)
			continue;

		if (mmap_valid) {
			q.init(i, V4L2_MEMORY_MMAP);
			fail_on_test(q.reqbufs(node, 1));
			fail_on_test(q.g_buffers() == 0);
			fail_on_test(q.g_memory() != V4L2_MEMORY_MMAP);
			fail_on_test(q.g_type() != i);
			fail_on_test(q.reqbufs(node, 1));
			fail_on_test(testQueryBuf(node, i, q.g_buffers()));
			node->valid_memorytype |= 1 << V4L2_MEMORY_MMAP;
		}

		if (userptr_valid) {
			q.init(i, V4L2_MEMORY_USERPTR);
			fail_on_test(q.reqbufs(node, 1));
			fail_on_test(q.g_buffers() == 0);
			fail_on_test(q.g_memory() != V4L2_MEMORY_USERPTR);
			fail_on_test(q.g_type() != i);
			fail_on_test(q.reqbufs(node, 1));
			fail_on_test(testQueryBuf(node, i, q.g_buffers()));
			node->valid_memorytype |= 1 << V4L2_MEMORY_USERPTR;
		}

		if (dmabuf_valid) {
			q.init(i, V4L2_MEMORY_DMABUF);
			fail_on_test(q.reqbufs(node, 1));
			fail_on_test(q.g_buffers() == 0);
			fail_on_test(q.g_memory() != V4L2_MEMORY_DMABUF);
			fail_on_test(q.g_type() != i);
			fail_on_test(q.reqbufs(node, 1));
			fail_on_test(testQueryBuf(node, i, q.g_buffers()));
			node->valid_memorytype |= 1 << V4L2_MEMORY_DMABUF;
		}

		/*
		 * It should be possible to set the same std, timings or
		 * native size even after reqbufs was called.
		 */
		fail_on_test(testCanSetSameTimings(node));

		if (can_rw) {
			char buf = 0;

			if (node->can_capture)
				ret = node->read(&buf, 1);
			else
				ret = node->write(&buf, 1);
			if (ret != -1)
				return fail("Expected -1, got %d\n", ret);
			if (errno != EBUSY)
				return fail("Expected EBUSY, got %d\n", errno);
		}
		fail_on_test(q.reqbufs(node, 0));

		for (m = V4L2_MEMORY_MMAP; m <= V4L2_MEMORY_DMABUF; m++) {
			if (!(node->valid_memorytype & (1 << m)))
				continue;
			cv4l_queue q2(i, m);
			fail_on_test(q.reqbufs(node, 1));
			if (!node->is_m2m) {
				fail_on_test(q2.reqbufs(node->node2, 1) != EBUSY);
				fail_on_test(q2.reqbufs(node->node2) != EBUSY);
				fail_on_test(q.reqbufs(node));
				fail_on_test(q2.reqbufs(node->node2, 1));
				fail_on_test(q2.reqbufs(node->node2));
			}
			memset(&reqbufs, 0xff, sizeof(reqbufs));
			reqbufs.count = 0;
			reqbufs.type = i;
			reqbufs.memory = m;
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &reqbufs));
			fail_on_test(check_0(reqbufs.reserved, sizeof(reqbufs.reserved)));
			q.reqbufs(node);

			ret = q.create_bufs(node, 1);
			if (ret == ENOTTY) {
				warn("VIDIOC_CREATE_BUFS not supported\n");
				break;
			}
			fail_on_test(ret);
			fail_on_test(q.g_buffers() == 0);
			fail_on_test(q.g_type() != i);
			fail_on_test(testQueryBuf(node, i, q.g_buffers()));
			fail_on_test(q.create_bufs(node, 1));
			fail_on_test(testQueryBuf(node, i, q.g_buffers()));
			if (!node->is_m2m)
				fail_on_test(q2.create_bufs(node->node2, 1) != EBUSY);

			memset(&crbufs, 0xff, sizeof(crbufs));
			node->g_fmt(crbufs.format, i);
			crbufs.count = 0;
			crbufs.memory = m;
			fail_on_test(doioctl(node, VIDIOC_CREATE_BUFS, &crbufs));
			fail_on_test(check_0(crbufs.reserved, sizeof(crbufs.reserved)));
			fail_on_test(crbufs.index != q.g_buffers());

			if (node->is_video) {
				cv4l_fmt fmt;

				node->g_fmt(fmt, q.g_type());
				if (V4L2_TYPE_IS_MULTIPLANAR(q.g_type())) {
					fmt.s_num_planes(fmt.g_num_planes() + 1);
					fail_on_test(q.create_bufs(node, 1, &fmt) != EINVAL);
					node->g_fmt(fmt, q.g_type());
				}
				fmt.s_height(fmt.g_height() / 2);
				for (unsigned p = 0; p < fmt.g_num_planes(); p++)
					fmt.s_sizeimage(fmt.g_sizeimage(p) / 2, p);
				fail_on_test(q.create_bufs(node, 1, &fmt) != EINVAL);
				fail_on_test(testQueryBuf(node, fmt.type, q.g_buffers()));
				node->g_fmt(fmt, q.g_type());
				for (unsigned p = 0; p < fmt.g_num_planes(); p++)
					fmt.s_sizeimage(fmt.g_sizeimage(p) * 2, p);
				fail_on_test(q.create_bufs(node, 1, &fmt));
			}
		}
		fail_on_test(q.reqbufs(node));
	}
	return 0;
}

int testExpBuf(struct node *node)
{
	bool have_expbuf = false;
	int type;

	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;
		if (v4l_type_is_overlay(type))
			continue;

		if (testSetupVbi(node, type))
			continue;

		cv4l_queue q(type, V4L2_MEMORY_MMAP);

		if (!(node->valid_memorytype & (1 << V4L2_MEMORY_MMAP))) {
			if (q.has_expbuf(node)) {
				if (node->valid_buftypes)
					fail("VIDIOC_EXPBUF is supported, but the V4L2_MEMORY_MMAP support is missing or malfunctioning.\n");
				fail("VIDIOC_EXPBUF is supported, but the V4L2_MEMORY_MMAP support is missing, probably due to earlier failing format tests.\n");
			}
			return ENOTTY;
		}

		fail_on_test(q.reqbufs(node, 2));
		if (q.has_expbuf(node)) {
			fail_on_test(q.export_bufs(node, q.g_type()));
			have_expbuf = true;
		} else {
			fail_on_test(!q.export_bufs(node, q.g_type()));
		}
		q.close_exported_fds();
		fail_on_test(q.reqbufs(node));
	}
	return have_expbuf ? 0 : ENOTTY;
}

int testReadWrite(struct node *node)
{
	bool can_rw = node->g_caps() & V4L2_CAP_READWRITE;
	int fd_flags = fcntl(node->g_fd(), F_GETFL);
	char buf = 0;
	int ret;

	if (v4l_has_vbi(node->g_v4l_fd()) &&
	    !(node->cur_io_caps & V4L2_IN_CAP_STD)) {
		return 0;
	}

	fcntl(node->g_fd(), F_SETFL, fd_flags | O_NONBLOCK);
	if (node->can_capture)
		ret = node->read(&buf, 1);
	else
		ret = node->write(&buf, 1);
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

	node->reopen();
	fcntl(node->g_fd(), F_SETFL, fd_flags | O_NONBLOCK);

	/* check that the close cleared the busy flag */
	if (node->can_capture)
		ret = node->read(&buf, 1);
	else
		ret = node->write(&buf, 1);
	fail_on_test((ret < 0 && errno != EAGAIN && errno != EBUSY) || ret > 1);
	return 0;
}

static int setupM2M(struct node *node, cv4l_queue &q, bool init = true)
{
	__u32 caps;

	last_m2m_seq.init();

	fail_on_test(q.reqbufs(node, 2));
	fail_on_test(q.mmap_bufs(node));
	caps = q.g_capabilities();
	fail_on_test(node->supports_orphaned_bufs ^ !!(caps & V4L2_BUF_CAP_SUPPORTS_ORPHANED_BUFS));
	if (v4l_type_is_video(q.g_type())) {
		cv4l_fmt fmt(q.g_type());

		node->g_fmt(fmt);
		cur_m2m_fmt = fmt;
		if (init) {
			last_m2m_seq.last_field = fmt.g_field();
			if (v4l_type_is_output(q.g_type()))
				stream_for_fmt(fmt.g_pixelformat());
		}
	}
	for (unsigned i = 0; i < q.g_buffers(); i++) {
		buffer buf(q);

		fail_on_test(buf.querybuf(node, i));
		buf.s_flags(buf.g_flags() & ~V4L2_BUF_FLAG_REQUEST_FD);
		fail_on_test(buf.qbuf(node, q));
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
	}
	fail_on_test(node->streamon(q.g_type()));
	return 0;
}

static int captureBufs(struct node *node, struct node *node_m2m_cap, const cv4l_queue &q,
		cv4l_queue &m2m_q, unsigned frame_count, int pollmode,
		unsigned &capture_count)
{
	static const char *pollmode_str[] = {
		"",
		" (select)",
		" (epoll)",
	};
	unsigned valid_output_flags =
		V4L2_BUF_FLAG_TIMECODE | V4L2_BUF_FLAG_TSTAMP_SRC_MASK |
		V4L2_BUF_FLAG_KEYFRAME | V4L2_BUF_FLAG_PFRAME | V4L2_BUF_FLAG_BFRAME;
	int fd_flags = fcntl(node->g_fd(), F_GETFL);
	cv4l_fmt fmt_q;
	buffer buf(q);
	unsigned count = frame_count;
	unsigned req_idx = q.g_buffers();
	bool stopped = false;
	bool got_eos = false;
	bool got_source_change = false;
	struct epoll_event ev;
	int epollfd = -1;
	int ret;

	if (node->is_m2m) {
		if (count <= q.g_buffers())
			count = 1;
		else
			count -= q.g_buffers();
	}

	capture_count = 0;

	if (show_info) {
		printf("\t    %s%s:\n",
			buftype2s(q.g_type()).c_str(), pollmode_str[pollmode]);
	}

	/*
	 * It should be possible to set the same std, timings or
	 * native size even while streaming.
	 */
	fail_on_test(testCanSetSameTimings(node));

	node->g_fmt(fmt_q, q.g_type());
	if (node->buftype_pixfmts[q.g_type()][fmt_q.g_pixelformat()] &
		V4L2_FMT_FLAG_COMPRESSED)
		valid_output_flags = V4L2_BUF_FLAG_TIMECODE | V4L2_BUF_FLAG_TSTAMP_SRC_MASK;
	if (node->is_m2m) {
		node_m2m_cap->g_fmt(fmt_q, m2m_q.g_type());
		if (node_m2m_cap->buftype_pixfmts[m2m_q.g_type()][fmt_q.g_pixelformat()] &
			V4L2_FMT_FLAG_COMPRESSED)
			valid_output_flags = V4L2_BUF_FLAG_TIMECODE | V4L2_BUF_FLAG_TSTAMP_SRC_MASK;

		struct v4l2_event_subscription sub = { 0 };

		sub.type = V4L2_EVENT_EOS;
		if (node->codec_mask & (STATEFUL_ENCODER | STATEFUL_DECODER))
			doioctl(node, VIDIOC_SUBSCRIBE_EVENT, &sub);
	}

	if (pollmode == POLL_MODE_EPOLL) {
		epollfd = epoll_create1(0);

		fail_on_test(epollfd < 0);
		if (node->is_m2m)
			ev.events = EPOLLIN | EPOLLOUT | EPOLLPRI;
		else if (v4l_type_is_output(q.g_type()))
			ev.events = EPOLLOUT;
		else
			ev.events = EPOLLIN;
		ev.data.fd = node->g_fd();
		fail_on_test(epoll_ctl(epollfd, EPOLL_CTL_ADD, node->g_fd(), &ev));
	}

	if (pollmode)
		fcntl(node->g_fd(), F_SETFL, fd_flags | O_NONBLOCK);
	for (;;) {
		buf.init(q);

		bool can_read = true;
		bool have_event = false;

		if (pollmode == POLL_MODE_SELECT) {
			struct timeval tv = { 2, 0 };
			fd_set rfds, wfds, efds;

			FD_ZERO(&rfds);
			FD_SET(node->g_fd(), &rfds);
			FD_ZERO(&wfds);
			FD_SET(node->g_fd(), &wfds);
			FD_ZERO(&efds);
			FD_SET(node->g_fd(), &efds);
			if (node->is_m2m)
				ret = select(node->g_fd() + 1, &rfds, &wfds, &efds, &tv);
			else if (v4l_type_is_output(q.g_type()))
				ret = select(node->g_fd() + 1, NULL, &wfds, NULL, &tv);
			else
				ret = select(node->g_fd() + 1, &rfds, NULL, NULL, &tv);
			fail_on_test(ret == 0);
			fail_on_test(ret < 0);
			fail_on_test(!FD_ISSET(node->g_fd(), &rfds) &&
				     !FD_ISSET(node->g_fd(), &wfds));
			can_read = FD_ISSET(node->g_fd(), &rfds);
			have_event = FD_ISSET(node->g_fd(), &efds);
		} else if (pollmode == POLL_MODE_EPOLL) {
			ret = epoll_wait(epollfd, &ev, 1, 2000);
			fail_on_test(ret == 0);
			fail_on_test(ret < 0);
			can_read = ev.events & EPOLLIN;
			have_event = ev.events & EPOLLPRI;
		}

		if (have_event) {
			struct v4l2_event ev;

			while (!doioctl(node, VIDIOC_DQEVENT, &ev)) {
				if (ev.type == V4L2_EVENT_EOS) {
					fail_on_test(got_eos);
					got_eos = true;
					fail_on_test(!stopped);
				}
				if (ev.type == V4L2_EVENT_SOURCE_CHANGE) {
					fail_on_test(got_source_change);
					got_source_change = true;
					//fail_on_test(stopped);
					stopped = true;
				}
			}
		}

		ret = EAGAIN;
		if (!node->is_m2m || !stopped)
			ret = buf.dqbuf(node);
		if (ret != EAGAIN) {
			fail_on_test(ret);
			if (show_info)
				printf("\t\t%s Buffer: %d Sequence: %d Field: %s Size: %d Flags: %s Timestamp: %ld.%06lds\n",
				       v4l_type_is_output(buf.g_type()) ? "Out" : "Cap",
				       buf.g_index(), buf.g_sequence(),
				       field2s(buf.g_field()).c_str(), buf.g_bytesused(),
				       bufferflags2s(buf.g_flags()).c_str(),
				       buf.g_timestamp().tv_sec, buf.g_timestamp().tv_usec);
			fail_on_test(buf.check(q, last_seq));
			if (!show_info && !no_progress) {
				printf("\r\t%s: Frame #%03d%s",
				       buftype2s(q.g_type()).c_str(),
				       frame_count - count,
				       pollmode_str[pollmode]);
				if (node->g_trace())
					printf("\n");
				fflush(stdout);
			}
			if (v4l_type_is_capture(buf.g_type()) && node->is_m2m && buf.ts_is_copy()) {
				fail_on_test(buffer_info.find(buf.g_timestamp()) == buffer_info.end());
				struct v4l2_buffer &orig_buf = buffer_info[buf.g_timestamp()];
				fail_on_test(buf.g_field() != orig_buf.field);
				fail_on_test((buf.g_flags() & valid_output_flags) !=
					     (orig_buf.flags & valid_output_flags));
				if (buf.g_flags() & V4L2_BUF_FLAG_TIMECODE)
					fail_on_test(memcmp(&buf.g_timecode(), &orig_buf.timecode,
								sizeof(orig_buf.timecode)));
			}
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);

			if (buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD) {
				buf.querybuf(node, buf.g_index());
				fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD);
				fail_on_test(buf.g_request_fd());
				fail_on_test(!buf.qbuf(node));
				buf.s_flags(V4L2_BUF_FLAG_REQUEST_FD);
				buf.s_request_fd(buf_req_fds[req_idx]);
			}
			fail_on_test(buf.qbuf(node, q));
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
			if (buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD) {
				fail_on_test(doioctl_fd(buf_req_fds[req_idx],
							MEDIA_REQUEST_IOC_QUEUE, 0));
				// testRequests will close some of these request fds,
				// so we need to find the next valid fds.
				do {
					req_idx = (req_idx + 1) % (2 * q.g_buffers());
				} while (buf_req_fds[req_idx] < 0);
			}
			count--;
			if (!node->is_m2m && !count)
				break;
			if (!count && (node->codec_mask & STATEFUL_ENCODER)) {
				struct v4l2_encoder_cmd cmd;

				memset(&cmd, 0, sizeof(cmd));
				cmd.cmd = V4L2_ENC_CMD_STOP;
				fail_on_test(doioctl(node, VIDIOC_ENCODER_CMD, &cmd));
				stopped = true;
			}
			if (!count && (node->codec_mask & STATEFUL_DECODER)) {
				struct v4l2_decoder_cmd cmd;

				memset(&cmd, 0, sizeof(cmd));
				cmd.cmd = V4L2_DEC_CMD_STOP;
				fail_on_test(doioctl(node, VIDIOC_DECODER_CMD, &cmd));
				stopped = true;
			}
		}
		if (!node->is_m2m || !can_read)
			continue;

		buf.init(m2m_q);
		do {
			ret = buf.dqbuf(node_m2m_cap);
		} while (ret == EAGAIN);
		capture_count++;

		if (show_info)
			printf("\t\t%s Buffer: %d Sequence: %d Field: %s Size: %d Flags: %s Timestamp: %ld.%06lds\n",
			       v4l_type_is_output(buf.g_type()) ? "Out" : "Cap",
			       buf.g_index(), buf.g_sequence(),
			       field2s(buf.g_field()).c_str(), buf.g_bytesused(),
			       bufferflags2s(buf.g_flags()).c_str(),
			       buf.g_timestamp().tv_sec, buf.g_timestamp().tv_usec);
		fail_on_test(ret);
		if (v4l_type_is_capture(buf.g_type()) && buf.g_bytesused())
			fail_on_test(buf.check(m2m_q, last_m2m_seq, true));
		if (v4l_type_is_capture(buf.g_type()) && buf.ts_is_copy() && buf.g_bytesused()) {
			fail_on_test(buffer_info.find(buf.g_timestamp()) == buffer_info.end());
			struct v4l2_buffer &orig_buf = buffer_info[buf.g_timestamp()];
			if (cur_fmt.g_field() == cur_m2m_fmt.g_field())
				fail_on_test(buf.g_field() != orig_buf.field);
			fail_on_test((buf.g_flags() & valid_output_flags) !=
					(orig_buf.flags & valid_output_flags));
			if (buf.g_flags() & V4L2_BUF_FLAG_TIMECODE)
				fail_on_test(memcmp(&buf.g_timecode(), &orig_buf.timecode,
							sizeof(orig_buf.timecode)));
		}
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
		if (!count || stopped) {
			if (!(node_m2m_cap->codec_mask & (STATEFUL_ENCODER | STATEFUL_DECODER)))
				break;
			if (buf.g_flags() & V4L2_BUF_FLAG_LAST) {
				fail_on_test(buf.dqbuf(node_m2m_cap) != EPIPE);
				fail_on_test(!got_eos && !got_source_change);
				if (!count)
					break;
				fail_on_test(node_m2m_cap->streamoff(m2m_q.g_type()));
				m2m_q.munmap_bufs(node_m2m_cap);
				fail_on_test(setupM2M(node_m2m_cap, m2m_q, false));
				stopped = false;
				got_source_change = false;

				struct v4l2_decoder_cmd cmd;

				memset(&cmd, 0, sizeof(cmd));
				cmd.cmd = V4L2_DEC_CMD_START;
				fail_on_test(doioctl(node_m2m_cap, VIDIOC_DECODER_CMD, &cmd));
				continue;
			}
		}
		buf.s_flags(buf.g_flags() & ~V4L2_BUF_FLAG_REQUEST_FD);
		fail_on_test(buf.qbuf(node_m2m_cap, m2m_q));
		// If the queued buffer is immediately returned as a last
		// empty buffer, then FLAG_DONE is set here.
		// Need to look at this more closely.
		//fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
	}
	if (pollmode)
		fcntl(node->g_fd(), F_SETFL, fd_flags);
	if (epollfd >= 0)
		close(epollfd);
	if (!show_info && !no_progress) {
		printf("\r\t                                                  \r");
		fflush(stdout);
	}
	if (node->is_m2m)
		printf("\t%s: Captured %d buffers\n", buftype2s(m2m_q.g_type()).c_str(), capture_count);
	
	return 0;
}

static int bufferOutputErrorTest(struct node *node, const buffer &orig_buf)
{
	buffer buf(orig_buf);
	bool have_prepare = false;
	int ret;

	for (unsigned p = 0; p < buf.g_num_planes(); p++) {
		buf.s_bytesused(buf.g_length(p) + 1, p);
		buf.s_data_offset(0, p);
	}
	ret = buf.prepare_buf(node, false);
	fail_on_test(ret != EINVAL && ret != ENOTTY);
	have_prepare = ret != ENOTTY;
	fail_on_test(buf.qbuf(node, false) != EINVAL);

	if (v4l_type_is_planar(buf.g_type())) {
		for (unsigned p = 0; p < buf.g_num_planes(); p++) {
			buf.s_bytesused(buf.g_length(p) / 2, p);
			buf.s_data_offset(buf.g_bytesused(p), p);
		}
		if (have_prepare)
			fail_on_test(buf.prepare_buf(node, false) != EINVAL);
		fail_on_test(buf.qbuf(node, false) != EINVAL);
	}
	buf.init(orig_buf);
	if (have_prepare) {
		fail_on_test(buf.prepare_buf(node, false));
		fail_on_test(buf.check(Prepared, 0));
		buf.init(orig_buf);
		for (unsigned p = 0; p < buf.g_num_planes(); p++) {
			buf.s_bytesused(0xdeadbeef, p);
			buf.s_data_offset(0xdeadbeef, p);
		}
	}
	fail_on_test(buf.qbuf(node, false));
	fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
	for (unsigned p = 0; p < buf.g_num_planes(); p++) {
		fail_on_test(buf.g_bytesused(p) != orig_buf.g_bytesused(p));
		fail_on_test(buf.g_data_offset(p));
	}
	return 0;
}

static int setupMmap(struct node *node, cv4l_queue &q)
{
	fail_on_test(q.mmap_bufs(node));
	for (unsigned i = 0; i < q.g_buffers(); i++) {
		buffer buf(q);
		int ret;

		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.check(q, Unqueued, i));

		for (unsigned p = 0; p < buf.g_num_planes(); p++) {
			// Try a random offset
			fail_on_test(node->mmap(buf.g_length(p),
				buf.g_mem_offset(p) + 0xdeadbeef) != MAP_FAILED);
		}
		fail_on_test(!buf.dqbuf(node));
		if (v4l_type_is_output(buf.g_type()) && i == 0) {
			for (unsigned p = 0; p < buf.g_num_planes(); p++) {
				buf.s_bytesused(buf.g_length(p), p);
				buf.s_data_offset(0, p);
			}
			fill_output_buffer(q, buf);
			fail_on_test(bufferOutputErrorTest(node, buf));
			fail_on_test(buf.querybuf(node, i));
			fail_on_test(buf.check(q, Queued, i));
		} else {
			ret = buf.prepare_buf(node, q);
			fail_on_test(ret && ret != ENOTTY);
			if (ret == 0) {
				fail_on_test(buf.querybuf(node, i));
				fail_on_test(buf.check(q, Prepared, i));
				fail_on_test(!buf.prepare_buf(node));
			}

			fail_on_test(buf.qbuf(node));
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
			fail_on_test(!buf.qbuf(node));
			fail_on_test(!buf.prepare_buf(node));
			// Test with invalid buffer index
			buf.s_index(buf.g_index() + VIDEO_MAX_FRAME);
			fail_on_test(!buf.prepare_buf(node));
			fail_on_test(!buf.qbuf(node));
			fail_on_test(!buf.querybuf(node, buf.g_index()));
			buf.s_index(buf.g_index() - VIDEO_MAX_FRAME);
			fail_on_test(buf.g_index() != i);
		}
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.check(q, Queued, i));
		fail_on_test(!buf.dqbuf(node));
	}
	return 0;
}

int testMmap(struct node *node, struct node *node_m2m_cap, unsigned frame_count,
	     enum poll_mode pollmode)
{
	bool can_stream = node->g_caps() & V4L2_CAP_STREAMING;
	bool have_createbufs = true;
	int type;
	int ret;

	if (!node->valid_buftypes)
		return ENOTTY;

	buffer_info.clear();
	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;
		if (v4l_type_is_overlay(type))
			continue;
		if (node->is_m2m && !v4l_type_is_output(type))
			continue;

		cv4l_queue q(type, V4L2_MEMORY_MMAP);
		cv4l_queue m2m_q(v4l_type_invert(type));
	
		if (testSetupVbi(node, type))
			continue;

		stream_close();

		ret = q.reqbufs(node, 0);
		if (ret) {
			fail_on_test(can_stream);
			return ret;
		}
		fail_on_test(!can_stream);

		fail_on_test(node->streamon(q.g_type()) != EINVAL);
		fail_on_test(node->streamoff(q.g_type()));

		q.init(type, V4L2_MEMORY_MMAP);
		fail_on_test(q.reqbufs(node, 2));
		fail_on_test(node->streamoff(q.g_type()));
		last_seq.init();

		// Test queuing buffers...
		for (unsigned i = 0; i < q.g_buffers(); i++) {
			buffer buf(q);

			fail_on_test(buf.querybuf(node, i));
			fail_on_test(buf.qbuf(node));
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD);
			fail_on_test(buf.g_request_fd());
		}
		// calling STREAMOFF...
		fail_on_test(node->streamoff(q.g_type()));
		// and now we should be able to queue those buffers again since
		// STREAMOFF should return them back to the dequeued state.
		for (unsigned i = 0; i < q.g_buffers(); i++) {
			buffer buf(q);

			fail_on_test(buf.querybuf(node, i));
			fail_on_test(buf.qbuf(node));
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
		}
		// Now request buffers again, freeing the old buffers.
		// Good check for whether all the internal vb2 calls are in
		// balance.
		fail_on_test(q.reqbufs(node, q.g_buffers()));
		fail_on_test(node->g_fmt(cur_fmt, q.g_type()));

		ret = q.create_bufs(node, 0);
		fail_on_test(ret != ENOTTY && ret != 0);
		if (ret == ENOTTY)
			have_createbufs = false;
		if (have_createbufs) {
			cv4l_fmt fmt(cur_fmt);

			if (node->is_video) {
				last_seq.last_field = cur_fmt.g_field();
				fmt.s_height(fmt.g_height() / 2);
				for (unsigned p = 0; p < fmt.g_num_planes(); p++)
					fmt.s_sizeimage(fmt.g_sizeimage(p) / 2, p);
				fail_on_test(q.create_bufs(node, 1, &fmt) != EINVAL);
				fail_on_test(testQueryBuf(node, cur_fmt.type, q.g_buffers()));
				fmt = cur_fmt;
				for (unsigned p = 0; p < fmt.g_num_planes(); p++)
					fmt.s_sizeimage(fmt.g_sizeimage(p) * 2, p);
			}
			fail_on_test(q.create_bufs(node, 1, &fmt));
			fail_on_test(q.reqbufs(node, 2));
		}
		if (v4l_type_is_output(type))
			stream_for_fmt(cur_fmt.g_pixelformat());

		fail_on_test(setupMmap(node, q));

		if (node->inject_error(VIVID_CID_START_STR_ERROR))
			fail_on_test(!node->streamon(q.g_type()));

		if (node->codec_mask & STATEFUL_DECODER) {
			struct v4l2_event_subscription sub = { 0 };

			sub.type = V4L2_EVENT_SOURCE_CHANGE;
			fail_on_test(doioctl(node, VIDIOC_SUBSCRIBE_EVENT, &sub));
		}

		fail_on_test(node->streamon(q.g_type()));
		fail_on_test(node->streamon(q.g_type()));

		unsigned capture_count;

		if (node->is_m2m) {
			if (node->codec_mask & STATEFUL_DECODER) {
				int fd_flags = fcntl(node->g_fd(), F_GETFL);
				struct timeval tv = { 1, 0 };
				fd_set efds;
				v4l2_event ev;

				fcntl(node->g_fd(), F_SETFL, fd_flags | O_NONBLOCK);
				FD_ZERO(&efds);
				FD_SET(node->g_fd(), &efds);
				ret = select(node->g_fd() + 1, NULL, NULL, &efds, &tv);
				fail_on_test(ret < 0);
				fail_on_test(ret == 0);
				fail_on_test(node->dqevent(ev));
				fcntl(node->g_fd(), F_SETFL, fd_flags);
				fail_on_test(ev.type != V4L2_EVENT_SOURCE_CHANGE);
				fail_on_test(!(ev.u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION));
			}
			fail_on_test(setupM2M(node_m2m_cap, m2m_q));
		}

		fail_on_test(captureBufs(node, node_m2m_cap, q, m2m_q, frame_count,
					 pollmode, capture_count));
		fail_on_test(node->streamoff(q.g_type()));
		fail_on_test(node->streamoff(q.g_type()));
		if (node->is_m2m)
			fail_on_test(node_m2m_cap->streamoff(m2m_q.g_type()));

		if (node->codec_mask & STATEFUL_ENCODER) {
			struct v4l2_encoder_cmd cmd;
			buffer buf_cap(m2m_q);

			memset(&cmd, 0, sizeof(cmd));
			cmd.cmd = V4L2_ENC_CMD_STOP;

			/* No buffers are queued, call STREAMON, then STOP */
			fail_on_test(node->streamon(q.g_type()));
			fail_on_test(node_m2m_cap->streamon(m2m_q.g_type()));
			fail_on_test(doioctl(node, VIDIOC_ENCODER_CMD, &cmd));

			fail_on_test(buf_cap.querybuf(node, 0));
			fail_on_test(buf_cap.qbuf(node));
			fail_on_test(buf_cap.dqbuf(node));
			fail_on_test(!(buf_cap.g_flags() & V4L2_BUF_FLAG_LAST));
			for (unsigned p = 0; p < buf_cap.g_num_planes(); p++)
				fail_on_test(buf_cap.g_bytesused(p));
			fail_on_test(node->streamoff(q.g_type()));
			fail_on_test(node_m2m_cap->streamoff(m2m_q.g_type()));

			/* Call STREAMON, queue one CAPTURE buffer, then STOP */
			fail_on_test(node->streamon(q.g_type()));
			fail_on_test(node_m2m_cap->streamon(m2m_q.g_type()));
			fail_on_test(buf_cap.querybuf(node, 0));
			fail_on_test(buf_cap.qbuf(node));
			fail_on_test(doioctl(node, VIDIOC_ENCODER_CMD, &cmd));

			fail_on_test(buf_cap.dqbuf(node));
			fail_on_test(!(buf_cap.g_flags() & V4L2_BUF_FLAG_LAST));
			for (unsigned p = 0; p < buf_cap.g_num_planes(); p++)
				fail_on_test(buf_cap.g_bytesused(p));
			fail_on_test(node->streamoff(q.g_type()));
			fail_on_test(node_m2m_cap->streamoff(m2m_q.g_type()));
		}

		if (node->codec_mask & STATEFUL_DECODER) {
			struct v4l2_decoder_cmd cmd;
			buffer buf_cap(m2m_q);

			memset(&cmd, 0, sizeof(cmd));
			cmd.cmd = V4L2_DEC_CMD_STOP;

			/* No buffers are queued, call STREAMON, then STOP */
			fail_on_test(node->streamon(q.g_type()));
			fail_on_test(node_m2m_cap->streamon(m2m_q.g_type()));
			fail_on_test(doioctl(node, VIDIOC_DECODER_CMD, &cmd));

			fail_on_test(buf_cap.querybuf(node, 0));
			fail_on_test(buf_cap.qbuf(node));
			fail_on_test(buf_cap.dqbuf(node));
			fail_on_test(!(buf_cap.g_flags() & V4L2_BUF_FLAG_LAST));
			for (unsigned p = 0; p < buf_cap.g_num_planes(); p++)
				fail_on_test(buf_cap.g_bytesused(p));
			fail_on_test(node->streamoff(q.g_type()));
			fail_on_test(node_m2m_cap->streamoff(m2m_q.g_type()));

			/* Call STREAMON, queue one CAPTURE buffer, then STOP */
			fail_on_test(node->streamon(q.g_type()));
			fail_on_test(node_m2m_cap->streamon(m2m_q.g_type()));
			fail_on_test(buf_cap.querybuf(node, 0));
			fail_on_test(buf_cap.qbuf(node));
			fail_on_test(doioctl(node, VIDIOC_DECODER_CMD, &cmd));

			fail_on_test(buf_cap.dqbuf(node));
			fail_on_test(!(buf_cap.g_flags() & V4L2_BUF_FLAG_LAST));
			for (unsigned p = 0; p < buf_cap.g_num_planes(); p++)
				fail_on_test(buf_cap.g_bytesused(p));
			fail_on_test(node->streamoff(q.g_type()));
			fail_on_test(node_m2m_cap->streamoff(m2m_q.g_type()));
		}

		if (node->supports_orphaned_bufs) {
			fail_on_test(q.reqbufs(node, 0));
			q.munmap_bufs(node);
		} else if (q.reqbufs(node, 0) != EBUSY) {
			// It's either a bug or this driver should set
			// V4L2_BUF_CAP_SUPPORTS_ORPHANED_BUFS
			warn("Can free buffers even if still mmap()ed\n");
			q.munmap_bufs(node);
		} else {
			q.munmap_bufs(node);
			fail_on_test(q.reqbufs(node, 0));
		}

		if (node->is_m2m) {
			if (node->supports_orphaned_bufs) {
				fail_on_test(m2m_q.reqbufs(node, 0));
				m2m_q.munmap_bufs(node_m2m_cap);
			} else if (m2m_q.reqbufs(node, 0) != EBUSY) {
				// It's either a bug or this driver should set
				// V4L2_BUF_CAP_SUPPORTS_ORPHANED_BUFS
				warn("Can free buffers even if still mmap()ed\n");
				q.munmap_bufs(node);
			} else {
				m2m_q.munmap_bufs(node_m2m_cap);
				fail_on_test(m2m_q.reqbufs(node_m2m_cap, 0));
			}
			fail_on_test(!capture_count);
		}
		stream_close();
	}
	return 0;
}

static int setupUserPtr(struct node *node, cv4l_queue &q)
{
	fail_on_test(q.alloc_bufs(node));

	for (unsigned i = 0; i < q.g_buffers(); i++) {
		buffer buf(q);
		int ret;

		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.check(q, Unqueued, i));

		for (unsigned p = 0; p < buf.g_num_planes(); p++) {
			// This should not work!
			fail_on_test(node->mmap(buf.g_length(p), 0) != MAP_FAILED);
		}

		ret = ENOTTY;
		// Try to use VIDIOC_PREPARE_BUF for every other buffer
		if ((i & 1) == 0) {
			for (unsigned p = 0; p < buf.g_num_planes(); p++)
				buf.s_userptr(0UL, p);
			ret = buf.prepare_buf(node);
			fail_on_test(!ret);
			for (unsigned p = 0; p < buf.g_num_planes(); p++)
				buf.s_userptr((char *)q.g_userptr(i, p) + 0xdeadbeef, p);
			ret = buf.prepare_buf(node);
			fail_on_test(!ret);
			for (unsigned p = 0; p < buf.g_num_planes(); p++)
				buf.s_userptr(q.g_userptr(i, p), p);
			ret = buf.prepare_buf(node, q);
			fail_on_test(ret && ret != ENOTTY);

			if (ret == 0) {
				fail_on_test(buf.querybuf(node, i));
				fail_on_test(buf.check(q, Prepared, i));
				for (unsigned p = 0; p < buf.g_num_planes(); p++) {
					buf.s_userptr(0UL, p);
					buf.s_bytesused(0, p);
					buf.s_length(0, p);
				}
			}
		}
		if (ret == ENOTTY) {
			for (unsigned p = 0; p < buf.g_num_planes(); p++)
				buf.s_userptr(0UL, p);
			ret = buf.qbuf(node);
			fail_on_test(!ret);

			for (unsigned p = 0; p < buf.g_num_planes(); p++)
				buf.s_userptr((char *)q.g_userptr(i, p) + 0xdeadbeef, p);
			ret = buf.qbuf(node);
			fail_on_test(!ret);

			for (unsigned p = 0; p < buf.g_num_planes(); p++)
				buf.s_userptr(q.g_userptr(i, p), p);
		}

		if ((i & 1) == 0)
			fail_on_test(buf.qbuf(node));
		else
			fail_on_test(buf.qbuf(node, q));

		for (unsigned p = 0; p < buf.g_num_planes(); p++) {
			fail_on_test(buf.g_userptr(p) != q.g_userptr(i, p));
			fail_on_test(buf.g_length(p) != q.g_length(p));
			if (v4l_type_is_output(q.g_type()))
				fail_on_test(!buf.g_bytesused(p));
		}
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.check(q, Queued, i));
	}
	return 0;
}

int testUserPtr(struct node *node, struct node *node_m2m_cap, unsigned frame_count,
		enum poll_mode pollmode)
{
	bool can_stream = node->g_caps() & V4L2_CAP_STREAMING;
	int type;
	int ret;

	if (!node->valid_buftypes)
		return ENOTTY;

	buffer_info.clear();
	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;
		if (v4l_type_is_overlay(type))
			continue;
		if (node->is_m2m && !v4l_type_is_output(type))
			continue;

		cv4l_queue q(type, V4L2_MEMORY_USERPTR);
		cv4l_queue m2m_q(v4l_type_invert(type));

		if (testSetupVbi(node, type))
			continue;

		stream_close();
		ret = q.reqbufs(node, 0);
		if (ret) {
			fail_on_test(!can_stream && ret != ENOTTY);
			fail_on_test(can_stream && ret != EINVAL);
			return ENOTTY;
		}
		fail_on_test(!can_stream);

		q.init(type, V4L2_MEMORY_USERPTR);
		fail_on_test(q.reqbufs(node, 2));
		fail_on_test(node->streamoff(q.g_type()));
		last_seq.init();
		if (node->is_video)
			last_seq.last_field = cur_fmt.g_field();
		if (v4l_type_is_output(type))
			stream_for_fmt(cur_fmt.g_pixelformat());

		fail_on_test(setupUserPtr(node, q));

		if (node->codec_mask & STATEFUL_DECODER) {
			struct v4l2_event_subscription sub = { 0 };

			sub.type = V4L2_EVENT_SOURCE_CHANGE;
			fail_on_test(doioctl(node, VIDIOC_SUBSCRIBE_EVENT, &sub));
		}

		fail_on_test(node->streamon(q.g_type()));
		fail_on_test(node->streamon(q.g_type()));

		unsigned capture_count;

		if (node->is_m2m) {
			if (node->codec_mask & STATEFUL_DECODER) {
				v4l2_event ev;

				fail_on_test(node->dqevent(ev));
				fail_on_test(ev.type != V4L2_EVENT_SOURCE_CHANGE);
				fail_on_test(!(ev.u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION));
			}
			fail_on_test(setupM2M(node_m2m_cap, m2m_q));
		}
		fail_on_test(captureBufs(node, node_m2m_cap, q, m2m_q, frame_count,
					 pollmode, capture_count));
		fail_on_test(node->streamoff(q.g_type()));
		fail_on_test(node->streamoff(q.g_type()));
		if (node->is_m2m) {
			fail_on_test(node_m2m_cap->streamoff(m2m_q.g_type()));
			m2m_q.munmap_bufs(node_m2m_cap);
			fail_on_test(m2m_q.reqbufs(node_m2m_cap, 0));
			fail_on_test(!capture_count);
		}
		stream_close();
	}
	return 0;
}

static int setupDmaBuf(struct node *expbuf_node, struct node *node,
		       cv4l_queue &q, cv4l_queue &exp_q)
{
	fail_on_test(exp_q.reqbufs(expbuf_node, q.g_buffers()));
	fail_on_test(exp_q.g_buffers() < q.g_buffers());
	fail_on_test(exp_q.export_bufs(expbuf_node, exp_q.g_type()));

	for (unsigned i = 0; i < q.g_buffers(); i++) {
		buffer buf(q);
		int ret;

		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.check(q, Unqueued, i));
		fail_on_test(exp_q.g_num_planes() < buf.g_num_planes());
		for (unsigned p = 0; p < buf.g_num_planes(); p++) {
			fail_on_test(exp_q.g_length(p) < buf.g_length(p));
			// This should not work!
			fail_on_test(node->mmap(buf.g_length(p), 0) != MAP_FAILED);
			q.s_fd(i, p, exp_q.g_fd(i, p));
		}

		for (unsigned p = 0; p < buf.g_num_planes(); p++)
			buf.s_fd(0xdeadbeef + q.g_fd(i, p), p);
		ret = buf.prepare_buf(node);
		fail_on_test(!ret);
	}
	fail_on_test(q.mmap_bufs(node));
	for (unsigned i = 0; i < q.g_buffers(); i++) {
		buffer buf(q);
		int ret;

		buf.init(q, i);
		fail_on_test(buf.querybuf(node, i));
		for (unsigned p = 0; p < buf.g_num_planes(); p++)
			buf.s_fd(q.g_fd(i, p), p);
		ret = buf.prepare_buf(node, q);
		if (ret != ENOTTY) {
			fail_on_test(ret);
			fail_on_test(buf.querybuf(node, i));
			fail_on_test(buf.check(q, Prepared, i));
			for (unsigned p = 0; p < buf.g_num_planes(); p++) {
				buf.s_fd(-1, p);
				buf.s_bytesused(0, p);
				buf.s_length(0, p);
			}
		}

		fail_on_test(buf.qbuf(node, false));
		for (unsigned p = 0; p < buf.g_num_planes(); p++) {
			fail_on_test(buf.g_fd(p) != q.g_fd(i, p));;
			fail_on_test(buf.g_length(p) != q.g_length(p));
			if (v4l_type_is_output(q.g_type()))
				fail_on_test(!buf.g_bytesused(p));
		}
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.check(q, Queued, i));
	}
	return 0;
}

int testDmaBuf(struct node *expbuf_node, struct node *node, struct node *node_m2m_cap,
	       unsigned frame_count, enum poll_mode pollmode)
{
	bool can_stream = node->g_caps() & V4L2_CAP_STREAMING;
	int expbuf_type, type;
	int ret;

	if (!node->valid_buftypes)
		return ENOTTY;

	buffer_info.clear();
	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;
		if (v4l_type_is_sdr(type))
			continue;
		if (v4l_type_is_overlay(type))
			continue;
		if (node->is_m2m && !v4l_type_is_output(type))
			continue;

		if (expbuf_node->g_caps() & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
			expbuf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		else if (expbuf_node->g_caps() & V4L2_CAP_VIDEO_CAPTURE)
			expbuf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		else if (expbuf_node->g_caps() & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
			expbuf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		else 
			expbuf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

		cv4l_queue q(type, V4L2_MEMORY_DMABUF);
		cv4l_queue m2m_q(v4l_type_invert(type));
		cv4l_queue exp_q(expbuf_type, V4L2_MEMORY_MMAP);

		if (testSetupVbi(node, type))
			continue;

		stream_close();
		ret = q.reqbufs(node, 0);
		if (ret) {
			fail_on_test(!can_stream && ret != ENOTTY);
			fail_on_test(can_stream && ret != EINVAL);
			return ENOTTY;
		}
		fail_on_test(!can_stream);

		fail_on_test(q.reqbufs(node, 2));
		fail_on_test(node->streamoff(q.g_type()));
		last_seq.init();
		if (node->is_video)
			last_seq.last_field = cur_fmt.g_field();
		if (v4l_type_is_output(type))
			stream_for_fmt(cur_fmt.g_pixelformat());

		fail_on_test(setupDmaBuf(expbuf_node, node, q, exp_q));

		if (node->codec_mask & STATEFUL_DECODER) {
			struct v4l2_event_subscription sub = { 0 };

			sub.type = V4L2_EVENT_SOURCE_CHANGE;
			fail_on_test(doioctl(node, VIDIOC_SUBSCRIBE_EVENT, &sub));
		}

		fail_on_test(node->streamon(q.g_type()));
		fail_on_test(node->streamon(q.g_type()));

		unsigned capture_count;

		if (node->is_m2m) {
			if (node->codec_mask & STATEFUL_DECODER) {
				v4l2_event ev;

				fail_on_test(node->dqevent(ev));
				fail_on_test(ev.type != V4L2_EVENT_SOURCE_CHANGE);
				fail_on_test(!(ev.u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION));
			}
			fail_on_test(setupM2M(node_m2m_cap, m2m_q));
		}
		fail_on_test(captureBufs(node, node_m2m_cap, q, m2m_q, frame_count,
					 pollmode, capture_count));
		fail_on_test(node->streamoff(q.g_type()));
		fail_on_test(node->streamoff(q.g_type()));
		if (node->supports_orphaned_bufs) {
			fail_on_test(q.reqbufs(node, 0));
			exp_q.close_exported_fds();
		} else if (q.reqbufs(node, 0) != EBUSY) {
			// It's either a bug or this driver should set
			// V4L2_BUF_CAP_SUPPORTS_ORPHANED_BUFS
			warn("Can free buffers even if exported DMABUF fds still open\n");
			q.munmap_bufs(node);
		} else {
			exp_q.close_exported_fds();
			fail_on_test(q.reqbufs(node, 0));
		}
		if (node->is_m2m) {
			fail_on_test(node_m2m_cap->streamoff(m2m_q.g_type()));
			if (node_m2m_cap->supports_orphaned_bufs) {
				fail_on_test(m2m_q.reqbufs(node, 0));
				m2m_q.munmap_bufs(node_m2m_cap);
			} else if (m2m_q.reqbufs(node_m2m_cap, 0) != EBUSY) {
				// It's either a bug or this driver should set
				// V4L2_BUF_CAP_SUPPORTS_ORPHANED_BUFS
				warn("Can free buffers even if still mmap()ed\n");
				q.munmap_bufs(node);
			} else {
				m2m_q.munmap_bufs(node_m2m_cap);
				fail_on_test(m2m_q.reqbufs(node_m2m_cap, 0));
			}
			fail_on_test(!capture_count);
		}
		stream_close();
	}
	return 0;
}

int testRequests(struct node *node, bool test_streaming)
{
	filehandles fhs;
	int media_fd = fhs.add(mi_get_media_fd(node->g_fd(), node->bus_info));
	int req_fd;
	qctrl_map::iterator iter;
	struct test_query_ext_ctrl valid_qctrl;
	v4l2_ext_controls ctrls;
	v4l2_ext_control ctrl;
	bool have_controls;
	int ret;

	if (node->buf_caps & V4L2_BUF_CAP_SUPPORTS_REQUESTS)
		fail_on_test(media_fd < 0);

	memset(&valid_qctrl, 0, sizeof(valid_qctrl));
	memset(&ctrls, 0, sizeof(ctrls));
	memset(&ctrl, 0, sizeof(ctrl));
	for (iter = node->controls.begin(); iter != node->controls.end(); ++iter) {
		test_query_ext_ctrl &qctrl = iter->second;

		if (qctrl.type != V4L2_CTRL_TYPE_INTEGER &&
		    qctrl.type != V4L2_CTRL_TYPE_BOOLEAN)
			continue;
		if (qctrl.flags & V4L2_CTRL_FLAG_WRITE_ONLY)
			continue;
		if (is_vivid && V4L2_CTRL_ID2WHICH(qctrl.id) == V4L2_CTRL_CLASS_VIVID)
			continue;
		if (qctrl.minimum != qctrl.maximum) {
			valid_qctrl = qctrl;
			ctrl.id = qctrl.id;
			break;
		}
	}

	if (ctrl.id == 0) {
		info("could not test the Request API, no suitable control found\n");
		return (node->buf_caps & V4L2_BUF_CAP_SUPPORTS_REQUESTS) ?
			0 : ENOTTY;
	}

	ctrls.which = V4L2_CTRL_WHICH_REQUEST_VAL;
	ret = doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls);
	fail_on_test(ret != EINVAL && ret != EBADR && ret != ENOTTY);
	have_controls = ret != ENOTTY;

	if (media_fd < 0 || ret == EBADR) {
		fail_on_test(node->buf_caps & V4L2_BUF_CAP_SUPPORTS_REQUESTS);
		return ENOTTY;
	}
	if (have_controls) {
		ctrls.request_fd = 10;
		fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls) != EINVAL);
	}
	ret = doioctl_fd(media_fd, MEDIA_IOC_REQUEST_ALLOC, &req_fd);
	if (ret == ENOTTY) {
		fail_on_test(node->buf_caps & V4L2_BUF_CAP_SUPPORTS_REQUESTS);
		return ENOTTY;
	}
	fail_on_test(ret);
	fhs.add(req_fd);
	fail_on_test(req_fd < 0);
	if (have_controls) {
		ctrls.request_fd = req_fd;
		fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls) != EACCES);
	}
	fail_on_test(doioctl_fd(req_fd, MEDIA_REQUEST_IOC_QUEUE, 0) != ENOENT);
	fail_on_test(doioctl_fd(req_fd, MEDIA_REQUEST_IOC_REINIT, 0));
	fhs.del(media_fd);
	fail_on_test(doioctl_fd(req_fd, MEDIA_REQUEST_IOC_QUEUE, 0) != ENOENT);
	fail_on_test(doioctl_fd(req_fd, MEDIA_REQUEST_IOC_REINIT, 0));
	fhs.del(req_fd);
	if (have_controls)
		fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls) != EINVAL);
	fail_on_test(doioctl_fd(req_fd, MEDIA_REQUEST_IOC_QUEUE, 0) != EBADF);
	fail_on_test(doioctl_fd(req_fd, MEDIA_REQUEST_IOC_REINIT, 0) != EBADF);

	media_fd = fhs.add(mi_get_media_fd(node->g_fd(), node->bus_info));
	fail_on_test(doioctl_fd(media_fd, MEDIA_IOC_REQUEST_ALLOC, &req_fd));
	fhs.add(req_fd);
	ctrls.count = 1;
	ctrls.controls = &ctrl;
	if (have_controls) {
		ctrl.value = valid_qctrl.minimum;
		ctrls.which = 0;
		fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls));
		ctrl.value = valid_qctrl.maximum;
		ctrls.which = V4L2_CTRL_WHICH_REQUEST_VAL;
		ctrls.request_fd = req_fd;
		fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls));
		ctrl.value = valid_qctrl.minimum;
		ctrls.request_fd = req_fd;
		fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls) != EACCES);
		ctrls.which = 0;
		fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls));
		fail_on_test(ctrl.value != valid_qctrl.minimum);
		ctrls.request_fd = req_fd;
		ctrls.which = V4L2_CTRL_WHICH_REQUEST_VAL;
		ctrl.id = 1;
		fail_on_test(!doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls));
		fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls) != EACCES);
	}
	ctrl.id = valid_qctrl.id;
	fhs.del(req_fd);
	fhs.del(media_fd);
	node->reopen();

	int type = node->g_type();
	if (node->is_m2m)
		type = v4l_type_invert(type);
	if (v4l_type_is_vbi(type)) {
		cv4l_fmt fmt;

		node->g_fmt(fmt, type);
		node->s_fmt(fmt, type);
	}
	
	if (!(node->valid_buftypes & (1 << type))) {
		fail_on_test(node->buf_caps & V4L2_BUF_CAP_SUPPORTS_REQUESTS);
		return ENOTTY;
	}
	bool supports_requests = node->buf_caps & V4L2_BUF_CAP_SUPPORTS_REQUESTS;

	buffer_info.clear();

	cv4l_queue q(type, V4L2_MEMORY_MMAP);
	cv4l_queue m2m_q(v4l_type_invert(type));

	q.init(type, V4L2_MEMORY_MMAP);
	fail_on_test(q.reqbufs(node, 2));

	unsigned min_bufs = q.g_buffers();
	fail_on_test(q.reqbufs(node, min_bufs + 4));
	unsigned num_bufs = q.g_buffers();
	unsigned num_requests = 2 * num_bufs;
	last_seq.init();

	media_fd = fhs.add(mi_get_media_fd(node->g_fd(), node->bus_info));

	for (unsigned i = 0; i < num_requests; i++) {
		fail_on_test(doioctl_fd(media_fd, MEDIA_IOC_REQUEST_ALLOC, &buf_req_fds[i]));
		fhs.add(buf_req_fds[i]);
		fail_on_test(buf_req_fds[i] < 0);
		fail_on_test(!doioctl_fd(buf_req_fds[i], MEDIA_REQUEST_IOC_QUEUE, 0));
	}
	fhs.del(media_fd);

	buffer buf(q);

	fail_on_test(buf.querybuf(node, 0));
	ret = buf.qbuf(node);
	fail_on_test(ret && ret != EBADR);
	fail_on_test(buf.querybuf(node, 1));
	buf.s_flags(V4L2_BUF_FLAG_REQUEST_FD);
	buf.s_request_fd(buf_req_fds[1]);
	if (ret == EBADR)
		fail_on_test(buf.qbuf(node));
	else
		fail_on_test(!buf.qbuf(node));

	node->reopen();

	q.init(type, V4L2_MEMORY_MMAP);
	fail_on_test(q.reqbufs(node, num_bufs));

	if (node->is_m2m) {
		fail_on_test(m2m_q.reqbufs(node, 2));
		fail_on_test(node->streamon(m2m_q.g_type()));

		buffer buf(m2m_q);

		fail_on_test(buf.querybuf(node, 0));
		buf.s_flags(V4L2_BUF_FLAG_REQUEST_FD);
		buf.s_request_fd(buf_req_fds[0]);
		fail_on_test(!buf.qbuf(node));
		fail_on_test(node->streamoff(m2m_q.g_type()));
		fail_on_test(m2m_q.reqbufs(node, 0));

		fail_on_test(setupM2M(node, m2m_q));
	}

	ctrls.which = V4L2_CTRL_WHICH_REQUEST_VAL;
	// Test queuing buffers...
	for (unsigned i = 0; i < num_bufs; i++) {
		buffer buf(q);

		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.g_request_fd());
		buf.s_flags(V4L2_BUF_FLAG_REQUEST_FD);
		if (i == 0) {
			buf.s_request_fd(-1);
			fail_on_test(!buf.qbuf(node));
			buf.s_request_fd(0xdead);
			fail_on_test(!buf.qbuf(node));
		}
		buf.s_request_fd(buf_req_fds[i]);
		if (v4l_type_is_video(buf.g_type()))
			buf.s_field(V4L2_FIELD_ANY);
		if (!(i & 1)) {
			fail_on_test(buf.prepare_buf(node) != EINVAL);
			buf.s_flags(0);
			if (node->inject_error(VIVID_CID_BUF_PREPARE_ERROR))
				fail_on_test(buf.prepare_buf(node) != EINVAL);
			fail_on_test(buf.prepare_buf(node));
			fail_on_test(buf.querybuf(node, i));
			fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_PREPARED));
			buf.s_flags(buf.g_flags() | V4L2_BUF_FLAG_REQUEST_FD);
			buf.s_request_fd(buf_req_fds[i]);
		}
		int err = buf.qbuf(node);
		if (!err) {
			fail_on_test(!supports_requests);
			fail_on_test(!buf.qbuf(node));
		} else {
			fail_on_test(supports_requests);
			fail_on_test(err != EBADR);
		}
		if (err) {
			fail_on_test(node->streamoff(q.g_type()));
			fail_on_test(q.reqbufs(node, 0));
			if (node->is_m2m) {
				fail_on_test(node->streamoff(m2m_q.g_type()));
				m2m_q.munmap_bufs(node);
				fail_on_test(m2m_q.reqbufs(node, 0));
			}
			node->reopen();
			return ENOTTY;
		}
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
		fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_IN_REQUEST));
		fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD));
		fail_on_test(buf.g_request_fd() < 0);
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
		fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_IN_REQUEST));
		fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD));
		fail_on_test(buf.g_request_fd() < 0);
		if (i & 1)
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_PREPARED);
		else
			fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_PREPARED));
		buf.s_request_fd(buf_req_fds[i]);
		fail_on_test(!buf.qbuf(node));

		ctrl.value = (i & 1) ? valid_qctrl.maximum : valid_qctrl.minimum;
		ctrls.request_fd = buf_req_fds[i];
		fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls));
		fail_on_test(doioctl_fd(buf_req_fds[i], MEDIA_REQUEST_IOC_REINIT, 0));

		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_IN_REQUEST);
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD);

		ctrls.request_fd = buf_req_fds[i];
		fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls));

		if (i)
			fail_on_test(!buf.qbuf(node));
		buf.s_flags(buf.g_flags() | V4L2_BUF_FLAG_REQUEST_FD);
		buf.s_request_fd(buf_req_fds[i]);
		buf.s_field(V4L2_FIELD_ANY);
		fail_on_test(buf.qbuf(node));
		if (v4l_type_is_video(buf.g_type()) && v4l_type_is_output(buf.g_type()))
			fail_on_test(buf.g_field() == V4L2_FIELD_ANY);
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_IN_REQUEST));
		fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD));
		if ((i & 1) && node->inject_error(i > num_bufs / 2 ?
						  VIVID_CID_BUF_PREPARE_ERROR :
						  VIVID_CID_REQ_VALIDATE_ERROR))
			fail_on_test(doioctl_fd(buf_req_fds[i],
						MEDIA_REQUEST_IOC_QUEUE, 0) != EINVAL);
		ret = doioctl_fd(buf_req_fds[i], MEDIA_REQUEST_IOC_QUEUE, 0);
		if (node->codec_mask & STATELESS_DECODER) {
			fail_on_test(ret != ENOENT);
			test_streaming = false;
			break;
		}
		fail_on_test(ret);
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_IN_REQUEST);
		fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD));
		fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_QUEUED));
		fail_on_test(doioctl_fd(buf_req_fds[i], MEDIA_REQUEST_IOC_REINIT, 0) != EBUSY);
		fail_on_test(doioctl_fd(buf_req_fds[i], MEDIA_REQUEST_IOC_QUEUE, 0) != EBUSY);
		if (i >= min_bufs) {
			close(buf_req_fds[i]);
			buf_req_fds[i] = -1;
		}
		if (i == min_bufs - 1) {
			if (node->inject_error(VIVID_CID_START_STR_ERROR))
				fail_on_test(!node->streamon(q.g_type()));
			fail_on_test(node->streamon(q.g_type()));
		}
	}

	fail_on_test(node->g_fmt(cur_fmt, q.g_type()));

	if (test_streaming) {
		unsigned capture_count;

		fail_on_test(captureBufs(node, node, q, m2m_q, num_bufs,
					 POLL_MODE_SELECT, capture_count));
	}
	fail_on_test(node->streamoff(q.g_type()));

	for (unsigned i = 0; test_streaming && i < min_bufs; i++) {
		buffer buf(q);

		ctrls.request_fd = buf_req_fds[i];
		fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls));
		fail_on_test(ctrl.value != ((i & 1) ? valid_qctrl.maximum :
			     valid_qctrl.minimum));
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD);
		fail_on_test(buf.g_request_fd());
		struct pollfd pfd = {
			buf_req_fds[i],
			POLLPRI, 0
		};
		fail_on_test(poll(&pfd, 1, 100) != 1);
		fail_on_test(doioctl_fd(buf_req_fds[i], MEDIA_REQUEST_IOC_QUEUE, 0) != EBUSY);
		fail_on_test(doioctl_fd(buf_req_fds[i], MEDIA_REQUEST_IOC_REINIT, 0));
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD);
		fail_on_test(buf.g_request_fd());
		fhs.del(buf_req_fds[i]);
		ctrls.request_fd = buf_req_fds[i];
		fail_on_test(!doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls));
	}
	for (unsigned i = 0; i < num_requests; i++)
		if (buf_req_fds[i] >= 0)
			fhs.del(buf_req_fds[i]);

	ctrls.which = 0;
	fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls));
	if (test_streaming)
		fail_on_test(ctrl.value != (((num_bufs - 1) & 1) ? valid_qctrl.maximum :
					    valid_qctrl.minimum));

	fail_on_test(q.reqbufs(node, 0));
	if (node->is_m2m) {
		fail_on_test(node->streamoff(m2m_q.g_type()));
		m2m_q.munmap_bufs(node);
		fail_on_test(m2m_q.reqbufs(node, 0));
	}

	return 0;
}

static int testBlockingDQBuf(struct node *node, cv4l_queue &q)
{
	int pid_dqbuf;
	int pid_streamoff;
	int pid;

	fail_on_test(q.reqbufs(node, 2));
	fail_on_test(node->streamon(q.g_type()));

	/*
	 * This test checks if a blocking wait in VIDIOC_DQBUF doesn't block
	 * other ioctls.
	 */
	fflush(stdout);
	pid_dqbuf = fork();
	fail_on_test(pid_dqbuf == -1);

	if (pid_dqbuf == 0) { // Child
		/*
		 * In the child process we call VIDIOC_DQBUF and wait
		 * indefinitely since no buffers are queued.
		 */
		cv4l_buffer buf(q.g_type(), V4L2_MEMORY_MMAP);

		node->dqbuf(buf);
		exit(0);
	}

	/* Wait for the child process to start and block */
	usleep(100000);
	pid = waitpid(pid_dqbuf, NULL, WNOHANG);
	/* Check that it is really blocking */
	fail_on_test(pid);

	fflush(stdout);
	pid_streamoff = fork();
	fail_on_test(pid_streamoff == -1);

	if (pid_streamoff == 0) { // Child
		/*
		 * In the second child call STREAMOFF: this shouldn't
		 * be blocked by the DQBUF!
		 */
		node->streamoff(q.g_type());
		exit(0);
	}

	int wstatus_streamoff = 0;

	/* Wait for the second child to start and exit */
	usleep(250000);
	pid = waitpid(pid_streamoff, &wstatus_streamoff, WNOHANG);
	kill(pid_dqbuf, SIGKILL);
	fail_on_test(pid != pid_streamoff);
	/* Test that the second child exited properly */
	if (!pid || !WIFEXITED(wstatus_streamoff)) {
		kill(pid_streamoff, SIGKILL);
		fail_on_test(!pid || !WIFEXITED(wstatus_streamoff));
	}

	fail_on_test(node->streamoff(q.g_type()));
	fail_on_test(q.reqbufs(node, 0));
	return 0;
}

int testBlockingWait(struct node *node)
{
	bool can_stream = node->g_caps() & V4L2_CAP_STREAMING;
	int type;

	if (!can_stream || !node->valid_buftypes)
		return ENOTTY;

	buffer_info.clear();
	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;
		if (v4l_type_is_overlay(type))
			continue;

		cv4l_queue q(type, V4L2_MEMORY_MMAP);
		cv4l_queue m2m_q(v4l_type_invert(type), V4L2_MEMORY_MMAP);
	
		if (testSetupVbi(node, type))
			continue;

		fail_on_test(testBlockingDQBuf(node, q));
		if (node->is_m2m)
			fail_on_test(testBlockingDQBuf(node, m2m_q));
	}
	return 0;
}

static void restoreCropCompose(struct node *node, __u32 field,
		v4l2_selection &crop, v4l2_selection &compose)
{
	if (node->has_inputs) {
		/*
		 * For capture restore the compose rectangle
		 * before the crop rectangle.
		 */
		if (compose.r.width)
			node->s_frame_selection(compose, field);
		if (crop.r.width)
			node->s_frame_selection(crop, field);
	} else {
		/*
		 * For output the crop rectangle should be
		 * restored before the compose rectangle.
		 */
		if (crop.r.width)
			node->s_frame_selection(crop, field);
		if (compose.r.width)
			node->s_frame_selection(compose, field);
	}
}

int restoreFormat(struct node *node)
{
	cv4l_fmt fmt;
	unsigned h;

	node->g_fmt(fmt);

	h = fmt.g_frame_height();
	if (node->cur_io_caps & V4L2_IN_CAP_STD) {
		v4l2_std_id std;

		fail_on_test(node->g_std(std));
		fmt.s_width(720);
		h = (std & V4L2_STD_525_60) ? 480 : 576;
	}
	if (node->cur_io_caps & V4L2_IN_CAP_DV_TIMINGS) {
		v4l2_dv_timings timings;

		fail_on_test(node->g_dv_timings(timings));
		fmt.s_width(timings.bt.width);
		h = timings.bt.height;
	}
	if (node->cur_io_caps & V4L2_IN_CAP_NATIVE_SIZE) {
		v4l2_selection sel = {
			node->g_selection_type(),
			V4L2_SEL_TGT_NATIVE_SIZE,
		};

		fail_on_test(node->g_selection(sel));
		fmt.s_width(sel.r.width);
		h = sel.r.height;
	}
	fmt.s_frame_height(h);
	/* First restore the format */
	node->s_fmt(fmt);

	v4l2_selection sel_compose = {
		node->g_selection_type(),
		V4L2_SEL_TGT_COMPOSE,
	};
	v4l2_selection sel_crop = {
		node->g_selection_type(),
		V4L2_SEL_TGT_CROP,
	};
	sel_compose.r.width = fmt.g_width();
	sel_compose.r.height = fmt.g_frame_height();
	sel_crop.r.width = fmt.g_width();
	sel_crop.r.height = fmt.g_frame_height();
	restoreCropCompose(node, fmt.g_field(), sel_crop, sel_compose);
	return 0;
}

static int testStreaming(struct node *node, unsigned frame_count)
{
	int type = node->g_type();

	if (!(node->valid_buftypes & (1 << type)))
		return ENOTTY;

	buffer_info.clear();

	cur_fmt.s_type(type);
	node->g_fmt(cur_fmt);

	bool is_output = v4l_type_is_output(type);

	if (node->g_caps() & V4L2_CAP_STREAMING) {
		cv4l_queue q(type, V4L2_MEMORY_MMAP);
		bool alternate = cur_fmt.g_field() == V4L2_FIELD_ALTERNATE;
		v4l2_std_id std = 0;

		node->g_std(std);

		unsigned field = cur_fmt.g_first_field(std);
		cv4l_buffer buf(q);
	
		if (is_output)
			stream_for_fmt(cur_fmt.g_pixelformat());

		fail_on_test(q.reqbufs(node, 3));
		fail_on_test(q.obtain_bufs(node));
		for (unsigned i = 0; i < q.g_buffers(); i++) {
			buf.init(q, i);
			buf.s_field(field);
			if (alternate)
				field ^= 1;
			if (is_output &&
			    !fill_output_buffer(q, buf))
				return 0;
			fail_on_test(node->qbuf(buf));
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
		}
		fail_on_test(node->streamon());

		while (node->dqbuf(buf) == 0) {
			if (!no_progress)
				printf("\r\t\t%s: Frame #%03d Field %s   ",
				       buftype2s(q.g_type()).c_str(),
				       buf.g_sequence(), field2s(buf.g_field()).c_str());
			fflush(stdout);
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
			buf.s_field(field);
			if (alternate)
				field ^= 1;
			if (is_output &&
			    !fill_output_buffer(q, buf))
				return 0;
			fail_on_test(node->qbuf(buf));
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
			if (--frame_count == 0)
				break;
		}
		q.free(node);
		if (is_output)
			stream_close();
		if (!no_progress)
			printf("\r\t\t                                                            ");
		return 0;
	}
	fail_on_test(!(node->g_caps() & V4L2_CAP_READWRITE));

	int size = cur_fmt.g_sizeimage();
	void *tmp = malloc(size);

	for (unsigned i = 0; i < frame_count; i++) {
		int ret;

		if (node->can_capture)
			ret = node->read(tmp, size);
		else
			ret = node->write(tmp, size);
		fail_on_test(ret != size);
		if (!no_progress)
			printf("\r\t\t%s: Frame #%03d", buftype2s(type).c_str(), i);
		fflush(stdout);
	}
	if (!no_progress)
		printf("\r\t\t                                                            ");
	return 0;
}

/*
 * Remember which combination of fmt, crop and compose rectangles have been
 * used to test streaming.
 * This helps prevent duplicate streaming tests.
 */
struct selTest {
	unsigned fmt_w, fmt_h, fmt_field;
	unsigned crop_w, crop_h;
	unsigned compose_w, compose_h;
};

static std::vector<selTest> selTests;

static selTest createSelTest(const cv4l_fmt &fmt,
		const v4l2_selection &crop, const v4l2_selection &compose)
{
	selTest st = {
		fmt.g_width(), fmt.g_height(), fmt.g_field(),
		crop.r.width, crop.r.height,
		compose.r.width, compose.r.height
	};

	return st;
}

static selTest createSelTest(struct node *node)
{
	v4l2_selection crop = {
		node->g_selection_type(),
		V4L2_SEL_TGT_CROP
	};
	v4l2_selection compose = {
		node->g_selection_type(),
		V4L2_SEL_TGT_COMPOSE
	};
	cv4l_fmt fmt;

	node->g_fmt(fmt);
	node->g_selection(crop);
	node->g_selection(compose);
	return createSelTest(fmt, crop, compose);
}

static bool haveSelTest(const selTest &test)
{
	for (unsigned i = 0; i < selTests.size(); i++)
		if (!memcmp(&selTests[i], &test, sizeof(test)))
			return true;
	return false;
}

static void streamFmtRun(struct node *node, cv4l_fmt &fmt, unsigned frame_count,
		bool testSelection = false)
{
	v4l2_selection crop = {
		node->g_selection_type(),
		V4L2_SEL_TGT_CROP
	};
	v4l2_selection compose = {
		node->g_selection_type(),
		V4L2_SEL_TGT_COMPOSE
	};
	bool has_compose = node->cur_io_has_compose();
	bool has_crop = node->cur_io_has_crop();
	char s_crop[32] = "";
	char s_compose[32] = "";

	if (has_crop) {
		node->g_frame_selection(crop, fmt.g_field());
		sprintf(s_crop, "Crop %ux%u@%ux%u, ",
				crop.r.width, crop.r.height,
				crop.r.left, crop.r.top);
	}
	if (has_compose) {
		node->g_frame_selection(compose, fmt.g_field());
		sprintf(s_compose, "Compose %ux%u@%ux%u, ",
				compose.r.width, compose.r.height,
				compose.r.left, compose.r.top);
	}
	if (!no_progress)
		printf("\r");
	printf("\t\t%s%sStride %u, Field %s%s: %s   \n",
			s_crop, s_compose,
			fmt.g_bytesperline(),
			field2s(fmt.g_field()).c_str(),
			testSelection ? ", SelTest" : "",
			ok(testStreaming(node, frame_count)));
	node->reopen();
}

static void streamFmt(struct node *node, __u32 pixelformat, __u32 w, __u32 h,
		      v4l2_fract *f, unsigned frame_count)
{
	const char *op = (node->g_caps() & V4L2_CAP_STREAMING) ? "MMAP" :
		(node->can_capture ? "read()" : "write()");
	bool has_compose = node->cur_io_has_compose();
	bool has_crop = node->cur_io_has_crop();
	__u32 default_field;
	v4l2_selection crop = {
		node->g_selection_type(),
		V4L2_SEL_TGT_CROP
	};
	v4l2_selection min_crop, max_crop;
	v4l2_selection compose = {
		node->g_selection_type(),
		V4L2_SEL_TGT_COMPOSE
	};
	v4l2_selection min_compose, max_compose;
	cv4l_fmt fmt;
	char hz[32] = "";

	if (!frame_count)
		frame_count = f ? 1.0 / fract2f(f) : 10;
	node->g_fmt(fmt);
	fmt.s_pixelformat(pixelformat);
	fmt.s_width(w);
	fmt.s_field(V4L2_FIELD_ANY);
	fmt.s_height(h);
	node->try_fmt(fmt);
	default_field = fmt.g_field();
	fmt.s_frame_height(h);
	node->s_fmt(fmt);
	if (f) {
		node->set_interval(*f);
		sprintf(hz, "@%.2f Hz", 1.0 / fract2f(f));
	}

	printf("\ttest %s for Format %s, Frame Size %ux%u%s:\n", op,
				fcc2s(pixelformat).c_str(),
				fmt.g_width(), fmt.g_frame_height(), hz);

	if (has_crop)
		node->g_frame_selection(crop, fmt.g_field());
	if (has_compose)
		node->g_frame_selection(compose, fmt.g_field());

	for (unsigned field = V4L2_FIELD_NONE;
			field <= V4L2_FIELD_INTERLACED_BT; field++) {
		node->g_fmt(fmt);
		fmt.s_field(field);
		fmt.s_width(w);
		fmt.s_frame_height(h);
		node->s_fmt(fmt);
		if (fmt.g_field() != field)
			continue;

		restoreCropCompose(node, fmt.g_field(), crop, compose);
		streamFmtRun(node, fmt, frame_count);

		// Test if the driver allows for user-specified 'bytesperline' values
		node->g_fmt(fmt);
		unsigned bpl = fmt.g_bytesperline();
		unsigned size = fmt.g_sizeimage();
		fmt.s_bytesperline(bpl + 64);
		node->s_fmt(fmt, false);
		if (fmt.g_bytesperline() == bpl)
			continue;
		if (fmt.g_sizeimage() <= size)
			fail("fmt.g_sizeimage() <= size\n");
		streamFmtRun(node, fmt, frame_count);
	}

	fmt.s_field(default_field);
	fmt.s_frame_height(h);
	node->s_fmt(fmt);
	restoreCropCompose(node, fmt.g_field(), crop, compose);

	if (has_crop) {
		min_crop = crop;
		min_crop.r.width = 0;
		min_crop.r.height = 0;
		node->s_frame_selection(min_crop, fmt.g_field());
		node->s_fmt(fmt);
		node->g_frame_selection(min_crop, fmt.g_field());
		max_crop = crop;
		max_crop.r.width = ~0;
		max_crop.r.height = ~0;
		node->s_frame_selection(max_crop, fmt.g_field());
		node->s_fmt(fmt);
		node->g_frame_selection(max_crop, fmt.g_field());
		restoreCropCompose(node, fmt.g_field(), crop, compose);
	}

	if (has_compose) {
		min_compose = compose;
		min_compose.r.width = 0;
		min_compose.r.height = 0;
		node->s_frame_selection(min_compose, fmt.g_field());
		node->s_fmt(fmt);
		node->g_frame_selection(min_compose, fmt.g_field());
		max_compose = compose;
		max_compose.r.width = ~0;
		max_compose.r.height = ~0;
		node->s_frame_selection(max_compose, fmt.g_field());
		node->s_fmt(fmt);
		node->g_frame_selection(max_compose, fmt.g_field());
		restoreCropCompose(node, fmt.g_field(), crop, compose);
	}

	if (min_crop.r.width == max_crop.r.width &&
	    min_crop.r.height == max_crop.r.height)
		has_crop = false;
	if (min_compose.r.width == max_compose.r.width &&
	    min_compose.r.height == max_compose.r.height)
		has_compose = false;

	if (!has_crop && !has_compose)
		return;

	if (!has_compose) {
		cv4l_fmt tmp;

		node->s_frame_selection(min_crop, fmt.g_field());
		node->g_fmt(tmp);
		if (tmp.g_width() != fmt.g_width() ||
		    tmp.g_height() != fmt.g_height())
			fail("Format resolution changed after changing to min crop\n");
		selTest test = createSelTest(node);
		if (!haveSelTest(test)) {
			selTests.push_back(test);
			streamFmtRun(node, fmt, frame_count, true);
		}
		node->s_frame_selection(max_crop, fmt.g_field());
		node->g_fmt(tmp);
		if (tmp.g_width() != fmt.g_width() ||
		    tmp.g_height() != fmt.g_height())
			fail("Format resolution changed after changing to max crop\n");
		test = createSelTest(node);
		if (!haveSelTest(test)) {
			selTests.push_back(test);
			streamFmtRun(node, fmt, frame_count, true);
		}
		restoreCropCompose(node, fmt.g_field(), crop, compose);
		return;
	}
	if (!has_crop) {
		cv4l_fmt tmp;
		node->s_frame_selection(min_compose, fmt.g_field());
		node->g_fmt(tmp);
		if (tmp.g_width() != fmt.g_width() ||
		    tmp.g_height() != fmt.g_height())
			fail("Format resolution changed after changing to min compose\n");
		selTest test = createSelTest(node);
		if (!haveSelTest(test)) {
			selTests.push_back(test);
			streamFmtRun(node, fmt, frame_count, true);
		}
		node->s_frame_selection(max_compose, fmt.g_field());
		node->g_fmt(tmp);
		if (tmp.g_width() != fmt.g_width() ||
		    tmp.g_height() != fmt.g_height())
			fail("Format resolution changed after changing to max compose\n");
		test = createSelTest(node);
		if (!haveSelTest(test)) {
			selTests.push_back(test);
			streamFmtRun(node, fmt, frame_count, true);
		}
		restoreCropCompose(node, fmt.g_field(), crop, compose);
		return;
	}

	v4l2_selection *selections[2][4] = {
		{ &min_crop, &max_crop, &min_compose, &max_compose },
		{ &min_compose, &max_compose, &min_crop, &max_crop }
	};

	selTest test = createSelTest(node);
	if (!haveSelTest(test)) 
		selTests.push_back(createSelTest(node));

	for (unsigned i = 0; i < 8; i++) {
		v4l2_selection *sel1 = selections[node->can_output][i & 1];
		v4l2_selection *sel2 = selections[node->can_output][2 + ((i & 2) >> 1)];
		v4l2_selection *sel3 = selections[node->can_output][(i & 4) >> 2];
		cv4l_fmt tmp;

		restoreCropCompose(node, fmt.g_field(), crop, compose);
		node->s_frame_selection(*sel1, fmt.g_field());
		node->g_fmt(tmp);
		if (tmp.g_width() != fmt.g_width() ||
		    tmp.g_height() != fmt.g_height())
			fail("Format resolution changed after changing first selection\n");
		selTest test = createSelTest(node);
		if (!haveSelTest(test)) {
			selTests.push_back(test);
			streamFmtRun(node, fmt, frame_count, true);
		}

		node->s_frame_selection(*sel2, fmt.g_field());
		node->g_fmt(tmp);
		if (tmp.g_width() != fmt.g_width() ||
		    tmp.g_height() != fmt.g_height())
			fail("Format resolution changed after changing second selection\n");
		test = createSelTest(node);
		if (!haveSelTest(test)) {
			selTests.push_back(test);
			streamFmtRun(node, fmt, frame_count, true);
		}

		node->s_frame_selection(*sel3, fmt.g_field());
		node->g_fmt(tmp);
		if (tmp.g_width() != fmt.g_width() ||
		    tmp.g_height() != fmt.g_height())
			fail("Format resolution changed after changing third selection\n");
		test = createSelTest(node);
		if (!haveSelTest(test)) {
			selTests.push_back(test);
			streamFmtRun(node, fmt, frame_count, true);
		}
	}
	restoreCropCompose(node, fmt.g_field(), crop, compose);
}

static void streamIntervals(struct node *node, __u32 pixelformat, __u32 w, __u32 h,
			    unsigned frame_count)
{
	v4l2_frmivalenum frmival = { 0 };

	if (node->enum_frameintervals(frmival, pixelformat, w, h)) {
		streamFmt(node, pixelformat, w, h, NULL, frame_count);
		return;
	}

	if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
		do {
			streamFmt(node, pixelformat, w, h, &frmival.discrete,
				  frame_count);
		} while (!node->enum_frameintervals(frmival));
		return;
	}
	streamFmt(node, pixelformat, w, h, &frmival.stepwise.min, frame_count);
	streamFmt(node, pixelformat, w, h, &frmival.stepwise.max, frame_count);
}

void streamAllFormats(struct node *node, unsigned frame_count)
{
	v4l2_fmtdesc fmtdesc;

	if (node->enum_fmt(fmtdesc, true))
		return;
	selTests.clear();
	do {
		v4l2_frmsizeenum frmsize;
		cv4l_fmt fmt;

		if (node->enum_framesizes(frmsize, fmtdesc.pixelformat)) {
			cv4l_fmt min, max;

			restoreFormat(node);
			node->g_fmt(fmt);
			min = fmt;
			min.s_width(0);
			min.s_height(0);
			node->try_fmt(min);
			max = fmt;
			max.s_width(~0);
			max.s_height(~0);
			node->try_fmt(max);
			if (min.g_width() != fmt.g_width() ||
			    min.g_height() != fmt.g_height()) {
				streamIntervals(node, fmtdesc.pixelformat,
					min.g_width(), min.g_frame_height(),
					frame_count);
				restoreFormat(node);
			}
			if (max.g_width() != fmt.g_width() ||
			    max.g_height() != fmt.g_height()) {
				streamIntervals(node, fmtdesc.pixelformat,
					max.g_width(), max.g_frame_height(),
					frame_count);
				restoreFormat(node);
			}
			streamIntervals(node, fmtdesc.pixelformat,
					fmt.g_width(), fmt.g_frame_height(),
					frame_count);
			continue;
		}

		v4l2_frmsize_stepwise &ss = frmsize.stepwise;

		switch (frmsize.type) {
		case V4L2_FRMSIZE_TYPE_DISCRETE:
			do {
				streamIntervals(node, fmtdesc.pixelformat,
						frmsize.discrete.width,
						frmsize.discrete.height,
						frame_count);
			} while (!node->enum_framesizes(frmsize));
			break;
		default:
			restoreFormat(node);
			streamIntervals(node, fmtdesc.pixelformat,
					ss.min_width, ss.min_height,
					frame_count);
			restoreFormat(node);
			if (ss.max_width != ss.min_width ||
			    ss.max_height != ss.min_height) {
				streamIntervals(node, fmtdesc.pixelformat,
						ss.max_width, ss.max_height,
						frame_count);
				restoreFormat(node);
			}	
			node->g_fmt(fmt);
			if (fmt.g_width() != ss.min_width ||
			    fmt.g_frame_height() != ss.min_height) {
				streamIntervals(node, fmtdesc.pixelformat,
					fmt.g_width(), fmt.g_frame_height(),
					frame_count);
				restoreFormat(node);
			}
			break;
		}
	} while (!node->enum_fmt(fmtdesc));
}

static void streamM2MRun(struct node *node, unsigned frame_count)
{
	cv4l_fmt cap_fmt, out_fmt;
	unsigned out_type = v4l_type_invert(node->g_type());

	node->g_fmt(cap_fmt);
	node->g_fmt(out_fmt, out_type);
	if (!no_progress)
		printf("\r");
	printf("\t%s (%s) %dx%d -> %s (%s) %dx%d: %s\n",
	       fcc2s(out_fmt.g_pixelformat()).c_str(),
	       pixfmt2s(out_fmt.g_pixelformat()).c_str(),
	       out_fmt.g_width(), out_fmt.g_height(),
	       fcc2s(cap_fmt.g_pixelformat()).c_str(),
	       pixfmt2s(cap_fmt.g_pixelformat()).c_str(),
	       cap_fmt.g_width(), cap_fmt.g_height(),
	       ok(testMmap(node, node, frame_count, POLL_MODE_SELECT)));
}

static int streamM2MOutFormat(struct node *node, __u32 pixelformat, __u32 w, __u32 h,
			      unsigned frame_count)
{
	unsigned cap_type = node->g_type();
	v4l2_fmtdesc fmtdesc;
	cv4l_fmt out_fmt;

	node->g_fmt(out_fmt, v4l_type_invert(cap_type));
	out_fmt.s_pixelformat(pixelformat);
	out_fmt.s_width(w);
	out_fmt.s_height(h);
	fail_on_test(node->s_fmt(out_fmt));

	if (node->enum_fmt(fmtdesc, true, 0))
		return 0;
	do {
		cv4l_fmt fmt;

		fail_on_test(node->g_fmt(fmt));
		fmt.s_pixelformat(fmtdesc.pixelformat);
		fmt.s_width(w);
		fmt.s_height(h);
		fail_on_test(node->s_fmt(fmt));
		streamM2MRun(node, frame_count);
	} while (!node->enum_fmt(fmtdesc));
	return 0;
}

void streamM2MAllFormats(struct node *node, unsigned frame_count)
{
	v4l2_fmtdesc fmtdesc;
	unsigned out_type = v4l_type_invert(node->g_type());

	if (!frame_count)
		frame_count = 10;

	if (node->enum_fmt(fmtdesc, true, 0, out_type))
		return;
	selTests.clear();
	do {
		v4l2_frmsizeenum frmsize;
		cv4l_fmt fmt;

		if (node->enum_framesizes(frmsize, fmtdesc.pixelformat)) {
			cv4l_fmt min, max;

			restoreFormat(node);
			node->g_fmt(fmt, out_type);
			min = fmt;
			min.s_width(0);
			min.s_height(0);
			node->try_fmt(min);
			max = fmt;
			max.s_width(~0);
			max.s_height(~0);
			node->try_fmt(max);
			if (min.g_width() != fmt.g_width() ||
			    min.g_height() != fmt.g_height()) {
				streamM2MOutFormat(node, fmtdesc.pixelformat,
						   min.g_width(), min.g_frame_height(),
						   frame_count);
				restoreFormat(node);
			}
			if (max.g_width() != fmt.g_width() ||
			    max.g_height() != fmt.g_height()) {
				streamM2MOutFormat(node, fmtdesc.pixelformat,
						   max.g_width(), max.g_frame_height(),
						   frame_count);
				restoreFormat(node);
			}
			streamM2MOutFormat(node, fmtdesc.pixelformat,
					   fmt.g_width(), fmt.g_frame_height(),
					   frame_count);
			continue;
		}

		v4l2_frmsize_stepwise &ss = frmsize.stepwise;

		switch (frmsize.type) {
		case V4L2_FRMSIZE_TYPE_DISCRETE:
			do {
				streamM2MOutFormat(node, fmtdesc.pixelformat,
						   frmsize.discrete.width,
						   frmsize.discrete.height,
						   frame_count);
			} while (!node->enum_framesizes(frmsize));
			break;
		default:
			node->g_fmt(fmt, out_type);

			if (fmt.g_width() != ss.min_width ||
			    fmt.g_frame_height() != ss.min_height)  {
				streamM2MOutFormat(node, fmtdesc.pixelformat,
						   ss.min_width, ss.min_height,
						   frame_count);
				restoreFormat(node);
			}
			if (fmt.g_width() != ss.max_width ||
			    fmt.g_frame_height() != ss.max_height)  {
				streamM2MOutFormat(node, fmtdesc.pixelformat,
						   ss.max_width, ss.max_height,
						   frame_count);
				restoreFormat(node);
			}	
			streamM2MOutFormat(node, fmtdesc.pixelformat,
					   fmt.g_width(), fmt.g_frame_height(),
					   frame_count);
			restoreFormat(node);
			break;
		}
	} while (!node->enum_fmt(fmtdesc));
}
