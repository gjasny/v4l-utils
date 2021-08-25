// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2016 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#include <cstring>
#include <map>
#include <sstream>
#include <vector>

#include <sys/ioctl.h>
#include <unistd.h>

#include "cec-compliance.h"

enum Months { Jan = 1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec };

/* Tuner Control */

static const char *bcast_type2s(__u8 bcast_type)
{
	switch (bcast_type) {
	case CEC_OP_ANA_BCAST_TYPE_CABLE:
		return "Cable";
	case CEC_OP_ANA_BCAST_TYPE_SATELLITE:
		return "Satellite";
	case CEC_OP_ANA_BCAST_TYPE_TERRESTRIAL:
		return "Terrestrial";
	default:
		return "Future use";
	}
}

static int log_tuner_service(const struct cec_op_tuner_device_info &info,
			     const char *prefix = "")
{
	printf("\t\t%s", prefix);

	if (info.is_analog) {
		double freq_mhz = (info.analog.ana_freq * 625) / 10000.0;

		printf("Analog Channel %.2f MHz (%s, %s)\n", freq_mhz,
		       bcast_system2s(info.analog.bcast_system),
		       bcast_type2s(info.analog.ana_bcast_type));

		switch (info.analog.bcast_system) {
		case CEC_OP_BCAST_SYSTEM_PAL_BG:
		case CEC_OP_BCAST_SYSTEM_SECAM_LQ:
		case CEC_OP_BCAST_SYSTEM_PAL_M:
		case CEC_OP_BCAST_SYSTEM_NTSC_M:
		case CEC_OP_BCAST_SYSTEM_PAL_I:
		case CEC_OP_BCAST_SYSTEM_SECAM_DK:
		case CEC_OP_BCAST_SYSTEM_SECAM_BG:
		case CEC_OP_BCAST_SYSTEM_SECAM_L:
		case CEC_OP_BCAST_SYSTEM_PAL_DK:
			break;
		default:
			return fail("invalid analog bcast_system %u", info.analog.bcast_system);
		}
		if (info.analog.ana_bcast_type > CEC_OP_ANA_BCAST_TYPE_TERRESTRIAL)
			return fail("invalid analog bcast_type %u\n", info.analog.ana_bcast_type);
		fail_on_test(!info.analog.ana_freq);
		return 0;
	}

	__u8 system = info.digital.dig_bcast_system;

	printf("%s Channel ", dig_bcast_system2s(system));
	if (info.digital.service_id_method) {
		__u16 major = info.digital.channel.major;
		__u16 minor = info.digital.channel.minor;

		switch (info.digital.channel.channel_number_fmt) {
		case CEC_OP_CHANNEL_NUMBER_FMT_2_PART:
			printf("%u.%u\n", major, minor);
			break;
		case CEC_OP_CHANNEL_NUMBER_FMT_1_PART:
			printf("%u\n", minor);
			break;
		default:
			printf("%u.%u\n", major, minor);
			return fail("invalid service ID method\n");
		}
		return 0;
	}


	switch (system) {
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_GEN:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_BS:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_CS:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_T: {
		__u16 tsid = info.digital.arib.transport_id;
		__u16 sid = info.digital.arib.service_id;
		__u16 onid = info.digital.arib.orig_network_id;

		printf("TSID: %u, SID: %u, ONID: %u\n", tsid, sid, onid);
		break;
	}
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_GEN:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_SAT:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_CABLE:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_T: {
		__u16 tsid = info.digital.atsc.transport_id;
		__u16 pn = info.digital.atsc.program_number;

		printf("TSID: %u, Program Number: %u\n", tsid, pn);
		break;
	}
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_GEN:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_S:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_S2:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_C:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_T: {
		__u16 tsid = info.digital.dvb.transport_id;
		__u16 sid = info.digital.dvb.service_id;
		__u16 onid = info.digital.dvb.orig_network_id;

		printf("TSID: %u, SID: %u, ONID: %u\n", tsid, sid, onid);
		break;
	}
	default:
		break;
	}

	switch (system) {
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_GEN:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_GEN:
	case CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_GEN:
		warn_once("generic digital broadcast systems should not be used");
		break;
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
		return fail("invalid digital broadcast system %u", system);
	}

	if (info.digital.service_id_method > CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL)
		return fail("invalid service ID method %u\n", info.digital.service_id_method);

	return 0;
}

static int tuner_ctl_test(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;
	struct cec_op_tuner_device_info info = {};
	std::vector<struct cec_op_tuner_device_info> info_vec;
	bool has_tuner = (1 << la) & (CEC_LOG_ADDR_MASK_TV | CEC_LOG_ADDR_MASK_TUNER);
	int ret;

	cec_msg_init(&msg, me, la);
	cec_msg_give_tuner_device_status(&msg, true, CEC_OP_STATUS_REQ_ONCE);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(!has_tuner && !timed_out_or_abort(&msg));
	if (!has_tuner)
		return OK_NOT_SUPPORTED;
	if (timed_out(&msg) || unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (cec_msg_status_is_abort(&msg))
		return OK_REFUSED;

	printf("\t    Start Channel Scan\n");
	cec_ops_tuner_device_status(&msg, &info);
	info_vec.push_back(info);
	ret = log_tuner_service(info);
	if (ret)
		return ret;

	while (true) {
		cec_msg_init(&msg, me, la);
		cec_msg_tuner_step_increment(&msg);
		fail_on_test(!transmit(node, &msg));
		fail_on_test(cec_msg_status_is_abort(&msg));
		if (cec_msg_status_is_abort(&msg)) {
			fail_on_test(abort_reason(&msg) == CEC_OP_ABORT_UNRECOGNIZED_OP);
			if (abort_reason(&msg) == CEC_OP_ABORT_REFUSED) {
				warn("Tuner step increment does not wrap.\n");
				break;
			}

			warn("Tuner at end of service list did not receive feature abort refused.\n");
			break;
		}
		cec_msg_init(&msg, me, la);
		cec_msg_give_tuner_device_status(&msg, true, CEC_OP_STATUS_REQ_ONCE);
		fail_on_test(!transmit_timeout(node, &msg));
		fail_on_test(timed_out_or_abort(&msg));
		memset(&info, 0, sizeof(info));
		cec_ops_tuner_device_status(&msg, &info);
		if (!memcmp(&info, &info_vec[0], sizeof(info)))
			break;
		ret = log_tuner_service(info);
		if (ret)
			return ret;
		info_vec.push_back(info);
	}
	printf("\t    Finished Channel Scan\n");

	printf("\t    Start Channel Test\n");
	for (const auto &iter : info_vec) {
		cec_msg_init(&msg, me, la);
		log_tuner_service(iter, "Select ");
		if (iter.is_analog)
			cec_msg_select_analogue_service(&msg, iter.analog.ana_bcast_type,
				iter.analog.ana_freq, iter.analog.bcast_system);
		else
			cec_msg_select_digital_service(&msg, &iter.digital);
		fail_on_test(!transmit(node, &msg));
		fail_on_test(cec_msg_status_is_abort(&msg));
		cec_msg_init(&msg, me, la);
		cec_msg_give_tuner_device_status(&msg, true, CEC_OP_STATUS_REQ_ONCE);
		fail_on_test(!transmit_timeout(node, &msg));
		fail_on_test(timed_out_or_abort(&msg));
		memset(&info, 0, sizeof(info));
		cec_ops_tuner_device_status(&msg, &info);
		if (memcmp(&info, &iter, sizeof(info))) {
			log_tuner_service(info);
			log_tuner_service(iter);
		}
		fail_on_test(memcmp(&info, &iter, sizeof(info)));
	}
	printf("\t    Finished Channel Test\n");

	cec_msg_init(&msg, me, la);
	cec_msg_select_analogue_service(&msg, 3, 16000, 9);
	printf("\t\tSelect invalid analog channel\n");
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(!cec_msg_status_is_abort(&msg));
	fail_on_test(abort_reason(&msg) != CEC_OP_ABORT_INVALID_OP);
	cec_msg_init(&msg, me, la);
	info.digital.service_id_method = CEC_OP_SERVICE_ID_METHOD_BY_DIG_ID;
	info.digital.dig_bcast_system = CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_S2;
	info.digital.dvb.transport_id = 0;
	info.digital.dvb.service_id = 0;
	info.digital.dvb.orig_network_id = 0;
	cec_msg_select_digital_service(&msg, &info.digital);
	printf("\t\tSelect invalid digital channel\n");
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(!cec_msg_status_is_abort(&msg));
	fail_on_test(abort_reason(&msg) != CEC_OP_ABORT_INVALID_OP);

	return 0;
}

const vec_remote_subtests tuner_ctl_subtests{
	{ "Tuner Control", CEC_LOG_ADDR_MASK_TUNER | CEC_LOG_ADDR_MASK_TV, tuner_ctl_test },
};

/* One Touch Record */

static int one_touch_rec_on_send(struct node *node, unsigned me, unsigned la,
                                 const struct cec_op_record_src &rec_src, __u8 &rec_status)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_record_off(&msg, false);
	fail_on_test(!transmit_timeout(node, &msg));

	cec_msg_init(&msg, me, la);
	cec_msg_record_on(&msg, true, &rec_src);
	/* Allow 10s for reply because the spec says it may take several seconds to accurately respond. */
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	fail_on_test(timed_out_or_abort(&msg));
	cec_ops_record_status(&msg, &rec_status);

	return OK;
}

static int one_touch_rec_on_send_invalid(struct node *node, unsigned me, unsigned la,
                                         const struct cec_op_record_src &rec_src)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_record_on(&msg, true, &rec_src);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(!cec_msg_status_is_abort(&msg));
	fail_on_test(abort_reason(&msg) != CEC_OP_ABORT_INVALID_OP);

	return OK;
}

/*
 * Returns true if the Record Status is an error indicating that the
 * request to start recording has failed.
 */
static bool rec_status_is_a_valid_error_status(__u8 rec_status)
{
	switch (rec_status) {
	case CEC_OP_RECORD_STATUS_NO_DIG_SERVICE:
	case CEC_OP_RECORD_STATUS_NO_ANA_SERVICE:
	case CEC_OP_RECORD_STATUS_NO_SERVICE:
	case CEC_OP_RECORD_STATUS_INVALID_EXT_PLUG:
	case CEC_OP_RECORD_STATUS_INVALID_EXT_PHYS_ADDR:
	case CEC_OP_RECORD_STATUS_UNSUP_CA:
	case CEC_OP_RECORD_STATUS_NO_CA_ENTITLEMENTS:
	case CEC_OP_RECORD_STATUS_CANT_COPY_SRC:
	case CEC_OP_RECORD_STATUS_NO_MORE_COPIES:
	case CEC_OP_RECORD_STATUS_NO_MEDIA:
	case CEC_OP_RECORD_STATUS_PLAYING:
	case CEC_OP_RECORD_STATUS_ALREADY_RECORDING:
	case CEC_OP_RECORD_STATUS_MEDIA_PROT:
	case CEC_OP_RECORD_STATUS_NO_SIGNAL:
	case CEC_OP_RECORD_STATUS_MEDIA_PROBLEM:
	case CEC_OP_RECORD_STATUS_NO_SPACE:
	case CEC_OP_RECORD_STATUS_PARENTAL_LOCK:
	case CEC_OP_RECORD_STATUS_OTHER:
		return true;
	default:
		return false;
	}
}

static int one_touch_rec_tv_screen(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_record_tv_screen(&msg, true);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test_v2(node->remote[la].cec_version,
			node->remote[la].has_rec_tv && unrecognized_op(&msg));
	fail_on_test_v2(node->remote[la].cec_version,
			!node->remote[la].has_rec_tv && !unrecognized_op(&msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return OK_PRESUMED;
	/* Follower should ignore this message if it is not sent by a recording device */
	if (node->prim_devtype != CEC_OP_PRIM_DEVTYPE_RECORD) {
		fail_on_test(!timed_out(&msg));
		return OK;
	}
	fail_on_test(timed_out(&msg));

	struct cec_op_record_src rec_src = {};

	cec_ops_record_on(&msg, &rec_src);

	fail_on_test(rec_src.type < CEC_OP_RECORD_SRC_OWN ||
	             rec_src.type > CEC_OP_RECORD_SRC_EXT_PHYS_ADDR);

	if (rec_src.type == CEC_OP_RECORD_SRC_DIGITAL) {
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
			return fail("Invalid digital service broadcast system operand.\n");
		}

		if (rec_src.digital.service_id_method == CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL)
			fail_on_test(rec_src.digital.channel.channel_number_fmt < CEC_OP_CHANNEL_NUMBER_FMT_1_PART ||
			             rec_src.digital.channel.channel_number_fmt > CEC_OP_CHANNEL_NUMBER_FMT_2_PART);
	}

	if (rec_src.type == CEC_OP_RECORD_SRC_ANALOG) {
		fail_on_test(rec_src.analog.ana_bcast_type > CEC_OP_ANA_BCAST_TYPE_TERRESTRIAL);
		fail_on_test(rec_src.analog.bcast_system > CEC_OP_BCAST_SYSTEM_PAL_DK &&
		             rec_src.analog.bcast_system != CEC_OP_BCAST_SYSTEM_OTHER);
		fail_on_test(rec_src.analog.ana_freq == 0 || rec_src.analog.ana_freq == 0xffff);
	}

	if (rec_src.type == CEC_OP_RECORD_SRC_EXT_PLUG)
		fail_on_test(rec_src.ext_plug.plug == 0);

	return OK;
}

static int one_touch_rec_on(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;
	struct cec_op_record_src rec_src = {};

	rec_src.type = CEC_OP_RECORD_SRC_OWN;
	cec_msg_init(&msg, me, la);
	cec_msg_record_on(&msg, true, &rec_src);
	/* Allow 10s for reply because the spec says it may take several seconds to accurately respond. */
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	fail_on_test(timed_out(&msg));
	if (unrecognized_op(&msg)) {
		fail_on_test(node->remote[la].prim_type == CEC_OP_PRIM_DEVTYPE_RECORD);
		return OK_NOT_SUPPORTED;
	}
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return OK_PRESUMED;

	__u8 rec_status;

	cec_ops_record_status(&msg, &rec_status);
	if (rec_status != CEC_OP_RECORD_STATUS_CUR_SRC)
		fail_on_test(!rec_status_is_a_valid_error_status(rec_status));

	/* In the following tests, these digital services are taken from the cec-follower tuner emulation. */
	memset(&rec_src, 0, sizeof(rec_src));
	rec_src.type = CEC_OP_RECORD_SRC_DIGITAL;
	rec_src.digital.service_id_method = CEC_OP_SERVICE_ID_METHOD_BY_DIG_ID;
	rec_src.digital.dig_bcast_system = CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_BS;
	rec_src.digital.arib.transport_id = 1032;
	rec_src.digital.arib.service_id = 30203;
	rec_src.digital.arib.orig_network_id = 1;
	fail_on_test(one_touch_rec_on_send(node, me, la, rec_src, rec_status));
	if (rec_status != CEC_OP_RECORD_STATUS_DIG_SERVICE)
		fail_on_test(!rec_status_is_a_valid_error_status(rec_status));

	memset(&rec_src, 0, sizeof(rec_src));
	rec_src.type = CEC_OP_RECORD_SRC_DIGITAL;
	rec_src.digital.service_id_method = CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL;
	rec_src.digital.dig_bcast_system = CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_T;
	rec_src.digital.channel.channel_number_fmt = CEC_OP_CHANNEL_NUMBER_FMT_2_PART;
	rec_src.digital.channel.major = 4;
	rec_src.digital.channel.minor = 1;
	fail_on_test(one_touch_rec_on_send(node, me, la, rec_src, rec_status));
	if (rec_status != CEC_OP_RECORD_STATUS_DIG_SERVICE)
		fail_on_test(!rec_status_is_a_valid_error_status(rec_status));

	memset(&rec_src, 0, sizeof(rec_src));
	rec_src.type = CEC_OP_RECORD_SRC_DIGITAL;
	rec_src.digital.service_id_method = CEC_OP_SERVICE_ID_METHOD_BY_DIG_ID;
	rec_src.digital.dig_bcast_system = CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_T;
	rec_src.digital.dvb.transport_id = 1004;
	rec_src.digital.dvb.service_id = 1040;
	rec_src.digital.dvb.orig_network_id = 8945;
	fail_on_test(one_touch_rec_on_send(node, me, la, rec_src, rec_status));
	if (rec_status != CEC_OP_RECORD_STATUS_DIG_SERVICE)
		fail_on_test(!rec_status_is_a_valid_error_status(rec_status));

	/* In the following tests, these channels taken from the cec-follower tuner emulation. */
	memset(&rec_src, 0, sizeof(rec_src));
	rec_src.type = CEC_OP_RECORD_STATUS_ANA_SERVICE;
	rec_src.analog.ana_bcast_type = CEC_OP_ANA_BCAST_TYPE_CABLE;
	rec_src.analog.ana_freq = (471250 * 10) / 625;
	rec_src.analog.bcast_system = CEC_OP_BCAST_SYSTEM_PAL_BG;
	fail_on_test(one_touch_rec_on_send(node, me, la, rec_src, rec_status));
	if (rec_status != CEC_OP_RECORD_STATUS_ANA_SERVICE)
		fail_on_test(!rec_status_is_a_valid_error_status(rec_status));

	memset(&rec_src, 0, sizeof(rec_src));
	rec_src.type = CEC_OP_RECORD_STATUS_ANA_SERVICE;
	rec_src.analog.ana_bcast_type = CEC_OP_ANA_BCAST_TYPE_SATELLITE;
	rec_src.analog.ana_freq = (551250 * 10) / 625;
	rec_src.analog.bcast_system = CEC_OP_BCAST_SYSTEM_SECAM_BG;
	fail_on_test(one_touch_rec_on_send(node, me, la, rec_src, rec_status));
	if (rec_status != CEC_OP_RECORD_STATUS_ANA_SERVICE)
		fail_on_test(!rec_status_is_a_valid_error_status(rec_status));

	memset(&rec_src, 0, sizeof(rec_src));
	rec_src.type = CEC_OP_RECORD_STATUS_ANA_SERVICE;
	rec_src.analog.ana_bcast_type = CEC_OP_ANA_BCAST_TYPE_TERRESTRIAL;
	rec_src.analog.ana_freq = (185250 * 10) / 625;
	rec_src.analog.bcast_system = CEC_OP_BCAST_SYSTEM_PAL_DK;
	fail_on_test(one_touch_rec_on_send(node, me, la, rec_src, rec_status));
	if (rec_status != CEC_OP_RECORD_STATUS_ANA_SERVICE)
		fail_on_test(!rec_status_is_a_valid_error_status(rec_status));

	memset(&rec_src, 0, sizeof(rec_src));
	rec_src.type = CEC_OP_RECORD_SRC_EXT_PLUG;
	rec_src.ext_plug.plug = 1;
	fail_on_test(one_touch_rec_on_send(node, me, la, rec_src, rec_status));
	if (rec_status != CEC_OP_RECORD_STATUS_EXT_INPUT)
		fail_on_test(!rec_status_is_a_valid_error_status(rec_status));

	memset(&rec_src, 0, sizeof(rec_src));
	rec_src.type = CEC_OP_RECORD_SRC_EXT_PHYS_ADDR;
	fail_on_test(one_touch_rec_on_send(node, me, la, rec_src, rec_status));
	if (rec_status != CEC_OP_RECORD_STATUS_EXT_INPUT)
		fail_on_test(!rec_status_is_a_valid_error_status(rec_status));

	return OK;
}

static int one_touch_rec_on_invalid(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_record_on_own(&msg);
	msg.msg[2] = 0;  /* Invalid source operand */
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	fail_on_test(!cec_msg_status_is_abort(&msg));
	fail_on_test(abort_reason(&msg) != CEC_OP_ABORT_INVALID_OP);

	cec_msg_init(&msg, me, la);
	cec_msg_record_on_own(&msg);
	msg.msg[2] = 6;  /* Invalid source operand */
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(!cec_msg_status_is_abort(&msg));
	fail_on_test(abort_reason(&msg) != CEC_OP_ABORT_INVALID_OP);

	struct cec_op_record_src rec_src = {};

	rec_src.type = CEC_OP_RECORD_SRC_DIGITAL;
	rec_src.digital.service_id_method = CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL;
	rec_src.digital.dig_bcast_system = 0x7f; /* Invalid digital service broadcast system operand */
	rec_src.digital.channel.channel_number_fmt = CEC_OP_CHANNEL_NUMBER_FMT_1_PART;
	rec_src.digital.channel.major = 0;
	rec_src.digital.channel.minor = 30203;
	fail_on_test(one_touch_rec_on_send_invalid(node, me, la, rec_src));

	rec_src.type = CEC_OP_RECORD_SRC_DIGITAL;
	rec_src.digital.service_id_method = CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL;
	rec_src.digital.dig_bcast_system = CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_BS;
	rec_src.digital.channel.channel_number_fmt = 0; /* Invalid channel number format operand */
	rec_src.digital.channel.major = 0;
	rec_src.digital.channel.minor = 30609;
	fail_on_test(one_touch_rec_on_send_invalid(node, me, la, rec_src));

	memset(&rec_src, 0, sizeof(rec_src));
	rec_src.type = CEC_OP_RECORD_SRC_ANALOG;
	rec_src.analog.ana_bcast_type = 0xff; /* Invalid analog broadcast type */
	rec_src.analog.ana_freq = (519250 * 10) / 625;
	rec_src.analog.bcast_system = CEC_OP_BCAST_SYSTEM_PAL_BG;
	fail_on_test(one_touch_rec_on_send_invalid(node, me, la, rec_src));

	memset(&rec_src, 0, sizeof(rec_src));
	rec_src.type = CEC_OP_RECORD_SRC_ANALOG;
	rec_src.analog.ana_bcast_type = CEC_OP_ANA_BCAST_TYPE_SATELLITE;
	rec_src.analog.ana_freq = (703250 * 10) / 625;
	rec_src.analog.bcast_system = 0xff; /* Invalid analog broadcast system */
	fail_on_test(one_touch_rec_on_send_invalid(node, me, la, rec_src));

	memset(&rec_src, 0, sizeof(rec_src));
	rec_src.type = CEC_OP_RECORD_SRC_ANALOG;
	rec_src.analog.ana_bcast_type = CEC_OP_ANA_BCAST_TYPE_TERRESTRIAL;
	rec_src.analog.ana_freq = 0; /* Invalid frequency */
	rec_src.analog.bcast_system = CEC_OP_BCAST_SYSTEM_NTSC_M;
	fail_on_test(one_touch_rec_on_send_invalid(node, me, la, rec_src));

	memset(&rec_src, 0, sizeof(rec_src));
	rec_src.type = CEC_OP_RECORD_SRC_ANALOG;
	rec_src.analog.ana_bcast_type = CEC_OP_ANA_BCAST_TYPE_CABLE;
	rec_src.analog.ana_freq = 0xffff; /* Invalid frequency */
	rec_src.analog.bcast_system = CEC_OP_BCAST_SYSTEM_SECAM_L;
	fail_on_test(one_touch_rec_on_send_invalid(node, me, la, rec_src));

	memset(&rec_src, 0, sizeof(rec_src));
	rec_src.type = CEC_OP_RECORD_SRC_EXT_PLUG;
	rec_src.ext_plug.plug = 0; /* Invalid plug */
	fail_on_test(one_touch_rec_on_send_invalid(node, me, la, rec_src));

	return OK;
}

static int one_touch_rec_off(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_record_off(&msg, true);
	/* Allow 10s for reply because the spec says it may take several seconds to accurately respond. */
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	if (unrecognized_op(&msg)) {
		fail_on_test(node->remote[la].prim_type == CEC_OP_PRIM_DEVTYPE_RECORD);
		return OK_NOT_SUPPORTED;
	}
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return OK_PRESUMED;
	if (timed_out(&msg))
		return OK_PRESUMED;

	__u8 rec_status;

	cec_ops_record_status(&msg, &rec_status);

	fail_on_test(rec_status != CEC_OP_RECORD_STATUS_TERMINATED_OK &&
	             rec_status != CEC_OP_RECORD_STATUS_ALREADY_TERM);

	return OK;
}

const vec_remote_subtests one_touch_rec_subtests{
	{ "Record TV Screen", CEC_LOG_ADDR_MASK_TV, one_touch_rec_tv_screen },
	{
		"Record On",
		CEC_LOG_ADDR_MASK_RECORD | CEC_LOG_ADDR_MASK_BACKUP,
		one_touch_rec_on,
	},
	{
		"Record On Invalid Operand",
		CEC_LOG_ADDR_MASK_RECORD | CEC_LOG_ADDR_MASK_BACKUP,
		one_touch_rec_on_invalid,
	},
	{ "Record Off", CEC_LOG_ADDR_MASK_RECORD | CEC_LOG_ADDR_MASK_BACKUP, one_touch_rec_off },

};

/* Timer Programming */

static int timer_status_is_valid(const struct cec_msg &msg)
{
	__u8 timer_overlap_warning;
	__u8 media_info;
	__u8 prog_info;
	__u8 prog_error;
	__u8 duration_hr;
	__u8 duration_min;

	cec_ops_timer_status(&msg, &timer_overlap_warning, &media_info, &prog_info,
	                     &prog_error, &duration_hr, &duration_min);
	fail_on_test(media_info > CEC_OP_MEDIA_INFO_NO_MEDIA);
	if (prog_info)
		fail_on_test(prog_info < CEC_OP_PROG_INFO_ENOUGH_SPACE ||
		             prog_info > CEC_OP_PROG_INFO_MIGHT_NOT_BE_ENOUGH_SPACE);
	else
		fail_on_test(prog_error < CEC_OP_PROG_ERROR_NO_FREE_TIMER ||
		             (prog_error > CEC_OP_PROG_ERROR_CLOCK_FAILURE &&
		              prog_error != CEC_OP_PROG_ERROR_DUPLICATE));

	return OK;
}

static int timer_cleared_status_is_valid(const struct cec_msg &msg)
{
	__u8 timer_cleared_status;

	cec_ops_timer_cleared_status(&msg, &timer_cleared_status);
	fail_on_test(timer_cleared_status != CEC_OP_TIMER_CLR_STAT_RECORDING &&
	             timer_cleared_status != CEC_OP_TIMER_CLR_STAT_NO_MATCHING &&
	             timer_cleared_status != CEC_OP_TIMER_CLR_STAT_NO_INFO &&
	             timer_cleared_status != CEC_OP_TIMER_CLR_STAT_CLEARED);

	return OK;
}

static bool timer_has_error(const struct cec_msg &msg)
{
	__u8 timer_overlap_warning;
	__u8 media_info;
	__u8 prog_info;
	__u8 prog_error;
	__u8 duration_hr;
	__u8 duration_min;

	cec_ops_timer_status(&msg, &timer_overlap_warning, &media_info, &prog_info,
	                     &prog_error, &duration_hr, &duration_min);
	if (prog_error)
		return true;

	return false;
}

static int send_timer_error(struct node *node, unsigned me, unsigned la, __u8 day, __u8 month,
                            __u8 start_hr, __u8 start_min, __u8 dur_hr, __u8 dur_min, __u8 rec_seq)
{
	struct cec_msg msg;
	cec_msg_init(&msg, me, la);
	cec_msg_set_analogue_timer(&msg, true, day, month, start_hr, start_min, dur_hr, dur_min,
	                           rec_seq, CEC_OP_ANA_BCAST_TYPE_CABLE, 7668, // 479.25 MHz
	                           node->remote[la].bcast_sys);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	fail_on_test(timed_out(&msg));
	if (cec_msg_status_is_abort(&msg))
		fail_on_test(abort_reason(&msg) != CEC_OP_ABORT_UNRECOGNIZED_OP);
	else
		fail_on_test(!timer_has_error(msg));

	return OK;
}

static bool timer_overlap_warning_is_set(const struct cec_msg &msg)
{
	__u8 timer_overlap_warning;
	__u8 media_info;
	__u8 prog_info;
	__u8 prog_error;
	__u8 duration_hr;
	__u8 duration_min;

	cec_ops_timer_status(&msg, &timer_overlap_warning, &media_info, &prog_info,
	                     &prog_error, &duration_hr, &duration_min);

	if (timer_overlap_warning)
		return true;

	return false;
}

static int send_timer_overlap(struct node *node, unsigned me, unsigned la, __u8 day, __u8 month,
                              __u8 start_hr, __u8 start_min, __u8 dur_hr, __u8 dur_min, __u8 rec_seq)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_set_analogue_timer(&msg, true, day, month, start_hr, start_min, dur_hr, dur_min,
	                           rec_seq, CEC_OP_ANA_BCAST_TYPE_CABLE, 7668, // 479.25 MHz
	                           node->remote[la].bcast_sys);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	fail_on_test(timed_out_or_abort(&msg));
	fail_on_test(timer_has_error(msg));
	fail_on_test(!timer_overlap_warning_is_set(msg));

	return OK;
}

static int clear_timer(struct node *node, unsigned me, unsigned la, __u8 day, __u8 month,
                       __u8 start_hr, __u8 start_min, __u8 dur_hr, __u8 dur_min, __u8 rec_seq)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_clear_analogue_timer(&msg, true, day, month, start_hr, start_min, dur_hr, dur_min,
	                             rec_seq, CEC_OP_ANA_BCAST_TYPE_CABLE, 7668, // 479.25 MHz
	                             node->remote[la].bcast_sys);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	fail_on_test(timed_out_or_abort(&msg));
	fail_on_test(timer_has_error(msg));
	fail_on_test(timer_cleared_status_is_valid(msg));

	return OK;
}

static int timer_prog_set_analog_timer(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	/* Set timer to start tomorrow, at current time, for 2 hr, 30 min. */
	time_t tomorrow = node->current_time + (24 * 60 * 60);
	struct tm *t = localtime(&tomorrow);
	cec_msg_set_analogue_timer(&msg, true, t->tm_mday, t->tm_mon + 1, t->tm_hour, t->tm_min, 2, 30,
	                           0x7f, CEC_OP_ANA_BCAST_TYPE_CABLE, 7668, // 479.25 MHz
	                           node->remote[la].bcast_sys);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	fail_on_test(timed_out(&msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return OK_PRESUMED;
	fail_on_test(timer_status_is_valid(msg));
	node->remote[la].has_analogue_timer = true;

	return OK;
}

static int timer_prog_set_digital_timer(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;
	struct cec_op_digital_service_id digital_service_id = {};

	digital_service_id.service_id_method = CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL;
	digital_service_id.channel.channel_number_fmt = CEC_OP_CHANNEL_NUMBER_FMT_1_PART;
	digital_service_id.channel.minor = 1;
	digital_service_id.dig_bcast_system = node->remote[la].dig_bcast_sys;
	cec_msg_init(&msg, me, la);
	/* Set timer to start 2 days from now, at current time, for 4 hr, 30 min. */
	time_t two_days_ahead = node->current_time + (2 * 24 * 60 * 60);
	struct tm *t = localtime(&two_days_ahead);
	cec_msg_set_digital_timer(&msg, true, t->tm_mday, t->tm_mon + 1, t->tm_hour,
	                          t->tm_min, 4, 30, CEC_OP_REC_SEQ_ONCE_ONLY, &digital_service_id);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	fail_on_test(timed_out(&msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return OK_PRESUMED;
	fail_on_test(timer_status_is_valid(msg));

	return 0;
}

static int timer_prog_set_ext_timer(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	/* Set timer to start 3 days from now, at current time, for 6 hr, 30 min. */
	time_t three_days_ahead = node->current_time + (3 * 24 * 60 * 60);
	struct tm *t = localtime(&three_days_ahead);
	cec_msg_set_ext_timer(&msg, true, t->tm_mday, t->tm_mon + 1, t->tm_hour, t->tm_min, 6, 30,
	                      CEC_OP_REC_SEQ_ONCE_ONLY, CEC_OP_EXT_SRC_PHYS_ADDR, 0, node->phys_addr);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	fail_on_test(timed_out(&msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return OK_PRESUMED;
	fail_on_test(timer_status_is_valid(msg));

	return 0;
}

static int timer_prog_clear_analog_timer(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	/* Clear timer set to start tomorrow, at current time, for 2 hr, 30 min. */
	time_t tomorrow = node->current_time + (24 * 60 * 60);
	struct tm *t = localtime(&tomorrow);
	cec_msg_clear_analogue_timer(&msg, true, t->tm_mday, t->tm_mon + 1, t->tm_hour, t->tm_min, 2, 30,
	                             0x7f, CEC_OP_ANA_BCAST_TYPE_CABLE,7668, // 479.25 MHz
	                             node->remote[la].bcast_sys);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	fail_on_test(timed_out(&msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return OK_PRESUMED;
	fail_on_test(timer_cleared_status_is_valid(msg));

	return 0;
}

static int timer_prog_clear_digital_timer(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;
	struct cec_op_digital_service_id digital_service_id = {};

	digital_service_id.service_id_method = CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL;
	digital_service_id.channel.channel_number_fmt = CEC_OP_CHANNEL_NUMBER_FMT_1_PART;
	digital_service_id.channel.minor = 1;
	digital_service_id.dig_bcast_system = node->remote[la].dig_bcast_sys;
	cec_msg_init(&msg, me, la);
	/* Clear timer set to start 2 days from now, at current time, for 4 hr, 30 min. */
	time_t two_days_ahead = node->current_time + (2 * 24 * 60 * 60);
	struct tm *t = localtime(&two_days_ahead);
	cec_msg_clear_digital_timer(&msg, true, t->tm_mday, t->tm_mon + 1, t->tm_hour,
	                            t->tm_min, 4, 30, CEC_OP_REC_SEQ_ONCE_ONLY, &digital_service_id);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	fail_on_test(timed_out(&msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return OK_PRESUMED;
	fail_on_test(timer_cleared_status_is_valid(msg));

	return 0;
}

static int timer_prog_clear_ext_timer(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	/* Clear timer set to start 3 days from now, at current time, for 6 hr, 30 min. */
	time_t three_days_ahead = node->current_time + (3 * 24 * 60 * 60);
	struct tm *t = localtime(&three_days_ahead);
	cec_msg_clear_ext_timer(&msg, true, t->tm_mday, t->tm_mon + 1, t->tm_hour, t->tm_min, 6, 30,
	                        CEC_OP_REC_SEQ_ONCE_ONLY, CEC_OP_EXT_SRC_PHYS_ADDR, 0, node->phys_addr);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	fail_on_test(timed_out(&msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return OK_PRESUMED;
	fail_on_test(timer_cleared_status_is_valid(msg));

	return 0;
}

static int timer_prog_set_prog_title(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_set_timer_program_title(&msg, "Super-Hans II");
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;

	return OK_PRESUMED;
}

static int timer_errors(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	if (!node->remote[la].has_analogue_timer)
		return OK_NOT_SUPPORTED;

	/* Day error: November 31, at 6:00 am, for 1 hr. */
	fail_on_test(send_timer_error(node, me, la, 31, Nov, 6, 0, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY));

	/* Day error: December 32, at 6:00 am, for 1 hr. */
	fail_on_test(send_timer_error(node, me, la, 32, Dec, 6, 0, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY));

	/* Day error: 0, in January, at 6:00 am, for 1 hr. Day range begins at 1. */
	fail_on_test(send_timer_error(node, me, la, 0, Jan, 6, 0, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY));

	/* Month error: 0, on day 5, at 6:00 am, for 1 hr. CEC month range is 1-12. */
	fail_on_test(send_timer_error(node, me, la, 5, 0, 6, 0, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY));

	/* Month error: 13, on day 5, at 6:00 am, for 1 hr. */
	fail_on_test(send_timer_error(node, me, la, 5, 13, 6, 0, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY));

	/* Start hour error: 24 hr, on August 5, for 1 hr. Start hour range is 0-23. */
	fail_on_test(send_timer_error(node, me, la, 5, Aug, 24, 0, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY));

	/* Start min error: 60 min, on August 5, for 1 hr. Start min range is 0-59. */
	fail_on_test(send_timer_error(node, me, la, 5, Aug, 0, 60, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY));

	/* Recording duration error: 0 hr, 0 min on August 5, at 6:00am. */
	fail_on_test(send_timer_error(node, me, la, 5, Aug, 6, 0, 0, 0, CEC_OP_REC_SEQ_ONCE_ONLY));

	/* Duplicate timer error: start 2 hrs from now, for 1 hr. */
	time_t two_hours_ahead = node->current_time + (2 * 60 * 60);
	struct tm *t = localtime(&two_hours_ahead);
	cec_msg_init(&msg, me, la);
	cec_msg_set_analogue_timer(&msg, true, t->tm_mday, t->tm_mon + 1, t->tm_hour, t->tm_min, 1, 0,
	                           CEC_OP_REC_SEQ_ONCE_ONLY,CEC_OP_ANA_BCAST_TYPE_CABLE,
	                           7668, // 479.25 MHz
	                           node->remote[la].bcast_sys);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	fail_on_test(timed_out_or_abort(&msg));
	fail_on_test(timer_has_error(msg)); /* The first timer should be set. */
	fail_on_test(send_timer_error(node, me, la, t->tm_mday, t->tm_mon + 1, t->tm_hour,
	                              t->tm_min, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY));

	/* Clear the timer that was set to test duplicate timers. */
	fail_on_test(clear_timer(node, me, la, t->tm_mday, t->tm_mon + 1, t->tm_hour, t->tm_min, 1, 0,
	                         CEC_OP_REC_SEQ_ONCE_ONLY));

	/* Recording sequence error: 0xff, on August 5, at 6:00 am, for 1 hr. */
	fail_on_test(send_timer_error(node, me, la, 5, Aug, 6, 0, 1, 0, 0xff));

	/* Error in last day of February, at 6:00 am, for 1 hr. */
	time_t current_time = node->current_time;
	t = localtime(&current_time);
	if ((t->tm_mon + 1) > Feb)
		t->tm_year++; /* The timer will be for next year. */
	if (!(t->tm_year % 4) && ((t->tm_year % 100) || !(t->tm_year % 400)))
		fail_on_test(send_timer_error(node, me, la, 30, Feb, 6, 0, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY));
	else
		fail_on_test(send_timer_error(node, me, la, 29, Feb, 6, 0, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY));

	return OK;
}

static int timer_overlap_warning(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	time_t tomorrow = node->current_time + (24 * 60 * 60);
	struct tm *t = localtime(&tomorrow);

	if (!node->remote[la].has_analogue_timer)
		return OK_NOT_SUPPORTED;

	/* No overlap: set timer for tomorrow at 8:00 am for 2 hr. */
	cec_msg_init(&msg, me, la);
	cec_msg_set_analogue_timer(&msg, true, t->tm_mday, t->tm_mon + 1, 8, 0, 2, 0,
	                           CEC_OP_REC_SEQ_ONCE_ONLY, CEC_OP_ANA_BCAST_TYPE_CABLE,
	                           7668, // 479.25 MHz
	                           node->remote[la].bcast_sys);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	fail_on_test(timed_out_or_abort(&msg));
	fail_on_test(timer_has_error(msg));
	fail_on_test(timer_overlap_warning_is_set(msg));

	/* No overlap, just adjacent: set timer for tomorrow at 10:00 am for 15 min. */
	cec_msg_init(&msg, me, la);
	cec_msg_set_analogue_timer(&msg, true, t->tm_mday, t->tm_mon + 1, 10, 0, 0, 15,
	                           CEC_OP_REC_SEQ_ONCE_ONLY, CEC_OP_ANA_BCAST_TYPE_CABLE,
	                           7668, // 479.25 MHz
	                           node->remote[la].bcast_sys);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	fail_on_test(timed_out_or_abort(&msg));
	fail_on_test(timer_has_error(msg));
	fail_on_test(timer_overlap_warning_is_set(msg));

	/* No overlap, just adjacent: set timer for tomorrow at 7:45 am for 15 min. */
	cec_msg_init(&msg, me, la);
	cec_msg_set_analogue_timer(&msg, true, t->tm_mday, t->tm_mon + 1, 7, 45, 0, 15,
	                           CEC_OP_REC_SEQ_ONCE_ONLY, CEC_OP_ANA_BCAST_TYPE_CABLE,
	                           7668, // 479.25 MHz
	                           node->remote[la].bcast_sys);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	fail_on_test(timed_out_or_abort(&msg));
	fail_on_test(timer_has_error(msg));
	fail_on_test(timer_overlap_warning_is_set(msg));

	/* Overlap tail end: set timer for tomorrow at 9:00 am for 2 hr, repeats on Sun. */
	fail_on_test(send_timer_overlap(node, me, la, t->tm_mday, t->tm_mon + 1, 9, 0, 2, 0, 0x1));

	/* Overlap front end: set timer for tomorrow at 7:00 am for 1 hr, 30 min. */
	fail_on_test(send_timer_overlap(node, me, la, t->tm_mday, t->tm_mon + 1, 7, 0, 1, 30, 0x1));

	/* Overlap same start time: set timer for tomorrow at 8:00 am for 30 min. */
	fail_on_test(send_timer_overlap(node, me, la, t->tm_mday, t->tm_mon + 1, 8, 0, 0, 30, 0x1));

	/* Overlap same end time: set timer for tomorrow at 9:30 am for 30 min. */
	fail_on_test(send_timer_overlap(node, me, la, t->tm_mday, t->tm_mon + 1, 9, 30, 0, 30, 0x1));

	/* Overlap all timers: set timer for tomorrow at 6:00 am for 6 hr. */
	fail_on_test(send_timer_overlap(node, me, la, t->tm_mday, t->tm_mon + 1, 6, 0, 6, 0, 0x1));

	/* Clear all the timers. */
	fail_on_test(clear_timer(node, me, la, t->tm_mday, t->tm_mon + 1, 8, 0, 2, 0,
	                         CEC_OP_REC_SEQ_ONCE_ONLY));
	fail_on_test(clear_timer(node, me, la, t->tm_mday, t->tm_mon + 1, 10, 0, 0, 15,
	                         CEC_OP_REC_SEQ_ONCE_ONLY));
	fail_on_test(clear_timer(node, me, la, t->tm_mday, t->tm_mon + 1, 7, 45, 0, 15,
	                         CEC_OP_REC_SEQ_ONCE_ONLY));
	fail_on_test(clear_timer(node, me, la, t->tm_mday, t->tm_mon + 1, 9, 0, 2, 0, 0x1));
	fail_on_test(clear_timer(node, me, la, t->tm_mday, t->tm_mon + 1, 7, 0, 1, 30, 0x1));
	fail_on_test(clear_timer(node, me, la, t->tm_mday, t->tm_mon + 1, 8, 0, 0, 30, 0x1));
	fail_on_test(clear_timer(node, me, la, t->tm_mday, t->tm_mon + 1, 9, 30, 0, 30, 0x1));
	fail_on_test(clear_timer(node, me, la, t->tm_mday, t->tm_mon + 1, 6, 0, 6, 0, 0x1));

	return OK;
}

const vec_remote_subtests timer_prog_subtests{
	{
		"Set Analogue Timer",
		CEC_LOG_ADDR_MASK_RECORD | CEC_LOG_ADDR_MASK_BACKUP,
		timer_prog_set_analog_timer,
	},
	{
		"Set Digital Timer",
		CEC_LOG_ADDR_MASK_RECORD | CEC_LOG_ADDR_MASK_BACKUP,
		timer_prog_set_digital_timer,
	},
	{
		"Set Timer Program Title",
		CEC_LOG_ADDR_MASK_RECORD | CEC_LOG_ADDR_MASK_BACKUP,
		timer_prog_set_prog_title,
	},
	{
		"Set External Timer",
		CEC_LOG_ADDR_MASK_RECORD | CEC_LOG_ADDR_MASK_BACKUP,
		timer_prog_set_ext_timer,
	},
	{
		"Clear Analogue Timer",
		CEC_LOG_ADDR_MASK_RECORD | CEC_LOG_ADDR_MASK_BACKUP,
		timer_prog_clear_analog_timer,
	},
	{
		"Clear Digital Timer",
		CEC_LOG_ADDR_MASK_RECORD | CEC_LOG_ADDR_MASK_BACKUP,
		timer_prog_clear_digital_timer,
	},
	{
		"Clear External Timer",
		CEC_LOG_ADDR_MASK_RECORD | CEC_LOG_ADDR_MASK_BACKUP,
		timer_prog_clear_ext_timer,
	},
	{
		"Set Timers with Errors",
		CEC_LOG_ADDR_MASK_RECORD | CEC_LOG_ADDR_MASK_BACKUP,
		timer_errors,
	},
	{
		"Set Overlapping Timers",
		CEC_LOG_ADDR_MASK_RECORD | CEC_LOG_ADDR_MASK_BACKUP,
		timer_overlap_warning,
	},
};
