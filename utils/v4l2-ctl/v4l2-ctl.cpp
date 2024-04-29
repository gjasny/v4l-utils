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

#include <cctype>
#include <vector>

#include <dirent.h>
#include <getopt.h>
#include <sys/epoll.h>

#include <linux/media.h>

#include "v4l2-ctl.h"

#include <media-info.h>

#ifdef HAVE_SYS_KLOG_H
#include <sys/klog.h>
#endif

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
	{"list-audio-inputs", no_argument, nullptr, OptListAudioInputs},
	{"list-audio-outputs", no_argument, nullptr, OptListAudioOutputs},
	{"all", no_argument, nullptr, OptAll},
	{"device", required_argument, nullptr, OptSetDevice},
	{"out-device", required_argument, nullptr, OptSetOutDevice},
	{"export-device", required_argument, nullptr, OptSetExportDevice},
	{"media-bus-info", required_argument, nullptr, OptMediaBusInfo},
	{"get-fmt-video", no_argument, nullptr, OptGetVideoFormat},
	{"set-fmt-video", required_argument, nullptr, OptSetVideoFormat},
	{"try-fmt-video", required_argument, nullptr, OptTryVideoFormat},
	{"get-fmt-video-out", no_argument, nullptr, OptGetVideoOutFormat},
	{"set-fmt-video-out", required_argument, nullptr, OptSetVideoOutFormat},
	{"try-fmt-video-out", required_argument, nullptr, OptTryVideoOutFormat},
	{"get-routing", no_argument, 0, OptGetRouting},
	{"set-routing", required_argument, 0, OptSetRouting},
	{"try-routing", required_argument, 0, OptTryRouting},
	{"help", no_argument, nullptr, OptHelp},
	{"help-tuner", no_argument, nullptr, OptHelpTuner},
	{"help-io", no_argument, nullptr, OptHelpIO},
	{"help-stds", no_argument, nullptr, OptHelpStds},
	{"help-vidcap", no_argument, nullptr, OptHelpVidCap},
	{"help-vidout", no_argument, nullptr, OptHelpVidOut},
	{"help-overlay", no_argument, nullptr, OptHelpOverlay},
	{"help-vbi", no_argument, nullptr, OptHelpVbi},
	{"help-sdr", no_argument, nullptr, OptHelpSdr},
	{"help-meta", no_argument, nullptr, OptHelpMeta},
	{"help-subdev", no_argument, nullptr, OptHelpSubDev},
	{"help-selection", no_argument, nullptr, OptHelpSelection},
	{"help-misc", no_argument, nullptr, OptHelpMisc},
	{"help-streaming", no_argument, nullptr, OptHelpStreaming},
	{"help-edid", no_argument, nullptr, OptHelpEdid},
	{"help-all", no_argument, nullptr, OptHelpAll},
#ifndef NO_LIBV4L2
	{"wrapper", no_argument, nullptr, OptUseWrapper},
#endif
	{"concise", no_argument, nullptr, OptConcise},
	{"get-output", no_argument, nullptr, OptGetOutput},
	{"set-output", required_argument, nullptr, OptSetOutput},
	{"list-outputs", no_argument, nullptr, OptListOutputs},
	{"list-inputs", no_argument, nullptr, OptListInputs},
	{"get-input", no_argument, nullptr, OptGetInput},
	{"set-input", required_argument, nullptr, OptSetInput},
	{"get-audio-input", no_argument, nullptr, OptGetAudioInput},
	{"set-audio-input", required_argument, nullptr, OptSetAudioInput},
	{"get-audio-output", no_argument, nullptr, OptGetAudioOutput},
	{"set-audio-output", required_argument, nullptr, OptSetAudioOutput},
	{"get-freq", no_argument, nullptr, OptGetFreq},
	{"set-freq", required_argument, nullptr, OptSetFreq},
	{"list-standards", no_argument, nullptr, OptListStandards},
	{"list-formats", optional_argument, nullptr, OptListFormats},
	{"list-formats-ext", optional_argument, nullptr, OptListFormatsExt},
	{"list-fields", no_argument, nullptr, OptListFields},
	{"list-framesizes", required_argument, nullptr, OptListFrameSizes},
	{"list-frameintervals", required_argument, nullptr, OptListFrameIntervals},
	{"list-formats-overlay", no_argument, nullptr, OptListOverlayFormats},
	{"list-formats-sdr", no_argument, nullptr, OptListSdrFormats},
	{"list-formats-sdr-out", no_argument, nullptr, OptListSdrOutFormats},
	{"list-formats-out", optional_argument, nullptr, OptListOutFormats},
	{"list-formats-out-ext", optional_argument, nullptr, OptListOutFormatsExt},
	{"list-formats-meta", optional_argument, nullptr, OptListMetaFormats},
	{"list-formats-meta-out", optional_argument, nullptr, OptListMetaOutFormats},
	{"list-subdev-mbus-codes", optional_argument, nullptr, OptListSubDevMBusCodes},
	{"list-subdev-framesizes", required_argument, nullptr, OptListSubDevFrameSizes},
	{"list-subdev-frameintervals", required_argument, nullptr, OptListSubDevFrameIntervals},
	{"list-fields-out", no_argument, nullptr, OptListOutFields},
	{"clear-clips", no_argument, nullptr, OptClearClips},
	{"clear-bitmap", no_argument, nullptr, OptClearBitmap},
	{"add-clip", required_argument, nullptr, OptAddClip},
	{"add-bitmap", required_argument, nullptr, OptAddBitmap},
	{"find-fb", no_argument, nullptr, OptFindFb},
	{"subset", required_argument, nullptr, OptSubset},
	{"get-standard", no_argument, nullptr, OptGetStandard},
	{"set-standard", required_argument, nullptr, OptSetStandard},
	{"get-detected-standard", no_argument, nullptr, OptQueryStandard},
	{"get-parm", no_argument, nullptr, OptGetParm},
	{"set-parm", required_argument, nullptr, OptSetParm},
	{"get-output-parm", no_argument, nullptr, OptGetOutputParm},
	{"set-output-parm", required_argument, nullptr, OptSetOutputParm},
	{"info", no_argument, nullptr, OptGetDriverInfo},
	{"list-ctrls", no_argument, nullptr, OptListCtrls},
	{"list-ctrls-menus", no_argument, nullptr, OptListCtrlsMenus},
	{"set-ctrl", required_argument, nullptr, OptSetCtrl},
	{"get-ctrl", required_argument, nullptr, OptGetCtrl},
	{"get-tuner", no_argument, nullptr, OptGetTuner},
	{"set-tuner", required_argument, nullptr, OptSetTuner},
	{"list-freq-bands", no_argument, nullptr, OptListFreqBands},
	{"silent", no_argument, nullptr, OptSilent},
	{"verbose", no_argument, nullptr, OptVerbose},
	{"log-status", no_argument, nullptr, OptLogStatus},
	{"get-fmt-overlay", no_argument, nullptr, OptGetOverlayFormat},
	{"set-fmt-overlay", optional_argument, nullptr, OptSetOverlayFormat},
	{"try-fmt-overlay", optional_argument, nullptr, OptTryOverlayFormat},
	{"get-fmt-sliced-vbi", no_argument, nullptr, OptGetSlicedVbiFormat},
	{"set-fmt-sliced-vbi", required_argument, nullptr, OptSetSlicedVbiFormat},
	{"try-fmt-sliced-vbi", required_argument, nullptr, OptTrySlicedVbiFormat},
	{"get-fmt-sliced-vbi-out", no_argument, nullptr, OptGetSlicedVbiOutFormat},
	{"set-fmt-sliced-vbi-out", required_argument, nullptr, OptSetSlicedVbiOutFormat},
	{"try-fmt-sliced-vbi-out", required_argument, nullptr, OptTrySlicedVbiOutFormat},
	{"get-fmt-vbi", no_argument, nullptr, OptGetVbiFormat},
	{"set-fmt-vbi", required_argument, nullptr, OptSetVbiFormat},
	{"try-fmt-vbi", required_argument, nullptr, OptTryVbiFormat},
	{"get-fmt-vbi-out", no_argument, nullptr, OptGetVbiOutFormat},
	{"set-fmt-vbi-out", required_argument, nullptr, OptSetVbiOutFormat},
	{"try-fmt-vbi-out", required_argument, nullptr, OptTryVbiOutFormat},
	{"get-fmt-sdr", no_argument, nullptr, OptGetSdrFormat},
	{"set-fmt-sdr", required_argument, nullptr, OptSetSdrFormat},
	{"try-fmt-sdr", required_argument, nullptr, OptTrySdrFormat},
	{"get-fmt-sdr-out", no_argument, nullptr, OptGetSdrOutFormat},
	{"set-fmt-sdr-out", required_argument, nullptr, OptSetSdrOutFormat},
	{"try-fmt-sdr-out", required_argument, nullptr, OptTrySdrOutFormat},
	{"get-fmt-meta", no_argument, nullptr, OptGetMetaFormat},
	{"set-fmt-meta", required_argument, nullptr, OptSetMetaFormat},
	{"try-fmt-meta", required_argument, nullptr, OptTryMetaFormat},
	{"get-fmt-meta-out", no_argument, nullptr, OptGetMetaOutFormat},
	{"set-fmt-meta-out", required_argument, nullptr, OptSetMetaOutFormat},
	{"try-fmt-meta-out", required_argument, nullptr, OptTryMetaOutFormat},
	{"get-subdev-fmt", optional_argument, nullptr, OptGetSubDevFormat},
	{"set-subdev-fmt", required_argument, nullptr, OptSetSubDevFormat},
	{"try-subdev-fmt", required_argument, nullptr, OptTrySubDevFormat},
	{"get-sliced-vbi-cap", no_argument, nullptr, OptGetSlicedVbiCap},
	{"get-sliced-vbi-out-cap", no_argument, nullptr, OptGetSlicedVbiOutCap},
	{"get-fbuf", no_argument, nullptr, OptGetFBuf},
	{"set-fbuf", required_argument, nullptr, OptSetFBuf},
	{"get-cropcap", no_argument, nullptr, OptGetCropCap},
	{"get-crop", no_argument, nullptr, OptGetCrop},
	{"set-crop", required_argument, nullptr, OptSetCrop},
	{"get-cropcap-output", no_argument, nullptr, OptGetOutputCropCap},
	{"get-crop-output", no_argument, nullptr, OptGetOutputCrop},
	{"set-crop-output", required_argument, nullptr, OptSetOutputCrop},
	{"get-cropcap-overlay", no_argument, nullptr, OptGetOverlayCropCap},
	{"get-crop-overlay", no_argument, nullptr, OptGetOverlayCrop},
	{"set-crop-overlay", required_argument, nullptr, OptSetOverlayCrop},
	{"get-cropcap-output-overlay", no_argument, nullptr, OptGetOutputOverlayCropCap},
	{"get-crop-output-overlay", no_argument, nullptr, OptGetOutputOverlayCrop},
	{"set-crop-output-overlay", required_argument, nullptr, OptSetOutputOverlayCrop},
	{"get-selection", required_argument, nullptr, OptGetSelection},
	{"set-selection", required_argument, nullptr, OptSetSelection},
	{"get-selection-output", required_argument, nullptr, OptGetOutputSelection},
	{"set-selection-output", required_argument, nullptr, OptSetOutputSelection},
	{"get-subdev-selection", required_argument, nullptr, OptGetSubDevSelection},
	{"set-subdev-selection", required_argument, nullptr, OptSetSubDevSelection},
	{"try-subdev-selection", required_argument, nullptr, OptTrySubDevSelection},
	{"get-subdev-fps", optional_argument, nullptr, OptGetSubDevFPS},
	{"set-subdev-fps", required_argument, nullptr, OptSetSubDevFPS},
	{"get-jpeg-comp", no_argument, nullptr, OptGetJpegComp},
	{"set-jpeg-comp", required_argument, nullptr, OptSetJpegComp},
	{"get-modulator", no_argument, nullptr, OptGetModulator},
	{"set-modulator", required_argument, nullptr, OptSetModulator},
	{"get-priority", no_argument, nullptr, OptGetPriority},
	{"set-priority", required_argument, nullptr, OptSetPriority},
	{"wait-for-event", required_argument, nullptr, OptWaitForEvent},
	{"poll-for-event", required_argument, nullptr, OptPollForEvent},
	{"epoll-for-event", required_argument, nullptr, OptEPollForEvent},
	{"overlay", required_argument, nullptr, OptOverlay},
	{"sleep", required_argument, nullptr, OptSleep},
	{"list-devices", no_argument, nullptr, OptListDevices},
	{"list-dv-timings", optional_argument, nullptr, OptListDvTimings},
	{"query-dv-timings", no_argument, nullptr, OptQueryDvTimings},
	{"get-dv-timings", no_argument, nullptr, OptGetDvTimings},
	{"set-dv-bt-timings", required_argument, nullptr, OptSetDvBtTimings},
	{"get-dv-timings-cap", optional_argument, nullptr, OptGetDvTimingsCap},
	{"freq-seek", required_argument, nullptr, OptFreqSeek},
	{"encoder-cmd", required_argument, nullptr, OptEncoderCmd},
	{"try-encoder-cmd", required_argument, nullptr, OptTryEncoderCmd},
	{"decoder-cmd", required_argument, nullptr, OptDecoderCmd},
	{"try-decoder-cmd", required_argument, nullptr, OptTryDecoderCmd},
	{"set-edid", required_argument, nullptr, OptSetEdid},
	{"clear-edid", optional_argument, nullptr, OptClearEdid},
	{"get-edid", optional_argument, nullptr, OptGetEdid},
	{"info-edid", optional_argument, nullptr, OptInfoEdid},
	{"show-edid", required_argument, nullptr, OptShowEdid},
	{"keep-edid-checksums", no_argument, nullptr, OptKeepEdidChecksums},
	{"tuner-index", required_argument, nullptr, OptTunerIndex},
	{"list-buffers", no_argument, nullptr, OptListBuffers},
	{"list-buffers-out", no_argument, nullptr, OptListBuffersOut},
	{"list-buffers-vbi", no_argument, nullptr, OptListBuffersVbi},
	{"list-buffers-sliced-vbi", no_argument, nullptr, OptListBuffersSlicedVbi},
	{"list-buffers-vbi-out", no_argument, nullptr, OptListBuffersVbiOut},
	{"list-buffers-sliced-vbi-out", no_argument, nullptr, OptListBuffersSlicedVbiOut},
	{"list-buffers-sdr", no_argument, nullptr, OptListBuffersSdr},
	{"list-buffers-sdr-out", no_argument, nullptr, OptListBuffersSdrOut},
	{"list-buffers-meta", no_argument, nullptr, OptListBuffersMeta},
	{"list-buffers-meta-out", no_argument, nullptr, OptListBuffersMetaOut},
	{"stream-count", required_argument, nullptr, OptStreamCount},
	{"stream-skip", required_argument, nullptr, OptStreamSkip},
	{"stream-loop", no_argument, nullptr, OptStreamLoop},
	{"stream-sleep", required_argument, nullptr, OptStreamSleep},
	{"stream-poll", no_argument, nullptr, OptStreamPoll},
	{"stream-no-query", no_argument, nullptr, OptStreamNoQuery},
#ifndef NO_STREAM_TO
	{"stream-to", required_argument, nullptr, OptStreamTo},
	{"stream-to-hdr", required_argument, nullptr, OptStreamToHdr},
	{"stream-lossless", no_argument, nullptr, OptStreamLossless},
	{"stream-to-host", required_argument, nullptr, OptStreamToHost},
#endif
	{"stream-buf-caps", no_argument, nullptr, OptStreamBufCaps},
	{"stream-show-delta-now", no_argument, nullptr, OptStreamShowDeltaNow},
	{"stream-mmap", optional_argument, nullptr, OptStreamMmap},
	{"stream-user", optional_argument, nullptr, OptStreamUser},
	{"stream-dmabuf", no_argument, nullptr, OptStreamDmaBuf},
	{"stream-from", required_argument, nullptr, OptStreamFrom},
	{"stream-from-hdr", required_argument, nullptr, OptStreamFromHdr},
	{"stream-from-host", required_argument, nullptr, OptStreamFromHost},
	{"stream-out-pattern", required_argument, nullptr, OptStreamOutPattern},
	{"stream-out-square", no_argument, nullptr, OptStreamOutSquare},
	{"stream-out-border", no_argument, nullptr, OptStreamOutBorder},
	{"stream-out-sav", no_argument, nullptr, OptStreamOutInsertSAV},
	{"stream-out-eav", no_argument, nullptr, OptStreamOutInsertEAV},
	{"stream-out-pixel-aspect", required_argument, nullptr, OptStreamOutPixelAspect},
	{"stream-out-video-aspect", required_argument, nullptr, OptStreamOutVideoAspect},
	{"stream-out-alpha", required_argument, nullptr, OptStreamOutAlphaComponent},
	{"stream-out-alpha-red-only", no_argument, nullptr, OptStreamOutAlphaRedOnly},
	{"stream-out-rgb-lim-range", required_argument, nullptr, OptStreamOutRGBLimitedRange},
	{"stream-out-hor-speed", required_argument, nullptr, OptStreamOutHorSpeed},
	{"stream-out-vert-speed", required_argument, nullptr, OptStreamOutVertSpeed},
	{"stream-out-perc-fill", required_argument, nullptr, OptStreamOutPercFill},
	{"stream-out-buf-caps", no_argument, nullptr, OptStreamOutBufCaps},
	{"stream-out-mmap", optional_argument, nullptr, OptStreamOutMmap},
	{"stream-out-user", optional_argument, nullptr, OptStreamOutUser},
	{"stream-out-dmabuf", no_argument, nullptr, OptStreamOutDmaBuf},
	{"list-patterns", no_argument, nullptr, OptListPatterns},
	{"version", no_argument, nullptr, OptVersion},
	{nullptr, 0, nullptr, 0}
};

static void usage_all()
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
       subdev_usage();
       selection_usage();
       misc_usage();
       streaming_usage();
       edid_usage();
}

static void print_version()
{
#define STR(x) #x
#define STRING(x) STR(x)
	printf("v4l2-ctl %s%s\n", PACKAGE_VERSION, STRING(GIT_COMMIT_CNT));
}

int test_ioctl(int fd, unsigned long cmd, void *arg)
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
	case V4L2_PIX_FMT_AYUV32:
	case V4L2_PIX_FMT_XYUV32:
	case V4L2_PIX_FMT_VUYA32:
	case V4L2_PIX_FMT_VUYX32:
	case V4L2_PIX_FMT_YUVA32:
	case V4L2_PIX_FMT_YUVX32:
	case V4L2_PIX_FMT_YUV410:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_HI240:
	case V4L2_PIX_FMT_NV12_16L16:
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
	struct v4l2_fmtdesc fmt = {};
	std::string s(" (");

	fmt.index = 0;
	fmt.type = type;
	while (test_ioctl(fd, VIDIOC_ENUM_FMT, &fmt) >= 0) {
		if (fmt.pixelformat == pixfmt)
			return s + reinterpret_cast<const char *>(fmt.description) + ")";
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
			auto bitmap = static_cast<unsigned char *>(vfmt.fmt.win.bitmap);
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
				static_cast<double>(vfmt.fmt.vbi.offset) / static_cast<double>(vfmt.fmt.vbi.sampling_rate));
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
	case V4L2_BUF_TYPE_META_OUTPUT:
		printf("\tSample Format   : '%s'%s\n", fcc2s(vfmt.fmt.meta.dataformat).c_str(),
		       printfmtname(fd, vfmt.type, vfmt.fmt.meta.dataformat).c_str());
		printf("\tBuffer Size     : %u\n", vfmt.fmt.meta.buffersize);
		break;
	}
}

static std::string frmtype2s(unsigned type)
{
	static constexpr const char *types[] = {
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
	} else if (frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
		printf("%dx%d - %dx%d",
				frmsize.stepwise.min_width,
				frmsize.stepwise.min_height,
				frmsize.stepwise.max_width,
				frmsize.stepwise.max_height);
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

void print_video_formats(cv4l_fd &fd, __u32 type, unsigned int mbus_code)
{
	cv4l_disable_trace dt(fd);
	struct v4l2_fmtdesc fmt = {};

	if (mbus_code && !(capabilities & V4L2_CAP_IO_MC))
		mbus_code = 0;

	printf("\tType: %s\n\n", buftype2s(type).c_str());
	if (fd.enum_fmt(fmt, true, 0, type, mbus_code))
		return;
	do {
		printf("\t[%d]: '%s' (%s", fmt.index, fcc2s(fmt.pixelformat).c_str(),
		       fmt.description);
		if (fmt.flags) {
			bool is_hsv = fmt.pixelformat == V4L2_PIX_FMT_HSV24 ||
				      fmt.pixelformat == V4L2_PIX_FMT_HSV32;

			printf(", %s", fmtdesc2s(fmt.flags, is_hsv).c_str());
		}
		printf(")\n");
	} while (!fd.enum_fmt(fmt));
}

void print_video_formats_ext(cv4l_fd &fd, __u32 type, unsigned int mbus_code)
{
	cv4l_disable_trace dt(fd);
	struct v4l2_fmtdesc fmt = {};
	struct v4l2_frmsizeenum frmsize;
	struct v4l2_frmivalenum frmival;

	if (mbus_code && !(capabilities & V4L2_CAP_IO_MC))
		mbus_code = 0;

	printf("\tType: %s\n\n", buftype2s(type).c_str());
	if (fd.enum_fmt(fmt, true, 0, type, mbus_code))
		return;
	do {
		printf("\t[%d]: '%s' (%s", fmt.index, fcc2s(fmt.pixelformat).c_str(),
		       fmt.description);
		if (fmt.flags) {
			bool is_hsv = fmt.pixelformat == V4L2_PIX_FMT_HSV24 ||
				      fmt.pixelformat == V4L2_PIX_FMT_HSV32;

			printf(", %s", fmtdesc2s(fmt.flags, is_hsv).c_str());
		}
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
	      __u32 &quantization, __u32 &flags, __u32 *bytesperline,
	      __u32 *sizeimage)
{
	char *value, *subs;
	int fmts = 0;
	unsigned bpl_index = 0;
	unsigned sizeimage_index = 0;
	bool be_pixfmt;

	field = V4L2_FIELD_ANY;
	flags = 0;
	subs = optarg;
	while (*subs != '\0') {
		static constexpr const char *subopts[] = {
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
			"sizeimage",
			nullptr
		};

		switch (parse_subopt(&subs, subopts, &value)) {
		case 0:
			width = strtoul(value, nullptr, 0);
			fmts |= FmtWidth;
			break;
		case 1:
			height = strtoul(value, nullptr, 0);
			fmts |= FmtHeight;
			break;
		case 2:
			be_pixfmt = strlen(value) == 7 && !memcmp(value + 4, "-BE", 3);
			if (be_pixfmt || strlen(value) == 4) {
				pixelformat =
					v4l2_fourcc(value[0], value[1],
						    value[2], value[3]);
				if (be_pixfmt)
					pixelformat |= 1U << 31;
			} else if (isdigit(value[0])) {
				pixelformat = strtol(value, nullptr, 0);
			} else {
				fprintf(stderr, "The pixelformat '%s' is invalid\n", value);
				std::exit(EXIT_FAILURE);
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
			bytesperline[bpl_index] = strtoul(value, nullptr, 0);
			if (bytesperline[bpl_index] > 0xffff) {
				fprintf(stderr, "bytesperline can't be more than 65535\n");
				bytesperline[bpl_index] = 0;
			}
			bpl_index++;
			fmts |= FmtBytesPerLine;
			break;
		case 8:
			if (strtoul(value, nullptr, 0))
				flags |= V4L2_PIX_FMT_FLAG_PREMUL_ALPHA;
			else
				flags &= ~V4L2_PIX_FMT_FLAG_PREMUL_ALPHA;
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
		case 11:
			sizeimage[sizeimage_index] = strtoul(value, nullptr, 0);
			sizeimage_index++;
			fmts |= FmtSizeImage;
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
	printf("Selection %s: %s, Left %d, Top %d, Width %d, Height %d, Flags: %s\n",
			buftype2s(sel.type).c_str(), seltarget2s(sel.target).c_str(),
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


static void print_event(int fd, const struct v4l2_event *ev)
{
	printf("%lld.%06ld: event %u, pending %u: ",
			static_cast<__u64>(ev->timestamp.tv_sec), ev->timestamp.tv_nsec / 1000,
			ev->sequence, ev->pending);
	switch (ev->type) {
	case V4L2_EVENT_VSYNC:
		printf("vsync %s\n", field2s(ev->u.vsync.field).c_str());
		break;
	case V4L2_EVENT_EOS:
		printf("eos\n");
		break;
	case V4L2_EVENT_CTRL:
		common_control_event(fd, ev);
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
		event = strtoul(e, nullptr, 0);
		if (event == V4L2_EVENT_CTRL) {
			fprintf(stderr, "Missing control name for ctrl event, use ctrl=<name>\n");
			misc_usage();
			std::exit(EXIT_FAILURE);
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
		std::exit(EXIT_FAILURE);
	}
	return event;
}

bool valid_pixel_format(int fd, __u32 pixelformat, bool output, bool mplane)
{
	struct v4l2_fmtdesc fmt = {};

	if (output)
		fmt.type = mplane ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE :
			V4L2_BUF_TYPE_VIDEO_OUTPUT;
	else
		fmt.type = mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
			V4L2_BUF_TYPE_VIDEO_CAPTURE;

	while (!ioctl(fd, VIDIOC_ENUM_FMT, &fmt)) {
		if (fmt.pixelformat == pixelformat)
			return true;
		fmt.index++;
	}
	return false;
}

__u32 find_pixel_format(int fd, unsigned index, bool output, bool mplane)
{
	struct v4l2_fmtdesc fmt = {};

	fmt.index = index;
	if (output)
		fmt.type = mplane ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE :
			V4L2_BUF_TYPE_VIDEO_OUTPUT;
	else
		fmt.type = mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
			V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (ioctl(fd, VIDIOC_ENUM_FMT, &fmt))
		return 0;
	return fmt.pixelformat;
}

static int open_media_bus_info(const std::string &bus_info)
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
			std::string devname = std::string("/dev/") + name;

			int fd = open(devname.c_str(), O_RDWR);
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

static const char *make_devname(const char *device, const char *devname,
				      const std::string &media_bus_info)
{
	if (device[0] >= '0' && device[0] <= '9' && strlen(device) <= 3) {
		static char newdev[32];

		sprintf(newdev, "/dev/%s%s", devname, device);
		return newdev;
	}
	if (media_bus_info.empty())
		return device;
	int media_fd = open_media_bus_info(media_bus_info);
	if (media_fd < 0)
		return device;

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

	static char newdev[32];
	sprintf(newdev, "/dev/char/%d:%d",
		ifaces[i].devnode.major, ifaces[i].devnode.minor);
	device = newdev;

err:
	delete [] ents;
	delete [] links;
	delete [] ifaces;
	close(media_fd);
	return device;
}

int main(int argc, char **argv)
{
	struct event {
		int type;
		__u32 ev;
		__u32 id;
		std::string name;
	};
	int i;
	cv4l_fd c_fd;
	cv4l_fd c_out_fd;
	cv4l_fd c_exp_fd;
	int fd = -1;
	int out_fd = -1;
	int exp_fd = -1;
	int media_fd = -1;
	bool is_subdev = false;
	std::string media_bus_info;

	/* command args */
	int ch;
	const char *device = "/dev/video0";	/* -d device */
	const char *out_device = nullptr;
	const char *export_device = nullptr;
	struct v4l2_capability vcap = {};
	struct v4l2_subdev_capability subdevcap = {};
	struct v4l2_subdev_client_capability subdevclientcap = {};
	std::vector<event> events;
	unsigned secs = 0;
	char short_options[26 * 2 * 3 + 1];
	int idx = 0;

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
	while (true) {
		int option_index = 0;
		const char *name;
		event new_ev;

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
			device = make_devname(optarg, "video", media_bus_info);
			break;
		case OptSetOutDevice:
			out_device = make_devname(optarg, "video", media_bus_info);
			break;
		case OptSetExportDevice:
			export_device = make_devname(optarg, "video", media_bus_info);
			break;
		case OptMediaBusInfo:
			media_bus_info = optarg;
			break;
		case OptWaitForEvent:
		case OptPollForEvent:
		case OptEPollForEvent:
			new_ev.type = ch;
			new_ev.ev = parse_event(optarg, &name);
			new_ev.id = 0;
			if (new_ev.ev == 0)
				return 1;
			if (new_ev.ev == V4L2_EVENT_SOURCE_CHANGE)
				new_ev.id = strtoul(name, nullptr, 0);
			else if (new_ev.ev == V4L2_EVENT_CTRL)
				new_ev.name = name;
			events.push_back(new_ev);
			break;
		case OptSleep:
			secs = strtoul(optarg, nullptr, 0);
			break;
		case OptVersion:
			print_version();
			return 0;
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
		std::exit(EXIT_FAILURE);
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
		std::exit(EXIT_FAILURE);
	}
	is_subdev = type == MEDIA_TYPE_SUBDEV;
	if (is_subdev)
		options[OptUseWrapper] = 0;
	c_fd.s_direct(!options[OptUseWrapper]);
	c_out_fd.s_direct(!options[OptUseWrapper]);
	c_exp_fd.s_direct(!options[OptUseWrapper]);

	if (is_subdev)
		fd = c_fd.subdev_open(device);
	else
		fd = c_fd.open(device);

	if (fd < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", device,
			strerror(errno));
		std::exit(EXIT_FAILURE);
	}
	verbose = options[OptVerbose];
	c_fd.s_trace(options[OptSilent] ? 0 : (verbose ? 2 : 1));

	if (!is_subdev && doioctl(fd, VIDIOC_QUERYCAP, &vcap)) {
		fprintf(stderr, "%s: not a v4l2 node\n", device);
		std::exit(EXIT_FAILURE);
	} else if (is_subdev) {
		// This ioctl was introduced in kernel 5.10, so don't
		// exit if this ioctl returns an error.
		doioctl(fd, VIDIOC_SUBDEV_QUERYCAP, &subdevcap);
		subdevclientcap.capabilities = ~0ULL;
		if (doioctl(fd, VIDIOC_SUBDEV_S_CLIENT_CAP, &subdevclientcap))
			subdevclientcap.capabilities = 0ULL;
	}
	if (!is_subdev) {
		capabilities = vcap.capabilities;
		if (capabilities & V4L2_CAP_DEVICE_CAPS)
			capabilities = vcap.device_caps;
	}

	media_fd = mi_get_media_fd(fd, is_subdev ? 0 : (const char *)vcap.bus_info);

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
			std::exit(EXIT_FAILURE);
		}
		c_out_fd.s_trace(options[OptSilent] ? 0 : (verbose ? 2 : 1));
		if (doioctl(out_fd, VIDIOC_QUERYCAP, &vcap)) {
			fprintf(stderr, "%s: not a v4l2 node\n", out_device);
			std::exit(EXIT_FAILURE);
		}
		out_capabilities = vcap.capabilities;
		if (out_capabilities & V4L2_CAP_DEVICE_CAPS)
			out_capabilities = vcap.device_caps;
		out_priv_magic = (out_capabilities & V4L2_CAP_EXT_PIX_FORMAT) ?
				V4L2_PIX_FMT_PRIV_MAGIC : 0;
	}

	if (export_device) {
		exp_fd = c_exp_fd.open(export_device);
		if (exp_fd < 0) {
			fprintf(stderr, "Failed to open %s: %s\n", export_device,
					strerror(errno));
			std::exit(EXIT_FAILURE);
		}
		c_exp_fd.s_trace(options[OptSilent] ? 0 : (verbose ? 2 : 1));
		if (doioctl(exp_fd, VIDIOC_QUERYCAP, &vcap)) {
			fprintf(stderr, "%s: not a v4l2 node\n", export_device);
			std::exit(EXIT_FAILURE);
		}
	}

	common_process_controls(c_fd);

	for (auto &e : events) {
		if (e.ev != V4L2_EVENT_CTRL)
			continue;
		e.id = common_find_ctrl_id(e.name.c_str());
		if (!e.id) {
			fprintf(stderr, "unknown control '%s'\n", e.name.c_str());
			std::exit(EXIT_FAILURE);
		}
	}

	if (options[OptAll]) {
		options[OptGetVideoFormat] = 1;
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
		options[OptGetMetaOutFormat] = 1;
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

	if (options[OptGetDriverInfo]) {
		printf("Driver Info%s:\n",
				options[OptUseWrapper] ? " (using libv4l2)" : "");
		if (is_subdev) {
			v4l2_info_subdev_capability(subdevcap, subdevclientcap);
		} else {
			v4l2_info_capability(vcap);
		}
	}
	if (options[OptGetDriverInfo] && media_fd >= 0)
		mi_media_info_for_fd(media_fd, fd);

	/* Set options */

	common_set(c_fd);
	tuner_set(c_fd);
	io_set(c_fd);
	stds_set(c_fd);
	vidcap_set(c_fd);
	vidout_set(out_device ? c_out_fd : c_fd);
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
	vidout_get(out_device ? c_out_fd : c_fd);
	overlay_get(c_fd);
	vbi_get(c_fd);
	sdr_get(c_fd);
	meta_get(c_fd);
	subdev_get(c_fd);
	selection_get(c_fd);
	misc_get(c_fd);
	edid_get(c_fd);

	/* List options */

	common_list(media_bus_info, c_fd);
	io_list(c_fd);
	stds_list(c_fd);
	vidcap_list(c_fd);
	vidout_list(out_device ? c_out_fd : c_fd);
	overlay_list(c_fd);
	vbi_list(c_fd);
	sdr_list(c_fd);
	meta_list(c_fd);
	subdev_list(c_fd);
	streaming_list(c_fd, c_out_fd);

	/* Special case: handled last */

	streaming_set(c_fd, c_out_fd, c_exp_fd);

	for (const auto &e : events) {
		struct v4l2_event_subscription sub;
		struct v4l2_event ev;

		if (e.type != OptWaitForEvent)
			continue;
		memset(&sub, 0, sizeof(sub));
		sub.type = e.ev;
		sub.id = e.id;
		if (!doioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub))
			if (!doioctl(fd, VIDIOC_DQEVENT, &ev))
				print_event(fd, &ev);
		doioctl(fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub);
	}

	for (const auto &e : events) {
		struct v4l2_event_subscription sub;

		if (e.type == OptWaitForEvent)
			continue;
		memset(&sub, 0, sizeof(sub));
		sub.flags = V4L2_EVENT_SUB_FL_SEND_INITIAL;
		sub.type = e.ev;
		sub.id = e.id;
		doioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
	}

	if (options[OptPollForEvent]) {
		fd_set fds;
		__u32 seq = 0;

		fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
		while (true) {
			struct v4l2_event ev;
			int res;

			FD_ZERO(&fds);
			FD_SET(fd, &fds);
			res = select(fd + 1, nullptr, nullptr, &fds, nullptr);
			if (res <= 0)
				break;
			if (doioctl(fd, VIDIOC_DQEVENT, &ev))
				break;
			print_event(fd, &ev);
			if (ev.sequence > seq)
				printf("\tMissed %d events\n", ev.sequence - seq);
			seq = ev.sequence + 1;
		}
	}

	if (options[OptEPollForEvent]) {
		struct epoll_event epoll_ev;
		int epollfd = -1;
		__u32 seq = 0;

		epollfd = epoll_create1(0);
		epoll_ev.events = EPOLLPRI;
		epoll_ev.data.fd = fd;

		fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
		epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &epoll_ev);
		while (true) {
			struct v4l2_event ev;
			int res;

			res = epoll_wait(epollfd, &epoll_ev, 1, -1);
			if (res <= 0)
				break;
			if (doioctl(fd, VIDIOC_DQEVENT, &ev))
				break;
			print_event(fd, &ev);
			if (ev.sequence > seq)
				printf("\tMissed %d events\n", ev.sequence - seq);
			seq = ev.sequence + 1;
		}
		close(epollfd);
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
	if (export_device)
		c_exp_fd.close();
	if (media_fd >= 0)
		close(media_fd);

	// --all sets --silent to avoid ioctl errors to be shown when an ioctl
	// is not implemented by the driver. Which is fine, but we shouldn't
	// return an application error in that specific case.
	std::exit(options[OptAll] ? 0 : app_result);
}
