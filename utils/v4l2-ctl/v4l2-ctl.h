#ifndef _V4L2_CTL_H
#define _V4L2_CTL_H

#ifdef ANDROID
#include <android-config.h>
#else
#include <config.h>
#endif

#include <string>

#include <linux/videodev2.h>

#ifndef NO_LIBV4L2
#include <libv4l2.h>
#else
#define v4l2_open(file, oflag, ...) (-1)
#define v4l2_close(fd) (-1)
#define v4l2_ioctl(fd, request, ...) (-1)
#define v4l2_mmap(start, length, prot, flags, fd, offset) (MAP_FAILED)
#define v4l2_munmap(_start, length) (-1)
#endif

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

	OptGetSlicedVbiOutFormat = 128,
	OptGetOverlayFormat,
	OptGetVbiFormat,
	OptGetVbiOutFormat,
	OptGetSdrFormat,
	OptGetSdrOutFormat,
	OptGetVideoOutFormat,
	OptSetSlicedVbiOutFormat,
	OptSetOverlayFormat,
	OptSetVbiFormat,
	OptSetVbiOutFormat,
	OptSetSdrFormat,
	OptSetSdrOutFormat,
	OptSetVideoOutFormat,
	OptTryVideoOutFormat,
	OptTrySlicedVbiOutFormat,
	OptTrySlicedVbiFormat,
	OptTryVideoFormat,
	OptTryOverlayFormat,
	OptTryVbiFormat,
	OptTryVbiOutFormat,
	OptTrySdrFormat,
	OptTrySdrOutFormat,
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
	OptStreamCount,
	OptStreamSkip,
	OptStreamLoop,
	OptStreamSleep,
	OptStreamPoll,
	OptStreamTo,
	OptStreamToHost,
	OptStreamMmap,
	OptStreamUser,
	OptStreamDmaBuf,
	OptStreamFrom,
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
	OptHelpSelection,
	OptHelpMisc,
	OptHelpStreaming,
	OptHelpEdid,
	OptHelpAll,
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

typedef struct {
	unsigned flag;
	const char *str;
} flag_def;

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

// v4l2-ctl.cpp
int doioctl_name(int fd, unsigned long int request, void *parm, const char *name);
int test_ioctl(int fd, int cmd, void *arg);
std::string flags2s(unsigned val, const flag_def *def);
int parse_subopt(char **subs, const char * const *subopts, char **value);
std::string std2s(v4l2_std_id std);
std::string buftype2s(int type);
std::string fcc2s(unsigned int val);
std::string fmtdesc2s(unsigned flags);
std::string colorspace2s(int val);
std::string service2s(unsigned service);
std::string field2s(int val);
void print_v4lstd(v4l2_std_id std);
__u32 parse_field(const char *s);
int parse_fmt(char *optarg, __u32 &width, __u32 &height, __u32 &pixelformat,
	      __u32 &field, __u32 &colorspace, __u32 &xfer, __u32 &ycbcr,
	      __u32 &quantization, __u32 &flags, __u32 *bytesperline);
__u32 find_pixel_format(int fd, unsigned index, bool output, bool mplane);
void printfmt(const struct v4l2_format &vfmt);
void print_video_formats(int fd, __u32 type);

#define doioctl(n, r, p) doioctl_name(n, r, p, #r)

// v4l2-ctl-common.cpp
void common_usage(void);
void common_cmd(int ch, char *optarg);
void common_set(int fd);
void common_get(int fd);
void common_list(int fd);
void common_process_controls(int fd);
void common_control_event(const struct v4l2_event *ev);
int common_find_ctrl_id(const char *name);

// v4l2-ctl-tuner.cpp
void tuner_usage(void);
void tuner_cmd(int ch, char *optarg);
void tuner_set(int fd);
void tuner_get(int fd);

// v4l2-ctl-io.cpp
void io_usage(void);
void io_cmd(int ch, char *optarg);
void io_set(int fd);
void io_get(int fd);
void io_list(int fd);

// v4l2-ctl-stds.cpp
void stds_usage(void);
void stds_cmd(int ch, char *optarg);
void stds_set(int fd);
void stds_get(int fd);
void stds_list(int fd);

// v4l2-ctl-vidcap.cpp
void vidcap_usage(void);
void vidcap_cmd(int ch, char *optarg);
void vidcap_set(int fd);
void vidcap_get(int fd);
void vidcap_list(int fd);

// v4l2-ctl-vidout.cpp
void vidout_usage(void);
void vidout_cmd(int ch, char *optarg);
void vidout_set(int fd);
void vidout_get(int fd);
void vidout_list(int fd);

// v4l2-ctl-overlay.cpp
void overlay_usage(void);
void overlay_cmd(int ch, char *optarg);
void overlay_set(int fd);
void overlay_get(int fd);
void overlay_list(int fd);

// v4l2-ctl-vbi.cpp
void vbi_usage(void);
void vbi_cmd(int ch, char *optarg);
void vbi_set(int fd);
void vbi_get(int fd);
void vbi_list(int fd);

// v4l2-ctl-sdr.cpp
void sdr_usage(void);
void sdr_cmd(int ch, char *optarg);
void sdr_set(int fd);
void sdr_get(int fd);
void sdr_list(int fd);

// v4l2-ctl-selection.cpp
void selection_usage(void);
void selection_cmd(int ch, char *optarg);
void selection_set(int fd);
void selection_get(int fd);

// v4l2-ctl-misc.cpp
// This one is also used by the streaming code.
extern struct v4l2_decoder_cmd dec_cmd;
void misc_usage(void);
void misc_cmd(int ch, char *optarg);
void misc_set(int fd);
void misc_get(int fd);

// v4l2-ctl-streaming.cpp
void streaming_usage(void);
void streaming_cmd(int ch, char *optarg);
void streaming_set(int fd, int out_fd);
void streaming_list(int fd, int out_fd);

// v4l2-ctl-edid.cpp
void edid_usage(void);
void edid_cmd(int ch, char *optarg);
void edid_set(int fd);
void edid_get(int fd);

/* v4l2-ctl-modes.cpp */
bool calc_cvt_modeline(int image_width, int image_height,
		       int refresh_rate, int reduced_blanking,
		       bool interlaced, bool reduced_fps,
		       struct v4l2_bt_timings *cvt);

bool calc_gtf_modeline(int image_width, int image_height,
		       int refresh_rate, bool reduced_blanking,
		       bool interlaced, struct v4l2_bt_timings *gtf);
#endif
