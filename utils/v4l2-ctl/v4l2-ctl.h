#ifndef _V4L2_CTL_H
#define _V4L2_CTL_H

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
	OptGetFreq = 'F',
	OptSetFreq = 'f',
	OptHelp = 'h',
	OptGetInput = 'I',
	OptSetInput = 'i',
	OptListCtrls = 'l',
	OptListCtrlsMenus = 'L',
	OptListOutputs = 'N',
	OptListInputs = 'n',
	OptGetOutput = 'O',
	OptSetOutput = 'o',
	OptGetParm = 'P',
	OptSetParm = 'p',
	OptGetStandard = 'S',
	OptSetStandard = 's',
	OptGetTuner = 'T',
	OptSetTuner = 't',
	OptGetVideoFormat = 'V',
	OptSetVideoFormat = 'v',
	OptUseWrapper = 'w',

	OptGetVideoMplaneFormat = 128,
	OptSetVideoMplaneFormat,
	OptGetSlicedVbiOutFormat,
	OptGetOverlayFormat,
	OptGetOutputOverlayFormat,
	OptGetVbiFormat,
	OptGetVbiOutFormat,
	OptGetVideoOutFormat,
	OptGetVideoOutMplaneFormat,
	OptSetSlicedVbiOutFormat,
	OptSetOutputOverlayFormat,
	OptSetOverlayFormat,
	//OptSetVbiFormat, TODO
	//OptSetVbiOutFormat, TODO
	OptSetVideoOutFormat,
	OptSetVideoOutMplaneFormat,
	OptTryVideoOutFormat,
	OptTryVideoOutMplaneFormat,
	OptTrySlicedVbiOutFormat,
	OptTrySlicedVbiFormat,
	OptTryVideoFormat,
	OptTryVideoMplaneFormat,
	OptTryOutputOverlayFormat,
	OptTryOverlayFormat,
	//OptTryVbiFormat, TODO
	//OptTryVbiOutFormat, TODO
	OptAll,
	OptStreamOff,
	OptStreamOn,
	OptListStandards,
	OptListFormats,
	OptListMplaneFormats,
	OptListFormatsExt,
	OptListMplaneFormatsExt,
	OptListFrameSizes,
	OptListFrameIntervals,
	OptListOverlayFormats,
	OptListOutFormats,
	OptListOutMplaneFormats,
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
	OptListDevices,
	OptGetOutputParm,
	OptSetOutputParm,
	OptQueryStandard,
	OptPollForEvent,
	OptWaitForEvent,
	OptGetPriority,
	OptSetPriority,
	OptListDvPresets,
	OptSetDvPreset,
	OptGetDvPreset,
	OptQueryDvPreset,
	OptListDvTimings,
	OptQueryDvTimings,
	OptGetDvTimings,
	OptSetDvBtTimings,
	OptGetDvTimingsCap,
	OptFreqSeek,
	OptEncoderCmd,
	OptTryEncoderCmd,
	OptDecoderCmd,
	OptTryDecoderCmd,
	OptTunerIndex,
	OptHelpTuner,
	OptHelpIO,
	OptHelpStds,
	OptHelpVidCap,
	OptHelpVidOut,
	OptHelpOverlay,
	OptHelpVbi,
	OptHelpSelection,
	OptHelpMisc,
	OptHelpAll,
	OptLast = 256
};

extern char options[OptLast];
extern unsigned capabilities;

typedef struct {
	unsigned flag;
	const char *str;
} flag_def;

// v4l2-ctl.cpp
int doioctl_name(int fd, unsigned long int request, void *parm, const char *name);
int test_ioctl(int fd, int cmd, void *arg);
std::string flags2s(unsigned val, const flag_def *def);
int parse_subopt(char **subs, const char * const *subopts, char **value);
std::string std2s(v4l2_std_id std);

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


#endif
