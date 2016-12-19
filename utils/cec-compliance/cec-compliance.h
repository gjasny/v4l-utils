/*
 * CEC API compliance test tool.
 *
 * Copyright 2016 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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

#ifndef _CEC_COMPLIANCE_H_
#define _CEC_COMPLIANCE_H_

#include <stdarg.h>
#include <cerrno>
#include <string>
#include <linux/cec-funcs.h>

#ifdef ANDROID
#include <android-config.h>
#else
#include <config.h>
#endif

#define TAG_AUDIO_RATE_CONTROL		1
#define TAG_ARC_CONTROL 		(1 << 1)
#define TAG_CAP_DISCOVERY_CONTROL 	(1 << 2)
#define TAG_DECK_CONTROL 		(1 << 3)
#define TAG_DEVICE_MENU_CONTROL 	(1 << 4)
#define TAG_DEVICE_OSD_TRANSFER 	(1 << 5)
#define TAG_DYNAMIC_AUTO_LIPSYNC 	(1 << 6)
#define TAG_OSD_DISPLAY 		(1 << 7)
#define TAG_ONE_TOUCH_PLAY 		(1 << 8)
#define TAG_ONE_TOUCH_RECORD 		(1 << 9)
#define TAG_POWER_STATUS 		(1 << 10)
#define TAG_REMOTE_CONTROL_PASSTHROUGH	(1 << 11)
#define TAG_ROUTING_CONTROL 		(1 << 12)
#define TAG_SYSTEM_AUDIO_CONTROL 	(1 << 13)
#define TAG_SYSTEM_INFORMATION		(1 << 14)
#define TAG_TIMER_PROGRAMMING 		(1 << 15)
#define TAG_TUNER_CONTROL 		(1 << 16)
#define TAG_VENDOR_SPECIFIC_COMMANDS 	(1 << 17)
#define TAG_STANDBY_RESUME		(1 << 18)

#define TAG_INTERACTIVE		(1 << 19)
#define TAG_CORE 0
#define TAG_ALL (~TAG_INTERACTIVE)

#define NUM_TAGS 21

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

extern bool show_info;
extern bool show_warnings;
extern unsigned warnings;
extern unsigned reply_threshold;

struct remote {
	bool recognized_op[256];
	bool unrecognized_op[256];
	unsigned prim_type;
	__u16 phys_addr;
	__u8 cec_version;
	__u8 rc_profile;
	__u8 dev_features;
	bool has_osd;
	bool has_power_status;
	bool has_image_view_on;
	bool has_text_view_on;
	bool in_standby;
	bool has_remote_control_passthrough;
	bool has_arc_rx;
	bool has_arc_tx;
	bool arc_initiated;
	bool has_sys_audio_mode_req;
	bool has_set_sys_audio_mode;
	bool has_sad;
	__u8 supp_format_id;
	__u8 supp_format_code;
	__u8 volume;
	__u8 mute;
	bool has_aud_rate;
	bool has_deck_ctl;
	__u8 bcast_sys;
	__u8 dig_bcast_sys;
	bool has_rec_tv;
	bool has_cdc;
};

struct node {
	int fd;
	const char *device;
	bool has_cec20;
	bool test_cec20;
	unsigned caps;
	unsigned available_log_addrs;
	unsigned num_log_addrs;
	unsigned adap_la_mask;
	__u8 log_addr[CEC_MAX_LOG_ADDRS];
	unsigned remote_la_mask;
	struct remote remote[16];
	__u16 phys_addr;
};

struct remote_subtest {
	const char *name;
	const __u16 la_mask;
	int (*const test_fn)(struct node *node, unsigned me, unsigned la, bool interactive);
};

#define PRESUMED_OK 1
#define FAIL 2
#define FAIL_CRITICAL 3
#define NOTSUPPORTED 4
#define NOTAPPLICABLE 5
#define REFUSED 6

#define CEC_LOG_ADDR_MASK_ALL 0xffff

#define ARRAY_SIZE(a) \
	(sizeof(a) / sizeof(*a))

#define cec_phys_addr_exp(pa) \
	((pa) >> 12), ((pa) >> 8) & 0xf, ((pa) >> 4) & 0xf, (pa) & 0xf

#define info(fmt, args...) 					\
	do {							\
		if (show_info)					\
			printf("\t\tinfo: " fmt, ##args);	\
	} while (0)

#define announce(fmt, args...) 					\
	do {							\
		printf("\t\t>>> " fmt "\n", ##args);			\
	} while (0)

#define interactive_info(block, fmt, args...)				\
	do {								\
		if (interactive) {					\
			printf("\t\t>>> " fmt "\n", ##args);		\
			if (block) {					\
				printf("\t\t>>> Press ENTER to proceed.\n"); \
				getchar();				\
			}						\
		}							\
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

#define fail(fmt, args...) 						\
({ 									\
	printf("\t\tfail: %s(%d): " fmt, __FILE__, __LINE__, ##args);	\
	FAIL;								\
})

#define fail_on_test(test) 				\
	do {						\
		if (test)				\
			return fail("%s\n", #test);	\
	} while (0)

#define fail_on_test_v2(version, test) fail_on_test(version >= CEC_OP_CEC_VERSION_2_0 && (test))

#define fail_on_test_v2_warn(version, test)				\
	do {								\
		if (test) {						\
			if (version >= CEC_OP_CEC_VERSION_2_0)		\
				return fail("%s\n", #test);		\
			else						\
				warn("fails in CEC 2.0: %s\n", #test);	\
		}							\
	} while(0)

static inline char get_yn()
{
	char c;

	while ((c = tolower(getchar())) != 'y' && c != 'n');
	getchar();
	return c;
}

static inline bool question(const char* prompt)
{
	printf("\t\t>>> %s (y/n) ", prompt);
	return get_yn() == 'y';
}

int cec_named_ioctl(struct node *node, const char *name,
		    unsigned long int request, void *parm);

#define doioctl(n, r, p) cec_named_ioctl(n, #r, r, p)

#define cec_phys_addr_exp(pa) \
        ((pa) >> 12), ((pa) >> 8) & 0xf, ((pa) >> 4) & 0xf, (pa) & 0xf

std::string opcode2s(const struct cec_msg *msg);

static inline bool is_tv(unsigned la, unsigned prim_type)
{
	return cec_has_tv(1 << la) ||
		(cec_has_specific(1 << la) && prim_type == CEC_OP_PRIM_DEVTYPE_TV);
}

static inline bool is_playback_or_rec(unsigned la)
{
	return cec_has_playback(1 << la) || cec_has_record(1 << la);
}

static inline bool cec_msg_status_is_abort(const struct cec_msg *msg)
{
	return msg->rx_status & CEC_RX_STATUS_FEATURE_ABORT;
}

static inline __u8 abort_reason(const struct cec_msg *msg)
{
	return msg->msg[3];
}

static inline bool unrecognized_op(const struct cec_msg *msg)
{
	if (!cec_msg_status_is_abort(msg))
		return false;
	if (abort_reason(msg) == CEC_OP_ABORT_UNRECOGNIZED_OP)
		return true;
	if (abort_reason(msg) == CEC_OP_ABORT_UNDETERMINED) {
		warn("Opcode %x was undetermined and is treated as not supported.\n", msg->msg[2]);
		return true;
	}
	return false;
}

static inline bool refused(const struct cec_msg *msg)
{
	return cec_msg_status_is_abort(msg) && abort_reason(msg) == CEC_OP_ABORT_REFUSED;
}

static inline bool timed_out(const struct cec_msg *msg)
{
	return msg->rx_status & CEC_RX_STATUS_TIMEOUT;
}

static inline bool timed_out_or_abort(const struct cec_msg *msg)
{
	return timed_out(msg) || cec_msg_status_is_abort(msg);
}

static inline unsigned response_time_ms(const struct cec_msg *msg)
{
	unsigned ms = (msg->rx_ts - msg->tx_ts) / 1000000;

	// Compensate for the time it took (approx.) to receive the
	// message.
	if (ms >= msg->len * 24)
		return ms - msg->len * 24;
	return 0;
}

static inline bool transmit_timeout(struct node *node, struct cec_msg *msg,
				    unsigned timeout = 2000)
{
	struct cec_msg original_msg = *msg;
	__u8 opcode = cec_msg_opcode(msg);
	int res;

	msg->timeout = timeout;
	res = doioctl(node, CEC_TRANSMIT, msg);
	if (res == ENODEV) {
		printf("Device was disconnected.\n");
		exit(1);
	}
	if (res || !(msg->tx_status & CEC_TX_STATUS_OK))
		return false;

	if (((msg->rx_status & CEC_RX_STATUS_OK) || (msg->rx_status & CEC_RX_STATUS_FEATURE_ABORT))
	    && response_time_ms(msg) > reply_threshold)
		warn("Waited %4ums for reply to msg 0x%02x.\n", response_time_ms(msg), opcode);

	if (!cec_msg_status_is_abort(msg))
		return true;

	if (cec_msg_is_broadcast(&original_msg)) {
		fail("Received Feature Abort in reply to broadcast message\n");
		return false;
	}

	const char *reason;

	switch (abort_reason(msg)) {
	case CEC_OP_ABORT_UNRECOGNIZED_OP:
	case CEC_OP_ABORT_UNDETERMINED:
		return true;
	case CEC_OP_ABORT_INVALID_OP:
		reason = "Invalid operand";
		break;
	case CEC_OP_ABORT_NO_SOURCE:
		reason = "Cannot provide source";
		break;
	case CEC_OP_ABORT_REFUSED:
		reason = "Refused";
		break;
	case CEC_OP_ABORT_INCORRECT_MODE:
		reason = "Incorrect mode";
		break;
	default:
		reason = "Unknown";
		break;
	}
	info("Opcode %s was replied to with Feature Abort [%s]\n",
	     opcode2s(&original_msg).c_str(), reason);

	return true;
}

static inline bool transmit(struct node *node, struct cec_msg *msg)
{
	return transmit_timeout(node, msg, 0);
}

static inline unsigned get_ts_ms()
{
	struct timespec timespec;

	clock_gettime(CLOCK_MONOTONIC, &timespec);
	return timespec.tv_sec * 1000ull + timespec.tv_nsec / 1000000;
}

const char *ok(int res);
const char *la2s(unsigned la);
const char *la_type2s(unsigned type);
const char *prim_type2s(unsigned type);
const char *version2s(unsigned version);
std::string status2s(const struct cec_msg &msg);
std::string all_dev_types2s(unsigned types);
std::string rc_src_prof2s(unsigned prof);
std::string dev_feat2s(unsigned feat);
const char *power_status2s(__u8 power_status);
std::string short_audio_desc2s(const struct short_audio_desc &sad);
void sad_decode(struct short_audio_desc *sad, __u32 descriptor);
const char *bcast_system2s(__u8 bcast_system);
const char *dig_bcast_system2s(__u8 bcast_system);
const char *hec_func_state2s(__u8 hfs);
const char *host_func_state2s(__u8 hfs);
const char *enc_func_state2s(__u8 efs);
const char *cdc_errcode2s(__u8 cdc_errcode);
int check_0(const void *p, int len);

// CEC adapter tests
int testCap(struct node *node);
int testDQEvent(struct node *node);
int testAdapPhysAddr(struct node *node);
int testAdapLogAddrs(struct node *node);
int testTransmit(struct node *node);
int testReceive(struct node *node);
int testNonBlocking(struct node *node);
int testModes(struct node *node, struct node *node2);
int testLostMsgs(struct node *node);

// CEC core tests
int testCore(struct node *node);

// CEC processing
int testProcessing(struct node *node, unsigned me);

// CEC testing
void testRemote(struct node *node, unsigned me, unsigned la, unsigned test_tags,
			     bool interactive);

// cec-audio.cpp
extern struct remote_subtest sac_subtests[];
extern const unsigned sac_subtests_size;
extern struct remote_subtest dal_subtests[];
extern const unsigned dal_subtests_size;
extern struct remote_subtest arc_subtests[];
extern const unsigned arc_subtests_size;
extern struct remote_subtest audio_rate_ctl_subtests[];
extern const unsigned audio_rate_ctl_subtests_size;

// cec-power.cpp
bool util_interactive_ensure_power_state(struct node *node, unsigned me, unsigned la, bool interactive,
					 __u8 target_pwr);
extern struct remote_subtest standby_subtests[];
extern const unsigned standby_subtests_size;
extern struct remote_subtest one_touch_play_subtests[];
extern const unsigned one_touch_play_subtests_size;
extern struct remote_subtest power_status_subtests[];
extern const unsigned power_status_subtests_size;
extern struct remote_subtest standby_resume_subtests[];
extern const unsigned standby_resume_subtests_size;

#endif
