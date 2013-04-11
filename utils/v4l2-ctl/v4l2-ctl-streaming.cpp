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

#define NUM_PATTERNS (4)

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
		stream_pat %= NUM_PATTERNS;
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

/* Note: the code to create the test patterns is derived from the vivi.c kernel
   driver. */

/* Bars and Colors should match positions */

enum colors {
	WHITE,
	AMBER,
	CYAN,
	GREEN,
	MAGENTA,
	RED,
	BLUE,
	BLACK,
	TEXT_BLACK,
};

/* R   G   B */
#define COLOR_WHITE	{204, 204, 204}
#define COLOR_AMBER	{208, 208,   0}
#define COLOR_CYAN	{  0, 206, 206}
#define	COLOR_GREEN	{  0, 239,   0}
#define COLOR_MAGENTA	{239,   0, 239}
#define COLOR_RED	{205,   0,   0}
#define COLOR_BLUE	{  0,   0, 205}
#define COLOR_BLACK	{  0,   0,   0}

struct bar_std {
	__u8 bar[9][3];
};

static struct bar_std bars[NUM_PATTERNS] = {
	{	/* Standard ITU-R color bar sequence */
		{ COLOR_WHITE, COLOR_AMBER, COLOR_CYAN, COLOR_GREEN,
		  COLOR_MAGENTA, COLOR_RED, COLOR_BLUE, COLOR_BLACK, COLOR_BLACK }
	}, {
		{ COLOR_WHITE, COLOR_AMBER, COLOR_BLACK, COLOR_WHITE,
		  COLOR_AMBER, COLOR_BLACK, COLOR_WHITE, COLOR_AMBER, COLOR_BLACK }
	}, {
		{ COLOR_WHITE, COLOR_CYAN, COLOR_BLACK, COLOR_WHITE,
		  COLOR_CYAN, COLOR_BLACK, COLOR_WHITE, COLOR_CYAN, COLOR_BLACK }
	}, {
		{ COLOR_WHITE, COLOR_GREEN, COLOR_BLACK, COLOR_WHITE,
		  COLOR_GREEN, COLOR_BLACK, COLOR_WHITE, COLOR_GREEN, COLOR_BLACK }
	},
};

#define TO_Y(r, g, b) \
	(((16829 * r + 33039 * g + 6416 * b  + 32768) >> 16) + 16)
/* RGB to  V(Cr) Color transform */
#define TO_V(r, g, b) \
	(((28784 * r - 24103 * g - 4681 * b  + 32768) >> 16) + 128)
/* RGB to  U(Cb) Color transform */
#define TO_U(r, g, b) \
	(((-9714 * r - 19070 * g + 28784 * b + 32768) >> 16) + 128)

static __u8 calc_bars[9][3];
static int pixel_size;

/* precalculate color bar values to speed up rendering */
static bool precalculate_bars(__u32 pixfmt, int pattern)
{
	__u8 r, g, b;
	int k, is_yuv;

	pixel_size = 2;
	for (k = 0; k < 9; k++) {
		r = bars[pattern].bar[k][0];
		g = bars[pattern].bar[k][1];
		b = bars[pattern].bar[k][2];
		is_yuv = 0;

		switch (pixfmt) {
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_YVYU:
		case V4L2_PIX_FMT_VYUY:
			is_yuv = 1;
			break;
		case V4L2_PIX_FMT_RGB565:
		case V4L2_PIX_FMT_RGB565X:
			r >>= 3;
			g >>= 2;
			b >>= 3;
			break;
		case V4L2_PIX_FMT_RGB555:
		case V4L2_PIX_FMT_RGB555X:
			r >>= 3;
			g >>= 3;
			b >>= 3;
			break;
		case V4L2_PIX_FMT_RGB24:
		case V4L2_PIX_FMT_BGR24:
			pixel_size = 3;
			break;
		case V4L2_PIX_FMT_RGB32:
		case V4L2_PIX_FMT_BGR32:
			pixel_size = 4;
			break;
		default:
			return false;
		}

		if (is_yuv) {
			calc_bars[k][0] = TO_Y(r, g, b);	/* Luma */
			calc_bars[k][1] = TO_U(r, g, b);	/* Cb */
			calc_bars[k][2] = TO_V(r, g, b);	/* Cr */
		} else {
			calc_bars[k][0] = r;
			calc_bars[k][1] = g;
			calc_bars[k][2] = b;
		}
	}
	return true;
}

/* 'odd' is true for pixels 1, 3, 5, etc. and false for pixels 0, 2, 4, etc. */
static void gen_twopix(__u8 *buf, __u32 pixfmt, int colorpos, bool odd)
{
	__u8 r_y, g_u, b_v;
	int color;

	r_y = calc_bars[colorpos][0]; /* R or precalculated Y */
	g_u = calc_bars[colorpos][1]; /* G or precalculated U */
	b_v = calc_bars[colorpos][2]; /* B or precalculated V */

	for (color = 0; color < pixel_size; color++) {
		__u8 *p = buf + color;

		switch (pixfmt) {
		case V4L2_PIX_FMT_YUYV:
			switch (color) {
			case 0:
				*p = r_y;
				break;
			case 1:
				*p = odd ? b_v : g_u;
				break;
			}
			break;
		case V4L2_PIX_FMT_UYVY:
			switch (color) {
			case 0:
				*p = odd ? b_v : g_u;
				break;
			case 1:
				*p = r_y;
				break;
			}
			break;
		case V4L2_PIX_FMT_YVYU:
			switch (color) {
			case 0:
				*p = r_y;
				break;
			case 1:
				*p = odd ? g_u : b_v;
				break;
			}
			break;
		case V4L2_PIX_FMT_VYUY:
			switch (color) {
			case 0:
				*p = odd ? g_u : b_v;
				break;
			case 1:
				*p = r_y;
				break;
			}
			break;
		case V4L2_PIX_FMT_RGB565:
			switch (color) {
			case 0:
				*p = (g_u << 5) | b_v;
				break;
			case 1:
				*p = (r_y << 3) | (g_u >> 3);
				break;
			}
			break;
		case V4L2_PIX_FMT_RGB565X:
			switch (color) {
			case 0:
				*p = (r_y << 3) | (g_u >> 3);
				break;
			case 1:
				*p = (g_u << 5) | b_v;
				break;
			}
			break;
		case V4L2_PIX_FMT_RGB555:
			switch (color) {
			case 0:
				*p = (g_u << 5) | b_v;
				break;
			case 1:
				*p = (r_y << 2) | (g_u >> 3);
				break;
			}
			break;
		case V4L2_PIX_FMT_RGB555X:
			switch (color) {
			case 0:
				*p = (r_y << 2) | (g_u >> 3);
				break;
			case 1:
				*p = (g_u << 5) | b_v;
				break;
			}
			break;
		case V4L2_PIX_FMT_RGB24:
			switch (color) {
			case 0:
				*p = r_y;
				break;
			case 1:
				*p = g_u;
				break;
			case 2:
				*p = b_v;
				break;
			}
			break;
		case V4L2_PIX_FMT_BGR24:
			switch (color) {
			case 0:
				*p = b_v;
				break;
			case 1:
				*p = g_u;
				break;
			case 2:
				*p = r_y;
				break;
			}
			break;
		case V4L2_PIX_FMT_RGB32:
			switch (color) {
			case 0:
				*p = 0;
				break;
			case 1:
				*p = r_y;
				break;
			case 2:
				*p = g_u;
				break;
			case 3:
				*p = b_v;
				break;
			}
			break;
		case V4L2_PIX_FMT_BGR32:
			switch (color) {
			case 0:
				*p = b_v;
				break;
			case 1:
				*p = g_u;
				break;
			case 2:
				*p = r_y;
				break;
			case 3:
				*p = 0;
				break;
			}
			break;
		}
	}
}

static void fill_buffer(void *buffer, struct v4l2_pix_format *pix)
{
	for (unsigned y = 0; y < pix->height; y++) {
		__u8 *ptr = (__u8 *)buffer + y * pix->bytesperline;

		for (unsigned x = 0; x < pix->width; x++) {
			int colorpos = x / (pix->width / 8) % 8;

			gen_twopix(ptr + x * pixel_size, pix->pixelformat, colorpos, x & 1);
		}
	}
}

static bool fill_buffer_from_file(void *buffers[], unsigned buffer_lengths[],
		unsigned buf_index, unsigned num_planes, FILE *fin)
{
	for (unsigned j = 0; j < num_planes; j++) {
		unsigned p = buf_index * num_planes + j;
		unsigned sz = fread(buffers[p], 1,
				buffer_lengths[p], fin);

		if (j == 0 && sz == 0 && stream_loop) {
			fseek(fin, 0, SEEK_SET);
			sz = fread(buffers[p], 1,
					buffer_lengths[p], fin);
		}
		if (sz == buffer_lengths[p])
			continue;
		if (sz)
			fprintf(stderr, "%u != %u\n", sz, buffer_lengths[p]);
		// Bail out if we get weird buffer sizes.
		return false;
	}
	return true;
}

static void do_setup_cap_buffers(int fd, struct v4l2_requestbuffers *reqbufs,
				 bool is_mplane, unsigned &num_planes, bool is_mmap,
				 void *buffers[], unsigned buffer_lengths[])
{
	for (unsigned i = 0; i < reqbufs->count; i++) {
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));
		memset(planes, 0, sizeof(planes));
		buf.type = reqbufs->type;
		buf.memory = reqbufs->memory;
		buf.index = i;
		if (is_mplane) {
			buf.m.planes = planes;
			buf.length = VIDEO_MAX_PLANES;
		}
		if (doioctl(fd, VIDIOC_QUERYBUF, &buf))
			return;

		if (is_mplane) {
			num_planes = buf.length;
			for (unsigned j = 0; j < num_planes; j++) {
				unsigned p = i * num_planes + j;

				buffer_lengths[p] = planes[j].length;
				if (is_mmap) {
					buffers[p] = mmap(NULL, planes[j].length,
							  PROT_READ | PROT_WRITE, MAP_SHARED,
							  fd, planes[j].m.mem_offset);

					if (buffers[p] == MAP_FAILED) {
						fprintf(stderr, "mmap failed\n");
						return;
					}
				}
				else {
					buffers[p] = calloc(1, planes[j].length);
					planes[j].m.userptr = (unsigned long)buffers[p];
				}
			}
		}
		else {
			buffer_lengths[i] = buf.length;
			if (is_mmap) {
				buffers[i] = mmap(NULL, buf.length,
						  PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

				if (buffers[i] == MAP_FAILED) {
					fprintf(stderr, "mmap failed\n");
					return;
				}
			}
			else {
				buffers[i] = calloc(1, buf.length);
				buf.m.userptr = (unsigned long)buffers[i];
			}
		}
		if (doioctl(fd, VIDIOC_QBUF, &buf))
			return;
	}
}

static void do_setup_out_buffers(int fd, struct v4l2_requestbuffers *reqbufs,
				 bool is_mplane, unsigned &num_planes, bool is_mmap,
				 void *buffers[], unsigned buffer_lengths[], FILE *fin)
{
	struct v4l2_format fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = reqbufs->type;
	doioctl(fd, VIDIOC_G_FMT, &fmt);

	if (!precalculate_bars(fmt.fmt.pix.pixelformat, stream_pat % NUM_PATTERNS)) {
		fprintf(stderr, "unsupported pixelformat\n");
		return;
	}

	for (unsigned i = 0; i < reqbufs->count; i++) {
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));
		memset(planes, 0, sizeof(planes));
		buf.type = reqbufs->type;
		buf.memory = reqbufs->memory;
		buf.index = i;
		if (is_mplane) {
			buf.m.planes = planes;
			buf.length = VIDEO_MAX_PLANES;
		}
		if (doioctl(fd, VIDIOC_QUERYBUF, &buf))
			return;

		if (is_mplane) {
			num_planes = buf.length;
			for (unsigned j = 0; j < num_planes; j++) {
				unsigned p = i * num_planes + j;

				buffer_lengths[p] = planes[j].length;
				buf.m.planes[j].bytesused = planes[j].length;
				if (is_mmap) {
					buffers[p] = mmap(NULL, planes[j].length,
							  PROT_READ | PROT_WRITE, MAP_SHARED,
							  fd, planes[j].m.mem_offset);

					if (buffers[p] == MAP_FAILED) {
						fprintf(stderr, "mmap failed\n");
						return;
					}
				}
				else {
					buffers[p] = calloc(1, planes[j].length);
					planes[j].m.userptr = (unsigned long)buffers[p];
				}
			}
			// TODO fill_buffer_mp(buffers[i], &fmt.fmt.pix_mp);
			if (fin)
				fill_buffer_from_file(buffers, buffer_lengths,
						      buf.index, num_planes, fin);
		}
		else {
			buffer_lengths[i] = buf.length;
			buf.bytesused = buf.length;
			if (is_mmap) {
				buffers[i] = mmap(NULL, buf.length,
						  PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

				if (buffers[i] == MAP_FAILED) {
					fprintf(stderr, "mmap failed\n");
					return;
				}
			}
			else {
				buffers[i] = calloc(1, buf.length);
				buf.m.userptr = (unsigned long)buffers[i];
			}
			if (!fin || !fill_buffer_from_file(buffers, buffer_lengths,
							   buf.index, num_planes, fin))
				fill_buffer(buffers[i], &fmt.fmt.pix);
		}
		if (doioctl(fd, VIDIOC_QBUF, &buf))
			return;
	}
}

static void do_release_buffers(struct v4l2_requestbuffers *reqbufs,
			       unsigned num_planes, bool is_mmap,
			       void *buffers[], unsigned buffer_lengths[])
{
	for (unsigned i = 0; i < reqbufs->count; i++) {
		for (unsigned j = 0; j < num_planes; j++) {
			unsigned p = i * num_planes + j;

			if (is_mmap)
				munmap(buffers[p], buffer_lengths[p]);
			else
				free(buffers[p]);
		}
	}
}

static int do_handle_cap(int fd, struct v4l2_requestbuffers *reqbufs,
			 bool is_mplane, unsigned num_planes,
			 void *buffers[], unsigned buffer_lengths[], FILE *fout,
			 unsigned &count, unsigned &last, struct timeval &tv_last)
{
	char ch = '+';
	int ret;

	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buf;
	memset(&buf, 0, sizeof(buf));
	memset(planes, 0, sizeof(planes));

	buf.type = reqbufs->type;
	buf.memory = reqbufs->memory;
	if (is_mplane) {
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
	if (fout && !stream_skip) {
		for (unsigned j = 0; j < num_planes; j++) {
			unsigned p = buf.index * num_planes + j;
			unsigned used = is_mplane ? planes[j].bytesused : buf.bytesused;
			unsigned offset = is_mplane ? planes[j].data_offset : 0;
			unsigned sz;

			if (offset > used) {
				// Should never happen
				fprintf(stderr, "offset %d > used %d!\n",
					offset, used);
				offset = 0;
			}
			used -= offset;
			sz = fwrite((char *)buffers[p] + offset, 1, used, fout);

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

	/*
	 * The stream_count and stream_skip does not apply to capture path of
	 * M2M devices.  In that case, we skip them with an early return.
	 */
	if ((capabilities & V4L2_CAP_VIDEO_M2M) ||
	    (capabilities & V4L2_CAP_VIDEO_M2M_MPLANE))
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

static int do_handle_out(int fd, struct v4l2_requestbuffers *reqbufs,
			 bool is_mplane, unsigned num_planes,
			 void *buffers[], unsigned buffer_lengths[], FILE *fin,
			 unsigned &count, unsigned &last, struct timeval &tv_last)
{
	int ret;

	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buf;

	memset(&buf, 0, sizeof(buf));
	memset(planes, 0, sizeof(planes));
	buf.type = reqbufs->type;
	buf.memory = reqbufs->memory;
	if (is_mplane) {
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
	if (fin && !fill_buffer_from_file(buffers, buffer_lengths,
					  buf.index, num_planes, fin))
		return -1;
	if (is_mplane) {
		for (unsigned j = 0; j < buf.length; j++)
			buf.m.planes[j].bytesused = buf.m.planes[j].length;
	} else {
		buf.bytesused = buf.length;
	}
	if (test_ioctl(fd, VIDIOC_QBUF, &buf))
		return -1;

	fprintf(stderr, "-");
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

void streaming_set_cap(int fd)
{
	struct v4l2_requestbuffers reqbufs;
	struct v4l2_event_subscription sub;
	int fd_flags = fcntl(fd, F_GETFL);
	bool is_mplane = capabilities &
			(V4L2_CAP_VIDEO_CAPTURE_MPLANE |
				 V4L2_CAP_VIDEO_M2M_MPLANE);
	bool is_mmap = options[OptStreamMmap];
	bool use_poll = options[OptStreamPoll];
	__u32 type = is_mplane ?
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
	FILE *fout = NULL;
	unsigned num_planes = 1;

	if (!(capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
	    !(capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
		fprintf(stderr, "unsupported stream type\n");
		return;
	}

	memset(&reqbufs, 0, sizeof(reqbufs));
	reqbufs.count = reqbufs_count_cap;
	reqbufs.type = type;
	reqbufs.memory = is_mmap ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
	memset(&sub, 0, sizeof(sub));
	sub.type = V4L2_EVENT_EOS;
	ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);

	if (file_cap) {
		if (!strcmp(file_cap, "-"))
			fout = stdout;
		else
			fout = fopen(file_cap, "w+");
	}

	if (doioctl(fd, VIDIOC_REQBUFS, &reqbufs))
		return;

	void *buffers[reqbufs.count * VIDEO_MAX_PLANES];
	unsigned buffer_lengths[reqbufs.count * VIDEO_MAX_PLANES];

	do_setup_cap_buffers(fd, &reqbufs, is_mplane, num_planes,
			     is_mmap, buffers, buffer_lengths);

	type = reqbufs.type;
	if (doioctl(fd, VIDIOC_STREAMON, &type))
		return;

	if (use_poll)
		fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK);

	unsigned count = 0, last = 0;
	struct timeval tv_last;
	bool eos = false;

	while (!eos) {
		fd_set read_fds;
		fd_set exception_fds;
		int r;

		if (use_poll) {
			struct timeval tv;

			FD_ZERO(&read_fds);
			FD_SET(fd, &read_fds);
			FD_ZERO(&exception_fds);
			FD_SET(fd, &exception_fds);

			/* Timeout. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(fd + 1, &read_fds, NULL, &exception_fds, &tv);

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
			r  = do_handle_cap(fd, &reqbufs, is_mplane, num_planes,
					   buffers, buffer_lengths, fout,
					   count, last, tv_last);
			if (r == -1)
				break;
		}

	}
	doioctl(fd, VIDIOC_STREAMOFF, &type);
	fcntl(fd, F_SETFL, fd_flags);
	fprintf(stderr, "\n");

	do_release_buffers(&reqbufs, num_planes, is_mmap, buffers,
			   buffer_lengths);

	if (fout && fout != stdout)
		fclose(fout);
}

void streaming_set_out(int fd)
{
	struct v4l2_requestbuffers reqbufs;
	int fd_flags = fcntl(fd, F_GETFL);
	bool is_mplane = capabilities &
			(V4L2_CAP_VIDEO_OUTPUT_MPLANE |
				 V4L2_CAP_VIDEO_M2M_MPLANE);
	bool is_mmap = options[OptStreamOutMmap];
	bool use_poll = options[OptStreamPoll];
	__u32 type = is_mplane ?
		V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT;
	FILE *fin = NULL;
	unsigned num_planes = 1;

	if (!(capabilities & V4L2_CAP_VIDEO_OUTPUT) &&
	    !(capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE)) {
		fprintf(stderr, "unsupported stream type\n");
		return;
	}

	memset(&reqbufs, 0, sizeof(reqbufs));
	reqbufs.count = reqbufs_count_out;
	reqbufs.type = type;
	reqbufs.memory = is_mmap ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;

	if (file_out) {
		if (!strcmp(file_out, "-"))
			fin = stdin;
		else
			fin = fopen(file_out, "r");
	}

	if (doioctl(fd, VIDIOC_REQBUFS, &reqbufs))
		return;

	void *buffers[reqbufs.count * VIDEO_MAX_PLANES];
	unsigned buffer_lengths[reqbufs.count * VIDEO_MAX_PLANES];

	do_setup_out_buffers(fd, &reqbufs, is_mplane, num_planes,
			     is_mmap, buffers, buffer_lengths, fin);

	type = reqbufs.type;
	if (doioctl(fd, VIDIOC_STREAMON, &type))
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
		r  = do_handle_out(fd, &reqbufs, is_mplane, num_planes,
				   buffers, buffer_lengths, fin,
				   count, last, tv_last);
		if (r == -1)
			break;

	}

	if (options[OptDecoderCmd]) {
		doioctl(fd, VIDIOC_DECODER_CMD, &dec_cmd);
		options[OptDecoderCmd] = false;
	}
	doioctl(fd, VIDIOC_STREAMOFF, &type);
	fcntl(fd, F_SETFL, fd_flags);
	fprintf(stderr, "\n");

	do_release_buffers(&reqbufs, num_planes, is_mmap, buffers,
			   buffer_lengths);

	if (fin && fin != stdin)
		fclose(fin);
}

enum stream_type {
	CAP,
	OUT,
};

void streaming_set_m2m(int fd)
{
	int fd_flags = fcntl(fd, F_GETFL);
	bool use_poll = options[OptStreamPoll];

	bool is_mplane = capabilities & V4L2_CAP_VIDEO_M2M_MPLANE;
	unsigned num_planes[2] = {1, 1};
	bool is_mmap = options[OptStreamMmap];

	__u32 type[2];
	FILE *file[2] = {NULL, NULL};
	struct v4l2_requestbuffers reqbufs[2];

	if (!(capabilities & V4L2_CAP_VIDEO_M2M) &&
	    !(capabilities & V4L2_CAP_VIDEO_M2M_MPLANE)) {
		fprintf(stderr, "unsupported stream type\n");
		return;
	}

	type[CAP] = is_mplane ?
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;

	type[OUT] = is_mplane ?
		V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT;

	memset(&reqbufs, 0, sizeof(reqbufs));

	reqbufs[CAP].count = reqbufs_count_cap;
	reqbufs[CAP].type = type[CAP];
	reqbufs[CAP].memory = is_mmap ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;

	reqbufs[OUT].count = reqbufs_count_out;
	reqbufs[OUT].type = type[OUT];
	reqbufs[OUT].memory = is_mmap ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;

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

	if (doioctl(fd, VIDIOC_REQBUFS, &reqbufs[CAP]) ||
	    doioctl(fd, VIDIOC_REQBUFS, &reqbufs[OUT]))
		return;

	void *buffers_cap[reqbufs_count_cap * VIDEO_MAX_PLANES];
	void *buffers_out[reqbufs_count_out * VIDEO_MAX_PLANES];
	unsigned buffer_lengths_cap[reqbufs_count_cap * VIDEO_MAX_PLANES];
	unsigned buffer_lengths_out[reqbufs_count_out * VIDEO_MAX_PLANES];

	do_setup_cap_buffers(fd, &reqbufs[CAP], is_mplane, num_planes[CAP],
			     is_mmap, buffers_cap, buffer_lengths_cap);

	do_setup_out_buffers(fd, &reqbufs[OUT], is_mplane, num_planes[OUT],
			     is_mmap, buffers_out, buffer_lengths_out,
			     file[OUT]);

	if (doioctl(fd, VIDIOC_STREAMON, &type[CAP]) ||
	    doioctl(fd, VIDIOC_STREAMON, &type[OUT]))
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

		int r;

		if (use_poll) {
			struct timeval tv;

			/* Timeout. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

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

			r = select(fd + 1, rd_fds, wr_fds, ex_fds, &tv);

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

		if (rd_fds && FD_ISSET(fd, rd_fds)) {
			r  = do_handle_cap(fd, &reqbufs[CAP], is_mplane, num_planes[CAP],
					   buffers_cap, buffer_lengths_cap, file[CAP],
					   count[CAP], last[CAP], tv_last[CAP]);
			if (r < 0) {
				rd_fds = NULL;
				ex_fds = NULL;
				doioctl(fd, VIDIOC_STREAMOFF, &type[CAP]);
			}
		}

		if (wr_fds && FD_ISSET(fd, wr_fds)) {
			r  = do_handle_out(fd, &reqbufs[OUT], is_mplane, num_planes[OUT],
					   buffers_out, buffer_lengths_out, file[OUT],
					   count[OUT], last[OUT], tv_last[OUT]);
			if (r < 0)  {
				wr_fds = NULL;

				if (options[OptDecoderCmd]) {
					doioctl(fd, VIDIOC_DECODER_CMD, &dec_cmd);
					options[OptDecoderCmd] = false;
				}

				doioctl(fd, VIDIOC_STREAMOFF, &type[OUT]);
			}
		}

		if (ex_fds && FD_ISSET(fd, ex_fds)) {
			struct v4l2_event ev;

			while (!ioctl(fd, VIDIOC_DQEVENT, &ev)) {

				if (ev.type != V4L2_EVENT_EOS)
					continue;

				rd_fds = NULL;
				ex_fds = NULL;
				doioctl(fd, VIDIOC_STREAMOFF, &type[CAP]);
				break;
			}
		}
	}

	fcntl(fd, F_SETFL, fd_flags);
	fprintf(stderr, "\n");

	do_release_buffers(&reqbufs[CAP], num_planes[CAP], is_mmap,
			   buffers_cap, buffer_lengths_cap);

	do_release_buffers(&reqbufs[OUT], num_planes[OUT], is_mmap,
			   buffers_out, buffer_lengths_out);

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
