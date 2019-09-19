// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
/*
 * Copyright 2016 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#include <sys/ioctl.h>

#include "cec-follower.h"

#define NUM_ANALOG_FREQS	3

/*
 * This table contains analog television channel frequencies in KHz.  There are
 * a total of three frequencies per analog broadcast type and broadcast system.
 *
 * CEC 17 and CEC Table 31 of the 1.4 specification lists the available analog
 * broadcast types and broadcast systems.
 *
 * The table is indexed by [ana_bcast_type][bcast_system][NUM_ANALOG_FREQS].
 *
 * Analog channel frequencies are from Wikipedia:
 *
 * https://en.wikipedia.org/wiki/Television_channel_frequencies
 */
static unsigned int analog_freqs_khz[3][9][NUM_ANALOG_FREQS] =
{
	// cable
	{
		// pal-bg
		{ 471250, 479250, 487250 },
		// secam-lq
		{ 615250, 623250, 631250 },
		// pal-m
		{ 501250, 507250, 513250 },
		// ntsc-m
		{ 519250, 525250, 531250 },
		// pal-i
		{ 45750, 53750, 61750 },
		// secam-dk
		{ 759250, 767250, 775250 },
		// secam-bg
		{ 495250, 503250, 511250 },
		// secam-l
		{ 639250, 647250, 655250 },
		// pal-dk
		{ 783250, 791250, 799250 }
	},
	// satellite
	{
		// pal-bg
		{ 519250, 527250, 535250 },
		// secam-lq
		{ 663250, 671250, 679250 },
		// pal-m
		{ 537250, 543250, 549250 },
		// ntsc-m
		{ 555250, 561250, 567250 },
		// pal-i
		{ 175250, 183250, 191250 },
		// secam-dk
		{ 807250, 815250, 823250 },
		// secam-bg
		{ 543250, 551250, 559250 },
		// secam-l
		{ 687250, 695250, 703250 },
		// pal-dk
		{ 831250, 839250, 847250 }
	},
	// terrestrial
	{
		// pal-bg
		{ 567250, 575250, 583250 },
		// secam-lq
		{ 711250, 719250, 727250 },
		// pal-m
		{ 573250, 579250, 585250 },
		// ntsc-m
		{ 591250, 597250, 603250 },
		// pal-i
		{ 199250, 207250, 215250 },
		// secam-dk
		{ 145250, 153250, 161250 },
		// secam-bg
		{ 591250, 599250, 607250 },
		// secam-l
		{ 735250, 743250, 751250 },
		// pal-dk
		{ 169250, 177250, 185250 }
	}
};

void process_tuner_record_timer_msgs(struct node *node, struct cec_msg &msg, unsigned me)
{
	bool is_bcast = cec_msg_is_broadcast(&msg);

	switch (msg.msg[1]) {


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
	default:
		break;
	}

	if (is_bcast)
		return;

	reply_feature_abort(node, &msg);
}
