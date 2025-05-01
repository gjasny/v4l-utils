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

#include <algorithm>
#include <atomic>
#include <csignal>
#include <map>
#include <set>
#include <vector>

#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
#include <sys/epoll.h>

#include "v4l2-compliance.h"

static cv4l_fmt cur_fmt;
static cv4l_fmt cur_m2m_fmt;
static int stream_from_fd = -1;
static bool stream_use_hdr;
static unsigned max_bytesused[VIDEO_MAX_PLANES];
static unsigned min_data_offset[VIDEO_MAX_PLANES];

static bool operator<(struct timeval const& n1, struct timeval const& n2)
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

using buf_info_map = std::map<struct timeval, struct v4l2_buffer>;
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

			if (sz < static_cast<ssize_t>(len)) {
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

		if (sz < static_cast<ssize_t>(bytesused)) {
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
	explicit buffer(unsigned type = 0, unsigned memory = 0, unsigned index = 0) :
		cv4l_buffer(type, memory, index) {}
	explicit buffer(const cv4l_queue &q, unsigned index = 0) :
		cv4l_buffer(q, index) {}
	explicit buffer(const cv4l_buffer &b) : cv4l_buffer(b) {}

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
	const cv4l_fmt &fmt = is_m2m ? cur_m2m_fmt : cur_fmt;

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

		// The vivid driver has unreliable timings causing wrong
		// sequence numbers on occasion. Skip this test until this
		// bug is solved.
		if (!is_vivid)
			fail_on_test((int)g_sequence() < seq.last_seq + 1);
		else if ((int)g_sequence() < seq.last_seq + 1)
			info("(int)g_sequence() < seq.last_seq + 1): %d < %d\n",
			     (int)g_sequence(), seq.last_seq + 1);

		if (v4l_type_is_video(g_type())) {
			fail_on_test(g_field() == V4L2_FIELD_ALTERNATE);
			fail_on_test(g_field() == V4L2_FIELD_ANY);
			if (fmt.g_field() == V4L2_FIELD_ALTERNATE) {
				fail_on_test(g_field() != V4L2_FIELD_BOTTOM &&
						g_field() != V4L2_FIELD_TOP);
				fail_on_test(g_field() == seq.last_field);
				seq.field_nr ^= 1;
				if (seq.field_nr) {
					if (static_cast<int>(g_sequence()) != seq.last_seq)
						warn("got sequence number %u, expected %u\n",
							g_sequence(), seq.last_seq);
				} else {
					fail_on_test((int)g_sequence() == seq.last_seq + 1);
					if (static_cast<int>(g_sequence()) != seq.last_seq + 1)
						warn("got sequence number %u, expected %u\n",
							g_sequence(), seq.last_seq + 1);
				}
			} else {
				fail_on_test(g_field() != fmt.g_field());
				if (static_cast<int>(g_sequence()) != seq.last_seq + 1)
					warn_or_info(is_vivid,
						     "got sequence number %u, expected %u\n",
						     g_sequence(), seq.last_seq + 1);
			}
		} else if (!v4l_type_is_meta(g_type()) && static_cast<int>(g_sequence()) != seq.last_seq + 1) {
			// Don't do this for meta streams: the sequence counter is typically
			// linked to the video capture to sync the metadata with the video
			// data. So the sequence counter would start at a non-zero value.
			warn_or_info(is_vivid, "got sequence number %u, expected %u\n",
				     g_sequence(), seq.last_seq + 1);
		}
		seq.last_seq = static_cast<int>(g_sequence());
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
	fail_on_test_val(ret != EINVAL, ret);
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

int testRemoveBufs(struct node *node)
{
	int ret;
	unsigned i;

	node->reopen();

	for (i = 1; i <= V4L2_BUF_TYPE_LAST; i++) {
		struct v4l2_remove_buffers removebufs = { };
		unsigned buffers;

		if (!(node->valid_buftypes & (1 << i)))
			continue;

		cv4l_queue q(i, V4L2_MEMORY_MMAP);

		if (testSetupVbi(node, i))
			continue;
		ret = q.remove_bufs(node, 0, 0);
		if (ret == ENOTTY)
			continue;

		q.init(i, V4L2_MEMORY_MMAP);
		ret = q.create_bufs(node, 0);

		memset(&removebufs, 0xff, sizeof(removebufs));
		removebufs.index = 0;
		removebufs.count = 0;
		removebufs.type = q.g_type();
		fail_on_test(doioctl(node, VIDIOC_REMOVE_BUFS, &removebufs));
		fail_on_test(check_0(removebufs.reserved, sizeof(removebufs.reserved)));

		buffer buf(i);

		/* Create only 1 buffer */
		fail_on_test(q.create_bufs(node, 1));
		buffers = q.g_buffers();
		fail_on_test(buffers != 1);
		/* Removing buffer index 1 must fail */
		fail_on_test(q.remove_bufs(node, 1, buffers) != EINVAL);
		/* Removing buffer index 0 is valid */
		fail_on_test(q.remove_bufs(node, 0, buffers));
		/* Removing buffer index 0 again must fail */
		fail_on_test(q.remove_bufs(node, 0, 1) != EINVAL);
		/* Create 3 buffers indexes 0 to 2 */
		fail_on_test(q.create_bufs(node, 3));
		/* Remove them one by one */
		fail_on_test(q.remove_bufs(node, 2, 1));
		fail_on_test(q.remove_bufs(node, 0, 1));
		fail_on_test(q.remove_bufs(node, 1, 1));
		/* Removing buffer index 0 again must fail */
		fail_on_test(q.remove_bufs(node, 0, 1) != EINVAL);

		/* Create 4 buffers indexes 0 to 3 */
		fail_on_test(q.create_bufs(node, 4));
		/* Remove buffers index 1 and 2 */
		fail_on_test(q.remove_bufs(node, 1, 2));
		/* Add 3 more buffers should be indexes 4 to 6 */
		fail_on_test(q.create_bufs(node, 3));
		/* Query buffers:
		 * 1 and 2 have been removed they must fail
		 * 0 and 3 to 6 must exist*/
		fail_on_test(buf.querybuf(node, 0));
		fail_on_test(buf.querybuf(node, 1) != EINVAL);
		fail_on_test(buf.querybuf(node, 2) != EINVAL);
		fail_on_test(buf.querybuf(node, 3));
		fail_on_test(buf.querybuf(node, 4));
		fail_on_test(buf.querybuf(node, 5));
		fail_on_test(buf.querybuf(node, 6));

		/* Remove existing buffer index 6 with bad type must fail */
		memset(&removebufs, 0xff, sizeof(removebufs));
		removebufs.index = 6;
		removebufs.count = 1;
		removebufs.type = 0;
		fail_on_test(doioctl(node, VIDIOC_REMOVE_BUFS, &removebufs) != EINVAL);

		/* Remove existing buffer index 6 with bad type and count == 0
		 * must fail */
		memset(&removebufs, 0xff, sizeof(removebufs));
		removebufs.index = 6;
		removebufs.count = 0;
		removebufs.type = 0;
		fail_on_test(doioctl(node, VIDIOC_REMOVE_BUFS, &removebufs) != EINVAL);

		/* Remove with count == 0 must always return 0 */
		fail_on_test(q.remove_bufs(node, 0, 0));
		fail_on_test(q.remove_bufs(node, 1, 0));
		fail_on_test(q.remove_bufs(node, 6, 0));
		fail_on_test(q.remove_bufs(node, 7, 0));
		fail_on_test(q.remove_bufs(node, 0xffffffff, 0));

		/* Remove crossing max allowed buffers boundary must fail */
		fail_on_test(q.remove_bufs(node, q.g_max_num_buffers() - 2, 7) != EINVAL);

		/* Remove overflow must fail */
		fail_on_test(q.remove_bufs(node, 3, 0xfffffff) != EINVAL);

		/* Remove 2 buffers on index 2 when index 2 is free must fail */
		fail_on_test(q.remove_bufs(node, 2, 2) != EINVAL);

		/* Remove 2 buffers on index 0 when index 1 is free must fail */
		fail_on_test(q.remove_bufs(node, 0, 2) != EINVAL);

		/* Remove 2 buffers on index 1 when index 1 and 2 are free must fail */
		fail_on_test(q.remove_bufs(node, 1, 2) != EINVAL);

		/* Create 2 buffers, that must fill the hole */
		fail_on_test(q.create_bufs(node, 2));
		/* Remove all buffers */
		fail_on_test(q.remove_bufs(node, 0, 7));

		fail_on_test(q.reqbufs(node, 0));
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
	fail_on_test_val(ret != EINVAL, ret);
	fail_on_test(node->node2 == nullptr);
	for (i = 1; i <= V4L2_BUF_TYPE_LAST; i++) {
		bool is_vbi_raw = (i == V4L2_BUF_TYPE_VBI_CAPTURE ||
				   i == V4L2_BUF_TYPE_VBI_OUTPUT);
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
		fail_on_test_val(ret && ret != EINVAL, ret);
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
		fail_on_test_val(ret && ret != EINVAL, ret);
		userptr_valid = !ret;
		fail_on_test(!mmap_valid && userptr_valid);
		if (caps)
			fail_on_test(userptr_valid ^ !!(caps & V4L2_BUF_CAP_SUPPORTS_USERPTR));

		q.init(i, V4L2_MEMORY_DMABUF);
		ret = q.reqbufs(node, 0);
		fail_on_test_val(ret && ret != EINVAL, ret);
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
			bool cache_hints_cap = false;
			bool coherent;

			cache_hints_cap = q.g_capabilities() & V4L2_BUF_CAP_SUPPORTS_MMAP_CACHE_HINTS;
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
			reqbufs.count = 1;
			reqbufs.type = i;
			reqbufs.memory = m;
			reqbufs.flags = V4L2_MEMORY_FLAG_NON_COHERENT;
			fail_on_test(doioctl(node, VIDIOC_REQBUFS, &reqbufs));
			coherent = reqbufs.flags & V4L2_MEMORY_FLAG_NON_COHERENT;
			if (!cache_hints_cap) {
				fail_on_test(coherent);
			} else {
				if (m == V4L2_MEMORY_MMAP)
					fail_on_test(!coherent);
				else
					fail_on_test(coherent);
			}
			q.reqbufs(node);

			ret = q.create_bufs(node, 0);
			if (ret == ENOTTY) {
				warn("VIDIOC_CREATE_BUFS not supported\n");
				break;
			}

			memset(&crbufs, 0xff, sizeof(crbufs));
			node->g_fmt(crbufs.format, i);
			crbufs.count = 1;
			crbufs.memory = m;
			crbufs.flags = V4L2_MEMORY_FLAG_NON_COHERENT;
			fail_on_test(doioctl(node, VIDIOC_CREATE_BUFS, &crbufs));
			fail_on_test(check_0(crbufs.reserved, sizeof(crbufs.reserved)));
			fail_on_test(crbufs.index != q.g_buffers());

			coherent = crbufs.flags & V4L2_MEMORY_FLAG_NON_COHERENT;
			if (!cache_hints_cap) {
				fail_on_test(coherent);
			} else {
				if (m == V4L2_MEMORY_MMAP)
					fail_on_test(!coherent);
				else
					fail_on_test(coherent);
			}

			if (cache_hints_cap) {
				/*
				 * Different memory consistency model. Should fail for MMAP
				 * queues which support cache hints.
				 */
				crbufs.flags = 0;
				if (m == V4L2_MEMORY_MMAP)
					fail_on_test(doioctl(node, VIDIOC_CREATE_BUFS, &crbufs) != EINVAL);
				else
					fail_on_test(doioctl(node, VIDIOC_CREATE_BUFS, &crbufs));
			}
			q.reqbufs(node);

			fail_on_test(q.create_bufs(node, 1));
			fail_on_test(q.g_buffers() == 0);
			fail_on_test(q.g_type() != i);
			fail_on_test(testQueryBuf(node, i, q.g_buffers()));
			fail_on_test(q.create_bufs(node, 1));
			fail_on_test(testQueryBuf(node, i, q.g_buffers()));
			if (!node->is_m2m)
				fail_on_test(q2.create_bufs(node->node2, 1) != EBUSY);
			q.reqbufs(node);

			cv4l_fmt fmt;

			node->g_fmt(fmt, q.g_type());
			if (V4L2_TYPE_IS_MULTIPLANAR(q.g_type())) {
				// num_planes == 0 is not allowed
				fmt.s_num_planes(0);
				fail_on_test(q.create_bufs(node, 1, &fmt) != EINVAL);
				node->g_fmt(fmt, q.g_type());

				if (fmt.g_num_planes() > 1) {
					// fewer planes than required by the format
					// is not allowed
					fmt.s_num_planes(fmt.g_num_planes() - 1);
					fail_on_test(q.create_bufs(node, 1, &fmt) != EINVAL);
					node->g_fmt(fmt, q.g_type());

					// A last plane with a 0 sizeimage is not allowed
					fmt.s_sizeimage(0, fmt.g_num_planes() - 1);
					fail_on_test(q.create_bufs(node, 1, &fmt) != EINVAL);
					node->g_fmt(fmt, q.g_type());
				}

				if (fmt.g_num_planes() < VIDEO_MAX_PLANES) {
					// Add an extra plane, but with size 0. The vb2
					// framework should test for this.
					fmt.s_num_planes(fmt.g_num_planes() + 1);
					fmt.s_sizeimage(0, fmt.g_num_planes() - 1);
					fail_on_test(q.create_bufs(node, 1, &fmt) != EINVAL);
					node->g_fmt(fmt, q.g_type());

					// This test is debatable: should we allow CREATE_BUFS
					// to create buffers with more planes than required
					// by the format?
					//
					// For now disallow this. If there is a really good
					// reason for allowing this, then that should be
					// documented and carefully tested.
					//
					// It is the driver in queue_setup that has to check
					// this.
					fmt.s_num_planes(fmt.g_num_planes() + 1);
					fmt.s_sizeimage(65536, fmt.g_num_planes() - 1);
					fail_on_test(q.create_bufs(node, 1, &fmt) != EINVAL);
					node->g_fmt(fmt, q.g_type());
				}
			}
			if (is_vbi_raw) {
				fmt.fmt.vbi.count[0] = 0;
				fmt.fmt.vbi.count[1] = 0;
			} else {
				fmt.s_sizeimage(0, 0);
			}
			// zero size for the first plane is not allowed
			fail_on_test(q.create_bufs(node, 1, &fmt) != EINVAL);
			node->g_fmt(fmt, q.g_type());

			// plane sizes that are too small are not allowed
			for (unsigned p = 0; p < fmt.g_num_planes(); p++) {
				if (is_vbi_raw) {
					fmt.fmt.vbi.count[0] /= 2;
					fmt.fmt.vbi.count[1] /= 2;
				} else {
					fmt.s_sizeimage(fmt.g_sizeimage(p) / 2, p);
				}
			}
			fail_on_test(q.create_bufs(node, 1, &fmt) != EINVAL);
			fail_on_test(testQueryBuf(node, fmt.type, q.g_buffers()));
			node->g_fmt(fmt, q.g_type());

			// Add 1 MB to each plane or double the vbi counts.
			// This is allowed.
			for (unsigned p = 0; p < fmt.g_num_planes(); p++) {
				if (is_vbi_raw) {
					fmt.fmt.vbi.count[0] *= 2;
					fmt.fmt.vbi.count[1] *= 2;
				} else {
					fmt.s_sizeimage(fmt.g_sizeimage(p) + (1 << 20), p);
				}
			}
			fail_on_test(q.create_bufs(node, 1, &fmt));
			buffer buf(q);

			// Check that the new buffer lengths are at least those of
			// the large sizes as specified by CREATE_BUFS
			fail_on_test(buf.querybuf(node, 0));
			fail_on_test(buf.g_num_planes() != fmt.g_num_planes());
			// Verify that the new buffers actually have the requested
			// buffer size
			for (unsigned p = 0; p < buf.g_num_planes(); p++)
				fail_on_test(buf.g_length(p) < fmt.g_sizeimage(p));
			node->g_fmt(fmt, q.g_type());
		}
		fail_on_test(q.reqbufs(node));
	}
	return 0;
}

int testCreateBufsMax(struct node *node)
{
	unsigned int i;
	int ret;

	node->reopen();

	cv4l_queue q(0, 0);

	for (i = 1; i <= V4L2_BUF_TYPE_LAST; i++) {
		if (!(node->valid_buftypes & (1 << i)))
			continue;

		q.init(i, V4L2_MEMORY_MMAP);
		ret = q.create_bufs(node, 0);
		if (!ret && (q.g_capabilities() & V4L2_BUF_CAP_SUPPORTS_MAX_NUM_BUFFERS)) {
			fail_on_test(q.create_bufs(node, q.g_max_num_buffers()));
			/* Some drivers may not have allocated all the requested buffers
			 * because of memory limitation, that is OK but make the next test
			 * failed so skip it
			 */
			if (q.g_max_num_buffers() != q.g_buffers())
				continue;
			ret = q.create_bufs(node, 1);
			fail_on_test(ret != ENOBUFS);
		}
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
	int ret, ret2;
	int err, err2;

	fcntl(node->g_fd(), F_SETFL, fd_flags | O_NONBLOCK);
	errno = 0;
	if (node->can_capture)
		ret = node->read(&buf, 1);
	else
		ret = node->write(&buf, 1);
	err = errno;
	fail_on_test(v4l_has_vbi(node->g_v4l_fd()) &&
		     !(node->cur_io_caps & V4L2_IN_CAP_STD) && ret >= 0);

	// Note: RDS can only return multiples of 3, so we accept
	// both 0 and 1 as return code.
	// EBUSY can be returned when attempting to read/write to a
	// multiplanar format.
	// EINVAL can be returned if read()/write() is not supported
	// for the current input/output.
	if (can_rw)
		fail_on_test((ret < 0 && err != EAGAIN && err != EBUSY && err != EINVAL) || ret > 1);
	else
		fail_on_test(ret >= 0 || err != EINVAL);
	if (!can_rw)
		return ENOTTY;

	node->reopen();
	fcntl(node->g_fd(), F_SETFL, fd_flags | O_NONBLOCK);

	/* check that the close cleared the busy flag */
	errno = 0;
	if (node->can_capture)
		ret2 = node->read(&buf, 1);
	else
		ret2 = node->write(&buf, 1);
	err2 = errno;
	fail_on_test(ret2 != ret || err2 != err);
	return 0;
}

static int setupM2M(struct node *node, cv4l_queue &q, bool init = true)
{
	unsigned buffers = 2;
	__u32 caps;

	last_m2m_seq.init();

	if (node->codec_mask & STATEFUL_DECODER) {
		struct v4l2_control ctrl = {
			.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
		};
		fail_on_test(node->g_ctrl(ctrl));
		fail_on_test(ctrl.value <= 0);
		buffers = ctrl.value;
		fail_on_test(q.reqbufs(node, 1));
		fail_on_test((unsigned)ctrl.value < q.g_buffers());
	}
	fail_on_test(q.reqbufs(node, buffers));
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
	static constexpr const char *pollmode_str[] = {
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

		/*
		 * Many older versions of the vb2 and m2m have a bug where
		 * EPOLLIN and EPOLLOUT events are never signaled unless they
		 * are part of the initial EPOLL_CTL_ADD. We set an initial
		 * empty set of events, which we then modify with EPOLL_CTL_MOD,
		 * in order to detect that condition.
		 */
		ev.events = 0;
		fail_on_test(epoll_ctl(epollfd, EPOLL_CTL_ADD, node->g_fd(), &ev));

		if (node->is_m2m)
			ev.events = EPOLLIN | EPOLLOUT | EPOLLPRI;
		else if (v4l_type_is_output(q.g_type()))
			ev.events = EPOLLOUT;
		else
			ev.events = EPOLLIN;
		ev.data.fd = node->g_fd();
		fail_on_test(epoll_ctl(epollfd, EPOLL_CTL_MOD, node->g_fd(), &ev));
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
				ret = select(node->g_fd() + 1, nullptr, &wfds, nullptr, &tv);
			else
				ret = select(node->g_fd() + 1, &rfds, nullptr, nullptr, &tv);
			fail_on_test(ret == 0);
			fail_on_test(ret < 0);
			fail_on_test(!FD_ISSET(node->g_fd(), &rfds) &&
				     !FD_ISSET(node->g_fd(), &wfds) &&
				     !FD_ISSET(node->g_fd(), &efds));
			can_read = FD_ISSET(node->g_fd(), &rfds);
			have_event = FD_ISSET(node->g_fd(), &efds);
		} else if (pollmode == POLL_MODE_EPOLL) {
			/*
			 * This can fail with a timeout on older kernels for
			 * drivers using vb2_core_poll() v4l2_m2m_poll().
			 */
			ret = epoll_wait(epollfd, &ev, 1, 2000);
			fail_on_test(ret == 0);
			fail_on_test_val(ret < 0, ret);
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
			fail_on_test_val(ret, ret);
			if (show_info)
				printf("\t\t%s Buffer: %d Sequence: %d Field: %s Size: %d Flags: %s Timestamp: %lld.%06llds\n",
				       v4l_type_is_output(buf.g_type()) ? "Out" : "Cap",
				       buf.g_index(), buf.g_sequence(),
				       field2s(buf.g_field()).c_str(), buf.g_bytesused(),
				       bufferflags2s(buf.g_flags()).c_str(),
				       static_cast<__u64>(buf.g_timestamp().tv_sec),  static_cast<__u64>(buf.g_timestamp().tv_usec));
			for (unsigned p = 0; p < buf.g_num_planes(); p++) {
				if (max_bytesused[p] < buf.g_bytesused(p))
					max_bytesused[p] = buf.g_bytesused(p);
				if (min_data_offset[p] > buf.g_data_offset(p))
					min_data_offset[p] = buf.g_data_offset(p);
			}

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
			// This is not necessarily wrong (v4l-touch drivers can do this),
			// but it is certainly unusual enough to warn about this.
			if (buf.g_flags() & V4L2_BUF_FLAG_DONE)
				warn_once("QBUF returned the buffer as DONE.\n");
			if (buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD) {
				fail_on_test(doioctl_fd(buf_req_fds[req_idx],
							MEDIA_REQUEST_IOC_QUEUE, nullptr));
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
			printf("\t\t%s Buffer: %d Sequence: %d Field: %s Size: %d Flags: %s Timestamp: %lld.%06llds\n",
			       v4l_type_is_output(buf.g_type()) ? "Out" : "Cap",
			       buf.g_index(), buf.g_sequence(),
			       field2s(buf.g_field()).c_str(), buf.g_bytesused(),
			       bufferflags2s(buf.g_flags()).c_str(),
			       static_cast<__u64>(buf.g_timestamp().tv_sec), static_cast<__u64>(buf.g_timestamp().tv_usec));
		fail_on_test_val(ret, ret);
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
	fail_on_test_val(ret != EINVAL && ret != ENOTTY, ret);
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
	bool cache_hints = q.g_capabilities() & V4L2_BUF_CAP_SUPPORTS_MMAP_CACHE_HINTS;

	fail_on_test(q.mmap_bufs(node));
	for (unsigned i = 0; i < q.g_buffers(); i++) {
		buffer buf(q);
		unsigned int flags;
		int ret;

		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.check(q, Unqueued, i));

		/*
		 * Do not set cache hints for all the buffers, but only on
		 * some of them, so that we can test more cases.
		 */
		if (i == 0) {
			flags = buf.g_flags();
			flags |= V4L2_BUF_FLAG_NO_CACHE_INVALIDATE;
			flags |= V4L2_BUF_FLAG_NO_CACHE_CLEAN;
			buf.s_flags(flags);
		}

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
			fail_on_test_val(ret && ret != ENOTTY, ret);
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
		flags = buf.g_flags();
		if (cache_hints) {
			if (i == 0) {
				/* We do expect cache hints on this buffer */
				fail_on_test(!(flags & V4L2_BUF_FLAG_NO_CACHE_INVALIDATE));
				fail_on_test(!(flags & V4L2_BUF_FLAG_NO_CACHE_CLEAN));
			} else {
				/* We expect no cache hints on this buffer */
				fail_on_test(flags & V4L2_BUF_FLAG_NO_CACHE_INVALIDATE);
				fail_on_test(flags & V4L2_BUF_FLAG_NO_CACHE_CLEAN);
			}
		} else if (node->might_support_cache_hints) {
			fail_on_test(flags & V4L2_BUF_FLAG_NO_CACHE_INVALIDATE);
			fail_on_test(flags & V4L2_BUF_FLAG_NO_CACHE_CLEAN);
		}
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.check(q, Queued, i));
		fail_on_test(!buf.dqbuf(node));
	}
	return 0;
}

int testMmap(struct node *node, struct node *node_m2m_cap, unsigned frame_count,
	     enum poll_mode pollmode, bool use_create_bufs)
{
	bool can_stream = node->g_caps() & V4L2_CAP_STREAMING;
	bool have_createbufs = true;
	int type;
	int ret;

	if (!node->valid_buftypes)
		return ENOTTY;

	buffer_info.clear();
	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
		unsigned reqbufs_buf_count;
		unsigned buffers = 2;
		cv4l_fmt fmt;

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

		if (node->is_m2m && (node->codec_mask & STATEFUL_ENCODER)) {
			struct v4l2_control ctrl = {
				.id = V4L2_CID_MIN_BUFFERS_FOR_OUTPUT,
			};

			fail_on_test(node->g_ctrl(ctrl));
			fail_on_test(ctrl.value <= 0);
			buffers = ctrl.value;
			fail_on_test(q.reqbufs(node, 1));
			fail_on_test((unsigned)ctrl.value < q.g_buffers());
		}
		fail_on_test(q.reqbufs(node, buffers));
		reqbufs_buf_count = q.g_buffers();
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
		fail_on_test_val(ret != ENOTTY && ret != 0, ret);
		if (ret == ENOTTY) {
			have_createbufs = false;
			if (use_create_bufs)
				return ENOTTY;
		}
		if (have_createbufs) {
			q.reqbufs(node);
			q.create_bufs(node, 2, &cur_fmt, V4L2_MEMORY_FLAG_NON_COHERENT);
			fail_on_test(setupMmap(node, q));
			q.munmap_bufs(node);
			q.reqbufs(node, buffers);

			cv4l_fmt fmt(cur_fmt);

			if (node->is_video) {
				last_seq.last_field = cur_fmt.g_field();
				fmt.s_height(fmt.g_height() / 2);
				for (unsigned p = 0; p < fmt.g_num_planes(); p++)
					fmt.s_sizeimage(fmt.g_sizeimage(p) / 2, p);
				fail_on_test(q.create_bufs(node, 1, &fmt) != EINVAL);
				fail_on_test(testQueryBuf(node, cur_fmt.type, q.g_buffers()));
				q.reqbufs(node);
				fail_on_test(q.create_bufs(node, 1, &fmt) != EINVAL);
				fail_on_test(testQueryBuf(node, cur_fmt.type, q.g_buffers()));
				fmt = cur_fmt;
				for (unsigned p = 0; p < fmt.g_num_planes(); p++)
					fmt.s_sizeimage(fmt.g_sizeimage(p) * 2, p);
				q.reqbufs(node, buffers);
			}
			fail_on_test(q.create_bufs(node, 1, &fmt));
			if (node->is_video) {
				buffer buf(q);

				fail_on_test(buf.querybuf(node, q.g_buffers() - 1));
				for (unsigned p = 0; p < fmt.g_num_planes(); p++)
					fail_on_test(buf.g_length(p) < fmt.g_sizeimage(p));
			}
			fail_on_test(q.reqbufs(node));
			if (use_create_bufs) {
				fmt = cur_fmt;
				if (node->is_video)
					for (unsigned p = 0; p < fmt.g_num_planes(); p++)
						fmt.s_sizeimage(fmt.g_sizeimage(p) + 10240, p);
				q.create_bufs(node, reqbufs_buf_count - 1, &fmt);
				q.create_bufs(node, 1, &fmt);
				fail_on_test(q.g_buffers() != reqbufs_buf_count);
				if (node->is_video) {
					for (unsigned b = 0; b < q.g_buffers(); b++) {
						buffer buf(q);

						fail_on_test(buf.querybuf(node, b));
						for (unsigned p = 0; p < fmt.g_num_planes(); p++)
							fail_on_test(buf.g_length(p) != fmt.g_sizeimage(p));
					}
				}
				fail_on_test(q.reqbufs(node));
				q.create_bufs(node, reqbufs_buf_count, &cur_fmt);
				fail_on_test(q.g_buffers() != reqbufs_buf_count);
			} else {
				fail_on_test(q.reqbufs(node, reqbufs_buf_count));
			}
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
				ret = select(node->g_fd() + 1, nullptr, nullptr, &efds, &tv);
				fail_on_test_val(ret < 0, ret);
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
	for (unsigned i = 0; i < q.g_buffers(); i++) {
		buffer buf(q);
		unsigned int flags;
		int ret;

		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.check(q, Unqueued, i));

		flags = buf.g_flags();
		flags |= V4L2_BUF_FLAG_NO_CACHE_INVALIDATE;
		flags |= V4L2_BUF_FLAG_NO_CACHE_CLEAN;
		buf.s_flags(flags);

		for (unsigned p = 0; p < buf.g_num_planes(); p++) {
			// This should not work!
			fail_on_test(node->mmap(buf.g_length(p), 0) != MAP_FAILED);
		}

		ret = ENOTTY;
		// Try to use VIDIOC_PREPARE_BUF for every other buffer
		if ((i & 1) == 0) {
			for (unsigned p = 0; p < buf.g_num_planes(); p++)
				buf.s_userptr(nullptr, p);
			ret = buf.prepare_buf(node);
			fail_on_test(!ret);
			for (unsigned p = 0; p < buf.g_num_planes(); p++)
				buf.s_userptr((char *)0x0eadb000 + buf.g_length(p), p);
			ret = buf.prepare_buf(node);
			fail_on_test(!ret);
			for (unsigned p = 0; p < buf.g_num_planes(); p++)
				buf.s_userptr(q.g_userptr(i, p), p);
			ret = buf.prepare_buf(node, q);
			fail_on_test_val(ret && ret != ENOTTY, ret);

			if (ret == 0) {
				fail_on_test(buf.querybuf(node, i));
				fail_on_test(buf.check(q, Prepared, i));
				for (unsigned p = 0; p < buf.g_num_planes(); p++) {
					buf.s_userptr(nullptr, p);
					buf.s_bytesused(0, p);
					buf.s_length(0, p);
				}
			}
		}
		if (ret == ENOTTY) {
			for (unsigned p = 0; p < buf.g_num_planes(); p++)
				buf.s_userptr(nullptr, p);
			ret = buf.qbuf(node);
			fail_on_test(!ret);

			for (unsigned p = 0; p < buf.g_num_planes(); p++)
				buf.s_userptr((char *)0x0eadb000 + buf.g_length(p), p);
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
		flags = buf.g_flags();
		fail_on_test(flags & V4L2_BUF_FLAG_NO_CACHE_INVALIDATE);
		fail_on_test(flags & V4L2_BUF_FLAG_NO_CACHE_CLEAN);
		fail_on_test(flags & V4L2_BUF_FLAG_DONE);
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.check(q, Queued, i));
	}
	return 0;
}

int testUserPtr(struct node *node, struct node *node_m2m_cap, unsigned frame_count,
		enum poll_mode pollmode)
{
	const __u32 filler = 0xdeadbeef;
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
			fail_on_test_val(!can_stream && ret != ENOTTY, ret);
			fail_on_test_val(can_stream && ret != EINVAL, ret);
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

		__u32 *buffers[q.g_buffers()][q.g_num_planes()];

		/*
		 * The alignment malloc uses depends on the gcc version and
		 * architecture. Applications compiled for 64-bit all use a
		 * 16 byte alignment. Applications compiled for 32-bit will
		 * use an 8 byte alignment if glibc was compiled with gcc
		 * version 6 or older, and 16 bytes when compiled with a newer
		 * gcc. This is due to the support for _float128 that gcc
		 * added in version 7 and that required this alignment change.
		 *
		 * Bottom line, in order to test user pointers the assumption
		 * has to be that the DMA can handle writing just 8 bytes to a
		 * page, since that's the worst case scenario.
		 */
		for (unsigned i = 0; i < q.g_buffers(); i++) {
			for (unsigned p = 0; p < q.g_num_planes(); p++) {
				/* ensure that len is a multiple of 4 */
				__u32 len = ((q.g_length(p) + 3) & ~0x3) + 4 * 4096;
				auto m = static_cast<__u32 *>(malloc(len));

				fail_on_test(!m);
				fail_on_test((uintptr_t)m & 0x7);
				for (__u32 *x = m; x < m + len / 4; x++)
					*x = filler;
				buffers[i][p] = m;
				m = m + 2 * 4096 / 4;
				/*
				 * Put the start of the buffer at the last 8 bytes
				 * of a page.
				 */
				m = (__u32 *)(((uintptr_t)m & ~0xfff) | 0xff8);
				q.s_userptr(i, p, m);
			}
		}
		// captureBufs() will update these values
		memset(max_bytesused, 0, sizeof(max_bytesused));
		memset(min_data_offset, 0xff, sizeof(min_data_offset));

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
				int fd_flags = fcntl(node->g_fd(), F_GETFL);
				struct timeval tv = { 1, 0 };
				fd_set efds;
				v4l2_event ev;

				fcntl(node->g_fd(), F_SETFL, fd_flags | O_NONBLOCK);
				FD_ZERO(&efds);
				FD_SET(node->g_fd(), &efds);
				ret = select(node->g_fd() + 1, nullptr, nullptr, &efds, &tv);
				fail_on_test_val(ret < 0, ret);
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
		if (node->is_m2m) {
			fail_on_test(node_m2m_cap->streamoff(m2m_q.g_type()));
			m2m_q.munmap_bufs(node_m2m_cap);
			fail_on_test(m2m_q.reqbufs(node_m2m_cap, 0));
			fail_on_test(!capture_count);
		}
		for (unsigned i = 0; i < q.g_buffers(); i++) {
			for (unsigned p = 0; p < q.g_num_planes(); p++) {
				__u32 buflen = (q.g_length(p) + 3U) & ~3U;
				__u32 memlen = buflen + 4 * 4096;
				__u32 *m = buffers[i][p];
				auto u = static_cast<__u32 *>(q.g_userptr(i, p));

				for (__u32 *x = m; x < u; x++)
					if (*x != filler)
						fail("data at %zd bytes before start of the buffer was touched\n",
						     (u - x) * 4);

				unsigned data_offset = min_data_offset[p];
				data_offset = (data_offset + 3U) & ~3U;
				if (!v4l_type_is_output(type) && u[data_offset / 4] == filler)
					fail("data at data_offset %u was untouched\n", data_offset);

				unsigned used = max_bytesused[p];
				// Should never happen
				fail_on_test(!used);
				used = (used + 3U) & ~3U;

				for (__u32 *x = u + used / 4; x < u + buflen / 4; x++) {
					if (*x == filler)
						continue;
					warn_once("data from max bytesused %u+%zd to length %u was touched in plane %u\n",
						  used, (x - u) * 4 - used, buflen, p);
					break;
				}
				for (__u32 *x = u + buflen / 4; x < m + memlen / 4; x++)
					if (*x != filler)
						fail("data at %zd bytes after the end of the buffer was touched\n",
						     (x - (u + buflen / 4)) * 4);
				free(m);
				q.s_userptr(i, p, nullptr);
			}
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
			if (exp_q.g_length(p) < buf.g_length(p))
				return fail("exp_q.g_length(%u) < buf.g_length(%u): %u < %u\n",
					    p, p, exp_q.g_length(p), buf.g_length(p));
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
		unsigned int flags;
		int ret;

		buf.init(q, i);
		fail_on_test(buf.querybuf(node, i));
		for (unsigned p = 0; p < buf.g_num_planes(); p++)
			buf.s_fd(q.g_fd(i, p), p);
		flags = buf.g_flags();
		flags |= V4L2_BUF_FLAG_NO_CACHE_INVALIDATE;
		flags |= V4L2_BUF_FLAG_NO_CACHE_CLEAN;
		buf.s_flags(flags);
		ret = buf.prepare_buf(node, q);
		if (ret != ENOTTY) {
			fail_on_test_val(ret, ret);
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
			fail_on_test(buf.g_fd(p) != q.g_fd(i, p));
			fail_on_test(buf.g_length(p) != q.g_length(p));
			if (v4l_type_is_output(q.g_type()))
				fail_on_test(!buf.g_bytesused(p));
		}
		flags = buf.g_flags();

		/* Make sure that flags are cleared */
		fail_on_test(flags & V4L2_BUF_FLAG_NO_CACHE_INVALIDATE);
		fail_on_test(flags & V4L2_BUF_FLAG_NO_CACHE_CLEAN);
		fail_on_test(flags & V4L2_BUF_FLAG_DONE);
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
			fail_on_test_val(!can_stream && ret != ENOTTY, ret);
			fail_on_test_val(can_stream && ret != EINVAL, ret);
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
				int fd_flags = fcntl(node->g_fd(), F_GETFL);
				struct timeval tv = { 1, 0 };
				fd_set efds;
				v4l2_event ev;

				fcntl(node->g_fd(), F_SETFL, fd_flags | O_NONBLOCK);
				FD_ZERO(&efds);
				FD_SET(node->g_fd(), &efds);
				ret = select(node->g_fd() + 1, nullptr, nullptr, &efds, &tv);
				fail_on_test_val(ret < 0, ret);
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
	struct test_query_ext_ctrl valid_qctrl;
	v4l2_ext_controls ctrls;
	v4l2_ext_control ctrl;
	v4l2_ext_control vivid_ro_ctrl = {
		.id = VIVID_CID_RO_INTEGER,
	};
	v4l2_ext_controls vivid_ro_ctrls = {};
	// Note: the vivid dynamic array has range 10-90
	// and the maximum number of elements is 100.
	__u32 vivid_dyn_array[101] = {};
	// Initialize with these values
	static const __u32 vivid_dyn_array_init[16] = {
		 6, 12, 18, 24, 30, 36, 42, 48,
		54, 60, 66, 72, 78, 84, 90, 96
	};
	// This is the clamped version to compare against
	static const __u32 vivid_dyn_array_clamped[16] = {
		10, 12, 18, 24, 30, 36, 42, 48,
		54, 60, 66, 72, 78, 84, 90, 90
	};
	const unsigned elem_size = sizeof(vivid_dyn_array[0]);
	v4l2_ext_control vivid_dyn_array_ctrl = {
		.id = VIVID_CID_U32_DYN_ARRAY,
	};
	v4l2_ext_controls vivid_dyn_array_ctrls = {};
	unsigned vivid_pixel_array_size = 0;
	v4l2_ext_control vivid_pixel_array_ctrl = {
		.id = VIVID_CID_U8_PIXEL_ARRAY,
	};
	v4l2_ext_controls vivid_pixel_array_ctrls = {};
	bool have_controls;
	int ret;

	// Note: trying to initialize vivid_ro_ctrls as was done for
	// vivid_ro_ctrl fails with gcc 7 with this error:
	// sorry, unimplemented: non-trivial designated initializers not supported
	// So just set this struct the old-fashioned way.
	vivid_ro_ctrls.which = V4L2_CTRL_WHICH_REQUEST_VAL;
	vivid_ro_ctrls.count = 1;
	vivid_ro_ctrls.controls = &vivid_ro_ctrl;

	if (is_vivid) {
		v4l2_query_ext_ctrl qec = { .id = VIVID_CID_U8_PIXEL_ARRAY };
		node->query_ext_ctrl(qec);
		vivid_pixel_array_size = qec.elems;
	}
	__u8 vivid_pixel_array[vivid_pixel_array_size + 1];
	vivid_pixel_array[vivid_pixel_array_size] = 0xff;
	vivid_pixel_array_ctrl.size = vivid_pixel_array_size;
	vivid_pixel_array_ctrl.p_u8 = vivid_pixel_array;

	// If requests are supported, then there must be a media device
	if (node->buf_caps & V4L2_BUF_CAP_SUPPORTS_REQUESTS)
		fail_on_test(media_fd < 0);

	// Check if the driver has controls that can be used to test requests
	memset(&valid_qctrl, 0, sizeof(valid_qctrl));
	memset(&ctrls, 0, sizeof(ctrls));
	memset(&ctrl, 0, sizeof(ctrl));
	for (auto &control : node->controls) {
		test_query_ext_ctrl &qctrl = control.second;

		if (qctrl.type != V4L2_CTRL_TYPE_INTEGER &&
		    qctrl.type != V4L2_CTRL_TYPE_BOOLEAN)
			continue;
		if (qctrl.flags & V4L2_CTRL_FLAG_WRITE_ONLY ||
		    qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY)
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

	// Test if V4L2_CTRL_WHICH_REQUEST_VAL is supported
	ctrls.which = V4L2_CTRL_WHICH_REQUEST_VAL;
	ret = doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls);
	fail_on_test_val(ret != EINVAL && ret != EBADR && ret != ENOTTY, ret);
	have_controls = ret != ENOTTY;

	if (media_fd < 0 || ret == EBADR) {
		// Should not happen if requests are supported
		fail_on_test(node->buf_caps & V4L2_BUF_CAP_SUPPORTS_REQUESTS);
		return ENOTTY;
	}
	if (have_controls) {
		ctrls.request_fd = 10;
		// Test that querying controls with an invalid request_fd
		// returns EINVAL
		fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls) != EINVAL);
	}
	ret = doioctl_fd(media_fd, MEDIA_IOC_REQUEST_ALLOC, &req_fd);
	if (ret == ENOTTY) {
		// Should not happen if requests are supported
		fail_on_test(node->buf_caps & V4L2_BUF_CAP_SUPPORTS_REQUESTS);
		return ENOTTY;
	}
	// Check that a request was allocated with a valid fd
	fail_on_test_val(ret, ret);
	fail_on_test(req_fd < 0);
	fhs.add(req_fd);
	if (have_controls) {
		ctrls.request_fd = req_fd;
		// The request is in unqueued state, so this should return EACCES
		fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls) != EACCES);
	}
	// You cannot queue a request that has no buffer
	fail_on_test(doioctl_fd(req_fd, MEDIA_REQUEST_IOC_QUEUE, nullptr) != ENOENT);
	// REINIT must work for an unqueued request
	fail_on_test(doioctl_fd(req_fd, MEDIA_REQUEST_IOC_REINIT, nullptr));
	// Close media_fd
	fhs.del(media_fd);
	// This should have no effect on req_fd
	fail_on_test(doioctl_fd(req_fd, MEDIA_REQUEST_IOC_QUEUE, nullptr) != ENOENT);
	fail_on_test(doioctl_fd(req_fd, MEDIA_REQUEST_IOC_REINIT, nullptr));
	// Close req_fd
	fhs.del(req_fd);
	// G_EXT_CTRLS must now return EINVAL for req_fd since it no longer exists
	if (have_controls)
		fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls) != EINVAL);
	// And the media request ioctls now must return EBADF
	fail_on_test(doioctl_fd(req_fd, MEDIA_REQUEST_IOC_QUEUE, nullptr) != EBADF);
	fail_on_test(doioctl_fd(req_fd, MEDIA_REQUEST_IOC_REINIT, nullptr) != EBADF);

	// Open media_fd and alloc a request again
	media_fd = fhs.add(mi_get_media_fd(node->g_fd(), node->bus_info));
	fail_on_test(doioctl_fd(media_fd, MEDIA_IOC_REQUEST_ALLOC, &req_fd));
	fhs.add(req_fd);
	ctrls.count = 1;
	ctrls.controls = &ctrl;
	if (have_controls) {
		ctrl.value = valid_qctrl.minimum;
		ctrls.which = 0;
		// Set control without requests
		fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls));
		ctrl.value = valid_qctrl.maximum;
		ctrls.which = V4L2_CTRL_WHICH_REQUEST_VAL;
		ctrls.request_fd = req_fd;
		// Set control for a request
		fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls));
		ctrl.value = valid_qctrl.minimum;
		ctrls.request_fd = req_fd;
		// But you cannot get the value of an unqueued request
		fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls) != EACCES);
		ctrls.which = 0;
		// But you can without a request
		fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls));
		fail_on_test(ctrl.value != valid_qctrl.minimum);
		ctrls.request_fd = req_fd;
		ctrls.which = V4L2_CTRL_WHICH_REQUEST_VAL;
		ctrl.id = 1;
		// Setting an invalid control in a request must fail
		fail_on_test(!doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls));
		// And also when trying to read an invalid control of a request
		fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls) != EACCES);
	}
	ctrl.id = valid_qctrl.id;
	// Close req_fd and media_fd and reopen device node
	fhs.del(req_fd);
	fhs.del(media_fd);
	node->reopen();

	int type = node->g_type();
	// For m2m devices g_type() will return the capture type, so
	// we need to invert it to get the output type.
	// At the moment only the output type of an m2m device can use
	// requests.
	if (node->is_m2m)
		type = v4l_type_invert(type);
	if (v4l_type_is_vbi(type)) {
		cv4l_fmt fmt;

		node->g_fmt(fmt, type);
		node->s_fmt(fmt, type);
	}

	if (!(node->valid_buftypes & (1 << type))) {
		// If the type is not supported, then check that requests
		// are also not supported.
		fail_on_test(node->buf_caps & V4L2_BUF_CAP_SUPPORTS_REQUESTS);
		return ENOTTY;
	}
	bool supports_requests = node->buf_caps & V4L2_BUF_CAP_SUPPORTS_REQUESTS;

	buffer_info.clear();

	cv4l_queue q(type, V4L2_MEMORY_MMAP);
	// For m2m devices q is the output queue and m2m_q is the capture queue
	cv4l_queue m2m_q(v4l_type_invert(type));

	q.init(type, V4L2_MEMORY_MMAP);
	fail_on_test(q.reqbufs(node, 15));

	unsigned min_bufs = q.g_buffers();
	fail_on_test(q.reqbufs(node, min_bufs + 6));
	unsigned num_bufs = q.g_buffers();
	// Create twice as many requests as there are buffers
	unsigned num_requests = 2 * num_bufs;
	last_seq.init();

	media_fd = fhs.add(mi_get_media_fd(node->g_fd(), node->bus_info));

	// Allocate the requests
	for (unsigned i = 0; i < num_requests; i++) {
		fail_on_test(doioctl_fd(media_fd, MEDIA_IOC_REQUEST_ALLOC, &buf_req_fds[i]));
		fhs.add(buf_req_fds[i]);
		fail_on_test(buf_req_fds[i] < 0);
		// Check that empty requests can't be queued
		fail_on_test(!doioctl_fd(buf_req_fds[i], MEDIA_REQUEST_IOC_QUEUE, nullptr));
	}
	// close the media fd, should not be needed anymore
	fhs.del(media_fd);

	buffer buf(q);

	fail_on_test(buf.querybuf(node, 0));
	// Queue a buffer without using requests
	ret = buf.qbuf(node);
	// If this fails, then that can only happen if the queue
	// requires requests. In that case EBADR is returned.
	fail_on_test_val(ret && ret != EBADR, ret);
	fail_on_test(buf.querybuf(node, 1));
	// Now try to queue the buffer to the request
	buf.s_flags(V4L2_BUF_FLAG_REQUEST_FD);
	buf.s_request_fd(buf_req_fds[1]);
	// If requests are required, then this must now work
	// If requests are optional, then this must now fail since the
	// queue in is non-request mode.
	if (ret == EBADR)
		fail_on_test(buf.qbuf(node));
	else
		fail_on_test(!buf.qbuf(node));

	// Reopen device node, clearing any pending requests
	node->reopen();

	q.init(type, V4L2_MEMORY_MMAP);
	fail_on_test(q.reqbufs(node, num_bufs));

	if (node->is_m2m) {
		// Setup the capture queue
		fail_on_test(m2m_q.reqbufs(node, 2));
		fail_on_test(node->streamon(m2m_q.g_type()));

		buffer buf(m2m_q);

		fail_on_test(buf.querybuf(node, 0));
		buf.s_flags(V4L2_BUF_FLAG_REQUEST_FD);
		buf.s_request_fd(buf_req_fds[0]);
		// Only the output queue can support requests,
		// so if the capture queue also supports requests,
		// then something is wrong.
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
		// No request was set, so this should return 0
		fail_on_test(buf.g_request_fd());
		buf.s_flags(V4L2_BUF_FLAG_REQUEST_FD);
		if (i == 0) {
			buf.s_request_fd(-1);
			// Can't queue to an invalid request fd
			fail_on_test(!buf.qbuf(node));
			buf.s_request_fd(0xdead);
			fail_on_test(!buf.qbuf(node));
		}
		buf.s_request_fd(buf_req_fds[i]);
		if (v4l_type_is_video(buf.g_type()))
			buf.s_field(V4L2_FIELD_ANY);
		if (!(i & 1)) {
			// VIDIOC_PREPARE_BUF is incompatible with requests
			fail_on_test(buf.prepare_buf(node) != EINVAL);
			buf.s_flags(0);
			// Test vivid error injection
			if (node->inject_error(VIVID_CID_BUF_PREPARE_ERROR))
				fail_on_test(buf.prepare_buf(node) != EINVAL);
			fail_on_test(buf.prepare_buf(node));
			fail_on_test(buf.querybuf(node, i));
			// Check that the buffer was prepared
			fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_PREPARED));
			buf.s_flags(buf.g_flags() | V4L2_BUF_FLAG_REQUEST_FD);
			buf.s_request_fd(buf_req_fds[i]);
		}
		// Queue the buffer to the request
		int err = buf.qbuf(node);
		if (!err) {
			// If requests are not supported, this should fail
			fail_on_test(!supports_requests);
			// You can't queue the same buffer again
			fail_on_test(!buf.qbuf(node));
		} else {
			// Can only fail if requests are not supported
			fail_on_test(supports_requests);
			// and should fail with EBADR in that case
			fail_on_test(err != EBADR);
		}
		if (err) {
			// Requests are not supported, so clean up and return
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
		// Check flags and request fd
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
		fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_IN_REQUEST));
		fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD));
		fail_on_test(buf.g_request_fd() < 0);
		// Query the buffer again
		fail_on_test(buf.querybuf(node, i));
		// Check returned flags and request fd
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_DONE);
		fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_IN_REQUEST));
		fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD));
		fail_on_test(buf.g_request_fd() < 0);
		if (i & 1)
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_PREPARED);
		else
			fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_PREPARED));
		// Check that you can't queue it again
		buf.s_request_fd(buf_req_fds[i]);
		fail_on_test(!buf.qbuf(node));

		// Set a control in the request, except for every third request.
		ctrl.value = (i & 1) ? valid_qctrl.maximum : valid_qctrl.minimum;
		ctrls.request_fd = buf_req_fds[i];
		if (i % 3 < 2)
			fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls));
		if (is_vivid) {
			// For vivid, check modifiable array support
			memset(vivid_pixel_array, i, vivid_pixel_array_size);
			vivid_pixel_array_ctrls.which = V4L2_CTRL_WHICH_REQUEST_VAL;
			vivid_pixel_array_ctrls.count = 1;
			vivid_pixel_array_ctrls.controls = &vivid_pixel_array_ctrl;
			vivid_pixel_array_ctrls.request_fd = buf_req_fds[i];
			fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS,
					     &vivid_pixel_array_ctrls));
			fail_on_test(vivid_pixel_array[vivid_pixel_array_size] != 0xff);

			// For vivid, check dynamic array support:
			vivid_dyn_array_ctrl.size = sizeof(vivid_dyn_array);
			vivid_dyn_array_ctrl.p_u32 = vivid_dyn_array;
			memset(vivid_dyn_array, 0xff, sizeof(vivid_dyn_array));
			vivid_dyn_array_ctrls.which = V4L2_CTRL_WHICH_REQUEST_VAL;
			vivid_dyn_array_ctrls.count = 1;
			vivid_dyn_array_ctrls.controls = &vivid_dyn_array_ctrl;
			vivid_dyn_array_ctrls.request_fd = buf_req_fds[i];
			// vivid_dyn_array_ctrl.size is too large, must return ENOSPC
			fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS,
					     &vivid_dyn_array_ctrls) != ENOSPC);
			// and size is set at 100 elems
			fail_on_test(vivid_dyn_array_ctrl.size != 100 * elem_size);
			// Check that the array is not overwritten
			fail_on_test(vivid_dyn_array[0] != 0xffffffff);
			if (i % 3 < 2) {
				unsigned size = (2 + 2 * (i % 8)) * elem_size;

				// Set proper size, varies per request
				vivid_dyn_array_ctrl.size = size;
				memcpy(vivid_dyn_array, vivid_dyn_array_init, size);
				fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS,
						     &vivid_dyn_array_ctrls));
				// check that the size is as expected
				fail_on_test(vivid_dyn_array_ctrl.size != size);
				// and the array values are correctly clamped
				fail_on_test(memcmp(vivid_dyn_array, vivid_dyn_array_clamped, size));
				// and the end of the array is not overwritten
				fail_on_test(vivid_dyn_array[size / elem_size] != 0xffffffff);
			}
		}
		// Re-init the unqueued request
		fail_on_test(doioctl_fd(buf_req_fds[i], MEDIA_REQUEST_IOC_REINIT, nullptr));

		// Make sure that the buffer is no longer in a request
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_IN_REQUEST);
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD);

		// Set the control again
		ctrls.request_fd = buf_req_fds[i];
		if (i % 3 < 2)
			fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls));
		if (is_vivid && i % 3 < 2) {
			// Set the pixel array control again
			memset(vivid_pixel_array, i, vivid_pixel_array_size);
			vivid_pixel_array_ctrls.request_fd = buf_req_fds[i];
			fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS,
					     &vivid_pixel_array_ctrls));
			// Set the dynamic array control again
			vivid_dyn_array_ctrls.request_fd = buf_req_fds[i];
			vivid_dyn_array_ctrl.size = (2 + 2 * (i % 8)) * elem_size;
			memcpy(vivid_dyn_array, vivid_dyn_array_init,
			       sizeof(vivid_dyn_array_init));
			fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS,
					     &vivid_dyn_array_ctrls));
		}

		// After the re-init the buffer is no longer marked for
		// a request. If a request has been queued before (hence
		// the 'if (i)' check), then queuing the buffer without
		// a request must fail since you can't mix the two streamining
		// models.
		if (i)
			fail_on_test(!buf.qbuf(node));
		buf.s_flags(buf.g_flags() | V4L2_BUF_FLAG_REQUEST_FD);
		buf.s_request_fd(buf_req_fds[i]);
		buf.s_field(V4L2_FIELD_ANY);
		// Queue the buffer for the request
		fail_on_test(buf.qbuf(node));
		// Verify that drivers will replace FIELD_ANY for video output queues
		if (v4l_type_is_video(buf.g_type()) && v4l_type_is_output(buf.g_type()))
			fail_on_test(buf.g_field() == V4L2_FIELD_ANY);
		// Query buffer and check that it is marked as being part of a request
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_IN_REQUEST));
		fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD));
		// Use vivid to check buffer prepare or request validation error injection
		if ((i & 1) && node->inject_error(i > num_bufs / 2 ?
						  VIVID_CID_BUF_PREPARE_ERROR :
						  VIVID_CID_REQ_VALIDATE_ERROR))
			fail_on_test(doioctl_fd(buf_req_fds[i],
						MEDIA_REQUEST_IOC_QUEUE, nullptr) != EINVAL);
		// Queue the request
		ret = doioctl_fd(buf_req_fds[i], MEDIA_REQUEST_IOC_QUEUE, nullptr);
		if (node->codec_mask & STATELESS_DECODER) {
			// Stateless decoders might require that certain
			// controls are present in the request. In that
			// case they return ENOENT and we just stop testing
			// since we don't know what those controls are.
			fail_on_test_val(ret != ENOENT, ret);
			test_streaming = false;
			break;
		}
		fail_on_test_val(ret, ret);
		if (i < min_bufs) {
			fail_on_test(buf.querybuf(node, i));
			// Check that the buffer is now queued up (i.e. no longer 'IN_REQUEST')
			fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_IN_REQUEST);
			fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD));
			fail_on_test(!(buf.g_flags() & V4L2_BUF_FLAG_QUEUED));
			// Re-initing or requeuing the request is no longer possible
			fail_on_test(doioctl_fd(buf_req_fds[i], MEDIA_REQUEST_IOC_REINIT, nullptr) != EBUSY);
			fail_on_test(doioctl_fd(buf_req_fds[i], MEDIA_REQUEST_IOC_QUEUE, nullptr) != EBUSY);
		}
		if (i >= min_bufs) {
			// Close some of the request fds to check that this
			// is safe to do
			close(buf_req_fds[i]);
			buf_req_fds[i] = -1;
		}
		if (i == min_bufs - 1) {
			// Check vivid STREAMON error injection
			if (node->inject_error(VIVID_CID_START_STR_ERROR))
				fail_on_test(!node->streamon(q.g_type()));
			fail_on_test(node->streamon(q.g_type()));
		}
	}

	fail_on_test(node->g_fmt(cur_fmt, q.g_type()));

	if (test_streaming) {
		unsigned capture_count;

		// Continue streaming
		// For m2m devices captureBufs() behaves a bit odd: you pass
		// in the total number of output buffers that you want to
		// stream, but since there are already q.g_buffers() output
		// buffers queued up (see previous loop), the captureBufs()
		// function will subtract that from frame_count, so it will
		// only queue frame_count - q.g_buffers() output buffers.
		// In order to ensure we captured at least
		// min_bufs buffers we need to add min_bufs to the frame_count.
		fail_on_test(captureBufs(node, node, q, m2m_q,
					 num_bufs + (node->is_m2m ? min_bufs : 0),
					 POLL_MODE_SELECT, capture_count));
	}
	fail_on_test(node->streamoff(q.g_type()));

	// Note that requests min_bufs...2*min_bufs-1 close their filehandles,
	// so here we just go through the first half of the requests.
	for (unsigned i = 0; test_streaming && i < min_bufs; i++) {
		buffer buf(q);

		// Get the control
		ctrls.request_fd = buf_req_fds[i];
		fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls));
		bool is_max = i & 1;
		// Since the control was not set for every third request,
		// the value will actually be that of the previous request.
		if (i % 3 == 2)
			is_max = !is_max;
		// Check that the value is as expected
		fail_on_test(ctrl.value != (is_max ? valid_qctrl.maximum :
			     valid_qctrl.minimum));
		if (is_vivid) {
			// vivid specific: check that the read-only control
			// of the completed request has the expected value
			// (sequence number & 0xff).
			vivid_ro_ctrls.request_fd = buf_req_fds[i];
			fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &vivid_ro_ctrls));
			// FIXME: due to unreliable sequence counters from vivid this
			// test fails regularly. For now replace the 'warn_once' by
			// 'info_once' until vivid is fixed.
			if (node->is_video && !node->can_output &&
			    vivid_ro_ctrl.value != (int)i)
				info_once("vivid_ro_ctrl.value (%d) != i (%u)\n",
					  vivid_ro_ctrl.value, i);

			// Check that the dynamic control array is set as
			// expected and with the correct values.
			vivid_dyn_array_ctrls.request_fd = buf_req_fds[i];
			memset(vivid_dyn_array, 0xff, sizeof(vivid_dyn_array));
			vivid_dyn_array_ctrl.size = sizeof(vivid_dyn_array);
			fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &vivid_dyn_array_ctrls));
			unsigned size = (2 + 2 * (i % 8)) * elem_size;
			if (i % 3 == 2)
				size = (2 + 2 * ((i - 1) % 8)) * elem_size;
			fail_on_test(vivid_dyn_array_ctrl.size != size);
			fail_on_test(memcmp(vivid_dyn_array, vivid_dyn_array_clamped,
					    vivid_dyn_array_ctrl.size));
			fail_on_test(vivid_dyn_array[size / elem_size] != 0xffffffff);
			// Check that the pixel array control is set as
			// expected and with the correct values.
			vivid_pixel_array_ctrls.request_fd = buf_req_fds[i];
			memset(vivid_pixel_array, 0xfe, vivid_pixel_array_size);
			fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &vivid_pixel_array_ctrls));
			bool ok = true;
			__u8 expected = (i % 3 == 2) ? i - 1 : i;
			for (unsigned j = 0; j < vivid_pixel_array_size; j++)
				if (vivid_pixel_array[j] != expected) {
					ok = false;
					break;
				}
			fail_on_test(!ok);
			fail_on_test(vivid_pixel_array[vivid_pixel_array_size] != 0xff);
		}
		fail_on_test(buf.querybuf(node, i));
		// Check that all the buffers of the stopped stream are
		// no longer marked as belonging to a request.
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD);
		fail_on_test(buf.g_request_fd());
		struct pollfd pfd = {
			buf_req_fds[i],
			POLLPRI, 0
		};
		// Check that polling the request fd will immediately return,
		// indicating that the request has completed.
		fail_on_test(poll(&pfd, 1, 100) != 1);
		// Requeuing the request must fail
		fail_on_test(doioctl_fd(buf_req_fds[i], MEDIA_REQUEST_IOC_QUEUE, nullptr) != EBUSY);
		// But reinit must succeed.
		fail_on_test(doioctl_fd(buf_req_fds[i], MEDIA_REQUEST_IOC_REINIT, nullptr));
		fail_on_test(buf.querybuf(node, i));
		fail_on_test(buf.g_flags() & V4L2_BUF_FLAG_REQUEST_FD);
		fail_on_test(buf.g_request_fd());
		ctrls.request_fd = buf_req_fds[i];
		// Check that getting controls from a reinited request fails
		fail_on_test(!doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls));
		// Close the request fd
		fhs.del(buf_req_fds[i]);
		buf_req_fds[i] = -1;
	}
	// Close any remaining open request fds
	for (unsigned i = 0; i < num_requests; i++)
		if (buf_req_fds[i] >= 0)
			fhs.del(buf_req_fds[i]);

	// Getting the current control value must work
	ctrls.which = 0;
	fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls));
	// Check the final control value
	if (test_streaming) {
		bool is_max = (num_bufs - 1) & 1;
		if ((num_bufs - 1) % 3 == 2)
			is_max = !is_max;
		fail_on_test(ctrl.value != (is_max ? valid_qctrl.maximum :
					    valid_qctrl.minimum));
		if (is_vivid) {
			// For vivid check the final read-only value,
			vivid_ro_ctrls.which = 0;
			fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &vivid_ro_ctrls));
			// FIXME: due to unreliable sequence counters from vivid this
			// test fails regularly. For now replace the 'warn' by 'info'
			// until vivid is fixed.
			if (node->is_video && !node->can_output &&
			    vivid_ro_ctrl.value != (int)(num_bufs - 1))
				info("vivid_ro_ctrl.value (%d) != num_bufs - 1 (%d)\n",
				     vivid_ro_ctrl.value, num_bufs - 1);

			// the final dynamic array value,
			v4l2_query_ext_ctrl q_dyn_array = {
				.id = VIVID_CID_U32_DYN_ARRAY,
			};
			fail_on_test(doioctl(node, VIDIOC_QUERY_EXT_CTRL, &q_dyn_array));
			unsigned elems = 2 + 2 * ((num_bufs - 1) % 8);
			if ((num_bufs - 1) % 3 == 2)
				elems = 2 + 2 * ((num_bufs - 2) % 8);
			fail_on_test(q_dyn_array.elems != elems);
			vivid_dyn_array_ctrls.which = 0;
			fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &vivid_dyn_array_ctrls));
			fail_on_test(vivid_dyn_array_ctrl.size != elems * elem_size);
			fail_on_test(memcmp(vivid_dyn_array, vivid_dyn_array_clamped,
					    vivid_dyn_array_ctrl.size));

			// and the final pixel array value.
			vivid_pixel_array_ctrls.which = 0;
			fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &vivid_pixel_array_ctrls));
			bool ok = true;
			__u8 expected = (num_bufs - 1) % 3 == 2 ? num_bufs - 2 : num_bufs - 1;
			for (unsigned j = 0; j < vivid_pixel_array_size; j++)
				if (vivid_pixel_array[j] != expected) {
					ok = false;
					break;
				}
			fail_on_test(!ok);
		}
	}

	// Cleanup
	fail_on_test(q.reqbufs(node, 0));
	if (node->is_m2m) {
		fail_on_test(node->streamoff(m2m_q.g_type()));
		m2m_q.munmap_bufs(node);
		fail_on_test(m2m_q.reqbufs(node, 0));
	}

	return 0;
}

/* Android does not have support for pthread_cancel */
#ifndef ANDROID

/*
 * This class wraps a pthread in such a way that it simplifies passing
 * parameters, checking completion, gracious halting, and aggressive
 * termination (an empty signal handler, as installed by testBlockingDQBuf,
 * is necessary). This alleviates the need for spaghetti error paths when
 * multiple potentially blocking threads are involved.
 */
class BlockingThread
{
public:
	virtual ~BlockingThread()
	{
		stop();
	}

	int start()
	{
		int ret = pthread_create(&thread, nullptr, startRoutine, this);
		if (ret < 0)
			return ret;

		running = true;
		return 0;
	}

	void stop()
	{
		if (!running)
			return;

		/*
		 * If the thread is blocked on an ioctl, try to wake it up with
		 * a signal.
		 */
		if (!done) {
			pthread_kill(thread, SIGUSR1);
			sleep(1);
		}

		/*
		 * If the signal failed to interrupt the ioctl, use the heavy
		 * artillery and cancel the thread.
		 */
		if (!done) {
			pthread_cancel(thread);
			sleep(1);
		}

		pthread_join(thread, nullptr);
		running = false;
	}

	std::atomic<bool> done{};

private:
	static void *startRoutine(void *arg)
	{
		auto self = static_cast<BlockingThread *>(arg);

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
		pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, nullptr);

		self->run();

		self->done = true;
		return nullptr;
	}

	virtual void run() = 0;

	pthread_t thread;
	std::atomic<bool> running{};
};

class DqbufThread : public BlockingThread
{
public:
	DqbufThread(cv4l_queue *q, struct node *n) : queue(q), node(n) {}

private:
	void run() override
	{
		/*
		 * In this thread we call VIDIOC_DQBUF and wait indefinitely
		 * since no buffers are queued.
		 */
		cv4l_buffer buf(queue->g_type(), V4L2_MEMORY_MMAP);
		node->dqbuf(buf);
	}

	cv4l_queue *queue;
	struct node *node;
};

class StreamoffThread : public BlockingThread
{
public:
	StreamoffThread(cv4l_queue *q, struct node *n) : queue(q), node(n) {}

private:
	void run() override
	{
		/*
		 * In this thread call STREAMOFF; this shouldn't
		 * be blocked by the DQBUF!
		 */
		node->streamoff(queue->g_type());
	}

	cv4l_queue *queue;
	struct node *node;
};

static void pthread_sighandler(int sig)
{
}

static int testBlockingDQBuf(struct node *node, cv4l_queue &q)
{
	DqbufThread thread_dqbuf(&q, node);
	StreamoffThread thread_streamoff(&q, node);

	/*
	 * SIGUSR1 is ignored by default, so install an empty signal handler
	 * so that we can use SIGUSR1 to wake up threads potentially blocked
	 * on ioctls.
	 */
	signal(SIGUSR1, pthread_sighandler);

	fail_on_test(q.reqbufs(node, 2));
	fail_on_test(node->streamon(q.g_type()));

	/*
	 * This test checks if a blocking wait in VIDIOC_DQBUF doesn't block
	 * other ioctls.
	 *
	 * If this fails, check that the vb2_ops wait_prepare/finish callbacks
	 * are set.
	 */
	fflush(stdout);
	thread_dqbuf.start();

	/* Wait for the child thread to start and block */
	sleep(1);
	/* Check that it is really blocking */
	fail_on_test(thread_dqbuf.done);

	fflush(stdout);
	thread_streamoff.start();

	/* Wait for the second child to start and exit */
	sleep(3);
	fail_on_test(!thread_streamoff.done);

	fail_on_test(node->streamoff(q.g_type()));
	fail_on_test(q.reqbufs(node, 0));
	return 0;
}

#endif //ANDROID

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

#ifndef ANDROID
		fail_on_test(testBlockingDQBuf(node, q));
		if (node->is_m2m)
			fail_on_test(testBlockingDQBuf(node, m2m_q));
#endif
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
	return std::any_of(selTests.begin(), selTests.end(), [&](const selTest &selfTest)
		{ return &selfTest != &test; });
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
		sprintf(s_crop, "Crop (%d,%d)/%ux%u, ",
				crop.r.left, crop.r.top,
				crop.r.width, crop.r.height);
	}
	if (has_compose) {
		node->g_frame_selection(compose, fmt.g_field());
		sprintf(s_compose, "Compose (%d,%d)/%ux%u, ",
				compose.r.left, compose.r.top,
				compose.r.width, compose.r.height);
	}
	printf("\r\t\t%s%sStride %u, Field %s%s: %s   \n",
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
		frame_count = f ? static_cast<unsigned>(1.0 / fract2f(f)) : 10;
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
		selTests.push_back(test);

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
		streamFmt(node, pixelformat, w, h, nullptr, frame_count);
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
	printf("\t%s (%s) %ux%u -> %s (%s) %ux%u: %s\n",
	       fcc2s(out_fmt.g_pixelformat()).c_str(),
	       pixfmt2s(out_fmt.g_pixelformat()).c_str(),
	       out_fmt.g_width(), out_fmt.g_height(),
	       fcc2s(cap_fmt.g_pixelformat()).c_str(),
	       pixfmt2s(cap_fmt.g_pixelformat()).c_str(),
	       cap_fmt.g_width(), cap_fmt.g_height(),
	       ok(testMmap(node, node, frame_count, POLL_MODE_SELECT, false)));
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
