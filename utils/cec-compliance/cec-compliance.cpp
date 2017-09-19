/*
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sstream>

#include "cec-compliance.h"
#ifndef ANDROID
#include "version.h"
#endif

#include "cec-table.h"

/* Short option list

   Please keep in alphabetical order.
   That makes it easier to see which short options are still free.

   In general the lower case is used to set something and the upper
   case is used to retrieve a setting. */
enum Option {
	OptTestAdapter = 'A',
	OptSetDevice = 'd',
	OptHelp = 'h',
	OptInteractive = 'i',
	OptNoWarnings = 'n',
	OptRemote = 'r',
	OptReplyThreshold = 'R',
	OptTimeout = 't',
	OptTrace = 'T',
	OptVerbose = 'v',
	OptWallClock = 'w',

	OptTestCore = 128,
	OptTestAudioRateControl,
	OptTestARCControl,
	OptTestCapDiscoveryControl,
	OptTestDeckControl,
	OptTestDeviceMenuControl,
	OptTestDeviceOSDTransfer,
	OptTestDynamicAutoLipsync,
	OptTestOSDDisplay,
	OptTestOneTouchPlay,
	OptTestOneTouchRecord,
	OptTestPowerStatus,
	OptTestRemoteControlPassthrough,
	OptTestRoutingControl,
	OptTestSystemAudioControl,
	OptTestSystemInformation,
	OptTestTimerProgramming,
	OptTestTunerControl,
	OptTestVendorSpecificCommands,
	OptTestStandbyResume,

	OptSkipTestAudioRateControl,
	OptSkipTestARCControl,
	OptSkipTestCapDiscoveryControl,
	OptSkipTestDeckControl,
	OptSkipTestDeviceMenuControl,
	OptSkipTestDeviceOSDTransfer,
	OptSkipTestDynamicAutoLipsync,
	OptSkipTestOSDDisplay,
	OptSkipTestOneTouchPlay,
	OptSkipTestOneTouchRecord,
	OptSkipTestPowerStatus,
	OptSkipTestRemoteControlPassthrough,
	OptSkipTestRoutingControl,
	OptSkipTestSystemAudioControl,
	OptSkipTestSystemInformation,
	OptSkipTestTimerProgramming,
	OptSkipTestTunerControl,
	OptSkipTestVendorSpecificCommands,
	OptSkipTestStandbyResume,
	OptLast = 256
};


static char options[OptLast];

static int app_result;
static int tests_total, tests_ok;

bool show_info;
bool show_warnings = true;
unsigned warnings;
unsigned reply_threshold = 1000;
unsigned long_timeout = 60;

static struct option long_options[] = {
	{"device", required_argument, 0, OptSetDevice},
	{"help", no_argument, 0, OptHelp},
	{"no-warnings", no_argument, 0, OptNoWarnings},
	{"remote", optional_argument, 0, OptRemote},
	{"timeout", required_argument, 0, OptTimeout},
	{"trace", no_argument, 0, OptTrace},
	{"verbose", no_argument, 0, OptVerbose},
	{ "wall-clock", no_argument, 0, OptWallClock },
	{"interactive", no_argument, 0, OptInteractive},
	{"reply-threshold", required_argument, 0, OptReplyThreshold},

	{"test-adapter", no_argument, 0, OptTestAdapter},
	{"test-core", no_argument, 0, OptTestCore},
	{"test-audio-rate-control", no_argument, 0, OptTestAudioRateControl},
	{"test-audio-return-channel-control", no_argument, 0, OptTestARCControl},
	{"test-capability-discovery-and-control", no_argument, 0, OptTestCapDiscoveryControl},
	{"test-deck-control", no_argument, 0, OptTestDeckControl},
	{"test-device-menu-control", no_argument, 0, OptTestDeviceMenuControl},
	{"test-device-osd-transfer", no_argument, 0, OptTestDeviceOSDTransfer},
	{"test-dynamic-auto-lipsync", no_argument, 0, OptTestDynamicAutoLipsync},
	{"test-osd-display", no_argument, 0, OptTestOSDDisplay},
	{"test-one-touch-play", no_argument, 0, OptTestOneTouchPlay},
	{"test-one-touch-record", no_argument, 0, OptTestOneTouchRecord},
	{"test-power-status", no_argument, 0, OptTestPowerStatus},
	{"test-remote-control-passthrough", no_argument, 0, OptTestRemoteControlPassthrough},
	{"test-routing-control", no_argument, 0, OptTestRoutingControl},
	{"test-system-audio-control", no_argument, 0, OptTestSystemAudioControl},
	{"test-system-information", no_argument, 0, OptTestSystemInformation},
	{"test-timer-programming", no_argument, 0, OptTestTimerProgramming},
	{"test-tuner-control", no_argument, 0, OptTestTunerControl},
	{"test-vendor-specific-commands", no_argument, 0, OptTestVendorSpecificCommands},
	{"test-standby-resume", no_argument, 0, OptTestStandbyResume},

	{"skip-test-audio-rate-control", no_argument, 0, OptSkipTestAudioRateControl},
	{"skip-test-audio-return-channel-control", no_argument, 0, OptSkipTestARCControl},
	{"skip-test-capability-discovery-and-control", no_argument, 0, OptSkipTestCapDiscoveryControl},
	{"skip-test-deck-control", no_argument, 0, OptSkipTestDeckControl},
	{"skip-test-device-menu-control", no_argument, 0, OptSkipTestDeviceMenuControl},
	{"skip-test-device-osd-transfer", no_argument, 0, OptSkipTestDeviceOSDTransfer},
	{"skip-test-dynamic-auto-lipsync", no_argument, 0, OptSkipTestDynamicAutoLipsync},
	{"skip-test-osd-display", no_argument, 0, OptSkipTestOSDDisplay},
	{"skip-test-one-touch-play", no_argument, 0, OptSkipTestOneTouchPlay},
	{"skip-test-one-touch-record", no_argument, 0, OptSkipTestOneTouchRecord},
	{"skip-test-power-status", no_argument, 0, OptSkipTestPowerStatus},
	{"skip-test-remote-control-passthrough", no_argument, 0, OptSkipTestRemoteControlPassthrough},
	{"skip-test-routing-control", no_argument, 0, OptSkipTestRoutingControl},
	{"skip-test-system-audio-control", no_argument, 0, OptSkipTestSystemAudioControl},
	{"skip-test-system-information", no_argument, 0, OptSkipTestSystemInformation},
	{"skip-test-timer-programming", no_argument, 0, OptSkipTestTimerProgramming},
	{"skip-test-tuner-control", no_argument, 0, OptSkipTestTunerControl},
	{"skip-test-vendor-specific-commands", no_argument, 0, OptSkipTestVendorSpecificCommands},
	{"skip-test-standby-resume", no_argument, 0, OptSkipTestStandbyResume},
	{0, 0, 0, 0}
};

static void usage(void)
{
	printf("Usage:\n"
	       "  -d, --device=<dev>   Use device <dev> instead of /dev/cec0\n"
	       "                       If <dev> starts with a digit, then /dev/cec<dev> is used.\n"
	       "  -r, --remote[=<la>]  As initiator test the remote logical address or all LAs if no LA was given\n"
	       "  -R, --reply-threshold=<timeout>\n"
	       "                       Warn if replies take longer than this threshold (default 1000ms)\n"
	       "  -i, --interactive    Interactive mode when doing remote tests\n"
	       "  -t, --timeout=<secs> Set the standby/resume timeout to <secs>. Default is 60s.\n"
	       "\n"
	       "  -A, --test-adapter                  Test the CEC adapter API\n"
	       "  --test-core                         Test the core functionality\n"
	       "\n"
	       "By changing --test to --skip-test in the following options you can skip tests\n"
	       "instead of enabling them.\n"
	       "\n"
	       "  --test-audio-rate-control           Test the Audio Rate Control feature\n"
	       "  --test-audio-return-channel-control Test the Audio Return Channel Control feature\n"
	       "  --test-capability-discovery-and-control Test the Capability Discovery and Control feature\n"
	       "  --test-deck-control                 Test the Deck Control feature\n"
	       "  --test-device-menu-control          Test the Device Menu Control feature\n"
	       "  --test-device-osd-transfer          Test the Device OSD Transfer feature\n"
	       "  --test-dynamic-auto-lipsync         Test the Dynamic Auto Lipsync feature\n"
	       "  --test-osd-display                  Test the OSD Display feature\n"
	       "  --test-one-touch-play               Test the One Touch Play feature\n"
	       "  --test-one-touch-record             Test the One Touch Record feature\n"
	       "  --test-power-status                 Test the Power Status feature\n"
	       "  --test-remote-control-passthrough   Test the Remote Control Passthrough feature\n"
	       "  --test-routing-control              Test the Routing Control feature\n"
	       "  --test-system-audio-control         Test the System Audio Control feature\n"
	       "  --test-system-information           Test the System Information feature\n"
	       "  --test-timer-programming            Test the Timer Programming feature\n"
	       "  --test-tuner-control                Test the Tuner Control feature\n"
	       "  --test-vendor-specific-commands     Test the Vendor Specific Commands feature\n"
	       "  --test-standby-resume               Test standby and resume functionality. This will activate\n"
	       "                                      testing of Standby, Give Device Power Status and One Touch Play.\n"
	       "\n"
	       "  -h, --help         Display this help message\n"
	       "  -n, --no-warnings  Turn off warning messages\n"
	       "  -T, --trace        Trace all called ioctls\n"
	       "  -v, --verbose      Turn on verbose reporting\n"
	       "  -w, --wall-clock   Show timestamps as wall-clock time\n"
	       );
}

static std::string ts2s(__u64 ts)
{
	std::string s;
	struct timespec now;
	struct timeval tv;
	struct timeval sub;
	struct timeval res;
	__u64 diff;
	char buf[64];
	time_t t;

	if (!options[OptWallClock]) {
		sprintf(buf, "%llu.%03llus", ts / 1000000000, (ts % 1000000000) / 1000000);
		return buf;
	}
	clock_gettime(CLOCK_MONOTONIC, &now);
	gettimeofday(&tv, NULL);
	diff = now.tv_sec * 1000000000ULL + now.tv_nsec - ts;
	sub.tv_sec = diff / 1000000000ULL;
	sub.tv_usec = (diff % 1000000000ULL) / 1000;
	timersub(&tv, &sub, &res);
	t = res.tv_sec;
	s = ctime(&t);
	s = s.substr(0, s.length() - 6);
	sprintf(buf, "%03lu", res.tv_usec / 1000);
	return s + "." + buf;
}

static std::string tx_status2s(const struct cec_msg &msg)
{
	std::string s;
	char num[4];
	unsigned stat = msg.tx_status;

	if (stat)
		s += "Tx";
	if (stat & CEC_TX_STATUS_OK)
		s += ", OK";
	if (stat & CEC_TX_STATUS_ARB_LOST) {
		sprintf(num, "%u", msg.tx_arb_lost_cnt);
		s += ", Arbitration Lost";
		if (msg.tx_arb_lost_cnt)
			s += " (" + std::string(num) + ")";
	}
	if (stat & CEC_TX_STATUS_NACK) {
		sprintf(num, "%u", msg.tx_nack_cnt);
		s += ", Not Acknowledged";
		if (msg.tx_nack_cnt)
			s += " (" + std::string(num) + ")";
	}
	if (stat & CEC_TX_STATUS_LOW_DRIVE) {
		sprintf(num, "%u", msg.tx_low_drive_cnt);
		s += ", Low Drive";
		if (msg.tx_low_drive_cnt)
			s += " (" + std::string(num) + ")";
	}
	if (stat & CEC_TX_STATUS_ERROR) {
		sprintf(num, "%u", msg.tx_error_cnt);
		s += ", Error";
		if (msg.tx_error_cnt)
			s += " (" + std::string(num) + ")";
	}
	if (stat & CEC_TX_STATUS_MAX_RETRIES)
		s += ", Max Retries";
	return s;
}

static std::string rx_status2s(unsigned stat)
{
	std::string s;

	if (stat)
		s += "Rx";
	if (stat & CEC_RX_STATUS_OK)
		s += ", OK";
	if (stat & CEC_RX_STATUS_TIMEOUT)
		s += ", Timeout";
	if (stat & CEC_RX_STATUS_FEATURE_ABORT)
		s += ", Feature Abort";
	return s;
}

std::string status2s(const struct cec_msg &msg)
{
	std::string s;

	if (msg.tx_status)
		s = tx_status2s(msg);
	if (msg.rx_status) {
		if (!s.empty())
			s += ", ";
		s += rx_status2s(msg.rx_status);
	}
	return s;
}

const char *power_status2s(__u8 power_status)
{
	switch (power_status) {
	case CEC_OP_POWER_STATUS_ON:
		return "On";
	case CEC_OP_POWER_STATUS_STANDBY:
		return "Standby";
	case CEC_OP_POWER_STATUS_TO_ON:
		return "In transition Standby to On";
	case CEC_OP_POWER_STATUS_TO_STANDBY:
		return "In transition On to Standby";
	default:
		return "Unknown";
	}
}

static std::string audio_format_code2s(__u8 format_code)
{
	switch (format_code) {
	case 0:
		return "Reserved";
	case SAD_FMT_CODE_LPCM:
		return "L-PCM";
	case SAD_FMT_CODE_AC3:
		return "AC-3";
	case SAD_FMT_CODE_MPEG1:
		return "MPEG-1";
	case SAD_FMT_CODE_MP3:
		return "MP3";
	case SAD_FMT_CODE_MPEG2:
		return "MPEG2";
	case SAD_FMT_CODE_AAC_LC:
		return "AAC LC";
	case SAD_FMT_CODE_DTS:
		return "DTS";
	case SAD_FMT_CODE_ATRAC:
		return "ATRAC";
	case SAD_FMT_CODE_ONE_BIT_AUDIO:
		return "One Bit Audio";
	case SAD_FMT_CODE_ENHANCED_AC3:
		return "Enhanced AC-3";
	case SAD_FMT_CODE_DTS_HD:
		return "DTS-HD";
	case SAD_FMT_CODE_MAT:
		return "MAT";
	case SAD_FMT_CODE_DST:
		return "DST";
	case SAD_FMT_CODE_WMA_PRO:
		return "WMA Pro";
	case SAD_FMT_CODE_EXTENDED:
		return "Extended";
	default:
		return "Illegal";
	};
}

std::string extension_type_code2s(__u8 type_code)
{
	switch (type_code) {
	case 0:
	case 1:
	case 2:
	case 3:
		return "Not in use";
	case SAD_EXT_TYPE_MPEG4_HE_AAC:
		return "MPEG-4 HE AAC";
	case SAD_EXT_TYPE_MPEG4_HE_AACv2:
		return "MPEG-4 HE AAC v2";
	case SAD_EXT_TYPE_MPEG4_AAC_LC:
		return "MPEG-4 AAC LC";
	case SAD_EXT_TYPE_DRA:
		return "DRA";
	case SAD_EXT_TYPE_MPEG4_HE_AAC_SURROUND:
		return "MPEG-4 HE AAC + MPEG Surround";
	case SAD_EXT_TYPE_MPEG4_AAC_LC_SURROUND:
		return "MPEG-4 AAC LC + MPEG Surround";
	case SAD_EXT_TYPE_MPEG_H_3D_AUDIO:
		return "MPEG-H 3D Audio";
	case SAD_EXT_TYPE_AC_4:
		return "AC-4";
	case SAD_EXT_TYPE_LPCM_3D_AUDIO:
		return "L-PCM 3D Audio";
	default:
		return "Reserved";
	}
}

std::string short_audio_desc2s(const struct short_audio_desc &sad)
{
	std::stringstream oss;

	if (sad.format_code != SAD_FMT_CODE_EXTENDED)
		oss << audio_format_code2s(sad.format_code);
	else
		oss << extension_type_code2s(sad.extension_type_code);
	oss << ", " << (int)sad.num_channels << " channels";

	oss << ", sampling rates (kHz): ";
	if (sad.sample_freq_mask & SAD_SAMPLE_FREQ_MASK_32)
		oss << "32,";
	if (sad.sample_freq_mask & SAD_SAMPLE_FREQ_MASK_44_1)
		oss << "44.1,";
	if (sad.sample_freq_mask & SAD_SAMPLE_FREQ_MASK_48)
		oss << "48,";
	if (sad.sample_freq_mask & SAD_SAMPLE_FREQ_MASK_88_2)
		oss << "88.2,";
	if (sad.sample_freq_mask & SAD_SAMPLE_FREQ_MASK_96)
		oss << "96,";
	if (sad.sample_freq_mask & SAD_SAMPLE_FREQ_MASK_176_4)
		oss << "176.4,";
	if (sad.sample_freq_mask & SAD_SAMPLE_FREQ_MASK_192)
		oss << "192,";
	if (sad.sample_freq_mask & (1 << 7))
		oss << "Reserved,";
	oss << "\b \b";

	if (sad.format_code == SAD_FMT_CODE_LPCM ||
	    (sad.format_code == SAD_FMT_CODE_EXTENDED &&
	     sad.extension_type_code == SAD_EXT_TYPE_LPCM_3D_AUDIO)) {
		oss << ", bit depth: ";
		if (sad.bit_depth_mask & SAD_BIT_DEPTH_MASK_16)
			oss << "16,";
		if (sad.bit_depth_mask & SAD_BIT_DEPTH_MASK_20)
			oss << "20,";
		if (sad.bit_depth_mask & SAD_BIT_DEPTH_MASK_24)
			oss << "24,";
		oss << "\b \b";
	} else if (sad.format_code >= 2 && sad.format_code <= 8)
		oss << " max bitrate (kbit/s): " << 8 * sad.max_bitrate;

	if (sad.format_code == SAD_FMT_CODE_EXTENDED) {
		switch (sad.extension_type_code) {
		case 4:
		case 5:
		case 6:
		case 8:
		case 10:
			oss << ", frame length: ";
			if (sad.frame_length_mask & SAD_FRAME_LENGTH_MASK_960)
				oss << "960,";
			if (sad.frame_length_mask & SAD_FRAME_LENGTH_MASK_1024)
				oss << "1024,";
			oss << "\b";
			break;
		}

		if (sad.extension_type_code == 8 || sad.extension_type_code == 10)
			oss << ", MPS";
	}

	return oss.str();
}

void sad_decode(struct short_audio_desc *sad, __u32 descriptor)
{
	__u8 b1 = (descriptor >> 16) & 0xff;
	__u8 b2 = (descriptor >> 8) & 0xff;
	__u8 b3 = descriptor & 0xff;

	sad->num_channels = (b1 & 0x07) + 1;
	sad->format_code = (b1 >> 3) & 0x0f;
	sad->sample_freq_mask = b2;

	switch (sad->format_code) {
	case SAD_FMT_CODE_LPCM:
		sad->bit_depth_mask = b3 & 0x07;
		break;
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
		sad->max_bitrate = b3;
		break;
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
		sad->format_dependent = b3;
		break;
	case SAD_FMT_CODE_WMA_PRO:
		sad->wma_profile = b3 & 0x03;
		break;
	case SAD_FMT_CODE_EXTENDED:
	        sad->extension_type_code = (b3 >> 3) & 0x1f;

		switch (sad->extension_type_code) {
		case 4:
		case 5:
		case 6:
			sad->frame_length_mask = (b3 >> 1) & 0x03;
			break;
		case 8:
		case 10:
			sad->frame_length_mask = (b3 >> 1) & 0x03;
			sad->mps = b3 & 1;
			break;
		case SAD_EXT_TYPE_MPEG_H_3D_AUDIO:
		case SAD_EXT_TYPE_AC_4:
			sad->format_dependent = b3 & 0x07;
		case SAD_EXT_TYPE_LPCM_3D_AUDIO:
			sad->bit_depth_mask = b3 & 0x07;
			break;
		}
		break;
	}
}

const char *bcast_system2s(__u8 bcast_system)
{
	switch (bcast_system) {
	case CEC_OP_BCAST_SYSTEM_PAL_BG:
		return "PAL B/G";
	case CEC_OP_BCAST_SYSTEM_SECAM_LQ:
		return "SECAM L'";
	case CEC_OP_BCAST_SYSTEM_PAL_M:
		return "PAL M";
	case CEC_OP_BCAST_SYSTEM_NTSC_M:
		return "NTSC M";
	case CEC_OP_BCAST_SYSTEM_PAL_I:
		return "PAL I";
	case CEC_OP_BCAST_SYSTEM_SECAM_DK:
		return "SECAM DK";
	case CEC_OP_BCAST_SYSTEM_SECAM_BG:
		return "SECAM B/G";
	case CEC_OP_BCAST_SYSTEM_SECAM_L:
		return "SECAM L";
	case CEC_OP_BCAST_SYSTEM_PAL_DK:
		return "PAL DK";
	case 31:
		return "Other System";
	default:
		return "Future use";
	}
}

const char *dig_bcast_system2s(__u8 bcast_system)
{
	switch (bcast_system) {
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_GEN:
		return "ARIB generic";
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_GEN:
		return "ATSC generic";
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_GEN:
		return "DVB generic";
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_BS:
		return "ARIB-BS";
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_CS:
		return "ARIB-CS";
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_T:
		return "ARIB-T";
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_CABLE:
		return "ATSC Cable";
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_SAT:
		return "ATSC Satellite";
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_T:
		return "ATSC Terrestrial";
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_C:
		return "DVB-C";
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_S:
		return "DVB-S";
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_S2:
		return "DVB S2";
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_T:
		return "DVB-T";
	default:
		return "Invalid";
	}
}

const char *hec_func_state2s(__u8 hfs)
{
	switch (hfs) {
	case CEC_OP_HEC_FUNC_STATE_NOT_SUPPORTED:
		return "HEC Not Supported";
	case CEC_OP_HEC_FUNC_STATE_INACTIVE:
		return "HEC Inactive";
	case CEC_OP_HEC_FUNC_STATE_ACTIVE:
		return "HEC Active";
	case CEC_OP_HEC_FUNC_STATE_ACTIVATION_FIELD:
		return "HEC Activation Field";
	default:
		return "Unknown";
	}
}

const char *host_func_state2s(__u8 hfs)
{
	switch (hfs) {
	case CEC_OP_HOST_FUNC_STATE_NOT_SUPPORTED:
		return "Host Not Supported";
	case CEC_OP_HOST_FUNC_STATE_INACTIVE:
		return "Host Inactive";
	case CEC_OP_HOST_FUNC_STATE_ACTIVE:
		return "Host Active";
	default:
		return "Unknown";
	}
}

const char *enc_func_state2s(__u8 efs)
{
	switch (efs) {
	case CEC_OP_ENC_FUNC_STATE_EXT_CON_NOT_SUPPORTED:
		return "Ext Con Not Supported";
	case CEC_OP_ENC_FUNC_STATE_EXT_CON_INACTIVE:
		return "Ext Con Inactive";
	case CEC_OP_ENC_FUNC_STATE_EXT_CON_ACTIVE:
		return "Ext Con Active";
	default:
		return "Unknown";
	}
}

const char *cdc_errcode2s(__u8 cdc_errcode)
{
	switch (cdc_errcode) {
	case CEC_OP_CDC_ERROR_CODE_NONE:
		return "No error";
	case CEC_OP_CDC_ERROR_CODE_CAP_UNSUPPORTED:
		return "Initiator does not have requested capability";
	case CEC_OP_CDC_ERROR_CODE_WRONG_STATE:
		return "Initiator is in wrong state";
	case CEC_OP_CDC_ERROR_CODE_OTHER:
		return "Other error";
	default:
		return "Unknown";
	}
}

std::string opcode2s(const struct cec_msg *msg)
{
	std::stringstream oss;
	__u8 opcode = msg->msg[1];

	if (msg->len == 1)
		return "MSG_POLL";

	if (opcode == CEC_MSG_CDC_MESSAGE) {
		__u8 cdc_opcode = msg->msg[4];

		for (unsigned i = 0; i < ARRAY_SIZE(cdcmsgtable); i++) {
			if (cdcmsgtable[i].opcode == cdc_opcode)
				return cdcmsgtable[i].name;
		}
		oss << "CDC: 0x" << std::hex << (unsigned)cdc_opcode;
		return oss.str();
	}

	for (unsigned i = 0; i < ARRAY_SIZE(msgtable); i++) {
		if (msgtable[i].opcode == opcode)
			return msgtable[i].name;
	}
	oss << "0x" << std::hex << (unsigned)opcode;
	return oss.str();
}

int cec_named_ioctl(struct node *node, const char *name,
		    unsigned long int request, void *parm)
{
	int retval;
	int e;
	struct cec_msg *msg = (struct cec_msg *)parm;
	__u8 opcode = 0;
	std::string opname;

	if (request == CEC_TRANSMIT) {
		opcode = cec_msg_opcode(msg);
		opname = opcode2s(msg);
	}

	retval = ioctl(node->fd, request, parm);

	if (request == CEC_RECEIVE) {
		opcode = cec_msg_opcode(msg);
		opname = opcode2s(msg);
	}

	e = retval == 0 ? 0 : errno;
	if (options[OptTrace] && (e || !show_info ||
			(request != CEC_TRANSMIT && request != CEC_RECEIVE))) {
		if (request == CEC_TRANSMIT)
			printf("\t\t%s: %s returned %d (%s)\n",
				opname.c_str(), name, retval, strerror(e));
		else
			printf("\t\t%s returned %d (%s)\n",
				name, retval, strerror(e));
	}

	if (!retval && request == CEC_TRANSMIT && show_info) {
		printf("\t\t%s: Sequence: %u Tx Timestamp: %s Length: %u",
		       opname.c_str(), msg->sequence, ts2s(msg->tx_ts).c_str(), msg->len);
		if (msg->rx_ts)
			printf("\n\t\t\tRx Timestamp: %s Approximate response time: %u ms",
			       ts2s(msg->rx_ts).c_str(),
			       response_time_ms(msg));
		if ((msg->tx_status & CEC_TX_STATUS_OK) &&
		    (msg->tx_status & (CEC_TX_STATUS_ARB_LOST | CEC_TX_STATUS_NACK |
				       CEC_TX_STATUS_LOW_DRIVE | CEC_TX_STATUS_ERROR)))
			printf("\n\t\t\tStatus: %s", status2s(*msg).c_str());
		printf("\n");
	}

	if (!retval && request == CEC_RECEIVE && show_info)
		printf("\t\t%s: Sequence: %u Rx Timestamp: %s Length: %u\n",
		       opname.c_str(), msg->sequence, ts2s(msg->rx_ts).c_str(), msg->len);

	if (!retval) {
		__u8 la = cec_msg_initiator(msg);

		/*
		 * TODO: The logic here might need to be re-evaluated.
		 *
		 * Currently a message is registered as recognized if
		 *     - We receive a reply that is not Feature Abort with
		 *       [Unrecognized Opcode] or [Undetermined]
		 *     - We manually receive (CEC_RECEIVE) and get a Feature Abort
		 *       with reason different than [Unrecognized Opcode] or
		 *       [Undetermined]
		 */
		if (request == CEC_TRANSMIT && msg->timeout > 0 &&
		    cec_msg_initiator(msg) != CEC_LOG_ADDR_UNREGISTERED &&
		    cec_msg_destination(msg) != CEC_LOG_ADDR_BROADCAST &&
		    (msg->tx_status & CEC_TX_STATUS_OK) &&
		    (msg->rx_status & CEC_RX_STATUS_OK)) {
			if (cec_msg_status_is_abort(msg) &&
			    (abort_reason(msg) == CEC_OP_ABORT_UNRECOGNIZED_OP ||
			     abort_reason(msg) == CEC_OP_ABORT_UNDETERMINED))
				node->remote[la].unrecognized_op[opcode] = true;
			else
				node->remote[la].recognized_op[opcode] = true;
		}

		if (request == CEC_RECEIVE &&
		    cec_msg_initiator(msg) != CEC_LOG_ADDR_UNREGISTERED &&
		    cec_msg_opcode(msg) == CEC_MSG_FEATURE_ABORT) {
			__u8 abort_msg = msg->msg[2];

			if (abort_reason(msg) == CEC_OP_ABORT_UNRECOGNIZED_OP ||
			    abort_reason(msg) == CEC_OP_ABORT_UNDETERMINED)
				node->remote[la].unrecognized_op[abort_msg] = true;
			else
				node->remote[la].recognized_op[abort_msg] = true;
		}
	}

	return retval == -1 ? e : (retval ? -1 : 0);
}

const char *ok(int res)
{
	static char buf[100];

	if (res == NOTSUPPORTED) {
		strcpy(buf, "OK (Not Supported)");
		res = 0;
	} else if (res == PRESUMED_OK) {
		strcpy(buf, "OK (Presumed)");
		res = 0;
	} else if (res == REFUSED) {
		strcpy(buf, "OK (Refused)");
		res = 0;
	} else
		strcpy(buf, "OK");
	tests_total++;
	if (res) {
		app_result = res;
		sprintf(buf, "FAIL");
	} else {
		tests_ok++;
	}
	return buf;
}

int check_0(const void *p, int len)
{
	const __u8 *q = (const __u8 *)p;

	while (len--)
		if (*q++)
			return 1;
	return 0;
}

#define TX_WAIT_FOR_HPD		10
#define TX_WAIT_FOR_HPD_RETURN	30

static bool wait_for_hpd(struct node *node, bool send_image_view_on)
{
	int fd = node->fd;
	int flags = fcntl(node->fd, F_GETFL);
	time_t t = time(NULL);

	fcntl(node->fd, F_SETFL, flags | O_NONBLOCK);
	for (;;) {
		struct timeval tv = { 1, 0 };
		fd_set ex_fds;
		int res;

		FD_ZERO(&ex_fds);
		FD_SET(fd, &ex_fds);
		res = select(fd + 1, NULL, NULL, &ex_fds, &tv);
		if (res < 0) {
			fail("select failed with error %d\n", errno);
			return false;
		}
		if (FD_ISSET(fd, &ex_fds)) {
			struct cec_event ev;

			res = doioctl(node, CEC_DQEVENT, &ev);
			if (!res && ev.event == CEC_EVENT_STATE_CHANGE &&
			    ev.state_change.log_addr_mask)
				break;
		}

		/*
		 * If the HPD doesn't return after TX_WAIT_FOR_HPD seconds,
		 * then send a IMAGE_VIEW_ON message (if possible).
		 */
		if (send_image_view_on && time(NULL) - t > TX_WAIT_FOR_HPD) {
			struct cec_msg image_view_on_msg;

			cec_msg_init(&image_view_on_msg, CEC_LOG_ADDR_UNREGISTERED,
				     CEC_LOG_ADDR_TV);
			cec_msg_image_view_on(&image_view_on_msg);
			// So the HPD is gone (possibly due to a standby), but
			// some TVs still have a working CEC bus, so send Image
			// View On to attempt to wake it up again.
			doioctl(node, CEC_TRANSMIT, &image_view_on_msg);
			send_image_view_on = false;
		}

		if (time(NULL) - t > TX_WAIT_FOR_HPD + TX_WAIT_FOR_HPD_RETURN) {
			fail("timed out after %d s waiting for HPD to return\n",
			     TX_WAIT_FOR_HPD + TX_WAIT_FOR_HPD_RETURN);
			return false;
		}
	}
	fcntl(node->fd, F_SETFL, flags);
	return true;
}

bool transmit_timeout(struct node *node, struct cec_msg *msg, unsigned timeout)
{
	struct cec_msg original_msg = *msg;
	__u8 opcode = cec_msg_opcode(msg);
	bool retried = false;
	int res;

	msg->timeout = timeout;
retry:
	res = doioctl(node, CEC_TRANSMIT, msg);
	if (res == ENODEV) {
		printf("Device was disconnected.\n");
		exit(1);
	}
	if (res == ENONET) {
		if (retried) {
			fail("HPD was lost twice, that can't be right\n");
			return false;
		}
		warn("HPD was lost, wait for it to come up again.\n");

		if (!wait_for_hpd(node, !(node->caps & CEC_CAP_NEEDS_HPD) &&
				  cec_msg_destination(msg) == CEC_LOG_ADDR_TV))
			return false;

		retried = true;
		goto retry;
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

static int poll_remote_devs(struct node *node)
{
	node->remote_la_mask = 0;
	if (!(node->caps & CEC_CAP_TRANSMIT))
		return 0;

	for (unsigned i = 0; i < 15; i++) {
		struct cec_msg msg;

		cec_msg_init(&msg, node->log_addr[0], i);

		fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
		if (msg.tx_status & CEC_TX_STATUS_OK)
			node->remote_la_mask |= 1 << i;
	}
	return 0;
}

static void topology_probe_device(struct node *node, unsigned i, unsigned la)
{
	struct cec_msg msg = { };
	bool ok;

	printf("\tSystem Information for device %d (%s) from device %d (%s):\n",
	       i, la2s(i), la, la2s(la));

	cec_msg_init(&msg, la, i);
	cec_msg_get_cec_version(&msg, true);
	ok = !transmit_timeout(node, &msg) || timed_out_or_abort(&msg);
	printf("\t\tCEC Version                : ");
	if (ok) {
		printf("%s\n", status2s(msg).c_str());
		node->remote[i].cec_version = CEC_OP_CEC_VERSION_1_4;
	}
	/* This needs to be kept in sync with newer CEC versions */
	else {
		node->remote[i].cec_version = msg.msg[2];
		if (msg.msg[2] < CEC_OP_CEC_VERSION_1_3A) {
			printf("< 1.3a (%x)\n", msg.msg[2]);
			warn("The reported CEC version is less than 1.3a. The device will be tested as a CEC 1.3a compliant device.\n");
		}
		else if (msg.msg[2] > CEC_OP_CEC_VERSION_2_0) {
			printf("> 2.0 (%x)\n", msg.msg[2]);
			warn("The reported CEC version is greater than 2.0. The device will be tested as a CEC 2.0 compliant device.\n");
		}
		else
			printf("%s\n", version2s(msg.msg[2]));
	}

	cec_msg_init(&msg, la, i);
	cec_msg_give_physical_addr(&msg, true);
	ok = !transmit_timeout(node, &msg) || timed_out_or_abort(&msg);
	printf("\t\tPhysical Address           : ");
	if (ok) {
		printf("%s\n", status2s(msg).c_str());
		node->remote[i].phys_addr = CEC_PHYS_ADDR_INVALID;
	}
	else {
		node->remote[i].phys_addr = (msg.msg[2] << 8) | msg.msg[3];
		printf("%x.%x.%x.%x\n",
		       cec_phys_addr_exp(node->remote[i].phys_addr));
		node->remote[i].prim_type = msg.msg[4];
		printf("\t\tPrimary Device Type        : %s\n",
		       prim_type2s(node->remote[i].prim_type));
	}

	cec_msg_init(&msg, la, i);
	cec_msg_give_device_vendor_id(&msg, true);
	ok = !transmit_timeout(node, &msg) || timed_out_or_abort(&msg);
	printf("\t\tVendor ID                  : ");
	if (ok)
		printf("%s\n", status2s(msg).c_str());
	else
		printf("0x%02x%02x%02x\n",
		       msg.msg[2], msg.msg[3], msg.msg[4]);

	cec_msg_init(&msg, la, i);
	cec_msg_give_osd_name(&msg, true);
	ok = !transmit_timeout(node, &msg) || timed_out_or_abort(&msg);
	printf("\t\tOSD Name                   : ");
	if (ok) {
		printf("%s\n", status2s(msg).c_str());
	} else {
		char osd_name[15];

		cec_ops_set_osd_name(&msg, osd_name);
		printf("'%s'\n", osd_name);
	}

	cec_msg_init(&msg, la, i);
	cec_msg_get_menu_language(&msg, true);
	if (transmit_timeout(node, &msg) && !timed_out_or_abort(&msg)) {
		char language[4];

		cec_ops_set_menu_language(&msg, language);
		language[3] = 0;
		printf("\t\tMenu Language              : %s\n", language);
	}

	cec_msg_init(&msg, la, i);
	cec_msg_give_device_power_status(&msg, true);
	ok = !transmit_timeout(node, &msg) || timed_out_or_abort(&msg);
	printf("\t\tPower Status               : ");
	if (ok) {
		printf("%s\n", status2s(msg).c_str());
	} else {
		__u8 pwr;

		cec_ops_report_power_status(&msg, &pwr);
		if (pwr >= 4)
			printf("Invalid\n");
		else {
			node->remote[i].has_power_status = true;
			node->remote[i].in_standby = pwr != CEC_OP_POWER_STATUS_ON;
			printf("%s\n", power_status2s(pwr));
		}
	}

	if (node->remote[i].cec_version < CEC_OP_CEC_VERSION_2_0)
		return;
	cec_msg_init(&msg, la, i);
	cec_msg_give_features(&msg, true);
	if (transmit_timeout(node, &msg) && !timed_out_or_abort(&msg)) {
		/* RC Profile and Device Features are assumed to be 1 byte. As of CEC 2.0 only
		   1 byte is used, but this might be extended in future versions. */
		__u8 cec_version, all_device_types;
		const __u8 *rc_profile = NULL, *dev_features = NULL;

		cec_ops_report_features(&msg, &cec_version, &all_device_types,
					&rc_profile, &dev_features);
		if (rc_profile == NULL || dev_features == NULL)
			return;
		node->remote[i].rc_profile = *rc_profile;
		node->remote[i].dev_features = *dev_features;
		node->remote[i].has_arc_rx =
			(*dev_features & CEC_OP_FEAT_DEV_SOURCE_HAS_ARC_RX) != 0;
		node->remote[i].has_arc_tx =
			(*dev_features & CEC_OP_FEAT_DEV_SINK_HAS_ARC_TX) != 0;
		node->remote[i].has_aud_rate =
			(*dev_features & CEC_OP_FEAT_DEV_HAS_SET_AUDIO_RATE) != 0;
		node->remote[i].has_deck_ctl =
			(*dev_features & CEC_OP_FEAT_DEV_HAS_DECK_CONTROL) != 0;
		node->remote[i].has_rec_tv =
			(*dev_features & CEC_OP_FEAT_DEV_HAS_RECORD_TV_SCREEN) != 0;
	}
}

int main(int argc, char **argv)
{
	const char *device = "/dev/cec0";	/* -d device */
	char short_options[26 * 2 * 2 + 1];
	int remote_la = -1;
	bool test_remote = false;
	unsigned test_tags = 0;
	int idx = 0;
	int fd = -1;
	int ch;
	int i;

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
		switch (ch) {
		case OptHelp:
			usage();
			return 0;
		case OptSetDevice:
			device = optarg;
			if (device[0] >= '0' && device[0] <= '9' && strlen(device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/cec%s", device);
				device = newdev;
			}
			break;
		case OptReplyThreshold:
			reply_threshold = strtoul(optarg, NULL, 0);
			break;
		case OptTimeout:
			long_timeout = strtoul(optarg, NULL, 0);
			break;
		case OptNoWarnings:
			show_warnings = false;
			break;
		case OptRemote:
			if (optarg) {
				remote_la = strtoul(optarg, NULL, 0);
				if (remote_la < 0 || remote_la > 15) {
					fprintf(stderr, "--test: invalid remote logical address\n");
					usage();
					return 1;
				}
			}
			test_remote = true;
			break;
		case OptVerbose:
			show_info = true;
			break;
		case ':':
			fprintf(stderr, "Option '%s' requires a value\n",
				argv[optind]);
			usage();
			return 1;
		case '?':
			if (argv[optind])
				fprintf(stderr, "Unknown argument '%s'\n", argv[optind]);
			usage();
			return 1;
		}
	}
	if (optind < argc) {
		printf("unknown arguments: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
		usage();
		return 1;
	}

	if ((fd = open(device, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", device,
			strerror(errno));
		exit(1);
	}

	struct node node = { };
	struct cec_caps caps = { };

	node.fd = fd;
	node.device = device;
	doioctl(&node, CEC_ADAP_G_CAPS, &caps);
	node.caps = caps.capabilities;
	node.available_log_addrs = caps.available_log_addrs;

	if (options[OptTestAudioRateControl])
		test_tags |= TAG_AUDIO_RATE_CONTROL;
	if (options[OptTestARCControl])
		test_tags |= TAG_ARC_CONTROL;
	if (options[OptTestCapDiscoveryControl])
		test_tags |= TAG_CAP_DISCOVERY_CONTROL;
	if (options[OptTestDeckControl])
		test_tags |= TAG_DECK_CONTROL;
	if (options[OptTestDeviceMenuControl])
		test_tags |= TAG_DEVICE_MENU_CONTROL;
	if (options[OptTestDeviceOSDTransfer])
		test_tags |= TAG_DEVICE_OSD_TRANSFER;
	if (options[OptTestDynamicAutoLipsync])
		test_tags |= TAG_DYNAMIC_AUTO_LIPSYNC;
	if (options[OptTestOSDDisplay])
		test_tags |= TAG_OSD_DISPLAY;
	if (options[OptTestOneTouchPlay])
		test_tags |= TAG_ONE_TOUCH_PLAY;
	if (options[OptTestOneTouchRecord])
		test_tags |= TAG_ONE_TOUCH_RECORD;
	if (options[OptTestPowerStatus])
		test_tags |= TAG_POWER_STATUS;
	if (options[OptTestRemoteControlPassthrough])
		test_tags |= TAG_REMOTE_CONTROL_PASSTHROUGH;
	if (options[OptTestRoutingControl])
		test_tags |= TAG_ROUTING_CONTROL;
	if (options[OptTestSystemAudioControl])
		test_tags |= TAG_SYSTEM_AUDIO_CONTROL;
	if (options[OptTestSystemInformation])
		test_tags |= TAG_SYSTEM_INFORMATION;
	if (options[OptTestTimerProgramming])
		test_tags |= TAG_TIMER_PROGRAMMING;
	if (options[OptTestTunerControl])
		test_tags |= TAG_TUNER_CONTROL;
	if (options[OptTestVendorSpecificCommands])
		test_tags |= TAG_VENDOR_SPECIFIC_COMMANDS;
	/* When code is added to the Standby/Resume test for waking up
	   other devices than TVs, the necessary tags should be added
	   here (probably Routing Control and/or RC Passthrough) */
	if (options[OptTestStandbyResume])
		test_tags |= TAG_POWER_STATUS | TAG_STANDBY_RESUME;

	if (!test_tags && !options[OptTestCore])
		test_tags = TAG_ALL;

	if (options[OptSkipTestAudioRateControl])
		test_tags &= ~TAG_AUDIO_RATE_CONTROL;
	if (options[OptSkipTestARCControl])
		test_tags &= ~TAG_ARC_CONTROL;
	if (options[OptSkipTestCapDiscoveryControl])
		test_tags &= ~TAG_CAP_DISCOVERY_CONTROL;
	if (options[OptSkipTestDeckControl])
		test_tags &= ~TAG_DECK_CONTROL;
	if (options[OptSkipTestDeviceMenuControl])
		test_tags &= ~TAG_DEVICE_MENU_CONTROL;
	if (options[OptSkipTestDeviceOSDTransfer])
		test_tags &= ~TAG_DEVICE_OSD_TRANSFER;
	if (options[OptSkipTestDynamicAutoLipsync])
		test_tags &= ~TAG_DYNAMIC_AUTO_LIPSYNC;
	if (options[OptSkipTestOSDDisplay])
		test_tags &= ~TAG_OSD_DISPLAY;
	if (options[OptSkipTestOneTouchPlay])
		test_tags &= ~TAG_ONE_TOUCH_PLAY;
	if (options[OptSkipTestOneTouchRecord])
		test_tags &= ~TAG_ONE_TOUCH_RECORD;
	if (options[OptSkipTestPowerStatus])
		test_tags &= ~TAG_POWER_STATUS;
	if (options[OptSkipTestRemoteControlPassthrough])
		test_tags &= ~TAG_REMOTE_CONTROL_PASSTHROUGH;
	if (options[OptSkipTestRoutingControl])
		test_tags &= ~TAG_ROUTING_CONTROL;
	if (options[OptSkipTestSystemAudioControl])
		test_tags &= ~TAG_SYSTEM_AUDIO_CONTROL;
	if (options[OptSkipTestSystemInformation])
		test_tags &= ~TAG_SYSTEM_INFORMATION;
	if (options[OptSkipTestTimerProgramming])
		test_tags &= ~TAG_TIMER_PROGRAMMING;
	if (options[OptSkipTestTunerControl])
		test_tags &= ~TAG_TUNER_CONTROL;
	if (options[OptSkipTestVendorSpecificCommands])
		test_tags &= ~TAG_VENDOR_SPECIFIC_COMMANDS;
	if (options[OptSkipTestStandbyResume])
		test_tags &= ~(TAG_POWER_STATUS | TAG_STANDBY_RESUME);

	if (options[OptInteractive])
		test_tags |= TAG_INTERACTIVE;

#ifdef SHA
#define STR(x) #x
#define STRING(x) STR(x)
	printf("cec-compliance SHA                 : %s\n", STRING(SHA));
#else
	printf("cec-compliance SHA                 : not available\n");
#endif

	node.phys_addr = CEC_PHYS_ADDR_INVALID;
	doioctl(&node, CEC_ADAP_G_PHYS_ADDR, &node.phys_addr);

	struct cec_log_addrs laddrs = { };
	doioctl(&node, CEC_ADAP_G_LOG_ADDRS, &laddrs);

	if (node.phys_addr == CEC_PHYS_ADDR_INVALID &&
	    !(node.caps & (CEC_CAP_PHYS_ADDR | CEC_CAP_NEEDS_HPD)) &&
	    laddrs.num_log_addrs) {
		struct cec_msg msg;

		/*
		 * Special corner case: if PA is invalid, then you can still try
		 * to poll a TV. If found, try to wake it up.
		 */
		cec_msg_init(&msg, CEC_LOG_ADDR_UNREGISTERED, CEC_LOG_ADDR_TV);

		fail_on_test(doioctl(&node, CEC_TRANSMIT, &msg));
		if (msg.tx_status & CEC_TX_STATUS_OK) {
			unsigned cnt = 0;

			cec_msg_image_view_on(&msg);
			fail_on_test(doioctl(&node, CEC_TRANSMIT, &msg));
			while ((msg.tx_status & CEC_TX_STATUS_OK) && cnt++ <= long_timeout) {
				fail_on_test(doioctl(&node, CEC_ADAP_G_PHYS_ADDR, &node.phys_addr));
				if (node.phys_addr != CEC_PHYS_ADDR_INVALID) {
					doioctl(&node, CEC_ADAP_G_LOG_ADDRS, &laddrs);
					break;
				}
				sleep(1);
			}
		}

	}

	cec_driver_info(caps, laddrs, node.phys_addr);

	bool missing_pa = node.phys_addr == CEC_PHYS_ADDR_INVALID && (node.caps & CEC_CAP_PHYS_ADDR);
	bool missing_la = laddrs.num_log_addrs == 0 && (node.caps & CEC_CAP_LOG_ADDRS);

	if (missing_la || missing_pa)
		printf("\n");
	if (missing_pa)
		fprintf(stderr, "FAIL: missing physical address, use cec-ctl to configure this\n");
	if (missing_la)
		fprintf(stderr, "FAIL: missing logical address(es), use cec-ctl to configure this\n");
	if (missing_la || missing_pa)
		exit(-1);

	printf("\nCompliance test for device %s:\n\n", device);
	printf("    The test results mean the following:\n"
	       "        OK                  Supported correctly by the device.\n"
	       "        OK (Not Supported)  Not supported and not mandatory for the device.\n"
	       "        OK (Presumed)       Presumably supported.  Manually check to confirm.\n"
	       "        OK (Unexpected)     Supported correctly but is not expected to be supported for this device.\n"
	       "        OK (Refused)        Supported by the device, but was refused.\n"
	       "        FAIL                Failed and was expected to be supported by this device.\n\n");

	node.has_cec20 = laddrs.cec_version >= CEC_OP_CEC_VERSION_2_0;
	node.num_log_addrs = laddrs.num_log_addrs;
	memcpy(node.log_addr, laddrs.log_addr, laddrs.num_log_addrs);
	node.adap_la_mask = laddrs.log_addr_mask;

	printf("Find remote devices:\n");
	printf("\tPolling: %s\n", ok(poll_remote_devs(&node)));

	if (options[OptTestAdapter])
		testAdapter(node, laddrs, device);
	printf("\n");

	printf("Network topology:\n");
	for (unsigned i = 0; i < 15; i++)
		if (node.remote_la_mask & (1 << i))
			topology_probe_device(&node, i, node.log_addr[0]);
	printf("\n");

	unsigned remote_la_mask = node.remote_la_mask;

	if (remote_la >= 0)
		remote_la_mask = 1 << remote_la;

	if (test_remote) {
		for (unsigned from = 0; from <= 15; from++) {
			if (!(node.adap_la_mask & (1 << from)))
				continue;
			for (unsigned to = 0; to <= 15; to++)
				if (!(node.adap_la_mask & (1 << to)) &&
				    (remote_la_mask & (1 << to)) &&
				    node.remote[to].phys_addr != CEC_PHYS_ADDR_INVALID)
					testRemote(&node, from, to, test_tags, options[OptInteractive]);
		}
	}

	/* Final test report */

	close(fd);

	printf("Total: %d, Succeeded: %d, Failed: %d, Warnings: %d\n",
	       tests_total, tests_ok, tests_total - tests_ok, warnings);
	exit(app_result);
}
