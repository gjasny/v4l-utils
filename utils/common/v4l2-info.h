/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright 2018 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef _V4L2_INFO_H
#define _V4L2_INFO_H

#include <string>

#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>

struct flag_def {
	unsigned flag;
	const char *str;
};

/* Return a comma-separated string of flags or hex value if unknown */
std::string flags2s(unsigned val, const flag_def *def);

/* Print capability information */
void v4l2_info_capability(const v4l2_capability &cap);
void v4l2_info_subdev_capability(const v4l2_subdev_capability &subdevcap);

/* Return fourcc pixelformat string */
std::string fcc2s(__u32 val);

/* Return the description of the pixel format */
std::string pixfmt2s(__u32 format);

/* Return buffer type description */
std::string buftype2s(int type);

/* Return buffer capability description */
std::string bufcap2s(__u32 caps);

static inline std::string buftype2s(enum v4l2_buf_type type)
{
       return buftype2s((int)type);
}

/* Return field description */
std::string field2s(int val);

/* Return colorspace description */
std::string colorspace2s(int val);

/* Return transfer function description */
std::string xfer_func2s(int val);

/* Return YCbCr encoding description */
std::string ycbcr_enc2s(int val);

/* Return quantization description */
std::string quantization2s(int val);

/* Return v4l2_pix_format flags description */
std::string pixflags2s(unsigned flags);

/* Return sliced vbi services description */
std::string service2s(unsigned service);

/* Return v4l2_subdev_mbus_code_enum flags description */
std::string mbus2s(unsigned flags, bool is_hsv);

/* Return v4l2_fmtdesc flags description */
std::string fmtdesc2s(unsigned flags, bool is_hsv);

/* Return selection flags description */
std::string selflags2s(__u32 flags);

/* Return selection target description */
std::string seltarget2s(__u32 target);

/*
 * v4l2-info.cpp has a table with valid selection targets,
 * these functions help navigate that table.
 */

/* Return true if the table at index i has a valid selection target */
bool valid_seltarget_at_idx(unsigned i);

/* Return the selection target in the table at index i (or 0 if out of range) */
unsigned seltarget_at_idx(unsigned i);

/* Return v4l2_std description, separate entries by sep */
std::string std2s(v4l2_std_id std, const char *sep = " ");

/* Return control flags description */
std::string ctrlflags2s(__u32 flags);

/* Return input status description */
std::string in_status2s(__u32 status);

/* Return input capabilities description */
std::string input_cap2s(__u32 capabilities);

/* Return output capabilities description */
std::string output_cap2s(__u32 capabilities);

/* Return framebuffer capabilities description */
std::string fbufcap2s(unsigned cap);

/* Return framebuffer flags description */
std::string fbufflags2s(unsigned fl);

/* Return DV Timings standards description */
std::string dv_standards2s(__u32 flags);

/*
 * Return DV Timings flags description, vsync is the
 * vertical sync value.
 */
std::string dvflags2s(unsigned vsync, int val);

/* Return DV Timings capabilities description */
std::string dv_caps2s(__u32 flags);

/* Return v4l2_timecode flags description */
std::string tc_flags2s(__u32 flags);

/* Return v4l2_buffer flags description */
std::string bufferflags2s(__u32 flags);

/* Return vbi flags description */
std::string vbiflags2s(__u32 flags);

/* Return tuner type description */
std::string ttype2s(int type);

/* Return audio mode description */
std::string audmode2s(int audmode);

/* Return RX subchannels description */
std::string rxsubchans2s(int rxsubchans);

/* Return TX subchannels description */
std::string txsubchans2s(int txsubchans);

/* Return tuner capabilities description */
std::string tcap2s(unsigned cap);

/* Return band modulation description */
std::string modulation2s(unsigned modulation);

#endif
