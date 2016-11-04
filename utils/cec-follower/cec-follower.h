/*
 * CEC API follower test tool.
 *
 * Copyright 2016 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * Alternatively you can redistribute this file under the terms of the
 * BSD license as stated below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. The names of its contributors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _CEC_FOLLOWER_H_
#define _CEC_FOLLOWER_H_

#include <stdarg.h>
#include <cerrno>
#include <string>
#include <linux/cec-funcs.h>

#ifdef ANDROID
#include <android-config.h>
#else
#include <config.h>
#endif

#define ARRAY_SIZE(a) \
	(sizeof(a) / sizeof(*a))

extern bool show_info;
extern bool show_msgs;
extern bool show_state;
extern bool show_warnings;
extern unsigned warnings;

struct state {
	__u16 active_source_pa;
	__u8 old_power_status;
	__u8 power_status;
	time_t power_status_changed_time;
	char menu_language[4];
	__u8 video_latency;
	__u8 low_latency_mode;
	__u8 audio_out_compensated;
	__u8 audio_out_delay;
	bool arc_active;
	bool sac_active;
	__u8 volume;
	bool mute;
	unsigned rc_state;
	__u8 rc_ui_cmd;
	__u64 rc_press_rx_ts;
	unsigned rc_press_hold_count;
	unsigned rc_duration_sum;
};

struct node {
	int fd;
	const char *device;
	unsigned caps;
	unsigned available_log_addrs;
	unsigned adap_la_mask;
	unsigned remote_la_mask;
	__u16 remote_phys_addr[15];
	struct state state;
	__u16 phys_addr;
	__u8 cec_version;
	bool has_arc_rx;
	bool has_arc_tx;
	bool has_aud_rate;
	bool has_deck_ctl;
	bool has_rec_tv;
	bool has_osd_string;
};

struct la_info {
	__u64 ts;
	struct {
		unsigned count;
		__u64 ts;
	} feature_aborted[256];
	__u16 phys_addr;
};

extern struct la_info la_info[15];

struct short_audio_desc {
	/* Byte 1 */
	__u8 num_channels;
	__u8 format_code;

	/* Byte 2 */
	__u8 sample_freq_mask;

	/* Byte 3 */
	union {
		__u8 bit_depth_mask;    // LPCM
		__u8 max_bitrate;       // Format codes 2-8
		__u8 format_dependent;  // Format codes 9-13
		__u8 wma_profile;       // WMA Pro
		__u8 frame_length_mask; // Extension type codes 4-6, 8-10
	};
	__u8 mps;                       // Format codes 8-10
	__u8 extension_type_code;
};

#define SAD_FMT_CODE_LPCM 		1
#define SAD_FMT_CODE_AC3		2
#define SAD_FMT_CODE_MPEG1		3
#define SAD_FMT_CODE_MP3 		4
#define SAD_FMT_CODE_MPEG2 		5
#define SAD_FMT_CODE_AAC_LC 		6
#define SAD_FMT_CODE_DTS 		7
#define SAD_FMT_CODE_ATRAC 		8
#define SAD_FMT_CODE_ONE_BIT_AUDIO	9
#define SAD_FMT_CODE_ENHANCED_AC3	10
#define SAD_FMT_CODE_DTS_HD 		11
#define SAD_FMT_CODE_MAT 		12
#define SAD_FMT_CODE_DST 		13
#define SAD_FMT_CODE_WMA_PRO 		14
#define SAD_FMT_CODE_EXTENDED 		15

#define SAD_BIT_DEPTH_MASK_16 		1
#define SAD_BIT_DEPTH_MASK_20 		(1 << 1)
#define SAD_BIT_DEPTH_MASK_24 		(1 << 2)

#define SAD_SAMPLE_FREQ_MASK_32 	1
#define SAD_SAMPLE_FREQ_MASK_44_1 	(1 << 1)
#define SAD_SAMPLE_FREQ_MASK_48 	(1 << 2)
#define SAD_SAMPLE_FREQ_MASK_88_2 	(1 << 3)
#define SAD_SAMPLE_FREQ_MASK_96 	(1 << 4)
#define SAD_SAMPLE_FREQ_MASK_176_4 	(1 << 5)
#define SAD_SAMPLE_FREQ_MASK_192 	(1 << 6)

#define SAD_FRAME_LENGTH_MASK_960 	1
#define SAD_FRAME_LENGTH_MASK_1024	(1 << 1)

#define SAD_EXT_TYPE_MPEG4_HE_AAC 		4
#define SAD_EXT_TYPE_MPEG4_HE_AACv2 		5
#define SAD_EXT_TYPE_MPEG4_AAC_LC 		6
#define SAD_EXT_TYPE_DRA 			7
#define SAD_EXT_TYPE_MPEG4_HE_AAC_SURROUND 	8
#define SAD_EXT_TYPE_MPEG4_AAC_LC_SURROUND	10
#define SAD_EXT_TYPE_MPEG_H_3D_AUDIO		11
#define SAD_EXT_TYPE_AC_4			12
#define SAD_EXT_TYPE_LPCM_3D_AUDIO		13

#define cec_phys_addr_exp(pa) \
	((pa) >> 12), ((pa) >> 8) & 0xf, ((pa) >> 4) & 0xf, (pa) & 0xf

#define info(fmt, args...) 					\
	do {							\
		if (show_info)					\
			printf("\t\tinfo: " fmt, ##args);	\
	} while (0)

#define dev_info(fmt, args...)					\
	do {							\
		if (show_state)					\
			printf(">>> " fmt, ##args);		\
	} while(0)

#define warn(fmt, args...) 					\
	do {							\
		warnings++;					\
		if (show_warnings)				\
			printf("\t\twarn: %s(%d): " fmt, __FILE__, __LINE__, ##args);	\
	} while (0)

#define warn_once(fmt, args...)						\
	do {								\
		static bool show;					\
									\
		if (!show) {						\
			show = true;					\
			warnings++;					\
			if (show_warnings)				\
				printf("\t\twarn: %s(%d): " fmt,	\
					__FILE__, __LINE__, ##args); 	\
		}							\
	} while (0)

int cec_named_ioctl(int fd, const char *name,
		    unsigned long int request, void *parm);

#define doioctl(n, r, p) cec_named_ioctl((n)->fd, #r, r, p)

#define cec_phys_addr_exp(pa) \
        ((pa) >> 12), ((pa) >> 8) & 0xf, ((pa) >> 4) & 0xf, (pa) & 0xf

#define transmit(n, m) (doioctl(n, CEC_TRANSMIT, m))

static inline unsigned ts_to_ms(__u64 ts)
{
	return ts / 1000000;
}

static inline unsigned ts_to_s(__u64 ts)
{
	return ts / 1000000000;
}

static inline __u64 get_ts()
{
	struct timespec timespec;

	clock_gettime(CLOCK_MONOTONIC, &timespec);
	return timespec.tv_sec * 1000000000ull + timespec.tv_nsec;
}

const char *la2s(unsigned la);
const char *la_type2s(unsigned type);
const char *prim_type2s(unsigned type);
const char *version2s(unsigned version);
std::string status2s(const struct cec_msg &msg);
std::string all_dev_types2s(unsigned types);
std::string rc_src_prof2s(unsigned prof);
std::string dev_feat2s(unsigned feat);
std::string audio_format_id_code2s(__u8 audio_format_id, __u8 audio_format_code);
std::string opcode2s(const struct cec_msg *msg);
void sad_encode(const struct short_audio_desc *sad, __u32 *descriptor);

// CEC processing
void testProcessing(struct node *node);

// cec-log.c
void log_msg(const struct cec_msg *msg);

#endif
