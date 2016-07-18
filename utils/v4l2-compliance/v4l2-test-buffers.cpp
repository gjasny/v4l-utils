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
#include <vector>
#include "v4l2-compliance.h"

static struct cv4l_fmt cur_fmt;

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

enum QueryBufMode {
	Unqueued,
	Prepared,
	Queued,
	Dequeued
};

typedef std::map<struct timeval, struct v4l2_buffer> buf_info_map;
static buf_info_map buffer_info;

std::string pixfmt2s(unsigned id)
{
	std::string pixfmt;

	pixfmt += (char)(id & 0x7f);
	pixfmt += (char)((id >> 8) & 0x7f);
	pixfmt += (char)((id >> 16) & 0x7f);
	pixfmt += (char)((id >> 24) & 0x7f);
	if (id & (1 << 31))
		pixfmt += "-BE";
	return pixfmt;
}

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
		return node->prepare_buf(*this);
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
		if (err == 0 && v4l_type_is_output(g_type()))
			buffer_info[g_timestamp()] = buf;
		return err;
	}
	int check(const cv4l_queue &q, enum QueryBufMode mode)
	{
		int ret = check(q.g_type(), q.g_memory(), g_index(), mode, last_seq);

		if (!ret)
			ret = check_planes(q, mode);
		return ret;
	}
	int check(const cv4l_queue &q, enum QueryBufMode mode, __u32 index)
	{
		int ret = check(q.g_type(), q.g_memory(), index, mode, last_seq);

		if (!ret)
			ret = check_planes(q, mode);
		return ret;
	}
	int check(const cv4l_queue &q, buf_seq &seq)
	{
		int ret = check(q.g_type(), q.g_memory(), g_index(), Dequeued, seq);

		if (!ret)
			ret = check_planes(q, Dequeued);
		return ret;
	}
	int check(enum QueryBufMode mode, __u32 index)
	{
		return check(g_type(), g_memory(), index, mode, last_seq);
	}
	int check(buf_seq &seq)
	{
		return check(g_type(), g_memory(), g_index(), Dequeued, seq);
	}

private:
	int check(unsigned type, unsigned memory, unsigned index,
			enum QueryBufMode mode, struct buf_seq &seq);
	int check_planes(const cv4l_queue &q, enum QueryBufMode mode);
	void fill_output_buf(bool fill_bytesused = true)
	{
		timespec ts;
		v4l2_timecode tc;

		clock_gettime(CLOCK_MONOTONIC, &ts);
		if (ts_is_copy()) {
			s_timestamp_ts(ts);
			s_timestamp_src(V4L2_BUF_FLAG_TSTAMP_SRC_SOE);
		}
		s_flags(V4L2_BUF_FLAG_TIMECODE | V4L2_BUF_FLAG_KEYFRAME);
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
};

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
			enum QueryBufMode mode, struct buf_seq &seq)
{
	unsigned timestamp = g_timestamp_type();
	bool ts_copy = ts_is_copy();
	unsigned timestamp_src = g_timestamp_src();
	unsigned frame_types = 0;
	unsigned buf_states = 0;

	fail_on_test(g_type() != type);
	fail_on_test(g_memory() == 0);
	fail_on_test(g_memory() != memory);
	fail_on_test(g_index() >= VIDEO_MAX_FRAME);
	fail_on_test(g_index() != index);
	fail_on_test(buf.reserved2 || buf.reserved);
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

	if (v4l_type_is_capture(g_type()) && !ts_copy &&
	    (g_flags() & V4L2_BUF_FLAG_TIMECODE))
		warn_once("V4L2_BUF_FLAG_TIMECODE was used!\n");

	if (mode == Dequeued) {
		for (unsigned p = 0; p < g_num_planes(); p++) {
			fail_on_test(!g_bytesused(p));
			fail_on_test(g_data_offset(p) >= g_bytesused(p));
			fail_on_test(g_bytesused(p) > g_length(p));
		}
		fail_on_test(!g_timestamp().tv_sec && !g_timestamp().tv_usec);
		fail_on_test(g_flags() & V4L2_BUF_FLAG_DONE);
		fail_on_test((int)g_sequence() < seq.last_seq + 1);
		if (v4l_type_is_video(g_type())) {
			fail_on_test(g_field() == V4L2_FIELD_ALTERNATE);
			fail_on_test(g_field() == V4L2_FIELD_ANY);
			if (cur_fmt.g_field() == V4L2_FIELD_ALTERNATE) {
				fail_on_test(g_field() != V4L2_FIELD_BOTTOM &&
						g_field() != V4L2_FIELD_TOP);
				fail_on_test(g_field() == seq.last_field);
				seq.field_nr ^= 1;
				if (seq.field_nr) {
					if ((int)g_sequence() != seq.last_seq)
						warn("got sequence number %u, expected %u\n",
							g_sequence(), seq.last_seq + 1);
				} else {
					fail_on_test((int)g_sequence() == seq.last_seq + 1);
					if ((int)g_sequence() != seq.last_seq + 1)
						warn("got sequence number %u, expected %u\n",
							g_sequence(), seq.last_seq + 1);
				}
			} else {
				fail_on_test(g_field() != cur_fmt.g_field());
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

		q.init(i, V4L2_MEMORY_USERPTR);
		ret = q.reqbufs(node, 0);
		fail_on_test(ret && ret != EINVAL);
		userptr_valid = !ret;

		q.init(i, V4L2_MEMORY_DMABUF);
		ret = q.reqbufs(node, 0);
		fail_on_test(ret && ret != EINVAL);
		dmabuf_valid = !ret;
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
		}
		fail_on_test(q.reqbufs(node));
	}
	return 0;
}

int testExpBuf(struct node *node)
{
	bool have_expbuf = false;
	int type;

	if (!(node->valid_memorytype & (1 << V4L2_MEMORY_MMAP))) {
		cv4l_queue q;

		fail_on_test(q.has_expbuf(node));
		return ENOTTY;
	}

	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;
		if (v4l_type_is_overlay(type))
			continue;

		if (testSetupVbi(node, type))
			continue;

		cv4l_queue q(type, V4L2_MEMORY_MMAP);

		fail_on_test(q.reqbufs(node, 2));
		if (q.has_expbuf(node)) {
			fail_on_test(q.export_bufs(node));
			have_expbuf = true;
		} else {
			fail_on_test(!q.export_bufs(node));
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

static int captureBufs(struct node *node, const cv4l_queue &q,
		const cv4l_queue &m2m_q, unsigned frame_count, bool use_poll)
{
	unsigned valid_output_flags =
		V4L2_BUF_FLAG_TIMECODE | V4L2_BUF_FLAG_TSTAMP_SRC_MASK |
		V4L2_BUF_FLAG_KEYFRAME | V4L2_BUF_FLAG_PFRAME | V4L2_BUF_FLAG_BFRAME;
	int fd_flags = fcntl(node->g_fd(), F_GETFL);
	cv4l_fmt fmt_q;
	buffer buf(q);
	unsigned count = frame_count;
	int ret;

	if (show_info) {
		printf("\t    %s%s:\n",
			buftype2s(q.g_type()).c_str(),
			use_poll ? " (polling)" : "");
	}

	/*
	 * It should be possible to set the same std, timings or
	 * native size even while streaming.
	 */
	fail_on_test(testCanSetSameTimings(node));

	node->g_fmt(fmt_q, q.g_type());
	if (node->buftype_pixfmts[q.g_type()][fmt_q.g_pixelformat()] &
		V4L2_FMT_FLAG_COMPRESSED)
		valid_output_flags = V4L2_BUF_FLAG_TSTAMP_SRC_MASK;
	if (node->is_m2m) {
		node->g_fmt(fmt_q, m2m_q.g_type());
		if (node->buftype_pixfmts[m2m_q.g_type()][fmt_q.g_pixelformat()] &
			V4L2_FMT_FLAG_COMPRESSED)
			valid_output_flags = V4L2_BUF_FLAG_TSTAMP_SRC_MASK;
	}

	if (use_poll)
		fcntl(node->g_fd(), F_SETFL, fd_flags | O_NONBLOCK);
	for (;;) {
		buf.init(q);

		if (use_poll) {
			struct timeval tv = { 2, 0 };
			fd_set fds;

			FD_ZERO(&fds);
			FD_SET(node->g_fd(), &fds);
			if (node->is_m2m)
				ret = select(node->g_fd() + 1, &fds, &fds, NULL, &tv);
			else if (v4l_type_is_output(q.g_type()))
				ret = select(node->g_fd() + 1, NULL, &fds, NULL, &tv);
			else
				ret = select(node->g_fd() + 1, &fds, NULL, NULL, &tv);
			fail_on_test(ret == 0);
			fail_on_test(ret < 0);
			fail_on_test(!FD_ISSET(node->g_fd(), &fds));
		}

		ret = buf.dqbuf(node);
		if (ret != EAGAIN) {
			fail_on_test(ret);
			if (show_info)
				printf("\t\tBuffer: %d Sequence: %d Field: %s Timestamp: %ld.%06lds\n",
						buf.g_index(), buf.g_sequence(), field2s(buf.g_field()).c_str(),
						buf.g_timestamp().tv_sec, buf.g_timestamp().tv_usec);
			fail_on_test(buf.check(q, last_seq));
			if (!show_info) {
				printf("\r\t%s: Frame #%03d%s",
						buftype2s(q.g_type()).c_str(),
						frame_count - count, use_poll ? " (polling)" : "");
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
			fail_on_test(buf.qbuf(node));
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
			if (--count == 0)
				break;
		}
		if (!node->is_m2m)
			continue;

		buf.init(m2m_q);
		ret = buf.dqbuf(node);
		if (ret == EAGAIN)
			continue;
		if (show_info)
			printf("\t\tBuffer: %d Sequence: %d Field: %s Timestamp: %ld.%06lds\n",
				buf.g_index(), buf.g_sequence(), field2s(buf.g_field()).c_str(),
				buf.g_timestamp().tv_sec, buf.g_timestamp().tv_usec);
		fail_on_test(ret);
		fail_on_test(buf.check(m2m_q, last_m2m_seq));
		if (v4l_type_is_capture(buf.g_type()) && buf.ts_is_copy()) {
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
		fail_on_test(buf.qbuf(node));
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
	}
	if (use_poll)
		fcntl(node->g_fd(), F_SETFL, fd_flags);
	if (!show_info) {
		printf("\r\t                                                  \r");
		fflush(stdout);
	}
	return 0;
}

static int setupM2M(struct node *node, cv4l_queue &q)
{
	last_m2m_seq.init();

	fail_on_test(q.reqbufs(node, 2));
	for (unsigned i = 0; i < q.g_buffers(); i++) {
		buffer buf(q);

		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.qbuf(node));
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
	}
	if (v4l_type_is_video(q.g_type())) {
		cv4l_fmt fmt(q.g_type());

		node->g_fmt(fmt);
		last_m2m_seq.last_field = fmt.g_field();
	}
	fail_on_test(node->streamon(q.g_type()));
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
	for (unsigned p = 0; p < buf.g_num_planes(); p++) {
		buf.s_bytesused(buf.g_length(p), p);
		buf.s_data_offset(0, p);
	}
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
		fail_on_test(buf.g_bytesused(p) != buf.g_length(p));
		fail_on_test(buf.g_data_offset(p));
	}
	return 0;
}

static int setupMmap(struct node *node, cv4l_queue &q)
{
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
			fail_on_test(bufferOutputErrorTest(node, buf));
			fail_on_test(buf.querybuf(node, i));
			fail_on_test(buf.check(q, Queued, i));
		} else {
			ret = buf.prepare_buf(node);
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
	fail_on_test(q.mmap_bufs(node));
	return 0;
}

int testMmap(struct node *node, unsigned frame_count)
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

		cv4l_queue q(type, V4L2_MEMORY_MMAP);
		cv4l_queue m2m_q(v4l_type_invert(type));
	
		if (testSetupVbi(node, type))
			continue;

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
		fail_on_test(setupMmap(node, q));

		fail_on_test(node->streamon(q.g_type()));
		fail_on_test(node->streamon(q.g_type()));

		if (node->is_m2m)
			fail_on_test(setupM2M(node, m2m_q));
		else
			fail_on_test(captureBufs(node, q, m2m_q, frame_count, false));
		fail_on_test(captureBufs(node, q, m2m_q, frame_count, true));
		fail_on_test(node->streamoff(q.g_type()));
		fail_on_test(node->streamoff(q.g_type()));
		q.munmap_bufs(node);
		fail_on_test(q.reqbufs(node, 0));
		if (node->is_m2m) {
			fail_on_test(node->streamoff(m2m_q.g_type()));
			m2m_q.munmap_bufs(node);
			fail_on_test(m2m_q.reqbufs(node, 0));
		}
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
			ret = buf.prepare_buf(node);
			fail_on_test(ret && ret != ENOTTY);

			if (ret == 0) {
				fail_on_test(buf.querybuf(node, i));
				fail_on_test(buf.check(q, Prepared, i));
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

		fail_on_test(buf.qbuf(node));
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.check(q, Queued, i));
	}
	return 0;
}

int testUserPtr(struct node *node, unsigned frame_count)
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

		cv4l_queue q(type, V4L2_MEMORY_USERPTR);
		cv4l_queue m2m_q(v4l_type_invert(type));

		if (testSetupVbi(node, type))
			continue;

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

		fail_on_test(setupUserPtr(node, q));

		fail_on_test(node->streamon(q.g_type()));
		fail_on_test(node->streamon(q.g_type()));

		if (node->is_m2m)
			fail_on_test(setupM2M(node, m2m_q));
		else
			fail_on_test(captureBufs(node, q, m2m_q, frame_count, false));
		fail_on_test(captureBufs(node, q, m2m_q, frame_count, true));
		fail_on_test(node->streamoff(q.g_type()));
		fail_on_test(node->streamoff(q.g_type()));
		if (node->is_m2m) {
			fail_on_test(node->streamoff(m2m_q.g_type()));
			m2m_q.munmap_bufs(node);
			fail_on_test(m2m_q.reqbufs(node, 0));
		}
	}
	return 0;
}

static int setupDmaBuf(struct node *expbuf_node, struct node *node,
		       cv4l_queue &q, cv4l_queue &exp_q)
{
	fail_on_test(exp_q.reqbufs(expbuf_node, q.g_buffers()));
	fail_on_test(exp_q.g_buffers() < q.g_buffers());
	fail_on_test(exp_q.export_bufs(expbuf_node));

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
		if (ret != ENOTTY) {
			buf.init(q, i);
			ret = buf.prepare_buf(node);
			fail_on_test(ret);
			fail_on_test(buf.querybuf(node, i));
			fail_on_test(buf.check(q, Prepared, i));
		} else {
			fail_on_test(!buf.qbuf(node));
			buf.init(q, i);
		}

		fail_on_test(buf.qbuf(node));
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.check(q, Queued, i));
	}
	fail_on_test(q.mmap_bufs(node));
	return 0;
}

int testDmaBuf(struct node *expbuf_node, struct node *node, unsigned frame_count)
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

		fail_on_test(setupDmaBuf(expbuf_node, node, q, exp_q));

		fail_on_test(node->streamon(q.g_type()));
		fail_on_test(node->streamon(q.g_type()));

		if (node->is_m2m)
			fail_on_test(setupM2M(node, m2m_q));
		else
			fail_on_test(captureBufs(node, q, m2m_q, frame_count, false));
		fail_on_test(captureBufs(node, q, m2m_q, frame_count, true));
		fail_on_test(node->streamoff(q.g_type()));
		fail_on_test(node->streamoff(q.g_type()));
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

	if (node->g_caps() & V4L2_CAP_STREAMING) {
		cv4l_queue q(type, V4L2_MEMORY_MMAP);
		bool alternate = cur_fmt.g_field() == V4L2_FIELD_ALTERNATE;
		v4l2_std_id std = 0;

		node->g_std(std);

		unsigned field = cur_fmt.g_first_field(std);
		cv4l_buffer buf(q);
	
		fail_on_test(q.reqbufs(node, 3));
		fail_on_test(q.obtain_bufs(node));
		for (unsigned i = 0; i < q.g_buffers(); i++) {
			buf.init(q, i);
			buf.s_field(field);
			if (alternate)
				field ^= 1;
			fail_on_test(node->qbuf(buf));
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
		}
		fail_on_test(node->streamon());

		while (node->dqbuf(buf) == 0) {
			printf("\r\t\t%s: Frame #%03d Field %s   ",
					buftype2s(q.g_type()).c_str(),
					buf.g_sequence(), field2s(buf.g_field()).c_str());
			fflush(stdout);
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
			buf.s_field(field);
			if (alternate)
				field ^= 1;
			fail_on_test(node->qbuf(buf));
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
			if (frame_count-- == 0)
				break;
		}
		q.free(node);
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
		printf("\r\t\t%s: Frame #%03d", buftype2s(type).c_str(), i);
		fflush(stdout);
	}
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
	printf("\r\t\t%s%sStride %u, Field %s%s: %s   \n",
			s_crop, s_compose,
			fmt.g_bytesperline(),
			field2s(fmt.g_field()).c_str(),
			testSelection ? ", SelTest" : "",
			ok(testStreaming(node, frame_count)));
	node->reopen();
}

static void streamFmt(struct node *node, __u32 pixelformat, __u32 w, __u32 h, v4l2_fract *f)
{
	const char *op = (node->g_caps() & V4L2_CAP_STREAMING) ? "MMAP" :
		(node->can_capture ? "read()" : "write()");
	unsigned frame_count = f ? 1.0 / fract2f(f) : 10;
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
				pixfmt2s(pixelformat).c_str(),
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

static void streamIntervals(struct node *node, __u32 pixelformat, __u32 w, __u32 h)
{
	v4l2_frmivalenum frmival = { 0 };

	if (node->enum_frameintervals(frmival, pixelformat, w, h)) {
		streamFmt(node, pixelformat, w, h, NULL);
		return;
	}

	if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
		do {
			streamFmt(node, pixelformat, w, h, &frmival.discrete);
		} while (!node->enum_frameintervals(frmival));
		return;
	}
	streamFmt(node, pixelformat, w, h, &frmival.stepwise.min);
	streamFmt(node, pixelformat, w, h, &frmival.stepwise.max);
}

void streamAllFormats(struct node *node)
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
					min.g_width(), min.g_frame_height());
				restoreFormat(node);
			}
			if (max.g_width() != fmt.g_width() ||
			    max.g_height() != fmt.g_height()) {
				streamIntervals(node, fmtdesc.pixelformat,
					max.g_width(), max.g_frame_height());
				restoreFormat(node);
			}
			streamIntervals(node, fmtdesc.pixelformat,
					fmt.g_width(), fmt.g_frame_height());
			continue;
		}

		v4l2_frmsize_stepwise &ss = frmsize.stepwise;

		switch (frmsize.type) {
		case V4L2_FRMSIZE_TYPE_DISCRETE:
			do {
				streamIntervals(node, fmtdesc.pixelformat,
						frmsize.discrete.width,
						frmsize.discrete.height);
			} while (!node->enum_framesizes(frmsize));
			break;
		default:
			restoreFormat(node);
			streamIntervals(node, fmtdesc.pixelformat,
					ss.min_width, ss.min_height);
			restoreFormat(node);
			if (ss.max_width != ss.min_width ||
			    ss.max_height != ss.min_height) {
				streamIntervals(node, fmtdesc.pixelformat,
						ss.max_width, ss.max_height);
				restoreFormat(node);
			}	
			node->g_fmt(fmt);
			if (fmt.g_width() != ss.min_width ||
			    fmt.g_frame_height() != ss.min_height) {
				streamIntervals(node, fmtdesc.pixelformat,
					fmt.g_width(), fmt.g_frame_height());
				restoreFormat(node);
			}
			break;
		}
	} while (!node->enum_fmt(fmtdesc));
}
