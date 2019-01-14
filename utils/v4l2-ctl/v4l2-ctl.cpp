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
#include <sys/sysmacros.h>
#include <dirent.h>
#include <math.h>

#include <linux/media.h>

#include "v4l2-ctl.h"

#include <media-info.h>

#ifdef HAVE_SYS_KLOG_H
#include <sys/klog.h>
#endif

#include <list>
#include <vector>
#include <map>
#include <algorithm>
#include <fstream>

char options[OptLast];

static int app_result;
int verbose;

unsigned capabilities;
unsigned out_capabilities;
unsigned priv_magic;
unsigned out_priv_magic;
bool is_multiplanar;
__u32 vidcap_buftype;
__u32 vidout_buftype;

static struct option long_options[] = {
	{"list-audio-inputs", no_argument, 0, OptListAudioInputs},
	{"list-audio-outputs", no_argument, 0, OptListAudioOutputs},
	{"all", no_argument, 0, OptAll},
	{"device", required_argument, 0, OptSetDevice},
	{"out-device", required_argument, 0, OptSetOutDevice},
	{"get-fmt-video", no_argument, 0, OptGetVideoFormat},
	{"set-fmt-video", required_argument, 0, OptSetVideoFormat},
	{"try-fmt-video", required_argument, 0, OptTryVideoFormat},
	{"get-fmt-video-out", no_argument, 0, OptGetVideoOutFormat},
	{"set-fmt-video-out", required_argument, 0, OptSetVideoOutFormat},
	{"try-fmt-video-out", required_argument, 0, OptTryVideoOutFormat},
	{"help", no_argument, 0, OptHelp},
	{"help-tuner", no_argument, 0, OptHelpTuner},
	{"help-io", no_argument, 0, OptHelpIO},
	{"help-stds", no_argument, 0, OptHelpStds},
	{"help-vidcap", no_argument, 0, OptHelpVidCap},
	{"help-vidout", no_argument, 0, OptHelpVidOut},
	{"help-overlay", no_argument, 0, OptHelpOverlay},
	{"help-vbi", no_argument, 0, OptHelpVbi},
	{"help-sdr", no_argument, 0, OptHelpSdr},
	{"help-meta", no_argument, 0, OptHelpMeta},
	{"help-subdev", no_argument, 0, OptHelpSubDev},
	{"help-selection", no_argument, 0, OptHelpSelection},
	{"help-misc", no_argument, 0, OptHelpMisc},
	{"help-streaming", no_argument, 0, OptHelpStreaming},
	{"help-edid", no_argument, 0, OptHelpEdid},
	{"help-all", no_argument, 0, OptHelpAll},
#ifndef NO_LIBV4L2
	{"wrapper", no_argument, 0, OptUseWrapper},
#endif
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
	{"list-formats-ext", no_argument, 0, OptListFormatsExt},
	{"list-fields", no_argument, 0, OptListFields},
	{"list-framesizes", required_argument, 0, OptListFrameSizes},
	{"list-frameintervals", required_argument, 0, OptListFrameIntervals},
	{"list-formats-overlay", no_argument, 0, OptListOverlayFormats},
	{"list-formats-sdr", no_argument, 0, OptListSdrFormats},
	{"list-formats-sdr-out", no_argument, 0, OptListSdrOutFormats},
	{"list-formats-out", no_argument, 0, OptListOutFormats},
	{"list-formats-out-ext", no_argument, 0, OptListOutFormatsExt},
	{"list-formats-meta", no_argument, 0, OptListMetaFormats},
	{"list-subdev-mbus-codes", optional_argument, 0, OptListSubDevMBusCodes},
	{"list-subdev-framesizes", required_argument, 0, OptListSubDevFrameSizes},
	{"list-subdev-frameintervals", required_argument, 0, OptListSubDevFrameIntervals},
	{"list-fields-out", no_argument, 0, OptListOutFields},
	{"clear-clips", no_argument, 0, OptClearClips},
	{"clear-bitmap", no_argument, 0, OptClearBitmap},
	{"add-clip", required_argument, 0, OptAddClip},
	{"add-bitmap", required_argument, 0, OptAddBitmap},
	{"find-fb", no_argument, 0, OptFindFb},
	{"subset", required_argument, 0, OptSubset},
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
	{"set-fmt-overlay", optional_argument, 0, OptSetOverlayFormat},
	{"try-fmt-overlay", optional_argument, 0, OptTryOverlayFormat},
	{"get-fmt-sliced-vbi", no_argument, 0, OptGetSlicedVbiFormat},
	{"set-fmt-sliced-vbi", required_argument, 0, OptSetSlicedVbiFormat},
	{"try-fmt-sliced-vbi", required_argument, 0, OptTrySlicedVbiFormat},
	{"get-fmt-sliced-vbi-out", no_argument, 0, OptGetSlicedVbiOutFormat},
	{"set-fmt-sliced-vbi-out", required_argument, 0, OptSetSlicedVbiOutFormat},
	{"try-fmt-sliced-vbi-out", required_argument, 0, OptTrySlicedVbiOutFormat},
	{"get-fmt-vbi", no_argument, 0, OptGetVbiFormat},
	{"set-fmt-vbi", required_argument, 0, OptSetVbiFormat},
	{"try-fmt-vbi", required_argument, 0, OptTryVbiFormat},
	{"get-fmt-vbi-out", no_argument, 0, OptGetVbiOutFormat},
	{"set-fmt-vbi-out", required_argument, 0, OptSetVbiOutFormat},
	{"try-fmt-vbi-out", required_argument, 0, OptTryVbiOutFormat},
	{"get-fmt-sdr", no_argument, 0, OptGetSdrFormat},
	{"set-fmt-sdr", required_argument, 0, OptSetSdrFormat},
	{"try-fmt-sdr", required_argument, 0, OptTrySdrFormat},
	{"get-fmt-sdr-out", no_argument, 0, OptGetSdrOutFormat},
	{"set-fmt-sdr-out", required_argument, 0, OptSetSdrOutFormat},
	{"try-fmt-sdr-out", required_argument, 0, OptTrySdrOutFormat},
	{"get-fmt-meta", no_argument, 0, OptGetMetaFormat},
	{"set-fmt-meta", required_argument, 0, OptSetMetaFormat},
	{"try-fmt-meta", required_argument, 0, OptTryMetaFormat},
	{"get-subdev-fmt", optional_argument, 0, OptGetSubDevFormat},
	{"set-subdev-fmt", required_argument, 0, OptSetSubDevFormat},
	{"try-subdev-fmt", required_argument, 0, OptTrySubDevFormat},
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
	{"get-subdev-selection", required_argument, 0, OptGetSubDevSelection},
	{"set-subdev-selection", required_argument, 0, OptSetSubDevSelection},
	{"try-subdev-selection", required_argument, 0, OptTrySubDevSelection},
	{"get-subdev-fps", optional_argument, 0, OptGetSubDevFPS},
	{"set-subdev-fps", required_argument, 0, OptSetSubDevFPS},
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
	{"list-dv-timings", optional_argument, 0, OptListDvTimings},
	{"query-dv-timings", no_argument, 0, OptQueryDvTimings},
	{"get-dv-timings", no_argument, 0, OptGetDvTimings},
	{"set-dv-bt-timings", required_argument, 0, OptSetDvBtTimings},
	{"get-dv-timings-cap", optional_argument, 0, OptGetDvTimingsCap},
	{"freq-seek", required_argument, 0, OptFreqSeek},
	{"encoder-cmd", required_argument, 0, OptEncoderCmd},
	{"try-encoder-cmd", required_argument, 0, OptTryEncoderCmd},
	{"decoder-cmd", required_argument, 0, OptDecoderCmd},
	{"try-decoder-cmd", required_argument, 0, OptTryDecoderCmd},
	{"set-edid", required_argument, 0, OptSetEdid},
	{"clear-edid", optional_argument, 0, OptClearEdid},
	{"get-edid", optional_argument, 0, OptGetEdid},
	{"info-edid", optional_argument, 0, OptInfoEdid},
	{"fix-edid-checksums", no_argument, 0, OptFixEdidChecksums},
	{"tuner-index", required_argument, 0, OptTunerIndex},
	{"list-buffers", no_argument, 0, OptListBuffers},
	{"list-buffers-out", no_argument, 0, OptListBuffersOut},
	{"list-buffers-vbi", no_argument, 0, OptListBuffersVbi},
	{"list-buffers-sliced-vbi", no_argument, 0, OptListBuffersSlicedVbi},
	{"list-buffers-vbi-out", no_argument, 0, OptListBuffersVbiOut},
	{"list-buffers-sliced-vbi-out", no_argument, 0, OptListBuffersSlicedVbiOut},
	{"list-buffers-sdr", no_argument, 0, OptListBuffersSdr},
	{"list-buffers-sdr-out", no_argument, 0, OptListBuffersSdrOut},
	{"list-buffers-meta", no_argument, 0, OptListBuffersMeta},
	{"stream-count", required_argument, 0, OptStreamCount},
	{"stream-skip", required_argument, 0, OptStreamSkip},
	{"stream-loop", no_argument, 0, OptStreamLoop},
	{"stream-sleep", required_argument, 0, OptStreamSleep},
	{"stream-poll", no_argument, 0, OptStreamPoll},
	{"stream-no-query", no_argument, 0, OptStreamNoQuery},
#ifndef NO_STREAM_TO
	{"stream-to", required_argument, 0, OptStreamTo},
	{"stream-to-hdr", required_argument, 0, OptStreamToHdr},
	{"stream-lossless", no_argument, 0, OptStreamLossless},
	{"stream-to-host", required_argument, 0, OptStreamToHost},
#endif
	{"stream-buf-caps", no_argument, 0, OptStreamBufCaps},
	{"stream-mmap", optional_argument, 0, OptStreamMmap},
	{"stream-user", optional_argument, 0, OptStreamUser},
	{"stream-dmabuf", no_argument, 0, OptStreamDmaBuf},
	{"stream-from", required_argument, 0, OptStreamFrom},
	{"stream-from-hdr", required_argument, 0, OptStreamFromHdr},
	{"stream-from-host", required_argument, 0, OptStreamFromHost},
	{"stream-out-pattern", required_argument, 0, OptStreamOutPattern},
	{"stream-out-square", no_argument, 0, OptStreamOutSquare},
	{"stream-out-border", no_argument, 0, OptStreamOutBorder},
	{"stream-out-sav", no_argument, 0, OptStreamOutInsertSAV},
	{"stream-out-eav", no_argument, 0, OptStreamOutInsertEAV},
	{"stream-out-pixel-aspect", required_argument, 0, OptStreamOutPixelAspect},
	{"stream-out-video-aspect", required_argument, 0, OptStreamOutVideoAspect},
	{"stream-out-alpha", required_argument, 0, OptStreamOutAlphaComponent},
	{"stream-out-alpha-red-only", no_argument, 0, OptStreamOutAlphaRedOnly},
	{"stream-out-rgb-lim-range", required_argument, 0, OptStreamOutRGBLimitedRange},
	{"stream-out-hor-speed", required_argument, 0, OptStreamOutHorSpeed},
	{"stream-out-vert-speed", required_argument, 0, OptStreamOutVertSpeed},
	{"stream-out-perc-fill", required_argument, 0, OptStreamOutPercFill},
	{"stream-out-buf-caps", no_argument, 0, OptStreamOutBufCaps},
	{"stream-out-mmap", optional_argument, 0, OptStreamOutMmap},
	{"stream-out-user", optional_argument, 0, OptStreamOutUser},
	{"stream-out-dmabuf", no_argument, 0, OptStreamOutDmaBuf},
	{"list-patterns", no_argument, 0, OptListPatterns},
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
       sdr_usage();
       meta_usage();
       selection_usage();
       misc_usage();
       streaming_usage();
       edid_usage();
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

/*
 * Any pixelformat that is not a YUV format is assumed to be
 * RGB or HSV.
 */
static bool is_rgb_or_hsv(__u32 pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_UV8:
	case V4L2_PIX_FMT_YVU410:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YYUV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV411P:
	case V4L2_PIX_FMT_Y41P:
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_YUV565:
	case V4L2_PIX_FMT_YUV32:
	case V4L2_PIX_FMT_YUV410:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_HI240:
	case V4L2_PIX_FMT_HM12:
	case V4L2_PIX_FMT_M420:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
	case V4L2_PIX_FMT_NV12MT:
	case V4L2_PIX_FMT_NV12MT_16X16:
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
	case V4L2_PIX_FMT_SN9C20X_I420:
	case V4L2_PIX_FMT_SPCA501:
	case V4L2_PIX_FMT_SPCA505:
	case V4L2_PIX_FMT_SPCA508:
	case V4L2_PIX_FMT_CIT_YYVYUY:
	case V4L2_PIX_FMT_KONICA420:
		return false;
	default:
		return true;
	}
}

static std::string printfmtname(int fd, __u32 type, __u32 pixfmt)
{
	struct v4l2_fmtdesc fmt;
	std::string s(" (");

	fmt.index = 0;
	fmt.type = type;
	while (test_ioctl(fd, VIDIOC_ENUM_FMT, &fmt) >= 0) {
		if (fmt.pixelformat == pixfmt)
			return s + (const char *)fmt.description + ")";
		fmt.index++;
	}
	return "";
}

void printfmt(int fd, const struct v4l2_format &vfmt)
{
	__u32 colsp = vfmt.fmt.pix.colorspace;
	__u32 ycbcr_enc = vfmt.fmt.pix.ycbcr_enc;

	printf("Format %s:\n", buftype2s(vfmt.type).c_str());

	switch (vfmt.type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		printf("\tWidth/Height      : %u/%u\n", vfmt.fmt.pix.width, vfmt.fmt.pix.height);
		printf("\tPixel Format      : '%s'%s\n", fcc2s(vfmt.fmt.pix.pixelformat).c_str(),
		       printfmtname(fd, vfmt.type, vfmt.fmt.pix.pixelformat).c_str());
		printf("\tField             : %s\n", field2s(vfmt.fmt.pix.field).c_str());
		printf("\tBytes per Line    : %u\n", vfmt.fmt.pix.bytesperline);
		printf("\tSize Image        : %u\n", vfmt.fmt.pix.sizeimage);
		printf("\tColorspace        : %s\n", colorspace2s(colsp).c_str());
		printf("\tTransfer Function : %s", xfer_func2s(vfmt.fmt.pix.xfer_func).c_str());
		if (vfmt.fmt.pix.xfer_func == V4L2_XFER_FUNC_DEFAULT)
			printf(" (maps to %s)",
			       xfer_func2s(V4L2_MAP_XFER_FUNC_DEFAULT(colsp)).c_str());
		printf("\n");
		printf("\tYCbCr/HSV Encoding: %s", ycbcr_enc2s(ycbcr_enc).c_str());
		if (ycbcr_enc == V4L2_YCBCR_ENC_DEFAULT) {
			ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(colsp);
			printf(" (maps to %s)", ycbcr_enc2s(ycbcr_enc).c_str());
		}
		printf("\n");
		printf("\tQuantization      : %s", quantization2s(vfmt.fmt.pix.quantization).c_str());
		if (vfmt.fmt.pix.quantization == V4L2_QUANTIZATION_DEFAULT)
			printf(" (maps to %s)",
			       quantization2s(V4L2_MAP_QUANTIZATION_DEFAULT(is_rgb_or_hsv(vfmt.fmt.pix.pixelformat),
									    colsp, ycbcr_enc)).c_str());
		printf("\n");
		if (vfmt.fmt.pix.priv == V4L2_PIX_FMT_PRIV_MAGIC)
			printf("\tFlags             : %s\n", pixflags2s(vfmt.fmt.pix.flags).c_str());
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		printf("\tWidth/Height      : %u/%u\n", vfmt.fmt.pix_mp.width, vfmt.fmt.pix_mp.height);
		printf("\tPixel Format      : '%s'%s\n", fcc2s(vfmt.fmt.pix_mp.pixelformat).c_str(),
		       printfmtname(fd, vfmt.type, vfmt.fmt.pix_mp.pixelformat).c_str());
		printf("\tField             : %s\n", field2s(vfmt.fmt.pix_mp.field).c_str());
		printf("\tNumber of planes  : %u\n", vfmt.fmt.pix_mp.num_planes);
		printf("\tFlags             : %s\n", pixflags2s(vfmt.fmt.pix_mp.flags).c_str());
		printf("\tColorspace        : %s\n", colorspace2s(vfmt.fmt.pix_mp.colorspace).c_str());
		printf("\tTransfer Function : %s\n", xfer_func2s(vfmt.fmt.pix_mp.xfer_func).c_str());
		printf("\tYCbCr/HSV Encoding: %s\n", ycbcr_enc2s(vfmt.fmt.pix_mp.ycbcr_enc).c_str());
		printf("\tQuantization      : %s\n", quantization2s(vfmt.fmt.pix_mp.quantization).c_str());
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
		if (vfmt.fmt.win.clips)
			for (unsigned i = 0; i < vfmt.fmt.win.clipcount; i++) {
				struct v4l2_rect &r = vfmt.fmt.win.clips[i].c;

				printf("\t\tClip %2d: %ux%u@%ux%u\n", i,
						r.width, r.height, r.left, r.top);
			}
		printf("\tClip Bitmap : %s", vfmt.fmt.win.bitmap ? "Yes, " : "No\n");
		if (vfmt.fmt.win.bitmap) {
			unsigned char *bitmap = (unsigned char *)vfmt.fmt.win.bitmap;
			unsigned stride = (vfmt.fmt.win.w.width + 7) / 8;
			unsigned cnt = 0;

			for (unsigned y = 0; y < vfmt.fmt.win.w.height; y++)
				for (unsigned x = 0; x < vfmt.fmt.win.w.width; x++)
					if (bitmap[y * stride + x / 8] & (1 << (x & 7)))
						cnt++;
			printf("%u bits of %u are set\n", cnt,
					vfmt.fmt.win.w.width * vfmt.fmt.win.w.height);
		}
		break;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		printf("\tSampling Rate   : %u Hz\n", vfmt.fmt.vbi.sampling_rate);
		printf("\tOffset          : %u samples (%g secs after leading edge)\n",
				vfmt.fmt.vbi.offset,
				(double)vfmt.fmt.vbi.offset / (double)vfmt.fmt.vbi.sampling_rate);
		printf("\tSamples per Line: %u\n", vfmt.fmt.vbi.samples_per_line);
		printf("\tSample Format   : '%s'\n", fcc2s(vfmt.fmt.vbi.sample_format).c_str());
		printf("\tStart 1st Field : %u\n", vfmt.fmt.vbi.start[0]);
		printf("\tCount 1st Field : %u\n", vfmt.fmt.vbi.count[0]);
		printf("\tStart 2nd Field : %u\n", vfmt.fmt.vbi.start[1]);
		printf("\tCount 2nd Field : %u\n", vfmt.fmt.vbi.count[1]);
		if (vfmt.fmt.vbi.flags)
			printf("\tFlags           : %s\n", vbiflags2s(vfmt.fmt.vbi.flags).c_str());
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
	case V4L2_BUF_TYPE_SDR_CAPTURE:
	case V4L2_BUF_TYPE_SDR_OUTPUT:
		printf("\tSample Format   : '%s'%s\n", fcc2s(vfmt.fmt.sdr.pixelformat).c_str(),
		       printfmtname(fd, vfmt.type, vfmt.fmt.sdr.pixelformat).c_str());
		printf("\tBuffer Size     : %u\n", vfmt.fmt.sdr.buffersize);
		break;
	case V4L2_BUF_TYPE_META_CAPTURE:
		printf("\tSample Format   : '%s'%s\n", fcc2s(vfmt.fmt.meta.dataformat).c_str(),
		       printfmtname(fd, vfmt.type, vfmt.fmt.meta.dataformat).c_str());
		printf("\tBuffer Size     : %u\n", vfmt.fmt.meta.buffersize);
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

	sprintf(buf, "%.3f", (1.0 * f.numerator) / f.denominator);
	return buf;
}

static std::string fract2fps(const struct v4l2_fract &f)
{
	char buf[100];

	sprintf(buf, "%.3f", (1.0 * f.denominator) / f.numerator);
	return buf;
}

void print_frmsize(const struct v4l2_frmsizeenum &frmsize, const char *prefix)
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

void print_frmival(const struct v4l2_frmivalenum &frmival, const char *prefix)
{
	printf("%s\tInterval: %s ", prefix, frmtype2s(frmival.type).c_str());
	if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
		printf("%ss (%s fps)\n", fract2sec(frmival.discrete).c_str(),
				fract2fps(frmival.discrete).c_str());
	} else if (frmival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
		printf("%ss - %ss (%s-%s fps)\n",
				fract2sec(frmival.stepwise.min).c_str(),
				fract2sec(frmival.stepwise.max).c_str(),
				fract2fps(frmival.stepwise.max).c_str(),
				fract2fps(frmival.stepwise.min).c_str());
	} else if (frmival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
		printf("%ss - %ss with step %ss (%s-%s fps)\n",
				fract2sec(frmival.stepwise.min).c_str(),
				fract2sec(frmival.stepwise.max).c_str(),
				fract2sec(frmival.stepwise.step).c_str(),
				fract2fps(frmival.stepwise.max).c_str(),
				fract2fps(frmival.stepwise.min).c_str());
	}
}

void print_video_formats(cv4l_fd &fd, __u32 type)
{
	cv4l_disable_trace dt(fd);
	struct v4l2_fmtdesc fmt;

	printf("\tType: %s\n\n", buftype2s(type).c_str());
	if (fd.enum_fmt(fmt, true, 0, type))
		return;
	do {
		printf("\t[%d]: '%s' (%s", fmt.index, fcc2s(fmt.pixelformat).c_str(),
		       fmt.description);
		if (fmt.flags)
			printf(", %s", fmtdesc2s(fmt.flags).c_str());
		printf(")\n");
	} while (!fd.enum_fmt(fmt));
}

void print_video_formats_ext(cv4l_fd &fd, __u32 type)
{
	cv4l_disable_trace dt(fd);
	struct v4l2_fmtdesc fmt;
	struct v4l2_frmsizeenum frmsize;
	struct v4l2_frmivalenum frmival;

	printf("\tType: %s\n\n", buftype2s(type).c_str());
	if (fd.enum_fmt(fmt, true, 0, type))
		return;
	do {
		printf("\t[%d]: '%s' (%s", fmt.index, fcc2s(fmt.pixelformat).c_str(),
		       fmt.description);
		if (fmt.flags)
			printf(", %s", fmtdesc2s(fmt.flags).c_str());
		printf(")\n");
		if (fd.enum_framesizes(frmsize, fmt.pixelformat))
			continue;
		do {
			print_frmsize(frmsize, "\t");
			if (frmsize.type != V4L2_FRMSIZE_TYPE_DISCRETE)
				continue;

			if (fd.enum_frameintervals(frmival, fmt.pixelformat,
						   frmsize.discrete.width,
						   frmsize.discrete.height))
				continue;
			do {
				print_frmival(frmival, "\t\t");
			} while (!fd.enum_frameintervals(frmival));
		} while (!fd.enum_framesizes(frmsize));
	} while (!fd.enum_fmt(fmt));
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

__u32 parse_field(const char *s)
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

__u32 parse_colorspace(const char *s)
{
	if (!strcmp(s, "smpte170m")) return V4L2_COLORSPACE_SMPTE170M;
	if (!strcmp(s, "smpte240m")) return V4L2_COLORSPACE_SMPTE240M;
	if (!strcmp(s, "rec709")) return V4L2_COLORSPACE_REC709;
	if (!strcmp(s, "470m")) return V4L2_COLORSPACE_470_SYSTEM_M;
	if (!strcmp(s, "470bg")) return V4L2_COLORSPACE_470_SYSTEM_BG;
	if (!strcmp(s, "jpeg")) return V4L2_COLORSPACE_JPEG;
	if (!strcmp(s, "srgb")) return V4L2_COLORSPACE_SRGB;
	if (!strcmp(s, "oprgb")) return V4L2_COLORSPACE_OPRGB;
	if (!strcmp(s, "bt2020")) return V4L2_COLORSPACE_BT2020;
	if (!strcmp(s, "dcip3")) return V4L2_COLORSPACE_DCI_P3;
	return 0;
}

__u32 parse_xfer_func(const char *s)
{
	if (!strcmp(s, "default")) return V4L2_XFER_FUNC_DEFAULT;
	if (!strcmp(s, "smpte240m")) return V4L2_XFER_FUNC_SMPTE240M;
	if (!strcmp(s, "rec709")) return V4L2_XFER_FUNC_709;
	if (!strcmp(s, "srgb")) return V4L2_XFER_FUNC_SRGB;
	if (!strcmp(s, "oprgb")) return V4L2_XFER_FUNC_OPRGB;
	if (!strcmp(s, "dcip3")) return V4L2_XFER_FUNC_DCI_P3;
	if (!strcmp(s, "smpte2084")) return V4L2_XFER_FUNC_SMPTE2084;
	if (!strcmp(s, "none")) return V4L2_XFER_FUNC_NONE;
	return 0;
}

__u32 parse_ycbcr(const char *s)
{
	if (!strcmp(s, "default")) return V4L2_YCBCR_ENC_DEFAULT;
	if (!strcmp(s, "601")) return V4L2_YCBCR_ENC_601;
	if (!strcmp(s, "709")) return V4L2_YCBCR_ENC_709;
	if (!strcmp(s, "xv601")) return V4L2_YCBCR_ENC_XV601;
	if (!strcmp(s, "xv709")) return V4L2_YCBCR_ENC_XV709;
	if (!strcmp(s, "bt2020")) return V4L2_YCBCR_ENC_BT2020;
	if (!strcmp(s, "bt2020c")) return V4L2_YCBCR_ENC_BT2020_CONST_LUM;
	if (!strcmp(s, "smpte240m")) return V4L2_YCBCR_ENC_SMPTE240M;
	return V4L2_YCBCR_ENC_DEFAULT;
}

__u32 parse_hsv(const char *s)
{
	if (!strcmp(s, "default")) return V4L2_YCBCR_ENC_DEFAULT;
	if (!strcmp(s, "180")) return V4L2_HSV_ENC_180;
	if (!strcmp(s, "256")) return V4L2_HSV_ENC_256;
	return V4L2_YCBCR_ENC_DEFAULT;
}

__u32 parse_quantization(const char *s)
{
	if (!strcmp(s, "default")) return V4L2_QUANTIZATION_DEFAULT;
	if (!strcmp(s, "full-range")) return V4L2_QUANTIZATION_FULL_RANGE;
	if (!strcmp(s, "lim-range")) return V4L2_QUANTIZATION_LIM_RANGE;
	return V4L2_QUANTIZATION_DEFAULT;
}

int parse_fmt(char *optarg, __u32 &width, __u32 &height, __u32 &pixelformat,
	      __u32 &field, __u32 &colorspace, __u32 &xfer_func, __u32 &ycbcr,
	      __u32 &quantization, __u32 &flags, __u32 *bytesperline)
{
	char *value, *subs;
	int fmts = 0;
	unsigned bpl_index = 0;
	bool be_pixfmt;

	field = V4L2_FIELD_ANY;
	flags = 0;
	subs = optarg;
	while (*subs != '\0') {
		static const char *const subopts[] = {
			"width",
			"height",
			"pixelformat",
			"field",
			"colorspace",
			"ycbcr",
			"hsv",
			"bytesperline",
			"premul-alpha",
			"quantization",
			"xfer",
			NULL
		};

		switch (parse_subopt(&subs, subopts, &value)) {
		case 0:
			width = strtoul(value, 0L, 0);
			fmts |= FmtWidth;
			break;
		case 1:
			height = strtoul(value, 0L, 0);
			fmts |= FmtHeight;
			break;
		case 2:
			be_pixfmt = strlen(value) == 7 && !memcmp(value + 4, "-BE", 3);
			if (be_pixfmt)
				value[4] = 0;
			if (strlen(value) == 4) {
				pixelformat =
					v4l2_fourcc(value[0], value[1],
							value[2], value[3]);
				if (be_pixfmt)
					pixelformat |= 1 << 31;
			} else {
				pixelformat = strtol(value, 0L, 0);
			}
			fmts |= FmtPixelFormat;
			break;
		case 3:
			field = parse_field(value);
			fmts |= FmtField;
			break;
		case 4:
			colorspace = parse_colorspace(value);
			if (colorspace)
				fmts |= FmtColorspace;
			else
				fprintf(stderr, "unknown colorspace %s\n", value);
			break;
		case 5:
			ycbcr = parse_ycbcr(value);
			fmts |= FmtYCbCr;
			break;
		case 6:
			ycbcr = parse_hsv(value);
			fmts |= FmtYCbCr;
			break;
		case 7:
			bytesperline[bpl_index] = strtoul(value, 0L, 0);
			if (bytesperline[bpl_index] > 0xffff) {
				fprintf(stderr, "bytesperline can't be more than 65535\n");
				bytesperline[bpl_index] = 0;
			}
			bpl_index++;
			fmts |= FmtBytesPerLine;
			break;
		case 8:
			flags |= V4L2_PIX_FMT_FLAG_PREMUL_ALPHA;
			fmts |= FmtFlags;
			break;
		case 9:
			quantization = parse_quantization(value);
			fmts |= FmtQuantization;
			break;
		case 10:
			xfer_func = parse_xfer_func(value);
			fmts |= FmtXferFunc;
			break;
		default:
			return 0;
		}
	}
	return fmts;
}

int parse_selection_flags(const char *s)
{
	if (!strcmp(s, "le")) return V4L2_SEL_FLAG_LE;
	if (!strcmp(s, "ge")) return V4L2_SEL_FLAG_GE;
	if (!strcmp(s, "keep-config")) return V4L2_SEL_FLAG_KEEP_CONFIG;
	return 0;
}

void print_selection(const struct v4l2_selection &sel)
{
	printf("Selection: %s, Left %d, Top %d, Width %d, Height %d, Flags: %s\n",
			seltarget2s(sel.target).c_str(),
			sel.r.left, sel.r.top, sel.r.width, sel.r.height,
			selflags2s(sel.flags).c_str());
}

int parse_selection_target(const char *s, unsigned int &target)
{
	if (!strcmp(s, "crop")) target = V4L2_SEL_TGT_CROP_ACTIVE;
	else if (!strcmp(s, "crop_default")) target = V4L2_SEL_TGT_CROP_DEFAULT;
	else if (!strcmp(s, "crop_bounds")) target = V4L2_SEL_TGT_CROP_BOUNDS;
	else if (!strcmp(s, "compose")) target = V4L2_SEL_TGT_COMPOSE_ACTIVE;
	else if (!strcmp(s, "compose_default")) target = V4L2_SEL_TGT_COMPOSE_DEFAULT;
	else if (!strcmp(s, "compose_bounds")) target = V4L2_SEL_TGT_COMPOSE_BOUNDS;
	else if (!strcmp(s, "compose_padded")) target = V4L2_SEL_TGT_COMPOSE_PADDED;
	else if (!strcmp(s, "native_size")) target = V4L2_SEL_TGT_NATIVE_SIZE;
	else return -EINVAL;

	return 0;
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
	case V4L2_EVENT_SOURCE_CHANGE:
		printf("source_change: pad/input=%d changes: %x\n", ev->id, ev->u.src_change.changes);
		break;
	case V4L2_EVENT_MOTION_DET:
		if (ev->u.motion_det.flags & V4L2_EVENT_MD_FL_HAVE_FRAME_SEQ)
			printf("motion_det frame %d, regions 0x%x\n",
					ev->u.motion_det.frame_sequence,
					ev->u.motion_det.region_mask);
		else
			printf("motion_det regions 0x%x\n", ev->u.motion_det.region_mask);
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

	*name = "0";
	if (isdigit(e[0])) {
		event = strtoul(e, 0L, 0);
		if (event == V4L2_EVENT_CTRL) {
			fprintf(stderr, "Missing control name for ctrl event, use ctrl=<name>\n");
			misc_usage();
			exit(1);
		}
	} else if (!strcmp(e, "eos")) {
		event = V4L2_EVENT_EOS;
	} else if (!strcmp(e, "vsync")) {
		event = V4L2_EVENT_VSYNC;
	} else if (!strcmp(e, "frame_sync")) {
		event = V4L2_EVENT_FRAME_SYNC;
	} else if (!strcmp(e, "motion_det")) {
		event = V4L2_EVENT_MOTION_DET;
	} else if (!strncmp(e, "ctrl=", 5)) {
		event = V4L2_EVENT_CTRL;
		*name = e + 5;
	} else if (!strncmp(e, "source_change=", 14)) {
		event = V4L2_EVENT_SOURCE_CHANGE;
		*name = e + 14;
	} else if (!strcmp(e, "source_change")) {
		event = V4L2_EVENT_SOURCE_CHANGE;
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
	cv4l_fd c_fd;
	cv4l_fd c_out_fd;
	int fd = -1;
	int out_fd = -1;
	int media_fd = -1;
	bool is_subdev = false;

	/* command args */
	int ch;
	const char *device = "/dev/video0";	/* -d device */
	const char *out_device = NULL;
	struct v4l2_capability vcap;	/* list_cap */
	__u32 wait_for_event = 0;	/* wait for this event */
	const char *wait_event_id = NULL;
	__u32 poll_for_event = 0;	/* poll for this event */
	const char *poll_event_id = NULL;
	unsigned secs = 0;
	char short_options[26 * 2 * 3 + 1];
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
		case OptHelpSdr:
			sdr_usage();
			return 0;
		case OptHelpMeta:
			meta_usage();
			return 0;
		case OptHelpSubDev:
			subdev_usage();
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
		case OptHelpEdid:
			edid_usage();
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
		case OptSetOutDevice:
			out_device = optarg;
			if (out_device[0] >= '0' && out_device[0] <= '9' && strlen(out_device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/video%s", out_device);
				out_device = newdev;
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
			sdr_cmd(ch, optarg);
			meta_cmd(ch, optarg);
			subdev_cmd(ch, optarg);
			selection_cmd(ch, optarg);
			misc_cmd(ch, optarg);
			streaming_cmd(ch, optarg);
			edid_cmd(ch, optarg);
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

	media_type type = mi_media_detect_type(device);
	if (type == MEDIA_TYPE_CANT_STAT) {
		fprintf(stderr, "Cannot open device %s, exiting.\n",
			device);
		exit(1);
	}

	switch (type) {
		// For now we can only handle V4L2 devices
	case MEDIA_TYPE_VIDEO:
	case MEDIA_TYPE_VBI:
	case MEDIA_TYPE_RADIO:
	case MEDIA_TYPE_SDR:
	case MEDIA_TYPE_TOUCH:
	case MEDIA_TYPE_SUBDEV:
		break;
	default:
		type = MEDIA_TYPE_UNKNOWN;
		break;
	}

	if (type == MEDIA_TYPE_UNKNOWN) {
		fprintf(stderr, "Unable to detect what device %s is, exiting.\n",
			device);
		exit(1);
	}
	is_subdev = type == MEDIA_TYPE_SUBDEV;
	if (is_subdev)
		options[OptUseWrapper] = 0;
	c_fd.s_direct(!options[OptUseWrapper]);
	c_out_fd.s_direct(!options[OptUseWrapper]);

	if (is_subdev)
		fd = c_fd.subdev_open(device);
	else
		fd = c_fd.open(device);

	if (fd < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", device,
			strerror(errno));
		exit(1);
	}
	verbose = options[OptVerbose];
	c_fd.s_trace(options[OptSilent] ? 0 : (verbose ? 2 : 1));
	c_out_fd.s_trace(options[OptSilent] ? 0 : (verbose ? 2 : 1));

	if (!is_subdev && doioctl(fd, VIDIOC_QUERYCAP, &vcap)) {
		fprintf(stderr, "%s: not a v4l2 node\n", device);
		exit(1);
	}
	capabilities = vcap.capabilities;
	if (capabilities & V4L2_CAP_DEVICE_CAPS)
		capabilities = vcap.device_caps;

	media_fd = mi_get_media_fd(fd);

	priv_magic = (capabilities & V4L2_CAP_EXT_PIX_FORMAT) ?
			V4L2_PIX_FMT_PRIV_MAGIC : 0;
	is_multiplanar = capabilities & (V4L2_CAP_VIDEO_CAPTURE_MPLANE |
					 V4L2_CAP_VIDEO_M2M_MPLANE |
					 V4L2_CAP_VIDEO_OUTPUT_MPLANE);

	vidcap_buftype = is_multiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
					  V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vidout_buftype = is_multiplanar ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE :
					  V4L2_BUF_TYPE_VIDEO_OUTPUT;

	if (out_device) {
		out_fd = c_out_fd.open(out_device);
		if (out_fd < 0) {
			fprintf(stderr, "Failed to open %s: %s\n", out_device,
					strerror(errno));
			exit(1);
		}
		if (doioctl(out_fd, VIDIOC_QUERYCAP, &vcap)) {
			fprintf(stderr, "%s: not a v4l2 node\n", out_device);
			exit(1);
		}
		out_capabilities = vcap.capabilities;
		if (out_capabilities & V4L2_CAP_DEVICE_CAPS)
			out_capabilities = vcap.device_caps;
		out_priv_magic = (out_capabilities & V4L2_CAP_EXT_PIX_FORMAT) ?
				V4L2_PIX_FMT_PRIV_MAGIC : 0;
	}

	common_process_controls(c_fd);

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
		options[OptGetVbiFormat] = 1;
		options[OptGetVbiOutFormat] = 1;
		options[OptGetSlicedVbiFormat] = 1;
		options[OptGetSlicedVbiOutFormat] = 1;
		options[OptGetSdrFormat] = 1;
		options[OptGetSdrOutFormat] = 1;
		options[OptGetMetaFormat] = 1;
		options[OptGetFBuf] = 1;
		options[OptGetCropCap] = 1;
		options[OptGetOutputCropCap] = 1;
		options[OptGetJpegComp] = 1;
		options[OptGetDvTimings] = 1;
		options[OptGetDvTimingsCap] = 1;
		options[OptGetPriority] = 1;
		options[OptGetSelection] = 1;
		options[OptGetOutputSelection] = 1;
		options[OptListCtrlsMenus] = 1;
		options[OptSilent] = 1;
	}

	/* Information Opts */

	if (!is_subdev && options[OptGetDriverInfo]) {
		printf("Driver Info%s:\n",
				options[OptUseWrapper] ? " (using libv4l2)" : "");
		v4l2_info_capability(vcap);
	}
	if (options[OptGetDriverInfo] && media_fd >= 0)
		mi_media_info_for_fd(media_fd, fd);

	/* Set options */

	common_set(c_fd);
	tuner_set(c_fd);
	io_set(c_fd);
	stds_set(c_fd);
	vidcap_set(c_fd);
	vidout_set(c_fd);
	overlay_set(c_fd);
	vbi_set(c_fd);
	sdr_set(c_fd);
	meta_set(c_fd);
	subdev_set(c_fd);
	selection_set(c_fd);
	misc_set(c_fd);
	edid_set(c_fd);

	/* Get options */

	common_get(c_fd);
	tuner_get(c_fd);
	io_get(c_fd);
	stds_get(c_fd);
	vidcap_get(c_fd);
	vidout_get(c_fd);
	overlay_get(c_fd);
	vbi_get(c_fd);
	sdr_get(c_fd);
	meta_get(c_fd);
	subdev_get(c_fd);
	selection_get(c_fd);
	misc_get(c_fd);
	edid_get(c_fd);

	/* List options */

	common_list(c_fd);
	io_list(c_fd);
	stds_list(c_fd);
	vidcap_list(c_fd);
	vidout_list(c_fd);
	overlay_list(c_fd);
	vbi_list(c_fd);
	sdr_list(c_fd);
	meta_list(c_fd);
	subdev_list(c_fd);
	streaming_list(c_fd, c_out_fd);

	/* Special case: handled last */

	streaming_set(c_fd, c_out_fd);

	if (options[OptWaitForEvent]) {
		struct v4l2_event_subscription sub;
		struct v4l2_event ev;

		memset(&sub, 0, sizeof(sub));
		sub.type = wait_for_event;
		if (wait_for_event == V4L2_EVENT_CTRL)
			sub.id = common_find_ctrl_id(wait_event_id);
		else if (wait_for_event == V4L2_EVENT_SOURCE_CHANGE)
			sub.id = strtoul(wait_event_id, 0L, 0);
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
		else if (poll_for_event == V4L2_EVENT_SOURCE_CHANGE)
			sub.id = strtoul(poll_event_id, 0L, 0);
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
		if (c_fd.querycap(vcap, true) == 0)
			printf("\tDriver name   : %s\n", vcap.driver);
		else
			perror("VIDIOC_QUERYCAP");
	}

	c_fd.close();
	if (out_device)
		c_out_fd.close();
	if (media_fd >= 0)
		close(media_fd);

	// --all sets --silent to avoid ioctl errors to be shown when an ioctl
	// is not implemented by the driver. Which is fine, but we shouldn't
	// return an application error in that specific case.
	exit(options[OptAll] ? 0 : app_result);
}
