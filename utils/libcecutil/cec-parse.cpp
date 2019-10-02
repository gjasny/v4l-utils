// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
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
#include <ctime>
#include <cerrno>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <linux/cec-funcs.h>
#include "cec-htng-funcs.h"
#include "cec-log.h"
#include "cec-parse.h"

#define xstr(s) str(s)
#define str(s) #s

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
	case CEC_OP_UI_CMD_SELECT_BROADCAST_TYPE:
		ui_command.ui_broadcast_type = ui_bcast_type;
		break;
	case CEC_OP_UI_CMD_SELECT_SOUND_PRESENTATION:
		ui_command.ui_sound_presentation_control = ui_snd_pres_ctl;
		break;
	case CEC_OP_UI_CMD_PLAY_FUNCTION:
		ui_command.play_mode = play_mode;
		break;
	case CEC_OP_UI_CMD_TUNE_FUNCTION:
		ui_command.channel_identifier.channel_number_fmt = channel_number_fmt;
		ui_command.channel_identifier.major = major;
		ui_command.channel_identifier.minor = minor;
		break;
	case CEC_OP_UI_CMD_SELECT_MEDIA_FUNCTION:
		ui_command.ui_function_media = ui_function_media;
		break;
	case CEC_OP_UI_CMD_SELECT_AV_INPUT_FUNCTION:
		ui_command.ui_function_select_av_input = ui_function_select_av_input;
		break;
	case CEC_OP_UI_CMD_SELECT_AUDIO_INPUT_FUNCTION:
		ui_command.ui_function_select_audio_input = ui_function_select_audio_input;
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

int cec_parse_subopt(char **subs, const char * const *subopts, char **value)
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

static unsigned parse_enum(const char *value, const struct cec_arg *a)
{
	if (isdigit(*value))
		return strtoul(value, NULL, 0);
	for (int i = 0; i < a->num_enum_values; i++) {
		if (!strcmp(value, a->values[i].type_name))
			return a->values[i].value;
	}
	return 0;
}

unsigned cec_parse_phys_addr(const char *value)
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

static unsigned parse_latency(const char *value)
{
	char *end;
	unsigned delay = strtoul(value, &end, 0);

	if (!memcmp(end, "ms", 2))
		delay = (delay / 2) + 1;
	if (delay < 1)
		delay = 1;
	else if (delay > 251)
		delay = 251;
	return delay;
}


#define VENDOR_EXTRA \
	"  --vendor-command payload=<byte>[:<byte>]*\n" \
	"                                  Send VENDOR_COMMAND message (" xstr(CEC_MSG_VENDOR_COMMAND) ")\n" \
	"  --vendor-command-with-id vendor-id=<val>,cmd=<byte>[:<byte>]*\n" \
	"                                  Send VENDOR_COMMAND_WITH_ID message (" xstr(CEC_MSG_VENDOR_COMMAND_WITH_ID) ")\n" \
	"  --vendor-remote-button-down rc-code=<byte>[:<byte>]*\n" \
	"                                  Send VENDOR_REMOTE_BUTTON_DOWN message (" xstr(CEC_MSG_VENDOR_REMOTE_BUTTON_DOWN) ")\n"

#include "cec-parse-src-gen.h"
