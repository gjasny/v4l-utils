/*
    V4L2 API compliance test tool.

    Copyright (C) 2008, 2010  Hans Verkuil <hverkuil@xs4all.nl>

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

#include <cctype>
#include <csignal>
#include <cstring>
#include <map>
#include <set>
#include <vector>

#include <dirent.h>
#include <getopt.h>
#include <sys/types.h>
#include <unistd.h>

#include "v4l2-compliance.h"
#include <media-info.h>
#include <v4l-getsubopt.h>

/* Short option list

   Please keep in alphabetical order.
   That makes it easier to see which short options are still free.

   In general the lower case is used to set something and the upper
   case is used to retrieve a setting. */
enum Option {
	OptStreamAllIO = 'a',
	OptStreamAllColorTest = 'c',
	OptColor = 'C',
	OptSetDevice = 'd',
	OptSetExpBufDevice = 'e',
	OptExitOnFail = 'E',
	OptStreamAllFormats = 'f',
	OptHelp = 'h',
	OptSetMediaDevice = 'm',
	OptSetMediaDeviceOnly = 'M',
	OptNoWarnings = 'n',
	OptNoProgress = 'P',
	OptSetRadioDevice = 'r',
	OptStreaming = 's',
	OptSetSWRadioDevice = 'S',
	OptSetTouchDevice = 't',
	OptTrace = 'T',
	OptSetSubDevDevice = 'u',
	OptVerbose = 'v',
	OptSetVbiDevice = 'V',
	OptUseWrapper = 'w',
	OptExitOnWarn = 'W',
	OptMediaBusInfo = 'z',
	OptStreamFrom = 128,
	OptStreamFromHdr,
	OptVersion,
	OptLast = 256
};

static char options[OptLast];

static int app_result;
static int tests_total, tests_ok;
static int grand_total, grand_ok, grand_warnings;

// Globals
bool show_info;
bool no_progress;
bool show_warnings = true;
bool show_colors;
bool exit_on_fail;
bool exit_on_warn;
bool is_vivid;
bool is_uvcvideo;
int media_fd = -1;
unsigned warnings;
bool has_mmu = true;

static unsigned color_component;
static unsigned color_skip;
static unsigned color_perc = 90;

struct dev_state {
	struct node *node;
	std::vector<v4l2_ext_control> control_vec;
	v4l2_ext_controls controls;
	v4l2_rect crop;
	v4l2_rect compose;
	v4l2_rect native_size;
	v4l2_format fmt;
	v4l2_input input;
	v4l2_output output;
	v4l2_audio ainput;
	v4l2_audioout aoutput;
	v4l2_frequency freq;
	v4l2_tuner tuner;
	v4l2_modulator modulator;
	v4l2_std_id std;
	v4l2_dv_timings timings;
	v4l2_fract interval;
};

static struct dev_state state;

static struct option long_options[] = {
	{"device", required_argument, nullptr, OptSetDevice},
	{"radio-device", required_argument, nullptr, OptSetRadioDevice},
	{"vbi-device", required_argument, nullptr, OptSetVbiDevice},
	{"sdr-device", required_argument, nullptr, OptSetSWRadioDevice},
	{"subdev-device", required_argument, nullptr, OptSetSubDevDevice},
	{"expbuf-device", required_argument, nullptr, OptSetExpBufDevice},
	{"touch-device", required_argument, nullptr, OptSetTouchDevice},
	{"media-device", required_argument, nullptr, OptSetMediaDevice},
	{"media-device-only", required_argument, nullptr, OptSetMediaDeviceOnly},
	{"media-bus-info", required_argument, nullptr, OptMediaBusInfo},
	{"help", no_argument, nullptr, OptHelp},
	{"verbose", no_argument, nullptr, OptVerbose},
	{"color", required_argument, nullptr, OptColor},
	{"no-warnings", no_argument, nullptr, OptNoWarnings},
	{"no-progress", no_argument, nullptr, OptNoProgress},
	{"exit-on-fail", no_argument, nullptr, OptExitOnFail},
	{"exit-on-warn", no_argument, nullptr, OptExitOnWarn},
	{"trace", no_argument, nullptr, OptTrace},
#ifndef NO_LIBV4L2
	{"wrapper", no_argument, nullptr, OptUseWrapper},
#endif
	{"streaming", optional_argument, nullptr, OptStreaming},
	{"stream-from", required_argument, nullptr, OptStreamFrom},
	{"stream-from-hdr", required_argument, nullptr, OptStreamFromHdr},
	{"stream-all-formats", optional_argument, nullptr, OptStreamAllFormats},
	{"stream-all-io", no_argument, nullptr, OptStreamAllIO},
	{"stream-all-color", required_argument, nullptr, OptStreamAllColorTest},
	{"version", no_argument, nullptr, OptVersion},
	{nullptr, 0, nullptr, 0}
};

#define STR(x) #x
#define STRING(x) STR(x)

static void print_sha()
{
	int fd = open("/dev/null", O_RDONLY);

	if (fd >= 0) {
		// FIONBIO is a write-only ioctl that takes an int argument that
		// enables (!= 0) or disables (== 0) nonblocking mode of a fd.
		//
		// Passing a nullptr should return EFAULT on MMU capable machines,
		// and it works if there is no MMU.
		has_mmu = ioctl(fd, FIONBIO, nullptr);
		close(fd);
	}
	printf("v4l2-compliance %s%s, ", PACKAGE_VERSION, STRING(GIT_COMMIT_CNT));
	printf("%zd bits, %zd-bit time_t%s\n",
	       sizeof(void *) * 8, sizeof(time_t) * 8,
	       has_mmu ? "" : ", has no MMU");
	if (strlen(STRING(GIT_SHA)))
		printf("v4l2-compliance SHA: %s %s\n",
		       STRING(GIT_SHA), STRING(GIT_COMMIT_DATE));
}

static void usage()
{
	printf("Usage:\n");
	printf("Common options:\n");
	printf("  -d, --device <dev> Use device <dev> as the video device.\n");
	printf("                     If <dev> starts with a digit, then /dev/video<dev> is used.\n");
	printf("                     Otherwise if -z was specified earlier, then <dev> is the entity name\n");
	printf("                     or interface ID (if prefixed with 0x) as found in the topology of the\n");
	printf("                     media device with the bus info string as specified by the -z option.\n");
	printf("  -V, --vbi-device <dev>\n");
	printf("                     Use device <dev> as the vbi device.\n");
	printf("                     If <dev> starts with a digit, then /dev/vbi<dev> is used.\n");
	printf("                     See the -d description of how <dev> is used in combination with -z.\n");
	printf("  -r, --radio-device <dev>\n");
	printf("                     Use device <dev> as the radio device.\n");
	printf("                     If <dev> starts with a digit, then /dev/radio<dev> is used.\n");
	printf("                     See the -d description of how <dev> is used in combination with -z.\n");
	printf("  -S, --sdr-device <dev>\n");
	printf("                     Use device <dev> as the SDR device.\n");
	printf("                     If <dev> starts with a digit, then /dev/swradio<dev> is used.\n");
	printf("                     See the -d description of how <dev> is used in combination with -z.\n");
	printf("  -t, --touch-device <dev>\n");
	printf("                     Use device <dev> as the touch device.\n");
	printf("                     If <dev> starts with a digit, then /dev/v4l-touch<dev> is used.\n");
	printf("                     See the -d description of how <dev> is used in combination with -z.\n");
	printf("  -u, --subdev-device <dev>\n");
	printf("                     Use device <dev> as the v4l-subdev device.\n");
	printf("                     If <dev> starts with a digit, then /dev/v4l-subdev<dev> is used.\n");
	printf("                     See the -d description of how <dev> is used in combination with -z.\n");
	printf("  -e, --expbuf-device <dev>\n");
	printf("                     Use video device <dev> to obtain DMABUF handles.\n");
	printf("                     If <dev> starts with a digit, then /dev/video<dev> is used.\n");
	printf("                     See the -d description of how <dev> is used in combination with -z.\n");
	printf("  -z, --media-bus-info <bus-info>\n");
	printf("                     Find the media device with the given bus info string. If set, then\n");
	printf("                     the options above can use the entity name or interface ID to refer\n");
	printf("                     to the device nodes.\n");
	printf("  -m, --media-device <dev>\n");
	printf("                     Use device <dev> as the media controller device. Besides this\n");
	printf("                     device it also tests all interfaces it finds.\n");
	printf("                     If <dev> starts with a digit, then /dev/media<dev> is used.\n");
	printf("                     If <dev> doesn't exist, then attempt to find a media device with a\n");
	printf("                     bus info string equal to <dev>.\n");
	printf("  -M, --media-device-only <dev>\n");
	printf("                     Use device <dev> as the media controller device. Only test this\n");
	printf("                     device, don't walk over all the interfaces.\n");
	printf("                     If <dev> starts with a digit, then /dev/media<dev> is used.\n");
	printf("                     If <dev> doesn't exist, then attempt to find a media device with a\n");
	printf("                     bus info string equal to <dev>.\n");
	printf("  -s, --streaming <count>\n");
	printf("                     Enable the streaming tests. Set <count> to the number of\n");
	printf("                     frames to stream (default 60). Requires a valid input/output\n");
	printf("                     and frequency (when dealing with a tuner). For DMABUF testing\n");
	printf("                     --expbuf-device needs to be set as well.\n");
	printf("  --stream-from [<pixelformat>=]<file>\n");
	printf("  --stream-from-hdr [<pixelformat>=]<file>\n");
	printf("                     Use the contents of the file to fill in output buffers.\n");
	printf("                     If the fourcc of the pixelformat is given, then use the file\n");
	printf("                     for output buffers using that pixelformat only.\n");
	printf("                     The --stream-from-hdr variant uses the format written by\n");
	printf("                     v4l2-ctl --stream-to-hdr where the payload sizes for each\n");
	printf("                     buffer are stored in a header. Useful for compressed formats.\n");
	printf("  -f, --stream-all-formats [<count>]\n");
	printf("                     Test streaming all available formats.\n");
	printf("                     This attempts to stream using MMAP mode or read/write\n");
	printf("                     for one second for all formats, at all sizes, at all intervals\n");
	printf("                     and with all field values. If <count> is given, then stream\n");
	printf("                     for that many frames instead of one second.\n");
	printf("  -a, --stream-all-io\n");
	printf("                     Do streaming tests for all inputs or outputs instead of just\n");
	printf("                     the current input or output. This requires that a valid video\n");
	printf("                     signal is present on all inputs and all outputs are hooked up.\n");
	printf("  -c, --stream-all-color color=red|green|blue,skip=<skip>,perc=<percentage>\n");
	printf("                     For all formats stream <skip + 1> frames and check if\n");
	printf("                     the last frame has at least <perc> percent of the pixels with\n");
	printf("                     a <color> component that is higher than the other two color\n");
	printf("                     components. This requires that a valid red, green or blue video\n");
	printf("                     signal is present on the input(s). If <skip> is not specified,\n");
	printf("                     then just capture the first frame. If <perc> is not specified,\n");
	printf("                     then this defaults to 90%%.\n");
	printf("  -E, --exit-on-fail Exit on the first fail.\n");
	printf("  -h, --help         Display this help message.\n");
	printf("  -C, --color <when> Highlight OK/warn/fail/FAIL strings with colors\n");
	printf("                     <when> can be set to always, never, or auto (the default)\n");
	printf("  -n, --no-warnings  Turn off warning messages.\n");
	printf("  -P, --no-progress  Turn off progress messages.\n");
	printf("  -T, --trace        Trace all called ioctls.\n");
	printf("  -v, --verbose      Turn on verbose reporting.\n");
	printf("  --version          Show version information.\n");
#ifndef NO_LIBV4L2
	printf("  -w, --wrapper      Use the libv4l2 wrapper library.\n");
#endif
	printf("  -W, --exit-on-warn Exit on the first warning.\n");
}

const char *ok(int res)
{
	static char buf[100];

	if (res == ENOTTY) {
		strcpy(buf, show_colors ?
		       COLOR_GREEN("OK") " (Not Supported)" :
		       "OK (Not Supported)");
		res = 0;
	} else {
		strcpy(buf, show_colors ? COLOR_GREEN("OK") : "OK");
	}
	tests_total++;
	if (res) {
		app_result = res;
		sprintf(buf, show_colors ? COLOR_RED("FAIL") : "FAIL");
	} else {
		tests_ok++;
	}
	return buf;
}

int check_string(const char *s, size_t len)
{
	size_t sz = strnlen(s, len);

	if (sz == 0)
		return fail("string empty\n");
	if (sz == len)
		return fail("string not 0-terminated\n");
	return 0;
}

int check_ustring(const __u8 *s, int len)
{
	return check_string(reinterpret_cast<const char *>(s), len);
}

int check_0(const void *p, int len)
{
	const __u8 *q = static_cast<const __u8 *>(p);

	while (len--)
		if (*q++)
			return 1;
	return 0;
}

static std::map<std::string, std::string> stream_from_map;
static std::map<std::string, bool> stream_hdr_map;

std::string stream_from(const std::string &pixelformat, bool &use_hdr)
{
	if (stream_from_map.find(pixelformat) == stream_from_map.end()) {
		if (pixelformat.empty())
			return "";
		return stream_from("", use_hdr);
	}
	use_hdr = stream_hdr_map[pixelformat];
	return stream_from_map[pixelformat];
}

static void storeStateTimings(struct node *node, __u32 caps)
{
	if (caps & V4L2_IN_CAP_STD)
		node->g_std(state.std);
	if (caps & V4L2_IN_CAP_DV_TIMINGS)
		node->g_dv_timings(state.timings);
	if (caps & V4L2_IN_CAP_NATIVE_SIZE) {
		v4l2_selection sel = {
			node->g_selection_type(),
			V4L2_SEL_TGT_NATIVE_SIZE
		};

		node->g_selection(sel);
		state.native_size = sel.r;
	}
}

static void storeState(struct node *node)
{
	state.node = node;
	if (node->has_inputs) {
		__u32 input;

		node->g_input(input);
		node->enum_input(state.input, true, input);
		if (state.input.audioset)
			node->g_audio(state.ainput);
		if (node->g_caps() & V4L2_CAP_TUNER) {
			node->g_tuner(state.tuner, state.input.tuner);
			node->g_frequency(state.freq, state.input.tuner);
		}
		storeStateTimings(node, state.input.capabilities);
	}
	if (node->has_outputs) {
		__u32 output;

		node->g_output(output);
		node->enum_output(state.output, true, output);
		if (state.output.audioset)
			node->g_audout(state.aoutput);
		if (node->g_caps() & V4L2_CAP_MODULATOR) {
			node->g_modulator(state.modulator, state.output.modulator);
			node->g_frequency(state.freq, state.output.modulator);
		}
		storeStateTimings(node, state.output.capabilities);
	}
	node->g_fmt(state.fmt);

	v4l2_selection sel = {
		node->g_selection_type(),
		V4L2_SEL_TGT_CROP
	};
	if (!node->g_selection(sel))
		state.crop = sel.r;
	sel.target = V4L2_SEL_TGT_COMPOSE;
	if (!node->g_selection(sel))
		state.compose = sel.r;
	node->get_interval(state.interval);

	v4l2_query_ext_ctrl qec = { 0 };

	while (!node->query_ext_ctrl(qec, true, true)) {
		if (qec.flags & (V4L2_CTRL_FLAG_DISABLED |
				 V4L2_CTRL_FLAG_READ_ONLY |
				 V4L2_CTRL_FLAG_WRITE_ONLY |
				 V4L2_CTRL_FLAG_VOLATILE))
			continue;
		v4l2_ext_control ctrl = { qec.id };
		if (qec.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD) {
			ctrl.size = qec.elems * qec.elem_size;
			ctrl.ptr = malloc(ctrl.size);
		}
		state.control_vec.push_back(ctrl);
	}
	if (state.control_vec.empty())
		return;
	state.controls.count = state.control_vec.size();
	state.controls.controls = &state.control_vec[0];
	node->g_ext_ctrls(state.controls);
}

static void restoreStateTimings(struct node *node, __u32 caps)
{
	if (caps & V4L2_IN_CAP_STD)
		node->s_std(state.std);
	if (caps & V4L2_IN_CAP_DV_TIMINGS)
		node->s_dv_timings(state.timings);
	if (caps & V4L2_IN_CAP_NATIVE_SIZE) {
		v4l2_selection sel = {
			node->g_selection_type(),
			V4L2_SEL_TGT_NATIVE_SIZE,
			0, state.native_size
		};

		node->s_selection(sel);
	}
}

static void restoreState()
{
	struct node *node = state.node;

	node->reopen();
	if (node->has_inputs) {
		node->s_input(state.input.index);
		if (state.input.audioset)
			node->s_audio(state.ainput.index);
		if (node->g_caps() & V4L2_CAP_TUNER) {
			node->s_tuner(state.tuner);
			node->s_frequency(state.freq);
		}
		restoreStateTimings(node, state.input.capabilities);
	}
	if (node->has_outputs) {
		node->s_output(state.output.index);
		if (state.output.audioset)
			node->s_audout(state.aoutput.index);

		if (node->g_caps() & V4L2_CAP_MODULATOR) {
			node->s_modulator(state.modulator);
			node->s_frequency(state.freq);
		}
		restoreStateTimings(node, state.output.capabilities);
	}

	/* First restore the format */
	node->s_fmt(state.fmt);

	v4l2_selection sel_compose = {
		node->g_selection_type(),
		V4L2_SEL_TGT_COMPOSE,
		0, state.compose
	};
	v4l2_selection sel_crop = {
		node->g_selection_type(),
		V4L2_SEL_TGT_CROP,
		0, state.crop
	};
	if (node->has_inputs) {
		/*
		 * For capture restore the compose rectangle
		 * before the crop rectangle.
		 */
		if (sel_compose.r.width && sel_compose.r.height)
			node->s_selection(sel_compose);
		if (sel_crop.r.width && sel_crop.r.height)
			node->s_selection(sel_crop);
	}
	if (node->has_outputs) {
		/*
		 * For output the crop rectangle should be
		 * restored before the compose rectangle.
		 */
		if (sel_crop.r.width && sel_crop.r.height)
			node->s_selection(sel_crop);
		if (sel_compose.r.width && sel_compose.r.height)
			node->s_selection(sel_compose);
	}
	if (state.interval.denominator)
		node->set_interval(state.interval);

	node->s_ext_ctrls(state.controls);
	// We need to reopen again so QUERYCAP is called again.
	// The vivid driver has controls (RDS related) that change
	// the capabilities, and if we don't do this, then the
	// internal caps stored in v4l_fd is out-of-sync with the
	// actual caps.
	node->reopen();
}

__attribute__((noreturn))
static void signal_handler_interrupt(int signum)
{
	restoreState();
	printf("\n");
	std::exit(EXIT_FAILURE);
}

static void determine_codec_mask(struct node &node)
{
	struct v4l2_fmtdesc fmt_desc;
	int num_cap_fmts = 0;
	int num_compressed_cap_fmts = 0;
	int num_out_fmts = 0;
	int num_compressed_out_fmts = 0;
	unsigned mask = 0;

	node.codec_mask = 0;
	if (!node.has_vid_m2m())
		return;

	if (node.enum_fmt(fmt_desc, true, 0, node.g_type()))
		return;

	do {
		if (fmt_desc.flags & V4L2_FMT_FLAG_COMPRESSED) {
			num_compressed_cap_fmts++;
			switch (fmt_desc.pixelformat) {
			case V4L2_PIX_FMT_JPEG:
			case V4L2_PIX_FMT_MJPEG:
				mask |= JPEG_ENCODER;
				break;
			case V4L2_PIX_FMT_H263:
			case V4L2_PIX_FMT_H264:
			case V4L2_PIX_FMT_H264_NO_SC:
			case V4L2_PIX_FMT_H264_MVC:
			case V4L2_PIX_FMT_MPEG1:
			case V4L2_PIX_FMT_MPEG2:
			case V4L2_PIX_FMT_MPEG4:
			case V4L2_PIX_FMT_XVID:
			case V4L2_PIX_FMT_VC1_ANNEX_G:
			case V4L2_PIX_FMT_VC1_ANNEX_L:
			case V4L2_PIX_FMT_VP8:
			case V4L2_PIX_FMT_VP9:
			case V4L2_PIX_FMT_HEVC:
			case V4L2_PIX_FMT_FWHT:
				mask |= STATEFUL_ENCODER;
				break;
#if 0 	// There are no stateless encoders (yet)
			case V4L2_PIX_FMT_MPEG2_SLICE:
				mask |= STATELESS_ENCODER;
				break;
#endif
			case V4L2_PIX_FMT_QC08C:
			case V4L2_PIX_FMT_QC10C:
				num_compressed_cap_fmts--;
				break;
			default:
				return;
			}
		}
		num_cap_fmts++;
	} while (!node.enum_fmt(fmt_desc));


	if (node.enum_fmt(fmt_desc, true, 0, v4l_type_invert(node.g_type())))
		return;

	do {
		if (fmt_desc.flags & V4L2_FMT_FLAG_COMPRESSED) {
			num_compressed_out_fmts++;
			switch (fmt_desc.pixelformat) {
			case V4L2_PIX_FMT_JPEG:
			case V4L2_PIX_FMT_MJPEG:
				mask |= JPEG_DECODER;
				break;
			case V4L2_PIX_FMT_H263:
			case V4L2_PIX_FMT_H264:
			case V4L2_PIX_FMT_H264_NO_SC:
			case V4L2_PIX_FMT_H264_MVC:
			case V4L2_PIX_FMT_MPEG1:
			case V4L2_PIX_FMT_MPEG2:
			case V4L2_PIX_FMT_MPEG4:
			case V4L2_PIX_FMT_XVID:
			case V4L2_PIX_FMT_VC1_ANNEX_G:
			case V4L2_PIX_FMT_VC1_ANNEX_L:
			case V4L2_PIX_FMT_VP8:
			case V4L2_PIX_FMT_VP9:
			case V4L2_PIX_FMT_HEVC:
			case V4L2_PIX_FMT_FWHT:
				mask |= STATEFUL_DECODER;
				break;
			case V4L2_PIX_FMT_MPEG2_SLICE:
			case V4L2_PIX_FMT_H264_SLICE:
			case V4L2_PIX_FMT_HEVC_SLICE:
			case V4L2_PIX_FMT_VP8_FRAME:
			case V4L2_PIX_FMT_VP9_FRAME:
			case V4L2_PIX_FMT_AV1_FRAME:
			case V4L2_PIX_FMT_FWHT_STATELESS:
				mask |= STATELESS_DECODER;
				break;
			case V4L2_PIX_FMT_QC08C:
			case V4L2_PIX_FMT_QC10C:
				num_compressed_out_fmts--;
				break;
			default:
				return;
			}
		}
		num_out_fmts++;
	} while (!node.enum_fmt(fmt_desc));

	if (num_compressed_out_fmts == 0 && num_compressed_cap_fmts == num_cap_fmts)
		node.codec_mask = mask;
	else if (num_compressed_cap_fmts == 0 && num_compressed_out_fmts == num_out_fmts)
		node.codec_mask = mask;
}

static int testCap(struct node *node)
{
	struct v4l2_capability vcap;
	__u32 caps, dcaps;
	const __u32 video_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OUTPUT |
			V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE |
			V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_VIDEO_OUTPUT_OVERLAY;
	const __u32 vbi_caps = V4L2_CAP_VBI_CAPTURE | V4L2_CAP_SLICED_VBI_CAPTURE |
			V4L2_CAP_VBI_OUTPUT | V4L2_CAP_SLICED_VBI_OUTPUT;
	const __u32 sdr_caps = V4L2_CAP_SDR_CAPTURE | V4L2_CAP_SDR_OUTPUT;
	const __u32 radio_caps = V4L2_CAP_RADIO | V4L2_CAP_MODULATOR;
	const __u32 input_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OVERLAY |
			V4L2_CAP_VBI_CAPTURE | V4L2_CAP_SLICED_VBI_CAPTURE |
			V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_HW_FREQ_SEEK |
			V4L2_CAP_TUNER | V4L2_CAP_SDR_CAPTURE | V4L2_CAP_META_CAPTURE;
	const __u32 output_caps = V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			V4L2_CAP_VIDEO_OUTPUT_OVERLAY | V4L2_CAP_VBI_OUTPUT |
			V4L2_CAP_SDR_OUTPUT | V4L2_CAP_SLICED_VBI_OUTPUT |
			V4L2_CAP_MODULATOR | V4L2_CAP_META_OUTPUT;
	const __u32 overlay_caps = V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_VIDEO_OUTPUT_OVERLAY;
	const __u32 m2m_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE;
	const __u32 io_caps = V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;
	const __u32 mplane_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
		V4L2_CAP_VIDEO_M2M_MPLANE;
	const __u32 splane_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OUTPUT |
		V4L2_CAP_VIDEO_M2M;

	memset(&vcap, 0xff, sizeof(vcap));
	fail_on_test(doioctl(node, VIDIOC_QUERYCAP, &vcap));
	if (has_mmu)
		fail_on_test(doioctl(node, VIDIOC_QUERYCAP, nullptr) != EFAULT);
	fail_on_test(check_ustring(vcap.driver, sizeof(vcap.driver)));
	fail_on_test(check_ustring(vcap.card, sizeof(vcap.card)));
	fail_on_test(check_ustring(vcap.bus_info, sizeof(vcap.bus_info)));
	// Check for valid prefixes
	if (memcmp(vcap.bus_info, "usb-", 4) &&
	    memcmp(vcap.bus_info, "PCI:", 4) &&
	    memcmp(vcap.bus_info, "PCIe:", 5) &&
	    memcmp(vcap.bus_info, "ISA:", 4) &&
	    memcmp(vcap.bus_info, "I2C:", 4) &&
	    memcmp(vcap.bus_info, "parport", 7) &&
	    memcmp(vcap.bus_info, "platform:", 9) &&
	    memcmp(vcap.bus_info, "rmi4:", 5) &&
	    memcmp(vcap.bus_info, "libcamera:", 10) &&
	    memcmp(vcap.bus_info, "gadget.", 7))
		return fail("missing bus_info prefix ('%s')\n", vcap.bus_info);
	if (!node->media_bus_info.empty() &&
	    node->media_bus_info != std::string(reinterpret_cast<const char *>(vcap.bus_info)))
		warn("media bus_info '%s' differs from V4L2 bus_info '%s'\n",
		     node->media_bus_info.c_str(), vcap.bus_info);
	fail_on_test((vcap.version >> 16) < 3);
	if (vcap.version >= 0x050900)  // Present from 5.9.0 onwards
		node->might_support_cache_hints = true;
	fail_on_test(check_0(vcap.reserved, sizeof(vcap.reserved)));
	caps = vcap.capabilities;
	dcaps = vcap.device_caps;
	node->is_m2m = dcaps & m2m_caps;
	fail_on_test(caps == 0);
	fail_on_test(!(caps & V4L2_CAP_DEVICE_CAPS));
	fail_on_test(dcaps & V4L2_CAP_DEVICE_CAPS);
	fail_on_test(dcaps & ~caps);
	fail_on_test(!(dcaps & caps));
	// set by the core, so this really should always be there
	// for a modern driver for both caps and dcaps
	fail_on_test(!(caps & V4L2_CAP_EXT_PIX_FORMAT));
	//fail_on_test(!(dcaps & V4L2_CAP_EXT_PIX_FORMAT));
	fail_on_test(node->is_video && !(dcaps & video_caps));
	fail_on_test(node->is_radio && !(dcaps & radio_caps));
	// V4L2_CAP_AUDIO is invalid for radio and sdr
	fail_on_test(node->is_radio && (dcaps & V4L2_CAP_AUDIO));
	fail_on_test(node->is_sdr && (dcaps & V4L2_CAP_AUDIO));
	fail_on_test(node->is_vbi && !(dcaps & vbi_caps));
	fail_on_test(node->is_sdr && !(dcaps & sdr_caps));
	// You can't have both set due to missing buffer type in VIDIOC_G/S_FBUF
	fail_on_test((dcaps & overlay_caps) == overlay_caps);
	// Overlay support makes no sense for m2m devices
	fail_on_test((dcaps & m2m_caps) && (dcaps & overlay_caps));
	fail_on_test(node->is_video && (dcaps & (vbi_caps | radio_caps | sdr_caps)));
	fail_on_test(node->is_radio && (dcaps & (vbi_caps | video_caps | sdr_caps)));
	fail_on_test(node->is_vbi && (dcaps & (video_caps | radio_caps | sdr_caps)));
	fail_on_test(node->is_sdr && (dcaps & (video_caps | V4L2_CAP_RADIO | vbi_caps)));
	if (node->is_m2m) {
		fail_on_test((dcaps & input_caps) && (dcaps & output_caps));
	} else {
		if (dcaps & input_caps)
			fail_on_test(dcaps & output_caps);
		if (dcaps & output_caps)
			fail_on_test(dcaps & input_caps);
	}
	if (node->can_capture || node->can_output) {
		// whether io_caps need to be set for RDS capture/output is
		// checked elsewhere as that depends on the tuner/modulator
		// capabilities.
		if (!(dcaps & (V4L2_CAP_RDS_CAPTURE | V4L2_CAP_RDS_OUTPUT)))
			fail_on_test(!(dcaps & io_caps));
	} else {
		fail_on_test(dcaps & io_caps);
	}
	// having both mplane and splane caps is not allowed (at least for now)
	fail_on_test((dcaps & mplane_caps) && (dcaps & splane_caps));

	if (node->codec_mask) {
		bool found_bit = false;

		// Only one bit may be set in the mask
		for (unsigned i = 0; i < 32; i++)
			if (node->codec_mask & (1U << i)) {
				fail_on_test(found_bit);
				found_bit = true;
			}
	}

	return 0;
}

#define NR_OPENS 100
static int testUnlimitedOpens(struct node *node)
{
	int fds[NR_OPENS];
	unsigned i;
	bool ok;

	/*
	 * There should *not* be an artificial limit to the number
	 * of open()s you can do on a V4L2 device node. So test whether
	 * you can open a device node at least 100 times.
	 *
	 * And please don't start rejecting opens in your driver at 101!
	 * There really shouldn't be a limit in the driver.
	 *
	 * If there are resource limits, then check against those limits
	 * where they are actually needed.
	 */
	for (i = 0; i < NR_OPENS; i++) {
		fds[i] = open(node->device, O_RDWR);
		if (fds[i] < 0)
			break;
	}
	ok = i == NR_OPENS;
	while (i--)
		close(fds[i]);
	fail_on_test(!ok);
	return 0;
}

static int check_prio(struct node *node, struct node *node2, enum v4l2_priority match)
{
	enum v4l2_priority prio;

	// Must be able to get priority
	fail_on_test(doioctl(node, VIDIOC_G_PRIORITY, &prio));
	// Must match the expected prio
	fail_on_test(prio != match);
	fail_on_test(doioctl(node2, VIDIOC_G_PRIORITY, &prio));
	fail_on_test(prio != match);
	return 0;
}

static int testPrio(struct node *node, struct node *node2)
{
	enum v4l2_priority prio;
	int err;

	err = check_prio(node, node2, V4L2_PRIORITY_DEFAULT);
	if (err)
		return err;

	prio = V4L2_PRIORITY_RECORD;
	// Must be able to change priority
	fail_on_test(doioctl(node, VIDIOC_S_PRIORITY, &prio));
	// Must match the new prio
	fail_on_test(check_prio(node, node2, V4L2_PRIORITY_RECORD));

	prio = V4L2_PRIORITY_INTERACTIVE;
	// Going back to interactive on the other node must fail
	fail_on_test(!doioctl(node2, VIDIOC_S_PRIORITY, &prio));
	prio = V4L2_PRIORITY_INTERACTIVE;
	// Changing it on the first node must work.
	fail_on_test(doioctl(node, VIDIOC_S_PRIORITY, &prio));
	fail_on_test(check_prio(node, node2, V4L2_PRIORITY_INTERACTIVE));
	return 0;
}

static int testInvalidIoctls(struct node *node, char type)
{
	unsigned ioc = _IOC(_IOC_NONE, type, 0xff, 0);
	unsigned char buf[0x4000] = {};

	fail_on_test(doioctl(node, ioc, nullptr) != ENOTTY);
	ioc = _IOC(_IOC_NONE, type, 0, 0x3fff);
	fail_on_test(doioctl(node, ioc, nullptr) != ENOTTY);
	ioc = _IOC(_IOC_READ, type, 0, 0x3fff);
	fail_on_test(doioctl(node, ioc, buf) != ENOTTY);
	fail_on_test(check_0(buf, sizeof(buf)));
	ioc = _IOC(_IOC_WRITE, type, 0, 0x3fff);
	fail_on_test(doioctl(node, ioc, buf) != ENOTTY);
	fail_on_test(check_0(buf, sizeof(buf)));
	ioc = _IOC(_IOC_READ | _IOC_WRITE, type, 0, 0x3fff);
	fail_on_test(doioctl(node, ioc, buf) != ENOTTY);
	fail_on_test(check_0(buf, sizeof(buf)));
	return 0;
}

static void streamingSetup(struct node *node)
{
	if (node->can_capture) {
		v4l2_fract min_period = { 1, 1000 };
		struct v4l2_input input;

		memset(&input, 0, sizeof(input));
		doioctl(node, VIDIOC_G_INPUT, &input.index);
		doioctl(node, VIDIOC_ENUMINPUT, &input);
		node->cur_io_caps = input.capabilities;
		node->set_interval(min_period);
	} else if (node->can_output) {
		struct v4l2_output output;

		memset(&output, 0, sizeof(output));
		doioctl(node, VIDIOC_G_OUTPUT, &output.index);
		doioctl(node, VIDIOC_ENUMOUTPUT, &output);
		node->cur_io_caps = output.capabilities;
	}
}

static int parse_subopt(char **subs, const char * const *subopts, char **value)
{
	int opt = v4l_getsubopt(subs, const_cast<char * const *>(subopts), value);

	if (opt == -1) {
		fprintf(stderr, "Invalid suboptions specified\n");
		return -1;
	}
	if (*value == nullptr) {
		fprintf(stderr, "No value given to suboption <%s>\n",
				subopts[opt]);
		return -1;
	}
	return opt;
}

static int open_media_bus_info(const std::string &bus_info, std::string &media_devname)
{
	DIR *dp;
	struct dirent *ep;

	dp = opendir("/dev");
	if (dp == nullptr)
		return -1;

	while ((ep = readdir(dp))) {
		const char *name = ep->d_name;

		if (!memcmp(name, "media", 5) && isdigit(name[5])) {
			struct media_device_info mdi;
			media_devname = std::string("/dev/") + name;

			int fd = open(media_devname.c_str(), O_RDWR);
			if (fd < 0)
				continue;
			if (!ioctl(fd, MEDIA_IOC_DEVICE_INFO, &mdi) &&
			    bus_info == mdi.bus_info) {
				closedir(dp);
				return fd;
			}
			close(fd);
		}
	}
	closedir(dp);
	return -1;
}

static std::string make_devname(const char *device, const char *devname,
				const std::string &media_bus_info, bool is_media = false)
{
	if (device[0] >= '0' && device[0] <= '9' && strlen(device) <= 3) {
		char newdev[32];

		sprintf(newdev, "/dev/%s%s", devname, device);
		return newdev;
	}
	if (media_bus_info.empty())
		return device;

	std::string media_devname;
	int media_fd = open_media_bus_info(media_bus_info, media_devname);
	if (media_fd < 0)
		return device;
	if (is_media) {
		close(media_fd);
		return media_devname;
	}

	media_v2_topology topology;
	memset(&topology, 0, sizeof(topology));
	if (ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, &topology)) {
		close(media_fd);
		return device;
	}

	auto ents = new media_v2_entity[topology.num_entities];
	topology.ptr_entities = (uintptr_t)ents;
	auto links = new media_v2_link[topology.num_links];
	topology.ptr_links = (uintptr_t)links;
	auto ifaces = new media_v2_interface[topology.num_interfaces];
	topology.ptr_interfaces = (uintptr_t)ifaces;

	unsigned i, ent_id, iface_id = 0;

	std::string result(device);

	if (ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, &topology))
		goto err;

	if (device[0] == '0' && device[1] == 'x')
		iface_id = strtoul(device, nullptr, 16);

	if (!iface_id) {
		for (i = 0; i < topology.num_entities; i++)
			if (!strcmp(ents[i].name, device))
				break;
		if (i >= topology.num_entities)
			goto err;
		ent_id = ents[i].id;
		for (i = 0; i < topology.num_links; i++)
			if (links[i].sink_id == ent_id &&
			    (links[i].flags & MEDIA_LNK_FL_LINK_TYPE) ==
			    MEDIA_LNK_FL_INTERFACE_LINK)
				break;
		if (i >= topology.num_links)
			goto err;
		iface_id = links[i].source_id;
	}
	for (i = 0; i < topology.num_interfaces; i++)
		if (ifaces[i].id == iface_id)
			break;
	if (i >= topology.num_interfaces)
		goto err;

	result = mi_media_get_device(ifaces[i].devnode.major, ifaces[i].devnode.minor);

err:
	delete [] ents;
	delete [] links;
	delete [] ifaces;
	close(media_fd);
	return result;
}

void testNode(struct node &node, struct node &node_m2m_cap, struct node &expbuf_node, media_type type,
	      unsigned frame_count, unsigned all_fmt_frame_count, int parent_media_fd)
{
	struct node node2;
	struct v4l2_capability vcap = {};
	struct v4l2_subdev_capability subdevcap = {};
	struct v4l2_subdev_client_capability subdevclientcap = {};
	std::string driver;

	tests_total = tests_ok = warnings = 0;

	node.is_video = type == MEDIA_TYPE_VIDEO;
	node.is_vbi = type == MEDIA_TYPE_VBI;
	node.is_radio = type == MEDIA_TYPE_RADIO;
	node.is_sdr = type == MEDIA_TYPE_SDR;
	node.is_touch = type == MEDIA_TYPE_TOUCH;

	if (node.is_v4l2()) {
		doioctl(&node, VIDIOC_QUERYCAP, &vcap);
		driver = reinterpret_cast<const char *>(vcap.driver);
		is_uvcvideo = driver == "uvcvideo";
		is_vivid = driver == "vivid";
		if (is_vivid)
			node.bus_info = reinterpret_cast<const char *>(vcap.bus_info);
		determine_codec_mask(node);
	} else if (node.is_subdev()) {
		doioctl(&node, VIDIOC_SUBDEV_QUERYCAP, &subdevcap);
		subdevclientcap.capabilities = ~0ULL;
		if (doioctl(&node, VIDIOC_SUBDEV_S_CLIENT_CAP, &subdevclientcap))
			subdevclientcap.capabilities = 0ULL;
	} else {
		memset(&vcap, 0, sizeof(vcap));
	}

	if (!node.is_media()) {
		if (parent_media_fd >= 0)
			media_fd = parent_media_fd;
		else
			media_fd = mi_get_media_fd(node.g_fd(), node.bus_info);
	}

	int fd = node.is_media() ? node.g_fd() : media_fd;
	if (fd >= 0) {
		struct media_device_info mdinfo;

		if (!ioctl(fd, MEDIA_IOC_DEVICE_INFO, &mdinfo)) {
			if (!node.is_v4l2())
				driver = mdinfo.driver;
			node.media_bus_info = mdinfo.bus_info;
			node.has_media = true;
		}
	}

	if (driver.empty())
		printf("Compliance test for device ");
	else
		printf("Compliance test for %s device ", driver.c_str());
	printf("%s%s:\n\n", node.device, node.g_direct() ? "" : " (using libv4l2)");

	if (node.g_caps() & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VBI_CAPTURE |
			 V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_SLICED_VBI_CAPTURE |
			 V4L2_CAP_META_CAPTURE))
		node.has_inputs = true;
	if (node.g_caps() & (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VBI_OUTPUT |
			 V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_SLICED_VBI_OUTPUT |
			 V4L2_CAP_META_OUTPUT))
		node.has_outputs = true;
	if (node.g_caps() & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VBI_CAPTURE |
			 V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE |
			 V4L2_CAP_VIDEO_M2M | V4L2_CAP_SLICED_VBI_CAPTURE |
			 V4L2_CAP_RDS_CAPTURE | V4L2_CAP_SDR_CAPTURE |
			 V4L2_CAP_META_CAPTURE))
		node.can_capture = true;
	if (node.g_caps() & (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VBI_OUTPUT |
			 V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE |
			 V4L2_CAP_VIDEO_M2M | V4L2_CAP_SLICED_VBI_OUTPUT |
			 V4L2_CAP_RDS_OUTPUT | V4L2_CAP_SDR_OUTPUT |
			 V4L2_CAP_META_OUTPUT))
		node.can_output = true;
	if (node.g_caps() & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			 V4L2_CAP_VIDEO_M2M_MPLANE))
		node.is_planar = true;
	if (node.g_caps() & (V4L2_CAP_META_CAPTURE | V4L2_CAP_META_OUTPUT)) {
		node.is_video = false;
		node.is_meta = true;
	}
	if (node.g_caps() & V4L2_CAP_IO_MC)
		node.is_io_mc = true;

	/* Information Opts */

	if (node.is_v4l2()) {
		printf("Driver Info:\n");
		v4l2_info_capability(vcap);
		switch (node.codec_mask) {
		case JPEG_ENCODER:
			printf("\tDetected JPEG Encoder\n");
			break;
		case JPEG_DECODER:
			printf("\tDetected JPEG Decoder\n");
			break;
		case STATEFUL_ENCODER:
			printf("\tDetected Stateful Encoder\n");
			break;
		case STATEFUL_DECODER:
			printf("\tDetected Stateful Decoder\n");
			break;
		case STATELESS_ENCODER:
			printf("\tDetected Stateless Encoder\n");
			break;
		case STATELESS_DECODER:
			printf("\tDetected Stateless Decoder\n");
			break;
		}
	} else if (node.is_subdev()) {
		printf("Driver Info:\n");
		v4l2_info_subdev_capability(subdevcap, subdevclientcap);
	}

	__u32 ent_id = 0;
	bool is_invalid = false;

	if (node.is_media())
		mi_media_info_for_fd(node.g_fd(), -1, &is_invalid);
	else if (media_fd >= 0)
		ent_id = mi_media_info_for_fd(media_fd, node.g_fd(), &is_invalid, &node.function);

	if (ent_id != MEDIA_ENT_F_UNKNOWN) {
		memset(&node.entity, 0, sizeof(node.entity));
		node.entity.id = ent_id;
		if (!ioctl(media_fd, MEDIA_IOC_ENUM_ENTITIES, &node.entity)) {
			struct media_links_enum links_enum;

			node.pads = new media_pad_desc[node.entity.pads];
			node.links = new media_link_desc[node.entity.links];
			memset(&links_enum, 0, sizeof(links_enum));
			links_enum.entity = ent_id;
			links_enum.pads = node.pads;
			links_enum.links = node.links;
			if (ioctl(media_fd, MEDIA_IOC_ENUM_LINKS, &links_enum))
				node.entity.id = 0;
		} else {
			node.entity.id = 0;
		}
	}

	/* Required ioctls */

	printf("\nRequired ioctls:\n");

	if (ent_id)
		printf("\ttest MC information (see 'Media Driver Info' above): %s\n", ok(is_invalid ? -1 : 0));

	if (node.is_v4l2())
		printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(&node)));
	else if (node.is_subdev())
		printf("\ttest VIDIOC_SUDBEV_QUERYCAP: %s\n", ok(testSubDevCap(&node)));

	if (node.is_v4l2() || node.is_subdev())
		printf("\ttest invalid ioctls: %s\n", ok(testInvalidIoctls(&node, 'V')));

	if (node.is_media()) {
		printf("\ttest MEDIA_IOC_DEVICE_INFO: %s\n",
		       ok(testMediaDeviceInfo(&node)));
		printf("\ttest invalid ioctls: %s\n", ok(testInvalidIoctls(&node, '|')));
	}
	printf("\n");

	/* Multiple opens */

	printf("Allow for multiple opens:\n");
	node2 = node;
	switch (type) {
	case MEDIA_TYPE_SUBDEV:
		printf("\ttest second %s open: %s\n", node.device,
		       ok(node2.subdev_open(node.device, false) >= 0 ? 0 : errno));
		if (node2.g_fd() >= 0)
			printf("\ttest VIDIOC_SUBDEV_QUERYCAP: %s\n", ok(testSubDevCap(&node2)));
		break;
	case MEDIA_TYPE_MEDIA:
		printf("\ttest second %s open: %s\n", node.device,
		       ok(node2.media_open(node.device, false) >= 0 ? 0 : errno));
		if (node2.g_fd() >= 0)
			printf("\ttest MEDIA_IOC_DEVICE_INFO: %s\n", ok(testMediaDeviceInfo(&node2)));
		break;
	default:
		printf("\ttest second %s open: %s\n", node.device,
		       ok(node2.open(node.device, false) >= 0 ? 0 : errno));
		if (node2.g_fd() >= 0) {
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(&node2)));
			printf("\ttest VIDIOC_G/S_PRIORITY: %s\n",
			       ok(testPrio(&node, &node2)));
		}
		break;
	}
	if (node2.g_fd() >= 0)
		node.node2 = &node2;

	printf("\ttest for unlimited opens: %s\n",
		ok(testUnlimitedOpens(&node)));

	printf("\n");
	storeState(&node);

	/* register signal handler for interrupt signal, to exit gracefully */
	signal(SIGINT, signal_handler_interrupt);

	unsigned cur_io = 0;
	unsigned min_io = 0;
	unsigned max_io = 0;

	/* Media ioctls */

	if (node.is_media()) {
		printf("Media Controller ioctls:\n");
		printf("\ttest MEDIA_IOC_G_TOPOLOGY: %s\n", ok(testMediaTopology(&node)));
		if (node.topology)
			printf("\tEntities: %u Interfaces: %u Pads: %u Links: %u\n",
			       node.topology->num_entities,
			       node.topology->num_interfaces,
			       node.topology->num_pads,
			       node.topology->num_links);
		printf("\ttest MEDIA_IOC_ENUM_ENTITIES/LINKS: %s\n", ok(testMediaEnum(&node)));
		printf("\ttest MEDIA_IOC_SETUP_LINK: %s\n", ok(testMediaSetupLink(&node)));
		printf("\n");
		goto show_total;
	}

	/* Debug ioctls */

	printf("Debug ioctls:\n");
	if (node.is_v4l2())
		printf("\ttest VIDIOC_DBG_G/S_REGISTER: %s\n", ok(testRegister(&node)));
	printf("\ttest VIDIOC_LOG_STATUS: %s\n", ok(testLogStatus(&node)));
	printf("\n");

	/* Input ioctls */

	printf("Input ioctls:\n");
	printf("\ttest VIDIOC_G/S_TUNER/ENUM_FREQ_BANDS: %s\n", ok(testTuner(&node)));
	printf("\ttest VIDIOC_G/S_FREQUENCY: %s\n", ok(testTunerFreq(&node)));
	printf("\ttest VIDIOC_S_HW_FREQ_SEEK: %s\n", ok(testTunerHwSeek(&node)));
	printf("\ttest VIDIOC_ENUMAUDIO: %s\n", ok(testEnumInputAudio(&node)));
	printf("\ttest VIDIOC_G/S/ENUMINPUT: %s\n", ok(testInput(&node)));
	printf("\ttest VIDIOC_G/S_AUDIO: %s\n", ok(testInputAudio(&node)));
	printf("\tInputs: %d Audio Inputs: %d Tuners: %d\n",
			node.inputs, node.audio_inputs, node.tuners);
	printf("\n");

	/* Output ioctls */

	printf("Output ioctls:\n");
	printf("\ttest VIDIOC_G/S_MODULATOR: %s\n", ok(testModulator(&node)));
	printf("\ttest VIDIOC_G/S_FREQUENCY: %s\n", ok(testModulatorFreq(&node)));
	printf("\ttest VIDIOC_ENUMAUDOUT: %s\n", ok(testEnumOutputAudio(&node)));
	printf("\ttest VIDIOC_G/S/ENUMOUTPUT: %s\n", ok(testOutput(&node)));
	printf("\ttest VIDIOC_G/S_AUDOUT: %s\n", ok(testOutputAudio(&node)));
	printf("\tOutputs: %d Audio Outputs: %d Modulators: %d\n",
			node.outputs, node.audio_outputs, node.modulators);
	printf("\n");

	/* I/O configuration ioctls */

	printf("Input/Output configuration ioctls:\n");
	printf("\ttest VIDIOC_ENUM/G/S/QUERY_STD: %s\n", ok(testStd(&node)));
	printf("\ttest VIDIOC_ENUM/G/S/QUERY_DV_TIMINGS: %s\n", ok(testTimings(&node)));
	printf("\ttest VIDIOC_DV_TIMINGS_CAP: %s\n", ok(testTimingsCap(&node)));
	printf("\ttest VIDIOC_G/S_EDID: %s\n", ok(testEdid(&node)));
	printf("\n");

	/* Sub-device ioctls */

	if (node.is_subdev()) {
		bool has_source = false;
		bool has_sink = false;
		struct v4l2_subdev_routing sd_routing[2] = {};
		struct v4l2_subdev_route sd_routes[2][NUM_ROUTES_MAX] = {};
		bool has_routes = !!(subdevcap.capabilities & V4L2_SUBDEV_CAP_STREAMS);
		int ret;

		node.frame_interval_pad = -1;
		node.enum_frame_interval_pad = -1;
		for (unsigned pad = 0; pad < node.entity.pads; pad++) {
			if (node.pads[pad].flags & MEDIA_PAD_FL_SINK)
				has_sink = true;
			if (node.pads[pad].flags & MEDIA_PAD_FL_SOURCE)
				has_source = true;
		}
		node.is_passthrough_subdev = has_source && has_sink;

		if (has_routes) {
			printf("Sub-Device routing ioctls:\n");

			for (unsigned which = V4L2_SUBDEV_FORMAT_TRY;
				which <= V4L2_SUBDEV_FORMAT_ACTIVE; which++) {

				printf("\ttest %s VIDIOC_SUBDEV_G_ROUTING/VIDIOC_SUBDEV_S_ROUTING: %s\n",
				       which ? "Active" : "Try",
				       ok(testSubDevRouting(&node, which)));
			}

			printf("\n");

			for (unsigned which = V4L2_SUBDEV_FORMAT_TRY;
				which <= V4L2_SUBDEV_FORMAT_ACTIVE; which++) {
				struct v4l2_subdev_routing &routing = sd_routing[which];

				routing.which = which;
				routing.routes = (uintptr_t)sd_routes[which];
				routing.len_routes = NUM_ROUTES_MAX;
				routing.num_routes = 0;

				ret = doioctl(&node, VIDIOC_SUBDEV_G_ROUTING, &routing);
				if (ret) {
					fail("VIDIOC_SUBDEV_G_ROUTING: failed to get routing\n");
					routing.num_routes = 0;
				}
			}
		}

		for (unsigned pad = 0; pad < node.entity.pads; pad++) {
			printf("Sub-Device ioctls (%s Pad %u):\n",
			       (node.pads[pad].flags & MEDIA_PAD_FL_SINK) ?
			       "Sink" : "Source", pad);
			node.has_subdev_enum_code = 0;
			node.has_subdev_enum_fsize = 0;
			node.has_subdev_enum_fival = 0;
			for (unsigned which = V4L2_SUBDEV_FORMAT_TRY;
			     which <= V4L2_SUBDEV_FORMAT_ACTIVE; which++) {
				struct v4l2_subdev_routing dummy_routing;
				struct v4l2_subdev_route dummy_routes[1];

				const struct v4l2_subdev_routing *routing;
				const struct v4l2_subdev_route *routes;

				if (has_routes) {
					routing = &sd_routing[which];
					routes = sd_routes[which];
				} else {
					dummy_routing.num_routes = 1;
					dummy_routing.routes = (uintptr_t)&dummy_routes;
					dummy_routes[0].source_pad = pad;
					dummy_routes[0].source_stream = 0;
					dummy_routes[0].sink_pad = pad;
					dummy_routes[0].sink_stream = 0;
					dummy_routes[0].flags = V4L2_SUBDEV_ROUTE_FL_ACTIVE;

					routing = &dummy_routing;
					routes = dummy_routes;
				}

				for (unsigned i = 0; i < routing->num_routes; ++i) {
					const struct v4l2_subdev_route *r = &routes[i];
					unsigned stream;

					if (!(r->flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE))
						continue;

					if ((node.pads[pad].flags & MEDIA_PAD_FL_SINK) &&
					    (r->sink_pad == pad))
						stream = r->sink_stream;
					else if ((node.pads[pad].flags & MEDIA_PAD_FL_SOURCE) &&
					    (r->source_pad == pad))
						stream = r->source_stream;
					else
						continue;

					printf("\t%s Stream %u\n",which ? "Active" : "Try",
					       stream);

					printf("\ttest %s VIDIOC_SUBDEV_ENUM_MBUS_CODE/FRAME_SIZE/FRAME_INTERVAL: %s\n",
					       which ? "Active" : "Try",
					       ok(testSubDevEnum(&node, which, pad, stream)));
					printf("\ttest %s VIDIOC_SUBDEV_G/S_FMT: %s\n",
					       which ? "Active" : "Try",
					       ok(testSubDevFormat(&node, which, pad, stream)));
					printf("\ttest %s VIDIOC_SUBDEV_G/S_SELECTION/CROP: %s\n",
					       which ? "Active" : "Try",
					       ok(testSubDevSelection(&node, which, pad, stream)));
					if (which)
						printf("\ttest %s VIDIOC_SUBDEV_G/S_FRAME_INTERVAL: %s\n",
						       which ? "Active" : "Try",
						       ok(testSubDevFrameInterval(&node, which, pad, stream)));
				}
			}

			/*
			 * These tests do not make sense for subdevs with multiplexed streams,
			 * as the try & active cases may have different routing and thus different
			 * behavior.
			 */
			if (!has_routes) {
				if (node.has_subdev_enum_code && node.has_subdev_enum_code < 3)
					fail("VIDIOC_SUBDEV_ENUM_MBUS_CODE: try/active mismatch\n");
				if (node.has_subdev_enum_fsize && node.has_subdev_enum_fsize < 3)
					fail("VIDIOC_SUBDEV_ENUM_FRAME_SIZE: try/active mismatch\n");
				if (node.has_subdev_enum_fival && node.has_subdev_enum_fival < 3)
					fail("VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL: try/active mismatch\n");
				if (node.has_subdev_fmt && node.has_subdev_fmt < 3)
					fail("VIDIOC_SUBDEV_G/S_FMT: try/active mismatch\n");
				if (node.has_subdev_selection && node.has_subdev_selection < 3)
					fail("VIDIOC_SUBDEV_G/S_SELECTION: try/active mismatch\n");
				if (node.has_subdev_selection &&
				    node.has_subdev_selection != node.has_subdev_fmt)
					fail("VIDIOC_SUBDEV_G/S_SELECTION: fmt/selection mismatch\n");
				if (node.has_ival_uses_which()) {
					if (node.has_subdev_frame_interval && node.has_subdev_frame_interval < 3)
						fail("VIDIOC_SUBDEV_G/S_FRAME_INTERVAL: try/active mismatch\n");
					if (node.has_subdev_frame_interval &&
					    node.has_subdev_frame_interval != node.has_subdev_fmt)
						fail("VIDIOC_SUBDEV_G/S_FRAME_INTERVAL: fmt/frame_interval mismatch\n");
				}
			}
			printf("\n");
		}
	}

	max_io = node.inputs > node.outputs ? node.inputs : node.outputs;

	for (unsigned io = 0; io < (max_io ? max_io : 1); io++) {
		v4l2_fract min_period = { 1, 1000 };
		char suffix[100] = "";

		node.std_controls = node.priv_controls = 0;
		node.std_compound_controls = node.priv_compound_controls = 0;
		node.controls.clear();
		node.frmsizes.clear();
		node.frmsizes_count.clear();
		node.has_frmintervals = false;
		node.has_enc_cap_frame_interval = false;
		node.valid_buftypes = 0;
		node.valid_memorytype = 0;
		node.buf_caps = 0;
		for (auto &buftype_pixfmt : node.buftype_pixfmts)
			buftype_pixfmt.clear();

		if (max_io) {
			sprintf(suffix, " (%s %u)",
				node.can_capture ? "Input" : "Output", io);
			if (node.can_capture) {
				struct v4l2_input descr;

				doioctl(&node, VIDIOC_S_INPUT, &io);
				descr.index = io;
				doioctl(&node, VIDIOC_ENUMINPUT, &descr);
				node.cur_io_caps = descr.capabilities;
			} else {
				struct v4l2_output descr;

				doioctl(&node, VIDIOC_S_OUTPUT, &io);
				descr.index = io;
				doioctl(&node, VIDIOC_ENUMOUTPUT, &descr);
				node.cur_io_caps = descr.capabilities;
			}
		}

		/* Control ioctls */

		printf("Control ioctls%s:\n", suffix);
		printf("\ttest VIDIOC_QUERY_EXT_CTRL/QUERYMENU: %s\n", ok(testQueryExtControls(&node)));
		printf("\ttest VIDIOC_QUERYCTRL: %s\n", ok(testQueryControls(&node)));
		printf("\ttest VIDIOC_G/S_CTRL: %s\n", ok(testSimpleControls(&node)));
		printf("\ttest VIDIOC_G/S/TRY_EXT_CTRLS: %s\n", ok(testExtendedControls(&node)));
		printf("\ttest VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: %s\n", ok(testEvents(&node)));
		printf("\ttest VIDIOC_G/S_JPEGCOMP: %s\n", ok(testJpegComp(&node)));
		printf("\tStandard Controls: %d Private Controls: %d\n",
		       node.std_controls - node.std_compound_controls,
		       node.priv_controls - node.priv_compound_controls);
		if (node.std_compound_controls || node.priv_compound_controls)
			printf("\tStandard Compound Controls: %d Private Compound Controls: %d\n",
			       node.std_compound_controls, node.priv_compound_controls);
		printf("\n");

		/* Format ioctls */

		printf("Format ioctls%s:\n", suffix);
		printf("\ttest VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: %s\n", ok(testEnumFormats(&node)));
		printf("\ttest VIDIOC_G/S_PARM: %s\n", ok(testParm(&node)));
		printf("\ttest VIDIOC_G_FBUF: %s\n", ok(testFBuf(&node)));
		printf("\ttest VIDIOC_G_FMT: %s\n", ok(testGetFormats(&node)));
		printf("\ttest VIDIOC_TRY_FMT: %s\n", ok(testTryFormats(&node)));
		printf("\ttest VIDIOC_S_FMT: %s\n", ok(testSetFormats(&node)));
		printf("\ttest VIDIOC_G_SLICED_VBI_CAP: %s\n", ok(testSlicedVBICap(&node)));
		printf("\ttest Cropping: %s\n", ok(testCropping(&node)));
		printf("\ttest Composing: %s\n", ok(testComposing(&node)));
		printf("\ttest Scaling: %s\n", ok(testScaling(&node)));
		printf("\n");

		/* Codec ioctls */

		printf("Codec ioctls%s:\n", suffix);
		printf("\ttest VIDIOC_(TRY_)ENCODER_CMD: %s\n", ok(testEncoder(&node)));
		printf("\ttest VIDIOC_G_ENC_INDEX: %s\n", ok(testEncIndex(&node)));
		printf("\ttest VIDIOC_(TRY_)DECODER_CMD: %s\n", ok(testDecoder(&node)));
		printf("\n");

		/* Buffer ioctls */

		printf("Buffer ioctls%s:\n", suffix);
		printf("\ttest VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: %s\n", ok(testReqBufs(&node)));
		printf("\ttest CREATE_BUFS maximum buffers: %s\n", ok(testCreateBufsMax(&node)));
		printf("\ttest VIDIOC_REMOVE_BUFS: %s\n", ok(testRemoveBufs(&node)));
		// Reopen after each streaming test to reset the streaming state
		// in case of any errors in the preceeding test.
		node.reopen();
		printf("\ttest VIDIOC_EXPBUF: %s\n", ok(testExpBuf(&node)));
		if (node.can_capture)
			node.set_interval(min_period);
		printf("\ttest Requests: %s\n", ok(testRequests(&node, options[OptStreaming])));
		if (sizeof(void *) == 4)
			printf("\ttest TIME32/64: %s\n", ok(testTime32_64(&node)));
		// Reopen after each streaming test to reset the streaming state
		// in case of any errors in the preceeding test.
		node.reopen();
		printf("\n");
	}

	cur_io = node.has_inputs ? state.input.index : state.output.index;
	min_io = 0;

	if (!options[OptStreamAllIO]) {
		min_io = cur_io;
		max_io = min_io + 1;
	}

	for (unsigned io = min_io; io < (max_io ? max_io : 1); io++) {
		restoreState();

		if (!node.is_v4l2())
			break;

		if (options[OptStreaming] || (node.is_video && options[OptStreamAllFormats]) ||
		    (node.is_video && node.can_capture && options[OptStreamAllColorTest]))
			printf("Test %s %d:\n\n",
				node.can_capture ? "input" : "output", io);

		if (node.can_capture)
			doioctl(&node, VIDIOC_S_INPUT, &io);
		else
			doioctl(&node, VIDIOC_S_OUTPUT, &io);

		if (options[OptStreaming]) {
			printf("Streaming ioctls:\n");
			streamingSetup(&node);

			printf("\ttest read/write: %s\n", ok(testReadWrite(&node)));
			// Reopen after each streaming test to reset the streaming state
			// in case of any errors in the preceeding test.
			node.reopen();
			printf("\ttest blocking wait: %s\n", ok(testBlockingWait(&node)));
			node.reopen();
			if (!(node.codec_mask & (STATEFUL_ENCODER | STATEFUL_DECODER))) {
				printf("\ttest MMAP (no poll): %s\n",
				       ok(testMmap(&node, &node_m2m_cap, frame_count, POLL_MODE_NONE)));
				node.reopen();
			}
			printf("\ttest MMAP (select): %s\n",
			       ok(testMmap(&node, &node_m2m_cap, frame_count, POLL_MODE_SELECT)));
			node.reopen();
			printf("\ttest MMAP (epoll): %s\n",
			       ok(testMmap(&node, &node_m2m_cap, frame_count, POLL_MODE_EPOLL)));
			node.reopen();
			if (!(node.codec_mask & (STATEFUL_ENCODER | STATEFUL_DECODER))) {
				printf("\ttest USERPTR (no poll): %s\n",
				       ok(testUserPtr(&node, &node_m2m_cap, frame_count, POLL_MODE_NONE)));
				node.reopen();
			}
			printf("\ttest USERPTR (select): %s\n",
			       ok(testUserPtr(&node, &node_m2m_cap, frame_count, POLL_MODE_SELECT)));
			node.reopen();
			if (options[OptSetExpBufDevice] ||
			    !(node.valid_memorytype & (1 << V4L2_MEMORY_DMABUF))) {
				if (!(node.codec_mask & (STATEFUL_ENCODER | STATEFUL_DECODER))) {
					printf("\ttest DMABUF (no poll): %s\n",
					       ok(testDmaBuf(&expbuf_node, &node, &node_m2m_cap,
							     frame_count, POLL_MODE_NONE)));
					node.reopen();
				}
				printf("\ttest DMABUF (select): %s\n",
				       ok(testDmaBuf(&expbuf_node, &node, &node_m2m_cap, frame_count, POLL_MODE_SELECT)));
				node.reopen();
			} else if (!options[OptSetExpBufDevice]) {
				printf("\ttest DMABUF: Cannot test, specify --expbuf-device\n");
			}

			printf("\n");
		}

		if (node.is_video && options[OptStreamAllFormats]) {
			printf("Stream using all formats:\n");

			if (node.is_m2m) {
				if (node.codec_mask &
				    (JPEG_DECODER | STATEFUL_DECODER | STATELESS_DECODER)) {
					printf("\tNot supported for decoder devices\n");
				} else {
					streamM2MAllFormats(&node, all_fmt_frame_count);
				}
			} else {
				streamingSetup(&node);
				streamAllFormats(&node, all_fmt_frame_count);
			}
		}

		if (node.is_video && node.can_capture && options[OptStreamAllColorTest]) {
			printf("Stream using all formats and do a color check:\n");

			if (node.is_m2m) {
				printf("\tNot supported for M2M devices\n");
			} else {
				streamingSetup(&node);
				testColorsAllFormats(&node, color_component,
						     color_skip, color_perc);
			}
		}
	}

	/*
	 * TODO: VIDIOC_S_FBUF/OVERLAY
	 * 	 S_SELECTION flags tests
	 */

	if (is_vivid &&
	    node.controls.find(VIVID_CID_DISCONNECT) != node.controls.end()) {
		if (node.node2)
			node.node2->close();
		node.node2 = nullptr;
		printf("\ttest Disconnect: %s\n\n", ok(testVividDisconnect(&node)));
	}

	restoreState();

show_total:
	/* Final test report */
	if (driver.empty())
		printf("Total for device %s%s: %d, Succeeded: %d, Failed: %d, Warnings: %d\n",
		       node.device, node.g_direct() ? "" : " (using libv4l2)",
		       tests_total, tests_ok, tests_total - tests_ok, warnings);
	else
		printf("Total for %s device %s%s: %d, Succeeded: %d, Failed: %d, Warnings: %d\n",
		       driver.c_str(), node.device, node.g_direct() ? "" : " (using libv4l2)",
		       tests_total, tests_ok, tests_total - tests_ok, warnings);
	grand_total += tests_total;
	grand_ok += tests_ok;
	grand_warnings += warnings;

	if (node.is_media() && options[OptSetMediaDevice]) {
		walkTopology(node, expbuf_node,
			     frame_count, all_fmt_frame_count);
		/* Final test report */
		printf("\nGrand Total for %s device %s: %d, Succeeded: %d, Failed: %d, Warnings: %d\n",
		       driver.c_str(), node.device,
		       grand_total, grand_ok, grand_total - grand_ok, grand_warnings);
	}

	node.close();
	if (node.node2)
		node.node2->close();
}

int main(int argc, char **argv)
{
	int i;
	struct node node;
	media_type type = MEDIA_TYPE_UNKNOWN;
	struct node expbuf_node;
	std::string media_bus_info;
	const char *env_media_apps_color = getenv("MEDIA_APPS_COLOR");

	/* command args */
	int ch;
	std::string device("/dev/video0");
	std::string expbuf_device;	/* --expbuf-device device */
	unsigned frame_count = 60;
	unsigned all_fmt_frame_count = 0;
	char short_options[26 * 2 * 3 + 1];
	char *value, *subs;
	int idx = 0;

	if (!env_media_apps_color || !strcmp(env_media_apps_color, "auto"))
		show_colors = isatty(STDOUT_FILENO);
	else if (!strcmp(env_media_apps_color, "always"))
		show_colors = true;
	else if (!strcmp(env_media_apps_color, "never"))
		show_colors = false;
	else {
		fprintf(stderr,
			"v4l2-compliance: invalid value for MEDIA_APPS_COLOR environment variable\n");
	}

	for (i = 0; long_options[i].name; i++) {
		if (!isalpha(long_options[i].val))
			continue;
		short_options[idx++] = long_options[i].val;
		if (long_options[i].has_arg == required_argument) {
			short_options[idx++] = ':';
		} else if (long_options[i].has_arg == optional_argument) {
			short_options[idx++] = ':';
			short_options[idx++] = ':';
		}
	}
	while (true) {
		int option_index = 0;

		short_options[idx] = 0;
		ch = getopt_long(argc, argv, short_options,
				 long_options, &option_index);
		if (ch == -1)
			break;

		options[ch] = 1;
		if (!option_index) {
			for (i = 0; long_options[i].val; i++) {
				if (long_options[i].val == ch) {
					option_index = i;
					break;
				}
			}
		}
		if (long_options[option_index].has_arg == optional_argument &&
		    !optarg && argv[optind] && argv[optind][0] != '-')
			optarg = argv[optind++];

		switch (ch) {
		case OptHelp:
			usage();
			std::exit(EXIT_FAILURE);
		case OptSetDevice:
			device = make_devname(optarg, "video", media_bus_info);
			break;
		case OptSetVbiDevice:
			device = make_devname(optarg, "vbi", media_bus_info);
			break;
		case OptSetRadioDevice:
			device = make_devname(optarg, "radio", media_bus_info);
			break;
		case OptSetSWRadioDevice:
			device = make_devname(optarg, "swradio", media_bus_info);
			break;
		case OptSetTouchDevice:
			device = make_devname(optarg, "v4l-touch", media_bus_info);
			break;
		case OptSetSubDevDevice:
			device = make_devname(optarg, "v4l-subdev", media_bus_info);
			break;
		case OptSetMediaDevice:
		case OptSetMediaDeviceOnly:
			device = make_devname(optarg, "media", optarg, true);
			type = MEDIA_TYPE_MEDIA;
			break;
		case OptMediaBusInfo:
			media_bus_info = optarg;
			break;
		case OptSetExpBufDevice:
			expbuf_device = make_devname(optarg, "video", media_bus_info);
			break;
		case OptStreaming:
			if (optarg)
				frame_count = strtoul(optarg, nullptr, 0);
			break;
		case OptStreamFrom:
		case OptStreamFromHdr: {
			char *equal = std::strchr(optarg, '=');
			bool has_hdr = ch == OptStreamFromHdr;

			if (equal == optarg)
				equal = nullptr;
			if (equal) {
				*equal = '\0';
				stream_from_map[optarg] = equal + 1;
				stream_hdr_map[optarg] = has_hdr;
			} else {
				stream_from_map[""] = optarg;
				stream_hdr_map[""] = has_hdr;
			}
			break;
		}
		case OptStreamAllFormats:
			if (optarg)
				all_fmt_frame_count = strtoul(optarg, nullptr, 0);
			break;
		case OptStreamAllColorTest:
			subs = optarg;
			while (*subs != '\0') {
				static constexpr const char *subopts[] = {
					"color",
					"skip",
					"perc",
					nullptr
				};

				switch (parse_subopt(&subs, subopts, &value)) {
				case 0:
					if (!strcmp(value, "red"))
						color_component = 0;
					else if (!strcmp(value, "green"))
						color_component = 1;
					else if (!strcmp(value, "blue"))
						color_component = 2;
					else {
						usage();
						std::exit(EXIT_FAILURE);
					}
					break;
				case 1:
					color_skip = strtoul(value, nullptr, 0);
					break;
				case 2:
					color_perc = strtoul(value, nullptr, 0);
					if (color_perc == 0)
						color_perc = 90;
					if (color_perc > 100)
						color_perc = 100;
					break;
				default:
					usage();
					std::exit(EXIT_FAILURE);
				}
			}
			break;
		case OptColor:
			if (!strcmp(optarg, "always"))
				show_colors = true;
			else if (!strcmp(optarg, "never"))
				show_colors = false;
			else if (!strcmp(optarg, "auto"))
				show_colors = isatty(STDOUT_FILENO);
			else {
				usage();
				std::exit(EXIT_FAILURE);
			}
			break;
		case OptNoWarnings:
			show_warnings = false;
			break;
		case OptExitOnWarn:
			exit_on_warn = true;
			break;
		case OptExitOnFail:
			exit_on_fail = true;
			break;
		case OptVerbose:
			show_info = true;
			break;
		case OptNoProgress:
			no_progress = true;
			break;
		case OptVersion:
			print_sha();
			std::exit(EXIT_SUCCESS);
		case ':':
			fprintf(stderr, "Option `%s' requires a value\n",
				argv[optind]);
			usage();
			std::exit(EXIT_FAILURE);
		case '?':
			if (argv[optind])
				fprintf(stderr, "Unknown argument `%s'\n",
					argv[optind]);
			usage();
			std::exit(EXIT_FAILURE);
		}
	}
	if (optind < argc) {
		fprintf(stderr, "unknown arguments: ");
		while (optind < argc)
			fprintf(stderr, "%s ", argv[optind++]);
		fprintf(stderr, "\n");
		usage();
		std::exit(EXIT_FAILURE);
	}

	print_sha();
	printf("\n");

	bool direct = !options[OptUseWrapper];
	int fd;

	if (type == MEDIA_TYPE_UNKNOWN)
		type = mi_media_detect_type(device.c_str());
	if (type == MEDIA_TYPE_CANT_STAT) {
		fprintf(stderr, "Cannot open device %s, exiting.\n",
			device.c_str());
		std::exit(EXIT_FAILURE);
	}
	if (type == MEDIA_TYPE_UNKNOWN) {
		fprintf(stderr, "Unable to detect what device %s is, exiting.\n",
			device.c_str());
		std::exit(EXIT_FAILURE);
	}

	node.device = device.c_str();
	node.s_trace(options[OptTrace]);
	switch (type) {
	case MEDIA_TYPE_MEDIA:
		node.s_direct(true);
		fd = node.media_open(device.c_str(), false);
		break;
	case MEDIA_TYPE_SUBDEV:
		node.s_direct(true);
		fd = node.subdev_open(device.c_str(), false);
		break;
	default:
		node.s_direct(direct);
		fd = node.open(device.c_str(), false);
		break;
	}
	if (fd < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", device.c_str(),
			strerror(errno));
		std::exit(EXIT_FAILURE);
	}

	if (!expbuf_device.empty()) {
		expbuf_node.s_trace(options[OptTrace]);
		expbuf_node.s_direct(true);
		fd = expbuf_node.open(expbuf_device.c_str(), false);
		if (fd < 0) {
			fprintf(stderr, "Failed to open %s: %s\n", expbuf_device.c_str(),
				strerror(errno));
			std::exit(EXIT_FAILURE);
		}
	}

	testNode(node, node, expbuf_node, type, frame_count, all_fmt_frame_count);

	if (!expbuf_device.empty())
		expbuf_node.close();
	if (media_fd >= 0)
		close(media_fd);

	std::exit(app_result);
}
