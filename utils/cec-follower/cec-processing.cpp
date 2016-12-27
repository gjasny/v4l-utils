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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <config.h>

#include "cec-follower.h"

#define VOLUME_MAX 0x64
#define VOLUME_MIN 0

/* States for the RC handling */
#define NOPRESS 0
#define PRESS 1
#define PRESS_HOLD 2

/* The follower safety timeout as defined in the spec */
#define FOLLOWER_SAFETY_TIMEOUT 450
#define MIN_INITIATOR_REP_TIME 200
#define MAX_INITIATOR_REP_TIME 500

/* Time between each polling message sent to a device */
#define POLL_PERIOD 15000

#define ARRAY_SIZE(a) \
	(sizeof(a) / sizeof(*a))

struct cec_enum_values {
	const char *type_name;
	__u8 value;
};

struct la_info la_info[15];

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

static const char *get_ui_cmd_string(__u8 ui_cmd)
{
	for (unsigned i = 0; i < ARRAY_SIZE(type_ui_cmd); i++) {
		if (type_ui_cmd[i].value == ui_cmd)
			return type_ui_cmd[i].type_name;
	}
	return "Unknown";
}

static void log_event(struct cec_event &ev)
{
	__u16 pa;

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

static void reply_feature_abort(struct node *node, struct cec_msg *msg, __u8 reason = CEC_OP_ABORT_UNRECOGNIZED_OP)
{
	unsigned la = cec_msg_initiator(msg);
	__u8 opcode = cec_msg_opcode(msg);
	__u64 ts_now = get_ts();

	if (cec_msg_is_broadcast(msg))
		return;
	if (reason == CEC_OP_ABORT_UNRECOGNIZED_OP) {
		la_info[la].feature_aborted[opcode].count++;
		if (la_info[la].feature_aborted[opcode].count == 2) {
			/* If the Abort Reason was "Unrecognized opcode", the Initiator should not send
			   the same message to the same Follower again at that time to avoid saturating
			   the bus. */
			warn("Received message %s from LA %d (%s) shortly after\n",
			     opcode2s(msg).c_str(), la, la2s(la));
			warn("replying Feature Abort [Unrecognized Opcode] to the same message.\n");
		}
	}
	else if (la_info[la].feature_aborted[opcode].count) {
		warn("Replying Feature Abort with abort reason different than [Unrecognized Opcode]\n");
		warn("to message that has previously been replied Feature Abort to with [Unrecognized Opcode].\n");
	}
	else
		la_info[la].feature_aborted[opcode].ts = ts_now;

	cec_msg_reply_feature_abort(msg, reason);
	transmit(node, msg);
}

static bool exit_standby(struct node *node)
{
	if (node->state.power_status == CEC_OP_POWER_STATUS_STANDBY ||
	    node->state.power_status == CEC_OP_POWER_STATUS_TO_STANDBY) {
		node->state.old_power_status = node->state.power_status;
		node->state.power_status = CEC_OP_POWER_STATUS_ON;
		node->state.power_status_changed_time = time(NULL);
		dev_info("Changing state to on\n");
		return true;
	}
	return false;
}

static bool enter_standby(struct node *node)
{
	if (node->state.power_status == CEC_OP_POWER_STATUS_ON ||
	    node->state.power_status == CEC_OP_POWER_STATUS_TO_ON) {
		node->state.old_power_status = node->state.power_status;
		node->state.power_status = CEC_OP_POWER_STATUS_STANDBY;
		node->state.power_status_changed_time = time(NULL);
		dev_info("Changing state to standby\n");
		return true;
	}
	return false;
}

static unsigned get_duration_ms(__u64 ts_a, __u64 ts_b)
{
	return (ts_a - ts_b) / 1000000;
}

static void rc_press_hold_stop(const struct state *state)
{
	unsigned mean_duration = state->rc_duration_sum / state->rc_press_hold_count;

	dev_info("Stop Press and Hold. Mean duration between User Control Pressed messages: %dms\n",
		 mean_duration);
	if (mean_duration < MIN_INITIATOR_REP_TIME) {
		warn("The mean duration between User Control Pressed messages is lower\n");
		warn("than the Minimum Initiator Repetition Time (200ms).\n");
	}
	if (mean_duration > MAX_INITIATOR_REP_TIME) {
		warn("The mean duration between User Control Pressed messages is higher\n");
		warn("than the Maximum Initiator Repetition Time (500ms).\n");
	}
}

static __u16 pa_common_mask(__u16 pa1, __u16 pa2)
{
	__u16 mask = 0xf000;

	for (int i = 0; i < 3; i++) {
		if ((pa1 & mask) != (pa2 & mask))
			break;
		mask = (mask >> 4) | 0xf000;
	}
	return mask << 4;
}
static bool pa_are_adjacent(__u16 pa1, __u16 pa2)
{
	const __u16 mask = pa_common_mask(pa1, pa2);
	const __u16 trail_mask = ((~mask) & 0xffff) >> 4;

	if (pa1 == CEC_PHYS_ADDR_INVALID || pa2 == CEC_PHYS_ADDR_INVALID || pa1 == pa2)
		return false;
	if ((pa1 & trail_mask) || (pa2 & trail_mask))
		return false;
	if (!((pa1 & ~mask) && (pa2 & ~mask)))
		return true;
	return false;
}

static bool pa_is_upstream_from(__u16 pa1, __u16 pa2)
{
	const __u16 mask = pa_common_mask(pa1, pa2);

	if (pa1 == CEC_PHYS_ADDR_INVALID || pa2 == CEC_PHYS_ADDR_INVALID)
		return false;
	if (!(pa1 & ~mask) && (pa2 & ~mask))
		return true;
	return false;
}

static void processMsg(struct node *node, struct cec_msg &msg, unsigned me)
{
	__u8 to = cec_msg_destination(&msg);
	__u8 from = cec_msg_initiator(&msg);
	bool is_bcast = cec_msg_is_broadcast(&msg);
	__u16 remote_pa = (from < 15) ? node->remote_phys_addr[from] : CEC_PHYS_ADDR_INVALID;
	const time_t time_to_transient = 4;
	const time_t time_to_stable = 8;

	switch (msg.msg[1]) {

		/* OSD Display */

	case CEC_MSG_SET_OSD_STRING: {
		__u8 disp_ctl;
		char osd[14];

		if (to != 0 && to != 14)
			break;
		if (!node->has_osd_string)
			break;

		cec_ops_set_osd_string(&msg, &disp_ctl, osd);
		switch (disp_ctl) {
		case CEC_OP_DISP_CTL_DEFAULT:
		case CEC_OP_DISP_CTL_UNTIL_CLEARED:
		case CEC_OP_DISP_CTL_CLEAR:
			return;
		}
		cec_msg_reply_feature_abort(&msg, CEC_OP_ABORT_INVALID_OP);
		transmit(node, &msg);
		return;
	}


		/* Give Device Power Status */

	case CEC_MSG_GIVE_DEVICE_POWER_STATUS: {
		__u8 report_status;

		if (time(NULL) - node->state.power_status_changed_time <= time_to_transient)
			report_status = node->state.old_power_status;
		else if (time(NULL) - node->state.power_status_changed_time >= time_to_stable)
			report_status = node->state.power_status;
		else if (node->state.power_status == CEC_OP_POWER_STATUS_ON)
			report_status = CEC_OP_POWER_STATUS_TO_ON;
		else
			report_status = CEC_OP_POWER_STATUS_TO_STANDBY;

		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_report_power_status(&msg, report_status);
		transmit(node, &msg);
		return;
	}
	case CEC_MSG_REPORT_POWER_STATUS:
		// Nothing to do here for now.
		return;


		/* Standby */

	case CEC_MSG_STANDBY:
		if (node->state.power_status == CEC_OP_POWER_STATUS_ON ||
		    node->state.power_status == CEC_OP_POWER_STATUS_TO_ON) {
			node->state.old_power_status = node->state.power_status;
			node->state.power_status = CEC_OP_POWER_STATUS_STANDBY;
			node->state.power_status_changed_time = time(NULL);
			dev_info("Changing state to standby\n");
		}
		return;


		/* One Touch Play and Routing Control */

	case CEC_MSG_ACTIVE_SOURCE: {
		__u16 phys_addr;

		cec_ops_active_source(&msg, &phys_addr);
		node->state.active_source_pa = phys_addr;
		dev_info("New active source: %x.%x.%x.%x\n", cec_phys_addr_exp(phys_addr));
		return;
	}
	case CEC_MSG_IMAGE_VIEW_ON:
	case CEC_MSG_TEXT_VIEW_ON:
		if (!cec_has_tv(1 << me))
			break;
		exit_standby(node);
		return;
	case CEC_MSG_SET_STREAM_PATH: {
		__u16 phys_addr;

		cec_ops_set_stream_path(&msg, &phys_addr);
		if (phys_addr != node->phys_addr)
			return;

		if (!cec_has_tv(1 << me))
			exit_standby(node);

		cec_msg_init(&msg, me, CEC_LOG_ADDR_BROADCAST);
		cec_msg_active_source(&msg, node->phys_addr);
		transmit(node, &msg);
		dev_info("Stream Path is directed to this device\n");
		return;
	}
	case CEC_MSG_ROUTING_INFORMATION: {
		__u8 la = cec_msg_initiator(&msg);

		if (cec_has_tv(1 << la) && la_info[la].phys_addr == 0)
			warn("TV (0) at 0.0.0.0 sent Routing Information.");
	}


		/* System Information */

	case CEC_MSG_GET_MENU_LANGUAGE:
		if (!cec_has_tv(1 << me))
			break;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_set_menu_language(&msg, node->state.menu_language);
		transmit(node, &msg);
		return;
	case CEC_MSG_CEC_VERSION:
		return;
	case CEC_MSG_REPORT_PHYSICAL_ADDR: {
		__u16 phys_addr;
		__u8 prim_dev_type;

		cec_ops_report_physical_addr(&msg, &phys_addr, &prim_dev_type);
		if (from < 15)
			node->remote_phys_addr[from] = phys_addr;
		return;
	}


		/* Remote Control Passthrough
		   (System Audio Control, Device Menu Control) */

	case CEC_MSG_USER_CONTROL_PRESSED: {
		struct cec_op_ui_command rc_press;
		unsigned new_state;
		unsigned duration;

		cec_ops_user_control_pressed(&msg, &rc_press);
		duration = get_duration_ms(msg.rx_ts, node->state.rc_press_rx_ts);

		new_state = PRESS;
		if (node->state.rc_state == NOPRESS)
			dev_info("Button press: %s\n", get_ui_cmd_string(rc_press.ui_cmd));
		else if (rc_press.ui_cmd != node->state.rc_ui_cmd) {
			/* We have not yet received User Control Released, but have received
			   another User Control Pressed with a different UI Command. */
			if (node->state.rc_state == PRESS_HOLD)
				rc_press_hold_stop(&node->state);
			dev_info("Button press (no User Control Released between): %s\n",
				 get_ui_cmd_string(rc_press.ui_cmd));

			/* The device shall send User Control Released if the time between
			   two messages is longer than the maximum Initiator Repetition Time. */
			if (duration > MAX_INITIATOR_REP_TIME)
				warn("Device waited more than the maximum Initiatior Repetition Time and should have sent a User Control Released message.");
		} else if (duration < FOLLOWER_SAFETY_TIMEOUT) {
			/* We have not yet received a User Control Released, but received
			   another User Control Pressed, with the same UI Command as the
			   previous, which means that the Press and Hold behavior should
			   be invoked. */
			new_state = PRESS_HOLD;
			if (node->state.rc_state != PRESS_HOLD) {
				dev_info("Start Press and Hold with button %s\n",
					 get_ui_cmd_string(rc_press.ui_cmd));
				node->state.rc_duration_sum = 0;
				node->state.rc_press_hold_count = 0;
			}
			node->state.rc_duration_sum += duration;
			node->state.rc_press_hold_count++;
		}

		node->state.rc_state = new_state;
		node->state.rc_ui_cmd = rc_press.ui_cmd;
		node->state.rc_press_rx_ts = msg.rx_ts;

		switch (rc_press.ui_cmd) {
		case 0x41:
			if (node->state.volume < VOLUME_MAX)
				node->state.volume++;
			break;
		case 0x42:
			if (node->state.volume > VOLUME_MIN)
				node->state.volume--;
			break;
		case 0x43:
			node->state.mute = !node->state.mute;
			break;
		case 0x6B:
			if (!enter_standby(node))
				exit_standby(node);
			break;
		case 0x6C:
			enter_standby(node);
			break;
		case 0x6D:
			exit_standby(node);
			break;
		}
		if (rc_press.ui_cmd >= 0x41 && rc_press.ui_cmd <= 0x43) {
			dev_info("Volume: %d%s\n", node->state.volume, node->state.mute ? ", muted" : "");
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_report_audio_status(&msg, node->state.mute ? 1 : 0, node->state.volume);
			transmit(node, &msg);
		}

		return;
	}
	case CEC_MSG_USER_CONTROL_RELEASED:
		if (node->state.rc_state == PRESS_HOLD)
			rc_press_hold_stop(&node->state);

		if (node->state.rc_state == NOPRESS)
			dev_info("Button release (unexpected, %ums after last press)\n",
				 ts_to_ms(msg.rx_ts - node->state.rc_press_rx_ts));
		else if (get_duration_ms(msg.rx_ts, node->state.rc_press_rx_ts) > FOLLOWER_SAFETY_TIMEOUT)
			dev_info("Button release: %s (after timeout, %ums after last press)\n",
				 get_ui_cmd_string(node->state.rc_ui_cmd),
				 ts_to_ms(msg.rx_ts - node->state.rc_press_rx_ts));
		else
			dev_info("Button release: %s (%ums after last press)\n",
				 get_ui_cmd_string(node->state.rc_ui_cmd),
				 ts_to_ms(msg.rx_ts - node->state.rc_press_rx_ts));
		node->state.rc_state = NOPRESS;
		return;


		/* Device Menu Control */

	case CEC_MSG_MENU_REQUEST: {
		if (node->cec_version < CEC_OP_CEC_VERSION_2_0 &&
		    !cec_has_tv(1 << me)) {
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_menu_status(&msg, CEC_OP_MENU_STATE_ACTIVATED);
			transmit(node, &msg);
			return;
		}
		break;
	}
	case CEC_MSG_MENU_STATUS:
		if (node->cec_version < CEC_OP_CEC_VERSION_2_0 &&
		    cec_has_tv(1 << me))
			return;
		break;


		/*
		  Deck Control

		  This is only a basic implementation.

		  TODO: Device state should reflect whether we are playing,
		  fast forwarding, etc.
		*/

	case CEC_MSG_GIVE_DECK_STATUS:
		if (node->has_deck_ctl) {
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_deck_status(&msg, CEC_OP_DECK_INFO_STOP);
			transmit(node, &msg);
			return;
		}
		break;
	case CEC_MSG_PLAY:
		if (node->has_deck_ctl)
			return;
		break;
	case CEC_MSG_DECK_CONTROL:
		if (node->has_deck_ctl)
			return;
		break;
	case CEC_MSG_DECK_STATUS:
		return;


		/*
		  Tuner Control

		  This is only a basic implementation.

		  TODO: Device state should change when selecting services etc.
		*/

	case CEC_MSG_GIVE_TUNER_DEVICE_STATUS: {
		if (!cec_has_tuner(1 << me))
			break;

		struct cec_op_tuner_device_info tuner_dev_info = {};

		cec_msg_set_reply_to(&msg, &msg);
		tuner_dev_info.rec_flag = CEC_OP_REC_FLAG_NOT_USED;
		tuner_dev_info.tuner_display_info = CEC_OP_TUNER_DISPLAY_INFO_NONE;
		tuner_dev_info.is_analog = false;
		tuner_dev_info.digital.service_id_method = CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL;
		tuner_dev_info.digital.dig_bcast_system = CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_C;
		tuner_dev_info.digital.channel.channel_number_fmt = CEC_OP_CHANNEL_NUMBER_FMT_1_PART;
		tuner_dev_info.digital.channel.minor = 1;

		cec_msg_tuner_device_status(&msg, &tuner_dev_info);
		transmit(node, &msg);
		return;
	}

	case CEC_MSG_TUNER_DEVICE_STATUS:
		return;

	case CEC_MSG_SELECT_ANALOGUE_SERVICE:
	case CEC_MSG_SELECT_DIGITAL_SERVICE:
	case CEC_MSG_TUNER_STEP_DECREMENT:
	case CEC_MSG_TUNER_STEP_INCREMENT:
		if (!cec_has_tuner(1 << me))
			break;
		return;


		/*
		  One Touch Record

		  This is only a basic implementation.

		  TODO:
		  - If we are a TV, we should only send Record On if the
		    remote end is a Recording device or Reserved. Otherwise ignore.

		  - Device state should reflect whether we are recording, etc. In
		    recording mode we should ignore Standby messages.
		*/

	case CEC_MSG_RECORD_TV_SCREEN: {
		if (!node->has_rec_tv)
			break;

		struct cec_op_record_src rec_src = {};

		rec_src.type = CEC_OP_RECORD_SRC_OWN;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_record_on(&msg, false, &rec_src);
		transmit(node, &msg);
		return;
	}
	case CEC_MSG_RECORD_ON:
		if (!cec_has_record(1 << me))
			break;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_record_status(&msg, CEC_OP_RECORD_STATUS_CUR_SRC);
		transmit(node, &msg);
		return;
	case CEC_MSG_RECORD_OFF:
		if (!cec_has_record(1 << me))
			break;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_record_status(&msg, CEC_OP_RECORD_STATUS_TERMINATED_OK);
		transmit(node, &msg);
		return;
	case CEC_MSG_RECORD_STATUS:
		return;


		/*
		  Timer Programming

		  This is only a basic implementation.

		  TODO/Ideas:
		  - Act like an actual recording device; keep track of recording
		    schedule and act correctly when colliding timers are set.
		  - Emulate a finite storage space for recordings
		 */

	case CEC_MSG_SET_ANALOGUE_TIMER:
	case CEC_MSG_SET_DIGITAL_TIMER:
	case CEC_MSG_SET_EXT_TIMER:
		if (!cec_has_record(1 << me))
			break;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_timer_status(&msg, CEC_OP_TIMER_OVERLAP_WARNING_NO_OVERLAP,
				     CEC_OP_MEDIA_INFO_NO_MEDIA,
				     CEC_OP_PROG_INFO_ENOUGH_SPACE, 0, 0, 0);
		transmit(node, &msg);
		return;
	case CEC_MSG_CLEAR_ANALOGUE_TIMER:
	case CEC_MSG_CLEAR_DIGITAL_TIMER:
	case CEC_MSG_CLEAR_EXT_TIMER:
		if (!cec_has_record(1 << me))
			break;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_timer_cleared_status(&msg, CEC_OP_TIMER_CLR_STAT_CLEARED);
		transmit(node, &msg);
		return;
	case CEC_MSG_SET_TIMER_PROGRAM_TITLE:
		if (!cec_has_record(1 << me))
			break;
		return;
	case CEC_MSG_TIMER_CLEARED_STATUS:
	case CEC_MSG_TIMER_STATUS:
		return;

		/* Dynamic Auto Lipsync */

	case CEC_MSG_REQUEST_CURRENT_LATENCY: {
		__u16 phys_addr;

		cec_ops_request_current_latency(&msg, &phys_addr);
		if (phys_addr == node->phys_addr) {
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_report_current_latency(&msg, phys_addr,
						       node->state.video_latency,
						       node->state.low_latency_mode,
						       node->state.audio_out_compensated,
						       node->state.audio_out_delay);
			transmit(node, &msg);
		}
		return;
	}


		/* Audio Return Channel Control */

	case CEC_MSG_INITIATE_ARC:
		if (node->has_arc_tx) {
			if (!pa_is_upstream_from(node->phys_addr, remote_pa) ||
			    !pa_are_adjacent(node->phys_addr, remote_pa)) {
				cec_msg_reply_feature_abort(&msg, CEC_OP_ABORT_REFUSED);
				transmit(node, &msg);
				return;
			}
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_report_arc_initiated(&msg);
			transmit(node, &msg);
			node->state.arc_active = true;
			dev_info("ARC is initiated");
			return;
		}
		break;
	case CEC_MSG_TERMINATE_ARC:
		if (node->has_arc_tx) {
			if (!pa_is_upstream_from(node->phys_addr, remote_pa) ||
			    !pa_are_adjacent(node->phys_addr, remote_pa)) {
				cec_msg_reply_feature_abort(&msg, CEC_OP_ABORT_REFUSED);
				transmit(node, &msg);
				return;
			}
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_report_arc_terminated(&msg);
			transmit(node, &msg);
			node->state.arc_active = false;
			dev_info("ARC is terminated\n");
			return;
		}
		break;
	case CEC_MSG_REQUEST_ARC_INITIATION:
		if (node->has_arc_rx) {
			if (pa_is_upstream_from(node->phys_addr, remote_pa) ||
			    !pa_are_adjacent(node->phys_addr, remote_pa)) {
				cec_msg_reply_feature_abort(&msg, CEC_OP_ABORT_REFUSED);
				transmit(node, &msg);
				return;
			}
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_initiate_arc(&msg, false);
			transmit(node, &msg);
			dev_info("ARC initiation has been requested.");
			return;
		}
		break;
	case CEC_MSG_REQUEST_ARC_TERMINATION:
		if (node->has_arc_rx) {
			if (pa_is_upstream_from(node->phys_addr, remote_pa) ||
			    !pa_are_adjacent(node->phys_addr, remote_pa)) {
				cec_msg_reply_feature_abort(&msg, CEC_OP_ABORT_REFUSED);
				transmit(node, &msg);
				return;
			}
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_terminate_arc(&msg, false);
			transmit(node, &msg);
			dev_info("ARC initiation has been requested.");
			return;
		}
		break;
	case CEC_MSG_REPORT_ARC_INITIATED:
		node->state.arc_active = true;
		dev_info("ARC is initiated\n");
		return;
	case CEC_MSG_REPORT_ARC_TERMINATED:
		node->state.arc_active = false;
		dev_info("ARC is terminated\n");
		return;


		/* System Audio Control */

	case CEC_MSG_SYSTEM_AUDIO_MODE_REQUEST: {
		if (!cec_has_audiosystem(1 << me))
			break;

		__u16 phys_addr;

		cec_ops_system_audio_mode_request(&msg, &phys_addr);
		cec_msg_init(&msg, me, CEC_LOG_ADDR_BROADCAST);
		if (phys_addr != CEC_PHYS_ADDR_INVALID) {
			cec_msg_set_system_audio_mode(&msg, CEC_OP_SYS_AUD_STATUS_ON);
			transmit(node, &msg);
			node->state.sac_active = true;
		}
		else {
			cec_msg_set_system_audio_mode(&msg, CEC_OP_SYS_AUD_STATUS_OFF);
			transmit(node, &msg);
			node->state.sac_active = false;
		}
		dev_info("System Audio Mode: %s\n", node->state.sac_active ? "on" : "off");
		return;
	}
	case CEC_MSG_SET_SYSTEM_AUDIO_MODE: {
		__u8 system_audio_status;

		if (!cec_msg_is_broadcast(&msg)) {
			/* Directly addressed Set System Audio Mode is used to see
			   if the TV supports the feature. If we time out, we
			   signalize that we support SAC. */
			if (cec_has_tv(1 << me))
				return;
			else
				break;
		}
		cec_ops_set_system_audio_mode(&msg, &system_audio_status);
		if (system_audio_status == CEC_OP_SYS_AUD_STATUS_ON)
			node->state.sac_active = true;
		else if (system_audio_status == CEC_OP_SYS_AUD_STATUS_OFF)
			node->state.sac_active = false;
		dev_info("System Audio Mode: %s\n", node->state.sac_active ? "on" : "off");
		return;
	}
	case CEC_MSG_REPORT_AUDIO_STATUS:
		return;
	case CEC_MSG_GIVE_SYSTEM_AUDIO_MODE_STATUS:
		if (!cec_has_audiosystem(1 << me))
			break;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_system_audio_mode_status(&msg, node->state.sac_active ? CEC_OP_SYS_AUD_STATUS_ON :
						 CEC_OP_SYS_AUD_STATUS_OFF);
		transmit(node, &msg);
	case CEC_MSG_GIVE_AUDIO_STATUS:
		if (!cec_has_audiosystem(1 << me))
			break;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_report_audio_status(&msg, node->state.mute ? 1 : 0, node->state.volume);
		transmit(node, &msg);
		return;
	case CEC_MSG_SYSTEM_AUDIO_MODE_STATUS:
		return;
	case CEC_MSG_REQUEST_SHORT_AUDIO_DESCRIPTOR: {
		if (!cec_has_audiosystem(1 << me))
			break;

		/* The list of formats that the follower 'supports' */
		const struct short_audio_desc supported_formats[] = {
			{ 2, SAD_FMT_CODE_AC3,
			  SAD_SAMPLE_FREQ_MASK_32 | SAD_SAMPLE_FREQ_MASK_44_1,
			  64, 0, 0 },
			{ 4, SAD_FMT_CODE_AC3,
			  SAD_SAMPLE_FREQ_MASK_32,
			  32, 0, 0 },
			{ 4, SAD_FMT_CODE_ONE_BIT_AUDIO,
			  SAD_SAMPLE_FREQ_MASK_48 | SAD_SAMPLE_FREQ_MASK_192,
			  123, 0, 0 },
			{ 8, SAD_FMT_CODE_EXTENDED,
			  SAD_SAMPLE_FREQ_MASK_96,
			  0, 0, SAD_EXT_TYPE_DRA },
			{ 2, SAD_FMT_CODE_EXTENDED,
			  SAD_SAMPLE_FREQ_MASK_176_4,
			  SAD_FRAME_LENGTH_MASK_960 | SAD_FRAME_LENGTH_MASK_1024, 1, SAD_EXT_TYPE_MPEG4_HE_AAC_SURROUND },
			{ 2, SAD_FMT_CODE_EXTENDED,
			  SAD_SAMPLE_FREQ_MASK_44_1,
			  SAD_BIT_DEPTH_MASK_16 | SAD_BIT_DEPTH_MASK_24, 0, SAD_EXT_TYPE_LPCM_3D_AUDIO },
		};

		__u8 num_descriptors, audio_format_id[4], audio_format_code[4];
		__u32 descriptors[4];
		std::string format_list;

		cec_ops_request_short_audio_descriptor(&msg, &num_descriptors, audio_format_id, audio_format_code);
		if (num_descriptors == 0) {
			warn("Got Request Short Audio Descriptor with no operands");
			reply_feature_abort(node, &msg, CEC_OP_ABORT_INVALID_OP);
			return;
		}

		unsigned found_descs = 0;

		for (int i = 0; i < num_descriptors; i++)
			format_list += audio_format_id_code2s(audio_format_id[i], audio_format_code[i]) + ",";
		format_list.erase(format_list.end() - 1);
		dev_info("Requested descriptors: %s\n", format_list.c_str());
		for (unsigned i = 0; i < num_descriptors; i++) {
			for (unsigned j = 0; j < ARRAY_SIZE(supported_formats); j++) {
				if (found_descs >= 4)
					break;
				if ((audio_format_id[i] == 0 &&
				     audio_format_code[i] == supported_formats[j].format_code) ||
				    (audio_format_id[i] == 1 &&
				     audio_format_code[i] == supported_formats[j].extension_type_code))
					sad_encode(&supported_formats[j], &descriptors[found_descs++]);
			}
		}

		if (found_descs > 0) {
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_report_short_audio_descriptor(&msg, found_descs, descriptors);
			transmit(node, &msg);
		}
		else
			reply_feature_abort(node, &msg, CEC_OP_ABORT_INVALID_OP);
		return;
	}


		/* Device OSD Transfer */

	case CEC_MSG_SET_OSD_NAME:
		return;


		/*
		  Audio Rate Control

		  This is only a basic implementation.

		  TODO: Set Audio Rate shall be sent at least every 2 seconds by
		  the controlling device. This should be checked and kept track of.
		*/

	case CEC_MSG_SET_AUDIO_RATE:
		if (node->has_aud_rate)
			return;
		break;


		/* CDC */

	case CEC_MSG_CDC_MESSAGE: {
		switch (msg.msg[4]) {
		case CEC_MSG_CDC_HEC_DISCOVER:
			__u16 phys_addr;

			cec_msg_set_reply_to(&msg, &msg);
			cec_ops_cdc_hec_discover(&msg, &phys_addr);
			cec_msg_cdc_hec_report_state(&msg, phys_addr,
						     CEC_OP_HEC_FUNC_STATE_NOT_SUPPORTED,
						     CEC_OP_HOST_FUNC_STATE_NOT_SUPPORTED,
						     CEC_OP_ENC_FUNC_STATE_EXT_CON_NOT_SUPPORTED,
						     CEC_OP_CDC_ERROR_CODE_NONE,
						     1, 0); // We do not support HEC on any HDMI connections
			transmit(node, &msg);
			return;
		}
	}


		/* Core */

	case CEC_MSG_FEATURE_ABORT:
		return;
	default:
		break;
	}

	if (is_bcast)
		return;

	reply_feature_abort(node, &msg);
}

static void poll_remote_devs(struct node *node, unsigned me)
{
	node->remote_la_mask = 0;
	for (unsigned i = 0; i < 15; i++)
		node->remote_phys_addr[i] = CEC_PHYS_ADDR_INVALID;

	if (!(node->caps & CEC_CAP_TRANSMIT))
		return;

	for (unsigned i = 0; i < 15; i++) {
		struct cec_msg msg;

		cec_msg_init(&msg, 0xf, i);

		doioctl(node, CEC_TRANSMIT, &msg);
		if (msg.tx_status & CEC_TX_STATUS_OK) {
			node->remote_la_mask |= 1 << i;
			cec_msg_init(&msg, me, i);
			cec_msg_give_physical_addr(&msg, true);
			doioctl(node, CEC_TRANSMIT, &msg);
			if (cec_msg_status_is_ok(&msg))
				node->remote_phys_addr[i] = (msg.msg[2] << 8) | msg.msg[3];
		}
	}
}

void testProcessing(struct node *node)
{
	struct cec_log_addrs laddrs;
	fd_set rd_fds;
	fd_set ex_fds;
	int fd = node->fd;
	__u32 mode = CEC_MODE_INITIATOR | CEC_MODE_FOLLOWER;
	int me;
	unsigned last_poll_la = 15;

	doioctl(node, CEC_S_MODE, &mode);
	doioctl(node, CEC_ADAP_G_LOG_ADDRS, &laddrs);
	me = laddrs.log_addr[0];

	poll_remote_devs(node, me);

	while (1) {
		int res;
		struct timeval timeval = {};

		timeval.tv_sec = 1;
		FD_ZERO(&rd_fds);
		FD_ZERO(&ex_fds);
		FD_SET(fd, &rd_fds);
		FD_SET(fd, &ex_fds);
		res = select(fd + 1, &rd_fds, NULL, &ex_fds, &timeval);
		if (res < 0)
			break;
		if (FD_ISSET(fd, &ex_fds)) {
			struct cec_event ev;

			res = doioctl(node, CEC_DQEVENT, &ev);
			if (res == ENODEV) {
				printf("Device was disconnected.\n");
				break;
			}
			if (res)
				continue;
			log_event(ev);
			if (ev.event == CEC_EVENT_STATE_CHANGE) {
				dev_info("CEC adapter state change.\n");
				node->phys_addr = ev.state_change.phys_addr;
				doioctl(node, CEC_ADAP_G_LOG_ADDRS, &laddrs);
				node->adap_la_mask = laddrs.log_addr_mask;
				node->state.active_source_pa = CEC_PHYS_ADDR_INVALID;
				me = laddrs.log_addr[0];
				memset(la_info, 0, sizeof(la_info));
			}
		}
		if (FD_ISSET(fd, &rd_fds)) {
			struct cec_msg msg = { };

			res = doioctl(node, CEC_RECEIVE, &msg);
			if (res == ENODEV) {
				printf("Device was disconnected.\n");
				break;
			}
			if (res)
				continue;

			__u8 from = cec_msg_initiator(&msg);
			__u8 to = cec_msg_destination(&msg);
			__u8 opcode = cec_msg_opcode(&msg);

			if (from != CEC_LOG_ADDR_UNREGISTERED &&
			    la_info[from].feature_aborted[opcode].ts &&
			    ts_to_ms(get_ts() - la_info[from].feature_aborted[opcode].ts) < 200) {
				warn("Received message %s from LA %d (%s) less than 200 ms after\n",
				     opcode2s(&msg).c_str(), from, la2s(from));
				warn("replying Feature Abort (not [Unrecognized Opcode]) to the same message.\n");
			}
			if (from != CEC_LOG_ADDR_UNREGISTERED && !la_info[from].ts)
				dev_info("Logical address %d (%s) discovered.\n", from, la2s(from));
			if (show_msgs) {
				printf("    %s to %s (%d to %d): ",
				       la2s(from), to == 0xf ? "all" : la2s(to), from, to);
				log_msg(&msg);
				if (show_info)
					printf("\tSequence: %u Rx Timestamp: %llu.%03llus\n",
					       msg.sequence, msg.rx_ts / 1000000000,
					       (msg.rx_ts % 1000000000) / 1000000);
			}
			if (node->adap_la_mask)
				processMsg(node, msg, me);
		}

		__u64 ts_now = get_ts();
		unsigned poll_la = ts_to_s(ts_now) % 16;

		if (poll_la != last_poll_la && poll_la < 15 && la_info[poll_la].ts &&
		    ts_to_ms(ts_now - la_info[poll_la].ts) > POLL_PERIOD) {
			struct cec_msg msg = {};

			cec_msg_init(&msg, 0xf, poll_la);
			transmit(node, &msg);
			if (msg.tx_status & CEC_TX_STATUS_NACK) {
				dev_info("Logical address %d stopped responding to polling message.\n", poll_la);
				memset(&la_info[poll_la], 0, sizeof(la_info[poll_la]));
				node->remote_la_mask &= ~(1 << poll_la);
				node->remote_phys_addr[poll_la] = CEC_PHYS_ADDR_INVALID;
			}
		}
		last_poll_la = poll_la;

		unsigned ms_since_press = ts_to_ms(ts_now - node->state.rc_press_rx_ts);

		if (ms_since_press > FOLLOWER_SAFETY_TIMEOUT) {
			if (node->state.rc_state == PRESS_HOLD)
				rc_press_hold_stop(&node->state);
			else if (node->state.rc_state == PRESS) {
				dev_info("Button timeout: %s\n", get_ui_cmd_string(node->state.rc_ui_cmd));
				node->state.rc_state = NOPRESS;
			}
		}
	}
	mode = CEC_MODE_INITIATOR;
	doioctl(node, CEC_S_MODE, &mode);
}
