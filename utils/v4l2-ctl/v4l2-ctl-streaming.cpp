#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <dirent.h>
#include <math.h>

#include "v4l2-ctl.h"
#include "v4l-stream.h"

extern "C" {
#include "v4l2-tpg.h"
}
#include "cv4l-helpers.h"

static unsigned stream_count;
static unsigned stream_skip;
static int stream_sleep = -1;
static unsigned stream_pat;
static bool stream_loop;
static bool stream_out_square;
static bool stream_out_border;
static bool stream_out_sav;
static bool stream_out_eav;
static int stream_out_pixel_aspect = -1;
static tpg_video_aspect stream_out_video_aspect;
static u8 stream_out_alpha;
static bool stream_out_alpha_red_only;
static bool stream_out_rgb_lim_range;
static unsigned stream_out_perc_fill = 100;
static v4l2_std_id stream_out_std;
static bool stream_out_refresh;
static tpg_move_mode stream_out_hor_mode = TPG_MOVE_NONE;
static tpg_move_mode stream_out_vert_mode = TPG_MOVE_NONE;
static unsigned reqbufs_count_cap = 4;
static unsigned reqbufs_count_out = 4;
static char *file_cap;
static char *host_cap;
static unsigned host_port_cap = V4L_STREAM_PORT;
static int host_fd_cap = -1;
static unsigned rle_perc;
static unsigned rle_perc_count;
static char *file_out;
static char *host_out;
static unsigned host_port_out = V4L_STREAM_PORT;
static int host_fd_out = -1;
static struct tpg_data tpg;
static unsigned output_field = V4L2_FIELD_NONE;
static bool output_field_alt;

static void *test_mmap(void *start, size_t length, int prot, int flags,
		int fd, int64_t offset)
{
 	return options[OptUseWrapper] ? v4l2_mmap(start, length, prot, flags, fd, offset) :
		mmap(start, length, prot, flags, fd, offset);
}

static int test_munmap(void *start, size_t length)
{
 	return options[OptUseWrapper] ? v4l2_munmap(start, length) :
		munmap(start, length);
}

void streaming_usage(void)
{
	printf("\nVideo Streaming options:\n"
	       "  --stream-count=<count>\n"
	       "                     stream <count> buffers. The default is to keep streaming\n"
	       "                     forever. This count does not include the number of initial\n"
	       "                     skipped buffers as is passed by --stream-skip.\n"
	       "  --stream-skip=<count>\n"
	       "                     skip the first <count> buffers. The default is 0.\n"
	       "  --stream-sleep=<count>\n"
	       "                     sleep for 1 second every <count> buffers. If <count> is 0,\n"
	       "                     then sleep forever right after streaming starts. The default\n"
	       "                     is -1 (never sleep).\n"
	       "  --stream-to=<file> stream to this file. The default is to discard the\n"
	       "                     data. If <file> is '-', then the data is written to stdout\n"
	       "                     and the --silent option is turned on automatically.\n"
	       "  --stream-to-host=<hostname[:port]> stream to this host. The default port is %d.\n"
	       "  --stream-poll      use non-blocking mode and select() to stream.\n"
	       "  --stream-mmap=<count>\n"
	       "                     capture video using mmap() [VIDIOC_(D)QBUF]\n"
	       "                     count: the number of buffers to allocate. The default is 3.\n"
	       "  --stream-user=<count>\n"
	       "                     capture video using user pointers [VIDIOC_(D)QBUF]\n"
	       "                     count: the number of buffers to allocate. The default is 3.\n"
	       "  --stream-dmabuf    capture video using dmabuf [VIDIOC_(D)QBUF]\n"
	       "                     Requires a corresponding --stream-out-mmap option.\n"
	       "  --stream-from=<file> stream from this file. The default is to generate a pattern.\n"
	       "                     If <file> is '-', then the data is read from stdin.\n"
	       "  --stream-from-host=<hostname[:port]> stream from this host. The default port is %d.\n"
	       "  --stream-loop      loop when the end of the file we are streaming from is reached.\n"
	       "                     The default is to stop.\n"
	       "  --stream-out-pattern=<count>\n"
	       "                     choose output test pattern. The default is 0.\n"
	       "  --stream-out-square\n"
	       "                     show a square in the middle of the output test pattern.\n"
	       "  --stream-out-border\n"
	       "                     show a border around the pillar/letterboxed video.\n"
	       "  --stream-out-sav   insert an SAV code in every line.\n"
	       "  --stream-out-eav   insert an EAV code in every line.\n"
	       "  --stream-out-pixel-aspect=<aspect\n"
	       "                     select a pixel aspect ratio. The default is to autodetect.\n"
	       "                     <aspect> can be one of: square, ntsc, pal\n"
	       "  --stream-out-video-aspect=<aspect\n"
	       "                     select a video aspect ratio. The default is to use the frame ratio.\n"
	       "                     <aspect> can be one of: 4x3, 14x9, 16x9, anamorphic\n"
	       "  --stream-out-alpha=<alpha-value>\n"
	       "                     value to use for the alpha component, range 0-255. The default is 0.\n"
	       "  --stream-out-alpha-red-only\n"
	       "                     only use the --stream-out-alpha value for the red colors,\n"
	       "                     for all others use 0.\n"
	       "  --stream-out-rgb-lim-range\n"
	       "                     Encode RGB values as limited [16-235] instead of full range.\n"
	       "  --stream-out-hor-speed=<speed>\n"
	       "                     choose speed for horizontal movement. The default is 0,\n"
	       "                     and the range is [-3...3].\n"
	       "  --stream-out-vert-speed=<speed>\n"
	       "                     choose speed for vertical movement. The default is 0,\n"
	       "                     and the range is [-3...3].\n"
	       "  --stream-out-perc-fill=<percentage>\n"
	       "                     percentage of the frame to actually fill. The default is 100%%.\n"
	       "  --stream-out-mmap=<count>\n"
	       "                     output video using mmap() [VIDIOC_(D)QBUF]\n"
	       "                     count: the number of buffers to allocate. The default is 4.\n"
	       "  --stream-out-user=<count>\n"
	       "                     output video using user pointers [VIDIOC_(D)QBUF]\n"
	       "                     count: the number of buffers to allocate. The default is 4.\n"
	       "  --stream-out-dmabuf\n"
	       "                     output video using dmabuf [VIDIOC_(D)QBUF]\n"
	       "                     Requires a corresponding --stream-mmap option.\n"
	       "  --list-patterns    list available patterns for use with --stream-pattern.\n"
	       "  --list-buffers     list all video buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-out list all video output buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-vbi list all VBI buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-vbi-out\n"
	       "                     list all VBI output buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-sliced-vbi\n"
	       "                     list all sliced VBI buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-sliced-vbi-out\n"
	       "                     list all sliced VBI output buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-sdr\n"
	       "                     list all SDR RX buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-sdr-out\n"
	       "                     list all SDR TX buffers [VIDIOC_QUERYBUF]\n",
		V4L_STREAM_PORT, V4L_STREAM_PORT);
}

static void setTimeStamp(struct v4l2_buffer &buf)
{
	struct timespec ts;

	if ((buf.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK) != V4L2_BUF_FLAG_TIMESTAMP_COPY)
		return;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	buf.timestamp.tv_sec = ts.tv_sec;
	buf.timestamp.tv_usec = ts.tv_nsec / 1000;
}

static __u32 read_u32(FILE *f)
{
	__u32 v;

	fread(&v, 1, sizeof(v), f);
	return ntohl(v);
}

static void write_u32(FILE *f, __u32 v)
{
	v = htonl(v);
	fwrite(&v, 1, sizeof(v), f);
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
	{ V4L2_BUF_FLAG_LAST, "last" },
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

static std::string timestamp_src2s(__u32 flags)
{
	char buf[20];

	switch (flags & V4L2_BUF_FLAG_TSTAMP_SRC_MASK) {
	case V4L2_BUF_FLAG_TSTAMP_SRC_EOF:
		return "End-of-Frame";
	case V4L2_BUF_FLAG_TSTAMP_SRC_SOE:
		return "Start-of-Exposure";
	default:
		sprintf(buf, "Source %d", (flags & V4L2_BUF_FLAG_TSTAMP_SRC_MASK) >> 16);
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
	const unsigned ts_flags = V4L2_BUF_FLAG_TIMESTAMP_MASK | V4L2_BUF_FLAG_TSTAMP_SRC_MASK;

	fprintf(f, "\tIndex    : %d\n", buf.index);
	fprintf(f, "\tType     : %s\n", buftype2s(buf.type).c_str());
	fprintf(f, "\tFlags    : %s\n", flags2s(buf.flags & ~ts_flags, flags_def).c_str());
	fprintf(f, "\tField    : %s\n", field2s(buf.field).c_str());
	fprintf(f, "\tSequence : %u\n", buf.sequence);
	fprintf(f, "\tLength   : %u\n", buf.length);
	fprintf(f, "\tBytesused: %u\n", buf.bytesused);
	fprintf(f, "\tTimestamp: %lu.%06lus (%s, %s)\n", buf.timestamp.tv_sec, buf.timestamp.tv_usec,
			timestamp_type2s(buf.flags).c_str(), timestamp_src2s(buf.flags).c_str());
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
	unsigned i;
	int speed;

	switch (ch) {
	case OptStreamCount:
		stream_count = strtoul(optarg, 0L, 0);
		break;
	case OptStreamSkip:
		stream_skip = strtoul(optarg, 0L, 0);
		break;
	case OptStreamSleep:
		stream_sleep = strtol(optarg, 0L, 0);
		break;
	case OptStreamLoop:
		stream_loop = true;
		break;
	case OptStreamOutPattern:
		stream_pat = strtoul(optarg, 0L, 0);
		for (i = 0; tpg_pattern_strings[i]; i++) ;
		if (stream_pat >= i)
			stream_pat = 0;
		break;
	case OptStreamOutSquare:
		stream_out_square = true;
		break;
	case OptStreamOutBorder:
		stream_out_border = true;
		break;
	case OptStreamOutInsertSAV:
		stream_out_sav = true;
		break;
	case OptStreamOutInsertEAV:
		stream_out_eav = true;
		break;
	case OptStreamOutPixelAspect:
		if (!strcmp(optarg, "square"))
			stream_out_pixel_aspect = TPG_PIXEL_ASPECT_SQUARE;
		else if (!strcmp(optarg, "ntsc"))
			stream_out_pixel_aspect = TPG_PIXEL_ASPECT_NTSC;
		else if (!strcmp(optarg, "pal"))
			stream_out_pixel_aspect = TPG_PIXEL_ASPECT_PAL;
		else
			streaming_usage();
		break;
	case OptStreamOutVideoAspect:
		if (!strcmp(optarg, "4x3"))
			stream_out_video_aspect = TPG_VIDEO_ASPECT_4X3;
		else if (!strcmp(optarg, "14x9"))
			stream_out_video_aspect = TPG_VIDEO_ASPECT_14X9_CENTRE;
		else if (!strcmp(optarg, "16x9"))
			stream_out_video_aspect = TPG_VIDEO_ASPECT_16X9_CENTRE;
		else if (!strcmp(optarg, "anamorphic"))
			stream_out_video_aspect = TPG_VIDEO_ASPECT_16X9_ANAMORPHIC;
		else
			streaming_usage();
		break;
	case OptStreamOutAlphaComponent:
		stream_out_alpha = strtoul(optarg, 0L, 0);
		break;
	case OptStreamOutAlphaRedOnly:
		stream_out_alpha_red_only = true;
		break;
	case OptStreamOutRGBLimitedRange:
		stream_out_rgb_lim_range = true;
		break;
	case OptStreamOutHorSpeed:
	case OptStreamOutVertSpeed:
		speed = strtol(optarg, 0L, 0);
		if (speed < -3)
			speed = -3;
		if (speed > 3)
			speed = 3;
		if (ch == OptStreamOutHorSpeed)
			stream_out_hor_mode = (tpg_move_mode)(speed + 3);
		else
			stream_out_vert_mode = (tpg_move_mode)(speed + 3);
		break;
	case OptStreamOutPercFill:
		stream_out_perc_fill = strtoul(optarg, 0L, 0);
		if (stream_out_perc_fill > 100)
			stream_out_perc_fill = 100;
		if (stream_out_perc_fill < 1)
			stream_out_perc_fill = 1;
		break;
	case OptStreamTo:
		file_cap = optarg;
		if (!strcmp(file_cap, "-"))
			options[OptSilent] = true;
		break;
	case OptStreamToHost:
		host_cap = optarg;
		break;
	case OptStreamFrom:
		file_out = optarg;
		break;
	case OptStreamFromHost:
		host_out = optarg;
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
	buffers(bool is_output)
	{
		if (capabilities & V4L2_CAP_SDR_CAPTURE)
			type = V4L2_BUF_TYPE_SDR_CAPTURE;
		else if (capabilities & V4L2_CAP_SDR_OUTPUT)
			type = V4L2_BUF_TYPE_SDR_OUTPUT;
		else
			type = is_output ? vidout_buftype : vidcap_buftype;
		if (is_output) {
			if (options[OptStreamOutMmap])
				memory = V4L2_MEMORY_MMAP;
			else if (options[OptStreamOutUser])
				memory = V4L2_MEMORY_USERPTR;
			else if (options[OptStreamOutDmaBuf])
				memory = V4L2_MEMORY_DMABUF;
			is_mplane = out_capabilities & (V4L2_CAP_VIDEO_M2M_MPLANE |
							V4L2_CAP_VIDEO_OUTPUT_MPLANE);
		} else {
			if (options[OptStreamMmap])
				memory = V4L2_MEMORY_MMAP;
			else if (options[OptStreamUser])
				memory = V4L2_MEMORY_USERPTR;
			else if (options[OptStreamDmaBuf])
				memory = V4L2_MEMORY_DMABUF;
			is_mplane = capabilities & (V4L2_CAP_VIDEO_M2M_MPLANE |
						    V4L2_CAP_VIDEO_CAPTURE_MPLANE);
		}
		for (int i = 0; i < VIDEO_MAX_FRAME; i++)
			for (int p = 0; p < VIDEO_MAX_PLANES; p++)
				fds[i][p] = -1;
		num_planes = is_mplane ? 0 : 1;
	}
	~buffers()
	{
		for (int i = 0; i < VIDEO_MAX_FRAME; i++)
			for (int p = 0; p < VIDEO_MAX_PLANES; p++)
				if (fds[i][p] != -1)
					close(fds[i][p]);
	}

public:
	unsigned type;
	unsigned memory;
	bool is_mplane;
	unsigned bcount;
	unsigned num_planes;
	struct v4l2_plane planes[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];
	void *bufs[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];
	int fds[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];
	unsigned bpl[VIDEO_MAX_PLANES];

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
		if (is_mplane) {
			struct v4l2_plane planes[VIDEO_MAX_PLANES];
			struct v4l2_buffer buf;

			memset(&buf, 0, sizeof(buf));
			memset(planes, 0, sizeof(planes));
			buf.type = type;
			buf.memory = memory;
			buf.m.planes = planes;
			buf.length = VIDEO_MAX_PLANES;
			err = doioctl(fd, VIDIOC_QUERYBUF, &buf);
			if (err)
				return err;
			num_planes = buf.length;
		}
		return err;
	}

	int expbufs(int fd, unsigned type)
	{
		struct v4l2_exportbuffer expbuf;
		unsigned i, p;
		int err;

		memset(&expbuf, 0, sizeof(expbuf));
		for (i = 0; i < bcount; i++) {
			for (p = 0; p < num_planes; p++) {
				expbuf.type = type;
				expbuf.index = i;
				expbuf.plane = p;
				expbuf.flags = O_RDWR;
				err = doioctl(fd, VIDIOC_EXPBUF, &expbuf);
				if (err < 0)
					return err;
				if (err >= 0)
					fds[i][p] = expbuf.fd;
			}
		}
		return 0;
	}
};

static bool fill_buffer_from_file(buffers &b, unsigned idx, FILE *fin)
{
	if (host_fd_out >= 0) {
		for (;;) {
			unsigned packet = read_u32(fin);

			if (packet == V4L_STREAM_PACKET_END) {
				fprintf(stderr, "END packet read\n");
				return false;
			}

			char buf[1024];
			unsigned sz = read_u32(fin);

			if (packet == V4L_STREAM_PACKET_FRAME_VIDEO)
				break;

			fprintf(stderr, "expected FRAME_VIDEO, got 0x%08x\n", packet);
			while (sz) {
				unsigned rdsize = sz > sizeof(buf) ? sizeof(buf) : sz;

				int n = fread(buf, 1, rdsize, fin);
				if (n < 0) {
					fprintf(stderr, "error reading %d bytes\n", sz);
					return false;
				}
				sz -= n;
			}
		}

		unsigned sz = read_u32(fin);

		if (sz != V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_HDR) {
			fprintf(stderr, "unsupported FRAME_VIDEO size\n");
			return false;
		}
		read_u32(fin);  // ignore field
		read_u32(fin);  // ignore flags
		for (unsigned j = 0; j < b.num_planes; j++) {
			__u8 *buf = (__u8 *)b.bufs[idx][j];
			struct v4l2_plane &p = b.planes[idx][j];

			sz = read_u32(fin);
			if (sz != V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_PLANE_HDR) {
				fprintf(stderr, "unsupported FRAME_VIDEO plane size\n");
				return false;
			}
			__u32 size = read_u32(fin);
			__u32 rle_size = read_u32(fin);
			__u32 offset = size - rle_size;
			unsigned sz = rle_size;

			if (size > p.length) {
				fprintf(stderr, "plane size is too large (%u > %u)\n",
					size, p.length);
				return false;
			}
			while (sz) {
				int n = fread(buf + offset, 1, sz, fin);
				if (n < 0) {
					fprintf(stderr, "error reading %d bytes\n", sz);
					return false;
				}
				if ((__u32)n == sz)
					break;
				offset += n;
				sz -= n;
			}
			rle_decompress(buf, size, rle_size, b.bpl[j]);
		}
		return true;
	}
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

static int do_setup_cap_buffers(int fd, buffers &b)
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
			return -1;

		if (b.is_mplane) {
			for (unsigned j = 0; j < b.num_planes; j++) {
				struct v4l2_plane &p = b.planes[i][j];

				p.length = planes[j].length;
				if (b.memory == V4L2_MEMORY_MMAP) {
					b.bufs[i][j] = test_mmap(NULL, p.length,
							  PROT_READ | PROT_WRITE, MAP_SHARED,
							  fd, planes[j].m.mem_offset);

					if (b.bufs[i][j] == MAP_FAILED) {
						fprintf(stderr, "mmap plane %u failed\n", j);
						return -1;
					}
				} else if (b.memory == V4L2_MEMORY_DMABUF) {
					b.bufs[i][j] = mmap(NULL, p.length,
							  PROT_READ | PROT_WRITE, MAP_SHARED,
							  b.fds[i][j], 0);

					if (b.bufs[i][j] == MAP_FAILED) {
						fprintf(stderr, "dmabuf mmap plane %u failed\n", j);
						return -1;
					}
					planes[j].m.fd = b.fds[i][j];
				} else {
					b.bufs[i][j] = calloc(1, p.length);
					planes[j].m.userptr = (unsigned long)b.bufs[i][j];
				}
			}
		}
		else {
			struct v4l2_plane &p = b.planes[i][0];

			p.length = buf.length;
			if (b.memory == V4L2_MEMORY_MMAP) {
				b.bufs[i][0] = test_mmap(NULL, p.length,
						  PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

				if (b.bufs[i][0] == MAP_FAILED) {
					fprintf(stderr, "mmap failed\n");
					return -1;
				}
			} else if (b.memory == V4L2_MEMORY_DMABUF) {
				b.bufs[i][0] = mmap(NULL, p.length,
						PROT_READ | PROT_WRITE, MAP_SHARED,
						b.fds[i][0], 0);

				if (b.bufs[i][0] == MAP_FAILED) {
					fprintf(stderr, "dmabuf mmap failed\n");
					return -1;
				}
				buf.m.fd = b.fds[i][0];
			} else {
				b.bufs[i][0] = calloc(1, p.length);
				buf.m.userptr = (unsigned long)b.bufs[i][0];
			}
		}
		if (doioctl(fd, VIDIOC_QBUF, &buf))
			return -1;
	}
	return 0;
}

static int do_setup_out_buffers(int fd, buffers &b, FILE *fin, bool qbuf)
{
	tpg_pixel_aspect aspect = TPG_PIXEL_ASPECT_SQUARE;
	struct v4l2_format fmt;
	u32 field;
	unsigned factor = 1;
	unsigned p;
	bool can_fill;

	memset(&fmt, 0, sizeof(fmt));
	fmt.fmt.pix.priv = out_priv_magic;
	fmt.type = b.type;
	doioctl(fd, VIDIOC_G_FMT, &fmt);
	if (test_ioctl(fd, VIDIOC_G_STD, &stream_out_std)) {
		struct v4l2_dv_timings timings;

		stream_out_std = 0;
		if (test_ioctl(fd, VIDIOC_G_DV_TIMINGS, &timings))
			memset(&timings, 0, sizeof(timings));
		else if (timings.bt.width == 720 && timings.bt.height == 480)
			aspect = TPG_PIXEL_ASPECT_NTSC;
		else if (timings.bt.width == 720 && timings.bt.height == 576)
			aspect = TPG_PIXEL_ASPECT_PAL;
	} else if (stream_out_std & V4L2_STD_525_60) {
		aspect = TPG_PIXEL_ASPECT_NTSC;
	} else if (stream_out_std & V4L2_STD_625_50) {
		aspect = TPG_PIXEL_ASPECT_PAL;
	}


	if (b.is_mplane)
		field = fmt.fmt.pix_mp.field;
	else
		field = fmt.fmt.pix.field;

	output_field = field;
	output_field_alt = field == V4L2_FIELD_ALTERNATE;
	if (V4L2_FIELD_HAS_T_OR_B(field)) {
		factor = 2;
		output_field = (stream_out_std & V4L2_STD_525_60) ?
			V4L2_FIELD_BOTTOM : V4L2_FIELD_TOP;
	}

	tpg_init(&tpg, 640, 360);
	if (b.is_mplane) {
		tpg_alloc(&tpg, fmt.fmt.pix_mp.width);
		can_fill = tpg_s_fourcc(&tpg, fmt.fmt.pix_mp.pixelformat);
		tpg_reset_source(&tpg, fmt.fmt.pix_mp.width,
				 fmt.fmt.pix_mp.height * factor, field);
		tpg_s_colorspace(&tpg, fmt.fmt.pix_mp.colorspace);
		tpg_s_xfer_func(&tpg, fmt.fmt.pix_mp.xfer_func);
		tpg_s_ycbcr_enc(&tpg, fmt.fmt.pix_mp.ycbcr_enc);
		tpg_s_quantization(&tpg, fmt.fmt.pix_mp.quantization);
		if (can_fill)
			for (p = 0; p < fmt.fmt.pix_mp.num_planes; p++)
				tpg_s_bytesperline(&tpg, p,
						fmt.fmt.pix_mp.plane_fmt[p].bytesperline);
	} else {
		tpg_alloc(&tpg, fmt.fmt.pix.width);
		can_fill = tpg_s_fourcc(&tpg, fmt.fmt.pix.pixelformat);
		tpg_reset_source(&tpg, fmt.fmt.pix.width,
				 fmt.fmt.pix.height * factor, field);
		tpg_s_colorspace(&tpg, fmt.fmt.pix.colorspace);
		tpg_s_xfer_func(&tpg, fmt.fmt.pix.xfer_func);
		tpg_s_ycbcr_enc(&tpg, fmt.fmt.pix.ycbcr_enc);
		tpg_s_quantization(&tpg, fmt.fmt.pix.quantization);
		tpg_s_bytesperline(&tpg, 0, fmt.fmt.pix.bytesperline);
	}
	tpg_s_pattern(&tpg, (tpg_pattern)stream_pat);
	tpg_s_mv_hor_mode(&tpg, stream_out_hor_mode);
	tpg_s_mv_vert_mode(&tpg, stream_out_vert_mode);
	tpg_s_show_square(&tpg, stream_out_square);
	tpg_s_show_border(&tpg, stream_out_border);
	tpg_s_insert_sav(&tpg, stream_out_sav);
	tpg_s_insert_eav(&tpg, stream_out_eav);
	tpg_s_perc_fill(&tpg, stream_out_perc_fill);
	if (stream_out_rgb_lim_range)
		tpg_s_real_rgb_range(&tpg, V4L2_DV_RGB_RANGE_LIMITED);
	tpg_s_alpha_component(&tpg, stream_out_alpha);
	tpg_s_alpha_mode(&tpg, stream_out_alpha_red_only);
	tpg_s_video_aspect(&tpg, stream_out_video_aspect);
	switch (stream_out_pixel_aspect) {
	case -1:
		tpg_s_pixel_aspect(&tpg, aspect);
		break;
	default:
		tpg_s_pixel_aspect(&tpg, (tpg_pixel_aspect)stream_out_pixel_aspect);
		break;
	}
	field = output_field;
	if (can_fill && ((V4L2_FIELD_HAS_T_OR_B(field) && (stream_count & 1)) ||
			 !tpg_pattern_is_static(&tpg)))
		stream_out_refresh = true;

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
			return -1;

		buf.field = field;
		tpg_s_field(&tpg, field, output_field_alt);
		if (output_field_alt) {
			if (field == V4L2_FIELD_TOP)
				field = V4L2_FIELD_BOTTOM;
			else if (field == V4L2_FIELD_BOTTOM)
				field = V4L2_FIELD_TOP;
		}

		if (b.is_mplane) {
			for (unsigned j = 0; j < b.num_planes; j++) {
				struct v4l2_plane &p = b.planes[i][j];

				p.length = planes[j].length;
				buf.m.planes[j].bytesused = planes[j].length;
				if (b.memory == V4L2_MEMORY_MMAP) {
					b.bufs[i][j] = mmap(NULL, p.length,
							  PROT_READ | PROT_WRITE, MAP_SHARED,
							  fd, planes[j].m.mem_offset);

					if (b.bufs[i][j] == MAP_FAILED) {
						fprintf(stderr, "mmap output plane %u failed\n", j);
						return -1;
					}
				} else if (b.memory == V4L2_MEMORY_DMABUF) {
					b.bufs[i][j] = mmap(NULL, p.length,
							  PROT_READ | PROT_WRITE, MAP_SHARED,
							  b.fds[i][j], 0);

					if (b.bufs[i][j] == MAP_FAILED) {
						fprintf(stderr, "dmabuf mmap output plane %u failed\n", j);
						return -1;
					}
					planes[j].m.fd = b.fds[i][j];
				} else {
					b.bufs[i][j] = calloc(1, p.length);
					planes[j].m.userptr = (unsigned long)b.bufs[i][j];
				}
				if (can_fill)
					tpg_fillbuffer(&tpg, stream_out_std, j, (u8 *)b.bufs[i][j]);
			}
			if (fin)
				fill_buffer_from_file(b, buf.index, fin);
		}
		else {
			b.planes[i][0].length = buf.length;
			buf.bytesused = buf.length;
			if (b.memory == V4L2_MEMORY_MMAP) {
				b.bufs[i][0] = test_mmap(NULL, buf.length,
						  PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

				if (b.bufs[i][0] == MAP_FAILED) {
					fprintf(stderr, "mmap output failed\n");
					return -1;
				}
			} else if (b.memory == V4L2_MEMORY_DMABUF) {
				b.bufs[i][0] = mmap(NULL, buf.length,
						PROT_READ | PROT_WRITE, MAP_SHARED,
						b.fds[i][0], 0);

				if (b.bufs[i][0] == MAP_FAILED) {
					fprintf(stderr, "dmabuf mmap output failed\n");
					return -1;
				}
				buf.m.fd = b.fds[i][0];
			} else {
				b.bufs[i][0] = calloc(1, buf.length);
				buf.m.userptr = (unsigned long)b.bufs[i][0];
			}
			if (!fin || !fill_buffer_from_file(b, buf.index, fin))
				if (can_fill)
					tpg_fillbuffer(&tpg, stream_out_std, 0, (u8 *)b.bufs[i][0]);
		}
		if (qbuf) {
			if (V4L2_TYPE_IS_OUTPUT(buf.type))
				setTimeStamp(buf);
			if (doioctl(fd, VIDIOC_QBUF, &buf))
				return -1;
			tpg_update_mv_count(&tpg, V4L2_FIELD_HAS_T_OR_B(field));
		}
	}
	if (qbuf)
		output_field = field;
	return 0;
}

static void do_release_buffers(buffers &b)
{
	for (unsigned i = 0; i < b.bcount; i++) {
		for (unsigned j = 0; j < b.num_planes; j++) {
			if (b.memory == V4L2_MEMORY_USERPTR)
				free(b.bufs[i][j]);
			else if (b.memory == V4L2_MEMORY_DMABUF)
				munmap(b.bufs[i][j], b.planes[i][j].length);
			else
				test_munmap(b.bufs[i][j], b.planes[i][j].length);
		}
	}
	tpg_free(&tpg);
}

static int do_handle_cap(int fd, buffers &b, FILE *fout, int *index,
			 unsigned &count, struct timespec &ts_last)
{
	char ch = '<';
	int ret;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buf;
	bool ignore_count_skip = false;
	static unsigned last_sec;

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

	for (;;) {
		ret = test_ioctl(fd, VIDIOC_DQBUF, &buf);
		if (ret < 0 && errno == EAGAIN)
			return 0;
		if (ret < 0) {
			fprintf(stderr, "%s: failed: %s\n", "VIDIOC_DQBUF", strerror(errno));
			return -1;
		}
		if (!(buf.flags & V4L2_BUF_FLAG_ERROR))
			break;
		if (verbose)
			print_buffer(stderr, buf);
		test_ioctl(fd, VIDIOC_QBUF, &buf);
	}
	if (fout && (!stream_skip || ignore_count_skip) && !(buf.flags & V4L2_BUF_FLAG_ERROR)) {
		unsigned rle_size[VIDEO_MAX_PLANES];

		if (host_fd_cap >= 0) {
			unsigned tot_rle_size = 0;
			unsigned tot_used = 0;

			for (unsigned j = 0; j < b.num_planes; j++) {
				__u32 used = b.is_mplane ? planes[j].bytesused : buf.bytesused;
				unsigned offset = b.is_mplane ? planes[j].data_offset : 0;

				rle_size[j] = rle_compress((__u8 *)b.bufs[buf.index][j] + offset,
							   used - offset, b.bpl[j]);
				tot_rle_size += rle_size[j];
				tot_used += used - offset;
			}
			write_u32(fout, V4L_STREAM_PACKET_FRAME_VIDEO);
			write_u32(fout, V4L_STREAM_PACKET_FRAME_VIDEO_SIZE(b.num_planes) + tot_rle_size);
			write_u32(fout, V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_HDR);
			write_u32(fout, buf.field);
			write_u32(fout, buf.flags);
			rle_perc += (tot_rle_size * 100 / tot_used);
			rle_perc_count++;
		}
		for (unsigned j = 0; j < b.num_planes; j++) {
			__u32 used = b.is_mplane ? planes[j].bytesused : buf.bytesused;
			unsigned offset = b.is_mplane ? planes[j].data_offset : 0;
			unsigned sz;

			if (offset > used) {
				// Should never happen
				fprintf(stderr, "offset %d > used %d!\n",
					offset, used);
				offset = 0;
			}
			used -= offset;
			if (host_fd_cap >= 0) {
				write_u32(fout, V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_PLANE_HDR);
				write_u32(fout, used);
				write_u32(fout, rle_size[j]);
				used = rle_size[j];
			}
			sz = fwrite((char *)b.bufs[buf.index][j] + offset, 1, used, fout);

			if (sz != used)
				fprintf(stderr, "%u != %u\n", sz, used);
		}
		if (host_fd_cap >= 0)
			fflush(fout);
	}
	if (buf.flags & V4L2_BUF_FLAG_KEYFRAME)
		ch = 'K';
	else if (buf.flags & V4L2_BUF_FLAG_PFRAME)
		ch = 'P';
	else if (buf.flags & V4L2_BUF_FLAG_BFRAME)
		ch = 'B';
	if (verbose)
		print_buffer(stderr, buf);
	if (index == NULL && test_ioctl(fd, VIDIOC_QBUF, &buf))
		return -1;
	if (index)
		*index = buf.index;

	if (!verbose) {
		fprintf(stderr, "%c", ch);
		fflush(stderr);
	}

	if (count == 0) {
		clock_gettime(CLOCK_MONOTONIC, &ts_last);
		last_sec = 0;
	} else {
		struct timespec ts_cur, res;

		clock_gettime(CLOCK_MONOTONIC, &ts_cur);
		res.tv_sec = ts_cur.tv_sec - ts_last.tv_sec;
		res.tv_nsec = ts_cur.tv_nsec - ts_last.tv_nsec;
		if (res.tv_nsec < 0) {
			res.tv_sec--;
			res.tv_nsec += 1000000000;
		}
		if (res.tv_sec > last_sec) {
			__u64 fps = 10000ULL * count;

			fps /= (__u64)res.tv_sec * 100ULL + (__u64)res.tv_nsec / 10000000ULL;
			last_sec = res.tv_sec;
			fprintf(stderr, " %llu.%02llu fps", fps / 100ULL, fps % 100ULL);
			if (host_fd_cap >= 0)
				fprintf(stderr, " %d%% compression", 100 - rle_perc / rle_perc_count);
			rle_perc_count = rle_perc = 0;
			fprintf(stderr, "\n");
		}
	}
	count++;

	if (ignore_count_skip)
		return 0;

	if (stream_skip) {
		stream_skip--;
		return 0;
	}
	if (stream_sleep > 0 && count % stream_sleep == 0)
		sleep(1);
	if (stream_count == 0)
		return 0;
	if (--stream_count == 0)
		return -1;

	return 0;
}

static int do_handle_out(int fd, buffers &b, FILE *fin, struct v4l2_buffer *cap,
			 unsigned &count, struct timespec &ts_last)
{
	static unsigned last_sec;
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

	if (cap) {
		buf.index = cap->index;
		ret = test_ioctl(fd, VIDIOC_QUERYBUF, &buf);
		if (b.is_mplane) {
			for (unsigned j = 0; j < b.num_planes; j++) {
				unsigned bytesused = cap->m.planes[j].bytesused;
				unsigned data_offset = cap->m.planes[j].data_offset;

				if (b.memory == V4L2_MEMORY_USERPTR) {
					planes[j].m.userptr = (unsigned long)cap->m.planes[j].m.userptr + data_offset;
					planes[j].bytesused = cap->m.planes[j].bytesused - data_offset;
					planes[j].data_offset = 0;
				} else if (b.memory == V4L2_MEMORY_DMABUF) {
					planes[j].m.fd = b.fds[cap->index][j];
					planes[j].bytesused = bytesused;
					planes[j].data_offset = data_offset;
				}
			}
		}
		else {
			buf.bytesused = cap->bytesused;
			if (b.memory == V4L2_MEMORY_USERPTR)
				buf.m.userptr = (unsigned long)cap->m.userptr;
			else if (b.memory == V4L2_MEMORY_DMABUF)
				buf.m.fd = b.fds[cap->index][0];
		}
	} else {
		ret = test_ioctl(fd, VIDIOC_DQBUF, &buf);
		if (ret < 0 && errno == EAGAIN)
			return 0;
		if (b.is_mplane) {
			for (unsigned j = 0; j < buf.length; j++)
				buf.m.planes[j].bytesused = buf.m.planes[j].length;
		} else {
			buf.bytesused = buf.length;
		}
	}
	if (ret < 0) {
		fprintf(stderr, "%s: failed: %s\n", "VIDIOC_DQBUF", strerror(errno));
		return -1;
	}
	buf.field = output_field;
	tpg_s_field(&tpg, output_field, output_field_alt);
	if (output_field_alt) {
		if (output_field == V4L2_FIELD_TOP)
			output_field = V4L2_FIELD_BOTTOM;
		else if (output_field == V4L2_FIELD_BOTTOM)
			output_field = V4L2_FIELD_TOP;
	}

	if (fin && !fill_buffer_from_file(b, buf.index, fin))
		return -1;
	if (!fin && stream_out_refresh) {
		if (b.is_mplane) {
			for (unsigned j = 0; j < b.num_planes; j++)
				tpg_fillbuffer(&tpg, stream_out_std, j, (u8 *)b.bufs[buf.index][j]);
		} else {
			tpg_fillbuffer(&tpg, stream_out_std, 0, (u8 *)b.bufs[buf.index][0]);
		}
	}

	if (V4L2_TYPE_IS_OUTPUT(buf.type))
		setTimeStamp(buf);

	if (test_ioctl(fd, VIDIOC_QBUF, &buf)) {
		fprintf(stderr, "%s: failed: %s\n", "VIDIOC_QBUF", strerror(errno));
		return -1;
	}
	tpg_update_mv_count(&tpg, V4L2_FIELD_HAS_T_OR_B(output_field));

	fprintf(stderr, ">");
	fflush(stderr);

	if (count == 0) {
		clock_gettime(CLOCK_MONOTONIC, &ts_last);
		last_sec = 0;
	} else {
		struct timespec ts_cur, res;

		clock_gettime(CLOCK_MONOTONIC, &ts_cur);
		res.tv_sec = ts_cur.tv_sec - ts_last.tv_sec;
		res.tv_nsec = ts_cur.tv_nsec - ts_last.tv_nsec;
		if (res.tv_nsec < 0) {
			res.tv_sec--;
			res.tv_nsec += 1000000000;
		}
		if (res.tv_sec > last_sec) {
			__u64 fps = 10000ULL * count;

			fps /= (__u64)res.tv_sec * 100ULL + (__u64)res.tv_nsec / 10000000ULL;
			last_sec = res.tv_sec;
			fprintf(stderr, " %llu.%02llu fps\n", fps / 100ULL, fps % 100ULL);
		}
	}
	count++;
	if (stream_sleep > 0 && count % stream_sleep == 0)
		sleep(1);
	if (stream_count == 0)
		return 0;
	if (--stream_count == 0)
		return -1;

	return 0;
}

static int do_handle_out_to_in(int out_fd, int fd, buffers &out, buffers &in)
{
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buf;
	int ret;

	memset(&buf, 0, sizeof(buf));
	memset(planes, 0, sizeof(planes));
	buf.type = out.type;
	buf.memory = out.memory;
	if (out.is_mplane) {
		buf.m.planes = planes;
		buf.length = VIDEO_MAX_PLANES;
	}

	do {
		ret = test_ioctl(out_fd, VIDIOC_DQBUF, &buf);
	} while (ret < 0 && errno == EAGAIN);
	if (ret < 0) {
		fprintf(stderr, "%s: failed: %s\n", "VIDIOC_DQBUF", strerror(errno));
		return -1;
	}
	memset(planes, 0, sizeof(planes));
	buf.type = in.type;
	buf.memory = in.memory;
	if (in.is_mplane) {
		buf.m.planes = planes;
		buf.length = VIDEO_MAX_PLANES;
	}
	ret = test_ioctl(fd, VIDIOC_QUERYBUF, &buf);
	if (ret == 0)
		ret = test_ioctl(fd, VIDIOC_QBUF, &buf);
	if (ret < 0) {
		fprintf(stderr, "%s: failed: %s\n", "VIDIOC_QBUF", strerror(errno));
		return -1;
	}
	return 0;
}

static void streaming_set_cap(int fd)
{
	struct v4l2_event_subscription sub;
	int fd_flags = fcntl(fd, F_GETFL);
	buffers b(false);
	bool use_poll = options[OptStreamPoll];
	unsigned count = 0;
	struct timespec ts_last;
	bool eos = false;
	FILE *fout = NULL;

	if (!(capabilities & (V4L2_CAP_VIDEO_CAPTURE |
			      V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			      V4L2_CAP_SDR_CAPTURE | V4L2_CAP_VIDEO_M2M |
			      V4L2_CAP_VIDEO_M2M_MPLANE))) {
		fprintf(stderr, "unsupported stream type\n");
		return;
	}
	if (options[OptStreamDmaBuf]) {
		fprintf(stderr, "--stream-dmabuf can only work in combination with --stream-out-mmap\n");
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
	} else if (host_cap) {
		char *p = strchr(host_cap, ':');
		struct sockaddr_in serv_addr;
		struct hostent *server;
		struct v4l2_format fmt = { };
		struct v4l2_cropcap cropcap = { };

		fmt.type = b.type;
		cropcap.type = b.type;
		ioctl(fd, VIDIOC_G_FMT, &fmt);

		cv4l_fmt cfmt(fmt);

		if (ioctl(fd, VIDIOC_CROPCAP, &cropcap) ||
		    !cropcap.pixelaspect.numerator ||
		    !cropcap.pixelaspect.denominator) {
			cropcap.pixelaspect.numerator = 1;
			cropcap.pixelaspect.denominator = 1;
		}
		if (p) {
			host_port_cap = strtoul(p + 1, 0L, 0);
			*p = '\0';
		}
		host_fd_cap = socket(AF_INET, SOCK_STREAM, 0);
		if (host_fd_cap < 0) {
			fprintf(stderr, "cannot open socket");
			exit(0);
		}
		server = gethostbyname(host_cap);
		if (server == NULL) {
			fprintf(stderr, "no such host %s\n", host_cap);
			exit(0);
		}
		memset((char *)&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		memcpy((char *)&serv_addr.sin_addr.s_addr,
		       (char *)server->h_addr,
		       server->h_length);
		serv_addr.sin_port = htons(host_port_cap);
		if (connect(host_fd_cap, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
			fprintf(stderr, "could not connect\n");
			exit(0);
		}
		fout = fdopen(host_fd_cap, "a");
		write_u32(fout, V4L_STREAM_ID);
		write_u32(fout, V4L_STREAM_VERSION);
		write_u32(fout, V4L_STREAM_PACKET_FMT_VIDEO);
		write_u32(fout, V4L_STREAM_PACKET_FMT_VIDEO_SIZE(cfmt.g_num_planes()));
		write_u32(fout, V4L_STREAM_PACKET_FMT_VIDEO_SIZE_FMT);
		write_u32(fout, cfmt.g_num_planes());
		write_u32(fout, cfmt.g_pixelformat());
		write_u32(fout, cfmt.g_width());
		write_u32(fout, cfmt.g_height());
		write_u32(fout, cfmt.g_field());
		write_u32(fout, cfmt.g_colorspace());
		write_u32(fout, cfmt.g_ycbcr_enc());
		write_u32(fout, cfmt.g_quantization());
		write_u32(fout, cfmt.g_xfer_func());
		write_u32(fout, cfmt.g_flags());
		write_u32(fout, cropcap.pixelaspect.numerator);
		write_u32(fout, cropcap.pixelaspect.denominator);
		for (unsigned i = 0; i < cfmt.g_num_planes(); i++) {
			write_u32(fout, V4L_STREAM_PACKET_FMT_VIDEO_SIZE_FMT_PLANE);
			write_u32(fout, cfmt.g_sizeimage(i));
			write_u32(fout, cfmt.g_bytesperline(i));
			b.bpl[i] = rle_calc_bpl(cfmt.g_bytesperline(i), cfmt.g_pixelformat());
		}
		fflush(fout);
	}

	if (b.reqbufs(fd, reqbufs_count_cap))
		goto done;

	if (do_setup_cap_buffers(fd, b))
		goto done;

	if (doioctl(fd, VIDIOC_STREAMON, &b.type))
		goto done;

	while (stream_sleep == 0)
		sleep(100);

	if (use_poll)
		fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK);

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
			goto done;
		}
		if (use_poll && r == 0) {
			fprintf(stderr, "select timeout\n");
			goto done;
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
			r  = do_handle_cap(fd, b, fout, NULL,
					   count, ts_last);
			if (r == -1)
				break;
		}

	}
	doioctl(fd, VIDIOC_STREAMOFF, &b.type);
	fcntl(fd, F_SETFL, fd_flags);
	fprintf(stderr, "\n");

	do_release_buffers(b);

done:
	if (fout && fout != stdout) {
		if (host_fd_cap >= 0)
			write_u32(fout, V4L_STREAM_PACKET_END);
		fclose(fout);
	}
}

static void streaming_set_out(int fd)
{
	buffers b(true);
	int fd_flags = fcntl(fd, F_GETFL);
	bool use_poll = options[OptStreamPoll];
	unsigned count = 0;
	struct timespec ts_last;
	FILE *fin = NULL;

	if (!(capabilities & (V4L2_CAP_VIDEO_OUTPUT |
			      V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			      V4L2_CAP_SDR_OUTPUT |
			      V4L2_CAP_VIDEO_M2M |
			      V4L2_CAP_VIDEO_M2M_MPLANE))) {
		fprintf(stderr, "unsupported stream type\n");
		return;
	}
	if (options[OptStreamOutDmaBuf]) {
		fprintf(stderr, "--stream-out-dmabuf can only work in combination with --stream-mmap\n");
		return;
	}

	if (file_out) {
		if (!strcmp(file_out, "-"))
			fin = stdin;
		else
			fin = fopen(file_out, "r");
	} else if (host_out) {
		char *p = strchr(host_out, ':');
		int listen_fd;
		socklen_t clilen;
		struct sockaddr_in serv_addr = {}, cli_addr;

		if (p) {
			host_port_out = strtoul(p + 1, 0L, 0);
			*p = '\0';
		}
		listen_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (listen_fd < 0) {
			fprintf(stderr, "could not opening socket\n");
			exit(1);
		}
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(host_port_out);
		if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
			fprintf(stderr, "could not bind\n");
			exit(1);
		}
		listen(listen_fd, 1);
		clilen = sizeof(cli_addr);
		host_fd_out = accept(listen_fd, (struct sockaddr *)&cli_addr, &clilen);
		if (host_fd_out < 0) {
			fprintf(stderr, "could not accept\n");
			exit(1);
		}
		fin = fdopen(host_fd_out, "r");
		if (read_u32(fin) != V4L_STREAM_ID) {
			fprintf(stderr, "unknown protocol ID\n");
			goto done;
		}
		if (read_u32(fin) != V4L_STREAM_VERSION) {
			fprintf(stderr, "unknown protocol version\n");
			goto done;
		}
		for (;;) {
			__u32 packet = read_u32(fin);
			char buf[1024];

			if (packet == V4L_STREAM_PACKET_END) {
				fprintf(stderr, "END packet read\n");
				goto done;
			}

			if (packet == V4L_STREAM_PACKET_FMT_VIDEO)
				break;

			unsigned sz = read_u32(fin);
			while (sz) {
				unsigned rdsize = sz > sizeof(buf) ? sizeof(buf) : sz;
				int n;

				n = fread(buf, 1, rdsize, fin);
				if (n < 0) {
					fprintf(stderr, "error reading %d bytes\n", sz);
					goto done;
				}
				sz -= n;
			}
		}
		read_u32(fin);

		cv4l_fmt cfmt;

		if (capabilities & (V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE))
			cfmt.s_type(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		else
			cfmt.s_type(V4L2_BUF_TYPE_VIDEO_OUTPUT);

		unsigned sz = read_u32(fin);

		if (sz != V4L_STREAM_PACKET_FMT_VIDEO_SIZE_FMT) {
			fprintf(stderr, "unsupported FMT_VIDEO size\n");
			goto done;
		}
		cfmt.s_num_planes(read_u32(fin));
		cfmt.s_pixelformat(read_u32(fin));
		cfmt.s_width(read_u32(fin));
		cfmt.s_height(read_u32(fin));
		cfmt.s_field(read_u32(fin));
		cfmt.s_colorspace(read_u32(fin));
		cfmt.s_ycbcr_enc(read_u32(fin));
		cfmt.s_quantization(read_u32(fin));
		cfmt.s_xfer_func(read_u32(fin));
		cfmt.s_flags(read_u32(fin));

		read_u32(fin); // pixelaspect.numerator
		read_u32(fin); // pixelaspect.denominator

		for (unsigned i = 0; i < cfmt.g_num_planes(); i++) {
			unsigned sz = read_u32(fin);

			if (sz != V4L_STREAM_PACKET_FMT_VIDEO_SIZE_FMT_PLANE) {
				fprintf(stderr, "unsupported FMT_VIDEO plane size\n");
				goto done;
			}
			cfmt.s_sizeimage(read_u32(fin), i);
			cfmt.s_bytesperline(read_u32(fin), i);
			b.bpl[i] = rle_calc_bpl(cfmt.g_bytesperline(i), cfmt.g_pixelformat());
		}
		if (doioctl(fd, VIDIOC_S_FMT, &cfmt)) {
			fprintf(stderr, "failed to set new format\n");
			goto done;
		}
	}

	if (b.reqbufs(fd, reqbufs_count_out))
		goto done;

	if (do_setup_out_buffers(fd, b, fin, true))
		goto done;

	if (doioctl(fd, VIDIOC_STREAMON, &b.type))
		goto done;

	while (stream_sleep == 0)
		sleep(100);

	if (use_poll)
		fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK);

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
				goto done;
			}

			if (r == 0) {
				fprintf(stderr, "select timeout\n");
				goto done;
			}
		}
		r = do_handle_out(fd, b, fin, NULL,
				   count, ts_last);
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

done:
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
	buffers in(false);
	buffers out(true);
	unsigned count[2] = { 0, 0 };
	struct timespec ts_last[2];
	FILE *file[2] = {NULL, NULL};
	fd_set fds[3];
	fd_set *rd_fds = &fds[0]; /* for capture */
	fd_set *ex_fds = &fds[1]; /* for capture */
	fd_set *wr_fds = &fds[2]; /* for output */

	if (!(capabilities & (V4L2_CAP_VIDEO_M2M |
			      V4L2_CAP_VIDEO_M2M_MPLANE))) {
		fprintf(stderr, "unsupported m2m stream type\n");
		return;
	}
	if (options[OptStreamDmaBuf] || options[OptStreamOutDmaBuf]) {
		fprintf(stderr, "--stream-dmabuf or --stream-out-dmabuf not supported for m2m devices\n");
		return;
	}
	if (options[OptStreamToHost] || options[OptStreamFromHost]) {
		/* Too lazy to implement this */
		fprintf(stderr, "--stream-to-host or --stream-from-host not supported for m2m devices\n");
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
		goto done;

	if (do_setup_cap_buffers(fd, in) ||
	    do_setup_out_buffers(fd, out, file[OUT], true))
		goto done;

	if (doioctl(fd, VIDIOC_STREAMON, &in.type) ||
	    doioctl(fd, VIDIOC_STREAMON, &out.type))
		goto done;

	while (stream_sleep == 0)
		sleep(100);

	if (use_poll)
		fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK);

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
			goto done;
		}
		if (use_poll && r == 0) {
			fprintf(stderr, "select timeout\n");
			goto done;
		}

		if (rd_fds && FD_ISSET(fd, rd_fds)) {
			r  = do_handle_cap(fd, in, file[CAP], NULL,
					   count[CAP], ts_last[CAP]);
			if (r < 0) {
				rd_fds = NULL;
				ex_fds = NULL;
				doioctl(fd, VIDIOC_STREAMOFF, &in.type);
			}
		}

		if (wr_fds && FD_ISSET(fd, wr_fds)) {
			r  = do_handle_out(fd, out, file[OUT], NULL,
					   count[OUT], ts_last[OUT]);
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

done:
	if (file[CAP] && file[CAP] != stdout)
		fclose(file[CAP]);

	if (file[OUT] && file[OUT] != stdin)
		fclose(file[OUT]);
}

static void streaming_set_cap2out(int fd, int out_fd)
{
	int fd_flags = fcntl(fd, F_GETFL);
	bool use_poll = options[OptStreamPoll];
	bool use_dmabuf = options[OptStreamDmaBuf] || options[OptStreamOutDmaBuf];
	bool use_userptr = options[OptStreamUser] && options[OptStreamOutUser];
	buffers in(false);
	buffers out(true);
	unsigned count[2] = { 0, 0 };
	struct timespec ts_last[2];
	FILE *file[2] = {NULL, NULL};
	fd_set fds;
	unsigned cnt = 0;

	if (!(capabilities & (V4L2_CAP_VIDEO_CAPTURE |
			      V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			      V4L2_CAP_VIDEO_M2M |
			      V4L2_CAP_VIDEO_M2M_MPLANE))) {
		fprintf(stderr, "unsupported capture stream type\n");
		return;
	} else if (!(out_capabilities & (V4L2_CAP_VIDEO_OUTPUT |
					 V4L2_CAP_VIDEO_OUTPUT_MPLANE |
					 V4L2_CAP_VIDEO_M2M |
					 V4L2_CAP_VIDEO_M2M_MPLANE))) {
		fprintf(stderr, "unsupported output stream type\n");
		return;
	}
	if (options[OptStreamDmaBuf] && !options[OptStreamOutMmap]) {
		fprintf(stderr, "--stream-dmabuf can only work in combination with --stream-out-mmap\n");
		return;
	}
	if (options[OptStreamOutDmaBuf] && !options[OptStreamMmap]) {
		fprintf(stderr, "--stream-out-dmabuf can only work in combination with --stream-mmap\n");
		return;
	}
	if (options[OptStreamDmaBuf])
		reqbufs_count_cap = reqbufs_count_out;
	if (options[OptStreamOutDmaBuf])
		reqbufs_count_out = reqbufs_count_cap;
	if (!use_dmabuf && !use_userptr) {
		fprintf(stderr, "Allowed combinations (for now):\n");
		fprintf(stderr, "\t--stream-mmap and --stream-out-dmabuf\n");
		fprintf(stderr, "\t--stream-dmabuf and --stream-out-mmap\n");
		fprintf(stderr, "\t--stream-user and --stream-out-user\n");
		return;
	}

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
	    out.reqbufs(out_fd, reqbufs_count_out))
		goto done;

	if (options[OptStreamDmaBuf]) {
		if (in.expbufs(out_fd, out.type))
			goto done;
	} else if (options[OptStreamOutDmaBuf]) {
		if (out.expbufs(fd, in.type))
			goto done;
	}

	if (in.num_planes != out.num_planes ||
	    in.is_mplane != out.is_mplane) {
		fprintf(stderr, "mismatch between number of planes\n");
		goto done;
	}

	if (do_setup_cap_buffers(fd, in) ||
	    do_setup_out_buffers(out_fd, out, file[OUT], false))
		goto done;

	if (doioctl(fd, VIDIOC_STREAMON, &in.type) ||
	    doioctl(out_fd, VIDIOC_STREAMON, &out.type))
		goto done;

	while (stream_sleep == 0)
		sleep(100);

	if (use_poll)
		fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK);

	while (1) {
		struct timeval tv = { use_poll ? 2 : 0, 0 };
		int r = 0;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		if (use_poll)
			r = select(fd + 1, &fds, NULL, NULL, &tv);

		if (r == -1) {
			if (EINTR == errno)
				continue;
			fprintf(stderr, "select error: %s\n",
					strerror(errno));
			goto done;
		}
		if (use_poll && r == 0) {
			fprintf(stderr, "select timeout\n");
			goto done;
		}

		if (FD_ISSET(fd, &fds)) {
			int index = -1;

			r = do_handle_cap(fd, in, file[CAP], &index,
					   count[CAP], ts_last[CAP]);
			if (r)
				fprintf(stderr, "handle cap %d\n", r);
			if (!r) {
				struct v4l2_plane planes[VIDEO_MAX_PLANES];
				struct v4l2_buffer buf;

				memset(&buf, 0, sizeof(buf));
				buf.type = in.type;
				buf.index = index;
				if (in.is_mplane) {
					buf.m.planes = planes;
					buf.length = VIDEO_MAX_PLANES;
					memset(planes, 0, sizeof(planes));
				}
				if (test_ioctl(fd, VIDIOC_QUERYBUF, &buf))
					break;
				r = do_handle_out(out_fd, out, file[OUT], &buf,
					   count[OUT], ts_last[OUT]);
			}
			if (r)
				fprintf(stderr, "handle out %d\n", r);
			if (!r && cnt++ > 1)
				r = do_handle_out_to_in(out_fd, fd, out, in);
			if (r)
				fprintf(stderr, "handle out2in %d\n", r);
			if (r < 0) {
				doioctl(fd, VIDIOC_STREAMOFF, &in.type);
				doioctl(out_fd, VIDIOC_STREAMOFF, &out.type);
				break;
			}
		}
	}

	fcntl(fd, F_SETFL, fd_flags);
	fprintf(stderr, "\n");

	do_release_buffers(in);
	do_release_buffers(out);

done:
	if (file[CAP] && file[CAP] != stdout)
		fclose(file[CAP]);

	if (file[OUT] && file[OUT] != stdin)
		fclose(file[OUT]);
}

void streaming_set(int fd, int out_fd)
{
	int do_cap = options[OptStreamMmap] + options[OptStreamUser] + options[OptStreamDmaBuf];
	int do_out = options[OptStreamOutMmap] + options[OptStreamOutUser] + options[OptStreamOutDmaBuf];

	if (out_fd < 0) {
		out_fd = fd;
		out_capabilities = capabilities;
		out_priv_magic = priv_magic;
	}

	if (do_cap > 1) {
		fprintf(stderr, "only one of --stream-mmap/user/dmabuf is allowed\n");
		return;
	}
	if (do_out > 1) {
		fprintf(stderr, "only one of --stream-out-mmap/user/dmabuf is allowed\n");
		return;
	}

	if (do_cap && do_out && fd == out_fd)
		streaming_set_m2m(fd);
	else if (do_cap && do_out)
		streaming_set_cap2out(fd, out_fd);
	else if (do_cap)
		streaming_set_cap(fd);
	else if (do_out)
		streaming_set_out(fd);
}

void streaming_list(int fd, int out_fd)
{
	if (out_fd < 0) {
		out_fd = fd;
		out_capabilities = capabilities;
	}

	if (options[OptListBuffers]) {
		list_buffers(fd, vidcap_buftype);
	}

	if (options[OptListBuffersOut]) {
		list_buffers(out_fd, vidout_buftype);
	}

	if (options[OptListBuffersVbi]) {
		list_buffers(fd, V4L2_BUF_TYPE_VBI_CAPTURE);
	}

	if (options[OptListBuffersSlicedVbi]) {
		list_buffers(fd, V4L2_BUF_TYPE_SLICED_VBI_CAPTURE);
	}

	if (options[OptListBuffersVbiOut]) {
		list_buffers(out_fd, V4L2_BUF_TYPE_VBI_OUTPUT);
	}

	if (options[OptListBuffersSlicedVbiOut]) {
		list_buffers(out_fd, V4L2_BUF_TYPE_SLICED_VBI_OUTPUT);
	}

	if (options[OptListBuffersSdr]) {
		list_buffers(fd, V4L2_BUF_TYPE_SDR_CAPTURE);
	}

	if (options[OptListBuffersSdrOut]) {
		list_buffers(fd, V4L2_BUF_TYPE_SDR_OUTPUT);
	}

	if (options[OptListPatterns]) {
		printf("List of available patterns:\n");
		for (unsigned i = 0; tpg_pattern_strings[i]; i++)
			printf("\t%2d: %s\n", i, tpg_pattern_strings[i]);
	}
}
