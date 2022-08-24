#ifndef _V4L2_CTL_H
#define _V4L2_CTL_H

#ifdef ANDROID
#include <android-config.h>
#else
#include <config.h>
#endif

#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <v4l-getsubopt.h>

#include <v4l2-info.h>

#ifndef NO_LIBV4L2
#include <libv4l2.h>
#else
#define v4l2_open(file, oflag, ...) (-1)
#define v4l2_close(fd) (-1)
#define v4l2_ioctl(fd, request, ...) (-1)
#define v4l2_mmap(start, length, prot, flags, fd, offset) (MAP_FAILED)
#define v4l2_munmap(_start, length) (-1)
#endif

#include "cv4l-helpers.h"

class cv4l_disable_trace {
public:
	cv4l_disable_trace(cv4l_fd &fd) : _fd(fd)
	{
		old_trace = _fd.g_trace();
		_fd.s_trace(0);
	}
	~cv4l_disable_trace()
	{
		_fd.s_trace(old_trace);
	}
private:
	cv4l_fd &_fd;
	unsigned int old_trace;
};

/* Available options.

   Please keep the first part (options < 128) in alphabetical order.
   That makes it easier to see which short options are still free.

   In general the lower case is used to set something and the upper
   case is used to retrieve a setting. */
enum Option {
	OptGetSlicedVbiFormat = 'B',
	OptSetSlicedVbiFormat = 'b',
	OptGetCtrl = 'C',
	OptSetCtrl = 'c',
	OptSetDevice = 'd',
	OptGetDriverInfo = 'D',
	OptSetOutDevice = 'e',
	OptSetExportDevice = 'E',
	OptGetFreq = 'F',
	OptSetFreq = 'f',
	OptHelp = 'h',
	OptGetInput = 'I',
	OptSetInput = 'i',
	OptConcise = 'k',
	OptListCtrls = 'l',
	OptListCtrlsMenus = 'L',
	OptListOutputs = 'N',
	OptListInputs = 'n',
	OptGetOutput = 'O',
	OptSetOutput = 'o',
	OptGetParm = 'P',
	OptSetParm = 'p',
	OptSubset = 'r',
	OptGetStandard = 'S',
	OptSetStandard = 's',
	OptGetTuner = 'T',
	OptSetTuner = 't',
	OptGetVideoFormat = 'V',
	OptSetVideoFormat = 'v',
	OptUseWrapper = 'w',
	OptGetVideoOutFormat = 'X',
	OptSetVideoOutFormat = 'x',
	OptMediaBusInfo = 'z',

	OptGetSlicedVbiOutFormat = 128,
	OptGetOverlayFormat,
	OptGetVbiFormat,
	OptGetVbiOutFormat,
	OptGetSdrFormat,
	OptGetSdrOutFormat,
	OptGetMetaFormat,
	OptGetMetaOutFormat,
	OptGetSubDevFormat,
	OptSetSlicedVbiOutFormat,
	OptSetOverlayFormat,
	OptSetVbiFormat,
	OptSetVbiOutFormat,
	OptSetSdrFormat,
	OptSetSdrOutFormat,
	OptSetMetaFormat,
	OptSetMetaOutFormat,
	OptSetSubDevFormat,
	OptTryVideoOutFormat,
	OptTrySlicedVbiOutFormat,
	OptTrySlicedVbiFormat,
	OptTryVideoFormat,
	OptTryOverlayFormat,
	OptTryVbiFormat,
	OptTryVbiOutFormat,
	OptTrySdrFormat,
	OptTrySdrOutFormat,
	OptTryMetaFormat,
	OptTryMetaOutFormat,
	OptTrySubDevFormat,
	OptAll,
	OptListStandards,
	OptListFormats,
	OptListFormatsExt,
	OptListFields,
	OptListFrameSizes,
	OptListFrameIntervals,
	OptListOverlayFormats,
	OptListSdrFormats,
	OptListSdrOutFormats,
	OptListOutFormats,
	OptListOutFormatsExt,
	OptListMetaFormats,
	OptListMetaOutFormats,
	OptListSubDevMBusCodes,
	OptListSubDevFrameSizes,
	OptListSubDevFrameIntervals,
	OptListOutFields,
	OptClearClips,
	OptClearBitmap,
	OptAddClip,
	OptAddBitmap,
	OptFindFb,
	OptLogStatus,
	OptVerbose,
	OptSilent,
	OptGetSlicedVbiCap,
	OptGetSlicedVbiOutCap,
	OptGetFBuf,
	OptSetFBuf,
	OptGetCrop,
	OptSetCrop,
	OptGetOutputCrop,
	OptSetOutputCrop,
	OptGetOverlayCrop,
	OptSetOverlayCrop,
	OptGetOutputOverlayCrop,
	OptSetOutputOverlayCrop,
	OptGetSelection,
	OptSetSelection,
	OptGetOutputSelection,
	OptSetOutputSelection,
	OptGetSubDevSelection,
	OptSetSubDevSelection,
	OptTrySubDevSelection,
	OptGetSubDevFPS,
	OptSetSubDevFPS,
	OptGetAudioInput,
	OptSetAudioInput,
	OptGetAudioOutput,
	OptSetAudioOutput,
	OptListAudioOutputs,
	OptListAudioInputs,
	OptGetCropCap,
	OptGetOutputCropCap,
	OptGetOverlayCropCap,
	OptGetOutputOverlayCropCap,
	OptOverlay,
	OptSleep,
	OptGetJpegComp,
	OptSetJpegComp,
	OptGetModulator,
	OptSetModulator,
	OptListFreqBands,
	OptListDevices,
	OptGetOutputParm,
	OptSetOutputParm,
	OptQueryStandard,
	OptPollForEvent,
	OptEPollForEvent,
	OptWaitForEvent,
	OptGetPriority,
	OptSetPriority,
	OptListDvTimings,
	OptQueryDvTimings,
	OptGetDvTimings,
	OptSetDvBtTimings,
	OptGetDvTimingsCap,
	OptSetEdid,
	OptClearEdid,
	OptGetEdid,
	OptInfoEdid,
	OptShowEdid,
	OptFixEdidChecksums,
	OptFreqSeek,
	OptEncoderCmd,
	OptTryEncoderCmd,
	OptDecoderCmd,
	OptTryDecoderCmd,
	OptTunerIndex,
	OptListBuffers,
	OptListBuffersOut,
	OptListBuffersVbi,
	OptListBuffersSlicedVbi,
	OptListBuffersVbiOut,
	OptListBuffersSlicedVbiOut,
	OptListBuffersSdr,
	OptListBuffersSdrOut,
	OptListBuffersMeta,
	OptListBuffersMetaOut,
	OptStreamCount,
	OptStreamSkip,
	OptStreamLoop,
	OptStreamSleep,
	OptStreamPoll,
	OptStreamNoQuery,
	OptStreamTo,
	OptStreamToHdr,
	OptStreamToHost,
	OptStreamLossless,
	OptStreamShowDeltaNow,
	OptStreamBufCaps,
	OptStreamMmap,
	OptStreamUser,
	OptStreamDmaBuf,
	OptStreamFrom,
	OptStreamFromHdr,
	OptStreamFromHost,
	OptStreamOutPattern,
	OptStreamOutSquare,
	OptStreamOutBorder,
	OptStreamOutInsertSAV,
	OptStreamOutInsertEAV,
	OptStreamOutHorSpeed,
	OptStreamOutVertSpeed,
	OptStreamOutPercFill,
	OptStreamOutAlphaComponent,
	OptStreamOutAlphaRedOnly,
	OptStreamOutRGBLimitedRange,
	OptStreamOutPixelAspect,
	OptStreamOutVideoAspect,
	OptStreamOutBufCaps,
	OptStreamOutMmap,
	OptStreamOutUser,
	OptStreamOutDmaBuf,
	OptListPatterns,
	OptHelpTuner,
	OptHelpIO,
	OptHelpStds,
	OptHelpVidCap,
	OptHelpVidOut,
	OptHelpOverlay,
	OptHelpVbi,
	OptHelpSdr,
	OptHelpMeta,
	OptHelpSubDev,
	OptHelpSelection,
	OptHelpMisc,
	OptHelpStreaming,
	OptHelpEdid,
	OptHelpAll,
	OptVersion,
	OptLast = 512
};

extern char options[OptLast];
extern unsigned capabilities;
extern unsigned out_capabilities;
extern unsigned priv_magic;
extern unsigned out_priv_magic;
extern bool is_multiplanar;
extern __u32 vidcap_buftype;
extern __u32 vidout_buftype;
extern int verbose;

/* fmts specified */
#define FmtWidth		(1L<<0)
#define FmtHeight		(1L<<1)
#define FmtChromaKey		(1L<<2)
#define FmtGlobalAlpha		(1L<<3)
#define FmtPixelFormat		(1L<<4)
#define FmtLeft			(1L<<5)
#define FmtTop			(1L<<6)
#define FmtField		(1L<<7)
#define FmtColorspace		(1L<<8)
#define FmtYCbCr		(1L<<9)
#define FmtQuantization		(1L<<10)
#define FmtFlags		(1L<<11)
#define FmtBytesPerLine		(1L<<12)
#define FmtXferFunc		(1L<<13)
#define FmtSizeImage		(1L<<14)

// v4l2-ctl.cpp
int doioctl_name(int fd, unsigned long int request, void *parm, const char *name);
int test_ioctl(int fd, unsigned long cmd, void *arg);
int parse_subopt(char **subs, const char * const *subopts, char **value);
__u32 parse_field(const char *s);
__u32 parse_colorspace(const char *s);
__u32 parse_xfer_func(const char *s);
__u32 parse_ycbcr(const char *s);
__u32 parse_hsv(const char *s);
__u32 parse_quantization(const char *s);
int parse_fmt(char *optarg, __u32 &width, __u32 &height, __u32 &pixelformat,
	      __u32 &field, __u32 &colorspace, __u32 &xfer, __u32 &ycbcr,
	      __u32 &quantization, __u32 &flags, __u32 *bytesperline,
	      __u32 *sizeimage);
int parse_selection_target(const char *s, unsigned int &target);
int parse_selection_flags(const char *s);
void print_selection(const struct v4l2_selection &sel);
__u32 find_pixel_format(int fd, unsigned index, bool output, bool mplane);
bool valid_pixel_format(int fd, __u32 pixelformat, bool output, bool mplane);
void print_frmsize(const struct v4l2_frmsizeenum &frmsize, const char *prefix);
void print_frmival(const struct v4l2_frmivalenum &frmival, const char *prefix);
void printfmt(int fd, const struct v4l2_format &vfmt);
void print_video_formats(cv4l_fd &fd, __u32 type, unsigned int mbus_code);
void print_video_formats_ext(cv4l_fd &fd, __u32 type, unsigned int mbus_code);

static inline bool subscribe_event(cv4l_fd &fd, __u32 type)
{
	struct v4l2_event_subscription sub;

	memset(&sub, 0, sizeof(sub));
	sub.type = type;

	cv4l_disable_trace dt(fd);
	return !fd.subscribe_event(sub);
}

#define doioctl(n, r, p) doioctl_name(n, r, p, #r)

// v4l2-ctl-common.cpp
void common_usage(void);
void common_cmd(const std::string &media_bus_info, int ch, char *optarg);
void common_set(cv4l_fd &fd);
void common_get(cv4l_fd &fd);
void common_list(cv4l_fd &fd);
void common_process_controls(cv4l_fd &fd);
void common_control_event(int fd, const struct v4l2_event *ev);
int common_find_ctrl_id(const char *name);

// v4l2-ctl-tuner.cpp
void tuner_usage(void);
void tuner_cmd(int ch, char *optarg);
void tuner_set(cv4l_fd &fd);
void tuner_get(cv4l_fd &fd);

// v4l2-ctl-io.cpp
void io_usage(void);
void io_cmd(int ch, char *optarg);
void io_set(cv4l_fd &fd);
void io_get(cv4l_fd &fd);
void io_list(cv4l_fd &fd);

// v4l2-ctl-stds.cpp
void stds_usage(void);
void stds_cmd(int ch, char *optarg);
void stds_set(cv4l_fd &fd);
void stds_get(cv4l_fd &fd);
void stds_list(cv4l_fd &fd);

// v4l2-ctl-vidcap.cpp
void vidcap_usage(void);
void vidcap_cmd(int ch, char *optarg);
int vidcap_get_and_update_fmt(cv4l_fd &_fd, struct v4l2_format &vfmt);
void vidcap_set(cv4l_fd &fd);
void vidcap_get(cv4l_fd &fd);
void vidcap_list(cv4l_fd &fd);
void print_touch_buffer(FILE *f, cv4l_buffer &buf, cv4l_fmt &fmt, cv4l_queue &q);

// v4l2-ctl-vidout.cpp
void vidout_usage(void);
void vidout_cmd(int ch, char *optarg);
void vidout_set(cv4l_fd &fd);
void vidout_get(cv4l_fd &fd);
void vidout_list(cv4l_fd &fd);

// v4l2-ctl-overlay.cpp
void overlay_usage(void);
void overlay_cmd(int ch, char *optarg);
void overlay_set(cv4l_fd &fd);
void overlay_get(cv4l_fd &fd);
void overlay_list(cv4l_fd &fd);

// v4l2-ctl-vbi.cpp
void vbi_usage(void);
void vbi_cmd(int ch, char *optarg);
void vbi_set(cv4l_fd &fd);
void vbi_get(cv4l_fd &fd);
void vbi_list(cv4l_fd &fd);

// v4l2-ctl-sdr.cpp
void sdr_usage(void);
void sdr_cmd(int ch, char *optarg);
void sdr_set(cv4l_fd &fd);
void sdr_get(cv4l_fd &fd);
void sdr_list(cv4l_fd &fd);

// v4l2-ctl-meta.cpp
void meta_usage(void);
void meta_cmd(int ch, char *optarg);
void meta_set(cv4l_fd &fd);
void meta_get(cv4l_fd &fd);
void meta_list(cv4l_fd &fd);
void print_meta_buffer(FILE *f, cv4l_buffer &buf, cv4l_fmt &fmt, cv4l_queue &q);
void meta_fillbuffer(cv4l_buffer &buf, cv4l_fmt &fmt, cv4l_queue &q);

// v4l2-ctl-subdev.cpp
void subdev_usage(void);
void subdev_cmd(int ch, char *optarg);
void subdev_set(cv4l_fd &fd);
void subdev_get(cv4l_fd &fd);
void subdev_list(cv4l_fd &fd);

// v4l2-ctl-selection.cpp
void selection_usage(void);
void selection_cmd(int ch, char *optarg);
void selection_set(cv4l_fd &fd);
void selection_get(cv4l_fd &fd);

// v4l2-ctl-misc.cpp
// This one is also used by the streaming code.
extern struct v4l2_decoder_cmd dec_cmd;
void misc_usage(void);
void misc_cmd(int ch, char *optarg);
void misc_set(cv4l_fd &fd);
void misc_get(cv4l_fd &fd);

// v4l2-ctl-streaming.cpp
void streaming_usage(void);
void streaming_cmd(int ch, char *optarg);
void streaming_set(cv4l_fd &fd, cv4l_fd &out_fd, cv4l_fd &exp_fd);
void streaming_list(cv4l_fd &fd, cv4l_fd &out_fd);

// v4l2-ctl-edid.cpp
void edid_usage(void);
void edid_cmd(int ch, char *optarg);
void edid_set(cv4l_fd &fd);
void edid_get(cv4l_fd &fd);

/* v4l2-ctl-modes.cpp */
bool calc_cvt_modeline(int image_width, int image_height,
		       int refresh_rate, int reduced_blanking,
		       bool interlaced, bool reduced_fps,
		       struct v4l2_bt_timings *cvt);

bool calc_gtf_modeline(int image_width, int image_height,
		       int refresh_rate, bool reduced_blanking,
		       bool interlaced, struct v4l2_bt_timings *gtf);
#endif
