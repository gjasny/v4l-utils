// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
/*
 * Copyright 2016 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>
#include <string>
#include <linux/cec-funcs.h>
#include "cec-htng-funcs.h"
#include "cec-info.h"
#include "../common/cec-log.h"

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

struct message {
	__u8 msg;
	__u8 num_args;
	const char *arg_names[CEC_MAX_ARGS+1];
	const struct arg *args[CEC_MAX_ARGS];
	const char *msg_name;
};

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
static void log_htng_unknown_msg(const struct cec_msg *msg);

#include "cec-log-gen.h"

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
	log_arg(&arg_ui_cmd, arg_name, ui_cmd->ui_cmd);
	if (!ui_cmd->has_opt_arg)
		return;
	switch (ui_cmd->ui_cmd) {
	case CEC_OP_UI_CMD_SELECT_BROADCAST_TYPE:
		log_arg(&arg_ui_bcast_type, "ui-broadcast-type",
			ui_cmd->ui_broadcast_type);
		break;
	case CEC_OP_UI_CMD_SELECT_SOUND_PRESENTATION:
		log_arg(&arg_ui_snd_pres_ctl, "ui-sound-presentation-control",
			ui_cmd->ui_sound_presentation_control);
		break;
	case CEC_OP_UI_CMD_PLAY_FUNCTION:
		log_arg(&arg_u8, "play-mode", ui_cmd->play_mode);
		break;
	case CEC_OP_UI_CMD_TUNE_FUNCTION:
		log_arg(&arg_channel_number_fmt, "channel-number-fmt",
			ui_cmd->channel_identifier.channel_number_fmt);
		log_arg(&arg_u16, "major", ui_cmd->channel_identifier.major);
		log_arg(&arg_u16, "minor", ui_cmd->channel_identifier.minor);
		break;
	case CEC_OP_UI_CMD_SELECT_MEDIA_FUNCTION:
		log_arg(&arg_u8, "ui-function-media", ui_cmd->ui_function_media);
		break;
	case CEC_OP_UI_CMD_SELECT_AV_INPUT_FUNCTION:
		log_arg(&arg_u8, "ui-function-select-av-input", ui_cmd->ui_function_select_av_input);
		break;
	case CEC_OP_UI_CMD_SELECT_AUDIO_INPUT_FUNCTION:
		log_arg(&arg_u8, "ui-function-select-audio-input", ui_cmd->ui_function_select_audio_input);
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

static void log_htng_unknown_msg(const struct cec_msg *msg)
{
	__u32 vendor_id;
	const __u8 *bytes;
	__u8 size;
	unsigned i;

	cec_ops_vendor_command_with_id(msg, &vendor_id, &size, &bytes);
	printf("CEC_MSG_VENDOR_COMMAND_WITH_ID (0x%02x):\n",
	       CEC_MSG_VENDOR_COMMAND_WITH_ID);
	log_arg(&arg_vendor_id, "vendor-id", vendor_id);
	printf("\tvendor-specific-data:");
	for (i = 0; i < size; i++)
		printf(" 0x%02x", bytes[i]);
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
		for (i = 0; i < size; i++)
			printf(" 0x%02x", bytes[i]);
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
			for (i = 0; i < size; i++)
				printf(" 0x%02x", bytes[i]);
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
		for (i = 2; i < msg->len; i++)
			printf(" 0x%02x", msg->msg[i]);
		printf("\n");
		break;
	}
}

const char *ui_cmd_string(__u8 ui_cmd)
{
	for (unsigned i = 0; i < arg_ui_cmd.num_enum_values; i++) {
		if (type_ui_cmd[i].value == ui_cmd)
			return type_ui_cmd[i].type_name;
	}
	return NULL;
}
