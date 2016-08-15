/*
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>
#include <string>
#include <linux/cec-funcs.h>

#define CEC_MAX_ARGS 16

#define xstr(s) str(s)
#define str(s) #s

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

#include "cec-log.h"

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

static void log_unknown_msg(const struct cec_msg *msg)
{
	__u32 vendor_id;
	__u16 phys_addr;
	unsigned i;

	switch (msg->msg[1]) {
	case CEC_MSG_VENDOR_COMMAND:
		printf("CEC_MSG_VENDOR_COMMAND:\n");
		printf("\tvendor-specific-data:");
		for (i = 2; i < msg->len; i++)
			printf(" 0x%02x", msg->msg[i]);
		printf("\n");
		break;
	case CEC_MSG_VENDOR_COMMAND_WITH_ID:
		printf("CEC_MSG_VENDOR_COMMAND_WITH_ID:\n");
		cec_ops_device_vendor_id(msg, &vendor_id);
		log_arg(&arg_vendor_id, "vendor-id", vendor_id);
		printf("\tvendor-specific-data:");
		for (i = 5; i < msg->len; i++)
			printf(" 0x%02x", msg->msg[i]);
		printf("\n");
		break;
	case CEC_MSG_VENDOR_REMOTE_BUTTON_DOWN:
		printf("CEC_MSG_VENDOR_REMOTE_BUTTON_DOWN:\n");
		printf("\tvendor-specific-rc-code:");
		for (i = 2; i < msg->len; i++)
			printf(" 0x%02x", msg->msg[i]);
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
