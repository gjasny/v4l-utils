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

static struct cv4l_fmt cur_fmt;

static const unsigned valid_output_flags =
	V4L2_BUF_FLAG_TIMECODE | V4L2_BUF_FLAG_TSTAMP_SRC_MASK |
	V4L2_BUF_FLAG_KEYFRAME | V4L2_BUF_FLAG_PFRAME | V4L2_BUF_FLAG_BFRAME;

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
	int prepare_buf(node *node)
	{
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
		warn("V4L2_BUF_FLAG_TIMECODE was used!\n");

	if (mode == Dequeued) {
		for (unsigned p = 0; p < g_num_planes(); p++) {
			fail_on_test(!g_bytesused(p));
			fail_on_test(g_data_offset(p) >= g_bytesused(p));
			fail_on_test(g_bytesused(p) > g_length(p));
		}
		fail_on_test(!g_timestamp().tv_sec && !g_timestamp().tv_usec);
		fail_on_test(!(g_flags() & (V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_ERROR)));
		if (v4l_type_is_video(g_type())) {
			fail_on_test(g_field() == V4L2_FIELD_ALTERNATE);
			fail_on_test(g_field() == V4L2_FIELD_ANY);
			if (cur_fmt.g_field() == V4L2_FIELD_ALTERNATE) {
				fail_on_test(g_field() != V4L2_FIELD_BOTTOM &&
						g_field() != V4L2_FIELD_TOP);
				fail_on_test(g_field() == seq.last_field);
				seq.field_nr ^= 1;
				if (seq.field_nr)
					fail_on_test((int)g_sequence() != seq.last_seq);
				else
					fail_on_test((int)g_sequence() != seq.last_seq + 1);
			} else {
				fail_on_test(g_field() != cur_fmt.g_field());
				fail_on_test((int)g_sequence() != seq.last_seq + 1);
			}
		} else {
			fail_on_test((int)g_sequence() != seq.last_seq + 1);
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

int testReqBufs(struct node *node)
{
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
	for (i = 1; i <= V4L2_BUF_TYPE_SDR_CAPTURE; i++) {
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
			fail_on_test(q.reqbufs(node));
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

	for (type = 0; type <= V4L2_BUF_TYPE_SDR_CAPTURE; type++) {
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
	int fd_flags = fcntl(node->g_fd(), F_GETFL);
	buffer buf(q);
	unsigned count = frame_count;
	int ret;

	if (show_info) {
		printf("\t    %s%s:\n",
			buftype2s(q.g_type()).c_str(),
			use_poll ? " (polling)" : "");
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
			fail_on_test(ret <= 0);
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
			fail_on_test(buf.qbuf(node));
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
		fail_on_test(buf.qbuf(node));
	}
	if (use_poll)
		fcntl(node->g_fd(), F_SETFL, fd_flags);
	if (!show_info) {
		printf("\r\t                                                  \r");
		fflush(stdout);
	}
	return 0;
}

static unsigned invert_buf_type(unsigned type)
{
	if (v4l_type_is_planar(type))
		return v4l_type_is_output(type) ?
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	return v4l_type_is_output(type) ?
		V4L2_BUF_TYPE_VIDEO_CAPTURE :
		V4L2_BUF_TYPE_VIDEO_OUTPUT;
}

static int setupM2M(struct node *node, cv4l_queue &q)
{
	last_m2m_seq.init();

	fail_on_test(q.reqbufs(node, 2));
	for (unsigned i = 0; i < q.g_buffers(); i++) {
		buffer buf(q);

		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.qbuf(node));
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
	ret = buf.prepare_buf(node);
	fail_on_test(ret != EINVAL && ret != ENOTTY);
	have_prepare = ret != ENOTTY;
	fail_on_test(buf.qbuf(node, false) != EINVAL);

	if (v4l_type_is_planar(buf.g_type())) {
		for (unsigned p = 0; p < buf.g_num_planes(); p++) {
			buf.s_bytesused(buf.g_length(p) / 2, p);
			buf.s_data_offset(buf.g_bytesused(p), p);
		}
		if (have_prepare)
			fail_on_test(buf.prepare_buf(node) != EINVAL);
		fail_on_test(buf.qbuf(node, false) != EINVAL);
	}
	buf.init(orig_buf);
	for (unsigned p = 0; p < buf.g_num_planes(); p++) {
		buf.s_bytesused(0, p);
		buf.s_data_offset(0, p);
	}
	if (have_prepare) {
		fail_on_test(buf.prepare_buf(node));
		fail_on_test(buf.check(Prepared, 0));
		buf.init(orig_buf);
		for (unsigned p = 0; p < buf.g_num_planes(); p++) {
			buf.s_bytesused(0xdeadbeef, p);
			buf.s_data_offset(0xdeadbeef, p);
		}
	}
	fail_on_test(buf.qbuf(node, false));
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
	for (type = 0; type <= V4L2_BUF_TYPE_SDR_CAPTURE; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;
		if (v4l_type_is_overlay(type))
			continue;

		cv4l_queue q(type, V4L2_MEMORY_MMAP);
		cv4l_queue m2m_q(invert_buf_type(type));
	
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
		}
		// calling STREAMOFF...
		fail_on_test(node->streamoff(q.g_type()));
		// and now we should be able to queue those buffers again since
		// STREAMOFF should return them back to the dequeued state.
		for (unsigned i = 0; i < q.g_buffers(); i++) {
			buffer buf(q);

			fail_on_test(buf.querybuf(node, i));
			fail_on_test(buf.qbuf(node));
		}
		// Now request buffers again, freeing the old buffers.
		// Good check for whether all the internal vb2 calls are in
		// balance.
		fail_on_test(q.reqbufs(node, q.g_buffers()));
		cur_fmt.s_type(q.g_type());
		node->g_fmt(cur_fmt);

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
				ret = q.create_bufs(node, 1, &fmt);
				fail_on_test(ret != EINVAL);
				fail_on_test(testQueryBuf(node, cur_fmt.type, q.g_buffers()));
				fmt = cur_fmt;
				for (unsigned p = 0; p < fmt.g_num_planes(); p++)
					fmt.s_sizeimage(fmt.g_sizeimage(p) * 2, p);
			}
			fail_on_test(q.create_bufs(node, 1, &fmt));
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
	for (type = 0; type <= V4L2_BUF_TYPE_SDR_CAPTURE; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;
		if (v4l_type_is_overlay(type))
			continue;

		cv4l_queue q(type, V4L2_MEMORY_USERPTR);
		cv4l_queue m2m_q(invert_buf_type(type));

		if (testSetupVbi(node, type))
			continue;

		ret = q.reqbufs(node, 0);
		if (ret) {
			fail_on_test(ret != EINVAL);
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
	for (type = 0; type <= V4L2_BUF_TYPE_SDR_CAPTURE; type++) {
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
		cv4l_queue m2m_q(invert_buf_type(type));
		cv4l_queue exp_q(expbuf_type, V4L2_MEMORY_MMAP);

		if (testSetupVbi(node, type))
			continue;

		ret = q.reqbufs(node, 0);
		if (ret) {
			fail_on_test(ret != EINVAL);
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
