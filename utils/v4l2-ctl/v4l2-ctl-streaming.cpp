#include <cstring>

#include <netdb.h>
#include <sys/types.h>

#include <linux/media.h>

#include "compiler.h"
#include "v4l2-ctl.h"
#include "v4l-stream.h"
#include <media-info.h>

extern "C" {
#include "v4l2-tpg.h"
}

static unsigned stream_count;
static unsigned stream_skip;
static __u32 memory = V4L2_MEMORY_MMAP;
static __u32 out_memory = V4L2_MEMORY_MMAP;
static int stream_sleep = -1;
static bool stream_no_query;
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
static char *file_to;
static bool to_with_hdr;
static char *host_to;
#ifndef NO_STREAM_TO
static unsigned host_port_to = V4L_STREAM_PORT;
static unsigned bpl_cap[VIDEO_MAX_PLANES];
#endif
static bool host_lossless;
static int host_fd_to = -1;
static unsigned comp_perc;
static unsigned comp_perc_count;
static char *file_from;
static bool from_with_hdr;
static char *host_from;
static unsigned host_port_from = V4L_STREAM_PORT;
static int host_fd_from = -1;
static struct tpg_data tpg;
static unsigned output_field = V4L2_FIELD_NONE;
static bool output_field_alt;
static unsigned bpl_out[VIDEO_MAX_PLANES];
static bool last_buffer = false;
static codec_ctx *ctx;

static unsigned int cropped_width;
static unsigned int cropped_height;
static unsigned int composed_width;
static unsigned int composed_height;
static bool support_cap_compose;
static bool support_out_crop;
static bool in_source_change_event;

static __u64 last_fwht_bf_ts;
static fwht_cframe_hdr last_fwht_hdr;

struct request_fwht {
	int fd;
	__u64 ts;
	struct v4l2_ctrl_fwht_params params;
};

static request_fwht fwht_reqs[VIDEO_MAX_FRAME];

#define TS_WINDOW 241
#define FILE_HDR_ID			v4l2_fourcc('V', 'h', 'd', 'r')

enum codec_type {
	NOT_CODEC,
	ENCODER,
	DECODER
};

static enum codec_type codec_type;

#define QUEUE_ERROR -1
#define QUEUE_STOPPED -2

class fps_timestamps {
private:
	unsigned idx;
	bool full;
	double first;
	double sum;
	double ts[TS_WINDOW];
	unsigned seq[TS_WINDOW];
	unsigned dropped_buffers;
	bool alternate_fields;
	unsigned field_cnt;
	unsigned last_field;

public:
	fps_timestamps()
	{
		reset();
	}

	void reset() {
		idx = 0;
		full = false;
		first = sum = 0;
		dropped_buffers = 0;
		last_field = 0;
		field_cnt = 0;
		alternate_fields = false;
	}

	void determine_field(int fd, unsigned type);
	bool add_ts(double ts_secs, unsigned sequence, unsigned field);
	bool has_fps(bool continuous);
	double fps();
	unsigned dropped();
};

void fps_timestamps::determine_field(int fd, unsigned type)
{
	struct v4l2_format fmt = { };

	fmt.type = type;
	ioctl(fd, VIDIOC_G_FMT, &fmt);
	cv4l_fmt cfmt(fmt);
	alternate_fields = cfmt.g_field() == V4L2_FIELD_ALTERNATE;
}

bool fps_timestamps::add_ts(double ts_secs, unsigned sequence, unsigned field)
{
	if (ts_secs <= 0) {
		struct timespec ts_cur;

		clock_gettime(CLOCK_MONOTONIC, &ts_cur);
		ts_secs = ts_cur.tv_sec + ts_cur.tv_nsec / 1000000000.0;
	}

	if (alternate_fields &&
	    field != V4L2_FIELD_TOP && field != V4L2_FIELD_BOTTOM)
		return false;

	if (!full && idx == 0) {
		ts[idx] = ts_secs;
		seq[TS_WINDOW - 1] = sequence;
		seq[idx++] = sequence;
		first = ts_secs;
		last_field = field;
		field_cnt++;
		return true;
	}

	unsigned prev_idx = (idx + TS_WINDOW - 1) % TS_WINDOW;
	unsigned next_idx = (idx + 1) % TS_WINDOW;

	if (seq[prev_idx] == sequence) {
		if (alternate_fields) {
			if (last_field == field)
				return false;
			if (field_cnt != 1)
				return false;
			field_cnt++;
			last_field = field;
		}
	} else {
		if (alternate_fields) {
			if (field_cnt == 1) {
				dropped_buffers++;
				last_field = last_field == V4L2_FIELD_TOP ?
					V4L2_FIELD_BOTTOM : V4L2_FIELD_TOP;
			}
			field_cnt = 1;
			if (field == last_field) {
				dropped_buffers++;
				field_cnt++;
			}
			last_field = field;
		}
		if (seq[prev_idx] - sequence > 1) {
			unsigned dropped = sequence - seq[prev_idx] - 1;

			if (alternate_fields)
				dropped *= 2;
			dropped_buffers += dropped;
		}
	}

	if (!full) {
		sum += ts_secs - ts[idx - 1];
		ts[idx] = ts_secs;
		seq[idx++] = sequence;
		if (idx == TS_WINDOW) {
			full = true;
			idx = 0;
		}
		return true;
	}

	sum -= ts[next_idx] - ts[idx];
	ts[idx] = ts_secs;
	seq[idx] = sequence;
	idx = next_idx;
	sum += ts_secs - ts[prev_idx];
	return true;
}

bool fps_timestamps::has_fps(bool continuous = false)
{
	unsigned prev_idx = (idx + TS_WINDOW - 1) % TS_WINDOW;

	if (!continuous && ts[prev_idx] - first < 1.0)
		return false;
	return full || idx > 4;
}

unsigned fps_timestamps::dropped()
{
	unsigned res = dropped_buffers;

	dropped_buffers = 0;
	return res;
}

double fps_timestamps::fps()
{
	unsigned prev_idx = (idx + TS_WINDOW - 1) % TS_WINDOW;
	double cnt = seq[prev_idx] - seq[full ? idx : 0];
	double period = sum / cnt;
	double fps = 1.0 / period;

	first += static_cast<unsigned>(ts[prev_idx] - first);
	return fps;
};

void streaming_usage()
{
	printf("\nVideo Streaming options:\n"
	       "  --stream-count <count>\n"
	       "                     stream <count> buffers. The default is to keep streaming\n"
	       "                     forever. This count does not include the number of initial\n"
	       "                     skipped buffers as is passed by --stream-skip.\n"
	       "  --stream-skip <count>\n"
	       "                     skip the first <count> buffers. The default is 0.\n"
	       "  --stream-sleep <count>\n"
	       "                     sleep for 1 second every <count> buffers. If <count> is 0,\n"
	       "                     then sleep forever right after streaming starts. The default\n"
	       "                     is -1 (never sleep).\n"
#ifndef NO_STREAM_TO
	       "  --stream-to <file> stream to this file. The default is to discard the\n"
	       "                     data. If <file> is '-', then the data is written to stdout\n"
	       "                     and the --silent option is turned on automatically.\n"
	       "  --stream-to-hdr <file> stream to this file. Same as --stream-to, but each\n"
	       "                     frame is prefixed by a header. Use for compressed data.\n"
	       "  --stream-to-host <hostname[:port]>\n"
               "                     stream to this host. The default port is %d.\n"
	       "  --stream-lossless  always use lossless video compression.\n"
#endif
	       "  --stream-poll      use non-blocking mode and select() to stream.\n"
	       "  --stream-buf-caps  show capture buffer capabilities\n"
	       "  --stream-show-delta-now\n"
	       "                     output the difference between the buffer timestamp and current\n"
	       "                     clock, if the buffer timestamp source is the monotonic clock.\n"
	       "                     Requires --verbose as well.\n"
	       "  --stream-mmap <count>\n"
	       "                     capture video using mmap() [VIDIOC_(D)QBUF]\n"
	       "                     count: the number of buffers to allocate. The default is 3.\n"
	       "  --stream-user <count>\n"
	       "                     capture video using user pointers [VIDIOC_(D)QBUF]\n"
	       "                     count: the number of buffers to allocate. The default is 3.\n"
	       "  --stream-dmabuf    capture video using dmabuf [VIDIOC_(D)QBUF]\n"
	       "                     Requires a corresponding --stream-out-mmap option.\n"
	       "  --stream-from <file>\n"
	       "                     stream from this file. The default is to generate a pattern.\n"
	       "                     If <file> is '-', then the data is read from stdin.\n"
	       "  --stream-from-hdr <file> stream from this file. Same as --stream-from, but each\n"
	       "                     frame is prefixed by a header. Use for compressed data.\n"
	       "  --stream-from-host <hostname[:port]>\n"
	       "                     stream from this host. The default port is %d.\n"
	       "  --stream-no-query  Do not query and set the DV timings or standard before streaming.\n"
	       "  --stream-loop      loop when the end of the file we are streaming from is reached.\n"
	       "                     The default is to stop.\n"
	       "  --stream-out-pattern <count>\n"
	       "                     choose output test pattern. The default is 0.\n"
	       "  --stream-out-square\n"
	       "                     show a square in the middle of the output test pattern.\n"
	       "  --stream-out-border\n"
	       "                     show a border around the pillar/letterboxed video.\n"
	       "  --stream-out-sav   insert an SAV code in every line.\n"
	       "  --stream-out-eav   insert an EAV code in every line.\n"
	       "  --stream-out-pixel-aspect <aspect\n"
	       "                     select a pixel aspect ratio. The default is to autodetect.\n"
	       "                     <aspect> can be one of: square, ntsc, pal\n"
	       "  --stream-out-video-aspect <aspect\n"
	       "                     select a video aspect ratio. The default is to use the frame ratio.\n"
	       "                     <aspect> can be one of: 4x3, 14x9, 16x9, anamorphic\n"
	       "  --stream-out-alpha <alpha-value>\n"
	       "                     value to use for the alpha component, range 0-255. The default is 0.\n"
	       "  --stream-out-alpha-red-only\n"
	       "                     only use the --stream-out-alpha value for the red colors,\n"
	       "                     for all others use 0.\n"
	       "  --stream-out-rgb-lim-range\n"
	       "                     Encode RGB values as limited [16-235] instead of full range.\n"
	       "  --stream-out-hor-speed <speed>\n"
	       "                     choose speed for horizontal movement. The default is 0,\n"
	       "                     and the range is [-3...3].\n"
	       "  --stream-out-vert-speed <speed>\n"
	       "                     choose speed for vertical movement. The default is 0,\n"
	       "                     and the range is [-3...3].\n"
	       "  --stream-out-perc-fill <percentage>\n"
	       "                     percentage of the frame to actually fill. The default is 100%%.\n"
	       "  --stream-out-buf-caps\n"
	       "                     show output buffer capabilities\n"
	       "  --stream-out-mmap <count>\n"
	       "                     output video using mmap() [VIDIOC_(D)QBUF]\n"
	       "                     count: the number of buffers to allocate. The default is 4.\n"
	       "  --stream-out-user <count>\n"
	       "                     output video using user pointers [VIDIOC_(D)QBUF]\n"
	       "                     count: the number of buffers to allocate. The default is 4.\n"
	       "  --stream-out-dmabuf\n"
	       "                     output video using dmabuf [VIDIOC_(D)QBUF]\n"
	       "                     Requires a corresponding --stream-mmap option.\n"
	       "  --list-patterns    list available patterns for use with --stream-out-pattern.\n"
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
	       "                     list all SDR TX buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-meta\n"
	       "                     list all Meta RX buffers [VIDIOC_QUERYBUF]\n",
#ifndef NO_STREAM_TO
		V4L_STREAM_PORT,
#endif
	       	V4L_STREAM_PORT);
}

static void get_codec_type(cv4l_fd &fd)
{
	cv4l_disable_trace dt(fd);
	struct v4l2_fmtdesc fmt_desc = {};
	int num_cap_fmts = 0;
	int num_compressed_cap_fmts = 0;
	int num_out_fmts = 0;
	int num_compressed_out_fmts = 0;

	codec_type = NOT_CODEC;

	if (!fd.has_vid_m2m())
		return;

	if (fd.enum_fmt(fmt_desc, true, 0, fd.g_type()))
		return;

	do {
		if (fmt_desc.flags & V4L2_FMT_FLAG_COMPRESSED)
			num_compressed_cap_fmts++;
		num_cap_fmts++;
	} while (!fd.enum_fmt(fmt_desc));


	if (fd.enum_fmt(fmt_desc, true, 0, v4l_type_invert(fd.g_type())))
		return;

	do {
		if (fmt_desc.flags & V4L2_FMT_FLAG_COMPRESSED)
			num_compressed_out_fmts++;
		num_out_fmts++;
	} while (!fd.enum_fmt(fmt_desc));

	if (num_compressed_out_fmts == 0 && num_compressed_cap_fmts == num_cap_fmts) {
		codec_type = ENCODER;
		return;
	}

	if (num_compressed_cap_fmts == 0 && num_compressed_out_fmts == num_out_fmts) {
		codec_type = DECODER;
		return;
	}
}

static void get_cap_compose_rect(cv4l_fd &fd)
{
	cv4l_disable_trace dt(fd);
	v4l2_selection sel;

	memset(&sel, 0, sizeof(sel));
	sel.type = vidcap_buftype;
	sel.target = V4L2_SEL_TGT_COMPOSE;

	if (fd.g_selection(sel) == 0) {
		support_cap_compose = true;
		composed_width = sel.r.width;
		composed_height = sel.r.height;
	} else {
		support_cap_compose = false;
    }
}

static void get_out_crop_rect(cv4l_fd &fd)
{
	cv4l_disable_trace dt(fd);
	v4l2_selection sel;

	memset(&sel, 0, sizeof(sel));
	sel.type = vidout_buftype;
	sel.target = V4L2_SEL_TGT_CROP;

	if (fd.g_selection(sel) == 0) {
		support_out_crop = true;
		cropped_width = sel.r.width;
		cropped_height = sel.r.height;
	} else {
		support_out_crop = false;
	}
}

static void set_time_stamp(cv4l_buffer &buf)
{
	if ((buf.g_flags() & V4L2_BUF_FLAG_TIMESTAMP_MASK) != V4L2_BUF_FLAG_TIMESTAMP_COPY)
		return;
	buf.s_timestamp_clock();
}

static __u32 read_u32(FILE *f)
{
	__u32 v;

	if (fread(&v, 1, sizeof(v), f) != sizeof(v))
		return 0;
	return ntohl(v);
}

static void write_u32(FILE *f, __u32 v)
{
	v = htonl(v);
	fwrite(&v, 1, sizeof(v), f);
}

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

static void print_buffer(FILE *f, struct v4l2_buffer &buf)
{
	const unsigned ts_flags = V4L2_BUF_FLAG_TIMESTAMP_MASK | V4L2_BUF_FLAG_TSTAMP_SRC_MASK;

	fprintf(f, "\tIndex    : %d\n", buf.index);
	fprintf(f, "\tType     : %s\n", buftype2s(buf.type).c_str());
	fprintf(f, "\tFlags    : %s\n", bufferflags2s(buf.flags & ~ts_flags).c_str());
	fprintf(f, "\tField    : %s\n", field2s(buf.field).c_str());
	fprintf(f, "\tSequence : %u\n", buf.sequence);
	fprintf(f, "\tLength   : %u\n", buf.length);
	fprintf(f, "\tBytesused: %u\n", buf.bytesused);
	fprintf(f, "\tTimestamp: %llu.%06llus (%s, %s)\n",
		static_cast<__u64>(buf.timestamp.tv_sec), static_cast<__u64>(buf.timestamp.tv_usec),
		timestamp_type2s(buf.flags).c_str(), timestamp_src2s(buf.flags).c_str());
	if (buf.flags & V4L2_BUF_FLAG_TIMECODE) {
		static constexpr int fps_types[] = { 0, 24, 25, 30, 50, 60 };
		int fps = buf.timecode.type;

		if (fps > 5)
			fps = 0;
		fprintf(f, "\tTimecode : %dfps %s %dh %dm %ds %df (0x%02x 0x%02x 0x%02x 0x%02x)\n",
			fps_types[fps],
			tc_flags2s(buf.timecode.flags).c_str(),
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

static void print_concise_buffer(FILE *f, cv4l_buffer &buf, cv4l_fmt &fmt,
				 cv4l_queue &q, fps_timestamps &fps_ts,
				 int comp_perc, bool skip_ts = false)
{
	static double last_ts;

	fprintf(f, "%s dqbuf: %*u seq: %6u bytesused: ",
		v4l_type_is_output(buf.g_type()) ? "out" : "cap",
		reqbufs_count_cap > 10 ? 2 : 1, buf.g_index(), buf.g_sequence());

	bool have_data_offset = false;

	for (unsigned i = 0; i < buf.g_num_planes(); i++) {
		fprintf(f, "%s%u", i ? "/" : "", buf.g_bytesused(i));
		if (buf.g_data_offset(i))
			have_data_offset = true;
	}
	if (have_data_offset) {
		fprintf(f, " offset: ");
		for (unsigned i = 0; i < buf.g_num_planes(); i++)
			fprintf(f, "%s%u", i ? "/" : "", buf.g_data_offset(i));
	}
	if (comp_perc >= 0)
		fprintf(f, " compression: %d%%", comp_perc);

	if (!skip_ts && (buf.g_flags() & V4L2_BUF_FLAG_TIMESTAMP_MASK) != V4L2_BUF_FLAG_TIMESTAMP_COPY) {
		double ts = buf.g_timestamp().tv_sec + buf.g_timestamp().tv_usec / 1000000.0;
		fprintf(f, " ts: %.06f", ts);
		if (last_ts != 0.0)
			fprintf(f, " delta: %.03f ms", (ts - last_ts) * 1000.0);
		if (options[OptStreamShowDeltaNow] &&
		    (buf.g_flags() & V4L2_BUF_FLAG_TIMESTAMP_MASK) ==
		    V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC) {
			timespec ts_clock;

			clock_gettime(CLOCK_MONOTONIC, &ts_clock);
			fprintf(f, " delta now: %+.03f ms",
				((ts_clock.tv_sec + ts_clock.tv_nsec / 1000000000.0) - ts) * 1000.0);
		}
		last_ts = ts;

		if (fps_ts.has_fps(true))
			fprintf(stderr, " fps: %.02f", fps_ts.fps());

		unsigned dropped = fps_ts.dropped();

		if (dropped)
			fprintf(stderr, " dropped: %u", dropped);
	}

	__u32 fl = buf.g_flags() & (V4L2_BUF_FLAG_ERROR |
				    V4L2_BUF_FLAG_KEYFRAME |
				    V4L2_BUF_FLAG_PFRAME |
				    V4L2_BUF_FLAG_BFRAME |
				    V4L2_BUF_FLAG_LAST |
				    V4L2_BUF_FLAG_TIMESTAMP_MASK |
				    V4L2_BUF_FLAG_TSTAMP_SRC_MASK |
				    V4L2_BUF_FLAG_TIMECODE);
	if (fl)
		fprintf(f, " (%s)", bufferflags2s(fl).c_str());
	fprintf(f, "\n");
	if (v4l_type_is_meta(buf.g_type()) && buf.g_bytesused(0) &&
	    !(buf.g_flags() & V4L2_BUF_FLAG_ERROR))
		print_meta_buffer(f, buf, fmt, q);

	if ((capabilities & V4L2_CAP_TOUCH) && buf.g_bytesused(0) &&
	    !(buf.g_flags() & V4L2_BUF_FLAG_ERROR) &&
	    (fmt.g_width() < 64 ||  fmt.g_height() < 64))
		print_touch_buffer(f, buf, fmt, q);
}

static void stream_buf_caps(cv4l_fd &fd, unsigned buftype)
{
	v4l2_create_buffers cbufs;

	memset(&cbufs, 0, sizeof(cbufs));
	cbufs.format.type = buftype;
	cbufs.memory = V4L2_MEMORY_MMAP;
	if (!v4l_ioctl(fd.g_v4l_fd(), VIDIOC_CREATE_BUFS, &cbufs)) {
		printf("Streaming I/O Capabilities for %s: %s\n",
		       buftype2s(buftype).c_str(),
		       bufcap2s(cbufs.capabilities).c_str());
		return;
	}
	v4l2_requestbuffers rbufs;
	memset(&rbufs, 0, sizeof(rbufs));
	rbufs.type = buftype;
	rbufs.memory = V4L2_MEMORY_MMAP;
	if (v4l_ioctl(fd.g_v4l_fd(), VIDIOC_REQBUFS, &rbufs))
		return;
	printf("Streaming I/O Capabilities for %s: %s\n",
	       buftype2s(buftype).c_str(),
	       bufcap2s(rbufs.capabilities).c_str());
}

static void list_buffers(cv4l_fd &fd, unsigned buftype)
{
	cv4l_disable_trace dt(fd);
	int i;

	for (i = 0; i < VIDEO_MAX_FRAME; i++) {
		cv4l_buffer buf(buftype);

		if (fd.querybuf(buf, i))
			break;
		if (i == 0)
			printf("VIDIOC_QUERYBUF:\n");
		print_buffer(stdout, buf.buf);
	}
}

void streaming_cmd(int ch, char *optarg)
{
	unsigned i;
	int speed;

	switch (ch) {
	case OptStreamCount:
		stream_count = strtoul(optarg, nullptr, 0);
		break;
	case OptStreamSkip:
		stream_skip = strtoul(optarg, nullptr, 0);
		break;
	case OptStreamSleep:
		stream_sleep = strtol(optarg, nullptr, 0);
		break;
	case OptStreamNoQuery:
		stream_no_query = true;
		break;
	case OptStreamLoop:
		stream_loop = true;
		break;
	case OptStreamOutPattern:
		stream_pat = strtoul(optarg, nullptr, 0);
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
		stream_out_alpha = strtoul(optarg, nullptr, 0);
		break;
	case OptStreamOutAlphaRedOnly:
		stream_out_alpha_red_only = true;
		break;
	case OptStreamOutRGBLimitedRange:
		stream_out_rgb_lim_range = true;
		break;
	case OptStreamOutHorSpeed:
	case OptStreamOutVertSpeed:
		speed = strtol(optarg, nullptr, 0);
		if (speed < -3)
			speed = -3;
		if (speed > 3)
			speed = 3;
		if (ch == OptStreamOutHorSpeed)
			stream_out_hor_mode = static_cast<tpg_move_mode>(speed + 3);
		else
			stream_out_vert_mode = static_cast<tpg_move_mode>(speed + 3);
		break;
	case OptStreamOutPercFill:
		stream_out_perc_fill = strtoul(optarg, nullptr, 0);
		if (stream_out_perc_fill > 100)
			stream_out_perc_fill = 100;
		if (stream_out_perc_fill < 1)
			stream_out_perc_fill = 1;
		break;
	case OptStreamTo:
		file_to = optarg;
		to_with_hdr = false;
		if (!strcmp(file_to, "-"))
			options[OptSilent] = true;
		break;
	case OptStreamToHdr:
		file_to = optarg;
		to_with_hdr = true;
		if (!strcmp(file_to, "-"))
			options[OptSilent] = true;
		break;
	case OptStreamToHost:
		host_to = optarg;
		break;
	case OptStreamLossless:
		host_lossless = true;
		break;
	case OptStreamFrom:
		file_from = optarg;
		from_with_hdr = false;
		break;
	case OptStreamFromHdr:
		file_from = optarg;
		from_with_hdr = true;
		break;
	case OptStreamFromHost:
		host_from = optarg;
		break;
	case OptStreamUser:
		memory = V4L2_MEMORY_USERPTR;
		fallthrough;
	case OptStreamMmap:
		if (optarg) {
			reqbufs_count_cap = strtoul(optarg, nullptr, 0);
			if (reqbufs_count_cap == 0)
				reqbufs_count_cap = 3;
		}
		break;
	case OptStreamDmaBuf:
		memory = V4L2_MEMORY_DMABUF;
		break;
	case OptStreamOutUser:
		out_memory = V4L2_MEMORY_USERPTR;
		fallthrough;
	case OptStreamOutMmap:
		if (optarg) {
			reqbufs_count_out = strtoul(optarg, nullptr, 0);
			if (reqbufs_count_out == 0)
				reqbufs_count_out = 3;
		}
		break;
	case OptStreamOutDmaBuf:
		out_memory = V4L2_MEMORY_DMABUF;
		break;
	}
}

/*
 * Assume that the fwht stream is valid and that each
 * frame starts right after the previous one.
 */
static bool read_fwht_frame(cv4l_fmt &fmt, unsigned char *buf,
			    FILE *fpointer, unsigned &sz,
			    unsigned &expected_len, unsigned buf_len)
{
	expected_len = sizeof(struct fwht_cframe_hdr);
	if (expected_len > buf_len)
		return false;
	sz = fread(&last_fwht_hdr, 1, sizeof(struct fwht_cframe_hdr), fpointer);
	if (sz < sizeof(struct fwht_cframe_hdr))
		return true;

	expected_len = ntohl(last_fwht_hdr.size);
	if (expected_len > buf_len)
		return false;
	sz = fread(buf, 1, ntohl(last_fwht_hdr.size), fpointer);
	return true;
}

static void set_fwht_stateless_params(struct v4l2_ctrl_fwht_params &fwht_params,
				      const struct fwht_cframe_hdr *hdr,
				      __u64 last_bf_ts)
{
	fwht_params.backward_ref_ts = last_bf_ts;
	fwht_params.version = ntohl(hdr->version);
	fwht_params.width = ntohl(hdr->width);
	fwht_params.height = ntohl(hdr->height);
	fwht_params.flags = ntohl(hdr->flags);
	fwht_params.colorspace = ntohl(hdr->colorspace);
	fwht_params.xfer_func = ntohl(hdr->xfer_func);
	fwht_params.ycbcr_enc = ntohl(hdr->ycbcr_enc);
	fwht_params.quantization = ntohl(hdr->quantization);

	/*
	 * if last_bf_ts is 0 it indicates that either this is the
	 * first frame, or that last frame returned with an error so
	 * it is better not to reference it so the error won't propagate
	 */
	if (!last_bf_ts)
		fwht_params.flags |= V4L2_FWHT_FL_I_FRAME;
}

static int alloc_fwht_req(int media_fd, unsigned index)
{
	int rc = 0;

	rc = ioctl(media_fd, MEDIA_IOC_REQUEST_ALLOC, &fwht_reqs[index]);
	if (rc < 0) {
		fprintf(stderr, "Unable to allocate media request: %s\n",
			strerror(errno));
		return rc;
	}

	return 0;
}

static void set_fwht_req_by_idx(unsigned idx, const struct fwht_cframe_hdr *hdr,
				__u64 last_bf_ts, __u64 ts)
{
	struct v4l2_ctrl_fwht_params fwht_params;

	set_fwht_stateless_params(fwht_params, hdr, last_bf_ts);

	fwht_reqs[idx].ts = ts;
	fwht_reqs[idx].params = fwht_params;
}

static int get_fwht_req_by_ts(__u64 ts)
{
	for (int idx = 0; idx < VIDEO_MAX_FRAME; idx++) {
		if (fwht_reqs[idx].ts == ts)
			return idx;
	}
	return -1;
}

static bool set_fwht_req_by_fd(const struct fwht_cframe_hdr *hdr,
			       int req_fd, __u64 last_bf_ts, __u64 ts)
{
	struct v4l2_ctrl_fwht_params fwht_params;

	set_fwht_stateless_params(fwht_params, hdr, last_bf_ts);

	for (auto &fwht_req : fwht_reqs) {
		if (fwht_req.fd == req_fd) {
			fwht_req.ts = ts;
			fwht_req.params = fwht_params;
			return true;
		}
	}
	return false;
}

static int set_fwht_ext_ctrl(cv4l_fd &fd, const struct fwht_cframe_hdr *hdr,
			     __u64 last_bf_ts, int req_fd)
{
	v4l2_ext_controls controls;
	struct v4l2_ext_control control;
	struct v4l2_ctrl_fwht_params fwht_params;

	memset(&control, 0, sizeof(control));
	memset(&controls, 0, sizeof(controls));

	set_fwht_stateless_params(fwht_params, hdr, last_bf_ts);

	control.id = V4L2_CID_STATELESS_FWHT_PARAMS;
	control.ptr = &fwht_params;
	control.size = sizeof(fwht_params);
	controls.which = V4L2_CTRL_WHICH_REQUEST_VAL;
	controls.request_fd = req_fd;
	controls.controls = &control;
	controls.count = 1;
	return fd.s_ext_ctrls(controls);
}

static bool read_write_padded_frame(cv4l_fmt &fmt, unsigned char *buf,
				    FILE *fpointer, unsigned &sz,
				    unsigned &expected_len, unsigned buf_len,
				    bool is_read)
{
	const struct v4l2_fwht_pixfmt_info *info =
			v4l2_fwht_find_pixfmt(fmt.g_pixelformat());
	unsigned coded_height = fmt.g_height();
	unsigned real_width;
	unsigned real_height;
	unsigned char *plane_p = buf;
	unsigned char *row_p;
	unsigned stride = fmt.g_bytesperline();

	if (is_read) {
		real_width  = cropped_width;
		real_height = cropped_height;
	} else {
		real_width  = composed_width;
		real_height = composed_height;
	}

	sz = 0;
	expected_len = real_width * real_height * info->sizeimage_mult / info->sizeimage_div;
	if (expected_len > buf_len)
		return false;
	for (unsigned plane_idx = 0; plane_idx < info->planes_num; plane_idx++) {
		bool is_chroma_plane = plane_idx == 1 || plane_idx == 2;
		unsigned h_div = is_chroma_plane ? info->height_div : 1;
		unsigned w_div = is_chroma_plane ? info->width_div : 1;
		unsigned step = is_chroma_plane ? info->chroma_step :
			info->luma_alpha_step;
		unsigned int consume_sz = step * real_width / w_div;

		if (info->planes_num == 3 && plane_idx == 1)
			stride /= 2;

		if (plane_idx == 1 &&
		    (info->id == V4L2_PIX_FMT_NV24 || info->id == V4L2_PIX_FMT_NV42))
			stride *= 2;

		row_p = plane_p;
		for (unsigned i = 0; i < real_height / h_div; i++) {
			unsigned int wsz = 0;

			if (is_read)
				wsz = fread(row_p, 1, consume_sz, fpointer);
			else
				wsz = fwrite(row_p, 1, consume_sz, fpointer);
			if (wsz == 0 && i == 0 && plane_idx == 0)
				break;
			if (wsz != consume_sz) {
				fprintf(stderr, "padding: needed %u bytes, got %u\n", consume_sz, wsz);
				return true;
			}
			sz += wsz;
			row_p += stride;
		}
		plane_p += stride * (coded_height / h_div);
		if (sz == 0)
			break;
	}
	return true;
}

static bool fill_buffer_from_file(cv4l_fd &fd, cv4l_queue &q, cv4l_buffer &b,
				  cv4l_fmt &fmt, FILE *fin)
{
	static bool first = true;
	static bool is_fwht = false;

	if (host_fd_from >= 0) {
		for (;;) {
			unsigned packet = read_u32(fin);

			if (packet == V4L_STREAM_PACKET_END) {
				fprintf(stderr, "END packet read\n");
				return false;
			}

			char buf[1024];
			unsigned sz = read_u32(fin);

			if (packet == V4L_STREAM_PACKET_FRAME_VIDEO_RLE ||
			    packet == V4L_STREAM_PACKET_FRAME_VIDEO_FWHT) {
				is_fwht = packet == V4L_STREAM_PACKET_FRAME_VIDEO_FWHT;
				if (is_fwht && !ctx) {
					fprintf(stderr, "cannot support FWHT encoding\n");
					return false;
				}
				break;
			}

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
		for (unsigned j = 0; j < q.g_num_planes(); j++) {
			__u8 *buf = static_cast<__u8 *>(q.g_dataptr(b.g_index(), j));

			sz = read_u32(fin);
			if (sz != V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_PLANE_HDR) {
				fprintf(stderr, "unsupported FRAME_VIDEO plane size\n");
				return false;
			}
			__u32 size = read_u32(fin);
			__u32 comp_size = read_u32(fin);
			__u32 offset = size - comp_size;
			unsigned sz = comp_size;
			__u8 *read_buf = buf;

			if (is_fwht) {
				read_buf = new __u8[comp_size];
				offset = 0;
			}

			if (size > q.g_length(j)) {
				fprintf(stderr, "plane size is too large (%u > %u)\n",
					size, q.g_length(j));
				return false;
			}
			while (sz) {
				int n = fread(read_buf + offset, 1, sz, fin);
				if (n < 0) {
					fprintf(stderr, "error reading %d bytes\n", sz);
					return false;
				}
				if (static_cast<__u32>(n) == sz)
					break;
				offset += n;
				sz -= n;
			}
			if (is_fwht) {
				fwht_decompress(ctx, read_buf, comp_size, buf, size);
				delete [] read_buf;
			} else {
				rle_decompress(buf, size, comp_size, bpl_out[j]);
			}
		}
		return true;
	}

restart:
	if (from_with_hdr) {
		__u32 v;

		if (!fread(&v, sizeof(v), 1, fin)) {
			if (first) {
				fprintf(stderr, "Insufficient data\n");
				return false;
			}
			if (stream_loop) {
				fseek(fin, 0, SEEK_SET);
				first = true;
				goto restart;
			}
			return false;
		}
		if (ntohl(v) != FILE_HDR_ID) {
			fprintf(stderr, "Unknown header ID\n");
			return false;
		}
	}

	for (unsigned j = 0; j < q.g_num_planes(); j++) {
		void *buf = q.g_dataptr(b.g_index(), j);
		unsigned buf_len = q.g_length(j);
		unsigned expected_len = q.g_length(j);
		unsigned sz;
		cv4l_fmt fmt;
		bool res = true;

		fd.g_fmt(fmt, q.g_type());
		if (from_with_hdr) {
			expected_len = read_u32(fin);
			if (expected_len > q.g_length(j)) {
				fprintf(stderr, "plane size is too large (%u > %u)\n",
					expected_len, q.g_length(j));
				return false;
			}
		}

		if (fmt.g_pixelformat() == V4L2_PIX_FMT_FWHT_STATELESS)
			res = read_fwht_frame(fmt, static_cast<unsigned char *>(buf), fin,
					      sz, expected_len, buf_len);
		else if (codec_type != NOT_CODEC && support_out_crop &&
			 v4l2_fwht_find_pixfmt(fmt.g_pixelformat()))
			res = read_write_padded_frame(fmt, static_cast<unsigned char *>(buf),
						      fin, sz, expected_len, buf_len, true);
		else
			sz = fread(buf, 1, expected_len, fin);

		if (!res) {
			fprintf(stderr, "amount intended to be read/written is larger than the buffer size\n");
			return false;
		}
		if (first && sz != expected_len && fmt.g_bytesperline(j)) {
			fprintf(stderr, "%s: size read (%u) is different than needed (%u) in the first frame\n",
				__func__, sz, expected_len);
			return false;
		}
		if (j == 0 && sz == 0 && stream_loop) {
			fseek(fin, 0, SEEK_SET);
			first = true;
			goto restart;
		}
		b.s_bytesused(sz, j);
		if (sz == expected_len)
			continue;
		if (sz == 0)
			return false;
		if (sz && fmt.g_bytesperline(j))
			fprintf(stderr, "%u != %u\n", sz, expected_len);
	}
	first = false;
	return true;
}

static int do_setup_out_buffers(cv4l_fd &fd, cv4l_queue &q, FILE *fin, bool qbuf,
				bool ignore_count_skip)
{
	tpg_pixel_aspect aspect = TPG_PIXEL_ASPECT_SQUARE;
	cv4l_fmt fmt(q.g_type());
	u32 field;
	unsigned p;
	bool can_fill = false;
	bool is_video = q.g_type() == V4L2_BUF_TYPE_VIDEO_OUTPUT ||
			q.g_type() == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	bool is_meta = q.g_type() == V4L2_BUF_TYPE_META_OUTPUT;

	if (q.obtain_bufs(&fd)) {
		fprintf(stderr, "%s q.obtain_bufs failed\n", __func__);
		return QUEUE_ERROR;
	}

	fd.g_fmt(fmt, q.g_type());
	{
		cv4l_disable_trace dt(fd);
		if (fd.g_std(stream_out_std)) {
			struct v4l2_dv_timings timings;

			stream_out_std = 0;
			if (fd.g_dv_timings(timings))
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
	}

	field = fmt.g_field();

	output_field = field;
	output_field_alt = field == V4L2_FIELD_ALTERNATE;
	if (V4L2_FIELD_HAS_T_OR_B(field))
		output_field = (stream_out_std & V4L2_STD_525_60) ?
			V4L2_FIELD_BOTTOM : V4L2_FIELD_TOP;

	if (is_video) {
		tpg_init(&tpg, 640, 360);
		tpg_alloc(&tpg, fmt.g_width());
		can_fill = tpg_s_fourcc(&tpg, fmt.g_pixelformat());
		tpg_reset_source(&tpg, fmt.g_width(), fmt.g_frame_height(), field);
		tpg_s_colorspace(&tpg, fmt.g_colorspace());
		tpg_s_xfer_func(&tpg, fmt.g_xfer_func());
		tpg_s_ycbcr_enc(&tpg, fmt.g_ycbcr_enc());
		tpg_s_quantization(&tpg, fmt.g_quantization());
		for (p = 0; p < fmt.g_num_planes(); p++)
			tpg_s_bytesperline(&tpg, p,
					   fmt.g_bytesperline(p));
		tpg_s_pattern(&tpg, static_cast<tpg_pattern>(stream_pat));
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
			tpg_s_pixel_aspect(&tpg, static_cast<tpg_pixel_aspect>(stream_out_pixel_aspect));
			break;
		}
		field = output_field;
		if (can_fill && ((V4L2_FIELD_HAS_T_OR_B(field) && (stream_count & 1)) ||
				 !tpg_pattern_is_static(&tpg)))
			stream_out_refresh = true;
	}

	for (unsigned i = 0; i < q.g_buffers(); i++) {
		cv4l_buffer buf(q);

		if (fd.querybuf(buf, i)) {
			fprintf(stderr, "%s fd.querybuf failed\n", __func__);
			return QUEUE_ERROR;
		}

		buf.update(q, i);
		for (unsigned j = 0; j < q.g_num_planes(); j++)
			buf.s_bytesused(buf.g_length(j), j);
		if (is_video) {
			buf.s_field(field);
			tpg_s_field(&tpg, field, output_field_alt);
			if (output_field_alt) {
				if (field == V4L2_FIELD_TOP)
					field = V4L2_FIELD_BOTTOM;
				else if (field == V4L2_FIELD_BOTTOM)
					field = V4L2_FIELD_TOP;
			}

			if (can_fill) {
				for (unsigned j = 0; j < q.g_num_planes(); j++)
					tpg_fillbuffer(&tpg, stream_out_std, j, static_cast<u8 *>(q.g_dataptr(i, j)));
			}
		}
		if (is_meta)
			meta_fillbuffer(buf, fmt, q);

		if (fin && !fill_buffer_from_file(fd, q, buf, fmt, fin))
			return QUEUE_STOPPED;

		if (fmt.g_pixelformat() == V4L2_PIX_FMT_FWHT_STATELESS) {
			struct v4l2_capability vcap = {};
			fd.querycap(vcap);
			int media_fd = mi_get_media_fd(fd.g_fd(), (const char *)vcap.bus_info);

			if (media_fd < 0) {
				fprintf(stderr, "%s: mi_get_media_fd failed\n", __func__);
				return media_fd;
			}

			if (alloc_fwht_req(media_fd, i))
				return QUEUE_ERROR;
			buf.s_request_fd(fwht_reqs[i].fd);
			buf.or_flags(V4L2_BUF_FLAG_REQUEST_FD);

			if (set_fwht_ext_ctrl(fd, &last_fwht_hdr, last_fwht_bf_ts,
					      buf.g_request_fd())) {
				fprintf(stderr, "%s: set_fwht_ext_ctrl failed on %dth buf: %s\n",
					__func__, i, strerror(errno));
				return QUEUE_ERROR;
			}
		}
		if (qbuf) {
			fps_timestamps fps_ts;

			set_time_stamp(buf);
			if (fd.qbuf(buf))
				return QUEUE_ERROR;
			tpg_update_mv_count(&tpg, V4L2_FIELD_HAS_T_OR_B(field));
			if (!verbose)
				fprintf(stderr, ">");
			fflush(stderr);
			if (!ignore_count_skip && stream_count)
				if (!--stream_count)
					return QUEUE_STOPPED;
		}
		if (fmt.g_pixelformat() == V4L2_PIX_FMT_FWHT_STATELESS) {
			set_fwht_req_by_idx(i, &last_fwht_hdr,
					    last_fwht_bf_ts, buf.g_timestamp_ns());
			last_fwht_bf_ts = buf.g_timestamp_ns();
			if (ioctl(buf.g_request_fd(), MEDIA_REQUEST_IOC_QUEUE) < 0) {
				fprintf(stderr, "Unable to queue media request: %s\n",
					strerror(errno));
				return QUEUE_ERROR;
			}
		}
	}
	if (qbuf)
		output_field = field;
	return 0;
}

static void write_buffer_to_file(cv4l_fd &fd, cv4l_queue &q, cv4l_buffer &buf,
				 cv4l_fmt &fmt, FILE *fout)
{
#ifndef NO_STREAM_TO
	unsigned comp_size[VIDEO_MAX_PLANES];
	__u8 *comp_ptr[VIDEO_MAX_PLANES];

	if (host_fd_to >= 0) {
		unsigned tot_comp_size = 0;
		unsigned tot_used = 0;

		for (unsigned j = 0; j < buf.g_num_planes(); j++) {
			__u32 used = buf.g_bytesused(j);
			unsigned offset = buf.g_data_offset(j);
			u8 *p = static_cast<u8 *>(q.g_dataptr(buf.g_index(), j)) + offset;

			if (ctx) {
				comp_ptr[j] = fwht_compress(ctx, p,
							    used - offset, &comp_size[j]);
			} else {
				comp_ptr[j] = p;
				comp_size[j] = rle_compress(p, used - offset,
							    bpl_cap[j]);
			}
			tot_comp_size += comp_size[j];
			tot_used += used - offset;
		}
		write_u32(fout, ctx ? V4L_STREAM_PACKET_FRAME_VIDEO_FWHT :
				V4L_STREAM_PACKET_FRAME_VIDEO_RLE);
		write_u32(fout, V4L_STREAM_PACKET_FRAME_VIDEO_SIZE(buf.g_num_planes()) + tot_comp_size);
		write_u32(fout, V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_HDR);
		write_u32(fout, buf.g_field());
		write_u32(fout, buf.g_flags());
		comp_perc += (tot_comp_size * 100 / tot_used);
		comp_perc_count++;
	}
	if (to_with_hdr)
		write_u32(fout, FILE_HDR_ID);
	for (unsigned j = 0; j < buf.g_num_planes(); j++) {
		__u32 used = buf.g_bytesused(j);
		unsigned offset = buf.g_data_offset(j);
		unsigned sz;

		if (offset > used) {
			// Should never happen
			fprintf(stderr, "offset %d > used %d!\n",
				offset, used);
			offset = 0;
		}
		used -= offset;
		if (host_fd_to >= 0) {
			write_u32(fout, V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_PLANE_HDR);
			write_u32(fout, used);
			write_u32(fout, comp_size[j]);
			used = comp_size[j];
		} else if (to_with_hdr) {
			write_u32(fout, used);
		}
		if (host_fd_to >= 0)
			sz = fwrite(comp_ptr[j] + offset, 1, used, fout);
		else if (codec_type != NOT_CODEC && support_cap_compose &&
			 v4l2_fwht_find_pixfmt(fmt.g_pixelformat()))
			read_write_padded_frame(fmt, static_cast<u8 *>(q.g_dataptr(buf.g_index(), j)) + offset,
						fout, sz, used, used, false);
		else
			sz = fwrite(static_cast<u8 *>(q.g_dataptr(buf.g_index(), j)) + offset, 1, used, fout);

		if (sz != used)
			fprintf(stderr, "%u != %u\n", sz, used);
	}
	if (host_fd_to >= 0)
		fflush(fout);
#endif
}

static int do_handle_cap(cv4l_fd &fd, cv4l_queue &q, FILE *fout, int *index,
			 unsigned &count, fps_timestamps &fps_ts, cv4l_fmt &fmt,
			 bool ignore_count_skip)
{
	char ch = '<';
	int ret;
	cv4l_buffer buf(q);

	for (;;) {
		ret = fd.dqbuf(buf);
		if (ret == EAGAIN)
			return 0;
		if (ret == EPIPE)
			return QUEUE_STOPPED;
		if (ret) {
			fprintf(stderr, "%s: failed: %s\n", "VIDIOC_DQBUF", strerror(errno));
			return QUEUE_ERROR;
		}
		if (buf.g_flags() & V4L2_BUF_FLAG_LAST) {
			last_buffer = true;
			break;
		}
		if (!(buf.g_flags() & V4L2_BUF_FLAG_ERROR))
			break;
		if (verbose)
			print_concise_buffer(stderr, buf, fmt, q, fps_ts, -1);
		if (fd.qbuf(buf))
			return QUEUE_ERROR;
	}

	bool is_empty_frame = !buf.g_bytesused(0);
	bool is_error_frame = buf.g_flags() & V4L2_BUF_FLAG_ERROR;

	double ts_secs = buf.g_timestamp().tv_sec + buf.g_timestamp().tv_usec / 1000000.0;
	fps_ts.add_ts(ts_secs, buf.g_sequence(), buf.g_field());

	if (fout && (!stream_skip || ignore_count_skip) &&
	    !is_empty_frame && !is_error_frame)
		write_buffer_to_file(fd, q, buf, fmt, fout);

	if (buf.g_flags() & V4L2_BUF_FLAG_KEYFRAME)
		ch = 'K';
	else if (buf.g_flags() & V4L2_BUF_FLAG_PFRAME)
		ch = 'P';
	else if (buf.g_flags() & V4L2_BUF_FLAG_BFRAME)
		ch = 'B';
	if (verbose) {
		print_concise_buffer(stderr, buf, fmt, q, fps_ts,
				     host_fd_to >= 0 ? 100 - comp_perc / comp_perc_count : -1);
		comp_perc_count = comp_perc = 0;
	}
	if (!last_buffer && index == nullptr) {
		/*
		 * EINVAL in qbuf can happen if this is the last buffer before
		 * a dynamic resolution change sequence. In this case the buffer
		 * has the size that fits the old resolution and might not
		 * fit to the new one.
		 */
		if (fd.qbuf(buf) && errno != EINVAL) {
			fprintf(stderr, "%s: qbuf error\n", __func__);
			return QUEUE_ERROR;
		}
	}
	if (index)
		*index = buf.g_index();

	if (!verbose) {
		fprintf(stderr, "%c", ch);
		fflush(stderr);

		if (fps_ts.has_fps()) {
			unsigned dropped = fps_ts.dropped();

			fprintf(stderr, " %.02f fps", fps_ts.fps());
			if (dropped)
				fprintf(stderr, ", dropped buffers: %u", dropped);
			if (host_fd_to >= 0)
				fprintf(stderr, " %d%% compression", 100 - comp_perc / comp_perc_count);
			comp_perc_count = comp_perc = 0;
			fprintf(stderr, "\n");
		}
	}
	count++;

	if (ignore_count_skip)
		return 0;

	if (stream_sleep > 0 && count % stream_sleep == 0)
		sleep(1);

	if (is_empty_frame || is_error_frame)
		return 0;

	if (stream_skip) {
		stream_skip--;
		return 0;
	}

	if (stream_count == 0)
		return 0;

	if (--stream_count == 0)
		return QUEUE_STOPPED;

	return 0;
}

static int do_handle_out(cv4l_fd &fd, cv4l_queue &q, FILE *fin, cv4l_buffer *cap,
			 unsigned &count, fps_timestamps &fps_ts, cv4l_fmt fmt,
			 bool stopped, bool ignore_count_skip)
{
	cv4l_buffer buf(q);
	bool is_meta = q.g_type() == V4L2_BUF_TYPE_META_OUTPUT;
	int ret = 0;

	if (cap) {
		buf.s_index(cap->g_index());

		if (fd.querybuf(buf)) {
			fprintf(stderr, "%s fd.querybuf failed\n", __func__);
			return QUEUE_ERROR;
		}

		for (unsigned j = 0; j < buf.g_num_planes(); j++) {
			unsigned data_offset = cap->g_data_offset(j);

			if (q.g_memory() == V4L2_MEMORY_USERPTR) {
				buf.s_userptr(static_cast<u8 *>(cap->g_userptr(j)) + data_offset, j);
				buf.s_bytesused(cap->g_bytesused(j) - data_offset, j);
				buf.s_data_offset(0, j);
			} else if (q.g_memory() == V4L2_MEMORY_DMABUF) {
				__u32 bytesused = cap->g_bytesused(j);
				/*
				 * bytesused comes from the exported cap buffer
				 * but the length of the out buffer might be smaller
				 * so take the smaller of the two
				 */
				if (bytesused > buf.g_length(j))
					bytesused = buf.g_length(j);
				buf.s_bytesused(bytesused, j);
				buf.s_data_offset(data_offset, j);
				buf.s_fd(q.g_fd(buf.g_index(), j));
			}
		}
	} else {
		ret = fd.dqbuf(buf);
		if (ret == EAGAIN)
			return 0;

		double ts_secs = buf.g_timestamp().tv_sec + buf.g_timestamp().tv_usec / 1000000.0;
		fps_ts.add_ts(ts_secs, buf.g_sequence(), buf.g_field());
		if (verbose)
			print_concise_buffer(stderr, buf, fmt, q, fps_ts, -1);

		for (unsigned j = 0; j < buf.g_num_planes(); j++)
			buf.s_bytesused(buf.g_length(j), j);
	}
	if (ret) {
		fprintf(stderr, "%s: failed: %s\n", "VIDIOC_DQBUF", strerror(ret));
		return QUEUE_ERROR;
	}
	if (fps_ts.has_fps()) {
		unsigned dropped = fps_ts.dropped();

		fprintf(stderr, " %.02f fps", fps_ts.fps());
		if (dropped)
			fprintf(stderr, ", dropped buffers: %u", dropped);
		fprintf(stderr, "\n");
	}
	if (stopped)
		return 0;

	buf.s_field(output_field);
	tpg_s_field(&tpg, output_field, output_field_alt);
	if (output_field_alt) {
		if (output_field == V4L2_FIELD_TOP)
			output_field = V4L2_FIELD_BOTTOM;
		else if (output_field == V4L2_FIELD_BOTTOM)
			output_field = V4L2_FIELD_TOP;
	}

	if (fin && !fill_buffer_from_file(fd, q, buf, fmt, fin))
		return QUEUE_STOPPED;

	if (!fin && stream_out_refresh) {
		for (unsigned j = 0; j < buf.g_num_planes(); j++)
			tpg_fillbuffer(&tpg, stream_out_std, j,
				       static_cast<u8 *>(q.g_dataptr(buf.g_index(), j)));
	}
	if (is_meta)
		meta_fillbuffer(buf, fmt, q);

	if (fmt.g_pixelformat() == V4L2_PIX_FMT_FWHT_STATELESS) {
		if (ioctl(buf.g_request_fd(), MEDIA_REQUEST_IOC_REINIT, NULL)) {
			fprintf(stderr, "Unable to reinit media request: %s\n",
				strerror(errno));
			return QUEUE_ERROR;
		}

		if (set_fwht_ext_ctrl(fd, &last_fwht_hdr, last_fwht_bf_ts,
				      buf.g_request_fd())) {
			fprintf(stderr, "%s: set_fwht_ext_ctrl failed: %s\n",
				__func__, strerror(errno));
			return QUEUE_ERROR;
		}
	}

	set_time_stamp(buf);

	if (fd.qbuf(buf)) {
		fprintf(stderr, "%s: failed: %s\n", "VIDIOC_QBUF", strerror(errno));
		return QUEUE_ERROR;
	}
	if (fmt.g_pixelformat() == V4L2_PIX_FMT_FWHT_STATELESS) {
		if (!set_fwht_req_by_fd(&last_fwht_hdr, buf.g_request_fd(), last_fwht_bf_ts,
					buf.g_timestamp_ns())) {
			fprintf(stderr, "%s: request for fd %d does not exist\n",
				__func__, buf.g_request_fd());
			return QUEUE_ERROR;
		}

		last_fwht_bf_ts = buf.g_timestamp_ns();
		if (ioctl(buf.g_request_fd(), MEDIA_REQUEST_IOC_QUEUE) < 0) {
			fprintf(stderr, "Unable to queue media request: %s\n",
				strerror(errno));
			return QUEUE_ERROR;
		}
	}

	tpg_update_mv_count(&tpg, V4L2_FIELD_HAS_T_OR_B(output_field));

	if (!verbose)
		fprintf(stderr, ">");
	fflush(stderr);

	count++;

	if (ignore_count_skip)
		return 0;

	if (stream_sleep > 0 && count % stream_sleep == 0)
		sleep(1);

	if (stream_skip) {
		stream_skip--;
		return 0;
	}

	if (stream_count == 0)
		return 0;
	if (--stream_count == 0)
		return QUEUE_STOPPED;

	return 0;
}

static int do_handle_out_to_in(cv4l_fd &out_fd, cv4l_fd &fd, cv4l_queue &out, cv4l_queue &in)
{
	cv4l_buffer buf(out);
	int ret;

	do {
		ret = out_fd.dqbuf(buf);
	} while (ret == EAGAIN);
	if (ret) {
		fprintf(stderr, "%s: failed: %s\n", "VIDIOC_DQBUF", strerror(errno));
		return QUEUE_ERROR;
	}
	buf.init(in, buf.g_index());
	ret = fd.querybuf(buf);
	if (ret == 0)
		ret = fd.qbuf(buf);
	if (ret) {
		fprintf(stderr, "%s: failed: %s\n", "VIDIOC_QBUF", strerror(errno));
		return QUEUE_ERROR;
	}
	return 0;
}

static FILE *open_output_file(cv4l_fd &fd)
{
	FILE *fout = nullptr;

#ifndef NO_STREAM_TO
	if (file_to) {
		if (!strcmp(file_to, "-"))
			return stdout;
		fout = fopen(file_to, "w+");
		if (!fout)
			fprintf(stderr, "could not open %s for writing\n", file_to);
		return fout;
	}
	if (!host_to)
		return nullptr;

	char *p = std::strchr(host_to, ':');
	struct sockaddr_in serv_addr;
	struct hostent *server;
	struct v4l2_fract aspect;
	unsigned width, height;
	cv4l_fmt cfmt;

	fd.g_fmt(cfmt);

	aspect = fd.g_pixel_aspect(width, height);
	if (p) {
		host_port_to = strtoul(p + 1, nullptr, 0);
		*p = '\0';
	}
	host_fd_to = socket(AF_INET, SOCK_STREAM, 0);
	if (host_fd_to < 0) {
		fprintf(stderr, "cannot open socket");
		std::exit(EXIT_SUCCESS);
	}
	server = gethostbyname(host_to);
	if (server == nullptr) {
		fprintf(stderr, "no such host %s\n", host_to);
		std::exit(EXIT_SUCCESS);
	}
	memset(reinterpret_cast<char *>(&serv_addr), 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy(reinterpret_cast<char *>(&serv_addr.sin_addr.s_addr),
	       server->h_addr,
	       server->h_length);
	serv_addr.sin_port = htons(host_port_to);
	if (connect(host_fd_to, reinterpret_cast<struct sockaddr *>(&serv_addr), sizeof(serv_addr)) < 0) {
		fprintf(stderr, "could not connect\n");
		std::exit(EXIT_SUCCESS);
	}
	fout = fdopen(host_fd_to, "a");
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
	write_u32(fout, aspect.numerator);
	write_u32(fout, aspect.denominator);
	for (unsigned i = 0; i < cfmt.g_num_planes(); i++) {
		write_u32(fout, V4L_STREAM_PACKET_FMT_VIDEO_SIZE_FMT_PLANE);
		write_u32(fout, cfmt.g_sizeimage(i));
		write_u32(fout, cfmt.g_bytesperline(i));
		bpl_cap[i] = rle_calc_bpl(cfmt.g_bytesperline(i), cfmt.g_pixelformat());
	}
	if (!host_lossless) {
		unsigned visible_width = support_cap_compose ? composed_width : cfmt.g_width();
		unsigned visible_height = support_cap_compose ? composed_height : cfmt.g_height();

		ctx = fwht_alloc(cfmt.g_pixelformat(), visible_width, visible_height,
				 cfmt.g_width(), cfmt.g_height(),
				 cfmt.g_field(), cfmt.g_colorspace(), cfmt.g_xfer_func(),
				 cfmt.g_ycbcr_enc(), cfmt.g_quantization());
	}
	fflush(fout);
#endif
	return fout;
}

static void streaming_set_cap(cv4l_fd &fd, cv4l_fd &exp_fd)
{
	int fd_flags = fcntl(fd.g_fd(), F_GETFL);
	cv4l_queue q(fd.g_type(), memory);
	cv4l_queue exp_q(exp_fd.g_type(), V4L2_MEMORY_MMAP);
	fps_timestamps fps_ts;
	bool use_poll = options[OptStreamPoll];
	unsigned count;
	bool eos;
	bool source_change;
	FILE *fout = nullptr;
	cv4l_fmt fmt;

	if (!(capabilities & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			      V4L2_CAP_VBI_CAPTURE | V4L2_CAP_SLICED_VBI_CAPTURE |
			      V4L2_CAP_META_CAPTURE | V4L2_CAP_SDR_CAPTURE |
			      V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE))) {
		fprintf(stderr, "unsupported stream type\n");
		return;
	}
	if (options[OptStreamDmaBuf] && exp_fd.g_fd() < 0) {
		fprintf(stderr, "--stream-dmabuf can only work in combination with --export-device\n");
		return;
	}
	switch (q.g_type()) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		break;
	default:
		if (host_to) {
			fprintf(stderr, "--stream-to-host is not supported for non-video streams\n");
			return;
		}
		break;
	}

	subscribe_event(fd, V4L2_EVENT_EOS);
	if (use_poll)
		subscribe_event(fd, V4L2_EVENT_SOURCE_CHANGE);

recover:
	eos = false;
	source_change = false;
	count = 0;

	if (!stream_no_query) {
		cv4l_disable_trace dt(fd);
		struct v4l2_dv_timings new_dv_timings = {};
		v4l2_std_id new_std;
		struct v4l2_input in = { };

		if (!fd.g_input(in.index) && !fd.enum_input(in, true, in.index)) {
			if (in.capabilities & V4L2_IN_CAP_DV_TIMINGS) {
				while (fd.query_dv_timings(new_dv_timings))
					sleep(1);
				fd.s_dv_timings(new_dv_timings);
				fprintf(stderr, "New timings found\n");
			} else if (in.capabilities & V4L2_IN_CAP_STD) {
				if (!fd.query_std(new_std))
					fd.s_std(new_std);
			}
		}
	}

	fout = open_output_file(fd);

	if (q.reqbufs(&fd, reqbufs_count_cap)) {
		if (q.g_type() != V4L2_BUF_TYPE_VBI_CAPTURE ||
		    !fd.has_raw_vbi_cap() || !fd.has_sliced_vbi_cap())
			goto done;
		fd.s_type(V4L2_BUF_TYPE_SLICED_VBI_CAPTURE);
		q.init(fd.g_type(), memory);
		if (q.reqbufs(&fd, reqbufs_count_cap))
			goto done;
	}

	if (options[OptStreamDmaBuf]) {
		if (exp_q.reqbufs(&exp_fd, reqbufs_count_cap))
			goto done;
		if (q.export_bufs(&exp_fd, exp_fd.g_type()))
			goto done;
	}

	if (q.obtain_bufs(&fd))
		goto done;

	if (q.queue_all(&fd))
		goto done;

	fps_ts.determine_field(fd.g_fd(), q.g_type());

	if (fd.streamon())
		goto done;

	fd.s_trace(0);
	exp_fd.s_trace(0);

	fd.g_fmt(fmt);

	while (stream_sleep == 0)
		sleep(100);

	if (use_poll)
		fcntl(fd.g_fd(), F_SETFL, fd_flags | O_NONBLOCK);

	while (!eos && !source_change) {
		fd_set read_fds;
		fd_set exception_fds;
		struct timeval tv = { use_poll ? 2 : 0, 0 };
		int r;

		FD_ZERO(&exception_fds);
		FD_SET(fd.g_fd(), &exception_fds);
		FD_ZERO(&read_fds);
		FD_SET(fd.g_fd(), &read_fds);
		r = select(fd.g_fd() + 1, use_poll ? &read_fds : nullptr, nullptr, &exception_fds, &tv);

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

		if (FD_ISSET(fd.g_fd(), &exception_fds)) {
			struct v4l2_event ev;

			while (!fd.dqevent(ev)) {
				switch (ev.type) {
				case V4L2_EVENT_SOURCE_CHANGE:
					source_change = true;
					if (!verbose)
						fprintf(stderr, "\n");
					fprintf(stderr, "SOURCE CHANGE EVENT\n");
					break;
				case V4L2_EVENT_EOS:
					eos = true;
					if (!verbose)
						fprintf(stderr, "\n");
					fprintf(stderr, "EOS EVENT\n");
					fflush(stderr);
					break;
				}
			}
		}

		if (FD_ISSET(fd.g_fd(), &read_fds)) {
			r = do_handle_cap(fd, q, fout, nullptr,
					  count, fps_ts, fmt, false);
			if (r < 0)
				break;
		}

	}
	fd.streamoff();
	fcntl(fd.g_fd(), F_SETFL, fd_flags);
	fprintf(stderr, "\n");

	q.free(&fd);
	tpg_free(&tpg);
	if (source_change && !stream_no_query)
		goto recover;

done:
	if (options[OptStreamDmaBuf])
		exp_q.close_exported_fds();
	if (fout && fout != stdout) {
		if (host_fd_to >= 0)
			write_u32(fout, V4L_STREAM_PACKET_END);
		fclose(fout);
	}
}

static FILE *open_input_file(cv4l_fd &fd, __u32 type)
{
	FILE *fin = nullptr;

	if (file_from) {
		if (!strcmp(file_from, "-"))
			return stdin;
		fin = fopen(file_from, "r");
		if (!fin)
			fprintf(stderr, "could not open %s for reading\n", file_from);
		return fin;
	}
	if (!host_from)
		return nullptr;

	char *p = std::strchr(host_from, ':');
	int listen_fd;
	socklen_t clilen;
	struct sockaddr_in serv_addr = {}, cli_addr;

	if (p) {
		host_port_from = strtoul(p + 1, nullptr, 0);
		*p = '\0';
	}
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		fprintf(stderr, "could not opening socket\n");
		std::exit(EXIT_FAILURE);
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(host_port_from);
	if (bind(listen_fd, reinterpret_cast<struct sockaddr *>(&serv_addr), sizeof(serv_addr)) < 0) {
		fprintf(stderr, "could not bind\n");
		std::exit(EXIT_FAILURE);
	}
	listen(listen_fd, 1);
	clilen = sizeof(cli_addr);
	host_fd_from = accept(listen_fd, reinterpret_cast<struct sockaddr *>(&cli_addr), &clilen);
	if (host_fd_from < 0) {
		fprintf(stderr, "could not accept\n");
		std::exit(EXIT_FAILURE);
	}
	fin = fdopen(host_fd_from, "r");
	if (read_u32(fin) != V4L_STREAM_ID) {
		fprintf(stderr, "unknown protocol ID\n");
		std::exit(EXIT_FAILURE);
	}
	if (read_u32(fin) != V4L_STREAM_VERSION) {
		fprintf(stderr, "unknown protocol version\n");
		std::exit(EXIT_FAILURE);
	}
	for (;;) {
		__u32 packet = read_u32(fin);
		char buf[1024];

		if (packet == V4L_STREAM_PACKET_END) {
			fprintf(stderr, "END packet read\n");
			std::exit(EXIT_FAILURE);
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
				std::exit(EXIT_FAILURE);
			}
			sz -= n;
		}
	}
	read_u32(fin);

	cv4l_fmt cfmt(type);

	unsigned sz = read_u32(fin);

	if (sz != V4L_STREAM_PACKET_FMT_VIDEO_SIZE_FMT) {
		fprintf(stderr, "unsupported FMT_VIDEO size\n");
		std::exit(EXIT_FAILURE);
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
	unsigned visible_width = support_out_crop ? cropped_width : cfmt.g_width();
	unsigned visible_height = support_out_crop ? cropped_height : cfmt.g_height();

	ctx = fwht_alloc(cfmt.g_pixelformat(), visible_width, visible_height,
			 cfmt.g_width(), cfmt.g_height(),
			 cfmt.g_field(), cfmt.g_colorspace(), cfmt.g_xfer_func(),
			 cfmt.g_ycbcr_enc(), cfmt.g_quantization());

	read_u32(fin); // pixelaspect.numerator
	read_u32(fin); // pixelaspect.denominator

	for (unsigned i = 0; i < cfmt.g_num_planes(); i++) {
		unsigned sz = read_u32(fin);

		if (sz != V4L_STREAM_PACKET_FMT_VIDEO_SIZE_FMT_PLANE) {
			fprintf(stderr, "unsupported FMT_VIDEO plane size\n");
			std::exit(EXIT_FAILURE);
		}
		cfmt.s_sizeimage(read_u32(fin), i);
		cfmt.s_bytesperline(read_u32(fin), i);
		bpl_out[i] = rle_calc_bpl(cfmt.g_bytesperline(i), cfmt.g_pixelformat());
	}
	if (fd.s_fmt(cfmt)) {
		fprintf(stderr, "failed to set new format\n");
		std::exit(EXIT_FAILURE);
	}
	return fin;
}

static void streaming_set_out(cv4l_fd &fd, cv4l_fd &exp_fd)
{
	__u32 type = fd.has_vid_m2m() ? v4l_type_invert(fd.g_type()) : fd.g_type();
	cv4l_queue q(type, out_memory);
	cv4l_queue exp_q(exp_fd.g_type(), V4L2_MEMORY_MMAP);
	int fd_flags = fcntl(fd.g_fd(), F_GETFL);
	bool use_poll = options[OptStreamPoll];
	fps_timestamps fps_ts;
	unsigned count = 0;
	bool stopped = false;
	FILE *fin = nullptr;
	cv4l_fmt fmt;

	fd.g_fmt(fmt);

	if (!(capabilities & (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			      V4L2_CAP_VBI_OUTPUT | V4L2_CAP_SLICED_VBI_OUTPUT |
			      V4L2_CAP_SDR_OUTPUT | V4L2_CAP_META_OUTPUT |
			      V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE))) {
		fprintf(stderr, "unsupported stream type\n");
		return;
	}
	if (options[OptStreamOutDmaBuf] && exp_fd.g_fd() < 0) {
		fprintf(stderr, "--stream-out-dmabuf can only work in combination with --export-device\n");
		return;
	}
	switch (q.g_type()) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		break;
	default:
		if (host_from) {
			fprintf(stderr, "--stream-from-host is not supported for non-video streams\n");
			return;
		}
		break;
	}

	fin = open_input_file(fd, type);

	if (q.reqbufs(&fd, reqbufs_count_out)) {
		if (q.g_type() != V4L2_BUF_TYPE_VBI_OUTPUT ||
		    !fd.has_raw_vbi_out() || !fd.has_sliced_vbi_out())
			goto done;
		fd.s_type(V4L2_BUF_TYPE_SLICED_VBI_OUTPUT);
		q.init(fd.g_type(), memory);
		if (q.reqbufs(&fd, reqbufs_count_out))
			goto done;
	}

	if (options[OptStreamOutDmaBuf]) {
		if (exp_q.reqbufs(&exp_fd, reqbufs_count_out))
			goto done;
		if (q.export_bufs(&exp_fd, exp_fd.g_type()))
			goto done;
	}

	if (q.obtain_bufs(&fd))
		goto done;

	if (do_setup_out_buffers(fd, q, fin, true, false) < 0)
		goto done;

	fps_ts.determine_field(fd.g_fd(), type);

	if (fd.streamon())
		goto done;

	fd.s_trace(0);
	exp_fd.s_trace(0);

	while (stream_sleep == 0)
		sleep(100);

	if (use_poll)
		fcntl(fd.g_fd(), F_SETFL, fd_flags | O_NONBLOCK);

	for (;;) {
		int r;

		if (use_poll) {
			fd_set fds;
			struct timeval tv;

			FD_ZERO(&fds);
			FD_SET(fd.g_fd(), &fds);

			/* Timeout. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(fd.g_fd() + 1, nullptr, &fds, nullptr, &tv);

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
		r = do_handle_out(fd, q, fin, nullptr,
				  count, fps_ts, fmt, stopped, false);
		if (r == QUEUE_STOPPED)
			stopped = true;
		if (r < 0)
			break;

	}

	if (options[OptDecoderCmd]) {
		fd.decoder_cmd(dec_cmd);
		options[OptDecoderCmd] = false;
	}
	fd.streamoff();
	fcntl(fd.g_fd(), F_SETFL, fd_flags);
	fprintf(stderr, "\n");

	q.free(&fd);
	tpg_free(&tpg);

done:
	if (options[OptStreamOutDmaBuf])
		exp_q.close_exported_fds();
	if (fin && fin != stdin)
		fclose(fin);
}

enum stream_type {
	CAP,
	OUT,
};

static int capture_setup(cv4l_fd &fd, cv4l_queue &in, cv4l_fd *exp_fd, cv4l_fmt *new_fmt = nullptr)
{
	if (fd.streamoff(in.g_type())) {
		fprintf(stderr, "%s: fd.streamoff error\n", __func__);
		return -1;
	}

	/* release any buffer allocated */
	if (in.reqbufs(&fd)) {
		fprintf(stderr, "%s: in.reqbufs 0 error\n", __func__);
		return -1;
	}

	if (options[OptSetVideoFormat]) {
		cv4l_fmt fmt;

		if (vidcap_get_and_update_fmt(fd, fmt)) {
			fprintf(stderr, "%s: vidcap_get_and_update_fmt error\n",
				__func__);
			return -1;
		}
		fd.s_fmt(fmt, in.g_type());
		if (new_fmt)
			*new_fmt = fmt;
	} else if (new_fmt) {
		fd.g_fmt(*new_fmt, in.g_type());
	}
	get_cap_compose_rect(fd);

	if (in.reqbufs(&fd, reqbufs_count_cap)) {
		fprintf(stderr, "%s: in.reqbufs %u error\n", __func__,
			reqbufs_count_cap);
		return -1;
	}

	if (exp_fd && in.export_bufs(exp_fd, exp_fd->g_type()))
		return -1;
	if (in.obtain_bufs(&fd) || in.queue_all(&fd)) {
		fprintf(stderr, "%s: in.obtain_bufs error\n", __func__);
		return -1;
	}

	if (fd.streamon(in.g_type())) {
		fprintf(stderr, "%s: fd.streamon error\n", __func__);
		return -1;
	}
	return 0;
}

static void stateful_m2m(cv4l_fd &fd, cv4l_queue &in, cv4l_queue &out,
			 FILE *fin, FILE *fout, cv4l_fmt &fmt_in,
			 const cv4l_fmt &fmt_out, cv4l_fd *exp_fd_p)
{
	int fd_flags = fcntl(fd.g_fd(), F_GETFL);
	fps_timestamps fps_ts[2];
	unsigned count[2] = { 0, 0 };
	fd_set fds[3];
	fd_set *rd_fds = &fds[0]; /* for capture */
	fd_set *ex_fds = &fds[1]; /* for capture */
	fd_set *wr_fds = &fds[2]; /* for output */
	bool cap_streaming = false;
	static struct v4l2_encoder_cmd enc_stop = {
		.cmd = V4L2_ENC_CMD_STOP,
	};
	static struct v4l2_decoder_cmd dec_stop = {
		.cmd = V4L2_DEC_CMD_STOP,
	};

	bool have_eos = subscribe_event(fd, V4L2_EVENT_EOS);
	bool is_encoder = false;
	bool ignore_count_skip = codec_type == ENCODER;

	if (have_eos) {
		cv4l_fmt fmt(in.g_type());

		fd.g_fmt(fmt);
		is_encoder = !fmt.g_bytesperline();
	}

	bool have_source_change = subscribe_event(fd, V4L2_EVENT_SOURCE_CHANGE);
	bool stopped = false;

	if (out.reqbufs(&fd, reqbufs_count_out))
		return;

	switch (do_setup_out_buffers(fd, out, fout, true, !ignore_count_skip)) {
	case 0:
		break;
	case QUEUE_STOPPED:
		stopped = true;
		break;
	default:
		return;
	}

	if (fd.streamon(out.g_type()))
		return;

	fd.s_trace(0);
	if (exp_fd_p)
		exp_fd_p->s_trace(0);

	if (codec_type != DECODER || !have_source_change)
		if (capture_setup(fd, in, exp_fd_p))
			return;

	fps_ts[CAP].determine_field(fd.g_fd(), in.g_type());
	fps_ts[OUT].determine_field(fd.g_fd(), out.g_type());

	while (stream_sleep == 0)
		sleep(100);

	fcntl(fd.g_fd(), F_SETFL, fd_flags | O_NONBLOCK);

	if (have_eos && stopped) {
		if (!verbose)
			fprintf(stderr, "\n");
		fprintf(stderr, "STOP %sCODER\n", is_encoder ? "EN" : "DE");
		if (is_encoder)
			fd.encoder_cmd(enc_stop);
		else
			fd.decoder_cmd(dec_stop);
	}

	while (rd_fds || wr_fds || ex_fds) {
		struct timeval tv = { 2, 0 };
		struct timeval stopped_tv = { 0, 500000 };
		int r = 0;

		if (rd_fds) {
			FD_ZERO(rd_fds);
			FD_SET(fd.g_fd(), rd_fds);
		}

		if (ex_fds) {
			FD_ZERO(ex_fds);
			FD_SET(fd.g_fd(), ex_fds);
		}

		if (wr_fds) {
			FD_ZERO(wr_fds);
			FD_SET(fd.g_fd(), wr_fds);
		}

		r = select(fd.g_fd() + 1, rd_fds, wr_fds, ex_fds,
			   stopped ? &stopped_tv : &tv);

		if (r == -1) {
			if (EINTR == errno)
				continue;
			fprintf(stderr, "select error: %s\n",
					strerror(errno));
			return;
		}
		if (r == 0) {
			if (!stopped)
				fprintf(stderr, "select timeout");
			fprintf(stderr, "\n");
			return;
		}

		if (rd_fds && FD_ISSET(fd.g_fd(), rd_fds)) {
			r = do_handle_cap(fd, in, fin, nullptr,
					  count[CAP], fps_ts[CAP], fmt_in,
					  ignore_count_skip);
			if (r == QUEUE_STOPPED)
				break;
			if (r < 0) {
				rd_fds = nullptr;
				if (!have_eos) {
					ex_fds = nullptr;
					break;
				}
			}
		}

		if (wr_fds && FD_ISSET(fd.g_fd(), wr_fds)) {
			r = do_handle_out(fd, out, fout, nullptr,
					  count[OUT], fps_ts[OUT], fmt_out, stopped,
					  !ignore_count_skip);
			if (r == QUEUE_STOPPED) {
				stopped = true;
				if (have_eos) {
					if (!verbose)
						fprintf(stderr, "\n");
					fprintf(stderr, "STOP %sCODER\n", is_encoder ? "EN" : "DE");
					if (is_encoder)
						fd.encoder_cmd(enc_stop);
					else
						fd.decoder_cmd(dec_stop);
				}
			} else if (r < 0) {
				break;
			}
		}

		if (ex_fds && FD_ISSET(fd.g_fd(), ex_fds)) {
			struct v4l2_event ev;

			while (!fd.dqevent(ev)) {
				if (ev.type == V4L2_EVENT_EOS) {
					wr_fds = nullptr;
					if (!verbose)
						fprintf(stderr, "\n");
					fprintf(stderr, "EOS EVENT\n");
					fflush(stderr);
				} else if (ev.type == V4L2_EVENT_SOURCE_CHANGE) {
					if (!verbose)
						fprintf(stderr, "\n");
					fprintf(stderr, "SOURCE CHANGE EVENT\n");
					in_source_change_event = true;

					/*
					 * if capture is not streaming, the
					 * driver will not send a last buffer so
					 * we set it here
					 */
					if (!cap_streaming)
						last_buffer = true;
				}
			}
		}

		if (last_buffer) {
			if (in_source_change_event) {
				in_source_change_event = false;
				last_buffer = false;
				if (capture_setup(fd, in, exp_fd_p, &fmt_in))
					return;
				fps_ts[CAP].reset();
				fps_ts[OUT].reset();
				cap_streaming = true;
			} else {
				break;
			}
		}
	}

	fcntl(fd.g_fd(), F_SETFL, fd_flags);
	fprintf(stderr, "\n");

	fd.streamoff(in.g_type());
	fd.streamoff(out.g_type());
	in.free(&fd);
	out.free(&fd);
	tpg_free(&tpg);
}

static void stateless_m2m(cv4l_fd &fd, cv4l_queue &in, cv4l_queue &out,
			  FILE *fin, FILE *fout, cv4l_fmt &fmt_in,
			  const cv4l_fmt &fmt_out, cv4l_fd *exp_fd_p)
{
	fps_timestamps fps_ts[2];
	unsigned count[2] = { 0, 0 };
	int fd_flags = fcntl(fd.g_fd(), F_GETFL);
	bool stopped = false;

	if (out.reqbufs(&fd, reqbufs_count_out)) {
		fprintf(stderr, "%s: out.reqbufs failed\n", __func__);
		return;
	}

	if (in.reqbufs(&fd, reqbufs_count_cap)) {
		fprintf(stderr, "%s: in.reqbufs failed\n", __func__);
		return;
	}

	if (exp_fd_p && in.export_bufs(exp_fd_p, exp_fd_p->g_type()))
		return;

	if (in.obtain_bufs(&fd)) {
		fprintf(stderr, "%s: in.obtain_bufs error\n", __func__);
		return;
	}

	if (do_setup_out_buffers(fd, out, fout, true, true) == QUEUE_ERROR) {
		fprintf(stderr, "%s: do_setup_out_buffers failed\n", __func__);
		return;
	}

	if (in.queue_all(&fd)) {
		fprintf(stderr, "%s: in.queue_all failed\n", __func__);
		return;
	}

	if (fd.streamon(out.g_type())) {
		fprintf(stderr, "%s: streamon for out failed\n", __func__);
		return;
	}

	if (fd.streamon(in.g_type())) {
		fprintf(stderr, "%s: streamon for in failed\n", __func__);
		return;
	}
	int index = 0;
	bool queue_lst_buf = false;
	cv4l_buffer last_in_buf;

	fcntl(fd.g_fd(), F_SETFL, fd_flags | O_NONBLOCK);

	while (true) {
		fd_set except_fds;
		int req_fd = fwht_reqs[index].fd;
		struct timeval tv = { 2, 0 };

		if (req_fd < 0)
			break;

		FD_ZERO(&except_fds);
		FD_SET(req_fd, &except_fds);

		int rc = select(req_fd + 1, nullptr, nullptr, &except_fds, &tv);

		if (rc == 0) {
			fprintf(stderr, "Timeout when waiting for media request\n");
			return;
		}
		if (rc < 0) {
			fprintf(stderr, "Unable to select media request: %s\n",
				strerror(errno));
			return;
		}
		/*
		 * it is safe to queue back last cap buffer only after
		 * the following request is done so that the buffer
		 * is not needed anymore as a reference frame
		 */
		if (queue_lst_buf) {
			if (fd.qbuf(last_in_buf)) {
				fprintf(stderr, "%s: qbuf failed\n", __func__);
				return;
			}
		}
		int buf_idx = -1;
		/*
		 * fin is not sent to do_handle_cap since the capture buf is
		 * written to the file in current function
		 */
		rc = do_handle_cap(fd, in, nullptr, &buf_idx, count[CAP],
				   fps_ts[CAP], fmt_in, false);
		if (rc && rc != QUEUE_STOPPED) {
			fprintf(stderr, "%s: do_handle_cap err\n", __func__);
			return;
		}
		/*
		 * in case of an error in the frame, set last ts to 0 as a
		 * means to recover so that next request will not use a
		 * reference buffer. Otherwise the error flag will be set to
		 * all the future capture buffers.
		 */
		if (buf_idx == -1) {
			fprintf(stderr, "%s: frame returned with error\n", __func__);
			last_fwht_bf_ts	= 0;
		} else {
			cv4l_buffer cap_buf(in, index);
			if (fd.querybuf(cap_buf))
				return;
			last_in_buf = cap_buf;
			queue_lst_buf = true;
			if (fin && cap_buf.g_bytesused(0) &&
			    !(cap_buf.g_flags() & V4L2_BUF_FLAG_ERROR)) {
				int idx = get_fwht_req_by_ts(cap_buf.g_timestamp_ns());

				if (idx < 0) {
					fprintf(stderr, "%s: could not find request from buffer\n", __func__);
					fprintf(stderr, "%s: ts = %llu\n", __func__, cap_buf.g_timestamp_ns());
					return;
				}
				composed_width = fwht_reqs[idx].params.width;
				composed_height = fwht_reqs[idx].params.height;
				write_buffer_to_file(fd, in, cap_buf,
						     fmt_in, fin);
			}
		}
		if (rc == QUEUE_STOPPED)
			return;

		if (!stopped) {
			rc = do_handle_out(fd, out, fout, nullptr, count[OUT],
					   fps_ts[OUT], fmt_out, false, true);
			if (rc) {
				stopped = true;
				if (rc != QUEUE_STOPPED)
					fprintf(stderr, "%s: output stream ended\n", __func__);
				close(req_fd);
				fwht_reqs[index].fd = -1;
			}
		}
		index = (index + 1) % out.g_buffers();
	}

	fcntl(fd.g_fd(), F_SETFL, fd_flags);
	fprintf(stderr, "\n");

	fd.streamoff(in.g_type());
	fd.streamoff(out.g_type());
	in.free(&fd);
	out.free(&fd);
	tpg_free(&tpg);
}

static void streaming_set_m2m(cv4l_fd &fd, cv4l_fd &exp_fd)
{
	cv4l_queue in(fd.g_type(), memory);
	cv4l_queue out(v4l_type_invert(fd.g_type()), out_memory);
	cv4l_queue exp_q(exp_fd.g_type(), V4L2_MEMORY_MMAP);
	cv4l_fd *exp_fd_p = nullptr;
	FILE *file[2] = {nullptr, nullptr};
	cv4l_fmt fmt[2];

	fd.g_fmt(fmt[OUT], out.g_type());
	fd.g_fmt(fmt[CAP], in.g_type());

	if (!fd.has_vid_m2m()) {
		fprintf(stderr, "unsupported m2m stream type\n");
		return;
	}
	if (options[OptStreamDmaBuf] && options[OptStreamOutDmaBuf]) {
		fprintf(stderr, "--stream-dmabuf and --stream-out-dmabuf not supported for m2m devices\n");
		return;
	}
	if ((options[OptStreamDmaBuf] || options[OptStreamOutDmaBuf]) && exp_fd.g_fd() < 0) {
		fprintf(stderr, "--stream-dmabuf or --stream-out-dmabuf can only work in combination with --export-device\n");
		return;
	}

	file[CAP] = open_output_file(fd);
	file[OUT] = open_input_file(fd, out.g_type());

	if (options[OptStreamDmaBuf]) {
		if (exp_q.reqbufs(&exp_fd, reqbufs_count_cap))
			goto done;
		exp_fd_p = &exp_fd;
	}

	if (options[OptStreamOutDmaBuf]) {
		if (exp_q.reqbufs(&exp_fd, reqbufs_count_out))
			goto done;
		if (out.export_bufs(&exp_fd, exp_fd.g_type()))
			goto done;
	}
	if (fmt[OUT].g_pixelformat() == V4L2_PIX_FMT_FWHT_STATELESS)
		stateless_m2m(fd, in, out, file[CAP], file[OUT], fmt[CAP], fmt[OUT], exp_fd_p);
	else
		stateful_m2m(fd, in, out, file[CAP], file[OUT], fmt[CAP], fmt[OUT], exp_fd_p);

done:
	if (options[OptStreamDmaBuf] || options[OptStreamOutDmaBuf])
		exp_q.close_exported_fds();

	if (file[CAP] && file[CAP] != stdout)
		fclose(file[CAP]);

	if (file[OUT] && file[OUT] != stdin)
		fclose(file[OUT]);
}

static void streaming_set_cap2out(cv4l_fd &fd, cv4l_fd &out_fd)
{
	int fd_flags = fcntl(fd.g_fd(), F_GETFL);
	bool use_poll = options[OptStreamPoll];
	bool use_dmabuf = options[OptStreamDmaBuf] || options[OptStreamOutDmaBuf];
	bool use_userptr = options[OptStreamUser] && options[OptStreamOutUser];
	__u32 out_type = out_fd.has_vid_m2m() ? v4l_type_invert(out_fd.g_type()) : out_fd.g_type();
	cv4l_queue in(fd.g_type(), memory);
	cv4l_queue out(out_type, out_memory);
	fps_timestamps fps_ts[2];
	unsigned count[2] = { 0, 0 };
	FILE *file[2] = {nullptr, nullptr};
	fd_set fds;
	unsigned cnt = 0;
	cv4l_fmt fmt[2];

	out_fd.g_fmt(fmt[OUT], out.g_type());
	fd.g_fmt(fmt[CAP], in.g_type());
	if (!(capabilities & (V4L2_CAP_VIDEO_CAPTURE |
			      V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			      V4L2_CAP_VIDEO_M2M |
			      V4L2_CAP_VIDEO_M2M_MPLANE))) {
		fprintf(stderr, "unsupported capture stream type\n");
		return;
	}
	if (!(out_capabilities & (V4L2_CAP_VIDEO_OUTPUT |
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

	if (file_to) {
		if (!strcmp(file_to, "-"))
			file[CAP] = stdout;
		else
			file[CAP] = fopen(file_to, "w+");
		if (!file[CAP]) {
			fprintf(stderr, "could not open %s for writing\n", file_to);
			return;
		}
	}

	if (file_from) {
		if (!strcmp(file_from, "-"))
			file[OUT] = stdin;
		else
			file[OUT] = fopen(file_from, "r");
		if (!file[OUT]) {
			fprintf(stderr, "could not open %s for reading\n", file_from);
			return;
		}
	}

	if (in.reqbufs(&fd, reqbufs_count_cap) ||
	    out.reqbufs(&out_fd, reqbufs_count_out))
		goto done;

	if (options[OptStreamDmaBuf]) {
		if (in.export_bufs(&out_fd, out.g_type()))
			goto done;
	} else if (options[OptStreamOutDmaBuf]) {
		if (out.export_bufs(&fd, in.g_type()))
			goto done;
	}

	if (in.g_num_planes() != out.g_num_planes()) {
		fprintf(stderr, "mismatch between number of planes\n");
		goto done;
	}
	if (in.obtain_bufs(&fd)) {
		fprintf(stderr, "%s: in.obtain_bufs failed\n", __func__);
		goto done;
	}

	if (in.queue_all(&fd)) {
		fprintf(stderr, "%s: in.queue_all failed\n", __func__);
		goto done;
	}


	if (do_setup_out_buffers(out_fd, out, file[OUT], false, false) == QUEUE_ERROR) {
		fprintf(stderr, "%s: do_setup_out_buffers failed\n", __func__);
		goto done;
	}

	fps_ts[CAP].determine_field(fd.g_fd(), in.g_type());
	fps_ts[OUT].determine_field(out_fd.g_fd(), out.g_type());

	if (fd.streamon() || out_fd.streamon())
		goto done;

	fd.s_trace(0);
	out_fd.s_trace(0);

	while (stream_sleep == 0)
		sleep(100);

	if (use_poll)
		fcntl(fd.g_fd(), F_SETFL, fd_flags | O_NONBLOCK);

	while (true) {
		struct timeval tv = { use_poll ? 2 : 0, 0 };
		int r = 0;

		FD_ZERO(&fds);
		FD_SET(fd.g_fd(), &fds);

		if (use_poll)
			r = select(fd.g_fd() + 1, &fds, nullptr, nullptr, &tv);

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

		if (FD_ISSET(fd.g_fd(), &fds)) {
			int index = -1;

			r = do_handle_cap(fd, in, file[CAP], &index,
					  count[CAP], fps_ts[CAP], fmt[CAP], true);
			if (r)
				fprintf(stderr, "handle cap %d\n", r);
			if (!r) {
				cv4l_buffer buf(in, index);

				if (fd.querybuf(buf))
					break;
				r = do_handle_out(out_fd, out, file[OUT], &buf,
						  count[OUT], fps_ts[OUT], fmt[OUT],
						  false, false);
			}
			if (r)
				fprintf(stderr, "handle out %d\n", r);
			if (!r && cnt++ > 1)
				r = do_handle_out_to_in(out_fd, fd, out, in);
			if (r)
				fprintf(stderr, "handle out2in %d\n", r);
			if (r < 0) {
				fd.streamoff();
				out_fd.streamoff();
				break;
			}
		}
	}

done:
	fcntl(fd.g_fd(), F_SETFL, fd_flags);
	fprintf(stderr, "\n");

	if (options[OptStreamDmaBuf])
		out.close_exported_fds();
	if (options[OptStreamOutDmaBuf])
		in.close_exported_fds();

	in.free(&fd);
	out.free(&out_fd);
	tpg_free(&tpg);

	if (file[CAP] && file[CAP] != stdout)
		fclose(file[CAP]);

	if (file[OUT] && file[OUT] != stdin)
		fclose(file[OUT]);
}

void streaming_set(cv4l_fd &fd, cv4l_fd &out_fd, cv4l_fd &exp_fd)
{
	int do_cap = options[OptStreamMmap] + options[OptStreamUser] + options[OptStreamDmaBuf];
	int do_out = options[OptStreamOutMmap] + options[OptStreamOutUser] + options[OptStreamOutDmaBuf];

	if (out_fd.g_fd() < 0) {
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

	unsigned int old_trace_fd = fd.g_trace();
	unsigned int old_trace_out_fd = out_fd.g_trace();
	unsigned int old_trace_exp_fd = exp_fd.g_trace();

	get_cap_compose_rect(fd);
	get_out_crop_rect(fd);
	get_codec_type(fd);

	if (do_cap && do_out && out_fd.g_fd() < 0)
		streaming_set_m2m(fd, exp_fd);
	else if (do_cap && do_out)
		streaming_set_cap2out(fd, out_fd);
	else if (do_cap)
		streaming_set_cap(fd, exp_fd);
	else if (do_out)
		streaming_set_out(fd, exp_fd);

	fd.s_trace(old_trace_fd);
	out_fd.s_trace(old_trace_out_fd);
	exp_fd.s_trace(old_trace_exp_fd);
}

void streaming_list(cv4l_fd &fd, cv4l_fd &out_fd)
{
	cv4l_fd *p_out_fd = out_fd.g_fd() < 0 ? &fd : &out_fd;

	if (out_fd.g_fd() < 0)
		out_capabilities = capabilities;

	if (options[OptListBuffers])
		list_buffers(fd, fd.g_type());

	if (options[OptListBuffersOut])
		list_buffers(*p_out_fd, p_out_fd->has_vid_m2m() ?
			     v4l_type_invert(p_out_fd->g_type()) : p_out_fd->g_type());

	if (options[OptStreamBufCaps])
		stream_buf_caps(fd, fd.g_type());

	if (options[OptStreamOutBufCaps])
		stream_buf_caps(*p_out_fd, v4l_type_invert(p_out_fd->g_type()));

	if (options[OptListBuffersVbi])
		list_buffers(fd, V4L2_BUF_TYPE_VBI_CAPTURE);

	if (options[OptListBuffersSlicedVbi])
		list_buffers(fd, V4L2_BUF_TYPE_SLICED_VBI_CAPTURE);

	if (options[OptListBuffersVbiOut])
		list_buffers(*p_out_fd, V4L2_BUF_TYPE_VBI_OUTPUT);

	if (options[OptListBuffersSlicedVbiOut])
		list_buffers(*p_out_fd, V4L2_BUF_TYPE_SLICED_VBI_OUTPUT);

	if (options[OptListBuffersSdr])
		list_buffers(fd, V4L2_BUF_TYPE_SDR_CAPTURE);

	if (options[OptListBuffersSdrOut])
		list_buffers(fd, V4L2_BUF_TYPE_SDR_OUTPUT);

	if (options[OptListBuffersMeta])
		list_buffers(fd, V4L2_BUF_TYPE_META_CAPTURE);

	if (options[OptListPatterns]) {
		printf("List of available patterns:\n");
		for (unsigned i = 0; tpg_pattern_strings[i]; i++)
			printf("\t%2d: %s\n", i, tpg_pattern_strings[i]);
	}
}
