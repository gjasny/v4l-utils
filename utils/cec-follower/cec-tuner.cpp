// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
/*
 * Copyright 2016 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#include <array>
#include <string>

#include <sys/ioctl.h>

#include "cec-follower.h"
#include "compiler.h"

#define NUM_ANALOG_FREQS 3
#define NUM_DIGITAL_CHANS 3
#define TOT_ANALOG_FREQS analog_freqs_khz[0][0].size()
#define TOT_DIGITAL_CHANS digital_arib_data[0].size() + digital_atsc_data[0].size() + digital_dvb_data[0].size()

enum Months { Jan = 1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec };

struct service_info {
	unsigned tsid;
	unsigned onid;
	unsigned sid;
	unsigned fmt;
	unsigned major;
	unsigned minor;
};

/*
 * This table contains the digital television channels for ARIB (ISDB).  There
 * are a total of three channels that are identified by digital IDs or by
 * channel.
 *
 * CEC 17 of the 1.4 specification lists the available digital identification
 * methods, IDs, and channel data.
 *
 * Digital channel data for ARIB-BS is from:
 *
 * https://sichbopvr.com/frequency-tables/19-20E
 *
 * No public data was found for ARIB-BS so data is just copied.
 *
 * Digital channel data for ARIB-T is from:
 *
 * https://sichbopvr.com/frequency-tables/Brazil/Rio%20de%20Janeiro/Rio%20De%20Janeiro
 */
using arib = std::array<service_info, NUM_DIGITAL_CHANS>;
static constexpr std::array<arib, 2> digital_arib_data{
	// satellite, arib-bs
	arib{
		// tsid, onid, sid, fmt, major, minor
		service_info{ 1032, 1, 30203, 1, 0, 30203 },
		service_info{ 1046, 1, 30505, 1, 0, 30505 },
		service_info{ 1060, 1, 30609, 1, 0, 30609 },
	},
	// terrestrial, arib-t
	arib{
		// tsid, onid, sid, fmt, major, minor
		service_info{ 1519, 1519, 48608, 1, 0, 48608 },
		service_info{ 1624, 1624, 51992, 1, 0, 51992 },
		service_info{ 1905, 1905, 60960, 1, 0, 60960 },
	},
};

/*
 * This table contains the digital television channels for ATSC.  There
 * are a total of three channels that are identified by digital IDs or by
 * channel.
 *
 * CEC 17 of the 1.4 specification lists the available digital identification
 * methods, IDs, and channel data.
 *
 * Digital channel data for atsc-sat is from:
 *
 * https://sichbopvr.com/frequency-tables/28-50E
 *
 * No public data was found for atsc-sat so data is just copied.
 *
 * Digital channel data for atsc-t is from:
 *
 * https://sichbopvr.com/frequency-tables/United%20States/Illinois/Caseyville
 *
 * ATSC does not use ONIDs and SID will be used as the program number.  All ATSC
 * channel number formats are 2 part.
 */
using atsc = std::array<service_info, NUM_DIGITAL_CHANS>;
static constexpr std::array<atsc, 2> digital_atsc_data{
	// satellite, atsc-sat
	atsc{
		// tsid, onid, sid, fmt, major, minor
		service_info{ 2065, 0, 50316, 2, 3, 50316 },
		service_info{ 2090, 0, 50882, 2, 3, 50882 },
		service_info{ 2122, 0, 55295, 2, 3, 55295 },
	},
	// terrestrial, atsc-t
	atsc{
		// tsid, onid, sid, fmt, major, minor
		service_info{ 1675, 0, 1, 2, 4, 1 },
		service_info{ 1675, 0, 2, 2, 4, 2 },
		service_info{ 1675, 0, 3, 2, 4, 3 },
	},
};

/*
 * This table contains the digital television channels for DVB.  There are a
 * total of three channels that are identified by digital IDs or by channel.
 *
 * CEC 17 of the 1.4 specification lists the available digital identification
 * methods, IDs, and channel data.
 *
 * Digital channel data for DVB-S2 is from:
 *
 * https://www.satellite-calculations.com/DVB/getchannellist.php?1west/Swedish_Nordig_Channel_List.htm
 *
 * Digital channel data for DVB-T is from:
 *
 * https://sichbopvr.com/frequency-tables/Denmark/Hovedstaden/Copenhagen
 * https://sichbopvr.com/frequency-tables/Sweden/Skane%20Lan/Malm%c3%b6
 *
 */
using dvb = std::array<service_info, NUM_DIGITAL_CHANS>;
static constexpr std::array<dvb, 2> digital_dvb_data{
	// satellite, dvb-s2
	dvb{
		// tsid, onid, sid, fmt, major, minor
		service_info{ 61, 70, 7193, 1, 0, 24 },
		service_info{ 65, 70, 7040, 1, 0, 72 },
		service_info{ 28, 70, 7012, 1, 0, 101 },
	},
	// terrestrial, dvb-t
	dvb{
		// tsid, onid, sid, fmt, major, minor
		service_info{ 1002, 8400, 2025, 1, 0, 21 },
		service_info{ 1004, 8400, 84, 1, 0, 31 },
		service_info{ 1004, 8945, 1040, 1, 0, 1040 },
	},
};

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
using khz = std::array<unsigned int, NUM_ANALOG_FREQS>;
using freqs = std::array<khz, 9>;
static constexpr std::array<freqs, 3> analog_freqs_khz{
	// cable
	freqs{
		// pal-bg
		khz{ 471250, 479250, 487250 },
		// secam-lq
		khz{ 615250, 623250, 631250 },
		// pal-m
		khz{ 501250, 507250, 513250 },
		// ntsc-m
		khz{ 519250, 525250, 531250 },
		// pal-i
		khz{ 45750, 53750, 61750 },
		// secam-dk
		khz{ 759250, 767250, 775250 },
		// secam-bg
		khz{ 495250, 503250, 511250 },
		// secam-l
		khz{ 639250, 647250, 655250 },
		// pal-dk
		khz{ 783250, 791250, 799250 },
	},
	// satellite
	freqs{
		// pal-bg
		khz{ 519250, 527250, 535250 },
		// secam-lq
		khz{ 663250, 671250, 679250 },
		// pal-m
		khz{ 537250, 543250, 549250 },
		// ntsc-m
		khz{ 555250, 561250, 567250 },
		// pal-i
		khz{ 175250, 183250, 191250 },
		// secam-dk
		khz{ 807250, 815250, 823250 },
		// secam-bg
		khz{ 543250, 551250, 559250 },
		// secam-l
		khz{ 687250, 695250, 703250 },
		// pal-dk
		khz{ 831250, 839250, 847250 },
	},
	// terrestrial
	freqs{
		// pal-bg
		khz{ 567250, 575250, 583250 },
		// secam-lq
		khz{ 711250, 719250, 727250 },
		// pal-m
		khz{ 573250, 579250, 585250 },
		// ntsc-m
		khz{ 591250, 597250, 603250 },
		// pal-i
		khz{ 199250, 207250, 215250 },
		// secam-dk
		khz{ 145250, 153250, 161250 },
		// secam-bg
		khz{ 591250, 599250, 607250 },
		// secam-l
		khz{ 735250, 743250, 751250 },
		// pal-dk
		khz{ 169250, 177250, 185250 },
	},
};

void tuner_dev_info_init(struct state *state)
{
	struct cec_op_tuner_device_info *info = &state->tuner_dev_info;
	struct cec_op_digital_service_id *digital = &info->digital;

	state->service_idx = 0;
	info->rec_flag = CEC_OP_REC_FLAG_NOT_USED;
	info->tuner_display_info = CEC_OP_TUNER_DISPLAY_INFO_DIGITAL;
	info->is_analog = false;
	digital->service_id_method = state->service_by_dig_id ?
		CEC_OP_SERVICE_ID_METHOD_BY_DIG_ID :
		CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL;
	digital->dig_bcast_system = CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_BS;
	if (state->service_by_dig_id) {
		digital->arib.transport_id = digital_arib_data[0][0].tsid;
		digital->arib.service_id = digital_arib_data[0][0].sid;
		digital->arib.orig_network_id = digital_arib_data[0][0].onid;
	} else {
		digital->channel.channel_number_fmt = digital_arib_data[0][0].fmt;
		digital->channel.major = digital_arib_data[0][0].major;
		digital->channel.minor = digital_arib_data[0][0].minor;
	}
}

static int digital_get_service_offset(const struct service_info *info,
				      const struct cec_op_digital_service_id *digital)
{
	__u8 method = digital->service_id_method;
	const struct cec_op_arib_data *arib = &digital->arib;
	const struct cec_op_atsc_data *atsc = &digital->atsc;
	const struct cec_op_dvb_data *dvb = &digital->dvb;
	const struct cec_op_channel_data *channel = &digital->channel;

	for (int i = 0; i < NUM_DIGITAL_CHANS; i++, info++) {
		switch (method) {
		case CEC_OP_SERVICE_ID_METHOD_BY_DIG_ID:
			switch (digital->dig_bcast_system) {
			case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_BS:
			case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_T:
				if (arib->transport_id == info->tsid &&
				    arib->service_id == info->sid &&
				    arib->orig_network_id == info->onid)
					return i;
				break;
			case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_SAT:
			case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_T:
				if (atsc->transport_id == info->tsid &&
				    atsc->program_number == info->sid)
					return i;
				break;
			case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_S2:
			case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_T:
				if (dvb->transport_id == info->tsid &&
				    dvb->service_id == info->sid &&
				    dvb->orig_network_id == info->onid)
					return i;
				break;
			}
			break;
		case CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL:
			switch (channel->channel_number_fmt) {
			case CEC_OP_CHANNEL_NUMBER_FMT_1_PART:
				if (channel->minor == info->minor)
					return i;
				break;
			case CEC_OP_CHANNEL_NUMBER_FMT_2_PART:
				if (channel->major == info->major &&
				    channel->minor == info->minor)
					return i;
				break;
			}
			break;
		default:
			break;
		}
	}
	return -1;
}

static int digital_get_service_idx(struct cec_op_digital_service_id *digital)
{
	const struct service_info *info;
	bool is_terrestrial = false;
	unsigned offset;
	int idx;

	switch (digital->dig_bcast_system) {
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_T:
		is_terrestrial = true;
		fallthrough;
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_BS:
		info = &digital_arib_data[is_terrestrial][0];
		offset = is_terrestrial * NUM_DIGITAL_CHANS;
		break;
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_T:
		is_terrestrial = true;
		fallthrough;
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_SAT:
		info = &digital_atsc_data[is_terrestrial][0];
		offset = (2 + is_terrestrial) * NUM_DIGITAL_CHANS;
		break;
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_T:
		is_terrestrial = true;
		fallthrough;
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_S2:
		info = &digital_dvb_data[is_terrestrial][0];
		offset = (4 + is_terrestrial) * NUM_DIGITAL_CHANS;
		break;
	default:
		return -1;
	}
	idx = digital_get_service_offset(info, digital);
	return idx >= 0 ? idx + offset : -1;
}

static bool digital_update_tuner_dev_info(struct node *node, int idx,
					  struct cec_msg *msg)
{
	if (idx < 0)
		return false;

	struct cec_op_tuner_device_info *info = &node->state.tuner_dev_info;
	struct cec_op_digital_service_id *digital = &info->digital;
	struct cec_op_arib_data *arib = &digital->arib;
	struct cec_op_dvb_data *dvb = &digital->dvb;
	struct cec_op_atsc_data *atsc = &digital->atsc;
	struct cec_op_channel_data *channel = &digital->channel;
	unsigned int tbl = idx / (NUM_DIGITAL_CHANS * 2);
	unsigned int sys = (idx % (NUM_DIGITAL_CHANS * 2)) / NUM_DIGITAL_CHANS;
	unsigned int offset = idx % NUM_DIGITAL_CHANS;

	info->tuner_display_info = CEC_OP_TUNER_DISPLAY_INFO_DIGITAL;
	info->is_analog = false;
	digital->service_id_method = node->state.service_by_dig_id ?
		CEC_OP_SERVICE_ID_METHOD_BY_DIG_ID :
		CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL;

	switch (tbl) {
	case 0: {
		if (sys)
			digital->dig_bcast_system = CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_T;
		else
			digital->dig_bcast_system = CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_BS;
		if (node->state.service_by_dig_id) {
			arib->transport_id = digital_arib_data[sys][offset].tsid;
			arib->orig_network_id = digital_arib_data[sys][offset].onid;
			arib->service_id = digital_arib_data[sys][offset].sid;
		} else {
			channel->channel_number_fmt = digital_arib_data[sys][offset].fmt;
			channel->major = digital_arib_data[sys][offset].major;
			channel->minor = digital_arib_data[sys][offset].minor;
		}
		break;
	}
	case 1: {
		if (sys)
			digital->dig_bcast_system = CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_T;
		else
			digital->dig_bcast_system = CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_SAT;
		if (node->state.service_by_dig_id) {
			atsc->transport_id = digital_atsc_data[sys][offset].tsid;
			atsc->program_number = digital_atsc_data[sys][offset].sid;
		} else {
			channel->channel_number_fmt = digital_atsc_data[sys][offset].fmt;
			channel->major = digital_atsc_data[sys][offset].major;
			channel->minor = digital_atsc_data[sys][offset].minor;
		}
		break;
	}
	case 2: {
		if (sys)
			digital->dig_bcast_system = CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_T;
		else
			digital->dig_bcast_system = CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_S2;
		if (node->state.service_by_dig_id) {
			dvb->transport_id = digital_dvb_data[sys][offset].tsid;
			dvb->orig_network_id = digital_dvb_data[sys][offset].onid;
			dvb->service_id = digital_dvb_data[sys][offset].sid;
		} else {
			channel->channel_number_fmt = digital_dvb_data[sys][offset].fmt;
			channel->major = digital_dvb_data[sys][offset].major;
			channel->minor = digital_dvb_data[sys][offset].minor;
		}
		break;
	}
	default:
		break;
	}
	if (node->state.service_idx != static_cast<unsigned>(idx) && node->state.tuner_report_changes) {
		cec_msg_set_reply_to(msg, msg);
		cec_msg_tuner_device_status(msg, &node->state.tuner_dev_info);
		transmit(node, msg);
	}
	node->state.service_idx = idx;
	return true;
}

static bool digital_set_tuner_dev_info(struct node *node, struct cec_msg *msg)
{
	struct cec_op_digital_service_id digital = {};

	cec_ops_select_digital_service(msg, &digital);
	return digital_update_tuner_dev_info(node, digital_get_service_idx(&digital), msg);
}

static unsigned int analog_get_nearest_service_idx(__u8 ana_bcast_type, __u8 ana_bcast_system,
						   int ana_freq_khz)
{
	int nearest = analog_freqs_khz[ana_bcast_type][ana_bcast_system][0];
	unsigned int offset = 0;

	for (int i = 0; i < NUM_ANALOG_FREQS; i++) {
		int freq = analog_freqs_khz[ana_bcast_type][ana_bcast_system][i];

		if (abs(ana_freq_khz - freq) < abs(ana_freq_khz - nearest)) {
			nearest = freq;
			offset = i;
		}
	}
	return NUM_ANALOG_FREQS * ((ana_bcast_type * 9) + ana_bcast_system) +
		offset + TOT_DIGITAL_CHANS;
}

static void analog_update_tuner_dev_info(struct node *node, unsigned int idx,
					 struct cec_msg *msg)
{
	struct cec_op_tuner_device_info *info = &node->state.tuner_dev_info;
	unsigned int sys_freqs = NUM_ANALOG_FREQS * 9;
	unsigned int offset;
	unsigned int freq_khz;

	info->tuner_display_info = CEC_OP_TUNER_DISPLAY_INFO_ANALOGUE;
	info->is_analog = true;
	info->analog.ana_bcast_type = (idx - TOT_DIGITAL_CHANS) / sys_freqs;
	info->analog.bcast_system =
		(idx - (sys_freqs * info->analog.ana_bcast_type + TOT_DIGITAL_CHANS)) / NUM_ANALOG_FREQS;
	offset = idx % NUM_ANALOG_FREQS;
	freq_khz = analog_freqs_khz[info->analog.ana_bcast_type][info->analog.bcast_system][offset];
	info->analog.ana_freq = (freq_khz * 10) / 625;
	if (node->state.service_idx != idx && node->state.tuner_report_changes) {
		cec_msg_set_reply_to(msg, msg);
		cec_msg_tuner_device_status(msg, &node->state.tuner_dev_info);
		transmit(node, msg);
	}
	node->state.service_idx = idx;
}

static bool analog_set_tuner_dev_info(struct node *node, struct cec_msg *msg)
{
	__u8 type;
	__u16 freq;
	__u8 system;
	unsigned int idx;

	cec_ops_select_analogue_service(msg, &type, &freq, &system);
	if (type < 3 && system < 9) {
		int freq_khz = (freq * 625) / 10;

		idx = analog_get_nearest_service_idx(type, system, freq_khz);
		analog_update_tuner_dev_info(node, idx, msg);
		return true;
	}
	return false;
}

static bool digital_operand_invalid(const struct cec_op_record_src &rec_src)
{
	switch (rec_src.digital.dig_bcast_system) {
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_GEN:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_GEN:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_GEN:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_BS:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_CS:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_T:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_CABLE:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_SAT:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_T:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_C:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_S:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_S2:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_T:
		break;
	default:
		return true;
	}

	if (rec_src.digital.service_id_method == CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL) {
		if (rec_src.digital.channel.channel_number_fmt < CEC_OP_CHANNEL_NUMBER_FMT_1_PART ||
		    rec_src.digital.channel.channel_number_fmt > CEC_OP_CHANNEL_NUMBER_FMT_2_PART)
		    return true;
	}

	return false;
}

static bool analog_operand_invalid(const cec_op_record_src &rec_src)
{
	if (rec_src.analog.ana_bcast_type > CEC_OP_ANA_BCAST_TYPE_TERRESTRIAL)
		return true;

	if (rec_src.analog.bcast_system > CEC_OP_BCAST_SYSTEM_PAL_DK &&
	    rec_src.analog.bcast_system != CEC_OP_BCAST_SYSTEM_OTHER)
		return true;

	if (rec_src.analog.ana_freq == 0 || rec_src.analog.ana_freq == 0xffff)
		return true;

	return false;
}

static bool analog_channel_is_available(const cec_op_record_src &rec_src)
{
	__u8 bcast_type = rec_src.analog.ana_bcast_type;
	unsigned freq = (rec_src.analog.ana_freq * 625) / 10;
	__u8 bcast_system = rec_src.analog.bcast_system;

	for (unsigned i = 0; i < NUM_ANALOG_FREQS; i++) {
		if (freq == analog_freqs_khz[bcast_type][bcast_system][i])
			return true;
	}

	return false;
}

void process_tuner_msgs(struct node *node, struct cec_msg &msg, unsigned me, __u8 type)
{
	bool is_bcast = cec_msg_is_broadcast(&msg);

	/* Tuner Control */
	switch (msg.msg[1]) {
	case CEC_MSG_GIVE_TUNER_DEVICE_STATUS: {
		__u8 status_req;

		if (!cec_has_tuner(1 << me) && !cec_has_tv(1 << me))
			break;

		cec_ops_give_tuner_device_status(&msg, &status_req);
		if (status_req < CEC_OP_STATUS_REQ_ON ||
		    status_req > CEC_OP_STATUS_REQ_ONCE) {
			reply_feature_abort(node, &msg, CEC_OP_ABORT_INVALID_OP);
			return;
		}
		if (status_req != CEC_OP_STATUS_REQ_ONCE)
			node->state.tuner_report_changes =
				status_req == CEC_OP_STATUS_REQ_ON;
		if (status_req == CEC_OP_STATUS_REQ_OFF)
			return;

		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_tuner_device_status(&msg, &node->state.tuner_dev_info);
		transmit(node, &msg);
		return;
	}

	case CEC_MSG_TUNER_DEVICE_STATUS:
		return;

	case CEC_MSG_SELECT_ANALOGUE_SERVICE:
		if (!cec_has_tuner(1 << me) && !cec_has_tv(1 << me))
			break;

		if (node->state.tuner_dev_info.rec_flag == CEC_OP_REC_FLAG_USED) {
			reply_feature_abort(node, &msg, CEC_OP_ABORT_REFUSED);
			return;
		}
		if (!analog_set_tuner_dev_info(node, &msg)) {
			reply_feature_abort(node, &msg, CEC_OP_ABORT_INVALID_OP);
			return;
		}
		return;

	case CEC_MSG_SELECT_DIGITAL_SERVICE:
		if (!cec_has_tuner(1 << me) && !cec_has_tv(1 << me))
			break;

		if (node->state.tuner_dev_info.rec_flag == CEC_OP_REC_FLAG_USED) {
			reply_feature_abort(node, &msg, CEC_OP_ABORT_REFUSED);
			return;
		}
		if (!digital_set_tuner_dev_info(node, &msg)) {
			reply_feature_abort(node, &msg, CEC_OP_ABORT_INVALID_OP);
			return;
		}
		return;

	case CEC_MSG_TUNER_STEP_DECREMENT: {
		if (!cec_has_tuner(1 << me) && !cec_has_tv(1 << me))
			break;

		if (node->state.service_idx == 0)
			node->state.service_idx =
				TOT_DIGITAL_CHANS + TOT_ANALOG_FREQS - 1;
		else
			node->state.service_idx--;
		if (node->state.service_idx < TOT_DIGITAL_CHANS)
			digital_update_tuner_dev_info(node, node->state.service_idx, &msg);
		else
			analog_update_tuner_dev_info(node, node->state.service_idx, &msg);
		return;
	}

	case CEC_MSG_TUNER_STEP_INCREMENT: {
		if (!cec_has_tuner(1 << me) && !cec_has_tv(1 << me))
			break;

		if (node->state.service_idx ==
				TOT_DIGITAL_CHANS + TOT_ANALOG_FREQS - 1)
			node->state.service_idx = 0;
		else
			node->state.service_idx++;
		if (node->state.service_idx < TOT_DIGITAL_CHANS)
			digital_update_tuner_dev_info(node, node->state.service_idx, &msg);
		else
			analog_update_tuner_dev_info(node, node->state.service_idx, &msg);
		return;
	}
	default:
		break;
	}

	if (is_bcast)
		return;

	reply_feature_abort(node, &msg);
}

void process_record_msgs(struct node *node, struct cec_msg &msg, unsigned me, __u8 type)
{
	__u8 from = cec_msg_initiator(&msg);
	bool is_bcast = cec_msg_is_broadcast(&msg);

	/* One Touch Record */
	switch (msg.msg[1]) {
	case CEC_MSG_RECORD_TV_SCREEN: {
		if (!node->has_rec_tv)
			break;

		/* Ignore if initiator is not a recording device */
		if (!cec_has_record(1 << from) && node->remote_prim_devtype[from] != CEC_OP_PRIM_DEVTYPE_RECORD)
			return;

		struct cec_op_record_src rec_src = {};

		rec_src.type = CEC_OP_RECORD_SRC_OWN;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_record_on(&msg, false, &rec_src);
		transmit(node, &msg);
		return;
	}
	case CEC_MSG_RECORD_ON: {
		if (type != CEC_LOG_ADDR_TYPE_RECORD)
			break;

		__u8 rec_status;
		bool feature_abort = false;
		struct cec_op_record_src rec_src = {};

		cec_ops_record_on(&msg, &rec_src);
		switch (rec_src.type) {
		case CEC_OP_RECORD_SRC_OWN:
			rec_status = CEC_OP_RECORD_STATUS_CUR_SRC;
			break;
		case CEC_OP_RECORD_SRC_DIGITAL:
			if (digital_operand_invalid(rec_src)) {
				feature_abort = true;
				break;
			}
			if (digital_get_service_idx(&rec_src.digital) >= 0)
				rec_status = CEC_OP_RECORD_STATUS_DIG_SERVICE;
			else
				rec_status = CEC_OP_RECORD_STATUS_NO_DIG_SERVICE;
			break;
		case CEC_OP_RECORD_SRC_ANALOG:
			if (analog_operand_invalid(rec_src)) {
				feature_abort = true;
				break;
			}
			if (analog_channel_is_available(rec_src))
				rec_status = CEC_OP_RECORD_STATUS_ANA_SERVICE;
			else
				rec_status = CEC_OP_RECORD_STATUS_NO_ANA_SERVICE;
			break;
		case CEC_OP_RECORD_SRC_EXT_PLUG:
			if (rec_src.ext_plug.plug == 0)
				feature_abort = true;
			/* Plug number range is 1-255 in spec, but a realistic range of connectors is 6. */
			else if (rec_src.ext_plug.plug > 6)
				rec_status = CEC_OP_RECORD_STATUS_INVALID_EXT_PLUG;
			else
				rec_status = CEC_OP_RECORD_STATUS_EXT_INPUT;
			break;
		case CEC_OP_RECORD_SRC_EXT_PHYS_ADDR:
			rec_status = CEC_OP_RECORD_STATUS_INVALID_EXT_PHYS_ADDR;
			break;
		default:
			feature_abort = true;
			break;
		}
		if (feature_abort) {
			reply_feature_abort(node, &msg, CEC_OP_ABORT_INVALID_OP);
			return;
		}
		if (node->state.one_touch_record_on)
			rec_status = CEC_OP_RECORD_STATUS_ALREADY_RECORDING;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_record_status(&msg, rec_status);
		transmit(node, &msg);
		node->state.one_touch_record_on = true;
		return;
	}
	case CEC_MSG_RECORD_OFF:
		if (type != CEC_LOG_ADDR_TYPE_RECORD)
			break;

		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_record_status(&msg, CEC_OP_RECORD_STATUS_TERMINATED_OK);
		transmit(node, &msg);
		node->state.one_touch_record_on = false;

		/* Delete any currently active recording timer or it may restart itself in first minute. */
		if (node->state.recording_controlled_by_timer) {
			node->state.recording_controlled_by_timer = false;
			programmed_timers.erase(programmed_timers.begin());
			if (show_info)
				printf("Deleted manually stopped timer.\n");
			print_timers(node);
		}
		/*
		 * If standby was received during recording, enter standby when the
		 * recording is finished unless recording device is the active source.
		 */
		if (node->state.record_received_standby) {
			if (node->phys_addr != node->state.active_source_pa)
				enter_standby(node);
			node->state.record_received_standby = false;
		}
		return;
	case CEC_MSG_RECORD_STATUS:
		return;
	default:
		break;
	}

	if (is_bcast)
		return;

	reply_feature_abort(node, &msg);
}

static struct Timer get_timer_from_message(const struct cec_msg &msg)
{
	struct Timer timer = {};

	__u8 day = 0;
	__u8 month = 0;
	__u8 start_hr = 0;
	__u8 start_min = 0;
	__u8 duration_hr = 0;
	__u8 duration_min = 0;
	__u8 ext_src_spec = 0;
	__u8 plug = 0;
	__u16 phys_addr = 0;

	switch (msg.msg[1]) {
	case CEC_MSG_CLEAR_ANALOGUE_TIMER:
	case CEC_MSG_SET_ANALOGUE_TIMER:
		timer.src.type = CEC_OP_RECORD_SRC_ANALOG;
		cec_ops_set_analogue_timer(&msg, &day, &month, &start_hr, &start_min,
		                           &duration_hr, &duration_min, &timer.recording_seq,
		                           &timer.src.analog.ana_bcast_type, &timer.src.analog.ana_freq,
		                           &timer.src.analog.bcast_system);
		break;
	case CEC_MSG_CLEAR_DIGITAL_TIMER:
	case CEC_MSG_SET_DIGITAL_TIMER: {
		struct cec_op_digital_service_id digital = {};
		timer.src.type = CEC_OP_RECORD_SRC_DIGITAL;
		timer.src.digital = digital;
		cec_ops_set_digital_timer(&msg, &day, &month, &start_hr, &start_min,
		                          &duration_hr, &duration_min, &timer.recording_seq,
		                          &timer.src.digital);
		break;
	}
	case CEC_MSG_CLEAR_EXT_TIMER:
	case CEC_MSG_SET_EXT_TIMER: {
		cec_ops_set_ext_timer(&msg, &day, &month, &start_hr, &start_min,
		                      &duration_hr, &duration_min, &timer.recording_seq, &ext_src_spec,
		                      &plug, &phys_addr);
		if (ext_src_spec == CEC_OP_EXT_SRC_PLUG) {
			timer.src.type = CEC_OP_RECORD_SRC_EXT_PLUG;
			timer.src.ext_plug.plug = plug;
		}
		if (ext_src_spec == CEC_OP_EXT_SRC_PHYS_ADDR) {
			timer.src.type = CEC_OP_RECORD_SRC_EXT_PHYS_ADDR;
			timer.src.ext_phys_addr.phys_addr = phys_addr;
		}
		break;
	}
	default:
		break;
	}

	timer.duration = ((duration_hr * 60) + duration_min) * 60; /* In seconds. */

	/* Use current time in the timer when it is not available from message e.g. year. */
	time_t current_time = time(nullptr);
	struct tm *temp = localtime(&current_time);
	temp->tm_mday = day;
	temp->tm_mon = month - 1; /* CEC months are 1-12 but struct tm range is 0-11. */
	temp->tm_hour = start_hr;
	temp->tm_min = start_min;
	/*
	 * Timer precision is only to the minute. Set sec to 0 so that differences in seconds
	 * do not affect timer comparisons.
	 */
	temp->tm_sec = 0;
	temp->tm_isdst = -1;
	timer.start_time = mktime(temp);

	return timer;
}

static bool timer_date_out_of_range(const struct cec_msg &msg, const struct Timer &timer)
{
	__u8 day = msg.msg[2];
	__u8 month = msg.msg[3];
	/* Hours and minutes are in BCD format */
	__u8 start_hr = (msg.msg[4] >> 4) * 10 + (msg.msg[4] & 0xf);
	__u8 start_min = (msg.msg[5] >> 4) * 10 + (msg.msg[5] & 0xf);
	__u8 duration_hr = (msg.msg[6] >> 4) * 10 + (msg.msg[6] & 0xf);
	__u8 duration_min = (msg.msg[7] >> 4) * 10 + (msg.msg[7] & 0xf);

	if (start_min > 59 || start_hr > 23 || month > 12 || month == 0 || day > 31 || day == 0 ||
	    duration_min > 59 || (duration_hr == 0 && duration_min == 0))
		return true;

	switch (month) {
	case Apr: case Jun: case Sep: case Nov:
		if (day > 30)
			return true;
		break;
	case Feb: {
		struct tm *tp = localtime(&timer.start_time);

		if (!(tp->tm_year % 4) && ((tp->tm_year % 100) || !(tp->tm_year % 400))) {
			if (day > 29)
				return true;
		} else {
			if (day > 28)
				return true;
		}
		break;
	}
	default:
		break;
	}

	return false;
}

static bool timer_overlap(const struct Timer &new_timer)
{
	if (programmed_timers.size() == 1)
		return false;

	time_t new_timer_end = new_timer.start_time + new_timer.duration;
	for (auto &t : programmed_timers) {

		if (new_timer == t)
			continue; /* Timer doesn't overlap itself. */

		time_t existing_timer_end = t.start_time + t.duration;

		if ((t.start_time < new_timer.start_time && new_timer.start_time < existing_timer_end) ||
		    (t.start_time < new_timer_end && new_timer_end < existing_timer_end) ||
		    (t.start_time == new_timer.start_time || existing_timer_end == new_timer_end) ||
		    (new_timer.start_time < t.start_time && existing_timer_end < new_timer_end))
		    return true;
	}

	return false;
}

void process_timer_msgs(struct node *node, struct cec_msg &msg, unsigned me, __u8 type)
{
	bool is_bcast = cec_msg_is_broadcast(&msg);

	/* Timer Programming */
	switch (msg.msg[1]) {
	case CEC_MSG_SET_ANALOGUE_TIMER:
	case CEC_MSG_SET_DIGITAL_TIMER:
	case CEC_MSG_SET_EXT_TIMER: {
		if (type != CEC_LOG_ADDR_TYPE_RECORD)
			break;

		__u8 prog_error = 0;
		__u8 prog_info = 0;
		__u8 timer_overlap_warning = CEC_OP_TIMER_OVERLAP_WARNING_NO_OVERLAP;
		__u8 available_space_hr = 0;
		__u8 available_space_min = 0;
		struct Timer timer = get_timer_from_message(msg);

		/* If timer starts in the past, increment the year so that timers can be set across year-end. */
		if (time(nullptr) > timer.start_time) {
			struct tm *temp = localtime(&timer.start_time);
			temp->tm_year++;
			temp->tm_isdst = -1;
			timer.start_time = mktime(temp);
		}

		if (timer_date_out_of_range(msg, timer))
			prog_error = CEC_OP_PROG_ERROR_DATE_OUT_OF_RANGE;

		if (timer.recording_seq > 0x7f)
			prog_error = CEC_OP_PROG_ERROR_REC_SEQ_ERROR;

		if (programmed_timers.find(timer) != programmed_timers.end())
			prog_error = CEC_OP_PROG_ERROR_DUPLICATE;

		if (!prog_error) {
			programmed_timers.insert(timer);

			if (timer_overlap(timer))
				timer_overlap_warning = CEC_OP_TIMER_OVERLAP_WARNING_OVERLAP;

			if (node->state.media_space_available <= 0 ||
			    timer.duration > node->state.media_space_available) {
				prog_info = CEC_OP_PROG_INFO_NOT_ENOUGH_SPACE;
			} else {
				int space_that_may_be_needed = 0;
				for (auto &t : programmed_timers) {
					space_that_may_be_needed += t.duration;
					if (t == timer) /* Only count the space up to and including the new timer. */
						break;
				}
				if ((node->state.media_space_available - space_that_may_be_needed) >= 0)
					prog_info = CEC_OP_PROG_INFO_ENOUGH_SPACE;
				else
					prog_info = CEC_OP_PROG_INFO_MIGHT_NOT_BE_ENOUGH_SPACE;
			}
			print_timers(node);
		}

		if (prog_info == CEC_OP_PROG_INFO_NOT_ENOUGH_SPACE ||
		    prog_info == CEC_OP_PROG_INFO_MIGHT_NOT_BE_ENOUGH_SPACE ||
		    prog_error == CEC_OP_PROG_ERROR_DUPLICATE) {
			available_space_hr = node->state.media_space_available / 3600; /* 3600 MB/hour */
			available_space_min = (node->state.media_space_available % 3600) / 60; /* 60 MB/min */
		}
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_timer_status(&msg, timer_overlap_warning, CEC_OP_MEDIA_INFO_UNPROT_MEDIA,
		                     prog_info, prog_error, available_space_hr, available_space_min);
		transmit(node, &msg);
		return;
	}
	case CEC_MSG_CLEAR_ANALOGUE_TIMER:
	case CEC_MSG_CLEAR_DIGITAL_TIMER:
	case CEC_MSG_CLEAR_EXT_TIMER: {
		if (type != CEC_LOG_ADDR_TYPE_RECORD)
			break;

		__u8 timer_cleared_status = CEC_OP_TIMER_CLR_STAT_NO_MATCHING;

		/* Look for timer in the previous year which have persisted across year-end. */
		struct Timer timer_in_previous_year = get_timer_from_message(msg);
		struct tm *temp = localtime(&timer_in_previous_year.start_time);
		temp->tm_year--;
		temp->tm_isdst = -1;
		timer_in_previous_year.start_time = mktime(temp);
		auto it_previous_year = programmed_timers.find(timer_in_previous_year);

		if (it_previous_year != programmed_timers.end()) {
			if (node->state.recording_controlled_by_timer && it_previous_year == programmed_timers.begin()) {
				timer_cleared_status = CEC_OP_TIMER_CLR_STAT_RECORDING;
				node->state.one_touch_record_on = false;
				node->state.recording_controlled_by_timer = false;
			} else {
				timer_cleared_status = CEC_OP_TIMER_CLR_STAT_CLEARED;
			}
			programmed_timers.erase(timer_in_previous_year);
			print_timers(node);
		}

		/* Look for timer in the current year. */
		struct Timer timer_in_current_year = get_timer_from_message(msg);
		auto it_current_year = programmed_timers.find(timer_in_current_year);

		if (it_current_year != programmed_timers.end()) {
			if (node->state.recording_controlled_by_timer && it_current_year == programmed_timers.begin()) {
				timer_cleared_status = CEC_OP_TIMER_CLR_STAT_RECORDING;
				node->state.one_touch_record_on = false;
				node->state.recording_controlled_by_timer = false;
			} else {
				/* Do not overwrite status if already set. */
				if (timer_cleared_status == CEC_OP_TIMER_CLR_STAT_NO_MATCHING)
					timer_cleared_status = CEC_OP_TIMER_CLR_STAT_CLEARED;
			}
			programmed_timers.erase(timer_in_current_year);
			print_timers(node);
		}

		/* Look for timer in the next year. */
		struct Timer timer_in_next_year = get_timer_from_message(msg);
		temp = localtime(&timer_in_next_year.start_time);
		temp->tm_year++;
		temp->tm_isdst = -1;
		timer_in_next_year.start_time = mktime(temp);
		if (programmed_timers.find(timer_in_next_year) != programmed_timers.end()) {
			/* Do not overwrite status if already set. */
			if (timer_cleared_status == CEC_OP_TIMER_CLR_STAT_NO_MATCHING)
				timer_cleared_status = CEC_OP_TIMER_CLR_STAT_CLEARED;
			programmed_timers.erase(timer_in_next_year);
			print_timers(node);
		}
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_timer_cleared_status(&msg, timer_cleared_status);
		transmit(node, &msg);
		/*
		 * If the cleared timer was recording, and standby was received during recording,
		 * enter standby when the recording stops unless recording device is the active source.
		 */
		if (timer_cleared_status == CEC_OP_TIMER_CLR_STAT_RECORDING) {
			if (node->state.record_received_standby) {
				if (node->phys_addr != node->state.active_source_pa)
					enter_standby(node);
				node->state.record_received_standby = false;
			}
		}
		return;
	}
	case CEC_MSG_SET_TIMER_PROGRAM_TITLE:
		if (type != CEC_LOG_ADDR_TYPE_RECORD)
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
