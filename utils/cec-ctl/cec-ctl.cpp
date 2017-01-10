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
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <cerrno>
#include <string>
#include <vector>
#include <algorithm>
#include <linux/cec-funcs.h>

#ifdef __ANDROID__
#include <android-config.h>
#else
#include <config.h>
#endif

#define CEC_MAX_ARGS 16

#define xstr(s) str(s)
#define str(s) #s

struct cec_enum_values {
	const char *type_name;
	__u8 value;
};

enum cec_types {
	CEC_TYPE_U8,
	CEC_TYPE_U16,
	CEC_TYPE_U32,
	CEC_TYPE_STRING,
	CEC_TYPE_ENUM,
};

struct arg {
	enum cec_types type;
	__u8 num_enum_values;
	const struct cec_enum_values *values;
};

static const struct arg arg_u8 = {
	CEC_TYPE_U8,
};

static const struct arg arg_u16 = {
	CEC_TYPE_U16,
};

static const struct arg arg_u32 = {
	CEC_TYPE_U32,
};

static const struct arg arg_string = {
	CEC_TYPE_STRING,
};

static const struct cec_enum_values type_ui_cmd[] = {
	{ "Select", 0x00 },
	{ "Up", 0x01 },
	{ "Down", 0x02 },
	{ "Left", 0x03 },
	{ "Right", 0x04 },
	{ "Right-Up", 0x05 },
	{ "Right-Down", 0x06 },
	{ "Left-Up", 0x07 },
	{ "Left-Down", 0x08 },
	{ "Device Root Menu", 0x09 },
	{ "Device Setup Menu", 0x0a },
	{ "Contents Menu", 0x0b },
	{ "Favorite Menu", 0x0c },
	{ "Back", 0x0d },
	{ "Media Top Menu", 0x10 },
	{ "Media Context-sensitive Menu", 0x11 },
	{ "Number Entry Mode", 0x1d },
	{ "Number 11", 0x1e },
	{ "Number 12", 0x1f },
	{ "Number 0 or Number 10", 0x20 },
	{ "Number 1", 0x21 },
	{ "Number 2", 0x22 },
	{ "Number 3", 0x23 },
	{ "Number 4", 0x24 },
	{ "Number 5", 0x25 },
	{ "Number 6", 0x26 },
	{ "Number 7", 0x27 },
	{ "Number 8", 0x28 },
	{ "Number 9", 0x29 },
	{ "Dot", 0x2a },
	{ "Enter", 0x2b },
	{ "Clear", 0x2c },
	{ "Next Favorite", 0x2f },
	{ "Channel Up", 0x30 },
	{ "Channel Down", 0x31 },
	{ "Previous Channel", 0x32 },
	{ "Sound Select", 0x33 },
	{ "Input Select", 0x34 },
	{ "Display Information", 0x35 },
	{ "Help", 0x36 },
	{ "Page Up", 0x37 },
	{ "Page Down", 0x38 },
	{ "Power", 0x40 },
	{ "Volume Up", 0x41 },
	{ "Volume Down", 0x42 },
	{ "Mute", 0x43 },
	{ "Play", 0x44 },
	{ "Stop", 0x45 },
	{ "Pause", 0x46 },
	{ "Record", 0x47 },
	{ "Rewind", 0x48 },
	{ "Fast forward", 0x49 },
	{ "Eject", 0x4a },
	{ "Skip Forward", 0x4b },
	{ "Skip Backward", 0x4c },
	{ "Stop-Record", 0x4d },
	{ "Pause-Record", 0x4e },
	{ "Angle", 0x50 },
	{ "Sub picture", 0x51 },
	{ "Video on Demand", 0x52 },
	{ "Electronic Program Guide", 0x53 },
	{ "Timer Programming", 0x54 },
	{ "Initial Configuration", 0x55 },
	{ "Select Broadcast Type", 0x56 },
	{ "Select Sound Presentation", 0x57 },
	{ "Audio Description", 0x58 },
	{ "Internet", 0x59 },
	{ "3D Mode", 0x5a },
	{ "Play Function", 0x60 },
	{ "Pause-Play Function", 0x61 },
	{ "Record Function", 0x62 },
	{ "Pause-Record Function", 0x63 },
	{ "Stop Function", 0x64 },
	{ "Mute Function", 0x65 },
	{ "Restore Volume Function", 0x66 },
	{ "Tune Function", 0x67 },
	{ "Select Media Function", 0x68 },
	{ "Select A/V Input Function", 0x69 },
	{ "Select Audio Input Function", 0x6a },
	{ "Power Toggle Function", 0x6b },
	{ "Power Off Function", 0x6c },
	{ "Power On Function", 0x6d },
	{ "F1 (Blue)", 0x71 },
	{ "F2 (Red)", 0x72 },
	{ "F3 (Green)", 0x73 },
	{ "F4 (Yellow)", 0x74 },
	{ "F5", 0x75 },
	{ "Data", 0x76 },
};

static const struct arg arg_rc_ui_cmd = {
	CEC_TYPE_ENUM, sizeof(type_ui_cmd) / sizeof(type_ui_cmd[0]), type_ui_cmd
};

struct message {
	__u8 msg;
	unsigned option;
	__u8 num_args;
	const char *arg_names[CEC_MAX_ARGS+1];
	const struct arg *args[CEC_MAX_ARGS];
	const char *msg_name;
};

static struct cec_op_digital_service_id *args2digital_service_id(__u8 service_id_method,
								 __u8 dig_bcast_system,
								 __u16 transport_id,
								 __u16 service_id,
								 __u16 orig_network_id,
								 __u16 program_number,
								 __u8 channel_number_fmt,
								 __u16 major,
								 __u16 minor)
{
	static struct cec_op_digital_service_id dsid;

	dsid.service_id_method = service_id_method;
	dsid.dig_bcast_system = dig_bcast_system;
	if (service_id_method == CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL) {
		dsid.channel.channel_number_fmt = channel_number_fmt;
		dsid.channel.major = major;
		dsid.channel.minor = minor;
		return &dsid;
	}
	switch (dig_bcast_system) {
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_GEN:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_CABLE:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_SAT:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_T:
		dsid.atsc.transport_id = transport_id;
		dsid.atsc.program_number = program_number;
		break;
	default:
		dsid.dvb.transport_id = transport_id;
		dsid.dvb.service_id = service_id;
		dsid.dvb.orig_network_id = orig_network_id;
		break;
	}
	return &dsid;
}

static struct cec_op_ui_command *args2ui_command(__u8 ui_cmd,
						 __u8 has_opt_arg,
						 __u8 play_mode,
						 __u8 ui_function_media,
						 __u8 ui_function_select_av_input,
						 __u8 ui_function_select_audio_input,
						 __u8 ui_bcast_type,
						 __u8 ui_snd_pres_ctl,
						 __u8 channel_number_fmt,
						 __u16 major,
						 __u16 minor)
{
	static struct cec_op_ui_command ui_command;

	ui_command.ui_cmd = ui_cmd;
	ui_command.has_opt_arg = has_opt_arg;
	if (!has_opt_arg)
		return &ui_command;
	switch (ui_cmd) {
	case 0x56:
		ui_command.ui_broadcast_type = ui_bcast_type;
		break;
	case 0x57:
		ui_command.ui_sound_presentation_control = ui_snd_pres_ctl;
		break;
	case 0x60:
		ui_command.play_mode = play_mode;
		break;
	case 0x68:
		ui_command.ui_function_media = ui_function_media;
		break;
	case 0x69:
		ui_command.ui_function_select_av_input = ui_function_select_av_input;
		break;
	case 0x6a:
		ui_command.ui_function_select_audio_input = ui_function_select_audio_input;
		break;
	case 0x67:
		ui_command.channel_identifier.channel_number_fmt = channel_number_fmt;
		ui_command.channel_identifier.major = major;
		ui_command.channel_identifier.minor = minor;
		break;
	default:
		ui_command.has_opt_arg = false;
		break;
	}
	return &ui_command;
}

static __u32 *args2short_descrs(__u32 descriptor1,
				__u32 descriptor2,
				__u32 descriptor3,
				__u32 descriptor4)
{
	static __u32 descriptors[4];

	descriptors[0] = descriptor1;
	descriptors[1] = descriptor2;
	descriptors[2] = descriptor3;
	descriptors[3] = descriptor4;
	return descriptors;
}

static __u8 *args2short_aud_fmt_ids(__u8 audio_format_id1,
				    __u8 audio_format_id2,
				    __u8 audio_format_id3,
				    __u8 audio_format_id4)
{
	static __u8 audio_format_ids[4];

	audio_format_ids[0] = audio_format_id1;
	audio_format_ids[1] = audio_format_id2;
	audio_format_ids[2] = audio_format_id3;
	audio_format_ids[3] = audio_format_id4;
	return audio_format_ids;
}

static __u8 *args2short_aud_fmt_codes(__u8 audio_format_code1,
				      __u8 audio_format_code2,
				      __u8 audio_format_code3,
				      __u8 audio_format_code4)
{
	static __u8 audio_format_codes[4];

	audio_format_codes[0] = audio_format_code1;
	audio_format_codes[1] = audio_format_code2;
	audio_format_codes[2] = audio_format_code3;
	audio_format_codes[3] = audio_format_code4;
	return audio_format_codes;
}

static int parse_subopt(char **subs, const char * const *subopts, char **value)
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

static unsigned parse_enum(const char *value, const struct arg *a)
{
	if (isdigit(*value))
		return strtoul(value, NULL, 0);
	for (int i = 0; i < a->num_enum_values; i++) {
		if (!strcmp(value, a->values[i].type_name))
			return a->values[i].value;
	}
	return 0;
}

static unsigned parse_phys_addr(const char *value)
{
	unsigned p1, p2, p3, p4;

	if (!strchr(value, '.'))
		return strtoul(value, NULL, 0);
	if (sscanf(value, "%x.%x.%x.%x", &p1, &p2, &p3, &p4) != 4) {
		fprintf(stderr, "Expected a physical address of the form x.x.x.x\n");
		return 0;
	}
	if (p1 > 0xf || p2 > 0xf || p3 > 0xf || p4 > 0xf) {
		fprintf(stderr, "Physical address components should never be larger than 0xf\n");
		return 0;
	}
	return (p1 << 12) | (p2 << 8) | (p3 << 4) | p4;
}

static char options[512];

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

static std::string status2s(const struct cec_msg &msg)
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

static void log_arg(const struct arg *arg, const char *arg_name, __u32 val)
{
	unsigned i;

	switch (arg->type) {
	case CEC_TYPE_ENUM:
		for (i = 0; i < arg->num_enum_values; i++) {
			if (arg->values[i].value == val) {
				printf("\t%s: %s (0x%02x)\n", arg_name,
				       arg->values[i].type_name, val);
				return;
			}
		}
		/* fall through */
	case CEC_TYPE_U8:
		printf("\t%s: %u (0x%02x)\n", arg_name, val, val);
		return;
	case CEC_TYPE_U16:
		if (strstr(arg_name, "phys-addr"))
			printf("\t%s: %x.%x.%x.%x\n", arg_name,
			       val >> 12, (val >> 8) & 0xf, (val >> 4) & 0xf, val & 0xf);
		else
			printf("\t%s: %u (0x%04x)\n", arg_name, val, val);
		return;
	case CEC_TYPE_U32:
		printf("\t%s: %u (0x%08x)\n", arg_name, val, val);
		return;
	default:
		break;
	}
	printf("\t%s: unknown type\n", arg_name);
}

static void log_arg(const struct arg *arg, const char *arg_name,
		    const char *s)
{
	switch (arg->type) {
	case CEC_TYPE_STRING:
		printf("\t%s: %s\n", arg_name, s);
		return;
	default:
		break;
	}
	printf("\t%s: unknown type\n", arg_name);
}

static const struct cec_enum_values type_rec_src_type[] = {
	{ "own", CEC_OP_RECORD_SRC_OWN },
	{ "digital", CEC_OP_RECORD_SRC_DIGITAL },
	{ "analog", CEC_OP_RECORD_SRC_ANALOG },
	{ "ext-plug", CEC_OP_RECORD_SRC_EXT_PLUG },
	{ "ext-phys-addr", CEC_OP_RECORD_SRC_EXT_PHYS_ADDR },
};

static const struct arg arg_rec_src_type = {
	CEC_TYPE_ENUM, 5, type_rec_src_type
};

static void log_digital(const char *arg_name, const struct cec_op_digital_service_id *digital);
static void log_rec_src(const char *arg_name, const struct cec_op_record_src *rec_src);
static void log_tuner_dev_info(const char *arg_name, const struct cec_op_tuner_device_info *tuner_dev_info);
static void log_features(const struct arg *arg, const char *arg_name, const __u8 *p);
static void log_ui_command(const char *arg_name, const struct cec_op_ui_command *ui_cmd);
static void log_descriptors(const char *arg_name, unsigned num, const __u32 *descriptors);
static void log_u8_array(const char *arg_name, unsigned num, const __u8 *vals);
static void log_unknown_msg(const struct cec_msg *msg);

#define VENDOR_EXTRA \
	"  --vendor-command=payload=<byte>[:<byte>]*\n" \
	"                                  Send VENDOR_COMMAND message (" xstr(CEC_MSG_VENDOR_COMMAND) ")\n" \
	"  --vendor-command-with-id=vendor-id=<val>,cmd=<byte>[:<byte>]*\n" \
	"                                  Send VENDOR_COMMAND_WITH_ID message (" xstr(CEC_MSG_VENDOR_COMMAND_WITH_ID) ")\n" \
	"  --vendor-remote-button-down=rc-code=<byte>[:<byte>]*\n" \
	"                                  Send VENDOR_REMOTE_BUTTON_DOWN message (" xstr(CEC_MSG_VENDOR_REMOTE_BUTTON_DOWN) ")\n" \
	"  --custom-command=cmd=<byte>,payload=<byte>[:<byte>]*\n" \
	"                                  Send custom message\n"

#include "cec-ctl-gen.h"

static void log_digital(const char *arg_name, const struct cec_op_digital_service_id *digital)
{
	log_arg(&arg_service_id_method, "service-id-method", digital->service_id_method);
	log_arg(&arg_dig_bcast_system, "dig-bcast-system", digital->dig_bcast_system);
	if (digital->service_id_method == CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL) {
		log_arg(&arg_channel_number_fmt, "channel-number-fmt", digital->channel.channel_number_fmt);
		log_arg(&arg_u16, "major", digital->channel.major);
		log_arg(&arg_u16, "minor", digital->channel.minor);
		return;
	}

	switch (digital->dig_bcast_system) {
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_GEN:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_CABLE:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_SAT:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_T:
		log_arg(&arg_u16, "transport-id", digital->atsc.transport_id);
		log_arg(&arg_u16, "program-number", digital->atsc.program_number);
		break;
	default:
		log_arg(&arg_u16, "transport-id", digital->dvb.transport_id);
		log_arg(&arg_u16, "service-id", digital->dvb.service_id);
		log_arg(&arg_u16, "orig-network-id", digital->dvb.orig_network_id);
		break;
	}
}

static void log_rec_src(const char *arg_name, const struct cec_op_record_src *rec_src)
{
	log_arg(&arg_rec_src_type, "rec-src-type", rec_src->type);
	switch (rec_src->type) {
	case CEC_OP_RECORD_SRC_OWN:
	default:
		break;
	case CEC_OP_RECORD_SRC_DIGITAL:
		log_digital(arg_name, &rec_src->digital);
		break;
	case CEC_OP_RECORD_SRC_ANALOG:
		log_arg(&arg_ana_bcast_type, "ana-bcast-type", rec_src->analog.ana_bcast_type);
		log_arg(&arg_u16, "ana-freq", rec_src->analog.ana_freq);
		log_arg(&arg_bcast_system, "bcast-system", rec_src->analog.bcast_system);
		break;
	case CEC_OP_RECORD_SRC_EXT_PLUG:
		log_arg(&arg_u8, "plug", rec_src->ext_plug.plug);
		break;
	case CEC_OP_RECORD_SRC_EXT_PHYS_ADDR:
		log_arg(&arg_u16, "phys-addr", rec_src->ext_phys_addr.phys_addr);
		break;
	}
}

static void log_tuner_dev_info(const char *arg_name, const struct cec_op_tuner_device_info *tuner_dev_info)
{
	log_arg(&arg_rec_flag, "rec-flag", tuner_dev_info->rec_flag);
	log_arg(&arg_tuner_display_info, "tuner-display-info", tuner_dev_info->tuner_display_info);
	if (tuner_dev_info->is_analog) {
		log_arg(&arg_ana_bcast_type, "ana-bcast-type", tuner_dev_info->analog.ana_bcast_type);
		log_arg(&arg_u16, "ana-freq", tuner_dev_info->analog.ana_freq);
		log_arg(&arg_bcast_system, "bcast-system", tuner_dev_info->analog.bcast_system);
	} else {
		log_digital(arg_name, &tuner_dev_info->digital);
	}
}

static void log_features(const struct arg *arg,
			 const char *arg_name, const __u8 *p)
{
	do {
		log_arg(arg, arg_name, (__u32)((*p) & ~CEC_OP_FEAT_EXT));
	} while ((*p++) & CEC_OP_FEAT_EXT);
}

static void log_ui_command(const char *arg_name,
			   const struct cec_op_ui_command *ui_cmd)
{
	log_arg(&arg_rc_ui_cmd, arg_name, ui_cmd->ui_cmd);
	if (!ui_cmd->has_opt_arg)
		return;
	switch (ui_cmd->ui_cmd) {
	case 0x56:
		log_arg(&arg_ui_bcast_type, "ui-broadcast-type",
			ui_cmd->ui_broadcast_type);
		break;
	case 0x57:
		log_arg(&arg_ui_snd_pres_ctl, "ui-sound-presentation-control",
			ui_cmd->ui_sound_presentation_control);
		break;
	case 0x60:
		log_arg(&arg_u8, "play-mode", ui_cmd->play_mode);
		break;
	case 0x68:
		log_arg(&arg_u8, "ui-function-media", ui_cmd->ui_function_media);
		break;
	case 0x69:
		log_arg(&arg_u8, "ui-function-select-av-input", ui_cmd->ui_function_select_av_input);
		break;
	case 0x6a:
		log_arg(&arg_u8, "ui-function-select-audio-input", ui_cmd->ui_function_select_audio_input);
		break;
	case 0x67:
		log_arg(&arg_channel_number_fmt, "channel-number-fmt",
			ui_cmd->channel_identifier.channel_number_fmt);
		log_arg(&arg_u16, "major", ui_cmd->channel_identifier.major);
		log_arg(&arg_u16, "minor", ui_cmd->channel_identifier.minor);
		break;
	}
}

static void log_descriptors(const char *arg_name, unsigned num, const __u32 *descriptors)
{
	for (unsigned i = 0; i < num; i++)
		log_arg(&arg_u32, arg_name, descriptors[i]);
}

static void log_u8_array(const char *arg_name, unsigned num, const __u8 *vals)
{
	for (unsigned i = 0; i < num; i++)
		log_arg(&arg_u8, arg_name, vals[i]);
}

/* Short option list

   Please keep in alphabetical order.
   That makes it easier to see which short options are still free.

   In general the lower case is used to set something and the upper
   case is used to retrieve a setting. */
enum Option {
	OptClear = 'C',
	OptSetDevice = 'd',
	OptFrom = 'f',
	OptHelp = 'h',
	OptMonitor = 'm',
	OptMonitorAll = 'M',
	OptNoReply = 'n',
	OptOsdName = 'o',
	OptPhysAddr = 'p',
	OptShowTopology = 'S',
	OptTo = 't',
	OptTrace = 'T',
	OptVerbose = 'v',
	OptVendorID = 'V',

	OptTV = 128,
	OptRecord,
	OptTuner,
	OptPlayback,
	OptAudio,
	OptProcessor,
	OptSwitch,
	OptCDCOnly,
	OptUnregistered,
	OptCECVersion1_4,
	OptAllowUnregFallback,
	OptNoRC,
	OptReplyToFollowers,
	OptTimeout,
	OptListUICommands,
	OptRcTVProfile1,
	OptRcTVProfile2,
	OptRcTVProfile3,
	OptRcTVProfile4,
	OptRcSrcDevRoot,
	OptRcSrcDevSetup,
	OptRcSrcContents,
	OptRcSrcMediaTop,
	OptRcSrcMediaContext,
	OptFeatRecordTVScreen,
	OptFeatSetOSDString,
	OptFeatDeckControl,
	OptFeatSetAudioRate,
	OptFeatSinkHasARCTx,
	OptFeatSourceHasARCRx,
	OptVendorCommand = 508,
	OptVendorCommandWithID,
	OptVendorRemoteButtonDown,
	OptCustomCommand,
};

struct node {
	int fd;
	const char *device;
	unsigned caps;
	unsigned available_log_addrs;
	unsigned num_log_addrs;
	__u8 log_addr[CEC_MAX_LOG_ADDRS];
};

#define doioctl(n, r, p) cec_named_ioctl((n)->fd, #r, r, p)

bool show_info;

typedef std::vector<cec_msg> msg_vec;

static const struct message *opt2message[OptLast - OptMessages];

static void init_messages()
{
	for (unsigned i = 0; messages[i].msg_name; i++)
		opt2message[messages[i].option - OptMessages] = &messages[i];
}

static struct option long_options[] = {
	{ "device", required_argument, 0, OptSetDevice },
	{ "help", no_argument, 0, OptHelp },
	{ "trace", no_argument, 0, OptTrace },
	{ "verbose", no_argument, 0, OptVerbose },
	{ "osd-name", required_argument, 0, OptOsdName },
	{ "phys-addr", required_argument, 0, OptPhysAddr },
	{ "vendor-id", required_argument, 0, OptVendorID },
	{ "cec-version-1.4", no_argument, 0, OptCECVersion1_4 },
	{ "allow-unreg-fallback", no_argument, 0, OptAllowUnregFallback },
	{ "no-rc-passthrough", no_argument, 0, OptNoRC },
	{ "reply-to-followers", no_argument, 0, OptReplyToFollowers },
	{ "timeout", required_argument, 0, OptTimeout },
	{ "clear", no_argument, 0, OptClear },
	{ "monitor", no_argument, 0, OptMonitor },
	{ "monitor-all", no_argument, 0, OptMonitorAll },
	{ "no-reply", no_argument, 0, OptNoReply },
	{ "to", required_argument, 0, OptTo },
	{ "from", required_argument, 0, OptFrom },
	{ "show-topology", no_argument, 0, OptShowTopology },
	{ "list-ui-commands", no_argument, 0, OptListUICommands },
	{ "rc-tv-profile-1", no_argument, 0, OptRcTVProfile1 },
	{ "rc-tv-profile-2", no_argument, 0, OptRcTVProfile2 },
	{ "rc-tv-profile-3", no_argument, 0, OptRcTVProfile3 },
	{ "rc-tv-profile-4", no_argument, 0, OptRcTVProfile4 },
	{ "rc-src-dev-root", no_argument, 0, OptRcSrcDevRoot },
	{ "rc-src-dev-setup", no_argument, 0, OptRcSrcDevSetup },
	{ "rc-src-contents", no_argument, 0, OptRcSrcContents },
	{ "rc-src-media-top", no_argument, 0, OptRcSrcMediaTop },
	{ "rc-src-media-context", no_argument, 0, OptRcSrcMediaContext },
	{ "feat-record-tv-screen", no_argument, 0, OptFeatRecordTVScreen },
	{ "feat-set-osd-string", no_argument, 0, OptFeatSetOSDString },
	{ "feat-deck-control", no_argument, 0, OptFeatDeckControl },
	{ "feat-set-audio-rate", no_argument, 0, OptFeatSetAudioRate },
	{ "feat-sink-has-arc-tx", no_argument, 0, OptFeatSinkHasARCTx },
	{ "feat-source-has-arc-rx", no_argument, 0, OptFeatSourceHasARCRx },

	{ "tv", no_argument, 0, OptTV },
	{ "record", no_argument, 0, OptRecord },
	{ "tuner", no_argument, 0, OptTuner },
	{ "playback", no_argument, 0, OptPlayback },
	{ "audio", no_argument, 0, OptAudio },
	{ "processor", no_argument, 0, OptProcessor },
	{ "switch", no_argument, 0, OptSwitch },
	{ "cdc-only", no_argument, 0, OptCDCOnly },
	{ "unregistered", no_argument, 0, OptUnregistered },
	{ "help-all", no_argument, 0, OptHelpAll },

	CEC_LONG_OPTS
	{ "vendor-remote-button-down", required_argument, 0, OptVendorRemoteButtonDown }, \
	{ "vendor-command-with-id", required_argument, 0, OptVendorCommandWithID }, \
	{ "vendor-command", required_argument, 0, OptVendorCommand }, \
	{ "custom-command", required_argument, 0, OptCustomCommand }, \

	{ 0, 0, 0, 0 }
};

static void usage(void)
{
	printf("Usage:\n"
	       "  -d, --device=<dev>       Use device <dev> instead of /dev/cec0\n"
	       "                           If <dev> starts with a digit, then /dev/cec<dev> is used.\n"
	       "  -p, --phys-addr=<addr>   Use this physical address\n"
	       "  -o, --osd-name=<name>    Use this OSD name\n"
	       "  -V, --vendor-id=<id>     Use this vendor ID\n"
	       "  -C, --clear              Clear all logical addresses\n"
	       "  -m, --monitor            Monitor CEC traffic\n"
	       "  -M, --monitor-all        Monitor all CEC traffic\n"
	       "  -n, --no-reply           Don't wait for a reply\n"
	       "  -t, --to=<la>            Send message to the given logical address\n"
	       "  -f, --from=<la>          Send message from the given logical address\n"
	       "                           By default use the first assigned logical address\n"
	       "  -S, --show-topology      Show the CEC topology\n"
	       "  -h, --help               Display this help message\n"
	       "  --help-all               Show all help messages\n"
	       "  -T, --trace              Trace all called ioctls\n"
	       "  -v, --verbose            Turn on verbose reporting\n"
	       "  --cec-version-1.4        Use CEC Version 1.4 instead of 2.0\n"
	       "  --allow-unreg-fallback   Allow fallback to Unregistered\n"
	       "  --no-rc-passthrough      Disable the RC passthrough\n"
	       "  --reply-to-followers     The reply will be sent to followers as well\n"
	       "  --timeout=<ms>           Set the reply timeout in milliseconds (default is 1000 ms)\n"
	       "  --list-ui-commands       List all UI commands that can be used with --user-control-pressed\n"
	       "\n"
	       "  --tv                     This is a TV\n"
	       "  --record                 This is a recording and playback device\n"
	       "  --tuner                  This is a tuner device\n"
	       "  --playback               This is a playback device\n"
	       "  --audio                  This is an audio system device\n"
	       "  --processor              This is a processor device\n"
	       "  --switch                 This is a pure CEC switch\n"
	       "  --cdc-only               This is a CDC-only device\n"
	       "  --unregistered           This is an unregistered device\n"
	       "\n"
	       "  --feat-record-tv-screen  Signal the Record TV Screen feature\n"
	       "  --feat-set-osd-string    Signal the Set OSD String feature\n"
	       "  --feat-deck-control      Signal the Deck Control feature\n"
	       "  --feat-set-audio-rate    Signal the Set Audio Rate feature\n"
	       "  --feat-sink-has-arc-tx   Signal the sink ARC Tx feature\n"
	       "  --feat-source-has-arc-rx Signal the source ARC Rx feature\n"
	       "\n"
	       "  --rc-tv-profile-1        Signal RC TV Profile 1\n"
	       "  --rc-tv-profile-2        Signal RC TV Profile 2\n"
	       "  --rc-tv-profile-3        Signal RC TV Profile 3\n"
	       "  --rc-tv-profile-4        Signal RC TV Profile 4\n"
	       "\n"
	       "  --rc-src-dev-root        Signal that the RC source has a Dev Root Menu\n"
	       "  --rc-src-dev-setup       Signal that the RC source has a Dev Setup Menu\n"
	       "  --rc-src-contents        Signal that the RC source has a Contents Menu\n"
	       "  --rc-src-media-top       Signal that the RC source has a Media Top Menu\n"
	       "  --rc-src-media-context   Signal that the RC source has a Media Context Menu\n"
	       "\n"
	       CEC_USAGE
	       );
}

static std::string caps2s(unsigned caps)
{
	std::string s;

	if (caps & CEC_CAP_PHYS_ADDR)
		s += "\t\tPhysical Address\n";
	if (caps & CEC_CAP_LOG_ADDRS)
		s += "\t\tLogical Addresses\n";
	if (caps & CEC_CAP_TRANSMIT)
		s += "\t\tTransmit\n";
	if (caps & CEC_CAP_PASSTHROUGH)
		s += "\t\tPassthrough\n";
	if (caps & CEC_CAP_RC)
		s += "\t\tRemote Control Support\n";
	if (caps & CEC_CAP_MONITOR_ALL)
		s += "\t\tMonitor All\n";
	return s;
}

static const char *power_status2s(__u8 power_status)
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

static const char *version2s(unsigned version)
{
	switch (version) {
	case CEC_OP_CEC_VERSION_1_3A:
		return "1.3a";
	case CEC_OP_CEC_VERSION_1_4:
		return "1.4";
	case CEC_OP_CEC_VERSION_2_0:
		return "2.0";
	default:
		return "Unknown";
	}
}

static const char *prim_type2s(unsigned type)
{
	switch (type) {
	case CEC_OP_PRIM_DEVTYPE_TV:
		return "TV";
	case CEC_OP_PRIM_DEVTYPE_RECORD:
		return "Record";
	case CEC_OP_PRIM_DEVTYPE_TUNER:
		return "Tuner";
	case CEC_OP_PRIM_DEVTYPE_PLAYBACK:
		return "Playback";
	case CEC_OP_PRIM_DEVTYPE_AUDIOSYSTEM:
		return "Audio System";
	case CEC_OP_PRIM_DEVTYPE_SWITCH:
		return "Switch";
	case CEC_OP_PRIM_DEVTYPE_PROCESSOR:
		return "Processor";
	default:
		return "Unknown";
	}
}

static const char *la_type2s(unsigned type)
{
	switch (type) {
	case CEC_LOG_ADDR_TYPE_TV:
		return "TV";
	case CEC_LOG_ADDR_TYPE_RECORD:
		return "Record";
	case CEC_LOG_ADDR_TYPE_TUNER:
		return "Tuner";
	case CEC_LOG_ADDR_TYPE_PLAYBACK:
		return "Playback";
	case CEC_LOG_ADDR_TYPE_AUDIOSYSTEM:
		return "Audio System";
	case CEC_LOG_ADDR_TYPE_SPECIFIC:
		return "Specific";
	case CEC_LOG_ADDR_TYPE_UNREGISTERED:
		return "Unregistered";
	default:
		return "Unknown";
	}
}

static const char *la2s(unsigned la)
{
	switch (la & 0xf) {
	case 0:
		return "TV";
	case 1:
		return "Recording Device 1";
	case 2:
		return "Recording Device 2";
	case 3:
		return "Tuner 1";
	case 4:
		return "Playback Device 1";
	case 5:
		return "Audio System";
	case 6:
		return "Tuner 2";
	case 7:
		return "Tuner 3";
	case 8:
		return "Playback Device 2";
	case 9:
		return "Playback Device 3";
	case 10:
		return "Tuner 4";
	case 11:
		return "Playback Device 3";
	case 12:
		return "Reserved 1";
	case 13:
		return "Reserved 2";
	case 14:
		return "Specific";
	case 15:
	default:
		return "Unregistered";
	}
}

static std::string laflags2s(unsigned flags)
{
	std::string s;

	if (!flags)
		return s;

	s = "(";
	if (flags & CEC_LOG_ADDRS_FL_ALLOW_UNREG_FALLBACK)
		s += "Allow Fallback to Unregistered, ";
	if (flags & CEC_LOG_ADDRS_FL_ALLOW_RC_PASSTHRU)
		s += "Allow RC Passthrough, ";
	if (flags & CEC_LOG_ADDRS_FL_CDC_ONLY)
		s += "CDC-Only, ";
	if (s.length())
		s.erase(s.length() - 2, 2);
	return s + ")";
}

static std::string all_dev_types2s(unsigned types)
{
	std::string s;

	if (types & CEC_OP_ALL_DEVTYPE_TV)
		s += "TV, ";
	if (types & CEC_OP_ALL_DEVTYPE_RECORD)
		s += "Record, ";
	if (types & CEC_OP_ALL_DEVTYPE_TUNER)
		s += "Tuner, ";
	if (types & CEC_OP_ALL_DEVTYPE_PLAYBACK)
		s += "Playback, ";
	if (types & CEC_OP_ALL_DEVTYPE_AUDIOSYSTEM)
		s += "Audio System, ";
	if (types & CEC_OP_ALL_DEVTYPE_SWITCH)
		s += "Switch, ";
	if (s.length())
		return s.erase(s.length() - 2, 2);
	return s;
}

static std::string rc_src_prof2s(unsigned prof, const std::string &prefix)
{
	std::string s;

	prof &= 0x1f;
	if (prof == 0)
		return prefix + "\t\tNone\n";
	if (prof & CEC_OP_FEAT_RC_SRC_HAS_DEV_ROOT_MENU)
		s += prefix + "\t\tSource Has Device Root Menu\n";
	if (prof & CEC_OP_FEAT_RC_SRC_HAS_DEV_SETUP_MENU)
		s += prefix + "\t\tSource Has Device Setup Menu\n";
	if (prof & CEC_OP_FEAT_RC_SRC_HAS_MEDIA_CONTEXT_MENU)
		s += prefix + "\t\tSource Has Contents Menu\n";
	if (prof & CEC_OP_FEAT_RC_SRC_HAS_MEDIA_TOP_MENU)
		s += prefix + "\t\tSource Has Media Top Menu\n";
	if (prof & CEC_OP_FEAT_RC_SRC_HAS_MEDIA_CONTEXT_MENU)
		s += prefix + "\t\tSource Has Media Context-Sensitive Menu\n";
	return s;
}

static std::string dev_feat2s(unsigned feat, const std::string &prefix)
{
	std::string s;

	feat &= 0x7e;
	if (feat == 0)
		return prefix + "\t\tNone\n";
	if (feat & CEC_OP_FEAT_DEV_HAS_RECORD_TV_SCREEN)
		s += prefix + "\t\tTV Supports <Record TV Screen>\n";
	if (feat & CEC_OP_FEAT_DEV_HAS_SET_OSD_STRING)
		s += prefix + "\t\tTV Supports <Set OSD String>\n";
	if (feat & CEC_OP_FEAT_DEV_HAS_DECK_CONTROL)
		s += prefix + "\t\tSupports Deck Control\n";
	if (feat & CEC_OP_FEAT_DEV_HAS_SET_AUDIO_RATE)
		s += prefix + "\t\tSource Supports <Set Audio Rate>\n";
	if (feat & CEC_OP_FEAT_DEV_SINK_HAS_ARC_TX)
		s += prefix + "\t\tSink Supports ARC Tx\n";
	if (feat & CEC_OP_FEAT_DEV_SOURCE_HAS_ARC_RX)
		s += prefix + "\t\tSource Supports ARC Rx\n";
	return s;
}

/*
 * Most of these vendor IDs come from include/cectypes.h from libcec.
 */
static const char *vendor2s(unsigned vendor)
{
	switch (vendor) {
	case 0x000039:
	case 0x000ce7:
		return " (Toshiba)";
	case 0x0000f0:
		return " (Samsung)";
	case 0x0005cd:
		return " (Denon)";
	case 0x000678:
		return " (Marantz)";
	case 0x000982:
		return " (Loewe)";
	case 0x0009b0:
		return " (Onkyo)";
	case 0x000c03:
		return " (HDMI)";
	case 0x001582:
		return " (Pulse-Eight)";
	case 0x001950:
	case 0x9c645e:
		return " (Harman Kardon)";
	case 0x001a11:
		return " (Google)";
	case 0x0020c7:
		return " (Akai)";
	case 0x002467:
		return " (AOC)";
	case 0x005060:
		return " (Cisco)";
	case 0x008045:
		return " (Panasonic)";
	case 0x00903e:
		return " (Philips)";
	case 0x009053:
		return " (Daewoo)";
	case 0x00a0de:
		return " (Yamaha)";
	case 0x00d0d5:
		return " (Grundig)";
	case 0x00d38d:
		return " (Hospitality Profile)";
	case 0x00e036:
		return " (Pioneer)";
	case 0x00e091:
		return " (LG)";
	case 0x08001f:
	case 0x534850:
		return " (Sharp)";
	case 0x080046:
		return " (Sony)";
	case 0x18c086:
		return " (Broadcom)";
	case 0x6b746d:
		return " (Vizio)";
	case 0x8065e9:
		return " (Benq)";
	default:
		return "";
	}
}

int cec_named_ioctl(int fd, const char *name,
		    unsigned long int request, void *parm)
{
	int retval = ioctl(fd, request, parm);
	int e;

	e = retval == 0 ? 0 : errno;
	if (options[OptTrace])
		printf("\t\t%s returned %d (%s)\n",
			name, retval, strerror(e));

	return retval == -1 ? e : (retval ? -1 : 0);
}

static void log_unknown_msg(const struct cec_msg *msg)
{
	__u32 vendor_id;
	__u16 phys_addr;
	const __u8 *bytes;
	__u8 size;
	unsigned i;

	switch (msg->msg[1]) {
	case CEC_MSG_VENDOR_COMMAND:
		printf("CEC_MSG_VENDOR_COMMAND:\n");
		cec_ops_vendor_command(msg, &size, &bytes);
		printf("\tvendor-specific-data:");
		for (i = 0; i < size; i++)
			printf(" 0x%02x", bytes[i]);
		printf("\n");
		break;
	case CEC_MSG_VENDOR_COMMAND_WITH_ID:
		printf("CEC_MSG_VENDOR_COMMAND_WITH_ID:\n");
		cec_ops_vendor_command_with_id(msg, &vendor_id, &size, &bytes);
		log_arg(&arg_vendor_id, "vendor-id", vendor_id);
		printf("\tvendor-specific-data:");
		for (i = 0; i < size; i++)
			printf(" 0x%02x", bytes[i]);
		printf("\n");
		break;
	case CEC_MSG_VENDOR_REMOTE_BUTTON_DOWN:
		printf("CEC_MSG_VENDOR_REMOTE_BUTTON_DOWN:\n");
		cec_ops_vendor_remote_button_down(msg, &size, &bytes);
		printf("\tvendor-specific-rc-code:");
		for (i = 0; i < size; i++)
			printf(" 0x%02x", bytes[i]);
		printf("\n");
		break;
	case CEC_MSG_CDC_MESSAGE:
		phys_addr = (msg->msg[2] << 8) | msg->msg[3];

		printf("CEC_MSG_CDC 0x%02x:\n", msg->msg[4]);
		log_arg(&arg_u16, "phys-addr", phys_addr);
		printf("\tpayload:");
		for (i = 5; i < msg->len; i++)
			printf(" 0x%02x", msg->msg[i]);
		printf("\n");
		break;
	default:
		printf("CEC_MSG (0x%02x)%s", msg->msg[1], msg->len > 2 ? ":\n\tpayload:" : "");
		for (i = 2; i < msg->len; i++)
			printf(" 0x%02x", msg->msg[i]);
		printf("\n");
		break;
	}
}

static void log_event(struct cec_event &ev)
{
	__u16 pa;

	printf("\n");
	if (ev.flags & CEC_EVENT_FL_INITIAL_STATE)
		printf("Initial ");
	switch (ev.event) {
	case CEC_EVENT_STATE_CHANGE:
		pa = ev.state_change.phys_addr;
		printf("Event: State Change: PA: %x.%x.%x.%x, LA mask: 0x%04x\n",
		       pa >> 12, (pa >> 8) & 0xf,
		       (pa >> 4) & 0xf, pa & 0xf,
		       ev.state_change.log_addr_mask);
		break;
	case CEC_EVENT_LOST_MSGS:
		printf("Event: Lost Messages\n");
		break;
	default:
		printf("Event: Unknown (0x%x)\n", ev.event);
		break;
	}
	if (show_info)
		printf("\tTimestamp: %llu.%03llus\n", ev.ts / 1000000000,
		       (ev.ts % 1000000000) / 1000000);
}

static __u16 phys_addrs[16];

static int showTopologyDevice(struct node *node, unsigned i, unsigned la)
{
	struct cec_msg msg;
	char osd_name[15];

	printf("\tSystem Information for device %d (%s) from device %d (%s):\n",
	       i, la2s(i), la, la2s(la));

	cec_msg_init(&msg, la, i);
	cec_msg_get_cec_version(&msg, true);
	doioctl(node, CEC_TRANSMIT, &msg);
	printf("\t\tCEC Version                : %s\n",
	       (!cec_msg_status_is_ok(&msg)) ? status2s(msg).c_str() : version2s(msg.msg[2]));

	cec_msg_init(&msg, la, i);
	cec_msg_give_physical_addr(&msg, true);
	doioctl(node, CEC_TRANSMIT, &msg);
	printf("\t\tPhysical Address           : ");
	if (!cec_msg_status_is_ok(&msg)) {
		printf("%s\n", status2s(msg).c_str());
	} else {
		__u16 phys_addr = (msg.msg[2] << 8) | msg.msg[3];

		printf("%x.%x.%x.%x\n",
		       phys_addr >> 12, (phys_addr >> 8) & 0xf,
		       (phys_addr >> 4) & 0xf, phys_addr & 0xf);
		printf("\t\tPrimary Device Type        : %s\n",
		       prim_type2s(msg.msg[4]));
		phys_addrs[i] = phys_addr;
	}

	cec_msg_init(&msg, la, i);
	cec_msg_give_device_vendor_id(&msg, true);
	doioctl(node, CEC_TRANSMIT, &msg);
	printf("\t\tVendor ID                  : ");
	if (!cec_msg_status_is_ok(&msg))
		printf("%s\n", status2s(msg).c_str());
	else
		printf("0x%02x%02x%02x%s\n",
		       msg.msg[2], msg.msg[3], msg.msg[4],
		       vendor2s(msg.msg[2] << 16 | msg.msg[3] << 8 | msg.msg[4]));

	cec_msg_init(&msg, la, i);
	cec_msg_give_osd_name(&msg, true);
	doioctl(node, CEC_TRANSMIT, &msg);
	cec_ops_set_osd_name(&msg, osd_name);
	printf("\t\tOSD Name                   : %s\n",
	       (!cec_msg_status_is_ok(&msg)) ? status2s(msg).c_str() : osd_name);

	cec_msg_init(&msg, la, i);
	cec_msg_get_menu_language(&msg, true);
	doioctl(node, CEC_TRANSMIT, &msg);
	if (cec_msg_status_is_ok(&msg)) {
		char language[4];

		cec_ops_set_menu_language(&msg, language);
		language[3] = 0;
		printf("\t\tMenu Language              : %s\n", language);
	}

	cec_msg_init(&msg, la, i);
	cec_msg_give_device_power_status(&msg, true);
	doioctl(node, CEC_TRANSMIT, &msg);
	if (cec_msg_status_is_ok(&msg)) {
		__u8 pwr;

		cec_ops_report_power_status(&msg, &pwr);
		printf("\t\tPower Status               : %s\n",
		       power_status2s(pwr));
	}

	cec_msg_init(&msg, la, i);
	cec_msg_give_features(&msg, true);
	doioctl(node, CEC_TRANSMIT, &msg);
	if (cec_msg_status_is_ok(&msg)) {
		__u8 vers, all_dev_types;
		const __u8 *rc, *feat;

		cec_ops_report_features(&msg, &vers, &all_dev_types, &rc, &feat);

		printf("\t\tFeatures                   :\n");
		printf("\t\t    CEC Version            : %s\n", version2s(vers));
		printf("\t\t    All Device Types       : %s\n",
		       all_dev_types2s(all_dev_types).c_str());
		while (rc) {
			if (*rc & 0x40) {
				printf("\t\t    RC Source Profile      :\n%s",
				       rc_src_prof2s(*rc, "\t").c_str());
			} else {
				const char *s = "Reserved";

				switch (*rc & 0xf) {
				case 0:
					s = "None";
					break;
				case 2:
					s = "RC Profile 1";
					break;
				case 6:
					s = "RC Profile 2";
					break;
				case 10:
					s = "RC Profile 3";
					break;
				case 14:
					s = "RC Profile 4";
					break;
				}
				printf("\t\t    RC TV Profile          : %s\n", s);
			}
			if (!(*rc++ & CEC_OP_FEAT_EXT))
				break;
		}

		while (feat) {
			printf("\t\t    Device Features        :\n%s",
				       dev_feat2s(*feat, "\t").c_str());
			if (!(*feat++ & CEC_OP_FEAT_EXT))
				break;
		}
	}
	return 0;
}

static __u16 calc_mask(__u16 pa)
{
	if (pa & 0xf)
		return 0xffff;
	if (pa & 0xff)
		return 0xfff0;
	if (pa & 0xfff)
		return 0xff00;
	if (pa & 0xffff)
		return 0xf000;
	return 0;
}

static int showTopology(struct node *node)
{
	struct cec_msg msg = { };
	struct cec_log_addrs laddrs = { };

	if (!(node->caps & CEC_CAP_TRANSMIT))
		return -ENOTTY;

	doioctl(node, CEC_ADAP_G_LOG_ADDRS, &laddrs);

	for (unsigned i = 0; i < 15; i++) {
		int ret;

		cec_msg_init(&msg, 0xf, i);
		ret = doioctl(node, CEC_TRANSMIT, &msg);

		if (ret)
			continue;

		if (msg.tx_status & CEC_TX_STATUS_OK)
			showTopologyDevice(node, i, laddrs.log_addr[0]);
		else if (show_info && !(msg.tx_status & CEC_TX_STATUS_MAX_RETRIES))
			printf("\t\t%s for addr %d\n", status2s(msg).c_str(), i);
	}

	__u16 pas[16];

	memcpy(pas, phys_addrs, sizeof(pas));
	std::sort(pas, pas + 16);
	unsigned level = 0;
	unsigned last_pa_mask = 0;

	if (pas[0] == 0xffff)
		return 0;

	printf("\n\tTopology:\n\n");
	for (unsigned i = 0; i < 16; i++) {
		__u16 pa = pas[i];
		unsigned la_for_pa = 0;

		if (pa == 0xffff)
			break;

		__u16 pa_mask = calc_mask(pa);
	
		while (last_pa_mask < pa_mask) {
			last_pa_mask = (last_pa_mask >> 4) | 0xf000;
			level++;
		}
		while (last_pa_mask > pa_mask) {
			last_pa_mask <<= 4;
			level--;
		}
		printf("\t");
		for (unsigned j = 0; j < level; j++)
			printf("    ");
		for (unsigned j = 0; j < 16; j++)
			if (pa == phys_addrs[j]) {
				la_for_pa = j;
				break;
			}
		printf("%x.%x.%x.%x: %s\n",
		       pa >> 12, (pa >> 8) & 0xf,
		       (pa >> 4) & 0xf, pa & 0xf,
		       la2s(la_for_pa));
	}
	return 0;
}

static inline unsigned response_time_ms(const struct cec_msg &msg)
{
	unsigned ms = (msg.rx_ts - msg.tx_ts) / 1000000;

	// Compensate for the time it took (approx.) to receive the
	// message.
	if (ms >= msg.len * 24)
		return ms - msg.len * 24;
	return 0;
}

int main(int argc, char **argv)
{
	const char *device = "/dev/cec0";	/* -d device */
	const message *opt;
	msg_vec msgs;
	char short_options[26 * 2 * 2 + 1];
	__u32 timeout = 1000;
	__u32 vendor_id = 0x000c03; /* HDMI LLC vendor ID */
	__u16 phys_addr;
	__u8 from = 0, to = 0;
	__u8 dev_features = 0;
	__u8 rc_tv = 0;
	__u8 rc_src = 0;
	const char *osd_name = "";
	bool reply = true;
	int idx = 0;
	int fd = -1;
	int ch;
	int i;

	init_messages();

	memset(phys_addrs, 0xff, sizeof(phys_addrs));

	for (i = 0; long_options[i].name; i++) {
		if (!isalpha(long_options[i].val))
			continue;
		short_options[idx++] = long_options[i].val;
		if (long_options[i].has_arg == required_argument)
			short_options[idx++] = ':';
	}
	while (1) {
		int option_index = 0;
		struct cec_msg msg;

		short_options[idx] = 0;
		ch = getopt_long(argc, argv, short_options,
				 long_options, &option_index);
		if (ch == -1)
			break;

		if (ch > OptMessages)
			cec_msg_init(&msg, 0, 0);
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
		case OptVerbose:
			show_info = true;
			break;
		case OptFrom:
			from = strtoul(optarg, NULL, 0) & 0xf;
			break;
		case OptTo:
			to = strtoul(optarg, NULL, 0) & 0xf;
			break;
		case OptTimeout:
			timeout = strtoul(optarg, NULL, 0);
			break;
		case OptNoReply:
			reply = false;
			break;
		case OptPhysAddr:
			phys_addr = parse_phys_addr(optarg);
			break;
		case OptOsdName:
			osd_name = optarg;
			break;
		case OptVendorID:
			vendor_id = strtoul(optarg, NULL, 0) & 0x00ffffff;
			break;
		case OptListUICommands:
			printf("'ui-cmd' can have these values:\n");
			for (unsigned i = 0; i < sizeof(type_ui_cmd) / sizeof(type_ui_cmd[0]); i++)
				printf("\t%s (%u)\n",
				       type_ui_cmd[i].type_name, type_ui_cmd[i].value);
			printf("\n");
			break;
		case OptRcTVProfile1:
			rc_tv = CEC_OP_FEAT_RC_TV_PROFILE_1;
			break;
		case OptRcTVProfile2:
			rc_tv = CEC_OP_FEAT_RC_TV_PROFILE_2;
			break;
		case OptRcTVProfile3:
			rc_tv = CEC_OP_FEAT_RC_TV_PROFILE_3;
			break;
		case OptRcTVProfile4:
			rc_tv = CEC_OP_FEAT_RC_TV_PROFILE_4;
			break;
		case OptRcSrcDevRoot:
			rc_src |= CEC_OP_FEAT_RC_SRC_HAS_DEV_ROOT_MENU;
			break;
		case OptRcSrcDevSetup:
			rc_src |= CEC_OP_FEAT_RC_SRC_HAS_DEV_SETUP_MENU;
			break;
		case OptRcSrcContents:
			rc_src |= CEC_OP_FEAT_RC_SRC_HAS_CONTENTS_MENU;
			break;
		case OptRcSrcMediaTop:
			rc_src |= CEC_OP_FEAT_RC_SRC_HAS_MEDIA_TOP_MENU;
			break;
		case OptRcSrcMediaContext:
			rc_src |= CEC_OP_FEAT_RC_SRC_HAS_MEDIA_CONTEXT_MENU;
			break;
		case OptFeatRecordTVScreen:
			dev_features |= CEC_OP_FEAT_DEV_HAS_RECORD_TV_SCREEN;
			break;
		case OptFeatSetOSDString:
			dev_features |= CEC_OP_FEAT_DEV_HAS_SET_OSD_STRING;
			break;
		case OptFeatDeckControl:
			dev_features |= CEC_OP_FEAT_DEV_HAS_DECK_CONTROL;
			break;
		case OptFeatSetAudioRate:
			dev_features |= CEC_OP_FEAT_DEV_HAS_SET_AUDIO_RATE;
			break;
		case OptFeatSinkHasARCTx:
			dev_features |= CEC_OP_FEAT_DEV_SINK_HAS_ARC_TX;
			break;
		case OptFeatSourceHasARCRx:
			dev_features |= CEC_OP_FEAT_DEV_SOURCE_HAS_ARC_RX;
			break;
		case OptPlayback:
		case OptRecord:
			if (options[OptPlayback] && options[OptRecord]) {
				fprintf(stderr, "--playback and --record cannot be combined.\n\n");
				usage();
				return 1;
			}
			break;
		case OptSwitch:
		case OptCDCOnly:
		case OptUnregistered:
			if (options[OptCDCOnly] + options[OptUnregistered] + options[OptSwitch] > 1) {
				fprintf(stderr, "--switch, --cdc-only and --unregistered cannot be combined.\n\n");
				usage();
				return 1;
			}
			break;
		case ':':
			fprintf(stderr, "Option '%s' requires a value\n\n",
				argv[optind]);
			usage();
			return 1;
		case '?':
			if (argv[optind])
				fprintf(stderr, "Unknown argument '%s'\n\n", argv[optind]);
			usage();
			return 1;
		case OptVendorCommand: {
			static const char *arg_names[] = {
				"payload",
				NULL
			};
			char *value, *endptr, *subs = optarg;
			__u8 size = 0;
			__u8 bytes[14];

			while (*subs != '\0') {
				switch (parse_subopt(&subs, arg_names, &value)) {
				case 0:
					while (size < sizeof(bytes)) {
						bytes[size++] = strtol(value, &endptr, 0L);
						if (endptr == value) {
							size--;
							break;
						}
						value = strchr(value, ':');
						if (value == NULL)
							break;
						value++;
					}
					break;
				default:
					exit(1);
				}
			}
			if (size) {
				cec_msg_vendor_command(&msg, size, bytes);
				msgs.push_back(msg);
			}
			break;
		}
		case OptCustomCommand: {
			static const char *arg_names[] = {
				"cmd",
				"payload",
				NULL
			};
			char *value, *endptr, *subs = optarg;
			bool have_cmd = false;
			__u8 cmd;
			__u8 size = 0;
			__u8 bytes[14];

			while (*subs != '\0') {
				switch (parse_subopt(&subs, arg_names, &value)) {
				case 0:
					cmd = strtol(value, &endptr, 0L);
					have_cmd = true;
					break;
				case 1:
					while (size < sizeof(bytes)) {
						bytes[size++] = strtol(value, &endptr, 0L);
						if (endptr == value) {
							size--;
							break;
						}
						value = strchr(value, ':');
						if (value == NULL)
							break;
						value++;
					}
					break;
				default:
					exit(1);
				}
			}
			if (have_cmd) {
				msg.len = 2 + size;
				msg.msg[1] = cmd;
				memcpy(msg.msg + 2, bytes, size);
				msgs.push_back(msg);
			}
			break;
		}
		case OptVendorCommandWithID: {
			static const char *arg_names[] = {
				"vendor-id",
				"cmd",
				NULL
			};
			char *value, *endptr, *subs = optarg;
			__u32 vendor_id = 0;
			__u8 size = 0;
			__u8 bytes[11];

			while (*subs != '\0') {
				switch (parse_subopt(&subs, arg_names, &value)) {
				case 0:
					vendor_id = strtol(value, 0L, 0);
					break;
				case 1:
					while (size < sizeof(bytes)) {
						bytes[size++] = strtol(value, &endptr, 0L);
						if (endptr == value) {
							size--;
							break;
						}
						value = strchr(value, ':');
						if (value == NULL)
							break;
						value++;
					}
					break;
				default:
					exit(1);
				}
			}
			if (size) {
				cec_msg_vendor_command_with_id(&msg, vendor_id, size, bytes);
				msgs.push_back(msg);
			}
			break;
		}
		case OptVendorRemoteButtonDown: {
			static const char *arg_names[] = {
				"rc-code",
				NULL
			};
			char *value, *endptr, *subs = optarg;
			__u8 size = 0;
			__u8 bytes[14];

			while (*subs != '\0') {
				switch (parse_subopt(&subs, arg_names, &value)) {
				case 0:
					while (size < sizeof(bytes)) {
						bytes[size++] = strtol(value, &endptr, 0L);
						if (endptr == value) {
							size--;
							break;
						}
						value = strchr(value, ':');
						if (value == NULL)
							break;
						value++;
					}
					break;
				default:
					exit(1);
				}
			}
			if (size) {
				cec_msg_vendor_remote_button_down(&msg, size, bytes);
				msgs.push_back(msg);
			}
			break;
		}
		default:
			if (ch >= OptHelpAll) {
				usage_options(ch);
				exit(0);
			}
			if (ch < OptMessages)
				break;
			opt = opt2message[ch - OptMessages];
			parse_msg_args(msg, reply, opt, ch);
			msgs.push_back(msg);
			break;
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

	if (rc_tv && rc_src) {
		fprintf(stderr, "--rc-tv- and --rc-src- options cannot be combined.\n\n");
		usage();
		return 1;
	}

	if (rc_tv && !options[OptTV]) {
		fprintf(stderr, "--rc-tv- can only be used in combination with --tv.\n\n");
		usage();
		return 1;
	}

	if (rc_src && options[OptTV]) {
		fprintf(stderr, "--rc-src- can't be used in combination with --tv.\n\n");
		usage();
		return 1;
	}

	if ((dev_features & (CEC_OP_FEAT_DEV_SOURCE_HAS_ARC_RX |
			     CEC_OP_FEAT_DEV_HAS_DECK_CONTROL |
			     CEC_OP_FEAT_DEV_HAS_SET_AUDIO_RATE)) && options[OptTV]) {
		fprintf(stderr, "--feat-deck-control, --feat-set-audio-rate and --feat-source-has-arc-rx cannot be used in combination with --tv.\n\n");
		usage();
		return 1;
	}

	if ((dev_features & (CEC_OP_FEAT_DEV_HAS_RECORD_TV_SCREEN |
			     CEC_OP_FEAT_DEV_HAS_SET_OSD_STRING)) && !options[OptTV]) {
		fprintf(stderr, "--feat-set-osd-string and --feat-record-tv-screen can only be used in combination with --tv.\n\n");
		usage();
		return 1;
	}

	if ((fd = open(device, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", device,
			strerror(errno));
		exit(1);
	}

	struct node node;
	struct cec_caps caps = { };

	node.fd = fd;
	node.device = device;
	doioctl(&node, CEC_ADAP_G_CAPS, &caps);
	node.caps = caps.capabilities;
	node.available_log_addrs = caps.available_log_addrs;

	unsigned flags = 0;

	if (options[OptOsdName])
		;
	else if (options[OptTV])
		osd_name = "TV";
	else if (options[OptRecord])
		osd_name = "Record";
	else if (options[OptPlayback])
		osd_name = "Playback";
	else if (options[OptTuner])
		osd_name = "Tuner";
	else if (options[OptAudio])
		osd_name = "Audio System";
	else if (options[OptProcessor])
		osd_name = "Processor";
	else if (options[OptSwitch] || options[OptCDCOnly] || options[OptUnregistered])
		osd_name = "";
	else
		osd_name = "TV";

	if (options[OptTV])
		flags |= 1 << CEC_OP_PRIM_DEVTYPE_TV;
	if (options[OptRecord])
		flags |= 1 << CEC_OP_PRIM_DEVTYPE_RECORD;
	if (options[OptTuner])
		flags |= 1 << CEC_OP_PRIM_DEVTYPE_TUNER;
	if (options[OptPlayback])
		flags |= 1 << CEC_OP_PRIM_DEVTYPE_PLAYBACK;
	if (options[OptAudio])
		flags |= 1 << CEC_OP_PRIM_DEVTYPE_AUDIOSYSTEM;
	if (options[OptProcessor])
		flags |= 1 << CEC_OP_PRIM_DEVTYPE_PROCESSOR;
	if (options[OptSwitch] || options[OptCDCOnly] || options[OptUnregistered])
		flags |= 1 << CEC_OP_PRIM_DEVTYPE_SWITCH;

	if (flags == 0 && (rc_tv || rc_src || dev_features)) {
		fprintf(stderr, "The --feat- and --rc- options can only be used in combination with selecting the device type.\n\n");
		usage();
		return 1;
	}

	printf("Driver Info:\n");
	printf("\tDriver Name                : %s\n", caps.driver);
	printf("\tAdapter Name               : %s\n", caps.name);
	printf("\tCapabilities               : 0x%08x\n", caps.capabilities);
	printf("%s", caps2s(caps.capabilities).c_str());
	printf("\tDriver version             : %d.%d.%d\n",
			caps.version >> 16,
			(caps.version >> 8) & 0xff,
			caps.version & 0xff);
	printf("\tAvailable Logical Addresses: %u\n",
	       caps.available_log_addrs);

	bool set_log_addrs = (node.caps & CEC_CAP_LOG_ADDRS) && flags;
	bool set_phys_addr = (node.caps & CEC_CAP_PHYS_ADDR) && options[OptPhysAddr];
	bool clear_log_addrs = (node.caps & CEC_CAP_LOG_ADDRS) && options[OptClear];

	// When setting both PA and LA it is best to clear the LAs first.
	if (clear_log_addrs || (set_phys_addr && set_log_addrs)) {
		struct cec_log_addrs laddrs = { };

		doioctl(&node, CEC_ADAP_S_LOG_ADDRS, &laddrs);
	}

	if (set_phys_addr)
		doioctl(&node, CEC_ADAP_S_PHYS_ADDR, &phys_addr);

	doioctl(&node, CEC_ADAP_G_PHYS_ADDR, &phys_addr);
	printf("\tPhysical Address           : %x.%x.%x.%x\n",
	       phys_addr >> 12, (phys_addr >> 8) & 0xf,
	       (phys_addr >> 4) & 0xf, phys_addr & 0xf);
	if (!options[OptPhysAddr] && phys_addr == 0xffff &&
	    (node.caps & CEC_CAP_PHYS_ADDR))
		printf("Perhaps you should use option --phys-addr?\n");

	if (set_log_addrs) {
		struct cec_log_addrs laddrs = {};
		__u8 all_dev_types = 0;
		__u8 prim_type = 0xff;

		doioctl(&node, CEC_ADAP_S_LOG_ADDRS, &laddrs);
		memset(&laddrs, 0, sizeof(laddrs));

		laddrs.cec_version = options[OptCECVersion1_4] ?
			CEC_OP_CEC_VERSION_1_4 : CEC_OP_CEC_VERSION_2_0;
		strncpy(laddrs.osd_name, osd_name, sizeof(laddrs.osd_name));
		laddrs.osd_name[sizeof(laddrs.osd_name) - 1] = 0;
		laddrs.vendor_id = vendor_id;
		if (options[OptAllowUnregFallback])
			laddrs.flags |= CEC_LOG_ADDRS_FL_ALLOW_UNREG_FALLBACK;
		if (!options[OptNoRC] && flags != (1 << CEC_OP_PRIM_DEVTYPE_SWITCH))
			laddrs.flags |= CEC_LOG_ADDRS_FL_ALLOW_RC_PASSTHRU;
		if (options[OptCDCOnly])
			laddrs.flags |= CEC_LOG_ADDRS_FL_CDC_ONLY;

		for (unsigned i = 0; i < 8; i++) {
			unsigned la_type;

			if (!(flags & (1 << i)))
				continue;
			if (prim_type == 0xff)
				prim_type = i;
			if (laddrs.num_log_addrs == node.available_log_addrs) {
				fprintf(stderr, "Attempt to define too many logical addresses\n");
				exit(-1);
			}
			switch (i) {
			case CEC_OP_PRIM_DEVTYPE_TV:
				la_type = CEC_LOG_ADDR_TYPE_TV;
				all_dev_types |= CEC_OP_ALL_DEVTYPE_TV;
				prim_type = i;
				break;
			case CEC_OP_PRIM_DEVTYPE_RECORD:
				la_type = CEC_LOG_ADDR_TYPE_RECORD;
				all_dev_types |= CEC_OP_ALL_DEVTYPE_RECORD;
				break;
			case CEC_OP_PRIM_DEVTYPE_TUNER:
				la_type = CEC_LOG_ADDR_TYPE_TUNER;
				all_dev_types |= CEC_OP_ALL_DEVTYPE_TUNER;
				break;
			case CEC_OP_PRIM_DEVTYPE_PLAYBACK:
				la_type = CEC_LOG_ADDR_TYPE_PLAYBACK;
				all_dev_types |= CEC_OP_ALL_DEVTYPE_PLAYBACK;
				break;
			case CEC_OP_PRIM_DEVTYPE_AUDIOSYSTEM:
				la_type = CEC_LOG_ADDR_TYPE_AUDIOSYSTEM;
				all_dev_types |= CEC_OP_ALL_DEVTYPE_AUDIOSYSTEM;
				if (prim_type != CEC_OP_PRIM_DEVTYPE_TV)
					prim_type = i;
				break;
			case CEC_OP_PRIM_DEVTYPE_PROCESSOR:
				la_type = CEC_LOG_ADDR_TYPE_SPECIFIC;
				all_dev_types |= CEC_OP_ALL_DEVTYPE_SWITCH;
				break;
			case CEC_OP_PRIM_DEVTYPE_SWITCH:
			default:
				la_type = CEC_LOG_ADDR_TYPE_UNREGISTERED;
				all_dev_types |= CEC_OP_ALL_DEVTYPE_SWITCH;
				break;
			}
			laddrs.log_addr_type[laddrs.num_log_addrs++] = la_type;
		}
		for (unsigned i = 0; i < laddrs.num_log_addrs; i++) {
			laddrs.primary_device_type[i] = prim_type;
			laddrs.all_device_types[i] = all_dev_types;
			laddrs.features[i][0] = rc_tv | rc_src;
			laddrs.features[i][1] = dev_features;
		}

		doioctl(&node, CEC_ADAP_S_LOG_ADDRS, &laddrs);
	}

	struct cec_log_addrs laddrs = { };
	doioctl(&node, CEC_ADAP_G_LOG_ADDRS, &laddrs);
	node.num_log_addrs = laddrs.num_log_addrs;
	printf("\tLogical Address Mask       : 0x%04x\n", laddrs.log_addr_mask);
	printf("\tCEC Version                : %s\n", version2s(laddrs.cec_version));
	if (laddrs.vendor_id != CEC_VENDOR_ID_NONE)
		printf("\tVendor ID                  : 0x%06x%s\n",
		       laddrs.vendor_id, vendor2s(laddrs.vendor_id));
	printf("\tOSD Name                   : '%s'\n", laddrs.osd_name);
	printf("\tLogical Addresses          : %u %s\n",
	       laddrs.num_log_addrs, laflags2s(laddrs.flags).c_str());
	for (unsigned i = 0; i < laddrs.num_log_addrs; i++) {
		if (laddrs.log_addr[i] == CEC_LOG_ADDR_INVALID) {
			printf("\n\t  Logical Address          : Not Allocated\n");
		} else {
			printf("\n\t  Logical Address          : %d (%s)\n",
			       laddrs.log_addr[i], la2s(laddrs.log_addr[i]));
			phys_addrs[laddrs.log_addr[i]] = phys_addr;
		}
		printf("\t    Primary Device Type    : %s\n",
		       prim_type2s(laddrs.primary_device_type[i]));
		printf("\t    Logical Address Type   : %s\n",
		       la_type2s(laddrs.log_addr_type[i]));
		if (laddrs.cec_version < CEC_OP_CEC_VERSION_2_0)
			continue;
		printf("\t    All Device Types       : %s\n",
		       all_dev_types2s(laddrs.all_device_types[i]).c_str());

		bool is_dev_feat = false;
		for (unsigned idx = 0; idx < sizeof(laddrs.features[0]); idx++) {
			__u8 byte = laddrs.features[i][idx];

			if (!is_dev_feat) {
				if (byte & 0x40) {
					printf("\t    RC Source Profile      :\n%s",
					       rc_src_prof2s(byte, "").c_str());
				} else {
					const char *s = "Reserved";

					switch (byte & 0xf) {
					case 0:
						s = "None";
						break;
					case 2:
						s = "RC Profile 1";
						break;
					case 6:
						s = "RC Profile 2";
						break;
					case 10:
						s = "RC Profile 3";
						break;
					case 14:
						s = "RC Profile 4";
						break;
					}
					printf("\t    RC TV Profile          : %s\n", s);
				}
			} else {
				printf("\t    Device Features        :\n%s",
				       dev_feat2s(byte, "").c_str());
			}
			if (byte & CEC_OP_FEAT_EXT)
				continue;
			if (!is_dev_feat)
				is_dev_feat = true;
			else
				break;
		}
	}
	if (node.num_log_addrs == 0) {
		if (options[OptMonitor] || options[OptMonitorAll])
			goto skip_la;
		return 0;
	}
	printf("\n");

	if (!options[OptFrom])
		from = laddrs.log_addr[0];

	if (options[OptShowTopology])
		showTopology(&node);

	for (msg_vec::iterator iter = msgs.begin(); iter != msgs.end(); ++iter) {
		struct cec_msg msg = *iter;

		if (!cec_msg_is_broadcast(&msg) && !options[OptTo]) {
			fprintf(stderr, "attempting to send message without --to\n");
			exit(1);
		}
		printf("\nTransmit from %s to %s (%d to %d):\n", la2s(from),
		       (cec_msg_is_broadcast(&msg) || to == 0xf) ? "all" : la2s(to),
		       from, cec_msg_is_broadcast(&msg) ? 0xf : to);
		msg.msg[0] |= (from << 4) | (cec_msg_is_broadcast(&msg) ? 0xf : to);
		msg.flags = options[OptReplyToFollowers] ? CEC_MSG_FL_REPLY_TO_FOLLOWERS : 0;
		msg.timeout = msg.reply ? timeout : 0;
		log_msg(&msg);
		if (doioctl(&node, CEC_TRANSMIT, &msg))
			continue;
		if (msg.rx_status & (CEC_RX_STATUS_OK | CEC_RX_STATUS_FEATURE_ABORT)) {
			printf("    Received from %s (%d):\n    ", la2s(cec_msg_initiator(&msg)),
			       cec_msg_initiator(&msg));
			log_msg(&msg);
		}
		printf("\tSequence: %u Tx Timestamp: %llu.%03llus",
			msg.sequence,
			msg.tx_ts / 1000000000,
			(msg.tx_ts % 1000000000) / 1000000);
		if (msg.rx_ts)
			printf(" Rx Timestamp: %llu.%03llus\n\tApproximate response time: %u ms",
				msg.rx_ts / 1000000000,
				(msg.rx_ts % 1000000000) / 1000000,
				response_time_ms(msg));
		printf("\n");
		if (!cec_msg_status_is_ok(&msg))
			printf("\t%s\n", status2s(msg).c_str());
	}

skip_la:
	if (options[OptMonitor] || options[OptMonitorAll]) {
		__u32 monitor = options[OptMonitorAll] ?
			CEC_MODE_MONITOR_ALL : CEC_MODE_MONITOR;
		fd_set rd_fds;
		fd_set ex_fds;
		int fd = node.fd;

		printf("\n");
		if (!(node.caps & CEC_CAP_MONITOR_ALL) &&
		    monitor == CEC_MODE_MONITOR_ALL) {
			printf("Monitor All mode is not supported, falling back to regular monitoring\n");
			monitor = CEC_MODE_MONITOR;
		}
		if (doioctl(&node, CEC_S_MODE, &monitor)) {
			printf("Selecting monitor mode failed, you may have to run this as root.\n");
			goto skip_mon;
		}

		fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
		while (1) {
			int res;

			FD_ZERO(&rd_fds);
			FD_ZERO(&ex_fds);
			FD_SET(fd, &rd_fds);
			FD_SET(fd, &ex_fds);
			res = select(fd + 1, &rd_fds, NULL, &ex_fds, NULL);
			if (res <= 0)
				break;
			if (FD_ISSET(fd, &rd_fds)) {
				struct cec_msg msg = { };
				__u8 from, to;

				res = doioctl(&node, CEC_RECEIVE, &msg);
				if (res == ENODEV) {
					printf("Device was disconnected.\n");
					break;
				}
				if (res)
					continue;

				from = cec_msg_initiator(&msg);
				to = cec_msg_destination(&msg);
				bool transmitted = msg.tx_status != 0;
				printf("%s %s to %s (%d to %d): ",
				       transmitted ? "Transmitted by" : "Received from",
				       la2s(from), to == 0xf ? "all" : la2s(to), from, to);
				log_msg(&msg);
				if (show_info && transmitted)
					printf("\tSequence: %u Tx Timestamp: %llu.%03llus\n",
					       msg.sequence,
					       msg.tx_ts / 1000000000,
					       (msg.tx_ts % 1000000000) / 1000000);
				else if (show_info && !transmitted)
					printf("\tSequence: %u Rx Timestamp: %llu.%03llus\n",
					       msg.sequence,
					       msg.rx_ts / 1000000000,
					       (msg.rx_ts % 1000000000) / 1000000);
			}
			if (FD_ISSET(fd, &ex_fds)) {
				struct cec_event ev;

				if (doioctl(&node, CEC_DQEVENT, &ev))
					continue;
				log_event(ev);
			}
		}
	}

skip_mon:
	close(fd);
	return 0;
}
