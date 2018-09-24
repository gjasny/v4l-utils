// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Copyright 2018 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/sysmacros.h>
#include <dirent.h>

#include <v4l2-info.h>

#include <fstream>

static std::string num2s(unsigned num, bool is_hex = true)
{
	char buf[16];

	if (is_hex)
		sprintf(buf, "%08x", num);
	else
		sprintf(buf, "%u", num);
	return buf;
}

typedef struct {
	unsigned flag;
	const char *str;
} flag_def;

static std::string flags2s(unsigned val, const flag_def *def)
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
	if (cap & V4L2_CAP_SDR_CAPTURE)
		s += "\t\tSDR Capture\n";
	if (cap & V4L2_CAP_SDR_OUTPUT)
		s += "\t\tSDR Output\n";
	if (cap & V4L2_CAP_META_CAPTURE)
		s += "\t\tMetadata Capture\n";
	if (cap & V4L2_CAP_TUNER)
		s += "\t\tTuner\n";
	if (cap & V4L2_CAP_TOUCH)
		s += "\t\tTouch Device\n";
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

void v4l2_info_capability(const v4l2_capability &vcap)
{
	printf("\tDriver name      : %s\n", vcap.driver);
	printf("\tCard type        : %s\n", vcap.card);
	printf("\tBus info         : %s\n", vcap.bus_info);
	printf("\tDriver version   : %d.%d.%d\n",
	       vcap.version >> 16,
	       (vcap.version >> 8) & 0xff,
	       vcap.version & 0xff);
	printf("\tCapabilities     : 0x%08x\n", vcap.capabilities);
	printf("%s", cap2s(vcap.capabilities).c_str());
	if (vcap.capabilities & V4L2_CAP_DEVICE_CAPS) {
		printf("\tDevice Caps      : 0x%08x\n", vcap.device_caps);
		printf("%s", cap2s(vcap.device_caps).c_str());
	}
}

std::string fcc2s(__u32 val)
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

std::string pixfmt2s(__u32 format)
{
	switch (format) {
#include "v4l2-pix-formats.h"
	default:
		return std::string("Unknown (") + num2s(format) + ")";
	}
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
	case V4L2_BUF_TYPE_META_CAPTURE:
		return "Metadata Capture";
	case V4L2_BUF_TYPE_PRIVATE:
		return "Private";
	default:
		return std::string("Unknown (") + num2s(type) + ")";
	}
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
	case V4L2_COLORSPACE_DEFAULT:
		return "Default";
	case V4L2_COLORSPACE_SMPTE170M:
		return "SMPTE 170M";
	case V4L2_COLORSPACE_SMPTE240M:
		return "SMPTE 240M";
	case V4L2_COLORSPACE_REC709:
		return "Rec. 709";
	case V4L2_COLORSPACE_BT878:
		return "Broken Bt878";
	case V4L2_COLORSPACE_470_SYSTEM_M:
		return "470 System M";
	case V4L2_COLORSPACE_470_SYSTEM_BG:
		return "470 System BG";
	case V4L2_COLORSPACE_JPEG:
		return "JPEG";
	case V4L2_COLORSPACE_SRGB:
		return "sRGB";
	case V4L2_COLORSPACE_OPRGB:
		return "opRGB";
	case V4L2_COLORSPACE_DCI_P3:
		return "DCI-P3";
	case V4L2_COLORSPACE_BT2020:
		return "BT.2020";
	case V4L2_COLORSPACE_RAW:
		return "Raw";
	default:
		return "Unknown (" + num2s(val) + ")";
	}
}

std::string xfer_func2s(int val)
{
	switch (val) {
	case V4L2_XFER_FUNC_DEFAULT:
		return "Default";
	case V4L2_XFER_FUNC_709:
		return "Rec. 709";
	case V4L2_XFER_FUNC_SRGB:
		return "sRGB";
	case V4L2_XFER_FUNC_OPRGB:
		return "opRGB";
	case V4L2_XFER_FUNC_DCI_P3:
		return "DCI-P3";
	case V4L2_XFER_FUNC_SMPTE2084:
		return "SMPTE 2084";
	case V4L2_XFER_FUNC_SMPTE240M:
		return "SMPTE 240M";
	case V4L2_XFER_FUNC_NONE:
		return "None";
	default:
		return "Unknown (" + num2s(val) + ")";
	}
}

std::string ycbcr_enc2s(int val)
{
	switch (val) {
	case V4L2_YCBCR_ENC_DEFAULT:
		return "Default";
	case V4L2_YCBCR_ENC_601:
		return "ITU-R 601";
	case V4L2_YCBCR_ENC_709:
		return "Rec. 709";
	case V4L2_YCBCR_ENC_XV601:
		return "xvYCC 601";
	case V4L2_YCBCR_ENC_XV709:
		return "xvYCC 709";
	case V4L2_YCBCR_ENC_BT2020:
		return "BT.2020";
	case V4L2_YCBCR_ENC_BT2020_CONST_LUM:
		return "BT.2020 Constant Luminance";
	case V4L2_YCBCR_ENC_SMPTE240M:
		return "SMPTE 240M";
	case V4L2_HSV_ENC_180:
		return "HSV with Hue 0-179";
	case V4L2_HSV_ENC_256:
		return "HSV with Hue 0-255";
	default:
		return "Unknown (" + num2s(val) + ")";
	}
}

std::string quantization2s(int val)
{
	switch (val) {
	case V4L2_QUANTIZATION_DEFAULT:
		return "Default";
	case V4L2_QUANTIZATION_FULL_RANGE:
		return "Full Range";
	case V4L2_QUANTIZATION_LIM_RANGE:
		return "Limited Range";
	default:
		return "Unknown (" + num2s(val) + ")";
	}
}

static const flag_def pixflags_def[] = {
	{ V4L2_PIX_FMT_FLAG_PREMUL_ALPHA,  "premultiplied-alpha" },
	{ 0, NULL }
};

std::string pixflags2s(unsigned flags)
{
	return flags2s(flags, pixflags_def);
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

static const flag_def fmtdesc_def[] = {
	{ V4L2_FMT_FLAG_COMPRESSED, "compressed" },
	{ V4L2_FMT_FLAG_EMULATED, "emulated" },
	{ 0, NULL }
};

std::string fmtdesc2s(unsigned flags)
{
	return flags2s(flags, fmtdesc_def);
}

static const flag_def selection_targets_def[] = {
	{ V4L2_SEL_TGT_CROP_ACTIVE, "crop" },
	{ V4L2_SEL_TGT_CROP_DEFAULT, "crop_default" },
	{ V4L2_SEL_TGT_CROP_BOUNDS, "crop_bounds" },
	{ V4L2_SEL_TGT_COMPOSE_ACTIVE, "compose" },
	{ V4L2_SEL_TGT_COMPOSE_DEFAULT, "compose_default" },
	{ V4L2_SEL_TGT_COMPOSE_BOUNDS, "compose_bounds" },
	{ V4L2_SEL_TGT_COMPOSE_PADDED, "compose_padded" },
	{ V4L2_SEL_TGT_NATIVE_SIZE, "native_size" },
	{ 0, NULL }
};

bool valid_seltarget_at_idx(unsigned i)
{
	return i < sizeof(selection_targets_def) / sizeof(selection_targets_def[0]) - 1;
}

unsigned seltarget_at_idx(unsigned i)
{
	if (valid_seltarget_at_idx(i))
		return selection_targets_def[i].flag;
	return 0;
}

std::string seltarget2s(__u32 target)
{
	int i = 0;

	while (selection_targets_def[i].str != NULL) {
		if (selection_targets_def[i].flag == target)
			return selection_targets_def[i].str;
		i++;
	}
	return std::string("Unknown (") + num2s(target) + ")";
}

const flag_def selection_flags_def[] = {
	{ V4L2_SEL_FLAG_GE, "ge" },
	{ V4L2_SEL_FLAG_LE, "le" },
	{ V4L2_SEL_FLAG_KEEP_CONFIG, "keep-config" },
	{ 0, NULL }
};

std::string selflags2s(__u32 flags)
{
	return flags2s(flags, selection_flags_def);
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

std::string std2s(v4l2_std_id std, const char *sep)
{
	std::string s;

	if (std & 0xfff) {
		s += partstd2s("PAL", std_pal, std);
	}
	if (std & 0xf000) {
		if (s.length()) s += sep;
		s += partstd2s("NTSC", std_ntsc, std >> 12);
	}
	if (std & 0xff0000) {
		if (s.length()) s += sep;
		s += partstd2s("SECAM", std_secam, std >> 16);
	}
	if (std & 0xf000000) {
		if (s.length()) s += sep;
		s += partstd2s("ATSC", std_atsc, std >> 24);
	}
	return s;
}

std::string ctrlflags2s(__u32 flags)
{
	static const flag_def def[] = {
		{ V4L2_CTRL_FLAG_GRABBED,    "grabbed" },
		{ V4L2_CTRL_FLAG_DISABLED,   "disabled" },
		{ V4L2_CTRL_FLAG_READ_ONLY,  "read-only" },
		{ V4L2_CTRL_FLAG_UPDATE,     "update" },
		{ V4L2_CTRL_FLAG_INACTIVE,   "inactive" },
		{ V4L2_CTRL_FLAG_SLIDER,     "slider" },
		{ V4L2_CTRL_FLAG_WRITE_ONLY, "write-only" },
		{ V4L2_CTRL_FLAG_VOLATILE,   "volatile" },
		{ V4L2_CTRL_FLAG_HAS_PAYLOAD,"has-payload" },
		{ V4L2_CTRL_FLAG_EXECUTE_ON_WRITE, "execute-on-write" },
		{ V4L2_CTRL_FLAG_MODIFY_LAYOUT, "modify-layout" },
		{ 0, NULL }
	};
	return flags2s(flags, def);
}

static const flag_def in_status_def[] = {
	{ V4L2_IN_ST_NO_POWER,    "no power" },
	{ V4L2_IN_ST_NO_SIGNAL,   "no signal" },
	{ V4L2_IN_ST_NO_COLOR,    "no color" },
	{ V4L2_IN_ST_HFLIP,       "hflip" },
	{ V4L2_IN_ST_VFLIP,       "vflip" },
	{ V4L2_IN_ST_NO_H_LOCK,   "no hsync lock" },
	{ V4L2_IN_ST_NO_V_LOCK,   "no vsync lock" },
	{ V4L2_IN_ST_NO_STD_LOCK, "no standard format lock" },
	{ V4L2_IN_ST_COLOR_KILL,  "color kill" },
	{ V4L2_IN_ST_NO_SYNC,     "no sync lock" },
	{ V4L2_IN_ST_NO_EQU,      "no equalizer lock" },
	{ V4L2_IN_ST_NO_CARRIER,  "no carrier" },
	{ V4L2_IN_ST_MACROVISION, "macrovision" },
	{ V4L2_IN_ST_NO_ACCESS,   "no conditional access" },
	{ V4L2_IN_ST_VTR,         "VTR time constant" },
	{ 0, NULL }
};

std::string in_status2s(__u32 status)
{
	return status ? flags2s(status, in_status_def) : "ok";
}

static const flag_def input_cap_def[] = {
	{ V4L2_IN_CAP_DV_TIMINGS, "DV timings" },
	{ V4L2_IN_CAP_STD, "SDTV standards" },
	{ V4L2_IN_CAP_NATIVE_SIZE, "Native Size" },
	{ 0, NULL }
};

std::string input_cap2s(__u32 capabilities)
{
	return capabilities ? flags2s(capabilities, input_cap_def) : "not defined";
}

static const flag_def output_cap_def[] = {
	{ V4L2_OUT_CAP_DV_TIMINGS, "DV timings" },
	{ V4L2_OUT_CAP_STD, "SDTV standards" },
	{ V4L2_OUT_CAP_NATIVE_SIZE, "Native Size" },
	{ 0, NULL }
};

std::string output_cap2s(__u32 capabilities)
{
	return capabilities ? flags2s(capabilities, output_cap_def) : "not defined";
}

std::string fbufcap2s(unsigned cap)
{
	std::string s;

	if (cap & V4L2_FBUF_CAP_EXTERNOVERLAY)
		s += "\t\t\tExtern Overlay\n";
	if (cap & V4L2_FBUF_CAP_CHROMAKEY)
		s += "\t\t\tChromakey\n";
	if (cap & V4L2_FBUF_CAP_SRC_CHROMAKEY)
		s += "\t\t\tSource Chromakey\n";
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

std::string fbufflags2s(unsigned fl)
{
	std::string s;

	if (fl & V4L2_FBUF_FLAG_PRIMARY)
		s += "\t\t\tPrimary Graphics Surface\n";
	if (fl & V4L2_FBUF_FLAG_OVERLAY)
		s += "\t\t\tOverlay Matches Capture/Output Size\n";
	if (fl & V4L2_FBUF_FLAG_CHROMAKEY)
		s += "\t\t\tChromakey\n";
	if (fl & V4L2_FBUF_FLAG_SRC_CHROMAKEY)
		s += "\t\t\tSource Chromakey\n";
	if (fl & V4L2_FBUF_FLAG_GLOBAL_ALPHA)
		s += "\t\t\tGlobal Alpha\n";
	if (fl & V4L2_FBUF_FLAG_LOCAL_ALPHA)
		s += "\t\t\tLocal Alpha\n";
	if (fl & V4L2_FBUF_FLAG_LOCAL_INV_ALPHA)
		s += "\t\t\tLocal Inverted Alpha\n";
	if (s.empty()) s += "\t\t\t\n";
	return s;
}

static const flag_def dv_standards_def[] = {
	{ V4L2_DV_BT_STD_CEA861, "CTA-861" },
	{ V4L2_DV_BT_STD_DMT, "DMT" },
	{ V4L2_DV_BT_STD_CVT, "CVT" },
	{ V4L2_DV_BT_STD_GTF, "GTF" },
	{ V4L2_DV_BT_STD_SDI, "SDI" },
	{ 0, NULL }
};

std::string dv_standards2s(__u32 flags)
{
	return flags2s(flags, dv_standards_def);
}

std::string dvflags2s(unsigned vsync, int val)
{
	std::string s;

	if (val & V4L2_DV_FL_REDUCED_BLANKING)
		s += vsync == 8 ?
			"reduced blanking v2, " :
			"reduced blanking, ";
	if (val & V4L2_DV_FL_CAN_REDUCE_FPS)
		s += "framerate can be reduced by 1/1.001, ";
	if (val & V4L2_DV_FL_REDUCED_FPS)
		s += "framerate is reduced by 1/1.001, ";
	if (val & V4L2_DV_FL_CAN_DETECT_REDUCED_FPS)
		s += "can detect reduced framerates, ";
	if (val & V4L2_DV_FL_HALF_LINE)
		s += "half-line, ";
	if (val & V4L2_DV_FL_IS_CE_VIDEO)
		s += "CE-video, ";
	if (val & V4L2_DV_FL_FIRST_FIELD_EXTRA_LINE)
		s += "first field has extra line, ";
	if (val & V4L2_DV_FL_HAS_PICTURE_ASPECT)
		s += "has picture aspect, ";
	if (val & V4L2_DV_FL_HAS_CEA861_VIC)
		s += "has CTA-861 VIC, ";
	if (val & V4L2_DV_FL_HAS_HDMI_VIC)
		s += "has HDMI VIC, ";
	if (s.length())
		return s.erase(s.length() - 2, 2);
	return s;
}

static const flag_def dv_caps_def[] = {
	{ V4L2_DV_BT_CAP_INTERLACED, "Interlaced" },
	{ V4L2_DV_BT_CAP_PROGRESSIVE, "Progressive" },
	{ V4L2_DV_BT_CAP_REDUCED_BLANKING, "Reduced Blanking" },
	{ V4L2_DV_BT_CAP_CUSTOM, "Custom Formats" },
	{ 0, NULL }
};

std::string dv_caps2s(__u32 flags)
{
	return flags2s(flags, dv_caps_def);
}

static const flag_def tc_flags_def[] = {
	{ V4L2_TC_FLAG_DROPFRAME, "dropframe" },
	{ V4L2_TC_FLAG_COLORFRAME, "colorframe" },
	{ V4L2_TC_USERBITS_field, "userbits-field" },
	{ V4L2_TC_USERBITS_USERDEFINED, "userbits-userdefined" },
	{ V4L2_TC_USERBITS_8BITCHARS, "userbits-8bitchars" },
	{ 0, NULL }
};

std::string tc_flags2s(__u32 flags)
{
	return flags2s(flags, tc_flags_def);
}

static const flag_def buffer_flags_def[] = {
	{ V4L2_BUF_FLAG_MAPPED, "mapped" },
	{ V4L2_BUF_FLAG_QUEUED, "queued" },
	{ V4L2_BUF_FLAG_DONE, "done" },
	{ V4L2_BUF_FLAG_KEYFRAME, "keyframe" },
	{ V4L2_BUF_FLAG_PFRAME, "P-frame" },
	{ V4L2_BUF_FLAG_BFRAME, "B-frame" },
	{ V4L2_BUF_FLAG_ERROR, "error" },
	{ V4L2_BUF_FLAG_TIMECODE, "timecode" },
	{ V4L2_BUF_FLAG_PREPARED, "prepared" },
	{ V4L2_BUF_FLAG_NO_CACHE_INVALIDATE, "no-cache-invalidate" },
	{ V4L2_BUF_FLAG_NO_CACHE_CLEAN, "no-cache-clean" },
	{ V4L2_BUF_FLAG_LAST, "last" },
	{ 0, NULL }
};

std::string bufferflags2s(__u32 flags)
{
	return flags2s(flags, buffer_flags_def);
}

static const flag_def vbi_def[] = {
	{ V4L2_VBI_UNSYNC,     "unsynchronized" },
	{ V4L2_VBI_INTERLACED, "interlaced" },
	{ 0, NULL }
};

std::string vbiflags2s(__u32 flags)
{
	return flags2s(flags, vbi_def);
}
