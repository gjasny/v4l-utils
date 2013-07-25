/*
    Copyright (C) 2003-2004  Kevin Thayer <nufan_wfk at yahoo dot com>

    Cleanup and VBI and audio in/out options, introduction in v4l-dvb,
    support for most new APIs since 2006.
    Copyright (C) 2004, 2006, 2007  Hans Verkuil <hverkuil@xs4all.nl>

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
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <dirent.h>
#include <math.h>
#include <config.h>

#ifdef HAVE_SYS_KLOG_H
#include <sys/klog.h>
#endif

#include <linux/videodev2.h>
#include <libv4l2.h>

#include <list>
#include <vector>
#include <map>
#include <string>
#include <algorithm>


#include "v4l2-ctl.h"

char options[OptLast];

static int app_result;
int verbose;

unsigned capabilities;

static struct option long_options[] = {
	{"list-audio-inputs", no_argument, 0, OptListAudioInputs},
	{"list-audio-outputs", no_argument, 0, OptListAudioOutputs},
	{"all", no_argument, 0, OptAll},
	{"device", required_argument, 0, OptSetDevice},
	{"get-fmt-video", no_argument, 0, OptGetVideoFormat},
	{"set-fmt-video", required_argument, 0, OptSetVideoFormat},
	{"try-fmt-video", required_argument, 0, OptTryVideoFormat},
	{"get-fmt-video-mplane", no_argument, 0, OptGetVideoMplaneFormat},
	{"set-fmt-video-mplane", required_argument, 0, OptSetVideoMplaneFormat},
	{"try-fmt-video-mplane", required_argument, 0, OptTryVideoMplaneFormat},
	{"get-fmt-video-out", no_argument, 0, OptGetVideoOutFormat},
	{"set-fmt-video-out", required_argument, 0, OptSetVideoOutFormat},
	{"try-fmt-video-out", required_argument, 0, OptTryVideoOutFormat},
	{"get-fmt-video-out-mplane", no_argument, 0, OptGetVideoOutMplaneFormat},
	{"set-fmt-video-out-mplane", required_argument, 0, OptSetVideoOutMplaneFormat},
	{"try-fmt-video-out-mplane", required_argument, 0, OptTryVideoOutMplaneFormat},
	{"help", no_argument, 0, OptHelp},
	{"help-tuner", no_argument, 0, OptHelpTuner},
	{"help-io", no_argument, 0, OptHelpIO},
	{"help-stds", no_argument, 0, OptHelpStds},
	{"help-vidcap", no_argument, 0, OptHelpVidCap},
	{"help-vidout", no_argument, 0, OptHelpVidOut},
	{"help-overlay", no_argument, 0, OptHelpOverlay},
	{"help-vbi", no_argument, 0, OptHelpVbi},
	{"help-selection", no_argument, 0, OptHelpSelection},
	{"help-misc", no_argument, 0, OptHelpMisc},
	{"help-streaming", no_argument, 0, OptHelpStreaming},
	{"help-all", no_argument, 0, OptHelpAll},
	{"wrapper", no_argument, 0, OptUseWrapper},
	{"concise", no_argument, 0, OptConcise},
	{"get-output", no_argument, 0, OptGetOutput},
	{"set-output", required_argument, 0, OptSetOutput},
	{"list-outputs", no_argument, 0, OptListOutputs},
	{"list-inputs", no_argument, 0, OptListInputs},
	{"get-input", no_argument, 0, OptGetInput},
	{"set-input", required_argument, 0, OptSetInput},
	{"get-audio-input", no_argument, 0, OptGetAudioInput},
	{"set-audio-input", required_argument, 0, OptSetAudioInput},
	{"get-audio-output", no_argument, 0, OptGetAudioOutput},
	{"set-audio-output", required_argument, 0, OptSetAudioOutput},
	{"get-freq", no_argument, 0, OptGetFreq},
	{"set-freq", required_argument, 0, OptSetFreq},
	{"list-standards", no_argument, 0, OptListStandards},
	{"list-formats", no_argument, 0, OptListFormats},
	{"list-formats-mplane", no_argument, 0, OptListMplaneFormats},
	{"list-formats-ext", no_argument, 0, OptListFormatsExt},
	{"list-formats-ext-mplane", no_argument, 0, OptListMplaneFormatsExt},
	{"list-framesizes", required_argument, 0, OptListFrameSizes},
	{"list-frameintervals", required_argument, 0, OptListFrameIntervals},
	{"list-formats-overlay", no_argument, 0, OptListOverlayFormats},
	{"list-formats-out", no_argument, 0, OptListOutFormats},
	{"list-formats-out-mplane", no_argument, 0, OptListOutMplaneFormats},
	{"get-standard", no_argument, 0, OptGetStandard},
	{"set-standard", required_argument, 0, OptSetStandard},
	{"get-detected-standard", no_argument, 0, OptQueryStandard},
	{"get-parm", no_argument, 0, OptGetParm},
	{"set-parm", required_argument, 0, OptSetParm},
	{"get-output-parm", no_argument, 0, OptGetOutputParm},
	{"set-output-parm", required_argument, 0, OptSetOutputParm},
	{"info", no_argument, 0, OptGetDriverInfo},
	{"list-ctrls", no_argument, 0, OptListCtrls},
	{"list-ctrls-menus", no_argument, 0, OptListCtrlsMenus},
	{"set-ctrl", required_argument, 0, OptSetCtrl},
	{"get-ctrl", required_argument, 0, OptGetCtrl},
	{"get-tuner", no_argument, 0, OptGetTuner},
	{"set-tuner", required_argument, 0, OptSetTuner},
	{"list-freq-bands", no_argument, 0, OptListFreqBands},
	{"silent", no_argument, 0, OptSilent},
	{"verbose", no_argument, 0, OptVerbose},
	{"log-status", no_argument, 0, OptLogStatus},
	{"get-fmt-overlay", no_argument, 0, OptGetOverlayFormat},
	{"set-fmt-overlay", required_argument, 0, OptSetOverlayFormat},
	{"try-fmt-overlay", required_argument, 0, OptTryOverlayFormat},
	{"get-fmt-output-overlay", no_argument, 0, OptGetOutputOverlayFormat},
	{"set-fmt-output-overlay", required_argument, 0, OptSetOutputOverlayFormat},
	{"try-fmt-output-overlay", required_argument, 0, OptTryOutputOverlayFormat},
	{"get-fmt-sliced-vbi", no_argument, 0, OptGetSlicedVbiFormat},
	{"set-fmt-sliced-vbi", required_argument, 0, OptSetSlicedVbiFormat},
	{"try-fmt-sliced-vbi", required_argument, 0, OptTrySlicedVbiFormat},
	{"get-fmt-sliced-vbi-out", no_argument, 0, OptGetSlicedVbiOutFormat},
	{"set-fmt-sliced-vbi-out", required_argument, 0, OptSetSlicedVbiOutFormat},
	{"try-fmt-sliced-vbi-out", required_argument, 0, OptTrySlicedVbiOutFormat},
	{"get-fmt-vbi", no_argument, 0, OptGetVbiFormat},
	{"get-fmt-vbi-out", no_argument, 0, OptGetVbiOutFormat},
	{"get-sliced-vbi-cap", no_argument, 0, OptGetSlicedVbiCap},
	{"get-sliced-vbi-out-cap", no_argument, 0, OptGetSlicedVbiOutCap},
	{"get-fbuf", no_argument, 0, OptGetFBuf},
	{"set-fbuf", required_argument, 0, OptSetFBuf},
	{"get-cropcap", no_argument, 0, OptGetCropCap},
	{"get-crop", no_argument, 0, OptGetCrop},
	{"set-crop", required_argument, 0, OptSetCrop},
	{"get-cropcap-output", no_argument, 0, OptGetOutputCropCap},
	{"get-crop-output", no_argument, 0, OptGetOutputCrop},
	{"set-crop-output", required_argument, 0, OptSetOutputCrop},
	{"get-cropcap-overlay", no_argument, 0, OptGetOverlayCropCap},
	{"get-crop-overlay", no_argument, 0, OptGetOverlayCrop},
	{"set-crop-overlay", required_argument, 0, OptSetOverlayCrop},
	{"get-cropcap-output-overlay", no_argument, 0, OptGetOutputOverlayCropCap},
	{"get-crop-output-overlay", no_argument, 0, OptGetOutputOverlayCrop},
	{"set-crop-output-overlay", required_argument, 0, OptSetOutputOverlayCrop},
	{"get-selection", required_argument, 0, OptGetSelection},
	{"set-selection", required_argument, 0, OptSetSelection},
	{"get-selection-output", required_argument, 0, OptGetOutputSelection},
	{"set-selection-output", required_argument, 0, OptSetOutputSelection},
	{"get-jpeg-comp", no_argument, 0, OptGetJpegComp},
	{"set-jpeg-comp", required_argument, 0, OptSetJpegComp},
	{"get-modulator", no_argument, 0, OptGetModulator},
	{"set-modulator", required_argument, 0, OptSetModulator},
	{"get-priority", no_argument, 0, OptGetPriority},
	{"set-priority", required_argument, 0, OptSetPriority},
	{"wait-for-event", required_argument, 0, OptWaitForEvent},
	{"poll-for-event", required_argument, 0, OptPollForEvent},
	{"overlay", required_argument, 0, OptOverlay},
	{"sleep", required_argument, 0, OptSleep},
	{"list-devices", no_argument, 0, OptListDevices},
	{"list-dv-timings", no_argument, 0, OptListDvTimings},
	{"query-dv-timings", no_argument, 0, OptQueryDvTimings},
	{"get-dv-timings", no_argument, 0, OptGetDvTimings},
	{"set-dv-bt-timings", required_argument, 0, OptSetDvBtTimings},
	{"get-dv-timings-cap", no_argument, 0, OptGetDvTimingsCap},
	{"freq-seek", required_argument, 0, OptFreqSeek},
	{"encoder-cmd", required_argument, 0, OptEncoderCmd},
	{"try-encoder-cmd", required_argument, 0, OptTryEncoderCmd},
	{"decoder-cmd", required_argument, 0, OptDecoderCmd},
	{"try-decoder-cmd", required_argument, 0, OptTryDecoderCmd},
	{"tuner-index", required_argument, 0, OptTunerIndex},
	{"list-buffers", no_argument, 0, OptListBuffers},
	{"list-buffers-out", no_argument, 0, OptListBuffersOut},
	{"list-buffers-vbi", no_argument, 0, OptListBuffersVbi},
	{"list-buffers-sliced-vbi", no_argument, 0, OptListBuffersSlicedVbi},
	{"list-buffers-vbi-out", no_argument, 0, OptListBuffersVbiOut},
	{"list-buffers-sliced-vbi-out", no_argument, 0, OptListBuffersSlicedVbiOut},
	{"stream-count", required_argument, 0, OptStreamCount},
	{"stream-skip", required_argument, 0, OptStreamSkip},
	{"stream-loop", no_argument, 0, OptStreamLoop},
	{"stream-poll", no_argument, 0, OptStreamPoll},
	{"stream-to", required_argument, 0, OptStreamTo},
	{"stream-mmap", optional_argument, 0, OptStreamMmap},
	{"stream-user", optional_argument, 0, OptStreamUser},
	{"stream-from", required_argument, 0, OptStreamFrom},
	{"stream-pattern", required_argument, 0, OptStreamPattern},
	{"stream-out-mmap", optional_argument, 0, OptStreamOutMmap},
	{"stream-out-user", optional_argument, 0, OptStreamOutUser},
	{0, 0, 0, 0}
};

static void usage_all(void)
{
       common_usage();
       tuner_usage();
       io_usage();
       stds_usage();
       vidcap_usage();
       vidout_usage();
       overlay_usage();
       vbi_usage();
       selection_usage();
       misc_usage();
       streaming_usage();
}

static int test_open(const char *file, int oflag)
{
 	return options[OptUseWrapper] ? v4l2_open(file, oflag) : open(file, oflag);
}

static int test_close(int fd)
{
	return options[OptUseWrapper] ? v4l2_close(fd) : close(fd);
}

int test_ioctl(int fd, int cmd, void *arg)
{
	return options[OptUseWrapper] ? v4l2_ioctl(fd, cmd, arg) : ioctl(fd, cmd, arg);
}

int doioctl_name(int fd, unsigned long int request, void *parm, const char *name)
{
	int retval = test_ioctl(fd, request, parm);

	if (retval < 0)
		app_result = -1;
	if (options[OptSilent]) return retval;
	if (retval < 0)
		printf("%s: failed: %s\n", name, strerror(errno));
	else if (verbose)
		printf("%s: ok\n", name);

	return retval;
}

static std::string num2s(unsigned num)
{
	char buf[10];

	sprintf(buf, "%08x", num);
	return buf;
}

std::string buftype2s(int type)
{
	switch (type) {
	case 0:
		return "Invalid";
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
	default:
		return "Unknown (" + num2s(type) + ")";
	}
}

std::string fcc2s(unsigned int val)
{
	std::string s;

	s += val & 0xff;
	s += (val >> 8) & 0xff;
	s += (val >> 16) & 0xff;
	s += (val >> 24) & 0xff;
	return s;
}

std::string field2s(int val)
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

std::string colorspace2s(int val)
{
	switch (val) {
	case V4L2_COLORSPACE_SMPTE170M:
		return "Broadcast NTSC/PAL (SMPTE170M/ITU601)";
	case V4L2_COLORSPACE_SMPTE240M:
		return "1125-Line (US) HDTV (SMPTE240M)";
	case V4L2_COLORSPACE_REC709:
		return "HDTV and modern devices (ITU709)";
	case V4L2_COLORSPACE_BT878:
		return "Broken Bt878";
	case V4L2_COLORSPACE_470_SYSTEM_M:
		return "NTSC/M (ITU470/ITU601)";
	case V4L2_COLORSPACE_470_SYSTEM_BG:
		return "PAL/SECAM BG (ITU470/ITU601)";
	case V4L2_COLORSPACE_JPEG:
		return "JPEG (JFIF/ITU601)";
	case V4L2_COLORSPACE_SRGB:
		return "SRGB";
	default:
		return "Unknown (" + num2s(val) + ")";
	}
}

std::string flags2s(unsigned val, const flag_def *def)
{
	std::string s;

	while (def->flag) {
		if (val & def->flag) {
			if (s.length()) s += ", ";
			s += def->str;
			val &= ~def->flag;
		}
		def++;
	}
	if (val) {
		if (s.length()) s += ", ";
		s += num2s(val);
	}
	return s;
}


static const flag_def service_def[] = {
	{ V4L2_SLICED_TELETEXT_B,  "teletext" },
	{ V4L2_SLICED_VPS,         "vps" },
	{ V4L2_SLICED_CAPTION_525, "cc" },
	{ V4L2_SLICED_WSS_625,     "wss" },
	{ 0, NULL }
};

std::string service2s(unsigned service)
{
	return flags2s(service, service_def);
}

void printfmt(const struct v4l2_format &vfmt)
{
	const flag_def vbi_def[] = {
		{ V4L2_VBI_UNSYNC,     "unsynchronized" },
		{ V4L2_VBI_INTERLACED, "interlaced" },
		{ 0, NULL }
	};
	printf("Format %s:\n", buftype2s(vfmt.type).c_str());

	switch (vfmt.type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		printf("\tWidth/Height  : %u/%u\n", vfmt.fmt.pix.width, vfmt.fmt.pix.height);
		printf("\tPixel Format  : '%s'\n", fcc2s(vfmt.fmt.pix.pixelformat).c_str());
		printf("\tField         : %s\n", field2s(vfmt.fmt.pix.field).c_str());
		printf("\tBytes per Line: %u\n", vfmt.fmt.pix.bytesperline);
		printf("\tSize Image    : %u\n", vfmt.fmt.pix.sizeimage);
		printf("\tColorspace    : %s\n", colorspace2s(vfmt.fmt.pix.colorspace).c_str());
		if (vfmt.fmt.pix.priv)
			printf("\tCustom Info   : %08x\n", vfmt.fmt.pix.priv);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		printf("\tWidth/Height      : %u/%u\n", vfmt.fmt.pix_mp.width, vfmt.fmt.pix_mp.height);
		printf("\tPixel Format      : '%s'\n", fcc2s(vfmt.fmt.pix_mp.pixelformat).c_str());
		printf("\tField             : %s\n", field2s(vfmt.fmt.pix_mp.field).c_str());
		printf("\tNumber of planes  : %u\n", vfmt.fmt.pix_mp.num_planes);
		printf("\tColorspace        : %s\n", colorspace2s(vfmt.fmt.pix_mp.colorspace).c_str());
		for (int i = 0; i < vfmt.fmt.pix_mp.num_planes && i < VIDEO_MAX_PLANES; i++) {
			printf("\tPlane %d           :\n", i);
			printf("\t   Bytes per Line : %u\n", vfmt.fmt.pix_mp.plane_fmt[i].bytesperline);
			printf("\t   Size Image     : %u\n", vfmt.fmt.pix_mp.plane_fmt[i].sizeimage);
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		printf("\tLeft/Top    : %d/%d\n",
				vfmt.fmt.win.w.left, vfmt.fmt.win.w.top);
		printf("\tWidth/Height: %d/%d\n",
				vfmt.fmt.win.w.width, vfmt.fmt.win.w.height);
		printf("\tField       : %s\n", field2s(vfmt.fmt.win.field).c_str());
		printf("\tChroma Key  : 0x%08x\n", vfmt.fmt.win.chromakey);
		printf("\tGlobal Alpha: 0x%02x\n", vfmt.fmt.win.global_alpha);
		printf("\tClip Count  : %u\n", vfmt.fmt.win.clipcount);
		printf("\tClip Bitmap : %s\n", vfmt.fmt.win.bitmap ? "Yes" : "No");
		break;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		printf("\tSampling Rate   : %u Hz\n", vfmt.fmt.vbi.sampling_rate);
		printf("\tOffset          : %u samples (%g secs after leading edge)\n",
				vfmt.fmt.vbi.offset,
				(double)vfmt.fmt.vbi.offset / (double)vfmt.fmt.vbi.sampling_rate);
		printf("\tSamples per Line: %u\n", vfmt.fmt.vbi.samples_per_line);
		printf("\tSample Format   : %s\n", fcc2s(vfmt.fmt.vbi.sample_format).c_str());
		printf("\tStart 1st Field : %u\n", vfmt.fmt.vbi.start[0]);
		printf("\tCount 1st Field : %u\n", vfmt.fmt.vbi.count[0]);
		printf("\tStart 2nd Field : %u\n", vfmt.fmt.vbi.start[1]);
		printf("\tCount 2nd Field : %u\n", vfmt.fmt.vbi.count[1]);
		if (vfmt.fmt.vbi.flags)
			printf("\tFlags           : %s\n", flags2s(vfmt.fmt.vbi.flags, vbi_def).c_str());
		break;
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		printf("\tService Set    : %s\n",
				service2s(vfmt.fmt.sliced.service_set).c_str());
		for (int i = 0; i < 24; i++) {
			printf("\tService Line %2d: %8s / %-8s\n", i,
			       service2s(vfmt.fmt.sliced.service_lines[0][i]).c_str(),
			       service2s(vfmt.fmt.sliced.service_lines[1][i]).c_str());
		}
		printf("\tI/O Size       : %u\n", vfmt.fmt.sliced.io_size);
		break;
	}
}

static const flag_def fmtdesc_def[] = {
	{ V4L2_FMT_FLAG_COMPRESSED, "compressed" },
	{ V4L2_FMT_FLAG_EMULATED, "emulated" },
	{ 0, NULL }
};

std::string fmtdesc2s(unsigned flags)
{
	return flags2s(flags, fmtdesc_def);
}

void print_video_formats(int fd, enum v4l2_buf_type type)
{
	struct v4l2_fmtdesc fmt;

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = type;
	while (test_ioctl(fd, VIDIOC_ENUM_FMT, &fmt) >= 0) {
		printf("\tIndex       : %d\n", fmt.index);
		printf("\tType        : %s\n", buftype2s(type).c_str());
		printf("\tPixel Format: '%s'", fcc2s(fmt.pixelformat).c_str());
		if (fmt.flags)
			printf(" (%s)", fmtdesc2s(fmt.flags).c_str());
		printf("\n");
		printf("\tName        : %s\n", fmt.description);
		printf("\n");
		fmt.index++;
	}
}

static std::string cap2s(unsigned cap)
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
	if (cap & V4L2_CAP_TUNER)
		s += "\t\tTuner\n";
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
	if (cap & V4L2_CAP_DEVICE_CAPS)
		s += "\t\tDevice Capabilities\n";
	return s;
}

int parse_subopt(char **subs, const char * const *subopts, char **value)
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

static std::string partstd2s(const char *prefix, const char *stds[], unsigned long long std)
{
	std::string s = std::string(prefix) + "-";
	int first = 1;

	while (*stds) {
		if (std & 1) {
			if (!first)
				s += "/";
			first = 0;
			s += *stds;
		}
		stds++;
		std >>= 1;
	}
	return s;
}

static const char *std_pal[] = {
	"B", "B1", "G", "H", "I", "D", "D1", "K",
	"M", "N", "Nc", "60",
	NULL
};
static const char *std_ntsc[] = {
	"M", "M-JP", "443", "M-KR",
	NULL
};
static const char *std_secam[] = {
	"B", "D", "G", "H", "K", "K1", "L", "Lc",
	NULL
};
static const char *std_atsc[] = {
	"8-VSB", "16-VSB",
	NULL
};

std::string std2s(v4l2_std_id std)
{
	std::string s;

	if (std & 0xfff) {
		s += partstd2s("PAL", std_pal, std);
	}
	if (std & 0xf000) {
		if (s.length()) s += " ";
		s += partstd2s("NTSC", std_ntsc, std >> 12);
	}
	if (std & 0xff0000) {
		if (s.length()) s += " ";
		s += partstd2s("SECAM", std_secam, std >> 16);
	}
	if (std & 0xf000000) {
		if (s.length()) s += " ";
		s += partstd2s("ATSC", std_atsc, std >> 24);
	}
	return s;
}

void print_v4lstd(v4l2_std_id std)
{
	if (std & 0xfff) {
		printf("\t%s\n", partstd2s("PAL", std_pal, std).c_str());
	}
	if (std & 0xf000) {
		printf("\t%s\n", partstd2s("NTSC", std_ntsc, std >> 12).c_str());
	}
	if (std & 0xff0000) {
		printf("\t%s\n", partstd2s("SECAM", std_secam, std >> 16).c_str());
	}
	if (std & 0xf000000) {
		printf("\t%s\n", partstd2s("ATSC", std_atsc, std >> 24).c_str());
	}
}

int parse_fmt(char *optarg, __u32 &width, __u32 &height, __u32 &pixelformat)
{
	char *value, *subs;
	int fmts = 0;

	subs = optarg;
	while (*subs != '\0') {
		static const char *const subopts[] = {
			"width",
			"height",
			"pixelformat",
			NULL
		};

		switch (parse_subopt(&subs, subopts, &value)) {
		case 0:
			width = strtol(value, 0L, 0);
			fmts |= FmtWidth;
			break;
		case 1:
			height = strtol(value, 0L, 0);
			fmts |= FmtHeight;
			break;
		case 2:
			if (strlen(value) == 4)
				pixelformat =
					v4l2_fourcc(value[0], value[1],
							value[2], value[3]);
			else
				pixelformat = strtol(value, 0L, 0);
			fmts |= FmtPixelFormat;
			break;
		default:
			return 0;
		}
	}
	return fmts;
}


static void print_event(const struct v4l2_event *ev)
{
	printf("%ld.%06ld: event %u, pending %u: ",
			ev->timestamp.tv_sec, ev->timestamp.tv_nsec / 1000,
			ev->sequence, ev->pending);
	switch (ev->type) {
	case V4L2_EVENT_VSYNC:
		printf("vsync %s\n", field2s(ev->u.vsync.field).c_str());
		break;
	case V4L2_EVENT_EOS:
		printf("eos\n");
		break;
	case V4L2_EVENT_CTRL:
		common_control_event(ev);
		break;
	case V4L2_EVENT_FRAME_SYNC:
		printf("frame_sync %d\n", ev->u.frame_sync.frame_sequence);
		break;
	default:
		if (ev->type >= V4L2_EVENT_PRIVATE_START)
			printf("unknown private event (%08x)\n", ev->type);
		else
			printf("unknown event (%08x)\n", ev->type);
		break;
	}
}

static __u32 parse_event(const char *e, const char **name)
{
	__u32 event = 0;

	*name = NULL;
	if (isdigit(e[0]))
		event = strtoul(e, 0L, 0);
	else if (!strcmp(e, "eos"))
		event = V4L2_EVENT_EOS;
	else if (!strcmp(e, "vsync"))
		event = V4L2_EVENT_VSYNC;
	else if (!strcmp(e, "frame_sync"))
		event = V4L2_EVENT_FRAME_SYNC;
	else if (!strncmp(e, "ctrl=", 5)) {
		event = V4L2_EVENT_CTRL;
		*name = e + 5;
	}

	if (event == 0) {
		fprintf(stderr, "Unknown event\n");
		misc_usage();
		exit(1);
	}
	return event;
}

__u32 find_pixel_format(int fd, unsigned index, bool output, bool mplane)
{
	struct v4l2_fmtdesc fmt;

	fmt.index = index;
	if (output)
		fmt.type = mplane ?  V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE :
			V4L2_BUF_TYPE_VIDEO_OUTPUT;
	else
		fmt.type = mplane ?  V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
			V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (doioctl(fd, VIDIOC_ENUM_FMT, &fmt))
		return 0;
	return fmt.pixelformat;
}

int main(int argc, char **argv)
{
	int i;

	int fd = -1;

	/* command args */
	int ch;
	const char *device = "/dev/video0";	/* -d device */
	struct v4l2_capability vcap;	/* list_cap */
	__u32 wait_for_event = 0;	/* wait for this event */
	const char *wait_event_id = NULL;
	__u32 poll_for_event = 0;	/* poll for this event */
	const char *poll_event_id = NULL;
	unsigned secs = 0;
	char short_options[26 * 2 * 2 + 1];
	int idx = 0;

	memset(&vcap, 0, sizeof(vcap));

	if (argc == 1) {
		common_usage();
		return 0;
	}
	for (i = 0; long_options[i].name; i++) {
		if (!isalpha(long_options[i].val))
			continue;
		short_options[idx++] = long_options[i].val;
		if (long_options[i].has_arg == required_argument)
			short_options[idx++] = ':';
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
			common_usage();
			return 0;
		case OptHelpTuner:
			tuner_usage();
			return 0;
		case OptHelpIO:
			io_usage();
			return 0;
		case OptHelpStds:
			stds_usage();
			return 0;
		case OptHelpVidCap:
			vidcap_usage();
			return 0;
		case OptHelpVidOut:
			vidout_usage();
			return 0;
		case OptHelpOverlay:
			overlay_usage();
			return 0;
		case OptHelpVbi:
			vbi_usage();
			return 0;
		case OptHelpSelection:
			selection_usage();
			return 0;
		case OptHelpMisc:
			misc_usage();
			return 0;
		case OptHelpStreaming:
			streaming_usage();
			return 0;
		case OptHelpAll:
			usage_all();
			return 0;
		case OptSetDevice:
			device = optarg;
			if (device[0] >= '0' && device[0] <= '9' && strlen(device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/video%s", device);
				device = newdev;
			}
			break;
		case OptWaitForEvent:
			wait_for_event = parse_event(optarg, &wait_event_id);
			if (wait_for_event == 0)
				return 1;
			break;
		case OptPollForEvent:
			poll_for_event = parse_event(optarg, &poll_event_id);
			if (poll_for_event == 0)
				return 1;
			break;
		case OptSleep:
			secs = strtoul(optarg, 0L, 0);
			break;
		case ':':
			fprintf(stderr, "Option '%s' requires a value\n",
					argv[optind]);
			common_usage();
			return 1;
		case '?':
			if (argv[optind])
				fprintf(stderr, "Unknown argument '%s'\n", argv[optind]);
			common_usage();
			return 1;
		default:
			common_cmd(ch, optarg);
			tuner_cmd(ch, optarg);
			io_cmd(ch, optarg);
			stds_cmd(ch, optarg);
			vidcap_cmd(ch, optarg);
			vidout_cmd(ch, optarg);
			overlay_cmd(ch, optarg);
			vbi_cmd(ch, optarg);
			selection_cmd(ch, optarg);
			misc_cmd(ch, optarg);
			streaming_cmd(ch, optarg);
			break;
		}
	}
	if (optind < argc) {
		printf("unknown arguments: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
		common_usage();
		return 1;
	}

	if ((fd = test_open(device, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", device,
			strerror(errno));
		exit(1);
	}

	verbose = options[OptVerbose];
	doioctl(fd, VIDIOC_QUERYCAP, &vcap);
	capabilities = vcap.capabilities;
	if (capabilities & V4L2_CAP_DEVICE_CAPS)
		capabilities = vcap.device_caps;

	common_process_controls(fd);

	if (wait_for_event == V4L2_EVENT_CTRL && wait_event_id)
		if (!common_find_ctrl_id(wait_event_id)) {
			fprintf(stderr, "unknown control '%s'\n", wait_event_id);
			exit(1);
		}
	if (poll_for_event == V4L2_EVENT_CTRL && poll_event_id)
		if (!common_find_ctrl_id(poll_event_id)) {
			fprintf(stderr, "unknown control '%s'\n", poll_event_id);
			exit(1);
		}

	if (options[OptAll]) {
		options[OptGetVideoFormat] = 1;
		options[OptGetCrop] = 1;
		options[OptGetVideoOutFormat] = 1;
		options[OptGetDriverInfo] = 1;
		options[OptGetInput] = 1;
		options[OptGetOutput] = 1;
		options[OptGetAudioInput] = 1;
		options[OptGetAudioOutput] = 1;
		options[OptGetStandard] = 1;
		options[OptGetParm] = 1;
		options[OptGetOutputParm] = 1;
		options[OptGetFreq] = 1;
		options[OptGetTuner] = 1;
		options[OptGetModulator] = 1;
		options[OptGetOverlayFormat] = 1;
		options[OptGetOutputOverlayFormat] = 1;
		options[OptGetVbiFormat] = 1;
		options[OptGetVbiOutFormat] = 1;
		options[OptGetSlicedVbiFormat] = 1;
		options[OptGetSlicedVbiOutFormat] = 1;
		options[OptGetFBuf] = 1;
		options[OptGetCropCap] = 1;
		options[OptGetOutputCropCap] = 1;
		options[OptGetJpegComp] = 1;
		options[OptGetDvTimings] = 1;
		options[OptGetDvTimingsCap] = 1;
		options[OptGetPriority] = 1;
		options[OptGetSelection] = 1;
		options[OptGetOutputSelection] = 1;
		options[OptListCtrls] = 1;
		options[OptSilent] = 1;
	}

	/* Information Opts */

	if (options[OptGetDriverInfo]) {
		printf("Driver Info (%susing libv4l2):\n",
				options[OptUseWrapper] ? "" : "not ");
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
	}

	/* Set options */

	common_set(fd);
	tuner_set(fd);
	io_set(fd);
	stds_set(fd);
	vidcap_set(fd);
	vidout_set(fd);
	overlay_set(fd);
	vbi_set(fd);
	selection_set(fd);
	streaming_set(fd);
	misc_set(fd);

	/* Get options */

	common_get(fd);
	tuner_get(fd);
	io_get(fd);
	stds_get(fd);
	vidcap_get(fd);
	vidout_get(fd);
	overlay_get(fd);
	vbi_get(fd);
	selection_get(fd);
	misc_get(fd);

	/* List options */

	common_list(fd);
	io_list(fd);
	stds_list(fd);
	vidcap_list(fd);
	vidout_list(fd);
	overlay_list(fd);
	vbi_list(fd);
	streaming_list(fd);

	if (options[OptWaitForEvent]) {
		struct v4l2_event_subscription sub;
		struct v4l2_event ev;

		memset(&sub, 0, sizeof(sub));
		sub.type = wait_for_event;
		if (wait_for_event == V4L2_EVENT_CTRL)
			sub.id = common_find_ctrl_id(wait_event_id);
		if (!doioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub))
			if (!doioctl(fd, VIDIOC_DQEVENT, &ev))
				print_event(&ev);
	}

	if (options[OptPollForEvent]) {
		struct v4l2_event_subscription sub;
		struct v4l2_event ev;

		memset(&sub, 0, sizeof(sub));
		sub.flags = V4L2_EVENT_SUB_FL_SEND_INITIAL;
		sub.type = poll_for_event;
		if (poll_for_event == V4L2_EVENT_CTRL)
			sub.id = common_find_ctrl_id(poll_event_id);
		if (!doioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub)) {
			fd_set fds;
			__u32 seq = 0;

			fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
			while (1) {
				int res;

				FD_ZERO(&fds);
				FD_SET(fd, &fds);
				res = select(fd + 1, NULL, NULL, &fds, NULL);
				if (res <= 0)
					break;
				if (!doioctl(fd, VIDIOC_DQEVENT, &ev)) {
					print_event(&ev);
					if (ev.sequence > seq)
						printf("\tMissed %d events\n",
							ev.sequence - seq);
					seq = ev.sequence + 1;
				}
			}
		}
	}

	if (options[OptSleep]) {
		sleep(secs);
		printf("Test VIDIOC_QUERYCAP:\n");
		if (test_ioctl(fd, VIDIOC_QUERYCAP, &vcap) == 0)
			printf("\tDriver name   : %s\n", vcap.driver);
		else
			perror("VIDIOC_QUERYCAP");
	}

	test_close(fd);
	exit(app_result);
}
