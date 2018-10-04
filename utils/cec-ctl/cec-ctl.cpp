// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2016 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
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
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <cerrno>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <linux/cec-funcs.h>
#include "cec-htng-funcs.h"

#ifdef __ANDROID__
#include <android-config.h>
#else
#include <config.h>
#endif

#include "cec-ctl.h"

#define CEC_MAX_ARGS 16

#define xstr(s) str(s)
#define str(s) #s

static struct timespec start_monotonic;
static struct timeval start_timeofday;
static bool ignore_la[16];

#define POLL_FAKE_OPCODE 256
static unsigned short ignore_opcode[257];

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
static void log_htng_unknown_msg(const struct cec_msg *msg);
static void log_unknown_msg(const struct cec_msg *msg);

#define VENDOR_EXTRA \
	"  --vendor-command payload=<byte>[:<byte>]*\n" \
	"                                  Send VENDOR_COMMAND message (" xstr(CEC_MSG_VENDOR_COMMAND) ")\n" \
	"  --vendor-command-with-id vendor-id=<val>,cmd=<byte>[:<byte>]*\n" \
	"                                  Send VENDOR_COMMAND_WITH_ID message (" xstr(CEC_MSG_VENDOR_COMMAND_WITH_ID) ")\n" \
	"  --vendor-remote-button-down rc-code=<byte>[:<byte>]*\n" \
	"                                  Send VENDOR_REMOTE_BUTTON_DOWN message (" xstr(CEC_MSG_VENDOR_REMOTE_BUTTON_DOWN) ")\n"

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
	OptLogicalAddress = 'l',
	OptLogicalAddresses = 'L',
	OptMonitor = 'm',
	OptMonitorAll = 'M',
	OptToggleNoReply = 'n',
	OptNonBlocking = 'N',
	OptOsdName = 'o',
	OptPhysAddr = 'p',
	OptPoll = 'P',
	OptShowRaw = 'r',
	OptSkipInfo = 's',
	OptShowTopology = 'S',
	OptTo = 't',
	OptTrace = 'T',
	OptVerbose = 'v',
	OptVendorID = 'V',
	OptWallClock = 'w',
	OptWaitForMsgs = 'W',

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
	OptListDevices,
	OptTimeout,
	OptMonitorTime,
	OptMonitorPin,
	OptIgnore,
	OptStorePin,
	OptAnalyzePin,
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
	__u16 log_addr_mask;
	__u16 phys_addr;
	__u8 log_addr[CEC_MAX_LOG_ADDRS];
};

#define doioctl(n, r, p) cec_named_ioctl((n)->fd, #r, r, p)

bool verbose;

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
	{ "wall-clock", no_argument, 0, OptWallClock },
	{ "osd-name", required_argument, 0, OptOsdName },
	{ "phys-addr", required_argument, 0, OptPhysAddr },
	{ "vendor-id", required_argument, 0, OptVendorID },
	{ "cec-version-1.4", no_argument, 0, OptCECVersion1_4 },
	{ "allow-unreg-fallback", no_argument, 0, OptAllowUnregFallback },
	{ "no-rc-passthrough", no_argument, 0, OptNoRC },
	{ "reply-to-followers", no_argument, 0, OptReplyToFollowers },
	{ "timeout", required_argument, 0, OptTimeout },
	{ "clear", no_argument, 0, OptClear },
	{ "wait-for-msgs", no_argument, 0, OptWaitForMsgs },
	{ "monitor", no_argument, 0, OptMonitor },
	{ "monitor-all", no_argument, 0, OptMonitorAll },
	{ "monitor-pin", no_argument, 0, OptMonitorPin },
	{ "monitor-time", required_argument, 0, OptMonitorTime },
	{ "ignore", required_argument, 0, OptIgnore },
	{ "store-pin", required_argument, 0, OptStorePin },
	{ "analyze-pin", required_argument, 0, OptAnalyzePin },
	{ "no-reply", no_argument, 0, OptToggleNoReply },
	{ "non-blocking", no_argument, 0, OptNonBlocking },
	{ "logical-address", no_argument, 0, OptLogicalAddress },
	{ "logical-addresses", no_argument, 0, OptLogicalAddresses },
	{ "to", required_argument, 0, OptTo },
	{ "from", required_argument, 0, OptFrom },
	{ "skip-info", no_argument, 0, OptSkipInfo },
	{ "show-raw", no_argument, 0, OptShowRaw },
	{ "show-topology", no_argument, 0, OptShowTopology },
	{ "list-devices", no_argument, 0, OptListDevices },
	{ "poll", no_argument, 0, OptPoll },
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
	       "  -d, --device <dev>       Use device <dev> instead of /dev/cec0\n"
	       "                           If <dev> starts with a digit, then /dev/cec<dev> is used.\n"
	       "  -p, --phys-addr <addr>   Use this physical address\n"
	       "  -o, --osd-name <name>    Use this OSD name\n"
	       "  -V, --vendor-id <id>     Use this vendor ID\n"
	       "  -l, --logical-address    Show first configured logical address\n"
	       "  -L, --logical-addresses  Show all configured logical addresses\n"
	       "  -C, --clear              Clear all logical addresses\n"
	       "  -n, --no-reply           Toggle 'don't wait for a reply'\n"
	       "  -N, --non-blocking       Transmit messages in non-blocking mode\n"
	       "  -t, --to <la>            Send message to the given logical address\n"
	       "  -f, --from <la>          Send message from the given logical address\n"
	       "                           By default use the first assigned logical address\n"
	       "  -r, --show-raw           Show the raw CEC message (hex values)\n"
	       "  -s, --skip-info          Skip Driver Info output\n"
	       "  -S, --show-topology      Show the CEC topology\n"
	       "  -P, --poll               Send poll message\n"
	       "  -h, --help               Display this help message\n"
	       "  --help-all               Show all help messages\n"
	       "  -T, --trace              Trace all called ioctls\n"
	       "  -v, --verbose            Turn on verbose reporting\n"
	       "  -w, --wall-clock         Show timestamps as wall-clock time (implies -v)\n"
	       "  -W, --wait-for-msgs      Wait for messages and events for up to --monitor-time secs.\n"
	       "  --cec-version-1.4        Use CEC Version 1.4 instead of 2.0\n"
	       "  --allow-unreg-fallback   Allow fallback to Unregistered\n"
	       "  --no-rc-passthrough      Disable the RC passthrough\n"
	       "  --reply-to-followers     The reply will be sent to followers as well\n"
	       "  --timeout <ms>           Set the reply timeout in milliseconds (default is 1000 ms)\n"
	       "  --list-ui-commands       List all UI commands that can be used with --user-control-pressed\n"
	       "  --list-devices           List all cec devices\n"
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
	       "  -m, --monitor            Monitor CEC traffic\n"
	       "  -M, --monitor-all        Monitor all CEC traffic\n"
	       "  --monitor-pin            Monitor low-level CEC pin\n"
	       "  --monitor-time <secs>    Monitor for <secs> seconds (default is forever)\n"
	       "  --ignore <la>,<opcode>   Ignore messages from logical address <la> and opcode\n"
	       "                           <opcode> when monitoring. 'all' can be used for <la>\n"
	       "                           or <opcode> to match all logical addresses or opcodes.\n"
	       "                           To ignore poll messages use 'poll' as <opcode>.\n"
	       "  --store-pin <to>         Store the low-level CEC pin changes to the file <to>.\n"
	       "                           Use - for stdout.\n"
	       "  --analyze-pin <from>     Analyze the low-level CEC pin changes from the file <from>.\n"
	       "                           Use - for stdin.\n"
	       "\n"
	       CEC_USAGE
	       "\n"
	       "  --custom-command cmd=<byte>[,payload=<byte>[:<byte>]*]\n"
	       "                                      Send custom message\n"
	       );
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

std::string ts2s(__u64 ts)
{
	std::string s;
	struct timeval sub;
	struct timeval res;
	__u64 diff;
	char buf[64];
	time_t t;

	if (!options[OptWallClock]) {
		sprintf(buf, "%llu.%03llus", ts / 1000000000, (ts % 1000000000) / 1000000);
		return buf;
	}
	diff = ts - start_monotonic.tv_sec * 1000000000ULL - start_monotonic.tv_nsec;
	sub.tv_sec = diff / 1000000000ULL;
	sub.tv_usec = (diff % 1000000000ULL) / 1000;
	timeradd(&start_timeofday, &sub, &res);
	t = res.tv_sec;
	s = ctime(&t);
	s = s.substr(0, s.length() - 6);
	sprintf(buf, "%03lu", res.tv_usec / 1000);
	return s + "." + buf;
}

std::string ts2s(double ts)
{
	if (!options[OptWallClock]) {
		char buf[64];

		sprintf(buf, "%10.06f", ts);
		return buf;
	}
	return ts2s((__u64)(ts * 1000000000.0));
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

static void print_bytes(const __u8 *bytes, unsigned len)
{
	for (unsigned i = 0; i < len; i++)
		printf(" 0x%02x", bytes[i]);
	printf(" (");
	for (unsigned i = 0; i < len; i++)
		if (bytes[i] >= 32 && bytes[i] <= 127)
		    printf("%c", bytes[i]);
		else
		    printf(" ");
	printf(")");
}

static void log_raw_msg(const struct cec_msg *msg)
{
	printf("\tRaw:");
	print_bytes(msg->msg, msg->len);
	printf("\n");
}

static void log_htng_unknown_msg(const struct cec_msg *msg)
{
	__u32 vendor_id;
	const __u8 *bytes;
	__u8 size;

	cec_ops_vendor_command_with_id(msg, &vendor_id, &size, &bytes);
	printf("CEC_MSG_VENDOR_COMMAND_WITH_ID (0x%02x):\n",
	       CEC_MSG_VENDOR_COMMAND_WITH_ID);
	log_arg(&arg_vendor_id, "vendor-id", vendor_id);
	printf("\tvendor-specific-data:");
	print_bytes(bytes, size);
	printf("\n");
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
		printf("CEC_MSG_VENDOR_COMMAND (0x%02x):\n",
		       CEC_MSG_VENDOR_COMMAND);
		cec_ops_vendor_command(msg, &size, &bytes);
		printf("\tvendor-specific-data:");
		print_bytes(bytes, size);
		printf("\n");
		break;
	case CEC_MSG_VENDOR_COMMAND_WITH_ID:
		cec_ops_vendor_command_with_id(msg, &vendor_id, &size, &bytes);
		switch (vendor_id) {
		case VENDOR_ID_HTNG:
			log_htng_msg(msg);
			break;
		default:
			printf("CEC_MSG_VENDOR_COMMAND_WITH_ID (0x%02x):\n",
			       CEC_MSG_VENDOR_COMMAND_WITH_ID);
			log_arg(&arg_vendor_id, "vendor-id", vendor_id);
			printf("\tvendor-specific-data:");
			print_bytes(bytes, size);
			printf("\n");
			break;
		}
		break;
	case CEC_MSG_VENDOR_REMOTE_BUTTON_DOWN:
		printf("CEC_MSG_VENDOR_REMOTE_BUTTON_DOWN (0x%02x):\n",
		       CEC_MSG_VENDOR_REMOTE_BUTTON_DOWN);
		cec_ops_vendor_remote_button_down(msg, &size, &bytes);
		printf("\tvendor-specific-rc-code:");
		for (i = 0; i < size; i++)
			printf(" 0x%02x", bytes[i]);
		printf("\n");
		break;
	case CEC_MSG_CDC_MESSAGE:
		phys_addr = (msg->msg[2] << 8) | msg->msg[3];

		printf("CEC_MSG_CDC_MESSAGE (0x%02x): 0x%02x:\n",
		       CEC_MSG_CDC_MESSAGE, msg->msg[4]);
		log_arg(&arg_u16, "phys-addr", phys_addr);
		printf("\tpayload:");
		for (i = 5; i < msg->len; i++)
			printf(" 0x%02x", msg->msg[i]);
		printf("\n");
		break;
	default:
		printf("CEC_MSG (0x%02x)%s", msg->msg[1], msg->len > 2 ? ":\n\tpayload:" : "");
		if (msg->len > 2)
			print_bytes(msg->msg + 2, msg->len - 2);
		printf("\n");
		break;
	}
}

static const char *event2s(__u32 event)
{
	switch (event) {
	case CEC_EVENT_STATE_CHANGE:
		return "State Change";
	case CEC_EVENT_LOST_MSGS:
		return "Lost Messages";
	case CEC_EVENT_PIN_CEC_LOW:
		return "CEC Pin Low";
	case CEC_EVENT_PIN_CEC_HIGH:
		return "CEC Pin High";
	case CEC_EVENT_PIN_HPD_LOW:
		return "HPD Pin Low";
	case CEC_EVENT_PIN_HPD_HIGH:
		return "HPD Pin High";
	case CEC_EVENT_PIN_5V_LOW:
		return "5V Pin Low";
	case CEC_EVENT_PIN_5V_HIGH:
		return "5V Pin High";
	default:
		return "Unknown";
	}
}

static void log_event(struct cec_event &ev, bool show)
{
	bool is_high = ev.event == CEC_EVENT_PIN_CEC_HIGH;
	__u16 pa;

	if (ev.event != CEC_EVENT_PIN_CEC_LOW && ev.event != CEC_EVENT_PIN_CEC_HIGH &&
	    ev.event != CEC_EVENT_PIN_HPD_LOW && ev.event != CEC_EVENT_PIN_HPD_HIGH &&
	    ev.event != CEC_EVENT_PIN_5V_LOW && ev.event != CEC_EVENT_PIN_5V_HIGH)
		printf("\n");
	if ((ev.flags & CEC_EVENT_FL_DROPPED_EVENTS) && show)
		printf("(warn: %s events were lost)\n", event2s(ev.event));
	if ((ev.flags & CEC_EVENT_FL_INITIAL_STATE) && show)
		printf("Initial ");
	switch (ev.event) {
	case CEC_EVENT_STATE_CHANGE:
		pa = ev.state_change.phys_addr;
		if (show)
			printf("Event: State Change: PA: %x.%x.%x.%x, LA mask: 0x%04x\n",
			       pa >> 12, (pa >> 8) & 0xf,
			       (pa >> 4) & 0xf, pa & 0xf,
			       ev.state_change.log_addr_mask);
		break;
	case CEC_EVENT_LOST_MSGS:
		if (show)
			printf("Event: Lost %d messages\n",
			       ev.lost_msgs.lost_msgs);
		break;
	case CEC_EVENT_PIN_CEC_LOW:
	case CEC_EVENT_PIN_CEC_HIGH:
		if ((ev.flags & CEC_EVENT_FL_INITIAL_STATE) && show)
			printf("Event: CEC Pin %s\n", is_high ? "High" : "Low");

		log_event_pin(is_high, ev.ts, show);
		return;
	case CEC_EVENT_PIN_HPD_LOW:
	case CEC_EVENT_PIN_HPD_HIGH:
		if (show)
			printf("Event: HPD Pin %s\n",
			       ev.event == CEC_EVENT_PIN_HPD_HIGH ? "High" : "Low");
		break;
	case CEC_EVENT_PIN_5V_LOW:
	case CEC_EVENT_PIN_5V_HIGH:
		if (show)
			printf("Event: 5V Pin %s\n",
			       ev.event == CEC_EVENT_PIN_5V_HIGH ? "High" : "Low");
		break;
	default:
		if (show)
			printf("Event: Unknown (0x%x)\n", ev.event);
		break;
	}
	if (verbose && show)
		printf("\tTimestamp: %s\n", ts2s(ev.ts).c_str());
}

/*
 * Bits 23-8 contain the physical address, bits 0-3 the logical address
 * (equal to the index).
 */
static __u32 phys_addrs[16];

static int showTopologyDevice(struct node *node, unsigned i, unsigned la)
{
	struct cec_msg msg;
	char osd_name[15];

	printf("\tSystem Information for device %d (%s) from device %d (%s):\n",
	       i, la2s(i), la & 0xf, la2s(la));

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
		phys_addrs[i] = (phys_addr << 8) | i;
	}

	cec_msg_init(&msg, la, i);
	cec_msg_give_device_vendor_id(&msg, true);
	doioctl(node, CEC_TRANSMIT, &msg);
	printf("\t\tVendor ID                  : ");
	if (!cec_msg_status_is_ok(&msg))
		printf("%s\n", status2s(msg).c_str());
	else
		printf("0x%02x%02x%02x %s\n",
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

	if (!laddrs.num_log_addrs)
		return 0;

	for (unsigned i = 0; i < 15; i++) {
		int ret;

		cec_msg_init(&msg, laddrs.log_addr[0], i);
		ret = doioctl(node, CEC_TRANSMIT, &msg);

		if (ret)
			continue;

		if (msg.tx_status & CEC_TX_STATUS_OK)
			showTopologyDevice(node, i, laddrs.log_addr[0]);
		else if (verbose && !(msg.tx_status & CEC_TX_STATUS_MAX_RETRIES))
			printf("\t\t%s for addr %d\n", status2s(msg).c_str(), i);
	}

	__u32 pas[16];

	memcpy(pas, phys_addrs, sizeof(pas));
	std::sort(pas, pas + 16);
	unsigned level = 0;
	unsigned last_pa_mask = 0;

	if ((pas[0] >> 8) == 0xffff)
		return 0;

	printf("\n\tTopology:\n\n");
	for (unsigned i = 0; i < 16; i++) {
		__u16 pa = pas[i] >> 8;
		__u8 la = pas[i] & 0xf;

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
		printf("%x.%x.%x.%x: %s\n",
		       pa >> 12, (pa >> 8) & 0xf,
		       (pa >> 4) & 0xf, pa & 0xf,
		       la2s(la));
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

static void generate_eob_event(__u64 ts, FILE *fstore)
{
	if (!eob_ts || eob_ts_max >= ts)
		return;

	struct cec_event ev_eob = {
		eob_ts,
		CEC_EVENT_PIN_CEC_HIGH
	};

	if (fstore) {
		fprintf(fstore, "%llu.%09llu %d\n",
			ev_eob.ts / 1000000000, ev_eob.ts % 1000000000,
			ev_eob.event - CEC_EVENT_PIN_CEC_LOW);
		fflush(fstore);
	}
	log_event(ev_eob, fstore != stdout);
}

static void show_msg(const cec_msg &msg)
{
	__u8 from = cec_msg_initiator(&msg);
	__u8 to = cec_msg_destination(&msg);

	if (ignore_la[from])
		return;
	if ((msg.len == 1 && (ignore_opcode[POLL_FAKE_OPCODE] & (1 << from))) ||
	    (msg.len > 1 && (ignore_opcode[msg.msg[1]] & (1 << from))))
		return;

	bool transmitted = msg.tx_status != 0;
	printf("%s %s to %s (%d to %d): ",
	       transmitted ? "Transmitted by" : "Received from",
	       la2s(from), to == 0xf ? "all" : la2s(to), from, to);
	log_msg(&msg);
	if (options[OptShowRaw])
		log_raw_msg(&msg);
	if (verbose && transmitted)
		printf("\tSequence: %u Tx Timestamp: %s\n",
		       msg.sequence, ts2s(msg.tx_ts).c_str());
	else if (verbose && !transmitted)
		printf("\tSequence: %u Rx Timestamp: %s\n",
		       msg.sequence, ts2s(msg.rx_ts).c_str());
}

static void wait_for_msgs(struct node &node, __u32 monitor_time)
{
	__u32 monitor = CEC_MODE_INITIATOR | CEC_MODE_FOLLOWER;
	fd_set rd_fds;
	fd_set ex_fds;
	int fd = node.fd;
	time_t t;
	
	if (doioctl(&node, CEC_S_MODE, &monitor)) {
		fprintf(stderr, "Selecting follower mode failed.\n");
		return;
	}

	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
	t = time(NULL) + monitor_time;

	while (!monitor_time || time(NULL) < t) {
		struct timeval tv = { 1, 0 };
		int res;

		fflush(stdout);
		FD_ZERO(&rd_fds);
		FD_ZERO(&ex_fds);
		FD_SET(fd, &rd_fds);
		FD_SET(fd, &ex_fds);
		res = select(fd + 1, &rd_fds, NULL, &ex_fds, &tv);
		if (res < 0)
			break;
		if (FD_ISSET(fd, &rd_fds)) {
			struct cec_msg msg = { };

			res = doioctl(&node, CEC_RECEIVE, &msg);
			if (res == ENODEV) {
				fprintf(stderr, "Device was disconnected.\n");
				break;
			}
			if (!res)
				show_msg(msg);
		}
		if (FD_ISSET(fd, &ex_fds)) {
			struct cec_event ev;

			if (doioctl(&node, CEC_DQEVENT, &ev))
				continue;
			log_event(ev, true);
		}
	}
}

#define MONITOR_FL_DROPPED_EVENTS     (1 << 16)

static void monitor(struct node &node, __u32 monitor_time, const char *store_pin)
{
	__u32 monitor = CEC_MODE_MONITOR;
	fd_set rd_fds;
	fd_set ex_fds;
	int fd = node.fd;
	FILE *fstore = NULL;
	time_t t;
	
	if (options[OptMonitorAll])
		monitor = CEC_MODE_MONITOR_ALL;
	else if (options[OptMonitorPin] || options[OptStorePin])
		monitor = CEC_MODE_MONITOR_PIN;

	if (!(node.caps & CEC_CAP_MONITOR_ALL) && monitor == CEC_MODE_MONITOR_ALL) {
		fprintf(stderr, "Monitor All mode is not supported, falling back to regular monitoring\n");
		monitor = CEC_MODE_MONITOR;
	}
	if (!(node.caps & CEC_CAP_MONITOR_PIN) && monitor == CEC_MODE_MONITOR_PIN) {
		fprintf(stderr, "Monitor Pin mode is not supported\n");
		usage();
		exit(1);
	}

	if (doioctl(&node, CEC_S_MODE, &monitor)) {
		fprintf(stderr, "Selecting monitor mode failed, you may have to run this as root.\n");
		return;
	}

	if (monitor == CEC_MODE_MONITOR_PIN) {
		struct cec_log_addrs laddrs = { };

		doioctl(&node, CEC_ADAP_G_LOG_ADDRS, &laddrs);
		if (laddrs.log_addr_mask && !options[OptSkipInfo]) {
			fprintf(stderr, "note: this CEC adapter is configured. This may cause inaccurate event\n");
			fprintf(stderr, "      timestamps. It is recommended to unconfigure the adapter (cec-ctl -C)\n");
		}
	}

	if (store_pin) {
		if (!strcmp(store_pin, "-"))
			fstore = stdout;
		else
			fstore = fopen(store_pin, "w+");
		if (fstore == NULL) {
			fprintf(stderr, "Failed to open %s: %s\n", store_pin,
				strerror(errno));
			exit(1);
		}
		fprintf(fstore, "# cec-ctl --store-pin\n");
		fprintf(fstore, "# version 1\n");
		fprintf(fstore, "# start_monotonic %lu.%09lu\n",
			start_monotonic.tv_sec, start_monotonic.tv_nsec);
		fprintf(fstore, "# start_timeofday %lu.%06lu\n",
			start_timeofday.tv_sec, start_timeofday.tv_usec);
		fprintf(fstore, "# log_addr_mask 0x%04x\n", node.log_addr_mask);
		fprintf(fstore, "# phys_addr %x.%x.%x.%x\n",
		       node.phys_addr >> 12, (node.phys_addr >> 8) & 0xf,
		       (node.phys_addr >> 4) & 0xf, node.phys_addr & 0xf);
	}

	if (fstore != stdout)
		printf("\n");

	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
	t = time(NULL) + monitor_time;

	while (!monitor_time || time(NULL) < t) {
		struct timeval tv = { 1, 0 };
		bool pin_event = false;
		int res;

		fflush(stdout);
		FD_ZERO(&rd_fds);
		FD_ZERO(&ex_fds);
		FD_SET(fd, &rd_fds);
		FD_SET(fd, &ex_fds);
		res = select(fd + 1, &rd_fds, NULL, &ex_fds, &tv);
		if (res < 0)
			break;
		if (FD_ISSET(fd, &rd_fds)) {
			struct cec_msg msg = { };

			res = doioctl(&node, CEC_RECEIVE, &msg);
			if (res == ENODEV) {
				fprintf(stderr, "Device was disconnected.\n");
				break;
			}
			if (!res && fstore != stdout)
				show_msg(msg);
		}
		if (FD_ISSET(fd, &ex_fds)) {
			struct cec_event ev;

			if (doioctl(&node, CEC_DQEVENT, &ev))
				continue;
			if (ev.event == CEC_EVENT_PIN_CEC_LOW ||
			    ev.event == CEC_EVENT_PIN_CEC_HIGH ||
			    ev.event == CEC_EVENT_PIN_HPD_LOW ||
			    ev.event == CEC_EVENT_PIN_HPD_HIGH ||
			    ev.event == CEC_EVENT_PIN_5V_LOW ||
			    ev.event == CEC_EVENT_PIN_5V_HIGH)
				pin_event = true;
			generate_eob_event(ev.ts, fstore);
			if (pin_event && fstore) {
				unsigned int v = ev.event - CEC_EVENT_PIN_CEC_LOW;

				if (ev.flags & CEC_EVENT_FL_DROPPED_EVENTS)
					v |= MONITOR_FL_DROPPED_EVENTS;
				fprintf(fstore, "%llu.%09llu %d\n",
					ev.ts / 1000000000, ev.ts % 1000000000, v);
				fflush(fstore);
			}
			if (!pin_event || options[OptMonitorPin])
				log_event(ev, fstore != stdout);
		}
		if (!res && eob_ts) {
			struct timespec ts;
			__u64 ts64;

			clock_gettime(CLOCK_MONOTONIC, &ts);
			ts64 = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
			generate_eob_event(ts64, fstore);
		}
	}
	if (fstore && fstore != stdout)
		fclose(fstore);
}

static void analyze(const char *analyze_pin)
{
	FILE *fanalyze;
	struct cec_event ev = { };
	unsigned long tv_sec, tv_nsec, tv_usec;
	unsigned version;
	unsigned log_addr_mask;
	unsigned pa1, pa2, pa3, pa4;
	unsigned line = 1;
	char s[100];

	if (!strcmp(analyze_pin, "-"))
		fanalyze = stdin;
	else
		fanalyze = fopen(analyze_pin, "r");
	if (fanalyze == NULL) {
		fprintf(stderr, "Failed to open %s: %s\n", analyze_pin,
			strerror(errno));
		exit(1);
	}
	if (!fgets(s, sizeof(s), fanalyze) ||
	    strcmp(s, "# cec-ctl --store-pin\n"))
		goto err;
	line++;
	if (!fgets(s, sizeof(s), fanalyze) ||
	    sscanf(s, "# version %u\n", &version) != 1 ||
	    version != 1)
		goto err;
	line++;
	if (!fgets(s, sizeof(s), fanalyze) ||
	    sscanf(s, "# start_monotonic %lu.%09lu\n", &tv_sec, &tv_nsec) != 2 ||
	    tv_nsec >= 1000000000)
		goto err;
	start_monotonic.tv_sec = tv_sec;
	start_monotonic.tv_nsec = tv_nsec;
	line++;
	if (!fgets(s, sizeof(s), fanalyze) ||
	    sscanf(s, "# start_timeofday %lu.%06lu\n", &tv_sec, &tv_usec) != 2 ||
	    tv_usec >= 1000000)
		goto err;
	start_timeofday.tv_sec = tv_sec;
	start_timeofday.tv_usec = tv_usec;
	line++;
	if (!fgets(s, sizeof(s), fanalyze) ||
	    sscanf(s, "# log_addr_mask 0x%04x\n", &log_addr_mask) != 1)
		goto err;
	line++;
	if (!fgets(s, sizeof(s), fanalyze) ||
	    sscanf(s, "# phys_addr %x.%x.%x.%x\n", &pa1, &pa2, &pa3, &pa4) != 4)
		goto err;
	line++;

	printf("Physical Address:     %x.%x.%x.%x\n", pa1, pa2, pa3, pa4);
	printf("Logical Address Mask: 0x%04x\n\n", log_addr_mask);

	while (fgets(s, sizeof(s), fanalyze)) {
		unsigned event;

		if (s[0] == '#' || s[0] == '\n') {
			line++;
			continue;
		}
		if (sscanf(s, "%lu.%09lu %d\n", &tv_sec, &tv_nsec, &event) != 3 ||
		    (event & ~MONITOR_FL_DROPPED_EVENTS) > 5) {
			fprintf(stderr, "malformed data at line %d\n", line);
			break;
		}
		ev.ts = tv_sec * 1000000000ULL + tv_nsec;
		ev.flags = 0;
		if (event & MONITOR_FL_DROPPED_EVENTS) {
			event &= ~MONITOR_FL_DROPPED_EVENTS;
			ev.flags = CEC_EVENT_FL_DROPPED_EVENTS;
		}
		ev.event = event + CEC_EVENT_PIN_CEC_LOW;
		log_event(ev, true);
		line++;
	}

	if (eob_ts) {
		ev.event = CEC_EVENT_PIN_CEC_HIGH;
		ev.ts = eob_ts;
		log_event(ev, true);
	}

	if (fanalyze != stdin)
		fclose(fanalyze);
	return;

err:
	fprintf(stderr, "Not a pin store file: malformed data at line %d\n", line);
	exit(1);
}

static int calc_node_val(const char *s)
{
	s = strrchr(s, '/') + 1;

	if (!memcmp(s, "cec", 3))
		return atol(s + 3);
	return 0;
}

static bool sort_on_device_name(const std::string &s1, const std::string &s2)
{
	int n1 = calc_node_val(s1.c_str());
	int n2 = calc_node_val(s2.c_str());

	return n1 < n2;
}

typedef std::vector<std::string> dev_vec;
typedef std::map<std::string, std::string> dev_map;

static void list_devices()
{
	DIR *dp;
	struct dirent *ep;
	dev_vec files;
	dev_map links;
	dev_map cards;
	struct cec_caps caps;

	dp = opendir("/dev");
	if (dp == NULL) {
		perror ("Couldn't open the directory");
		return;
	}
	while ((ep = readdir(dp)))
		if (!memcmp(ep->d_name, "cec", 3) && isdigit(ep->d_name[3]))
			files.push_back(std::string("/dev/") + ep->d_name);
	closedir(dp);

	/* Find device nodes which are links to other device nodes */
	for (dev_vec::iterator iter = files.begin();
			iter != files.end(); ) {
		char link[64+1];
		int link_len;
		std::string target;

		link_len = readlink(iter->c_str(), link, 64);
		if (link_len < 0) {	/* Not a link or error */
			iter++;
			continue;
		}
		link[link_len] = '\0';

		/* Only remove from files list if target itself is in list */
		if (link[0] != '/')	/* Relative link */
			target = std::string("/dev/");
		target += link;
		if (find(files.begin(), files.end(), target) == files.end()) {
			iter++;
			continue;
		}

		/* Move the device node from files to links */
		if (links[target].empty())
			links[target] = *iter;
		else
			links[target] += ", " + *iter;
		iter = files.erase(iter);
	}

	std::sort(files.begin(), files.end(), sort_on_device_name);

	for (dev_vec::iterator iter = files.begin();
			iter != files.end(); ++iter) {
		int fd = open(iter->c_str(), O_RDWR);
		std::string cec_info;

		if (fd < 0)
			continue;
		int err = ioctl(fd, CEC_ADAP_G_CAPS, &caps);
		close(fd);
		if (err)
			continue;
		cec_info = std::string(caps.driver) + " (" + caps.name + ")";
		if (cards[cec_info].empty())
			cards[cec_info] += cec_info + ":\n";
		cards[cec_info] += "\t" + (*iter);
		if (!(links[*iter].empty()))
			cards[cec_info] += " <- " + links[*iter];
		cards[cec_info] += "\n";
	}
	for (dev_map::iterator iter = cards.begin();
			iter != cards.end(); ++iter) {
		printf("%s\n", iter->second.c_str());
	}
}

int main(int argc, char **argv)
{
	const char *device = "/dev/cec0";	/* -d device */
	const message *opt;
	msg_vec msgs;
	char short_options[26 * 2 * 2 + 1];
	__u32 timeout = 1000;
	__u32 monitor_time = 0;
	__u32 vendor_id = 0x000c03; /* HDMI LLC vendor ID */
	__u16 phys_addr;
	__u8 from = 0, to = 0, first_to = 0xff;
	__u8 dev_features = 0;
	__u8 rc_tv = 0;
	__u8 rc_src = 0;
	const char *osd_name = "";
	const char *store_pin = NULL;
	const char *analyze_pin = NULL;
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

		cec_msg_init(&msg, 0, 0);
		msg.msg[0] = cec_msg_is_broadcast(&msg) ? 0xf : (options[OptTo] ? to : 0xf0);
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
			verbose = true;
			break;
		case OptFrom:
			from = strtoul(optarg, NULL, 0) & 0xf;
			break;
		case OptTo:
			to = strtoul(optarg, NULL, 0) & 0xf;
			if (first_to == 0xff)
				first_to = to;
			break;
		case OptTimeout:
			timeout = strtoul(optarg, NULL, 0);
			break;
		case OptMonitorTime:
			monitor_time = strtoul(optarg, NULL, 0);
			break;
		case OptIgnore: {
			bool all_la = !strncmp(optarg, "all", 3);
			bool all_opcodes = true;
			const char *sep = strchr(optarg, ',');
			unsigned la_mask = 0xffff, opcode, la = 0;

			if (sep)
				all_opcodes = !strncmp(sep + 1, "all", 3);
			if (!all_la) {
				la = strtoul(optarg, NULL, 0);

				if (la > 15) {
					fprintf(stderr, "invalid logical address (> 15)\n");
					usage();
					return 1;
				}
				la_mask = 1 << la;
			}
			if (!all_opcodes) {
				if (!strncmp(sep + 1, "poll", 4)) {
					opcode = POLL_FAKE_OPCODE;
				} else {
					opcode = strtoul(sep + 1, NULL, 0);
					if (opcode > 255) {
						fprintf(stderr, "invalid opcode (> 255)\n");
						usage();
						return 1;
					}
				}
				ignore_opcode[opcode] |= la_mask;
				break;
			}
			if (all_la && all_opcodes) {
				fprintf(stderr, "all,all is invalid\n");
				usage();
				return 1;
			}
			ignore_la[la] = true;
			break;
		}
		case OptStorePin:
			store_pin = optarg;
			break;
		case OptAnalyzePin:
			analyze_pin = optarg;
			break;
		case OptToggleNoReply:
			reply = !reply;
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
		case OptPoll:
			msgs.push_back(msg);
			break;

		case OptListDevices:
			list_devices();
			break;

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
		fprintf(stderr, "unknown arguments: ");
		while (optind < argc)
			fprintf(stderr, "%s ", argv[optind++]);
		fprintf(stderr, "\n");
		usage();
		return 1;
	}

	if (store_pin && analyze_pin) {
		fprintf(stderr, "--store-pin and --analyze-pin options cannot be combined.\n\n");
		usage();
		return 1;
	}

	if (analyze_pin && options[OptSetDevice]) {
		fprintf(stderr, "--device and --analyze-pin options cannot be combined.\n\n");
		usage();
		return 1;
	}

	if (analyze_pin) {
		analyze(analyze_pin);
		return 0;
	}

	if (options[OptWallClock] && !options[OptMonitorPin])
		verbose = true;

	if (store_pin && !strcmp(store_pin, "-"))
		options[OptSkipInfo] = 1;

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

	clock_gettime(CLOCK_MONOTONIC, &start_monotonic);
	gettimeofday(&start_timeofday, NULL);

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

	if (options[OptPhysAddr] && !(node.caps & CEC_CAP_PHYS_ADDR))
		fprintf(stderr, "The CEC adapter doesn't allow setting the physical address manually, ignore this option.\n\n");

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
	node.log_addr_mask = laddrs.log_addr_mask;
	node.phys_addr = phys_addr;

	for (i = 0; i < laddrs.num_log_addrs; i++) {
		__u8 la = laddrs.log_addr[i];

		if (la != CEC_LOG_ADDR_INVALID)
			phys_addrs[la] = (phys_addr << 8) | la;
	}

	if (!options[OptSkipInfo])
		cec_driver_info(caps, laddrs, phys_addr);

	if (node.num_log_addrs == 0) {
		if (options[OptMonitor] || options[OptMonitorAll] ||
		    options[OptMonitorPin] || options[OptStorePin])
			goto skip_la;
		return 0;
	}
	if (!options[OptSkipInfo])
		printf("\n");

	if (!options[OptFrom])
		from = laddrs.log_addr[0] & 0xf;

	if (options[OptShowTopology])
		showTopology(&node);

	if (options[OptLogicalAddress])
		printf("%d\n", laddrs.log_addr[0] & 0xf);
	if (options[OptLogicalAddresses]) {
		for (i = 0; i < laddrs.num_log_addrs; i++)
			printf("%d ", laddrs.log_addr[i] & 0xf);
		printf("\n");
	}

	if (options[OptNonBlocking])
		fcntl(node.fd, F_SETFL, fcntl(node.fd, F_GETFL) | O_NONBLOCK);

	for (msg_vec::iterator iter = msgs.begin(); iter != msgs.end(); ++iter) {
		struct cec_msg msg = *iter;

		fflush(stdout);
		if (!cec_msg_is_broadcast(&msg) && !options[OptTo]) {
			fprintf(stderr, "attempting to send message without --to\n");
			exit(1);
		}
		if (msg.msg[0] == 0xf0)
			msg.msg[0] = first_to;
		msg.msg[0] |= from << 4;
		to = msg.msg[0] & 0xf;
		printf("\nTransmit from %s to %s (%d to %d):\n",
		       la2s(from), to == 0xf ? "all" : la2s(to), from, to);
		msg.flags = options[OptReplyToFollowers] ? CEC_MSG_FL_REPLY_TO_FOLLOWERS : 0;
		msg.timeout = msg.reply ? timeout : 0;
		log_msg(&msg);
		if (doioctl(&node, CEC_TRANSMIT, &msg))
			continue;
		if (msg.rx_status & (CEC_RX_STATUS_OK | CEC_RX_STATUS_FEATURE_ABORT)) {
			printf("    Received from %s (%d):\n    ", la2s(cec_msg_initiator(&msg)),
			       cec_msg_initiator(&msg));
			log_msg(&msg);
			if (options[OptShowRaw])
				log_raw_msg(&msg);
		}
		printf("\tSequence: %u Tx Timestamp: %s",
			msg.sequence, ts2s(msg.tx_ts).c_str());
		if (msg.rx_ts)
			printf(" Rx Timestamp: %s\n\tApproximate response time: %u ms",
				ts2s(msg.rx_ts).c_str(),
				response_time_ms(msg));
		printf("\n");
		if (!cec_msg_status_is_ok(&msg) || verbose)
			printf("\t%s\n", status2s(msg).c_str());
	}
	fflush(stdout);

	if (options[OptNonBlocking])
		fcntl(node.fd, F_SETFL, fcntl(node.fd, F_GETFL) & ~O_NONBLOCK);

skip_la:
	if (options[OptMonitor] || options[OptMonitorAll] ||
	    options[OptMonitorPin] || options[OptStorePin])
		monitor(node, monitor_time, store_pin);
	else if (options[OptWaitForMsgs])
		wait_for_msgs(node, monitor_time);
	fflush(stdout);
	close(fd);
	return 0;
}
