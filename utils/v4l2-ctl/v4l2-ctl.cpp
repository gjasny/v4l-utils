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
static int verbose;

unsigned capabilities;

static const flag_def service_def[] = {
	{ V4L2_SLICED_TELETEXT_B,  "teletext" },
	{ V4L2_SLICED_VPS,         "vps" },
	{ V4L2_SLICED_CAPTION_525, "cc" },
	{ V4L2_SLICED_WSS_625,     "wss" },
	{ 0, NULL }
};

/* fmts specified */
#define FmtWidth		(1L<<0)
#define FmtHeight		(1L<<1)
#define FmtChromaKey		(1L<<2)
#define FmtGlobalAlpha		(1L<<3)
#define FmtPixelFormat		(1L<<4)
#define FmtLeft			(1L<<5)
#define FmtTop			(1L<<6)
#define FmtField		(1L<<7)

/* crop specified */
#define CropWidth		(1L<<0)
#define CropHeight		(1L<<1)
#define CropLeft		(1L<<2)
#define CropTop 		(1L<<3)

/* selection specified */
#define SelectionWidth		(1L<<0)
#define SelectionHeight		(1L<<1)
#define SelectionLeft		(1L<<2)
#define SelectionTop 		(1L<<3)
#define SelectionFlags 		(1L<<4)

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
	{"help-all", no_argument, 0, OptHelpAll},
	{"wrapper", no_argument, 0, OptUseWrapper},
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
	{"streamoff", no_argument, 0, OptStreamOff},
	{"streamon", no_argument, 0, OptStreamOn},
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
	{"list-dv-presets", no_argument, 0, OptListDvPresets},
	{"set-dv-presets", required_argument, 0, OptSetDvPreset},
	{"get-dv-presets", no_argument, 0, OptGetDvPreset},
	{"query-dv-presets", no_argument, 0, OptQueryDvPreset},
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
	{0, 0, 0, 0}
};

static void usage_stds(void)
{
	printf("\nStandards/Presets/Timings options:\n"
	       "  --list-standards   display supported video standards [VIDIOC_ENUMSTD]\n"
	       "  -S, --get-standard\n"
	       "                     query the video standard [VIDIOC_G_STD]\n"
	       "  -s, --set-standard=<num>\n"
	       "                     set the video standard to <num> [VIDIOC_S_STD]\n"
	       "                     <num> a numerical v4l2_std value, or one of:\n"
	       "                     pal or pal-X (X = B/G/H/N/Nc/I/D/K/M/60) (V4L2_STD_PAL)\n"
	       "                     ntsc or ntsc-X (X = M/J/K) (V4L2_STD_NTSC)\n"
	       "                     secam or secam-X (X = B/G/H/D/K/L/Lc) (V4L2_STD_SECAM)\n"
	       "  --get-detected-standard\n"
	       "                     display detected input video standard [VIDIOC_QUERYSTD]\n"
	       "  --list-dv-presets  list supported dv presets [VIDIOC_ENUM_DV_PRESETS]\n"
	       "  --set-dv-preset=<num>\n"
	       "                     set the digital video preset to <num> [VIDIOC_S_DV_PRESET]\n"
	       "  --get-dv-preset    query the digital video preset in use [VIDIOC_G_DV_PRESET]\n"
	       "  --query-dv-preset  query the detected dv preset [VIDIOC_QUERY_DV_PRESET]\n"
	       "  --list-dv-timings  list supp. standard dv timings [VIDIOC_ENUM_DV_TIMINGS]\n"
	       "  --set-dv-bt-timings\n"
	       "                     query: use the output of VIDIOC_QUERY_DV_PRESET\n"
	       "                     index=<index>: use the index as provided by --list-dv-presets\n"
	       "                     or give a fully specified timings:\n"
	       "                     width=<width>,height=<height>,interlaced=<0/1>,\n"
	       "                     polarities=<polarities mask>,pixelclock=<pixelclock Hz>,\n"
	       "                     hfp=<horizontal front porch>,hs=<horizontal sync>,\n"
	       "                     hbp=<horizontal back porch>,vfp=<vertical front porch>,\n"
	       "                     vs=<vertical sync>,vbp=<vertical back porch>,\n"
	       "                     il_vfp=<vertical front porch for bottom field>,\n"
	       "                     il_vs=<vertical sync for bottom field>,\n"
	       "                     il_vbp=<vertical back porch for bottom field>,\n"
	       "                     set the digital video timings according to the BT 656/1120\n"
	       "                     standard [VIDIOC_S_DV_TIMINGS]\n"
	       "  --get-dv-timings   get the digital video timings in use [VIDIOC_G_DV_TIMINGS]\n"
	       "  --query-dv-timings query the detected dv timings [VIDIOC_QUERY_DV_TIMINGS]\n"
	       "  --get-dv-timings-cap\n"
	       "                     get the dv timings capabilities [VIDIOC_DV_TIMINGS_CAP]\n"
	       );
}

static void usage_vidcap(void)
{
	printf("\nVideo Capture Formats options:\n"
	       "  --list-formats     display supported video formats [VIDIOC_ENUM_FMT]\n"
	       "  --list-formats-mplane\n"
 	       "                     display supported video multi-planar formats\n"
 	       "                     [VIDIOC_ENUM_FMT]\n"
	       "  --list-formats-ext display supported video formats including frame sizes\n"
	       "                     and intervals\n"
	       "  --list-formats-ext-mplane\n"
	       "                     display supported video multi-planar formats including\n"
	       "                     frame sizes and intervals\n"
	       "  --list-framesizes=<f>\n"
	       "                     list supported framesizes for pixelformat <f>\n"
	       "                     [VIDIOC_ENUM_FRAMESIZES]\n"
	       "                     pixelformat is the fourcc value as a string\n"
	       "  --list-frameintervals=width=<w>,height=<h>,pixelformat=<f>\n"
	       "                     list supported frame intervals for pixelformat <f> and\n"
	       "                     the given width and height [VIDIOC_ENUM_FRAMEINTERVALS]\n"
	       "                     pixelformat is the fourcc value as a string\n"
	       "  -V, --get-fmt-video\n"
	       "     		     query the video capture format [VIDIOC_G_FMT]\n"
	       "  -v, --set-fmt-video=width=<w>,height=<h>,pixelformat=<f>\n"
	       "                     set the video capture format [VIDIOC_S_FMT]\n"
	       "                     pixelformat is either the format index as reported by\n"
	       "                     --list-formats, or the fourcc value as a string\n"
	       "  --try-fmt-video=width=<w>,height=<h>,pixelformat=<f>\n"
	       "                     try the video capture format [VIDIOC_TRY_FMT]\n"
	       "                     pixelformat is either the format index as reported by\n"
	       "                     --list-formats, or the fourcc value as a string\n"
	       "  --get-fmt-video-mplane\n"
	       "     		     query the video capture format through the multi-planar API\n"
	       "                     [VIDIOC_G_FMT]\n"
	       "  --set-fmt-video-mplane\n"
	       "  --try-fmt-video-mplane=width=<w>,height=<h>,pixelformat=<f>\n"
	       "                     set/try the video capture format using the multi-planar API\n"
	       "                     [VIDIOC_S/TRY_FMT]\n"
	       "                     pixelformat is either the format index as reported by\n"
	       "                     --list-formats-mplane, or the fourcc value as a string\n"
	       );
}

static void usage_vidout(void)
{
	printf("\nVideo Output Formats options:\n"
	       "  --list-formats-out display supported video output formats [VIDIOC_ENUM_FMT]\n"
	       "  --get-fmt-video-out\n"
	       "     		     query the video output format [VIDIOC_G_FMT]\n"
	       "  --set-fmt-video-out\n"
	       "  --try-fmt-video-out=width=<w>,height=<h>,pixelformat=<f>\n"
	       "                     set/try the video output format [VIDIOC_TRY_FMT]\n"
	       "                     pixelformat is either the format index as reported by\n"
	       "                     --list-formats-out, or the fourcc value as a string\n"
	       "  --list-formats-out-mplane\n"
 	       "                     display supported video output multi-planar formats\n"
 	       "                     [VIDIOC_ENUM_FMT]\n"
	       "  --get-fmt-video-out-mplane\n"
	       "     		     query the video output format using the multi-planar API\n"
	       "                     [VIDIOC_G_FMT]\n"
	       "  --set-fmt-video-out-mplane\n"
	       "  --try-fmt-video-out-mplane=width=<w>,height=<h>,pixelformat=<f>\n"
	       "                     set/try the video output format with the multi-planar API\n"
	       "                     [VIDIOC_S/TRY_FMT]\n"
	       "                     pixelformat is either the format index as reported by\n"
	       "                     --list-formats-out-mplane, or the fourcc value as a string\n"
	       );
}

static void usage_overlay(void)
{
	printf("\nVideo Overlay options:\n"
	       "  --list-formats-overlay\n"
	       "                     display supported overlay formats [VIDIOC_ENUM_FMT]\n"
	       "  --overlay=<on>     turn overlay on (1) or off (0) (VIDIOC_OVERLAY)\n"
	       "  --get-fmt-overlay  query the video overlay format [VIDIOC_G_FMT]\n"
	       "  --get-fmt-output-overlay\n"
	       "     		     query the video output overlay format [VIDIOC_G_FMT]\n"
	       "  --set-fmt-overlay\n"
	       "  --try-fmt-overlay\n"
	       "  --set-fmt-output-overlay\n"
	       "  --try-fmt-output-overlay=chromakey=<key>,global_alpha=<alpha>,\n"
	       "                           top=<t>,left=<l>,width=<w>,height=<h>,field=<f>\n"
	       "     		     set/try the video or video output overlay format\n"
	       "                     [VIDIOC_S/TRY_FMT], <f> can be one of:\n"
	       "                     any, none, top, bottom, interlaced, seq_tb, seq_bt,\n"
	       "                     alternate, interlaced_tb, interlaced_bt\n"
	       "  --get-fbuf         query the overlay framebuffer data [VIDIOC_G_FBUF]\n"
	       "  --set-fbuf=chromakey=<b>,global_alpha=<b>,local_alpha=<b>,local_inv_alpha=<b>\n"
	       "		     set the overlay framebuffer [VIDIOC_S_FBUF]\n"
	       "                     b = 0 or 1\n"
	       );
}

static void usage_vbi(void)
{
	printf("\nVBI Formats options:\n"
	       "  --get-sliced-vbi-cap\n"
	       "		     query the sliced VBI capture capabilities\n"
	       "                     [VIDIOC_G_SLICED_VBI_CAP]\n"
	       "  --get-sliced-vbi-out-cap\n"
	       "		     query the sliced VBI output capabilities\n"
	       "                     [VIDIOC_G_SLICED_VBI_CAP]\n"
	       "  -B, --get-fmt-sliced-vbi\n"
	       "		     query the sliced VBI capture format [VIDIOC_G_FMT]\n"
	       "  --get-fmt-sliced-vbi-out\n"
	       "		     query the sliced VBI output format [VIDIOC_G_FMT]\n"
	       "  -b, --set-fmt-sliced-vbi\n"
	       "  --try-fmt-sliced-vbi\n"
	       "  --set-fmt-sliced-vbi-out\n"
	       "  --try-fmt-sliced-vbi-out=<mode>\n"
	       "                     set/try the sliced VBI capture/output format to <mode>\n"
	       "                     [VIDIOC_S/TRY_FMT], <mode> is a comma separated list of:\n"
	       "                     off:      turn off sliced VBI (cannot be combined with\n"
	       "                               other modes)\n"
	       "                     teletext: teletext (PAL/SECAM)\n"
	       "                     cc:       closed caption (NTSC)\n"
	       "                     wss:      widescreen signal (PAL/SECAM)\n"
	       "                     vps:      VPS (PAL/SECAM)\n"
	       "  --get-fmt-vbi      query the VBI capture format [VIDIOC_G_FMT]\n"
	       "  --get-fmt-vbi-out  query the VBI output format [VIDIOC_G_FMT]\n"
	       );
}

static void usage_selection(void)
{
	printf("\nSelection/Cropping options:\n"
	       "  --get-cropcap      query the crop capabilities [VIDIOC_CROPCAP]\n"
	       "  --get-crop	     query the video capture crop window [VIDIOC_G_CROP]\n"
	       "  --set-crop=top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video capture crop window [VIDIOC_S_CROP]\n"
	       "  --get-cropcap-output\n"
	       "                     query crop capabilities for video output [VIDIOC_CROPCAP]\n"
	       "  --get-crop-output  query the video output crop window [VIDIOC_G_CROP]\n"
	       "  --set-crop-output=top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video output crop window [VIDIOC_S_CROP]\n"
	       "  --get-cropcap-overlay\n"
	       "                     query crop capabilities for video overlay [VIDIOC_CROPCAP]\n"
	       "  --get-crop-overlay query the video overlay crop window [VIDIOC_G_CROP]\n"
	       "  --set-crop-overlay=top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video overlay crop window [VIDIOC_S_CROP]\n"
	       "  --get-cropcap-output-overlay\n"
	       "                     query the crop capabilities for video output overlays\n"
	       "                     [VIDIOC_CROPCAP]\n"
	       "  --get-crop-output-overlay\n"
	       "                     query the video output overlay crop window [VIDIOC_G_CROP]\n"
	       "  --set-crop-output-overlay=top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video output overlay crop window [VIDIOC_S_CROP]\n"
	       "  --get-selection=target=<target>\n"
	       "                     query the video capture selection rectangle [VIDIOC_G_SELECTION]\n"
	       "                     See --set-selection command for the valid <target> values.\n"
	       "  --set-selection=target=<target>,flags=<flags>,top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video capture selection rectangle [VIDIOC_S_SELECTION]\n"
	       "                     target=crop|crop_bounds|crop_default|compose|compose_bounds|\n"
	       "                            compose_default|compose_padded\n"
	       "                     flags=le|ge\n"
	       "  --get-selection-output=target=<target>\n"
	       "                     query the video output selection rectangle [VIDIOC_G_SELECTION]\n"
	       "                     See --set-selection command for the valid <target> values.\n"
	       "  --set-selection-output=target=<target>,flags=<flags>,top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video output selection rectangle [VIDIOC_S_SELECTION]\n"
	       "                     See --set-selection command for the arguments.\n"
	       );
}

static void usage_misc(void)
{
	printf("\nMiscellaneous options:\n"
	       "  --wait-for-event=<event>\n"
	       "                     wait for an event [VIDIOC_DQEVENT]\n"
	       "                     <event> is the event number or one of:\n"
	       "                     eos, vsync, ctrl=<id>, frame_sync\n"
	       "                     where <id> is the name of the control\n"
	       "  --poll-for-event=<event>\n"
	       "                     poll for an event [VIDIOC_DQEVENT]\n"
	       "                     see --wait-for-event for possible events\n"
	       "  -P, --get-parm     display video parameters [VIDIOC_G_PARM]\n"
	       "  -p, --set-parm=<fps>\n"
	       "                     set video framerate in <fps> [VIDIOC_S_PARM]\n"
	       "  --get-output-parm  display output video parameters [VIDIOC_G_PARM]\n"
	       "  --set-output-parm=<fps>\n"
	       "                     set output video framerate in <fps> [VIDIOC_S_PARM]\n"
	       "  --get-jpeg-comp    query the JPEG compression [VIDIOC_G_JPEGCOMP]\n"
	       "  --set-jpeg-comp=quality=<q>,markers=<markers>,comment=<c>,app<n>=<a>\n"
	       "                     set the JPEG compression [VIDIOC_S_JPEGCOMP]\n"
	       "                     <n> is the app segment: 0-9/a-f, <a> is the actual string.\n"
	       "                     <markers> is a colon separated list of:\n"
	       "                     dht:      Define Huffman Tables\n"
	       "                     dqt:      Define Quantization Tables\n"
	       "                     dri:      Define Restart Interval\n"
	       "  --encoder-cmd=cmd=<cmd>,flags=<flags>\n"
	       "                     Send a command to the encoder [VIDIOC_ENCODER_CMD]\n"
	       "                     cmd=start|stop|pause|resume\n"
	       "                     flags=stop_at_gop_end\n"
	       "  --try-encoder-cmd=cmd=<cmd>,flags=<flags>\n"
	       "                     Try an encoder command [VIDIOC_TRY_ENCODER_CMD]\n"
	       "                     See --encoder-cmd for the arguments.\n"
	       "  --decoder-cmd=cmd=<cmd>,flags=<flags>,stop_pts=<pts>,start_speed=<speed>,\n"
	       "                     start_format=<none|gop>\n"
	       "                     Send a command to the decoder [VIDIOC_DECODER_CMD]\n"
	       "                     cmd=start|stop|pause|resume\n"
	       "                     flags=start_mute_audio|pause_to_black|stop_to_black|\n"
	       "                           stop_immediately\n"
	       "  --try-decoder-cmd=cmd=<cmd>,flags=<flags>\n"
	       "                     Try a decoder command [VIDIOC_TRY_DECODER_CMD]\n"
	       "                     See --decoder-cmd for the arguments.\n"
	       "  --streamoff        turn the stream off [VIDIOC_STREAMOFF]\n"
	       "  --streamon         turn the stream on [VIDIOC_STREAMON]\n"
	       );
}

static void usage_all(void)
{
       common_usage();
       tuner_usage();
       io_usage();
       usage_stds();
       usage_vidcap();
       usage_vidout();
       usage_overlay();
       usage_vbi();
       usage_selection();
       usage_misc();
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

	if (retval < 0) {
		app_result = -1;
	}
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

static std::string buftype2s(int type)
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
	case V4L2_BUF_TYPE_PRIVATE:
		return "Private";
	default:
		return "Unknown (" + num2s(type) + ")";
	}
}

static std::string fcc2s(unsigned int val)
{
	std::string s;

	s += val & 0xff;
	s += (val >> 8) & 0xff;
	s += (val >> 16) & 0xff;
	s += (val >> 24) & 0xff;
	return s;
}

static std::string field2s(int val)
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

static std::string colorspace2s(int val)
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

static void print_sliced_vbi_cap(struct v4l2_sliced_vbi_cap &cap)
{
	printf("\tType           : %s\n", buftype2s(cap.type).c_str());
	printf("\tService Set    : %s\n",
			flags2s(cap.service_set, service_def).c_str());
	for (int i = 0; i < 24; i++) {
		printf("\tService Line %2d: %8s / %-8s\n", i,
				flags2s(cap.service_lines[0][i], service_def).c_str(),
				flags2s(cap.service_lines[1][i], service_def).c_str());
	}
}


static std::string fbufcap2s(unsigned cap)
{
	std::string s;

	if (cap & V4L2_FBUF_CAP_EXTERNOVERLAY)
		s += "\t\t\tExtern Overlay\n";
	if (cap & V4L2_FBUF_CAP_CHROMAKEY)
		s += "\t\t\tChromakey\n";
	if (cap & V4L2_FBUF_CAP_GLOBAL_ALPHA)
		s += "\t\t\tGlobal Alpha\n";
	if (cap & V4L2_FBUF_CAP_LOCAL_ALPHA)
		s += "\t\t\tLocal Alpha\n";
	if (cap & V4L2_FBUF_CAP_LOCAL_INV_ALPHA)
		s += "\t\t\tLocal Inverted Alpha\n";
	if (cap & V4L2_FBUF_CAP_LIST_CLIPPING)
		s += "\t\t\tClipping List\n";
	if (cap & V4L2_FBUF_CAP_BITMAP_CLIPPING)
		s += "\t\t\tClipping Bitmap\n";
	if (s.empty()) s += "\t\t\t\n";
	return s;
}

static std::string fbufflags2s(unsigned fl)
{
	std::string s;

	if (fl & V4L2_FBUF_FLAG_PRIMARY)
		s += "\t\t\tPrimary Graphics Surface\n";
	if (fl & V4L2_FBUF_FLAG_OVERLAY)
		s += "\t\t\tOverlay Matches Capture/Output Size\n";
	if (fl & V4L2_FBUF_FLAG_CHROMAKEY)
		s += "\t\t\tChromakey\n";
	if (fl & V4L2_FBUF_FLAG_GLOBAL_ALPHA)
		s += "\t\t\tGlobal Alpha\n";
	if (fl & V4L2_FBUF_FLAG_LOCAL_ALPHA)
		s += "\t\t\tLocal Alpha\n";
	if (fl & V4L2_FBUF_FLAG_LOCAL_INV_ALPHA)
		s += "\t\t\tLocal Inverted Alpha\n";
	if (s.empty()) s += "\t\t\t\n";
	return s;
}

static void printfbuf(const struct v4l2_framebuffer &fb)
{
	int is_ext = fb.capability & V4L2_FBUF_CAP_EXTERNOVERLAY;

	printf("Framebuffer Format:\n");
	printf("\tCapability    : %s", fbufcap2s(fb.capability).c_str() + 3);
	printf("\tFlags         : %s", fbufflags2s(fb.flags).c_str() + 3);
	if (fb.base)
		printf("\tBase          : 0x%p\n", fb.base);
	printf("\tWidth         : %d\n", fb.fmt.width);
	printf("\tHeight        : %d\n", fb.fmt.height);
	printf("\tPixel Format  : '%s'\n", fcc2s(fb.fmt.pixelformat).c_str());
	if (!is_ext) {
		printf("\tBytes per Line: %d\n", fb.fmt.bytesperline);
		printf("\tSize image    : %d\n", fb.fmt.sizeimage);
		printf("\tColorspace    : %s\n", colorspace2s(fb.fmt.colorspace).c_str());
		if (fb.fmt.priv)
			printf("\tCustom Info   : %08x\n", fb.fmt.priv);
	}
}

static std::string markers2s(unsigned markers)
{
	std::string s;

	if (markers & V4L2_JPEG_MARKER_DHT)
		s += "\t\tDefine Huffman Tables\n";
	if (markers & V4L2_JPEG_MARKER_DQT)
		s += "\t\tDefine Quantization Tables\n";
	if (markers & V4L2_JPEG_MARKER_DRI)
		s += "\t\tDefine Restart Interval\n";
	if (markers & V4L2_JPEG_MARKER_COM)
		s += "\t\tDefine Comment\n";
	if (markers & V4L2_JPEG_MARKER_APP)
		s += "\t\tDefine APP segment\n";
	return s;
}

static void printjpegcomp(const struct v4l2_jpegcompression &jc)
{
	printf("JPEG compression:\n");
	printf("\tQuality: %d\n", jc.quality);
	if (jc.COM_len)
		printf("\tComment: '%s'\n", jc.COM_data);
	if (jc.APP_len)
		printf("\tAPP%x   : '%s'\n", jc.APPn, jc.APP_data);
	printf("\tMarkers: 0x%08x\n", jc.jpeg_markers);
	printf("%s", markers2s(jc.jpeg_markers).c_str());
}

static void printcrop(const struct v4l2_crop &crop)
{
	printf("Crop: Left %d, Top %d, Width %d, Height %d\n",
			crop.c.left, crop.c.top, crop.c.width, crop.c.height);
}

static void printcropcap(const struct v4l2_cropcap &cropcap)
{
	printf("Crop Capability %s:\n", buftype2s(cropcap.type).c_str());
	printf("\tBounds      : Left %d, Top %d, Width %d, Height %d\n",
			cropcap.bounds.left, cropcap.bounds.top, cropcap.bounds.width, cropcap.bounds.height);
	printf("\tDefault     : Left %d, Top %d, Width %d, Height %d\n",
			cropcap.defrect.left, cropcap.defrect.top, cropcap.defrect.width, cropcap.defrect.height);
	printf("\tPixel Aspect: %u/%u\n", cropcap.pixelaspect.numerator, cropcap.pixelaspect.denominator);
}

static const flag_def selection_targets_def[] = {
	{ V4L2_SEL_TGT_CROP_ACTIVE, "crop" },
	{ V4L2_SEL_TGT_CROP_DEFAULT, "crop_default" },
	{ V4L2_SEL_TGT_CROP_BOUNDS, "crop_bounds" },
	{ V4L2_SEL_TGT_COMPOSE_ACTIVE, "compose" },
	{ V4L2_SEL_TGT_COMPOSE_DEFAULT, "compose_default" },
	{ V4L2_SEL_TGT_COMPOSE_BOUNDS, "compose_bounds" },
	{ V4L2_SEL_TGT_COMPOSE_PADDED, "compose_padded" },
	{ 0, NULL }
};

static std::string seltarget2s(__u32 target)
{
	int i = 0;

	while (selection_targets_def[i++].str != NULL) {
		if (selection_targets_def[i].flag == target)
			return selection_targets_def[i].str;
	}
	return "Unknown";
}

static void print_selection(const struct v4l2_selection &sel)
{
	printf("Selection: %s, Left %d, Top %d, Width %d, Height %d\n",
			seltarget2s(sel.target).c_str(),
			sel.r.left, sel.r.top, sel.r.width, sel.r.height);
}

static void printfmt(const struct v4l2_format &vfmt)
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
		for (int i = 0; i < vfmt.fmt.pix_mp.num_planes; i++) {
			printf("\tPlane %d           :\n", i);
			printf("\t   Bytes per Line : %u\n", vfmt.fmt.pix_mp.plane_fmt[i].bytesperline);
			printf("\t   Size Image     : %u\n", vfmt.fmt.pix_mp.plane_fmt[i].sizeimage);
			if (i >= VIDEO_MAX_PLANES)
				break;
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
				flags2s(vfmt.fmt.sliced.service_set, service_def).c_str());
		for (int i = 0; i < 24; i++) {
			printf("\tService Line %2d: %8s / %-8s\n", i,
			       flags2s(vfmt.fmt.sliced.service_lines[0][i], service_def).c_str(),
			       flags2s(vfmt.fmt.sliced.service_lines[1][i], service_def).c_str());
		}
		printf("\tI/O Size       : %u\n", vfmt.fmt.sliced.io_size);
		break;
	case V4L2_BUF_TYPE_PRIVATE:
		break;
	}
}

static std::string frmtype2s(unsigned type)
{
	static const char *types[] = {
		"Unknown",
		"Discrete",
		"Continuous",
		"Stepwise"
	};

	if (type > 3)
		type = 0;
	return types[type];
}

static std::string fract2sec(const struct v4l2_fract &f)
{
	char buf[100];

	sprintf(buf, "%.3f s", (1.0 * f.numerator) / f.denominator);
	return buf;
}

static std::string fract2fps(const struct v4l2_fract &f)
{
	char buf[100];

	sprintf(buf, "%.3f fps", (1.0 * f.denominator) / f.numerator);
	return buf;
}

static void print_frmsize(const struct v4l2_frmsizeenum &frmsize, const char *prefix)
{
	printf("%s\tSize: %s ", prefix, frmtype2s(frmsize.type).c_str());
	if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
		printf("%dx%d", frmsize.discrete.width, frmsize.discrete.height);
	} else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
		printf("%dx%d - %dx%d with step %d/%d",
				frmsize.stepwise.min_width,
				frmsize.stepwise.min_height,
				frmsize.stepwise.max_width,
				frmsize.stepwise.max_height,
				frmsize.stepwise.step_width,
				frmsize.stepwise.step_height);
	}
	printf("\n");
}

static void print_frmival(const struct v4l2_frmivalenum &frmival, const char *prefix)
{
	printf("%s\tInterval: %s ", prefix, frmtype2s(frmival.type).c_str());
	if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
		printf("%s (%s)\n", fract2sec(frmival.discrete).c_str(),
				fract2fps(frmival.discrete).c_str());
	} else if (frmival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
		printf("%s - %s with step %s\n",
				fract2sec(frmival.stepwise.min).c_str(),
				fract2sec(frmival.stepwise.max).c_str(),
				fract2sec(frmival.stepwise.step).c_str());
		printf("%s\t            : ", prefix);
		printf("(%s - %s with step %s)\n",
				fract2fps(frmival.stepwise.min).c_str(),
				fract2fps(frmival.stepwise.max).c_str(),
				fract2fps(frmival.stepwise.step).c_str());
	}
}

static const flag_def fmtdesc_def[] = {
	{ V4L2_FMT_FLAG_COMPRESSED, "compressed" },
	{ V4L2_FMT_FLAG_EMULATED, "emulated" },
	{ 0, NULL }
};

static void print_video_formats(int fd, enum v4l2_buf_type type)
{
	struct v4l2_fmtdesc fmt;

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = type;
	while (test_ioctl(fd, VIDIOC_ENUM_FMT, &fmt) >= 0) {
		printf("\tIndex       : %d\n", fmt.index);
		printf("\tType        : %s\n", buftype2s(type).c_str());
		printf("\tPixel Format: '%s'", fcc2s(fmt.pixelformat).c_str());
		if (fmt.flags)
			printf(" (%s)", flags2s(fmt.flags, fmtdesc_def).c_str());
		printf("\n");
		printf("\tName        : %s\n", fmt.description);
		printf("\n");
		fmt.index++;
	}
}

static void print_video_formats_ext(int fd, enum v4l2_buf_type type)
{
	struct v4l2_fmtdesc fmt;
	struct v4l2_frmsizeenum frmsize;
	struct v4l2_frmivalenum frmival;

	fmt.index = 0;
	fmt.type = type;
	while (test_ioctl(fd, VIDIOC_ENUM_FMT, &fmt) >= 0) {
		printf("\tIndex       : %d\n", fmt.index);
		printf("\tType        : %s\n", buftype2s(type).c_str());
		printf("\tPixel Format: '%s'", fcc2s(fmt.pixelformat).c_str());
		if (fmt.flags)
			printf(" (%s)", flags2s(fmt.flags, fmtdesc_def).c_str());
		printf("\n");
		printf("\tName        : %s\n", fmt.description);
		frmsize.pixel_format = fmt.pixelformat;
		frmsize.index = 0;
		while (test_ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
			print_frmsize(frmsize, "\t");
			if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
				frmival.index = 0;
				frmival.pixel_format = fmt.pixelformat;
				frmival.width = frmsize.discrete.width;
				frmival.height = frmsize.discrete.height;
				while (test_ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) >= 0) {
					print_frmival(frmival, "\t\t");
					frmival.index++;
				}
			}
			frmsize.index++;
		}
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

static v4l2_std_id parse_pal(const char *pal)
{
	if (pal[0] == '-') {
		switch (pal[1]) {
			case '6':
				return V4L2_STD_PAL_60;
			case 'b':
			case 'B':
			case 'g':
			case 'G':
				return V4L2_STD_PAL_BG;
			case 'h':
			case 'H':
				return V4L2_STD_PAL_H;
			case 'n':
			case 'N':
				if (pal[2] == 'c' || pal[2] == 'C')
					return V4L2_STD_PAL_Nc;
				return V4L2_STD_PAL_N;
			case 'i':
			case 'I':
				return V4L2_STD_PAL_I;
			case 'd':
			case 'D':
			case 'k':
			case 'K':
				return V4L2_STD_PAL_DK;
			case 'M':
			case 'm':
				return V4L2_STD_PAL_M;
			case '-':
				break;
		}
	}
	fprintf(stderr, "pal specifier not recognised\n");
	return 0;
}

static v4l2_std_id parse_secam(const char *secam)
{
	if (secam[0] == '-') {
		switch (secam[1]) {
			case 'b':
			case 'B':
			case 'g':
			case 'G':
			case 'h':
			case 'H':
				return V4L2_STD_SECAM_B | V4L2_STD_SECAM_G | V4L2_STD_SECAM_H;
			case 'd':
			case 'D':
			case 'k':
			case 'K':
				return V4L2_STD_SECAM_DK;
			case 'l':
			case 'L':
				if (secam[2] == 'C' || secam[2] == 'c')
					return V4L2_STD_SECAM_LC;
				return V4L2_STD_SECAM_L;
			case '-':
				break;
		}
	}
	fprintf(stderr, "secam specifier not recognised\n");
	return 0;
}

static v4l2_std_id parse_ntsc(const char *ntsc)
{
	if (ntsc[0] == '-') {
		switch (ntsc[1]) {
			case 'm':
			case 'M':
				return V4L2_STD_NTSC_M;
			case 'j':
			case 'J':
				return V4L2_STD_NTSC_M_JP;
			case 'k':
			case 'K':
				return V4L2_STD_NTSC_M_KR;
			case '-':
				break;
		}
	}
	fprintf(stderr, "ntsc specifier not recognised\n");
	return 0;
}

int parse_subopt(char **subs, const char * const *subopts, char **value)
{
	int opt = getsubopt(subs, (char * const *)subopts, value);

	if (opt == -1) {
		fprintf(stderr, "Invalid suboptions specified\n");
		return -1;
	}
	if (value == NULL) {
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

static void print_v4lstd(v4l2_std_id std)
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

static void print_enccmd(const struct v4l2_encoder_cmd &cmd)
{
	switch (cmd.cmd) {
	case V4L2_ENC_CMD_START:
		printf("\tstart\n");
		break;
	case V4L2_ENC_CMD_STOP:
		printf("\tstop%s\n",
			(cmd.flags & V4L2_ENC_CMD_STOP_AT_GOP_END) ? " at gop end" : "");
		break;
	case V4L2_ENC_CMD_PAUSE:
		printf("\tpause\n");
		break;
	case V4L2_ENC_CMD_RESUME:
		printf("\tresume\n");
		break;
	}
}

static void print_deccmd(const struct v4l2_decoder_cmd &cmd)
{
	__s32 speed;

	switch (cmd.cmd) {
	case V4L2_DEC_CMD_START:
		speed = cmd.start.speed;
		if (speed == 0)
			speed = 1000;
		printf("\tstart%s%s, ",
			cmd.start.format == V4L2_DEC_START_FMT_GOP ? " (GOP aligned)" : "",
			(speed != 1000 &&
			 (cmd.flags & V4L2_DEC_CMD_START_MUTE_AUDIO)) ? " (mute audio)" : "");
		if (speed == 1 || speed == -1)
			printf("single step %s\n",
				speed == 1 ? "forward" : "backward");
		else
			printf("speed %.3fx\n", speed / 1000.0);
		break;
	case V4L2_DEC_CMD_STOP:
		printf("\tstop%s%s\n",
			(cmd.flags & V4L2_DEC_CMD_STOP_TO_BLACK) ? " to black" : "",
			(cmd.flags & V4L2_DEC_CMD_STOP_IMMEDIATELY) ? " immediately" : "");
		break;
	case V4L2_DEC_CMD_PAUSE:
		printf("\tpause%s\n",
			(cmd.flags & V4L2_DEC_CMD_PAUSE_TO_BLACK) ? " to black" : "");
		break;
	case V4L2_DEC_CMD_RESUME:
		printf("\tresume\n");
		break;
	}
}

static void do_crop(int fd, unsigned int set_crop, struct v4l2_rect &vcrop, v4l2_buf_type type)
{
	struct v4l2_crop in_crop;

	in_crop.type = type;
	if (doioctl(fd, VIDIOC_G_CROP, &in_crop) == 0) {
		if (set_crop & CropWidth)
			in_crop.c.width = vcrop.width;
		if (set_crop & CropHeight)
			in_crop.c.height = vcrop.height;
		if (set_crop & CropLeft)
			in_crop.c.left = vcrop.left;
		if (set_crop & CropTop)
			in_crop.c.top = vcrop.top;
		doioctl(fd, VIDIOC_S_CROP, &in_crop);
	}
}

static void parse_crop(char *optarg, unsigned int &set_crop, v4l2_rect &vcrop)
{
	char *value;
	char *subs = optarg;

	while (*subs != '\0') {
		static const char *const subopts[] = {
			"left",
			"top",
			"width",
			"height",
			NULL
		};

		switch (parse_subopt(&subs, subopts, &value)) {
		case 0:
			vcrop.left = strtol(value, 0L, 0);
			set_crop |= CropLeft;
			break;
		case 1:
			vcrop.top = strtol(value, 0L, 0);
			set_crop |= CropTop;
			break;
		case 2:
			vcrop.width = strtol(value, 0L, 0);
			set_crop |= CropWidth;
			break;
		case 3:
			vcrop.height = strtol(value, 0L, 0);
			set_crop |= CropHeight;
			break;
		default:
			usage_selection();
			exit(1);
		}
	}
}

static void do_selection(int fd, unsigned int set_selection, struct v4l2_selection &vsel,
			 v4l2_buf_type type)
{
	struct v4l2_selection in_selection;

	in_selection.type = type;
	in_selection.target = vsel.target;

	if (doioctl(fd, VIDIOC_G_SELECTION, &in_selection) == 0) {
		if (set_selection & SelectionWidth)
			in_selection.r.width = vsel.r.width;
		if (set_selection & SelectionHeight)
			in_selection.r.height = vsel.r.height;
		if (set_selection & SelectionLeft)
			in_selection.r.left = vsel.r.left;
		if (set_selection & SelectionTop)
			in_selection.r.top = vsel.r.top;
		in_selection.flags = (set_selection & SelectionFlags) ? vsel.flags : 0;
		doioctl(fd, VIDIOC_S_SELECTION, &in_selection);
	}
}

static int parse_selection_target(const char *s, unsigned int &target)
{
	if (!strcmp(s, "crop")) target = V4L2_SEL_TGT_CROP_ACTIVE;
	else if (!strcmp(s, "crop_default")) target = V4L2_SEL_TGT_CROP_DEFAULT;
	else if (!strcmp(s, "crop_bounds")) target = V4L2_SEL_TGT_CROP_BOUNDS;
	else if (!strcmp(s, "compose")) target = V4L2_SEL_TGT_COMPOSE_ACTIVE;
	else if (!strcmp(s, "compose_default")) target = V4L2_SEL_TGT_COMPOSE_DEFAULT;
	else if (!strcmp(s, "compose_bounds")) target = V4L2_SEL_TGT_COMPOSE_BOUNDS;
	else if (!strcmp(s, "compose_padded")) target = V4L2_SEL_TGT_COMPOSE_PADDED;
	else return -EINVAL;

	return 0;
}

static int parse_selection_flags(const char *s)
{
	if (!strcmp(s, "le")) return V4L2_SEL_FLAG_LE;
	if (!strcmp(s, "ge")) return V4L2_SEL_FLAG_GE;
	return 0;
}

static int parse_selection(char *optarg, unsigned int &set_sel, v4l2_selection &vsel)
{
	char *value;
	char *subs = optarg;

	while (*subs != '\0') {
		static const char *const subopts[] = {
			"target",
			"flags",
			"left",
			"top",
			"width",
			"height",
			NULL
		};

		switch (parse_subopt(&subs, subopts, &value)) {
		case 0:
			if (parse_selection_target(value, vsel.target)) {
				fprintf(stderr, "Unknown selection target\n");
				usage_selection();
				exit(1);
			}
			break;
		case 1:
			vsel.flags = parse_selection_flags(value);
			set_sel |= SelectionFlags;
			break;
		case 2:
			vsel.r.left = strtol(value, 0L, 0);
			set_sel |= SelectionLeft;
			break;
		case 3:
			vsel.r.top = strtol(value, 0L, 0);
			set_sel |= SelectionTop;
			break;
		case 4:
			vsel.r.width = strtol(value, 0L, 0);
			set_sel |= SelectionWidth;
			break;
		case 5:
			vsel.r.height = strtol(value, 0L, 0);
			set_sel |= SelectionHeight;
			break;
		default:
			fprintf(stderr, "Unknown option\n");
			usage_selection();
			exit(1);
		}
	}

	return 0;
}

/* Used for both encoder and decoder commands since they are the same
   at the moment. */
static int parse_cmd(const char *s)
{
	if (!strcmp(s, "start")) return V4L2_ENC_CMD_START;
	if (!strcmp(s, "stop")) return V4L2_ENC_CMD_STOP;
	if (!strcmp(s, "pause")) return V4L2_ENC_CMD_PAUSE;
	if (!strcmp(s, "resume")) return V4L2_ENC_CMD_RESUME;
	return 0;
}

static int parse_encflags(const char *s)
{
	if (!strcmp(s, "stop_at_gop_end")) return V4L2_ENC_CMD_STOP_AT_GOP_END;
	return 0;
}

static int parse_decflags(const char *s)
{
	if (!strcmp(s, "start_mute_audio")) return V4L2_DEC_CMD_START_MUTE_AUDIO;
	if (!strcmp(s, "pause_to_black")) return V4L2_DEC_CMD_PAUSE_TO_BLACK;
	if (!strcmp(s, "stop_to_black")) return V4L2_DEC_CMD_STOP_TO_BLACK;
	if (!strcmp(s, "stop_immediately")) return V4L2_DEC_CMD_STOP_IMMEDIATELY;
	return 0;
}

static void parse_dv_bt_timings(char *optarg, struct v4l2_dv_timings *dv_timings,
		bool &query, int &enumerate)
{
	char *value;
	char *subs = optarg;
	struct v4l2_bt_timings *bt = &dv_timings->bt;

	dv_timings->type = V4L2_DV_BT_656_1120;

	while (*subs != '\0') {
		static const char *const subopts[] = {
			"width",
			"height",
			"interlaced",
			"polarities",
			"pixelclock",
			"hfp",
			"hs",
			"hbp",
			"vfp",
			"vs",
			"vbp",
			"il_vfp",
			"il_vs",
			"il_vbp",
			"index",
			"query",
			NULL
		};

		switch (parse_subopt(&subs, subopts, &value)) {
		case 0:
			bt->width = atoi(value);
			break;
		case 1:
			bt->height = strtol(value, 0L, 0);
			break;
		case 2:
			bt->interlaced = strtol(value, 0L, 0);
			break;
		case 3:
			bt->polarities = strtol(value, 0L, 0);
			break;
		case 4:
			bt->pixelclock = strtol(value, 0L, 0);
			break;
		case 5:
			bt->hfrontporch = strtol(value, 0L, 0);
			break;
		case 6:
			bt->hsync = strtol(value, 0L, 0);
			break;
		case 7:
			bt->hbackporch = strtol(value, 0L, 0);
			break;
		case 8:
			bt->vfrontporch = strtol(value, 0L, 0);
			break;
		case 9:
			bt->vsync = strtol(value, 0L, 0);
			break;
		case 10:
			bt->vbackporch = strtol(value, 0L, 0);
			break;
		case 11:
			bt->il_vfrontporch = strtol(value, 0L, 0);
			break;
		case 12:
			bt->il_vsync = strtol(value, 0L, 0);
			break;
		case 13:
			bt->il_vbackporch = strtol(value, 0L, 0);
			break;
		case 14:
			enumerate = strtol(value, 0L, 0);
			break;
		case 15:
			query = true;
			break;
		default:
			usage_stds();
			exit(1);
		}
	}
}


static const flag_def dv_standards_def[] = {
	{ V4L2_DV_BT_STD_CEA861, "CEA-861" },
	{ V4L2_DV_BT_STD_DMT, "DMT" },
	{ V4L2_DV_BT_STD_CVT, "CVT" },
	{ V4L2_DV_BT_STD_GTF, "GTF" },
	{ 0, NULL }
};

static const flag_def dv_flags_def[] = {
	{ V4L2_DV_FL_REDUCED_BLANKING, "reduced blanking" },
	{ V4L2_DV_FL_CAN_REDUCE_FPS, "framerate can be reduced by 1/1.001" },
	{ V4L2_DV_FL_REDUCED_FPS, "framerate is reduced by 1/1.001" },
	{ V4L2_DV_FL_HALF_LINE, "half-line" },
	{ 0, NULL }
};

static void print_dv_timings(const struct v4l2_dv_timings *t)
{
	const struct v4l2_bt_timings *bt;

	switch (t->type) {
	case V4L2_DV_BT_656_1120:
		bt = &t->bt;

		printf("\tActive width: %d\n", bt->width);
		printf("\tActive height: %d\n", bt->height);
		printf("\tTotal width: %d\n",bt->width +
				bt->hfrontporch + bt->hsync + bt->hbackporch);
		printf("\tTotal height: %d\n", bt->height +
				bt->vfrontporch + bt->vsync + bt->vbackporch +
				bt->il_vfrontporch + bt->il_vsync + bt->il_vbackporch);

		printf("\tFrame format: %s\n", bt->interlaced ? "interlaced" : "progressive");
		printf("\tPolarities: %cvsync %chsync\n",
				(bt->polarities & V4L2_DV_VSYNC_POS_POL) ? '+' : '-',
				(bt->polarities & V4L2_DV_HSYNC_POS_POL) ? '+' : '-');
		printf("\tPixelclock: %lld Hz", bt->pixelclock);
		if (bt->width && bt->height)
			printf(" (%.2f fps)", (double)bt->pixelclock /
					((bt->width + bt->hfrontporch + bt->hsync + bt->hbackporch) *
					 (bt->height + bt->vfrontporch + bt->vsync + bt->vbackporch +
					  bt->il_vfrontporch + bt->il_vsync + bt->il_vbackporch)));
		printf("\n");
		printf("\tHorizontal frontporch: %d\n", bt->hfrontporch);
		printf("\tHorizontal sync: %d\n", bt->hsync);
		printf("\tHorizontal backporch: %d\n", bt->hbackporch);
		if (bt->interlaced)
			printf("\tField 1:\n");
		printf("\tVertical frontporch: %d\n", bt->vfrontporch);
		printf("\tVertical sync: %d\n", bt->vsync);
		printf("\tVertical backporch: %d\n", bt->vbackporch);
		if (bt->interlaced) {
			printf("\tField 2:\n");
			printf("\tVertical frontporch: %d\n", bt->il_vfrontporch);
			printf("\tVertical sync: %d\n", bt->il_vsync);
			printf("\tVertical backporch: %d\n", bt->il_vbackporch);
		}
		printf("\tStandards: %s\n", flags2s(bt->standards, dv_standards_def).c_str());
		printf("\tFlags: %s\n", flags2s(bt->flags, dv_flags_def).c_str());
		break;
	default:
		printf("Timing type not defined\n");
		break;
	}
}

static enum v4l2_field parse_field(const char *s)
{
	if (!strcmp(s, "any")) return V4L2_FIELD_ANY;
	if (!strcmp(s, "none")) return V4L2_FIELD_NONE;
	if (!strcmp(s, "top")) return V4L2_FIELD_TOP;
	if (!strcmp(s, "bottom")) return V4L2_FIELD_BOTTOM;
	if (!strcmp(s, "interlaced")) return V4L2_FIELD_INTERLACED;
	if (!strcmp(s, "seq_tb")) return V4L2_FIELD_SEQ_TB;
	if (!strcmp(s, "seq_bt")) return V4L2_FIELD_SEQ_BT;
	if (!strcmp(s, "alternate")) return V4L2_FIELD_ALTERNATE;
	if (!strcmp(s, "interlaced_tb")) return V4L2_FIELD_INTERLACED_TB;
	if (!strcmp(s, "interlaced_bt")) return V4L2_FIELD_INTERLACED_BT;
	return V4L2_FIELD_ANY;
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
		usage_misc();
		exit(1);
	}
	return event;
}

static __u32 find_pixel_format(int fd, unsigned index, bool mplane)
{
	struct v4l2_fmtdesc fmt;

	fmt.index = index;
	fmt.type = mplane ?
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (doioctl(fd, VIDIOC_ENUM_FMT, &fmt))
		return 0;
	return fmt.pixelformat;
}

int main(int argc, char **argv)
{
	char *value, *subs;
	int i;

	int fd = -1;

	/* bitfield for fmts */
	unsigned int set_fmts = 0;
	unsigned int set_fmts_out = 0;
	unsigned int set_crop = 0;
	unsigned int set_crop_out = 0;
	unsigned int set_crop_overlay = 0;
	unsigned int set_crop_out_overlay = 0;
	unsigned int set_selection = 0;
	unsigned int set_selection_out = 0;
	int get_sel_target = 0;
	unsigned int set_fbuf = 0;
	unsigned int set_overlay_fmt = 0;
	unsigned int set_overlay_fmt_out = 0;

	/* command args */
	int ch;
	const char *device = "/dev/video0";	/* -d device */
	struct v4l2_format vfmt;	/* set_format/get_format for video */
	struct v4l2_format vfmt_out;	/* set_format/get_format video output */
	struct v4l2_format vbi_fmt;	/* set_format/get_format for sliced VBI */
	struct v4l2_format vbi_fmt_out;	/* set_format/get_format for sliced VBI output */
	struct v4l2_format raw_fmt;	/* set_format/get_format for VBI */
	struct v4l2_format raw_fmt_out;	/* set_format/get_format for VBI output */
	struct v4l2_format overlay_fmt;	/* set_format/get_format video overlay */
	struct v4l2_format overlay_fmt_out;	/* set_format/get_format video overlay output */
	struct v4l2_capability vcap;	/* list_cap */
	struct v4l2_frmsizeenum frmsize;/* list frame sizes */
	struct v4l2_frmivalenum frmival;/* list frame intervals */
	struct v4l2_rect vcrop; 	/* crop rect */
	struct v4l2_rect vcrop_out; 	/* crop rect */
	struct v4l2_rect vcrop_overlay; 	/* crop rect */
	struct v4l2_rect vcrop_out_overlay; 	/* crop rect */
	struct v4l2_selection vselection; 	/* capture selection */
	struct v4l2_selection vselection_out;	/* output selection */
	struct v4l2_framebuffer fbuf;   /* fbuf */
	struct v4l2_jpegcompression jpegcomp; /* jpeg compression */
	struct v4l2_streamparm parm;	/* get/set parm */
	struct v4l2_dv_enum_preset dv_enum_preset; /* list_dv_preset */
	struct v4l2_enum_dv_timings dv_enum_timings; /* list_dv_timings */
	struct v4l2_dv_preset dv_preset; /* set_dv_preset/get_dv_preset/query_dv_preset */
	struct v4l2_dv_timings dv_timings; /* set_dv_bt_timings/get_dv_timings/query_dv_timings */
	bool query_and_set_dv_timings = false;
	int enum_and_set_dv_timings = -1;
	struct v4l2_dv_timings_cap dv_timings_cap; /* get_dv_timings_cap */
	struct v4l2_encoder_cmd enc_cmd; /* (try_)encoder_cmd */
	struct v4l2_decoder_cmd dec_cmd; /* (try_)decoder_cmd */
	v4l2_std_id std;		/* get_std/set_std */
	double fps = 0;			/* set framerate speed, in fps */
	double output_fps = 0;		/* set framerate speed, in fps */
	struct v4l2_standard vs;	/* list_std */
	int overlay;			/* overlay */
	unsigned int *set_overlay_fmt_ptr = NULL;
	struct v4l2_format *overlay_fmt_ptr = NULL;
	unsigned secs = 0;
	__u32 wait_for_event = 0;	/* wait for this event */
	const char *wait_event_id = NULL;
	__u32 poll_for_event = 0;	/* poll for this event */
	const char *poll_event_id = NULL;
	char short_options[26 * 2 * 2 + 1];
	int idx = 0;
	int ret;

	memset(&vfmt, 0, sizeof(vfmt));
	memset(&vbi_fmt, 0, sizeof(vbi_fmt));
	memset(&raw_fmt, 0, sizeof(raw_fmt));
	memset(&vfmt_out, 0, sizeof(vfmt_out));
	memset(&vbi_fmt_out, 0, sizeof(vbi_fmt_out));
	memset(&overlay_fmt, 0, sizeof(overlay_fmt));
	memset(&overlay_fmt_out, 0, sizeof(overlay_fmt_out));
	memset(&raw_fmt_out, 0, sizeof(raw_fmt_out));
	memset(&vcap, 0, sizeof(vcap));
	memset(&frmsize, 0, sizeof(frmsize));
	memset(&frmival, 0, sizeof(frmival));
	memset(&vcrop, 0, sizeof(vcrop));
	memset(&vcrop_out, 0, sizeof(vcrop_out));
	memset(&vcrop_overlay, 0, sizeof(vcrop_overlay));
	memset(&vcrop_out_overlay, 0, sizeof(vcrop_out_overlay));
	memset(&vselection, 0, sizeof(vselection));
	memset(&vselection_out, 0, sizeof(vselection_out));
	memset(&vs, 0, sizeof(vs));
	memset(&fbuf, 0, sizeof(fbuf));
	memset(&jpegcomp, 0, sizeof(jpegcomp));
	memset(&dv_preset, 0, sizeof(dv_preset));
	memset(&dv_timings, 0, sizeof(dv_timings));
	memset(&dv_enum_preset, 0, sizeof(dv_enum_preset));
	memset(&enc_cmd, 0, sizeof(enc_cmd));
	memset(&dec_cmd, 0, sizeof(dec_cmd));

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
			usage_stds();
			return 0;
		case OptHelpVidCap:
			usage_vidcap();
			return 0;
		case OptHelpVidOut:
			usage_vidout();
			return 0;
		case OptHelpOverlay:
			usage_overlay();
			return 0;
		case OptHelpVbi:
			usage_vbi();
			return 0;
		case OptHelpSelection:
			usage_selection();
			return 0;
		case OptHelpMisc:
			usage_misc();
			return 0;
		case OptHelpAll:
			usage_all();
			return 0;
		case OptSetDevice:
			device = optarg;
			if (device[0] >= '0' && device[0] <= '9' && device[1] == 0) {
				static char newdev[20];
				char dev = device[0];

				sprintf(newdev, "/dev/video%c", dev);
				device = newdev;
			}
			break;
		case OptSleep:
			secs = strtoul(optarg, 0L, 0);
			break;
		case OptSetVideoOutMplaneFormat:
		case OptTryVideoOutMplaneFormat:
		case OptSetVideoOutFormat:
		case OptTryVideoOutFormat:
		case OptSetVideoMplaneFormat:
		case OptTryVideoMplaneFormat:
		case OptSetVideoFormat:
		case OptTryVideoFormat: {
			__u32 width = 0, height = 0, pixelformat = 0;
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
					switch (ch) {
					case OptSetVideoOutMplaneFormat:
					case OptTryVideoOutMplaneFormat:
					case OptSetVideoOutFormat:
					case OptTryVideoOutFormat:
						usage_vidout();
						break;
					case OptSetVideoMplaneFormat:
					case OptTryVideoMplaneFormat:
					case OptSetVideoFormat:
					case OptTryVideoFormat:
						usage_vidcap();
						break;
					}
					exit(1);
				}
			}
			switch (ch) {
			case OptSetVideoFormat:
			case OptTryVideoFormat:
				vfmt.fmt.pix.width = width;
				vfmt.fmt.pix.height = height;
				vfmt.fmt.pix.pixelformat = pixelformat;
				set_fmts = fmts;
				break;
			case OptSetVideoMplaneFormat:
			case OptTryVideoMplaneFormat:
				vfmt.fmt.pix_mp.width = width;
				vfmt.fmt.pix_mp.height = height;
				vfmt.fmt.pix_mp.pixelformat = pixelformat;
				set_fmts = fmts;
				break;
			case OptSetVideoOutFormat:
			case OptTryVideoOutFormat:
				vfmt_out.fmt.pix.width = width;
				vfmt_out.fmt.pix.height = height;
				vfmt_out.fmt.pix.pixelformat = pixelformat;
				set_fmts_out = fmts;
				break;
			case OptSetVideoOutMplaneFormat:
			case OptTryVideoOutMplaneFormat:
				vfmt_out.fmt.pix_mp.width = width;
				vfmt_out.fmt.pix_mp.height = height;
				vfmt_out.fmt.pix_mp.pixelformat = pixelformat;
				set_fmts_out = fmts;
				break;
			}
			break;
		}
		case OptSetOverlayFormat:
		case OptTryOverlayFormat:
		case OptSetOutputOverlayFormat:
		case OptTryOutputOverlayFormat:
			switch (ch) {
			case OptSetOverlayFormat:
			case OptTryOverlayFormat:
				set_overlay_fmt_ptr = &set_overlay_fmt;
				overlay_fmt_ptr = &overlay_fmt;
				break;
			case OptSetOutputOverlayFormat:
			case OptTryOutputOverlayFormat:
				set_overlay_fmt_ptr = &set_overlay_fmt_out;
				overlay_fmt_ptr = &overlay_fmt_out;
				break;
			}
			subs = optarg;
			while (*subs != '\0') {
				static const char *const subopts[] = {
					"chromakey",
					"global_alpha",
					"left",
					"top",
					"width",
					"height",
					"field",
					NULL
				};

				switch (parse_subopt(&subs, subopts, &value)) {
				case 0:
					overlay_fmt_ptr->fmt.win.chromakey = strtol(value, 0L, 0);
					*set_overlay_fmt_ptr |= FmtChromaKey;
					break;
				case 1:
					overlay_fmt_ptr->fmt.win.global_alpha = strtol(value, 0L, 0);
					*set_overlay_fmt_ptr |= FmtGlobalAlpha;
					break;
				case 2:
					overlay_fmt_ptr->fmt.win.w.left = strtol(value, 0L, 0);
					*set_overlay_fmt_ptr |= FmtLeft;
					break;
				case 3:
					overlay_fmt_ptr->fmt.win.w.top = strtol(value, 0L, 0);
					*set_overlay_fmt_ptr |= FmtTop;
					break;
				case 4:
					overlay_fmt_ptr->fmt.win.w.width = strtol(value, 0L, 0);
					*set_overlay_fmt_ptr |= FmtWidth;
					break;
				case 5:
					overlay_fmt_ptr->fmt.win.w.height = strtol(value, 0L, 0);
					*set_overlay_fmt_ptr |= FmtHeight;
					break;
				case 6:
					overlay_fmt_ptr->fmt.win.field = parse_field(value);
					*set_overlay_fmt_ptr |= FmtField;
					break;
				default:
					usage_overlay();
					break;
				}
			}
			break;
		case OptSetFBuf:
			subs = optarg;
			while (*subs != '\0') {
				static const char *const subopts[] = {
					"chromakey",
					"global_alpha",
					"local_alpha",
					"local_inv_alpha",
					NULL
				};

				switch (parse_subopt(&subs, subopts, &value)) {
				case 0:
					fbuf.flags |= strtol(value, 0L, 0) ? V4L2_FBUF_FLAG_CHROMAKEY : 0;
					set_fbuf |= V4L2_FBUF_FLAG_CHROMAKEY;
					break;
				case 1:
					fbuf.flags |= strtol(value, 0L, 0) ? V4L2_FBUF_FLAG_GLOBAL_ALPHA : 0;
					set_fbuf |= V4L2_FBUF_FLAG_GLOBAL_ALPHA;
					break;
				case 2:
					fbuf.flags |= strtol(value, 0L, 0) ? V4L2_FBUF_FLAG_LOCAL_ALPHA : 0;
					set_fbuf |= V4L2_FBUF_FLAG_LOCAL_ALPHA;
					break;
				case 3:
					fbuf.flags |= strtol(value, 0L, 0) ? V4L2_FBUF_FLAG_LOCAL_INV_ALPHA : 0;
					set_fbuf |= V4L2_FBUF_FLAG_LOCAL_INV_ALPHA;
					break;
				default:
					usage_overlay();
					break;
				}
			}
			break;
		case OptOverlay:
			overlay = strtol(optarg, 0L, 0);
			break;
		case OptListFrameSizes:
			if (strlen(optarg) == 4)
			    frmsize.pixel_format = v4l2_fourcc(optarg[0], optarg[1],
					optarg[2], optarg[3]);
			else
			    frmsize.pixel_format = strtol(optarg, 0L, 0);
			break;
		case OptListFrameIntervals:
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
					frmival.width = strtol(value, 0L, 0);
					break;
				case 1:
					frmival.height = strtol(value, 0L, 0);
					break;
				case 2:
					if (strlen(value) == 4)
						frmival.pixel_format =
						    v4l2_fourcc(value[0], value[1],
							    value[2], value[3]);
					else
						frmival.pixel_format = strtol(value, 0L, 0);
					break;
				default:
					usage_vidcap();
					break;
				}
			}
			break;
		case OptSetCrop:
			parse_crop(optarg, set_crop, vcrop);
			break;
		case OptSetOutputCrop:
			parse_crop(optarg, set_crop_out, vcrop_out);
			break;
		case OptSetOverlayCrop:
			parse_crop(optarg, set_crop_overlay, vcrop_overlay);
			break;
		case OptSetOutputOverlayCrop:
			parse_crop(optarg, set_crop_out_overlay, vcrop_out_overlay);
			break;
		case OptSetSelection:
			parse_selection(optarg, set_selection, vselection);
			break;
		case OptSetOutputSelection:
			parse_selection(optarg, set_selection_out, vselection_out);
			break;
		case OptGetOutputSelection:
		case OptGetSelection: {
			struct v4l2_selection gsel;
			unsigned int get_sel;

			if (parse_selection(optarg, get_sel, gsel)) {
				fprintf(stderr, "Unknown selection target\n");
				usage_selection();
				exit(1);
			}
			get_sel_target = gsel.target;
			break;
		}
		case OptSetStandard:
			if (!strncmp(optarg, "pal", 3)) {
				if (optarg[3])
					std = parse_pal(optarg + 3);
				else
					std = V4L2_STD_PAL;
			}
			else if (!strncmp(optarg, "ntsc", 4)) {
				if (optarg[4])
					std = parse_ntsc(optarg + 4);
				else
					std = V4L2_STD_NTSC;
			}
			else if (!strncmp(optarg, "secam", 5)) {
				if (optarg[5])
					std = parse_secam(optarg + 5);
				else
					std = V4L2_STD_SECAM;
			}
			else {
				std = strtol(optarg, 0L, 0) | (1ULL << 63);
			}
			break;
		case OptSetParm:
			fps = strtod(optarg, NULL);
			break;
		case OptSetOutputParm:
			output_fps = strtod(optarg, NULL);
			break;
		case OptSetSlicedVbiFormat:
		case OptSetSlicedVbiOutFormat:
		case OptTrySlicedVbiFormat:
		case OptTrySlicedVbiOutFormat:
		{
			bool foundOff = false;
			v4l2_format *fmt = &vbi_fmt;

			if (ch == OptSetSlicedVbiOutFormat || ch == OptTrySlicedVbiOutFormat)
				fmt = &vbi_fmt_out;
			fmt->fmt.sliced.service_set = 0;
			subs = optarg;
			while (*subs != '\0') {
				static const char *const subopts[] = {
					"off",
					"teletext",
					"cc",
					"wss",
					"vps",
					NULL
				};

				switch (parse_subopt(&subs, subopts, &value)) {
				case 0:
					foundOff = true;
					break;
				case 1:
					fmt->fmt.sliced.service_set |=
					    V4L2_SLICED_TELETEXT_B;
					break;
				case 2:
					fmt->fmt.sliced.service_set |=
					    V4L2_SLICED_CAPTION_525;
					break;
				case 3:
					fmt->fmt.sliced.service_set |=
					    V4L2_SLICED_WSS_625;
					break;
				case 4:
					fmt->fmt.sliced.service_set |=
					    V4L2_SLICED_VPS;
					break;
				default:
					usage_vbi();
					break;
				}
			}
			if (foundOff && fmt->fmt.sliced.service_set) {
				fprintf(stderr, "Sliced VBI mode 'off' cannot be combined with other modes\n");
				usage_vbi();
				return 1;
			}
			break;
		}
		case OptSetJpegComp:
		{
			subs = optarg;
			while (*subs != '\0') {
				static const char *const subopts[] = {
					"app0", "app1", "app2", "app3",
					"app4", "app5", "app6", "app7",
					"app8", "app9", "appa", "appb",
					"appc", "appd", "appe", "appf",
					"quality",
					"markers",
					"comment",
					NULL
				};
				size_t len;
				int opt = parse_subopt(&subs, subopts, &value);

				switch (opt) {
				case 16:
					jpegcomp.quality = strtol(value, 0L, 0);
					break;
				case 17:
					if (strstr(value, "dht"))
						jpegcomp.jpeg_markers |= V4L2_JPEG_MARKER_DHT;
					if (strstr(value, "dqt"))
						jpegcomp.jpeg_markers |= V4L2_JPEG_MARKER_DQT;
					if (strstr(value, "dri"))
						jpegcomp.jpeg_markers |= V4L2_JPEG_MARKER_DRI;
					break;
				case 18:
					len = strlen(value);
					if (len > sizeof(jpegcomp.COM_data) - 1)
						len = sizeof(jpegcomp.COM_data) - 1;
					jpegcomp.COM_len = len;
					memcpy(jpegcomp.COM_data, value, len);
					jpegcomp.COM_data[len] = '\0';
					break;
				default:
					if (opt < 0 || opt > 15) {
						usage_misc();
						exit(1);
					}
					len = strlen(value);
					if (len > sizeof(jpegcomp.APP_data) - 1)
						len = sizeof(jpegcomp.APP_data) - 1;
					if (jpegcomp.APP_len) {
						fprintf(stderr, "Only one APP segment can be set\n");
						break;
					}
					jpegcomp.APP_len = len;
					memcpy(jpegcomp.APP_data, value, len);
					jpegcomp.APP_data[len] = '\0';
					jpegcomp.APPn = opt;
					break;
				}
			}
			break;
		}
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
		case OptSetDvPreset:
			dv_preset.preset = strtoul(optarg, 0L, 0);
			break;
		case OptSetDvBtTimings:
			parse_dv_bt_timings(optarg, &dv_timings,
					query_and_set_dv_timings, enum_and_set_dv_timings);
			break;
		case OptEncoderCmd:
		case OptTryEncoderCmd:
			subs = optarg;
			while (*subs != '\0') {
				static const char *const subopts[] = {
					"cmd",
					"flags",
					NULL
				};

				switch (parse_subopt(&subs, subopts, &value)) {
				case 0:
					enc_cmd.cmd = parse_cmd(value);
					break;
				case 1:
					enc_cmd.flags = parse_encflags(value);
					break;
				default:
					usage_misc();
					exit(1);
				}
			}
			break;
		case OptDecoderCmd:
		case OptTryDecoderCmd:
			subs = optarg;
			while (*subs != '\0') {
				static const char *const subopts[] = {
					"cmd",
					"flags",
					"stop_pts",
					"start_speed",
					"start_format",
					NULL
				};

				switch (parse_subopt(&subs, subopts, &value)) {
				case 0:
					dec_cmd.cmd = parse_cmd(value);
					break;
				case 1:
					dec_cmd.flags = parse_decflags(value);
					break;
				case 2:
					dec_cmd.stop.pts = strtoull(value, 0, 0);
					break;
				case 3:
					dec_cmd.start.speed = strtol(value, 0, 0);
					break;
				case 4:
					if (!strcmp(value, "gop"))
						dec_cmd.start.format = V4L2_DEC_START_FMT_GOP;
					else if (!strcmp(value, "none"))
						dec_cmd.start.format = V4L2_DEC_START_FMT_NONE;
					break;
				default:
					usage_misc();
					exit(1);
				}
			}
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
		options[OptGetDvPreset] = 1;
		options[OptGetDvTimings] = 1;
		options[OptGetDvTimingsCap] = 1;
		options[OptGetPriority] = 1;
		options[OptGetSelection] = 1;
		options[OptGetOutputSelection] = 1;
		options[OptListCtrls] = 1;
		get_sel_target = -1;
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

	if (options[OptStreamOff]) {
		int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		doioctl(fd, VIDIOC_STREAMOFF, &type);
	}

	common_set(fd);
	tuner_set(fd);
	io_set(fd);

	if (options[OptSetStandard]) {
		if (std & (1ULL << 63)) {
			vs.index = std & 0xffff;
			if (test_ioctl(fd, VIDIOC_ENUMSTD, &vs) >= 0) {
				std = vs.id;
			}
		}
		if (doioctl(fd, VIDIOC_S_STD, &std) == 0)
			printf("Standard set to %08llx\n", (unsigned long long)std);
	}

        if (options[OptSetDvPreset]) {
		if (doioctl(fd, VIDIOC_S_DV_PRESET, &dv_preset) >= 0) {
			printf("Preset set: %d\n", dv_preset.preset);
		}
	}

	if (options[OptSetDvBtTimings]) {
		struct v4l2_enum_dv_timings et;

		if (query_and_set_dv_timings)
			doioctl(fd, VIDIOC_QUERY_DV_TIMINGS, &dv_timings);
		if (enum_and_set_dv_timings >= 0) {
			memset(&et, 0, sizeof(et));
			et.index = enum_and_set_dv_timings;
			doioctl(fd, VIDIOC_ENUM_DV_TIMINGS, &et);
			dv_timings = et.timings;
		}
		if (doioctl(fd, VIDIOC_S_DV_TIMINGS, &dv_timings) >= 0) {
			printf("BT timings set\n");
		}
	}

	if (options[OptSetParm]) {
		memset(&parm, 0, sizeof(parm));
		parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		parm.parm.capture.timeperframe.numerator = 1000;
		parm.parm.capture.timeperframe.denominator =
			fps * parm.parm.capture.timeperframe.numerator;

		if (doioctl(fd, VIDIOC_S_PARM, &parm) == 0) {
			struct v4l2_fract *tf = &parm.parm.capture.timeperframe;

			if (!tf->denominator || !tf->numerator)
				printf("Invalid frame rate\n");
			else
				printf("Frame rate set to %.3f fps\n",
					1.0 * tf->denominator / tf->numerator);
		}
	}

	if (options[OptSetOutputParm]) {
		memset(&parm, 0, sizeof(parm));
		parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		parm.parm.output.timeperframe.numerator = 1000;
		parm.parm.output.timeperframe.denominator =
			output_fps * parm.parm.output.timeperframe.numerator;

		if (doioctl(fd, VIDIOC_S_PARM, &parm) == 0) {
			struct v4l2_fract *tf = &parm.parm.output.timeperframe;

			if (!tf->denominator || !tf->numerator)
				printf("Invalid frame rate\n");
			else
				printf("Frame rate set to %.3f fps\n",
					1.0 * tf->denominator / tf->numerator);
		}
	}

	if (options[OptSetVideoFormat] || options[OptTryVideoFormat]) {
		struct v4l2_format in_vfmt;

		in_vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (doioctl(fd, VIDIOC_G_FMT, &in_vfmt) == 0) {
			if (set_fmts & FmtWidth)
				in_vfmt.fmt.pix.width = vfmt.fmt.pix.width;
			if (set_fmts & FmtHeight)
				in_vfmt.fmt.pix.height = vfmt.fmt.pix.height;
			if (set_fmts & FmtPixelFormat) {
				in_vfmt.fmt.pix.pixelformat = vfmt.fmt.pix.pixelformat;
				if (in_vfmt.fmt.pix.pixelformat < 256) {
					in_vfmt.fmt.pix.pixelformat =
						find_pixel_format(fd, in_vfmt.fmt.pix.pixelformat,
								  false);
				}
			}
			if (options[OptSetVideoFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &in_vfmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &in_vfmt);
			if (ret == 0 && verbose)
				printfmt(in_vfmt);
		}
	}

	if (options[OptSetVideoMplaneFormat] || options[OptTryVideoMplaneFormat]) {
		struct v4l2_format in_vfmt;

		in_vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		if (doioctl(fd, VIDIOC_G_FMT, &in_vfmt) == 0) {
			if (set_fmts & FmtWidth)
				in_vfmt.fmt.pix_mp.width = vfmt.fmt.pix_mp.width;
			if (set_fmts & FmtHeight)
				in_vfmt.fmt.pix_mp.height = vfmt.fmt.pix_mp.height;
			if (set_fmts & FmtPixelFormat) {
				in_vfmt.fmt.pix_mp.pixelformat = vfmt.fmt.pix_mp.pixelformat;
				if (in_vfmt.fmt.pix_mp.pixelformat < 256) {
					in_vfmt.fmt.pix_mp.pixelformat =
						find_pixel_format(fd, in_vfmt.fmt.pix_mp.pixelformat,
								  true);
				}
			}
			if (options[OptSetVideoMplaneFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &in_vfmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &in_vfmt);
			if (ret == 0 && verbose)
				printfmt(in_vfmt);
		}
	}

	if (options[OptSetVideoOutFormat] || options[OptTryVideoOutFormat]) {
		struct v4l2_format in_vfmt;

		in_vfmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (doioctl(fd, VIDIOC_G_FMT, &in_vfmt) == 0) {
			if (set_fmts_out & FmtWidth)
				in_vfmt.fmt.pix.width = vfmt_out.fmt.pix.width;
			if (set_fmts_out & FmtHeight)
				in_vfmt.fmt.pix.height = vfmt_out.fmt.pix.height;
			if (set_fmts & FmtPixelFormat) {
				in_vfmt.fmt.pix.pixelformat = vfmt_out.fmt.pix.pixelformat;
				if (in_vfmt.fmt.pix.pixelformat < 256) {
					in_vfmt.fmt.pix.pixelformat =
						find_pixel_format(fd, in_vfmt.fmt.pix.pixelformat,
								  false);
				}
			}

			if (options[OptSetVideoOutFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &in_vfmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &in_vfmt);
			if (ret == 0 && verbose)
				printfmt(in_vfmt);
		}
	}

	if (options[OptSetVideoOutMplaneFormat] || options[OptTryVideoOutMplaneFormat]) {
		struct v4l2_format in_vfmt;

		in_vfmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		if (doioctl(fd, VIDIOC_G_FMT, &in_vfmt) == 0) {
			if (set_fmts_out & FmtWidth)
				in_vfmt.fmt.pix_mp.width = vfmt_out.fmt.pix_mp.width;
			if (set_fmts_out & FmtHeight)
				in_vfmt.fmt.pix_mp.height = vfmt_out.fmt.pix_mp.height;
			if (set_fmts_out & FmtPixelFormat) {
				in_vfmt.fmt.pix_mp.pixelformat = vfmt_out.fmt.pix_mp.pixelformat;
				if (in_vfmt.fmt.pix_mp.pixelformat < 256) {
					in_vfmt.fmt.pix_mp.pixelformat =
						find_pixel_format(fd, in_vfmt.fmt.pix_mp.pixelformat,
								  true);
				}
			}
			if (options[OptSetVideoOutMplaneFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &in_vfmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &in_vfmt);
			if (ret == 0 && verbose)
				printfmt(in_vfmt);
		}
	}

	if (options[OptSetSlicedVbiFormat] || options[OptTrySlicedVbiFormat]) {
		vbi_fmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
		if (options[OptSetSlicedVbiFormat])
			ret = doioctl(fd, VIDIOC_S_FMT, &vbi_fmt);
		else
			ret = doioctl(fd, VIDIOC_TRY_FMT, &vbi_fmt);
		if (ret == 0 && verbose)
			printfmt(vbi_fmt);
	}

	if (options[OptSetSlicedVbiOutFormat] || options[OptTrySlicedVbiOutFormat]) {
		vbi_fmt_out.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
		if (options[OptSetSlicedVbiOutFormat])
			ret = doioctl(fd, VIDIOC_S_FMT, &vbi_fmt_out);
		else
			ret = doioctl(fd, VIDIOC_TRY_FMT, &vbi_fmt_out);
		if (ret == 0 && verbose)
			printfmt(vbi_fmt_out);
	}

	if (options[OptSetOverlayFormat] || options[OptTryOverlayFormat]) {
		struct v4l2_format fmt;

		fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
		if (doioctl(fd, VIDIOC_G_FMT, &fmt) == 0) {
			if (set_overlay_fmt & FmtChromaKey)
				fmt.fmt.win.chromakey = overlay_fmt.fmt.win.chromakey;
			if (set_overlay_fmt & FmtGlobalAlpha)
				fmt.fmt.win.global_alpha = overlay_fmt.fmt.win.global_alpha;
			if (set_overlay_fmt & FmtLeft)
				fmt.fmt.win.w.left = overlay_fmt.fmt.win.w.left;
			if (set_overlay_fmt & FmtTop)
				fmt.fmt.win.w.top = overlay_fmt.fmt.win.w.top;
			if (set_overlay_fmt & FmtWidth)
				fmt.fmt.win.w.width = overlay_fmt.fmt.win.w.width;
			if (set_overlay_fmt & FmtHeight)
				fmt.fmt.win.w.height = overlay_fmt.fmt.win.w.height;
			if (set_overlay_fmt & FmtField)
				fmt.fmt.win.field = overlay_fmt.fmt.win.field;
			if (options[OptSetOverlayFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &fmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &fmt);
			if (ret == 0 && verbose)
				printfmt(fmt);
		}
	}

	if (options[OptSetOutputOverlayFormat] || options[OptTryOutputOverlayFormat]) {
		struct v4l2_format fmt;

		fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
		if (doioctl(fd, VIDIOC_G_FMT, &fmt) == 0) {
			if (set_overlay_fmt_out & FmtChromaKey)
				fmt.fmt.win.chromakey = overlay_fmt_out.fmt.win.chromakey;
			if (set_overlay_fmt_out & FmtGlobalAlpha)
				fmt.fmt.win.global_alpha = overlay_fmt_out.fmt.win.global_alpha;
			if (set_overlay_fmt_out & FmtLeft)
				fmt.fmt.win.w.left = overlay_fmt_out.fmt.win.w.left;
			if (set_overlay_fmt_out & FmtTop)
				fmt.fmt.win.w.top = overlay_fmt_out.fmt.win.w.top;
			if (set_overlay_fmt_out & FmtWidth)
				fmt.fmt.win.w.width = overlay_fmt_out.fmt.win.w.width;
			if (set_overlay_fmt_out & FmtHeight)
				fmt.fmt.win.w.height = overlay_fmt_out.fmt.win.w.height;
			if (set_overlay_fmt_out & FmtField)
				fmt.fmt.win.field = overlay_fmt_out.fmt.win.field;
			if (options[OptSetOutputOverlayFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &fmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &fmt);
			if (ret == 0 && verbose)
				printfmt(fmt);
		}
	}

	if (options[OptSetFBuf]) {
		struct v4l2_framebuffer fb;

		if (doioctl(fd, VIDIOC_G_FBUF, &fb) == 0) {
			fb.flags &= ~set_fbuf;
			fb.flags |= fbuf.flags;
			doioctl(fd, VIDIOC_S_FBUF, &fb);
		}
	}

	if (options[OptSetJpegComp]) {
		doioctl(fd, VIDIOC_S_JPEGCOMP, &jpegcomp);
	}

	if (options[OptOverlay]) {
		doioctl(fd, VIDIOC_OVERLAY, &overlay);
	}

	if (options[OptSetCrop]) {
		do_crop(fd, set_crop, vcrop, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	}

	if (options[OptSetOutputCrop]) {
		do_crop(fd, set_crop_out, vcrop_out, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	}

	if (options[OptSetOverlayCrop]) {
		do_crop(fd, set_crop_overlay, vcrop_overlay, V4L2_BUF_TYPE_VIDEO_OVERLAY);
	}

	if (options[OptSetOutputOverlayCrop]) {
		do_crop(fd, set_crop_out_overlay, vcrop_out_overlay, V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY);
	}

	if (options[OptSetSelection]) {
		do_selection(fd, set_selection, vselection, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	}

	if (options[OptSetOutputSelection]) {
		do_selection(fd, set_selection_out, vselection_out, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	}
	
	if (options[OptEncoderCmd])
		doioctl(fd, VIDIOC_ENCODER_CMD, &enc_cmd);
	if (options[OptTryEncoderCmd])
		if (doioctl(fd, VIDIOC_TRY_ENCODER_CMD, &enc_cmd) == 0)
			print_enccmd(enc_cmd);
	if (options[OptDecoderCmd])
		doioctl(fd, VIDIOC_DECODER_CMD, &dec_cmd);
	if (options[OptTryDecoderCmd])
		if (doioctl(fd, VIDIOC_TRY_DECODER_CMD, &dec_cmd) == 0)
			print_deccmd(dec_cmd);

	/* Get options */

	common_get(fd);
	tuner_get(fd);
	io_get(fd);

	if (options[OptGetVideoFormat]) {
		vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (doioctl(fd, VIDIOC_G_FMT, &vfmt) == 0)
			printfmt(vfmt);
	}

	if (options[OptGetVideoMplaneFormat]) {
		vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		if (doioctl(fd, VIDIOC_G_FMT, &vfmt) == 0)
			printfmt(vfmt);
	}

	if (options[OptGetVideoOutFormat]) {
		vfmt_out.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (doioctl(fd, VIDIOC_G_FMT, &vfmt_out) == 0)
			printfmt(vfmt_out);
	}

	if (options[OptGetVideoOutMplaneFormat]) {
		vfmt_out.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		if (doioctl(fd, VIDIOC_G_FMT, &vfmt_out) == 0)
			printfmt(vfmt_out);
	}

	if (options[OptGetOverlayFormat]) {
		struct v4l2_format fmt;

		fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
		if (doioctl(fd, VIDIOC_G_FMT, &fmt) == 0)
			printfmt(fmt);
	}

	if (options[OptGetOutputOverlayFormat]) {
		struct v4l2_format fmt;

		fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
		if (doioctl(fd, VIDIOC_G_FMT, &fmt) == 0)
			printfmt(fmt);
	}

	if (options[OptGetSlicedVbiFormat]) {
		vbi_fmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
		if (doioctl(fd, VIDIOC_G_FMT, &vbi_fmt) == 0)
			printfmt(vbi_fmt);
	}

	if (options[OptGetSlicedVbiOutFormat]) {
		vbi_fmt_out.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
		if (doioctl(fd, VIDIOC_G_FMT, &vbi_fmt_out) == 0)
			printfmt(vbi_fmt_out);
	}

	if (options[OptGetVbiFormat]) {
		raw_fmt.type = V4L2_BUF_TYPE_VBI_CAPTURE;
		if (doioctl(fd, VIDIOC_G_FMT, &raw_fmt) == 0)
			printfmt(raw_fmt);
	}

	if (options[OptGetVbiOutFormat]) {
		raw_fmt_out.type = V4L2_BUF_TYPE_VBI_OUTPUT;
		if (doioctl(fd, VIDIOC_G_FMT, &raw_fmt_out) == 0)
			printfmt(raw_fmt_out);
	}

	if (options[OptGetFBuf]) {
		struct v4l2_framebuffer fb;
		if (doioctl(fd, VIDIOC_G_FBUF, &fb) == 0)
			printfbuf(fb);
	}

	if (options[OptGetJpegComp]) {
		struct v4l2_jpegcompression jc;
		if (doioctl(fd, VIDIOC_G_JPEGCOMP, &jc) == 0)
			printjpegcomp(jc);
	}

	if (options[OptGetCropCap]) {
		struct v4l2_cropcap cropcap;

		cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (doioctl(fd, VIDIOC_CROPCAP, &cropcap) == 0)
			printcropcap(cropcap);
	}

	if (options[OptGetOutputCropCap]) {
		struct v4l2_cropcap cropcap;

		cropcap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (doioctl(fd, VIDIOC_CROPCAP, &cropcap) == 0)
			printcropcap(cropcap);
	}

	if (options[OptGetOverlayCropCap]) {
		struct v4l2_cropcap cropcap;

		cropcap.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
		if (doioctl(fd, VIDIOC_CROPCAP, &cropcap) == 0)
			printcropcap(cropcap);
	}

	if (options[OptGetOutputOverlayCropCap]) {
		struct v4l2_cropcap cropcap;

		cropcap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
		if (doioctl(fd, VIDIOC_CROPCAP, &cropcap) == 0)
			printcropcap(cropcap);
	}

	if (options[OptGetCrop]) {
		struct v4l2_crop crop;

		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (doioctl(fd, VIDIOC_G_CROP, &crop) == 0)
			printcrop(crop);
	}

	if (options[OptGetOutputCrop]) {
		struct v4l2_crop crop;

		crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (doioctl(fd, VIDIOC_G_CROP, &crop) == 0)
			printcrop(crop);
	}

	if (options[OptGetOverlayCrop]) {
		struct v4l2_crop crop;

		crop.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
		if (doioctl(fd, VIDIOC_G_CROP, &crop) == 0)
			printcrop(crop);
	}

	if (options[OptGetOutputOverlayCrop]) {
		struct v4l2_crop crop;

		crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
		if (doioctl(fd, VIDIOC_G_CROP, &crop) == 0)
			printcrop(crop);
	}

	if (options[OptGetSelection]) {
		struct v4l2_selection sel;
		int t = 0;

		memset(&sel, 0, sizeof(sel));
		sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (get_sel_target == -1) {
			while (selection_targets_def[t++].str != NULL) {
				sel.target = selection_targets_def[t].flag;
				if (doioctl(fd, VIDIOC_G_SELECTION, &sel) == 0)
					print_selection(sel);
			}
		} else {
			sel.target = get_sel_target;
			if (doioctl(fd, VIDIOC_G_SELECTION, &sel) == 0)
				print_selection(sel);
		}
	}

	if (options[OptGetOutputSelection]) {
		struct v4l2_selection sel;
		int t = 0;

		memset(&sel, 0, sizeof(sel));
		sel.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

		if (get_sel_target == -1) {
			while (selection_targets_def[t++].str != NULL) {
				sel.target = selection_targets_def[t].flag;
				if (doioctl(fd, VIDIOC_G_SELECTION, &sel) == 0)
					print_selection(sel);
			}
		} else {
			sel.target = get_sel_target;
			if (doioctl(fd, VIDIOC_G_SELECTION, &sel) == 0)
				print_selection(sel);
		}
	}

	if (options[OptGetStandard]) {
		if (doioctl(fd, VIDIOC_G_STD, &std) == 0) {
			printf("Video Standard = 0x%08llx\n", (unsigned long long)std);
			print_v4lstd((unsigned long long)std);
		}
	}

	if (options[OptGetDvPreset]) {
		if (doioctl(fd, VIDIOC_G_DV_PRESET, &dv_preset) >= 0) {
			printf("DV preset: %d\n", dv_preset.preset);
		}
	}

	if (options[OptGetDvTimings]) {
		if (doioctl(fd, VIDIOC_G_DV_TIMINGS, &dv_timings) >= 0) {
			printf("DV timings:\n");
			print_dv_timings(&dv_timings);
		}
	}

	if (options[OptGetDvTimingsCap]) {
		if (doioctl(fd, VIDIOC_DV_TIMINGS_CAP, &dv_timings_cap) >= 0) {
			static const flag_def dv_caps_def[] = {
				{ V4L2_DV_BT_CAP_INTERLACED, "Interlaced" },
				{ V4L2_DV_BT_CAP_PROGRESSIVE, "Progressive" },
				{ V4L2_DV_BT_CAP_REDUCED_BLANKING, "Reduced Blanking" },
				{ V4L2_DV_BT_CAP_CUSTOM, "Custom Formats" },
				{ 0, NULL }
			};
			struct v4l2_bt_timings_cap *bt = &dv_timings_cap.bt;

			printf("DV timings capabilities:\n");
			if (dv_timings_cap.type != V4L2_DV_BT_656_1120)
				printf("\tUnknown type\n");
			else {
				printf("\tMinimum Width: %u\n", bt->min_width);
				printf("\tMaximum Width: %u\n", bt->max_width);
				printf("\tMinimum Height: %u\n", bt->min_height);
				printf("\tMaximum Height: %u\n", bt->max_height);
				printf("\tMinimum PClock: %llu\n", bt->min_pixelclock);
				printf("\tMaximum PClock: %llu\n", bt->max_pixelclock);
				printf("\tStandards: %s\n",
					flags2s(bt->standards, dv_standards_def).c_str());
				printf("\tCapabilities: %s\n",
					flags2s(bt->capabilities, dv_caps_def).c_str());
			}
		}
	}

        if (options[OptQueryStandard]) {
		if (doioctl(fd, VIDIOC_QUERYSTD, &std) == 0) {
			printf("Video Standard = 0x%08llx\n", (unsigned long long)std);
			print_v4lstd((unsigned long long)std);
		}
	}

        if (options[OptQueryDvPreset]) {
                doioctl(fd, VIDIOC_QUERY_DV_PRESET, &dv_preset);
                if (dv_preset.preset != V4L2_DV_INVALID) {
                        printf("Preset: %d\n", dv_preset.preset);
                } else {
                        fprintf(stderr, "No active input detected\n");
                }
        }

        if (options[OptQueryDvTimings]) {
                doioctl(fd, VIDIOC_QUERY_DV_TIMINGS, &dv_timings);
		print_dv_timings(&dv_timings);
        }

        if (options[OptGetParm]) {
		memset(&parm, 0, sizeof(parm));
		parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (doioctl(fd, VIDIOC_G_PARM, &parm) == 0) {
			const struct v4l2_fract &tf = parm.parm.capture.timeperframe;

			printf("Streaming Parameters %s:\n", buftype2s(parm.type).c_str());
			if (parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)
				printf("\tCapabilities     : timeperframe\n");
			if (parm.parm.capture.capturemode & V4L2_MODE_HIGHQUALITY)
				printf("\tCapture mode     : high quality\n");
			if (!tf.denominator || !tf.numerator)
				printf("\tFrames per second: invalid (%d/%d)\n",
						tf.denominator, tf.numerator);
			else
				printf("\tFrames per second: %.3f (%d/%d)\n",
						(1.0 * tf.denominator) / tf.numerator,
						tf.denominator, tf.numerator);
			printf("\tRead buffers     : %d\n", parm.parm.output.writebuffers);
		}
	}

	if (options[OptGetOutputParm]) {
		memset(&parm, 0, sizeof(parm));
		parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (doioctl(fd, VIDIOC_G_PARM, &parm) == 0) {
			const struct v4l2_fract &tf = parm.parm.output.timeperframe;

			printf("Streaming Parameters %s:\n", buftype2s(parm.type).c_str());
			if (parm.parm.output.capability & V4L2_CAP_TIMEPERFRAME)
				printf("\tCapabilities     : timeperframe\n");
			if (parm.parm.output.outputmode & V4L2_MODE_HIGHQUALITY)
				printf("\tOutput mode      : high quality\n");
			if (!tf.denominator || !tf.numerator)
				printf("\tFrames per second: invalid (%d/%d)\n",
						tf.denominator, tf.numerator);
			else
				printf("\tFrames per second: %.3f (%d/%d)\n",
						(1.0 * tf.denominator) / tf.numerator,
						tf.denominator, tf.numerator);
			printf("\tWrite buffers    : %d\n", parm.parm.output.writebuffers);
		}
	}

	/* List options */

	common_list(fd);
	io_list(fd);

	if (options[OptListStandards]) {
		printf("ioctl: VIDIOC_ENUMSTD\n");
		vs.index = 0;
		while (test_ioctl(fd, VIDIOC_ENUMSTD, &vs) >= 0) {
			if (vs.index)
				printf("\n");
			printf("\tIndex       : %d\n", vs.index);
			printf("\tID          : 0x%016llX\n", (unsigned long long)vs.id);
			printf("\tName        : %s\n", vs.name);
			printf("\tFrame period: %d/%d\n",
			       vs.frameperiod.numerator,
			       vs.frameperiod.denominator);
			printf("\tFrame lines : %d\n", vs.framelines);
			vs.index++;
		}
	}

	if (options[OptListFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	}

	if (options[OptListMplaneFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	}

	if (options[OptListFormatsExt]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats_ext(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	}

	if (options[OptListMplaneFormatsExt]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats_ext(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	}

	if (options[OptListFrameSizes]) {
		printf("ioctl: VIDIOC_ENUM_FRAMESIZES\n");
		frmsize.index = 0;
		while (test_ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
			print_frmsize(frmsize, "");
			frmsize.index++;
		}
	}

	if (options[OptListFrameIntervals]) {
		printf("ioctl: VIDIOC_ENUM_FRAMEINTERVALS\n");
		frmival.index = 0;
		while (test_ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) >= 0) {
			print_frmival(frmival, "");
			frmival.index++;
		}
	}

	if (options[OptListOverlayFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_VIDEO_OVERLAY);
	}

	if (options[OptListOutFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	}

	if (options[OptListOutMplaneFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	}

	if (options[OptGetSlicedVbiCap]) {
		struct v4l2_sliced_vbi_cap cap;

		cap.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
		if (doioctl(fd, VIDIOC_G_SLICED_VBI_CAP, &cap) == 0) {
			print_sliced_vbi_cap(cap);
		}
	}

	if (options[OptGetSlicedVbiOutCap]) {
		struct v4l2_sliced_vbi_cap cap;

		cap.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
		if (doioctl(fd, VIDIOC_G_SLICED_VBI_CAP, &cap) == 0) {
			print_sliced_vbi_cap(cap);
		}
	}

	if (options[OptListDvPresets]) {
		dv_enum_preset.index = 0;
		printf("ioctl: VIDIOC_ENUM_DV_PRESETS\n");
		while (test_ioctl(fd, VIDIOC_ENUM_DV_PRESETS, &dv_enum_preset) >= 0) {
			if (dv_enum_preset.index)
				printf("\n");
			printf("\tIndex   : %d\n", dv_enum_preset.index);
			printf("\tPreset  : %d\n", dv_enum_preset.preset);
			printf("\tName    : %s\n", dv_enum_preset.name);
			printf("\tWidth   : %d\n", dv_enum_preset.width);
			printf("\tHeight  : %d\n", dv_enum_preset.height);
			dv_enum_preset.index++;
		}
	}

	if (options[OptListDvTimings]) {
		dv_enum_timings.index = 0;
		printf("ioctl: VIDIOC_ENUM_DV_TIMINGS\n");
		while (test_ioctl(fd, VIDIOC_ENUM_DV_TIMINGS, &dv_enum_timings) >= 0) {
			if (dv_enum_timings.index)
				printf("\n");
			printf("\tIndex: %d\n", dv_enum_timings.index);
			print_dv_timings(&dv_enum_timings.timings);
			dv_enum_timings.index++;
		}
	}

	if (options[OptStreamOn]) {
		int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		doioctl(fd, VIDIOC_STREAMON, &type);
	}

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
