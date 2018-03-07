// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2017 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
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
#include <stdarg.h>
#include <cerrno>
#include <string>
#include <vector>
#include <algorithm>
#include <linux/cec.h>
#include "cec-htng.h"

#ifdef __ANDROID__
#include <android-config.h>
#else
#include <config.h>
#endif

#include "cec-ctl.h"
#include "cec-pin-gen.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static std::string find_opcode_name(__u8 opcode)
{
	for (unsigned i = 0; i < ARRAY_SIZE(msgtable); i++)
		if (msgtable[i].opcode == opcode)
			return std::string(": ") + msgtable[i].name;
	return "";
}

static std::string find_cdc_opcode_name(__u8 opcode)
{
	for (unsigned i = 0; i < ARRAY_SIZE(cdcmsgtable); i++)
		if (cdcmsgtable[i].opcode == opcode)
			return std::string(": ") + cdcmsgtable[i].name;
	return "";
}

enum cec_state {
	CEC_ST_IDLE,
	CEC_ST_RECEIVE_START_BIT,
	CEC_ST_RECEIVING_DATA,
};

/* All timings are in microseconds */
#define CEC_TIM_MARGIN			100

#define CEC_TIM_START_BIT_LOW		3700
#define CEC_TIM_START_BIT_LOW_MIN	3500
#define CEC_TIM_START_BIT_LOW_MAX	3900
#define CEC_TIM_START_BIT_TOTAL		4500
#define CEC_TIM_START_BIT_TOTAL_MIN	4300
#define CEC_TIM_START_BIT_TOTAL_MAX	4700

#define CEC_TIM_DATA_BIT_0_LOW		1500
#define CEC_TIM_DATA_BIT_0_LOW_MIN	1300
#define CEC_TIM_DATA_BIT_0_LOW_MAX	1700
#define CEC_TIM_DATA_BIT_1_LOW		600
#define CEC_TIM_DATA_BIT_1_LOW_MIN	400
#define CEC_TIM_DATA_BIT_1_LOW_MAX	800
#define CEC_TIM_DATA_BIT_TOTAL		2400
#define CEC_TIM_DATA_BIT_TOTAL_MIN	2050
#define CEC_TIM_DATA_BIT_TOTAL_MAX	2750
#define CEC_TIM_DATA_BIT_SAMPLE		1050
#define CEC_TIM_DATA_BIT_SAMPLE_MIN	850
#define CEC_TIM_DATA_BIT_SAMPLE_MAX	1250

#define CEC_TIM_IDLE_SAMPLE		1000
#define CEC_TIM_IDLE_SAMPLE_MIN		500
#define CEC_TIM_IDLE_SAMPLE_MAX		1500
#define CEC_TIM_START_BIT_SAMPLE	500
#define CEC_TIM_START_BIT_SAMPLE_MIN	300
#define CEC_TIM_START_BIT_SAMPLE_MAX	700

#define CEC_TIM_LOW_DRIVE_ERROR         (1.5 * CEC_TIM_DATA_BIT_TOTAL)
#define CEC_TIM_LOW_DRIVE_ERROR_MIN     (1.4 * CEC_TIM_DATA_BIT_TOTAL)
#define CEC_TIM_LOW_DRIVE_ERROR_MAX     (1.6 * CEC_TIM_DATA_BIT_TOTAL)

/*
 * Start and data bit error injection 'long' bit periods.
 * The CEC PIN framework can insert too-long bit periods
 * for either start or data bits. The period is stretched
 * to these values.
 *
 * We warn but accept bit periods up to these values in the
 * analyzer as this makes the analyzer results more readable.
 */
#define CEC_TIM_START_BIT_TOTAL_LONG	(5000 + CEC_TIM_MARGIN)
#define CEC_TIM_DATA_BIT_TOTAL_LONG	(2900 + CEC_TIM_MARGIN)

__u64 eob_ts;
__u64 eob_ts_max;

// Global CEC state
static enum cec_state state;
static double ts;
static __u64 low_usecs;
static unsigned int rx_bit;
static __u8 byte;
static bool eom;
static bool eom_reached;
static __u8 byte_cnt;
static bool bcast;
static bool cdc;
static struct cec_msg msg;

static void cec_pin_rx_start_bit_was_high(bool is_high, __u64 usecs, __u64 usecs_min, bool show)
{
	bool period_too_long = low_usecs + usecs > CEC_TIM_START_BIT_TOTAL_LONG;

	if (is_high && show)
		printf("%s: warn: start bit: total period too long\n", ts2s(ts).c_str());
	else if (low_usecs + usecs > CEC_TIM_START_BIT_TOTAL_MAX && show)
		printf("%s: warn: start bit: total period too long (%.2f > %.2f ms)\n",
		       ts2s(ts).c_str(), (low_usecs + usecs) / 1000.0,
		       CEC_TIM_START_BIT_TOTAL_MAX / 1000.0);
	if (is_high || period_too_long) {
		if (show)
			printf("\n");
		state = CEC_ST_IDLE;
		return;
	}
	if (low_usecs + usecs < CEC_TIM_START_BIT_TOTAL_MIN - CEC_TIM_MARGIN && show)
		printf("%s: warn: start bit: total period too short (%.2f < %.2f ms)\n",
		       ts2s(ts).c_str(), (low_usecs + usecs) / 1000.0,
		       CEC_TIM_START_BIT_TOTAL_MIN / 1000.0);
	state = CEC_ST_RECEIVING_DATA;
	rx_bit = 0;
	byte = 0;
	eom = false;
	eom_reached = false;
	byte_cnt = 0;
	bcast = false;
	cdc = false;
	msg.len = 0;
}

static void cec_pin_rx_start_bit_was_low(__u64 ev_ts, __u64 usecs, __u64 usecs_min, bool show)
{
	if (usecs_min > CEC_TIM_START_BIT_LOW_MAX && show)
		printf("%s: warn: start bit: low time too long (%.2f > %.2f ms)\n",
			ts2s(ts).c_str(), usecs / 1000.0,
			CEC_TIM_START_BIT_LOW_MAX / 1000.0);
	if (usecs_min > CEC_TIM_START_BIT_LOW_MAX + CEC_TIM_MARGIN * 5) {
		if (show)
			printf("\n");
		state = CEC_ST_IDLE;
		return;
	}
	if (usecs_min < CEC_TIM_START_BIT_LOW_MIN - CEC_TIM_MARGIN * 6) {
		state = CEC_ST_IDLE;
		return;
	}
	low_usecs = usecs;
	eob_ts = ev_ts + 1000 * (CEC_TIM_START_BIT_TOTAL - low_usecs);
	eob_ts_max = ev_ts + 1000 * (CEC_TIM_START_BIT_TOTAL_LONG - low_usecs);
}

static void cec_pin_rx_data_bit_was_high(bool is_high, __u64 ev_ts,
					 __u64 usecs, __u64 usecs_min, bool show)
{
	bool period_too_long = low_usecs + usecs > CEC_TIM_DATA_BIT_TOTAL_LONG;
	bool bit;

	if (is_high && rx_bit < 9 && show)
		printf("%s: warn: data bit %d: total period too long\n", ts2s(ts).c_str(), rx_bit);
	else if (rx_bit < 9 && show &&
		 low_usecs + usecs > CEC_TIM_DATA_BIT_TOTAL_MAX + CEC_TIM_MARGIN)
		printf("%s: warn: data bit %d: total period too long (%.2f ms)\n",
			ts2s(ts).c_str(), rx_bit, (low_usecs + usecs) / 1000.0);
	if (low_usecs + usecs < CEC_TIM_DATA_BIT_TOTAL_MIN - CEC_TIM_MARGIN && show)
		printf("%s: warn: data bit %d: total period too short (%.2f ms)\n",
			ts2s(ts).c_str(), rx_bit, (low_usecs + usecs) / 1000.0);

	bit = low_usecs < CEC_TIM_DATA_BIT_1_LOW_MAX + CEC_TIM_MARGIN;
	if (rx_bit <= 7) {
		byte |= bit << (7 - rx_bit);
	} else if (rx_bit == 8) {
		eom = bit;
	} else {
		std::string s;

		if (byte_cnt == 0) {
			bcast = (byte & 0xf) == 0xf;
			s = ": " + std::string(la2s(byte >> 4)) +
			    " to " + (bcast ? "All" : la2s(byte & 0xf));
		} else if (byte_cnt == 1) {
			s = find_opcode_name(byte);
		} else if (cdc && byte_cnt == 4) {
			s = find_cdc_opcode_name(byte);
		}

		bool ack = !(bcast ^ bit);

		if (msg.len < CEC_MAX_MSG_SIZE)
			msg.msg[msg.len++] = byte;
		if (show)
			printf("%s: rx 0x%02x%s%s%s%s%s\n", ts2s(ts).c_str(), byte,
			       eom ? " EOM" : "", ack ? " ACK" : " NACK",
			       bcast ? " (broadcast)" : "",
			       eom_reached ? " (warn: spurious byte)" : "",
			       s.c_str());
		if (!eom_reached && is_high && !eom && ack && show)
			printf("%s: warn: missing EOM\n", ts2s(ts).c_str());
		else if (!is_high && !period_too_long && verbose && show)
			printf("\n");
		if (byte_cnt == 1 && byte == CEC_MSG_CDC_MESSAGE)
			cdc = true;
		byte_cnt++;
		if (byte_cnt >= CEC_MAX_MSG_SIZE)
			eom_reached = true;
		if (show && eom && msg.len > 2) {
			msg.rx_status = CEC_RX_STATUS_OK;
			msg.rx_ts = ev_ts;
			printf("\nTransmit from %s to %s (%d to %d):\n",
			       la2s(cec_msg_initiator(&msg)),
			       cec_msg_is_broadcast(&msg) ? "all" : la2s(cec_msg_destination(&msg)),
			       cec_msg_initiator(&msg), cec_msg_destination(&msg));
			log_msg(&msg);
		}
	}
	rx_bit++;
	if ((is_high || period_too_long) && !eom) {
		eom_reached = false;
		if (show)
			printf("\n");
		state = is_high ? CEC_ST_IDLE : CEC_ST_RECEIVE_START_BIT;
		return;
	}
	if (rx_bit == 10) {
		if (eom) {
			eom_reached = true;
			if (is_high) {
				if (show)
					printf("\n");
				state = CEC_ST_IDLE;
			}
		}
		rx_bit = 0;
		byte = 0;
		eom = false;
	}
}

static void cec_pin_rx_data_bit_was_low(__u64 ev_ts, __u64 usecs, __u64 usecs_min, bool show)
{
	/*
	 * If the low drive starts at the end of a 0 bit, then the actual
	 * maximum time that the bus can be low is the two summed.
	 */
	const unsigned max_low_drive = CEC_TIM_LOW_DRIVE_ERROR_MAX +
		CEC_TIM_DATA_BIT_0_LOW_MAX + CEC_TIM_MARGIN;

	low_usecs = usecs;
	if (usecs >= CEC_TIM_LOW_DRIVE_ERROR_MIN - CEC_TIM_MARGIN) {
		if (usecs >= max_low_drive && show)
			printf("%s: warn: low drive too long (%.2f > %.2f ms)\n\n",
			       ts2s(ts).c_str(), usecs / 1000.0,
			       CEC_TIM_LOW_DRIVE_ERROR_MAX / 1000.0);
		if (show)
			printf("\n");
		state = CEC_ST_IDLE;
		return;
	}

	if (rx_bit == 0 && byte_cnt &&
	    usecs >= CEC_TIM_START_BIT_LOW_MIN - CEC_TIM_MARGIN) {
		if (show)
			printf("%s: warn: unexpected start bit\n", ts2s(ts).c_str());
		cec_pin_rx_start_bit_was_low(ev_ts, usecs, usecs_min, show);
		state = CEC_ST_RECEIVE_START_BIT;
		return;
	}

	if (usecs_min > CEC_TIM_DATA_BIT_0_LOW_MAX) {
		if (show)
			printf("%s: warn: data bit %d: low time too long (%.2f ms)\n",
				ts2s(ts).c_str(), rx_bit, usecs / 1000.0);
		if (usecs_min > CEC_TIM_DATA_BIT_TOTAL_MAX) {
			if (show)
				printf("\n");
			state = CEC_ST_IDLE;
		}
		return;
	}
	if (usecs_min > CEC_TIM_DATA_BIT_1_LOW_MAX &&
	    usecs < CEC_TIM_DATA_BIT_0_LOW_MIN - CEC_TIM_MARGIN && show) {
		printf("%s: warn: data bit %d: invalid 0->1 transition (%.2f ms)\n",
			ts2s(ts).c_str(), rx_bit, usecs / 1000.0);
	}
	if (usecs < CEC_TIM_DATA_BIT_1_LOW_MIN - CEC_TIM_MARGIN && show) {
		printf("%s: warn: data bit %d: low time too short (%.2f ms)\n",
			ts2s(ts).c_str(), rx_bit, usecs / 1000.0);
	}

	eob_ts = ev_ts + 1000 * (CEC_TIM_DATA_BIT_TOTAL - low_usecs);
	eob_ts_max = ev_ts + 1000 * (CEC_TIM_DATA_BIT_TOTAL_LONG - low_usecs);
}

static void cec_pin_debug(__u64 ev_ts, __u64 usecs, bool was_high, bool is_high, bool show)
{
	__u64 usecs_min = usecs > CEC_TIM_MARGIN ? usecs - CEC_TIM_MARGIN : 0;

	switch (state) {
	case CEC_ST_RECEIVE_START_BIT:
		eom_reached = false;
		if (was_high)
			cec_pin_rx_start_bit_was_high(is_high, usecs, usecs_min, show);
		else
			cec_pin_rx_start_bit_was_low(ev_ts, usecs, usecs_min, show);
		break;

	case CEC_ST_RECEIVING_DATA:
		if (was_high)
			cec_pin_rx_data_bit_was_high(is_high, ev_ts, usecs, usecs_min, show);
		else
			cec_pin_rx_data_bit_was_low(ev_ts, usecs, usecs_min, show);
		break;

	case CEC_ST_IDLE:
		eom_reached = false;
		if (!is_high)
			state = CEC_ST_RECEIVE_START_BIT;
		break;
	}
}

void log_event_pin(bool is_high, __u64 ev_ts, bool show)
{
	static __u64 last_ts;
	static __u64 last_change_ts;
	static __u64 last_1_to_0_ts;
	static bool was_high = true;
	double bit_periods = ((ev_ts - last_ts) / 1000.0) / CEC_TIM_DATA_BIT_TOTAL;

	eob_ts = eob_ts_max = 0;

	ts = ev_ts / 1000000000.0;
	if (last_change_ts == 0) {
		last_ts = last_change_ts = last_1_to_0_ts = ev_ts - CEC_TIM_DATA_BIT_TOTAL * 16000;
		if (is_high)
			return;
	}
	if (verbose && show) {
		double delta = (ev_ts - last_change_ts) / 1000000.0;

		if (!was_high && last_change_ts && state == CEC_ST_RECEIVE_START_BIT &&
		    delta * 1000 >= CEC_TIM_START_BIT_LOW_MIN - CEC_TIM_MARGIN)
			printf("\n");
		printf("%s: ", ts2s(ts).c_str());
		if (last_change_ts && is_high && was_high &&
		    (ev_ts - last_1_to_0_ts) / 1000000 <= 10)
			printf("1 -> 1 (was 1 for %.2f ms, period of previous %spulse %.2f ms)\n",
			       delta, state == CEC_ST_RECEIVE_START_BIT ? "start " : "",
			       (ev_ts - last_1_to_0_ts) / 1000000.0);
		else if (last_change_ts && is_high && was_high)
			printf("1 -> 1 (%.2f ms)\n", delta);
		else if (was_high && state == CEC_ST_IDLE) {
			if (bit_periods > 1 && bit_periods < 10)
				printf("1 -> 0 (was 1 for %.2f ms, free signal time = %.1f bit periods)\n",
				       delta, bit_periods);
			else
				printf("1 -> 0 (was 1 for %.2f ms)\n", delta);
		} else if (was_high && (ev_ts - last_1_to_0_ts) / 1000000 <= 10)
			printf("1 -> 0 (was 1 for %.2f ms, period of previous %spulse %.2f ms)\n",
			       delta, state == CEC_ST_RECEIVE_START_BIT ? "start " : "",
			       (ev_ts - last_1_to_0_ts) / 1000000.0);
		else if (was_high)
			printf("1 -> 0 (was 1 for %.2f ms)\n", delta);
		else if (last_change_ts && state == CEC_ST_RECEIVE_START_BIT &&
			 delta * 1000 < CEC_TIM_START_BIT_LOW_MIN - CEC_TIM_MARGIN)
			printf("0 -> 1 (was 0 for %.2f ms, might indicate %d bit)\n", delta,
			       delta * 1000 < CEC_TIM_DATA_BIT_1_LOW_MAX + CEC_TIM_MARGIN);
		else if (last_change_ts && state == CEC_ST_RECEIVE_START_BIT)
			printf("0 -> 1 (was 0 for %.2f ms)\n", delta);
		else if (last_change_ts &&
			 delta * 1000 >= CEC_TIM_LOW_DRIVE_ERROR_MIN - CEC_TIM_MARGIN)
			printf("0 -> 1 (was 0 for %.2f ms, warn: indicates low drive)\n", delta);
		else if (last_change_ts)
			printf("0 -> 1 (was 0 for %.2f ms, indicates %d bit)\n", delta,
			       delta * 1000 < CEC_TIM_DATA_BIT_1_LOW_MAX + CEC_TIM_MARGIN);
		else
			printf("0 -> 1\n");
	} else if (!is_high && bit_periods > 1 && bit_periods < 10 && show) {
		printf("%s: free signal time = %.1f bit periods\n",
		       ts2s(ts).c_str(), bit_periods);
	}
	cec_pin_debug(ev_ts, (ev_ts - last_ts) / 1000, was_high, is_high, show);
	last_change_ts = ev_ts;
	if (!is_high)
		last_1_to_0_ts = ev_ts;
	last_ts = ev_ts;
	was_high = is_high;
}
