#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <dirent.h>
#include <math.h>
#include <config.h>

#include <linux/videodev2.h>
#include <libv4l2.h>
#include <string>

#include "v4l2-ctl.h"

static unsigned stream_count;
static unsigned stream_skip;
static unsigned stream_pat;
static bool stream_loop;
static unsigned reqbufs_count_cap = 3;
static unsigned reqbufs_count_out = 3;
static char *file_cap;
static char *file_out;

void streaming_usage(void)
{
	printf("\nVideo Streaming options:\n"
	       "  --stream-count=<count>\n"
	       "                     stream <count> buffers. The default is to keep streaming\n"
	       "                     forever. This count does not include the number of initial\n"
	       "                     skipped buffers as is passed by --stream-skip.\n"
	       "  --stream-skip=<count>\n"
	       "                     skip the first <count> buffers. The default is 0.\n"
	       "  --stream-to=<file> stream to this file. The default is to discard the\n"
	       "                     data. If <file> is '-', then the data is written to stdout\n"
	       "                     and the --silent option is turned on automatically.\n"
	       "  --stream-poll      use non-blocking mode and select() to stream.\n"
	       "  --stream-mmap=<count>\n"
	       "                     capture video using mmap() [VIDIOC_(D)QBUF]\n"
	       "                     count: the number of buffers to allocate. The default is 3.\n"
	       "  --stream-user=<count>\n"
	       "                     capture video using user pointers [VIDIOC_(D)QBUF]\n"
	       "                     count: the number of buffers to allocate. The default is 3.\n"
	       "  --stream-from=<file> stream from this file. The default is to generate a pattern.\n"
	       "                     If <file> is '-', then the data is read from stdin.\n"
	       "  --stream-loop      loop when the end of the file we are streaming from is reached.\n"
	       "                     The default is to stop.\n"
	       "  --stream-pattern=<count>\n"
	       "                     choose output pattern. The default is 0.\n"
	       "  --stream-out-mmap=<count>\n"
	       "                     output video using mmap() [VIDIOC_(D)QBUF]\n"
	       "                     count: the number of buffers to allocate. The default is 3.\n"
	       "  --stream-out-user=<count>\n"
	       "                     output video using user pointers [VIDIOC_(D)QBUF]\n"
	       "                     count: the number of buffers to allocate. The default is 3.\n"
	       "  --list-buffers     list all video buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-out list all video output buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-vbi list all VBI buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-vbi-out\n"
	       "                     list all VBI output buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-sliced-vbi\n"
	       "                     list all sliced VBI buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-sliced-vbi-out\n"
	       "                     list all sliced VBI output buffers [VIDIOC_QUERYBUF]\n"
	       );
}

static const flag_def flags_def[] = {
	{ V4L2_BUF_FLAG_MAPPED, "mapped" },
	{ V4L2_BUF_FLAG_QUEUED, "queued" },
	{ V4L2_BUF_FLAG_DONE, "done" },
	{ V4L2_BUF_FLAG_KEYFRAME, "keyframe" },
	{ V4L2_BUF_FLAG_PFRAME, "P-frame" },
	{ V4L2_BUF_FLAG_BFRAME, "B-frame" },
	{ V4L2_BUF_FLAG_ERROR, "error" },
	{ V4L2_BUF_FLAG_TIMECODE, "timecode" },
	{ V4L2_BUF_FLAG_PREPARED, "prepared" },
	{ V4L2_BUF_FLAG_NO_CACHE_INVALIDATE, "no-cache-invalidate" },
	{ V4L2_BUF_FLAG_NO_CACHE_CLEAN, "no-cache-clean" },
	{ 0, NULL }
};

static std::string timestamp_type2s(__u32 flags)
{
	char buf[20];

	switch (flags & V4L2_BUF_FLAG_TIMESTAMP_MASK) {
	case V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN:
		return "Unknown";
	case V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC:
		return "Monotonic";
	case V4L2_BUF_FLAG_TIMESTAMP_COPY:
		return "Copy";
	default:
		sprintf(buf, "Type %d", (flags & V4L2_BUF_FLAG_TIMESTAMP_MASK) >> 13);
		return std::string(buf);
	}
}

static const flag_def tc_flags_def[] = {
	{ V4L2_TC_FLAG_DROPFRAME, "dropframe" },
	{ V4L2_TC_FLAG_COLORFRAME, "colorframe" },
	{ V4L2_TC_USERBITS_field, "userbits-field" },
	{ V4L2_TC_USERBITS_USERDEFINED, "userbits-userdefined" },
	{ V4L2_TC_USERBITS_8BITCHARS, "userbits-8bitchars" },
	{ 0, NULL }
};

static void print_buffer(FILE *f, struct v4l2_buffer &buf)
{
	fprintf(f, "\tIndex    : %d\n", buf.index);
	fprintf(f, "\tType     : %s\n", buftype2s(buf.type).c_str());
	fprintf(f, "\tFlags    : %s\n", flags2s(buf.flags, flags_def).c_str());
	fprintf(f, "\tField    : %s\n", field2s(buf.field).c_str());
	fprintf(f, "\tSequence : %u\n", buf.sequence);
	fprintf(f, "\tLength   : %u\n", buf.length);
	fprintf(f, "\tBytesused: %u\n", buf.bytesused);
	fprintf(f, "\tTimestamp: %lu.%06lus (%s)\n", buf.timestamp.tv_sec, buf.timestamp.tv_usec,
			timestamp_type2s(buf.flags).c_str());
	if (buf.flags & V4L2_BUF_FLAG_TIMECODE) {
		static const int fps_types[] = { 0, 24, 25, 30, 50, 60 };
		int fps = buf.timecode.type;

		if (fps > 5)
			fps = 0;
		fprintf(f, "\tTimecode : %dfps %s %dh %dm %ds %df (0x%02x 0x%02x 0x%02x 0x%02x)\n",
			fps_types[fps],
			flags2s(buf.timecode.flags, tc_flags_def).c_str(),
			buf.timecode.hours, buf.timecode.minutes,
			buf.timecode.seconds, buf.timecode.frames,
			buf.timecode.userbits[0], buf.timecode.userbits[1],
			buf.timecode.userbits[2], buf.timecode.userbits[3]);
	}
	if (buf.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
	    buf.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		for (unsigned i = 0; i < buf.length; i++) {
			struct v4l2_plane *p = buf.m.planes + i;

			fprintf(f, "\tPlane    : %d\n", i);
			fprintf(f, "\t\tLength     : %u\n", p->length);
			fprintf(f, "\t\tBytesused  : %u\n", p->bytesused);
			fprintf(f, "\t\tData Offset: %u\n", p->data_offset);
		}
	}
			
	fprintf(f, "\n");
}

static void list_buffers(int fd, unsigned buftype)
{
	int i;

	for (i = 0; i < VIDEO_MAX_FRAME; i++) {
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));
		buf.type = buftype;
		buf.index = i;
		buf.reserved = 0;
		if (buftype == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
		    buftype == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			buf.m.planes = planes;
			buf.length = VIDEO_MAX_PLANES;
			memset(planes, 0, sizeof(planes));
		}
		if (test_ioctl(fd, VIDIOC_QUERYBUF, &buf))
			break;
		if (i == 0)
			printf("VIDIOC_QUERYBUF:\n");
		print_buffer(stdout, buf);
	}
}

void streaming_cmd(int ch, char *optarg)
{
	switch (ch) {
	case OptStreamCount:
		stream_count = strtoul(optarg, 0L, 0);
		break;
	case OptStreamSkip:
		stream_skip = strtoul(optarg, 0L, 0);
		break;
	case OptStreamLoop:
		stream_loop = true;
		break;
	case OptStreamPattern:
		stream_pat = strtoul(optarg, 0L, 0);
		break;
	case OptStreamTo:
		file_cap = optarg;
		if (!strcmp(file_cap, "-"))
			options[OptSilent] = true;
		break;
	case OptStreamFrom:
		file_out = optarg;
		break;
	case OptStreamMmap:
	case OptStreamUser:
		if (optarg) {
			reqbufs_count_cap = strtoul(optarg, 0L, 0);
			if (reqbufs_count_cap == 0)
				reqbufs_count_cap = 3;
		}
		break;
	case OptStreamOutMmap:
	case OptStreamOutUser:
		if (optarg) {
			reqbufs_count_out = strtoul(optarg, 0L, 0);
			if (reqbufs_count_out == 0)
				reqbufs_count_out = 3;
		}
		break;
	}
}

class buffers {
public:
	buffers(bool is_output, bool is_mmap)
	{
		if (is_output) {
			is_mplane = capabilities &
				(V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE);
			type = is_mplane ?
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT;
		} else {
			is_mplane = capabilities &
				(V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE);
			type = is_mplane ?
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
		}
		memory = is_mmap ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
	}

public:
	unsigned type;
	unsigned memory;
	bool is_mplane;
	unsigned bcount;
	unsigned num_planes;
	struct v4l2_plane planes[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];
	void *bufs[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];

	int reqbufs(int fd, unsigned buf_count)
	{
		struct v4l2_requestbuffers reqbufs;
		int err;

		memset(&reqbufs, 0, sizeof(reqbufs));
		reqbufs.count = buf_count;
		reqbufs.type = type;
		reqbufs.memory = memory;
		err = doioctl(fd, VIDIOC_REQBUFS, &reqbufs);
		if (err >= 0)
			bcount = reqbufs.count;
		return err;
	}
};

static bool fill_buffer_from_file(buffers &b, unsigned idx, FILE *fin)
{
	for (unsigned j = 0; j < b.num_planes; j++) {
		void *buf = b.bufs[idx][j];
		struct v4l2_plane &p = b.planes[idx][j];
		unsigned sz = fread(buf, 1, p.length, fin);

		if (j == 0 && sz == 0 && stream_loop) {
			fseek(fin, 0, SEEK_SET);
			sz = fread(buf, 1, p.length, fin);
		}
		if (sz == p.length)
			continue;
		if (sz)
			fprintf(stderr, "%u != %u\n", sz, p.length);
		// Bail out if we get weird buffer sizes.
		return false;
	}
	return true;
}

static void do_setup_cap_buffers(int fd, buffers &b)
{
	for (unsigned i = 0; i < b.bcount; i++) {
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));
		memset(planes, 0, sizeof(planes));
		buf.type = b.type;
		buf.memory = b.memory;
		buf.index = i;
		if (b.is_mplane) {
			buf.m.planes = planes;
			buf.length = VIDEO_MAX_PLANES;
		}
		if (doioctl(fd, VIDIOC_QUERYBUF, &buf))
			return;

		if (b.is_mplane) {
			b.num_planes = buf.length;
			for (unsigned j = 0; j < b.num_planes; j++) {
				struct v4l2_plane &p = b.planes[i][j];

				p.length = planes[j].length;
				if (b.memory == V4L2_MEMORY_MMAP) {
					b.bufs[i][j] = mmap(NULL, p.length,
							  PROT_READ | PROT_WRITE, MAP_SHARED,
							  fd, planes[j].m.mem_offset);

					if (b.bufs[i][j] == MAP_FAILED) {
						fprintf(stderr, "mmap failed\n");
						return;
					}
				}
				else {
					b.bufs[i][j] = calloc(1, p.length);
					planes[j].m.userptr = (unsigned long)b.bufs[i][j];
				}
			}
		}
		else {
			struct v4l2_plane &p = b.planes[i][0];

			b.num_planes = 1;
			p.length = buf.length;
			if (b.memory == V4L2_MEMORY_MMAP) {
				b.bufs[i][0] = mmap(NULL, p.length,
						  PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

				if (b.bufs[i][0] == MAP_FAILED) {
					fprintf(stderr, "mmap failed\n");
					return;
				}
			}
			else {
				b.bufs[i][0] = calloc(1, p.length);
				buf.m.userptr = (unsigned long)b.bufs[i][0];
			}
		}
		if (doioctl(fd, VIDIOC_QBUF, &buf))
			return;
	}
}

static void do_setup_out_buffers(int fd, buffers &b, FILE *fin)
{
	struct v4l2_format fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = b.type;
	doioctl(fd, VIDIOC_G_FMT, &fmt);

	if (!precalculate_bars(fmt.fmt.pix.pixelformat, stream_pat)) {
		fprintf(stderr, "unsupported pixelformat\n");
		return;
	}

	for (unsigned i = 0; i < b.bcount; i++) {
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));
		memset(planes, 0, sizeof(planes));
		buf.type = b.type;
		buf.memory = b.memory;
		buf.index = i;
		if (b.is_mplane) {
			buf.m.planes = planes;
			buf.length = VIDEO_MAX_PLANES;
		}
		if (doioctl(fd, VIDIOC_QUERYBUF, &buf))
			return;

		if (b.is_mplane) {
			b.num_planes = buf.length;
			for (unsigned j = 0; j < b.num_planes; j++) {
				struct v4l2_plane &p = b.planes[i][j];

				p.length = planes[j].length;
				buf.m.planes[j].bytesused = planes[j].length;
				if (b.memory == V4L2_MEMORY_MMAP) {
					b.bufs[i][j] = mmap(NULL, p.length,
							  PROT_READ | PROT_WRITE, MAP_SHARED,
							  fd, planes[j].m.mem_offset);

					if (b.bufs[i][j] == MAP_FAILED) {
						fprintf(stderr, "mmap failed\n");
						return;
					}
				}
				else {
					b.bufs[i][j] = calloc(1, p.length);
					planes[j].m.userptr = (unsigned long)b.bufs[i][j];
				}
			}
			// TODO fill_buffer_mp(bufs[i], &fmt.fmt.pix_mp);
			if (fin)
				fill_buffer_from_file(b, buf.index, fin);
		}
		else {
			b.planes[i][0].length = buf.length;
			buf.bytesused = buf.length;
			if (b.memory == V4L2_MEMORY_MMAP) {
				b.bufs[i][0] = mmap(NULL, buf.length,
						  PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

				if (b.bufs[i][0] == MAP_FAILED) {
					fprintf(stderr, "mmap failed\n");
					return;
				}
			}
			else {
				b.bufs[i][0] = calloc(1, buf.length);
				buf.m.userptr = (unsigned long)b.bufs[i][0];
			}
			if (!fin || !fill_buffer_from_file(b, buf.index, fin))
				fill_buffer(b.bufs[i][0], &fmt.fmt.pix);
		}
		if (doioctl(fd, VIDIOC_QBUF, &buf))
			return;
	}
}

static void do_release_buffers(buffers &b)
{
	for (unsigned i = 0; i < b.bcount; i++) {
		for (unsigned j = 0; j < b.num_planes; j++) {
			if (b.memory == V4L2_MEMORY_MMAP)
				munmap(b.bufs[i][j], b.planes[i][j].length);
			else
				free(b.bufs[i][j]);
		}
	}
}

static int do_handle_cap(int fd, buffers &b, FILE *fout,
			 unsigned &count, unsigned &last, struct timeval &tv_last)
{
	char ch = '<';
	int ret;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buf;
	bool ignore_count_skip = false;

	memset(&buf, 0, sizeof(buf));
	memset(planes, 0, sizeof(planes));

	/*
	 * The stream_count and stream_skip does not apply to capture path of
	 * M2M devices.
	 */
	if ((capabilities & V4L2_CAP_VIDEO_M2M) ||
	    (capabilities & V4L2_CAP_VIDEO_M2M_MPLANE))
		ignore_count_skip = true;

	buf.type = b.type;
	buf.memory = b.memory;
	if (b.is_mplane) {
		buf.m.planes = planes;
		buf.length = VIDEO_MAX_PLANES;
	}

	ret = test_ioctl(fd, VIDIOC_DQBUF, &buf);
	if (ret < 0 && errno == EAGAIN)
		return 0;
	if (ret < 0) {
		fprintf(stderr, "%s: failed: %s\n", "VIDIOC_DQBUF", strerror(errno));
		return -1;
	}
	if (fout && (!stream_skip || ignore_count_skip)) {
		for (unsigned j = 0; j < b.num_planes; j++) {
			unsigned used = b.is_mplane ? planes[j].bytesused : buf.bytesused;
			unsigned offset = b.is_mplane ? planes[j].data_offset : 0;
			unsigned sz;

			if (offset > used) {
				// Should never happen
				fprintf(stderr, "offset %d > used %d!\n",
					offset, used);
				offset = 0;
			}
			used -= offset;
			sz = fwrite((char *)b.bufs[buf.index][j] + offset, 1, used, fout);

			if (sz != used)
				fprintf(stderr, "%u != %u\n", sz, used);
		}
	}
	if (buf.flags & V4L2_BUF_FLAG_KEYFRAME)
		ch = 'K';
	else if (buf.flags & V4L2_BUF_FLAG_PFRAME)
		ch = 'P';
	else if (buf.flags & V4L2_BUF_FLAG_BFRAME)
		ch = 'B';
	if (verbose)
		print_buffer(stderr, buf);
	if (test_ioctl(fd, VIDIOC_QBUF, &buf))
		return -1;

	if (!verbose) {
		fprintf(stderr, "%c", ch);
		fflush(stderr);
	}

	if (count == 0) {
		gettimeofday(&tv_last, NULL);
	} else {
		struct timeval tv_cur, res;

		gettimeofday(&tv_cur, NULL);
		timersub(&tv_cur, &tv_last, &res);
		if (res.tv_sec) {
			unsigned fps = (100 * (count - last)) /
				(res.tv_sec * 100 + res.tv_usec / 10000);
			last = count;
			tv_last = tv_cur;
			fprintf(stderr, " %d fps\n", fps);
		}
	}
	count++;

	if (ignore_count_skip)
		return 0;

	if (stream_skip) {
		stream_skip--;
		return 0;
	}
	if (stream_count == 0)
		return 0;
	if (--stream_count == 0)
		return -1;

	return 0;
}

static int do_handle_out(int fd, buffers &b, FILE *fin,
			 unsigned &count, unsigned &last, struct timeval &tv_last)
{
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buf;
	int ret;

	memset(&buf, 0, sizeof(buf));
	memset(planes, 0, sizeof(planes));
	buf.type = b.type;
	buf.memory = b.memory;
	if (b.is_mplane) {
		buf.m.planes = planes;
		buf.length = VIDEO_MAX_PLANES;
	}

	ret = test_ioctl(fd, VIDIOC_DQBUF, &buf);
	if (ret < 0 && errno == EAGAIN)
		return 0;
	if (ret < 0) {
		fprintf(stderr, "%s: failed: %s\n", "VIDIOC_DQBUF", strerror(errno));
		return -1;
	}
	if (fin && !fill_buffer_from_file(b, buf.index, fin))
		return -1;
	if (b.is_mplane) {
		for (unsigned j = 0; j < buf.length; j++)
			buf.m.planes[j].bytesused = buf.m.planes[j].length;
	} else {
		buf.bytesused = buf.length;
	}
	if (test_ioctl(fd, VIDIOC_QBUF, &buf))
		return -1;

	fprintf(stderr, ">");
	fflush(stderr);

	if (count == 0) {
		gettimeofday(&tv_last, NULL);
	} else {
		struct timeval tv_cur, res;

		gettimeofday(&tv_cur, NULL);
		timersub(&tv_cur, &tv_last, &res);
		if (res.tv_sec) {
			unsigned fps = (100 * (count - last)) /
				(res.tv_sec * 100 + res.tv_usec / 10000);
			last = count;
			tv_last = tv_cur;
			fprintf(stderr, " %d fps\n", fps);
		}
	}
	count++;
	if (stream_count == 0)
		return 0;
	if (--stream_count == 0)
		return -1;

	return 0;
}

static void streaming_set_cap(int fd)
{
	struct v4l2_event_subscription sub;
	int fd_flags = fcntl(fd, F_GETFL);
	buffers b(false, options[OptStreamMmap]);
	bool use_poll = options[OptStreamPoll];
	FILE *fout = NULL;

	if (!(capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
	    !(capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
		fprintf(stderr, "unsupported stream type\n");
		return;
	}

	memset(&sub, 0, sizeof(sub));
	sub.type = V4L2_EVENT_EOS;
	ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);

	if (file_cap) {
		if (!strcmp(file_cap, "-"))
			fout = stdout;
		else
			fout = fopen(file_cap, "w+");
	}

	if (b.reqbufs(fd, reqbufs_count_cap))
		return;

	do_setup_cap_buffers(fd, b);

	if (doioctl(fd, VIDIOC_STREAMON, &b.type))
		return;

	if (use_poll)
		fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK);

	unsigned count = 0, last = 0;
	struct timeval tv_last;
	bool eos = false;

	while (!eos) {
		fd_set read_fds;
		fd_set exception_fds;
		struct timeval tv = { use_poll ? 2 : 0, 0 };
		int r;

		FD_ZERO(&exception_fds);
		FD_SET(fd, &exception_fds);
		FD_ZERO(&read_fds);
		FD_SET(fd, &read_fds);
		r = select(fd + 1, use_poll ? &read_fds : NULL, NULL, &exception_fds, &tv);

		if (r == -1) {
			if (EINTR == errno)
				continue;
			fprintf(stderr, "select error: %s\n",
					strerror(errno));
			return;
		}
		if (use_poll && r == 0) {
			fprintf(stderr, "select timeout\n");
			return;
		}

		if (FD_ISSET(fd, &exception_fds)) {
			struct v4l2_event ev;

			while (!ioctl(fd, VIDIOC_DQEVENT, &ev)) {
				if (ev.type != V4L2_EVENT_EOS)
					continue;
				eos = true;
				break;
			}
		}

		if (FD_ISSET(fd, &read_fds)) {
			r  = do_handle_cap(fd, b, fout,
					   count, last, tv_last);
			if (r == -1)
				break;
		}

	}
	doioctl(fd, VIDIOC_STREAMOFF, &b.type);
	fcntl(fd, F_SETFL, fd_flags);
	fprintf(stderr, "\n");

	do_release_buffers(b);

	if (fout && fout != stdout)
		fclose(fout);
}

static void streaming_set_out(int fd)
{
	buffers b(true, options[OptStreamOutMmap]);
	int fd_flags = fcntl(fd, F_GETFL);
	bool use_poll = options[OptStreamPoll];
	FILE *fin = NULL;

	if (!(capabilities & V4L2_CAP_VIDEO_OUTPUT) &&
	    !(capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE)) {
		fprintf(stderr, "unsupported stream type\n");
		return;
	}

	if (file_out) {
		if (!strcmp(file_out, "-"))
			fin = stdin;
		else
			fin = fopen(file_out, "r");
	}

	if (b.reqbufs(fd, reqbufs_count_out))
		return;

	do_setup_out_buffers(fd, b, fin);

	if (doioctl(fd, VIDIOC_STREAMON, &b.type))
		return;

	if (use_poll)
		fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK);

	unsigned count = 0, last = 0;
	struct timeval tv_last;

	for (;;) {
		int r;

		if (use_poll) {
			fd_set fds;
			struct timeval tv;

			FD_ZERO(&fds);
			FD_SET(fd, &fds);

			/* Timeout. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(fd + 1, NULL, &fds, NULL, &tv);

			if (r == -1) {
				if (EINTR == errno)
					continue;
				fprintf(stderr, "select error: %s\n",
					strerror(errno));
				return;
			}

			if (r == 0) {
				fprintf(stderr, "select timeout\n");
				return;
			}
		}
		r = do_handle_out(fd, b, fin,
				   count, last, tv_last);
		if (r == -1)
			break;

	}

	if (options[OptDecoderCmd]) {
		doioctl(fd, VIDIOC_DECODER_CMD, &dec_cmd);
		options[OptDecoderCmd] = false;
	}
	doioctl(fd, VIDIOC_STREAMOFF, &b.type);
	fcntl(fd, F_SETFL, fd_flags);
	fprintf(stderr, "\n");

	do_release_buffers(b);

	if (fin && fin != stdin)
		fclose(fin);
}

enum stream_type {
	CAP,
	OUT,
};

static void streaming_set_m2m(int fd)
{
	int fd_flags = fcntl(fd, F_GETFL);
	bool use_poll = options[OptStreamPoll];
	buffers in(false, options[OptStreamMmap]);
	buffers out(true, options[OptStreamOutMmap]);
	FILE *file[2] = {NULL, NULL};

	if (!(capabilities & V4L2_CAP_VIDEO_M2M) &&
	    !(capabilities & V4L2_CAP_VIDEO_M2M_MPLANE)) {
		fprintf(stderr, "unsupported stream type\n");
		return;
	}

	struct v4l2_event_subscription sub;

	memset(&sub, 0, sizeof(sub));
	sub.type = V4L2_EVENT_EOS;
	ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);

	if (file_cap) {
		if (!strcmp(file_cap, "-"))
			file[CAP] = stdout;
		else
			file[CAP] = fopen(file_cap, "w+");
	}

	if (file_out) {
		if (!strcmp(file_out, "-"))
			file[OUT] = stdin;
		else
			file[OUT] = fopen(file_out, "r");
	}

	if (in.reqbufs(fd, reqbufs_count_cap) ||
	    out.reqbufs(fd, reqbufs_count_out))
		return;

	do_setup_cap_buffers(fd, in);
	do_setup_out_buffers(fd, out, file[OUT]);

	if (doioctl(fd, VIDIOC_STREAMON, &in.type) ||
	    doioctl(fd, VIDIOC_STREAMON, &out.type))
		return;

	if (use_poll)
		fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK);

	unsigned count[2] = { 0, 0 };
	unsigned last[2] = { 0, 0 };
	struct timeval tv_last[2];

	fd_set fds[3];
	fd_set *rd_fds = &fds[0]; /* for capture */
	fd_set *ex_fds = &fds[1]; /* for capture */
	fd_set *wr_fds = &fds[2]; /* for output */

	while (rd_fds || wr_fds || ex_fds) {
		struct timeval tv = { use_poll ? 2 : 0, 0 };
		int r = 0;

		if (rd_fds) {
			FD_ZERO(rd_fds);
			FD_SET(fd, rd_fds);
		}

		if (ex_fds) {
			FD_ZERO(ex_fds);
			FD_SET(fd, ex_fds);
		}

		if (wr_fds) {
			FD_ZERO(wr_fds);
			FD_SET(fd, wr_fds);
		}

		if (use_poll || ex_fds)
			r = select(fd + 1, use_poll ? rd_fds : NULL,
					   use_poll ? wr_fds : NULL, ex_fds, &tv);

		if (r == -1) {
			if (EINTR == errno)
				continue;
			fprintf(stderr, "select error: %s\n",
					strerror(errno));
			return;
		}
		if (use_poll && r == 0) {
			fprintf(stderr, "select timeout\n");
			return;
		}

		if (rd_fds && FD_ISSET(fd, rd_fds)) {
			r  = do_handle_cap(fd, in, file[CAP],
					   count[CAP], last[CAP], tv_last[CAP]);
			if (r < 0) {
				rd_fds = NULL;
				ex_fds = NULL;
				doioctl(fd, VIDIOC_STREAMOFF, &in.type);
			}
		}

		if (wr_fds && FD_ISSET(fd, wr_fds)) {
			r  = do_handle_out(fd, out, file[OUT],
					   count[OUT], last[OUT], tv_last[OUT]);
			if (r < 0)  {
				wr_fds = NULL;

				if (options[OptDecoderCmd]) {
					doioctl(fd, VIDIOC_DECODER_CMD, &dec_cmd);
					options[OptDecoderCmd] = false;
				}

				doioctl(fd, VIDIOC_STREAMOFF, &out.type);
			}
		}

		if (ex_fds && FD_ISSET(fd, ex_fds)) {
			struct v4l2_event ev;

			while (!ioctl(fd, VIDIOC_DQEVENT, &ev)) {

				if (ev.type != V4L2_EVENT_EOS)
					continue;

				rd_fds = NULL;
				ex_fds = NULL;
				doioctl(fd, VIDIOC_STREAMOFF, &in.type);
				break;
			}
		}
	}

	fcntl(fd, F_SETFL, fd_flags);
	fprintf(stderr, "\n");

	do_release_buffers(in);
	do_release_buffers(out);

	if (file[CAP] && file[CAP] != stdout)
		fclose(file[CAP]);

	if (file[OUT] && file[OUT] != stdin)
		fclose(file[OUT]);
}

void streaming_set(int fd)
{
	bool do_cap = options[OptStreamMmap] || options[OptStreamUser];
	bool do_out = options[OptStreamOutMmap] || options[OptStreamOutUser];

	if (do_cap && do_out)
		streaming_set_m2m(fd);
	else if (do_cap)
		streaming_set_cap(fd);
	else if (do_out)
		streaming_set_out(fd);
}

void streaming_list(int fd)
{
	if (options[OptListBuffers]) {
		bool is_mplane = capabilities &
			(V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			 V4L2_CAP_VIDEO_M2M_MPLANE);
		
		list_buffers(fd, is_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
					     V4L2_BUF_TYPE_VIDEO_CAPTURE);
	}

	if (options[OptListBuffersOut]) {
		bool is_mplane = capabilities &
			(V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			 V4L2_CAP_VIDEO_M2M_MPLANE);
		
		list_buffers(fd, is_mplane ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE :
					     V4L2_BUF_TYPE_VIDEO_OUTPUT);
	}

	if (options[OptListBuffersVbi]) {
		list_buffers(fd, V4L2_BUF_TYPE_VBI_CAPTURE);
	}

	if (options[OptListBuffersSlicedVbi]) {
		list_buffers(fd, V4L2_BUF_TYPE_SLICED_VBI_CAPTURE);
	}

	if (options[OptListBuffersVbiOut]) {
		list_buffers(fd, V4L2_BUF_TYPE_VBI_OUTPUT);
	}

	if (options[OptListBuffersSlicedVbiOut]) {
		list_buffers(fd, V4L2_BUF_TYPE_SLICED_VBI_OUTPUT);
	}
}
