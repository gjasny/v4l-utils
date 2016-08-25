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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <math.h>
#include <sys/utsname.h>
#include <signal.h>
#include <vector>

#include "v4l2-compliance.h"
#ifndef ANDROID
#include "version.h"
#endif

/* Short option list

   Please keep in alphabetical order.
   That makes it easier to see which short options are still free.

   In general the lower case is used to set something and the upper
   case is used to retrieve a setting. */
enum Option {
	OptStreamAllIO = 'a',
	OptStreamAllColorTest = 'c',
	OptSetDevice = 'd',
	OptSetExpBufDevice = 'e',
	OptStreamAllFormats = 'f',
	OptHelp = 'h',
	OptNoWarnings = 'n',
	OptSetRadioDevice = 'r',
	OptStreaming = 's',
	OptSetSWRadioDevice = 'S',
	OptSetTouchDevice = 't',
	OptTrace = 'T',
	OptVerbose = 'v',
	OptSetVbiDevice = 'V',
	OptUseWrapper = 'w',
	OptLast = 256
};

static char options[OptLast];

static int app_result;
static int tests_total, tests_ok;

// Globals
bool show_info;
bool show_warnings = true;
int kernel_version;
unsigned warnings;

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
	{"device", required_argument, 0, OptSetDevice},
	{"radio-device", required_argument, 0, OptSetRadioDevice},
	{"vbi-device", required_argument, 0, OptSetVbiDevice},
	{"sdr-device", required_argument, 0, OptSetSWRadioDevice},
	{"expbuf-device", required_argument, 0, OptSetExpBufDevice},
	{"touch-device", required_argument, 0, OptSetTouchDevice},
	{"help", no_argument, 0, OptHelp},
	{"verbose", no_argument, 0, OptVerbose},
	{"no-warnings", no_argument, 0, OptNoWarnings},
	{"trace", no_argument, 0, OptTrace},
#ifndef NO_LIBV4L2
	{"wrapper", no_argument, 0, OptUseWrapper},
#endif
	{"streaming", optional_argument, 0, OptStreaming},
	{"stream-all-formats", no_argument, 0, OptStreamAllFormats},
	{"stream-all-io", no_argument, 0, OptStreamAllIO},
	{"stream-all-color", required_argument, 0, OptStreamAllColorTest},
	{0, 0, 0, 0}
};

static void usage(void)
{
	printf("Usage:\n");
	printf("Common options:\n");
	printf("  -d, --device=<dev> Use device <dev> as the video device.\n");
	printf("                     If <dev> starts with a digit, then /dev/video<dev> is used.\n");
	printf("  -V, --vbi-device=<dev>\n");
	printf("                     Use device <dev> as the vbi device.\n");
	printf("                     If <dev> starts with a digit, then /dev/vbi<dev> is used.\n");
	printf("  -r, --radio-device=<dev>\n");
	printf("                     Use device <dev> as the radio device.\n");
	printf("                     If <dev> starts with a digit, then /dev/radio<dev> is used.\n");
	printf("  -S, --sdr-device=<dev>\n");
	printf("                     Use device <dev> as the SDR device.\n");
	printf("                     If <dev> starts with a digit, then /dev/swradio<dev> is used.\n");
	printf("  -t, --touch-device=<dev>\n");
	printf("                     Use device <dev> as the touch device.\n");
	printf("                     If <dev> starts with a digit, then /dev/v4l-touch<dev> is used.\n");
	printf("  -e, --expbuf-device=<dev>\n");
	printf("                     Use device <dev> to obtain DMABUF handles.\n");
	printf("                     If <dev> starts with a digit, then /dev/video<dev> is used.\n");
	printf("                     only /dev/videoX devices are supported.\n");
	printf("  -s, --streaming=<count>\n");
	printf("                     Enable the streaming tests. Set <count> to the number of\n");
	printf("                     frames to stream (default 60). Requires a valid input/output\n");
	printf("                     and frequency (when dealing with a tuner). For DMABUF testing\n");
	printf("                     --expbuf-device needs to be set as well.\n");
	printf("  -f, --stream-all-formats\n");
	printf("                     Test streaming all available formats.\n");
	printf("                     This attempts to stream using MMAP mode or read/write\n");
	printf("                     for one second for all formats, at all sizes, at all intervals\n");
	printf("                     and with all field values.\n");
	printf("  -a, --stream-all-io\n");
	printf("                     Do streaming tests for all inputs or outputs instead of just\n");
	printf("                     the current input or output. This requires that a valid video\n");
	printf("                     signal is present on all inputs and all outputs are hooked up.\n");
	printf("  -c, --stream-all-color=color=red|green|blue,skip=<skip>,perc=<percentage>\n");
	printf("                     For all formats stream <skip + 1> frames and check if\n");
	printf("                     the last frame has at least <perc> percent of the pixels with\n");
	printf("                     a <color> component that is higher than the other two color\n");
	printf("                     components. This requires that a valid red, green or blue video\n");
	printf("                     signal is present on the input(s). If <skip> is not specified,\n");
	printf("                     then just capture the first frame. If <perc> is not specified,\n");
	printf("                     then this defaults to 90%%.\n");
	printf("  -h, --help         Display this help message.\n");
	printf("  -n, --no-warnings  Turn off warning messages.\n");
	printf("  -T, --trace        Trace all called ioctls.\n");
	printf("  -v, --verbose      Turn on verbose reporting.\n");
#ifndef NO_LIBV4L2
	printf("  -w, --wrapper      Use the libv4l2 wrapper library.\n");
#endif
	exit(0);
}

std::string cap2s(unsigned cap)
{
	std::string s;

	if (cap & V4L2_CAP_VIDEO_CAPTURE)
		s += "\t\tVideo Capture\n";
	if (cap & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		s += "\t\tVideo Capture Multiplanar\n";
	if (cap & V4L2_CAP_VIDEO_OUTPUT)
		s += "\t\tVideo Output\n";
	if (cap & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
		s += "\t\tVideo Output Multiplanar\n";
	if (cap & V4L2_CAP_VIDEO_M2M)
		s += "\t\tVideo Memory-to-Memory\n";
	if (cap & V4L2_CAP_VIDEO_M2M_MPLANE)
		s += "\t\tVideo Memory-to-Memory Multiplanar\n";
	if (cap & V4L2_CAP_VIDEO_OVERLAY)
		s += "\t\tVideo Overlay\n";
	if (cap & V4L2_CAP_VIDEO_OUTPUT_OVERLAY)
		s += "\t\tVideo Output Overlay\n";
	if (cap & V4L2_CAP_VBI_CAPTURE)
		s += "\t\tVBI Capture\n";
	if (cap & V4L2_CAP_VBI_OUTPUT)
		s += "\t\tVBI Output\n";
	if (cap & V4L2_CAP_SLICED_VBI_CAPTURE)
		s += "\t\tSliced VBI Capture\n";
	if (cap & V4L2_CAP_SLICED_VBI_OUTPUT)
		s += "\t\tSliced VBI Output\n";
	if (cap & V4L2_CAP_RDS_CAPTURE)
		s += "\t\tRDS Capture\n";
	if (cap & V4L2_CAP_RDS_OUTPUT)
		s += "\t\tRDS Output\n";
	if (cap & V4L2_CAP_SDR_CAPTURE)
		s += "\t\tSDR Capture\n";
	if (cap & V4L2_CAP_SDR_OUTPUT)
		s += "\t\tSDR Output\n";
	if (cap & V4L2_CAP_TOUCH)
		s += "\t\tTouch Device\n";
	if (cap & V4L2_CAP_TUNER)
		s += "\t\tTuner\n";
	if (cap & V4L2_CAP_HW_FREQ_SEEK)
		s += "\t\tHW Frequency Seek\n";
	if (cap & V4L2_CAP_MODULATOR)
		s += "\t\tModulator\n";
	if (cap & V4L2_CAP_AUDIO)
		s += "\t\tAudio\n";
	if (cap & V4L2_CAP_RADIO)
		s += "\t\tRadio\n";
	if (cap & V4L2_CAP_READWRITE)
		s += "\t\tRead/Write\n";
	if (cap & V4L2_CAP_ASYNCIO)
		s += "\t\tAsync I/O\n";
	if (cap & V4L2_CAP_STREAMING)
		s += "\t\tStreaming\n";
	if (cap & V4L2_CAP_EXT_PIX_FORMAT)
		s += "\t\tExtended Pix Format\n";
	if (cap & V4L2_CAP_DEVICE_CAPS)
		s += "\t\tDevice Capabilities\n";
	return s;
}

std::string fcc2s(unsigned int val)
{
	std::string s;

	s += val & 0x7f;
	s += (val >> 8) & 0x7f;
	s += (val >> 16) & 0x7f;
	s += (val >> 24) & 0x7f;
	if (val & (1 << 31))
		s += "-BE";
	return s;
}

std::string buftype2s(int type)
{
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		return "Video Capture";
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		return "Video Capture Multiplanar";
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return "Video Output";
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return "Video Output Multiplanar";
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		return "Video Overlay";
	case V4L2_BUF_TYPE_VBI_CAPTURE:
		return "VBI Capture";
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		return "VBI Output";
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
		return "Sliced VBI Capture";
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		return "Sliced VBI Output";
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		return "Video Output Overlay";
	case V4L2_BUF_TYPE_SDR_CAPTURE:
		return "SDR Capture";
	case V4L2_BUF_TYPE_SDR_OUTPUT:
		return "SDR Output";
	case V4L2_BUF_TYPE_PRIVATE:
		return "Private";
	default:
		return std::string("Unknown");
	}
}

const char *ok(int res)
{
	static char buf[100];

	if (res == ENOTTY) {
		strcpy(buf, "OK (Not Supported)");
		res = 0;
	} else {
		strcpy(buf, "OK");
	}
	tests_total++;
	if (res) {
		app_result = res;
		sprintf(buf, "FAIL");
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
	return check_string((const char *)s, len);
}

int check_0(const void *p, int len)
{
	const __u8 *q = (const __u8 *)p;

	while (len--)
		if (*q++)
			return 1;
	return 0;
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
}

static void signal_handler_interrupt(int signum)
{
	restoreState();
	printf("\n");
	exit(-1);
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
			V4L2_CAP_TUNER | V4L2_CAP_SDR_CAPTURE;
	const __u32 output_caps = V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			V4L2_CAP_VIDEO_OUTPUT_OVERLAY | V4L2_CAP_VBI_OUTPUT |
			V4L2_CAP_SDR_OUTPUT | V4L2_CAP_SLICED_VBI_OUTPUT |
			V4L2_CAP_MODULATOR;
	const __u32 overlay_caps = V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_VIDEO_OUTPUT_OVERLAY;
	const __u32 m2m_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE;
	const __u32 io_caps = V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;
	const __u32 mplane_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
		V4L2_CAP_VIDEO_M2M_MPLANE;
	const __u32 splane_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OUTPUT |
		V4L2_CAP_VIDEO_M2M;

	memset(&vcap, 0xff, sizeof(vcap));
	// Must always be there
	fail_on_test(doioctl(node, VIDIOC_QUERYCAP, &vcap));
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
	    memcmp(vcap.bus_info, "rmi4:", 5))
		return fail("missing bus_info prefix ('%s')\n", vcap.bus_info);
	fail_on_test((vcap.version >> 16) < 3);
	fail_on_test(check_0(vcap.reserved, sizeof(vcap.reserved)));
	caps = vcap.capabilities;
	dcaps = vcap.device_caps;
	node->is_m2m = dcaps & m2m_caps;
	fail_on_test(caps == 0);
	fail_on_test(caps & V4L2_CAP_ASYNCIO);
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

static void streamingSetup(struct node *node)
{
	if (node->can_capture) {
		struct v4l2_input input;

		memset(&input, 0, sizeof(input));
		doioctl(node, VIDIOC_G_INPUT, &input.index);
		doioctl(node, VIDIOC_ENUMINPUT, &input);
		node->cur_io_caps = input.capabilities;
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
	int opt = getsubopt(subs, (char * const *)subopts, value);

	if (opt == -1) {
		fprintf(stderr, "Invalid suboptions specified\n");
		return -1;
	}
	if (*value == NULL) {
		fprintf(stderr, "No value given to suboption <%s>\n",
				subopts[opt]);
		return -1;
	}
	return opt;
}

int main(int argc, char **argv)
{
	int i;
	struct node node;
	struct node video_node;
	struct node video_node2;
	struct node vbi_node;
	struct node vbi_node2;
	struct node radio_node;
	struct node radio_node2;
	struct node sdr_node;
	struct node sdr_node2;
	struct node touch_node;
	struct node touch_node2;
	struct node expbuf_node;

	/* command args */
	int ch;
	const char *device = NULL;
	const char *video_device = NULL;	/* -d device */
	const char *vbi_device = NULL;		/* -V device */
	const char *radio_device = NULL;	/* -r device */
	const char *sdr_device = NULL;		/* -S device */
	const char *touch_device = NULL;	/* -t device */
	const char *expbuf_device = NULL;	/* --expbuf-device device */
	struct v4l2_capability vcap;		/* list_cap */
	unsigned frame_count = 60;
	char short_options[26 * 2 * 3 + 1];
	char *value, *subs;
	int idx = 0;

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
	while (1) {
		int option_index = 0;

		short_options[idx] = 0;
		ch = getopt_long(argc, argv, short_options,
				 long_options, &option_index);
		if (ch == -1)
			break;

		options[(int)ch] = 1;
		switch (ch) {
		case OptHelp:
			usage();
			return 0;
		case OptSetDevice:
			video_device = optarg;
			if (video_device[0] >= '0' && video_device[0] <= '9' && strlen(video_device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/video%s", video_device);
				video_device = newdev;
			}
			break;
		case OptSetVbiDevice:
			vbi_device = optarg;
			if (vbi_device[0] >= '0' && vbi_device[0] <= '9' && strlen(vbi_device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/vbi%s", vbi_device);
				vbi_device = newdev;
			}
			break;
		case OptSetRadioDevice:
			radio_device = optarg;
			if (radio_device[0] >= '0' && radio_device[0] <= '9' && strlen(radio_device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/radio%s", radio_device);
				radio_device = newdev;
			}
			break;
		case OptSetSWRadioDevice:
			sdr_device = optarg;
			if (sdr_device[0] >= '0' && sdr_device[0] <= '9' && strlen(sdr_device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/swradio%s", sdr_device);
				sdr_device = newdev;
			}
			break;
		case OptSetTouchDevice:
			touch_device = optarg;
			if (touch_device[0] >= '0' && touch_device[0] <= '9' && strlen(touch_device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/v4l-touch%s", touch_device);
				touch_device = newdev;
			}
			break;
		case OptSetExpBufDevice:
			expbuf_device = optarg;
			if (expbuf_device[0] >= '0' && expbuf_device[0] <= '9' && strlen(expbuf_device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/video%s", expbuf_device);
				expbuf_device = newdev;
			}
			break;
		case OptStreaming:
			if (optarg)
				frame_count = strtoul(optarg, NULL, 0);
			break;
		case OptStreamAllColorTest:
			subs = optarg;
			while (*subs != '\0') {
				static const char *const subopts[] = {
					"color",
					"skip",
					"perc",
					NULL
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
						exit(1);
					}
					break;
				case 1:
					color_skip = strtoul(value, 0L, 0);
					break;
				case 2:
					color_perc = strtoul(value, 0L, 0);
					if (color_perc == 0)
						color_perc = 90;
					if (color_perc > 100)
						color_perc = 100;
					break;
				default:
					usage();
					exit(1);
				}
			}
			break;
		case OptNoWarnings:
			show_warnings = false;
			break;
		case OptVerbose:
			show_info = true;
			break;
		case ':':
			fprintf(stderr, "Option `%s' requires a value\n",
				argv[optind]);
			usage();
			return 1;
		case '?':
			fprintf(stderr, "Unknown argument `%s'\n",
				argv[optind]);
			usage();
			return 1;
		}
	}
	if (optind < argc) {
		printf("unknown arguments: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
		usage();
		return 1;
	}
	bool direct = !options[OptUseWrapper];

	struct utsname uts;
	int v1, v2, v3;
	int fd;

	uname(&uts);
	sscanf(uts.release, "%d.%d.%d", &v1, &v2, &v3);
	if (v1 == 2 && v2 == 6)
		kernel_version = v3;

	if (!video_device && !vbi_device && !radio_device &&
	    !sdr_device && !touch_device)
		video_device = "/dev/video0";

	if (video_device) {
		video_node.s_trace(options[OptTrace]);
		video_node.s_direct(direct);
		fd = video_node.open(video_device, false);
		if (fd < 0) {
			fprintf(stderr, "Failed to open %s: %s\n", video_device,
				strerror(errno));
			exit(1);
		}
	}

	if (vbi_device) {
		vbi_node.s_trace(options[OptTrace]);
		vbi_node.s_direct(direct);
		fd = vbi_node.open(vbi_device, false);
		if (fd < 0) {
			fprintf(stderr, "Failed to open %s: %s\n", vbi_device,
				strerror(errno));
			exit(1);
		}
	}

	if (radio_device) {
		radio_node.s_trace(options[OptTrace]);
		radio_node.s_direct(direct);
		fd = radio_node.open(radio_device, false);
		if (fd < 0) {
			fprintf(stderr, "Failed to open %s: %s\n", radio_device,
					strerror(errno));
			exit(1);
		}
	}

	if (sdr_device) {
		sdr_node.s_trace(options[OptTrace]);
		sdr_node.s_direct(direct);
		fd = sdr_node.open(sdr_device, false);
		if (fd < 0) {
			fprintf(stderr, "Failed to open %s: %s\n", sdr_device,
				strerror(errno));
			exit(1);
		}
	}

	if (touch_device) {
		touch_node.s_trace(options[OptTrace]);
		touch_node.s_direct(direct);
		fd = touch_node.open(touch_device, false);
		if (fd < 0) {
			fprintf(stderr, "Failed to open %s: %s\n", touch_device,
				strerror(errno));
			exit(1);
		}
	}

	if (expbuf_device) {
		expbuf_node.s_trace(options[OptTrace]);
		expbuf_node.s_direct(true);
		fd = expbuf_node.open(expbuf_device, false);
		if (fd < 0) {
			fprintf(stderr, "Failed to open %s: %s\n", expbuf_device,
				strerror(errno));
			exit(1);
		}
	}

	if (video_node.g_fd() >= 0) {
		node = video_node;
		device = video_device;
		node.is_video = true;
	} else if (vbi_node.g_fd() >= 0) {
		node = vbi_node;
		device = vbi_device;
		node.is_vbi = true;
	} else if (radio_node.g_fd() >= 0) {
		node = radio_node;
		device = radio_device;
		node.is_radio = true;
	} else if (sdr_node.g_fd() >= 0) {
		node = sdr_node;
		device = sdr_device;
		node.is_sdr = true;
	} else if (touch_node.g_fd() >= 0) {
		node = touch_node;
		device = touch_device;
		node.is_touch = true;
	}
	node.device = device;

	doioctl(&node, VIDIOC_QUERYCAP, &vcap);
	if (node.g_caps() & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VBI_CAPTURE |
			 V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_SLICED_VBI_CAPTURE))
		node.has_inputs = true;
	if (node.g_caps() & (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VBI_OUTPUT |
			 V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_SLICED_VBI_OUTPUT))
		node.has_outputs = true;
	if (node.g_caps() & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VBI_CAPTURE |
			 V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE |
			 V4L2_CAP_VIDEO_M2M | V4L2_CAP_SLICED_VBI_CAPTURE |
			 V4L2_CAP_RDS_CAPTURE | V4L2_CAP_SDR_CAPTURE))
		node.can_capture = true;
	if (node.g_caps() & (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VBI_OUTPUT |
			 V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE |
			 V4L2_CAP_VIDEO_M2M | V4L2_CAP_SLICED_VBI_OUTPUT |
			 V4L2_CAP_RDS_OUTPUT | V4L2_CAP_SDR_OUTPUT))
		node.can_output = true;
	if (node.g_caps() & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			 V4L2_CAP_VIDEO_M2M_MPLANE))
		node.is_planar = true;

	/* Information Opts */

#ifdef SHA
#define STR(x) #x
#define STRING(x) STR(x)
	printf("v4l2-compliance SHA   : %s\n", STRING(SHA));
#else
	printf("v4l2-compliance SHA   : not available\n");
#endif
	if (kernel_version)
		printf("Running on 2.6.%d\n", kernel_version);

	printf("\nDriver Info:\n");
	printf("\tDriver name   : %s\n", vcap.driver);
	printf("\tCard type     : %s\n", vcap.card);
	printf("\tBus info      : %s\n", vcap.bus_info);
	printf("\tDriver version: %d.%d.%d\n",
			vcap.version >> 16,
			(vcap.version >> 8) & 0xff,
			vcap.version & 0xff);
	printf("\tCapabilities  : 0x%08X\n", vcap.capabilities);
	printf("%s", cap2s(vcap.capabilities).c_str());
	if (vcap.capabilities & V4L2_CAP_DEVICE_CAPS) {
		printf("\tDevice Caps   : 0x%08X\n", vcap.device_caps);
		printf("%s", cap2s(vcap.device_caps).c_str());
	}

	printf("\nCompliance test for device %s (%susing libv4l2):\n\n",
			device, direct ? "not " : "");

	/* Required ioctls */

	printf("Required ioctls:\n");
	printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(&node)));
	printf("\n");

	/* Multiple opens */

	printf("Allow for multiple opens:\n");
	if (video_device) {
		video_node2 = node;
		printf("\ttest second video open: %s\n",
				ok(video_node2.open(video_device, false) >= 0 ? 0 : errno));
		if (video_node2.g_fd() >= 0) {
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(&video_node2)));
			printf("\ttest VIDIOC_G/S_PRIORITY: %s\n",
					ok(testPrio(&node, &video_node2)));
			node.node2 = &video_node2;
		}
	}
	if (vbi_device) {
		vbi_node2 = node;
		printf("\ttest second vbi open: %s\n",
				ok(vbi_node2.open(vbi_device, false) >= 0 ? 0 : errno));
		if (vbi_node2.g_fd() >= 0) {
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(&vbi_node2)));
			printf("\ttest VIDIOC_G/S_PRIORITY: %s\n",
					ok(testPrio(&node, &vbi_node2)));
			node.node2 = &vbi_node2;
		}
	}
	if (radio_device) {
		radio_node2 = node;
		printf("\ttest second radio open: %s\n",
				ok(radio_node2.open(radio_device, false) >= 0 ? 0 : errno));
		if (radio_node2.g_fd() >= 0) {
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(&radio_node2)));
			printf("\ttest VIDIOC_G/S_PRIORITY: %s\n",
					ok(testPrio(&node, &radio_node2)));
			node.node2 = &radio_node2;
		}
	}
	if (sdr_device) {
		sdr_node2 = node;
		printf("\ttest second sdr open: %s\n",
				ok(sdr_node2.open(sdr_device, false) >= 0 ? 0 : errno));
		if (sdr_node2.g_fd() >= 0) {
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(&sdr_node2)));
			printf("\ttest VIDIOC_G/S_PRIORITY: %s\n",
					ok(testPrio(&node, &sdr_node2)));
			node.node2 = &sdr_node2;
		}
	}
	printf("\ttest for unlimited opens: %s\n",
		ok(testUnlimitedOpens(&node)));
	printf("\n");

	storeState(&node);

	/* register signal handler for interrupt signal, to exit gracefully */
	signal(SIGINT, signal_handler_interrupt);

	/* Debug ioctls */

	printf("Debug ioctls:\n");
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

	unsigned max_io = node.inputs > node.outputs ? node.inputs : node.outputs;

	for (unsigned io = 0; io < (max_io ? max_io : 1); io++) {
		node.std_controls = node.priv_controls = 0;
		node.controls.clear();
		node.frmsizes.clear();
		node.frmsizes_count.clear();
		node.has_frmintervals = false;
		for (unsigned idx = 0; idx < V4L2_BUF_TYPE_LAST + 1; idx++)
			node.buftype_pixfmts[idx].clear();

		if (max_io) {
			printf("Test %s %d:\n\n",
				node.can_capture ? "input" : "output", io);
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

		printf("\tControl ioctls:\n");
		printf("\t\ttest VIDIOC_QUERY_EXT_CTRL/QUERYMENU: %s\n", ok(testQueryExtControls(&node)));
		printf("\t\ttest VIDIOC_QUERYCTRL: %s\n", ok(testQueryControls(&node)));
		printf("\t\ttest VIDIOC_G/S_CTRL: %s\n", ok(testSimpleControls(&node)));
		printf("\t\ttest VIDIOC_G/S/TRY_EXT_CTRLS: %s\n", ok(testExtendedControls(&node)));
		printf("\t\ttest VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: %s\n", ok(testEvents(&node)));
		printf("\t\ttest VIDIOC_G/S_JPEGCOMP: %s\n", ok(testJpegComp(&node)));
		printf("\t\tStandard Controls: %d Private Controls: %d\n",
				node.std_controls, node.priv_controls);
		printf("\n");

		/* Format ioctls */

		printf("\tFormat ioctls:\n");
		printf("\t\ttest VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: %s\n", ok(testEnumFormats(&node)));
		printf("\t\ttest VIDIOC_G/S_PARM: %s\n", ok(testParm(&node)));
		printf("\t\ttest VIDIOC_G_FBUF: %s\n", ok(testFBuf(&node)));
		printf("\t\ttest VIDIOC_G_FMT: %s\n", ok(testGetFormats(&node)));
		printf("\t\ttest VIDIOC_TRY_FMT: %s\n", ok(testTryFormats(&node)));
		printf("\t\ttest VIDIOC_S_FMT: %s\n", ok(testSetFormats(&node)));
		printf("\t\ttest VIDIOC_G_SLICED_VBI_CAP: %s\n", ok(testSlicedVBICap(&node)));
		printf("\t\ttest Cropping: %s\n", ok(testCropping(&node)));
		printf("\t\ttest Composing: %s\n", ok(testComposing(&node)));
		printf("\t\ttest Scaling: %s\n", ok(testScaling(&node)));
		printf("\n");

		/* Codec ioctls */

		printf("\tCodec ioctls:\n");
		printf("\t\ttest VIDIOC_(TRY_)ENCODER_CMD: %s\n", ok(testEncoder(&node)));
		printf("\t\ttest VIDIOC_G_ENC_INDEX: %s\n", ok(testEncIndex(&node)));
		printf("\t\ttest VIDIOC_(TRY_)DECODER_CMD: %s\n", ok(testDecoder(&node)));
		printf("\n");

		/* Buffer ioctls */

		printf("\tBuffer ioctls:\n");
		printf("\t\ttest VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: %s\n", ok(testReqBufs(&node)));
		printf("\t\ttest VIDIOC_EXPBUF: %s\n", ok(testExpBuf(&node)));
		printf("\n");
	}

	unsigned cur_io = node.has_inputs ? state.input.index : state.output.index;
	unsigned min_io = 0;

	if (!options[OptStreamAllIO]) {
		min_io = cur_io;
		max_io = min_io + 1;
	}

	for (unsigned io = min_io; io < (max_io ? max_io : 1); io++) {
		restoreState();

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
			printf("\ttest MMAP: %s\n", ok(testMmap(&node, frame_count)));
			node.reopen();
			printf("\ttest USERPTR: %s\n", ok(testUserPtr(&node, frame_count)));
			node.reopen();
			if (options[OptSetExpBufDevice] ||
					!(node.valid_memorytype & (1 << V4L2_MEMORY_DMABUF)))
				printf("\ttest DMABUF: %s\n", ok(testDmaBuf(&expbuf_node, &node, frame_count)));
			else if (!options[OptSetExpBufDevice])
				printf("\ttest DMABUF: Cannot test, specify --expbuf-device\n");
			node.reopen();

			printf("\n");
		}

		if (node.is_video && options[OptStreamAllFormats]) {
			printf("Stream using all formats:\n");

			if (node.is_m2m) {
				printf("\tNot supported for M2M devices\n");
			} else {
				streamingSetup(&node);
				streamAllFormats(&node);
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
	if (touch_device) {
		touch_node2 = node;
		printf("\ttest second touch open: %s\n",
				ok(touch_node2.open(touch_device, false) >= 0 ? 0 : errno));
		if (touch_node2.g_fd() >= 0) {
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(&touch_node2)));
			printf("\ttest VIDIOC_G/S_PRIORITY: %s\n",
					ok(testPrio(&node, &touch_node2)));
			node.node2 = &touch_node2;
		}
	}
	printf("\n");

	/*
	 * TODO: VIDIOC_S_FBUF/OVERLAY
	 * 	 S_SELECTION flags tests
	 */

	restoreState();

	/* Final test report */

	node.close();
	if (node.node2)
		node.node2->close();
	if (expbuf_device)
		expbuf_node.close();
	printf("Total: %d, Succeeded: %d, Failed: %d, Warnings: %d\n",
			tests_total, tests_ok, tests_total - tests_ok, warnings);
	if (!strcmp((const char *)vcap.driver, "vivid") && tests_total - tests_ok > 19) {
		printf("\nThis vivid driver has error injection controls that cause the compliance\n");
	        printf("tests to fail unless you load the vivid module with the no_error_inj=1\n");
		printf("module option to disable those error injection controls. It looks from\n");
		printf("the number of failures that that wasn't done.\n");
	}
	exit(app_result);
}
