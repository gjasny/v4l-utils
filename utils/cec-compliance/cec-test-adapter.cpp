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
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sstream>

#include "cec-compliance.h"

static const __u8 tx_ok_retry_mask = CEC_TX_STATUS_OK | CEC_TX_STATUS_MAX_RETRIES;

// Flush any pending messages
static int flush_pending_msgs(struct node *node)
{
	int res;

	do {
		struct cec_msg msg;

		memset(&msg, 0xff, sizeof(msg));
		msg.timeout = 1;
		res = doioctl(node, CEC_RECEIVE, &msg);
		fail_on_test(res && res != ETIMEDOUT);
	} while (res == 0);
	return 0;
}

int testCap(struct node *node)
{
	struct cec_caps caps;

	memset(&caps, 0xff, sizeof(caps));
	// Must always be there
	fail_on_test(doioctl(node, CEC_ADAP_G_CAPS, &caps));
	fail_on_test(caps.available_log_addrs == 0 ||
		     caps.available_log_addrs > CEC_MAX_LOG_ADDRS);
	fail_on_test((caps.capabilities & CEC_CAP_PASSTHROUGH) &&
		     !(caps.capabilities & CEC_CAP_TRANSMIT));
	return 0;
}

int testDQEvent(struct node *node)
{
	struct cec_event ev;

	memset(&ev, 0xff, sizeof(ev));
	fail_on_test(doioctl(node, CEC_DQEVENT, &ev));
	fail_on_test(!(ev.flags & CEC_EVENT_FL_INITIAL_STATE));
	fail_on_test(ev.flags & ~CEC_EVENT_FL_INITIAL_STATE);
	fail_on_test(ev.ts == 0 || ev.ts == ~0ULL);
	fail_on_test(ev.event != CEC_EVENT_STATE_CHANGE);
	fail_on_test(ev.state_change.log_addr_mask == 0xffff);
	memset(&ev.state_change, 0, sizeof(ev.state_change));
	fail_on_test(check_0(ev.raw, sizeof(ev.raw)));
	return 0;
}

int testAdapPhysAddr(struct node *node)
{
	__u16 old_pa = 0xefff;
	__u16 pa = 0x1000;

	fail_on_test(doioctl(node, CEC_ADAP_G_PHYS_ADDR, &old_pa));
	fail_on_test(old_pa == 0xefff);
	if (node->caps & CEC_CAP_PHYS_ADDR) {
		fail_on_test(doioctl(node, CEC_ADAP_S_PHYS_ADDR, &pa));
		fail_on_test(doioctl(node, CEC_ADAP_G_PHYS_ADDR, &pa));
		fail_on_test(pa != 0x1000);

		fail_on_test(doioctl(node, CEC_ADAP_S_PHYS_ADDR, &old_pa));
		fail_on_test(doioctl(node, CEC_ADAP_G_PHYS_ADDR, &pa));
		fail_on_test(pa != old_pa);
	} else {
		fail_on_test(doioctl(node, CEC_ADAP_S_PHYS_ADDR, &pa) != ENOTTY);
	}
	return 0;
}

int testAdapLogAddrs(struct node *node)
{
	static const __u8 la_types[] = {
		CEC_LOG_ADDR_TYPE_TV,
		CEC_LOG_ADDR_TYPE_RECORD,
		CEC_LOG_ADDR_TYPE_TUNER,
		CEC_LOG_ADDR_TYPE_AUDIOSYSTEM
	};
	static const __u8 prim_dev_types[] = {
		CEC_OP_PRIM_DEVTYPE_TV,
		CEC_OP_PRIM_DEVTYPE_RECORD,
		CEC_OP_PRIM_DEVTYPE_TUNER,
		CEC_OP_PRIM_DEVTYPE_AUDIOSYSTEM
	};
	static const __u8 all_dev_types[2] = {
		CEC_OP_ALL_DEVTYPE_TV | CEC_OP_ALL_DEVTYPE_RECORD |
		CEC_OP_ALL_DEVTYPE_AUDIOSYSTEM,
		CEC_OP_ALL_DEVTYPE_RECORD | CEC_OP_ALL_DEVTYPE_AUDIOSYSTEM,
	};
	static const __u8 features[12] = {
		0x90, 0x00, 0x8e, 0x00,
		0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff
	};
	struct cec_log_addrs clear = { };
	struct cec_log_addrs laddrs;
	struct cec_event ev;
	int res;

	memset(&laddrs, 0xff, sizeof(laddrs));
	fail_on_test(doioctl(node, CEC_ADAP_G_LOG_ADDRS, &laddrs));
	fail_on_test(laddrs.vendor_id != CEC_VENDOR_ID_NONE &&
		     (laddrs.vendor_id & 0xff000000));
	fail_on_test(laddrs.cec_version != CEC_OP_CEC_VERSION_1_4 &&
		     laddrs.cec_version != CEC_OP_CEC_VERSION_2_0);
	fail_on_test(laddrs.num_log_addrs > CEC_MAX_LOG_ADDRS);

	if (node->phys_addr == CEC_PHYS_ADDR_INVALID || laddrs.num_log_addrs == 0)
		fail_on_test(laddrs.log_addr_mask);
	else
		fail_on_test(!laddrs.log_addr_mask);

	if (!(node->caps & CEC_CAP_LOG_ADDRS)) {
		fail_on_test(!laddrs.num_log_addrs);
		fail_on_test(doioctl(node, CEC_ADAP_S_LOG_ADDRS, &laddrs) != ENOTTY);
		return 0;
	}

	fail_on_test(doioctl(node, CEC_ADAP_S_LOG_ADDRS, &clear));
	fail_on_test(clear.num_log_addrs != 0);
	fail_on_test(clear.log_addr_mask != 0);
	fail_on_test(doioctl(node, CEC_ADAP_G_LOG_ADDRS, &laddrs));
	fail_on_test(laddrs.num_log_addrs != 0);
	fail_on_test(laddrs.log_addr_mask != 0);

	__u16 pa;

	fail_on_test(doioctl(node, CEC_ADAP_G_PHYS_ADDR, &pa));
	fail_on_test(pa != node->phys_addr);
	unsigned skip_tv = pa ? 1 : 0;
	unsigned available_log_addrs = node->available_log_addrs;

	if (skip_tv && available_log_addrs == CEC_MAX_LOG_ADDRS)
		available_log_addrs--;
	memset(&laddrs, 0, sizeof(laddrs));
	strcpy(laddrs.osd_name, "Compliance");
	laddrs.num_log_addrs = available_log_addrs;
	laddrs.cec_version = laddrs.num_log_addrs > 2 ?
		CEC_OP_CEC_VERSION_1_4: CEC_OP_CEC_VERSION_2_0;

	for (unsigned i = 0; i < CEC_MAX_LOG_ADDRS - skip_tv; i++) {
		laddrs.log_addr_type[i] = la_types[i + skip_tv];
		laddrs.primary_device_type[i] = prim_dev_types[i + skip_tv];
		laddrs.all_device_types[i] = all_dev_types[skip_tv];
		memcpy(laddrs.features[i], features, sizeof(features));
	}

	fail_on_test(doioctl(node, CEC_ADAP_S_LOG_ADDRS, &laddrs));
	fail_on_test(laddrs.num_log_addrs != available_log_addrs);
	fail_on_test(laddrs.log_addr_mask == 0);
	for (unsigned i = 0; i < laddrs.num_log_addrs; i++) {
		fail_on_test(laddrs.log_addr[i] == CEC_LOG_ADDR_INVALID);
		fail_on_test(memcmp(laddrs.features[i], features, 4));
		fail_on_test(check_0(laddrs.features[i] + 4, 8));
	}
	for (unsigned i = laddrs.num_log_addrs; i < CEC_MAX_LOG_ADDRS; i++) {
		fail_on_test(laddrs.log_addr_type[i]);
		fail_on_test(laddrs.primary_device_type[i]);
		fail_on_test(laddrs.all_device_types[i]);
		fail_on_test(check_0(laddrs.features[i], sizeof(laddrs.features[i])));
	}
	fail_on_test(doioctl(node, CEC_ADAP_S_LOG_ADDRS, &laddrs) != EBUSY);

	fcntl(node->fd, F_SETFL, fcntl(node->fd, F_GETFL) | O_NONBLOCK);
	do {
		res = doioctl(node, CEC_DQEVENT, &ev);
		fail_on_test(res && res != EAGAIN);
		if (!res) {
			struct timeval tv = { 0, 10000 }; // 10 ms

			fail_on_test(ev.flags & CEC_EVENT_FL_INITIAL_STATE);
			select(0, NULL, NULL, NULL, &tv);
		}
	} while (!res);
	fail_on_test(doioctl(node, CEC_ADAP_S_LOG_ADDRS, &clear));
	do {
		struct timeval tv = { 0, 10000 }; // 10 ms

		res = doioctl(node, CEC_DQEVENT, &ev);
		fail_on_test(res && res != EAGAIN);
		if (res)
			select(0, NULL, NULL, NULL, &tv);
	} while (res);
	fail_on_test(ev.flags & CEC_EVENT_FL_INITIAL_STATE);
	fail_on_test(ev.ts == 0);
	fail_on_test(ev.event != CEC_EVENT_STATE_CHANGE);
	fail_on_test(ev.state_change.phys_addr != node->phys_addr);
	fail_on_test(ev.state_change.log_addr_mask);

	fail_on_test(doioctl(node, CEC_ADAP_S_LOG_ADDRS, &laddrs));
	do {
		struct timeval tv = { 0, 10000 }; // 10 ms

		res = doioctl(node, CEC_DQEVENT, &ev);
		fail_on_test(res && res != EAGAIN);
		if (res)
			select(0, NULL, NULL, NULL, &tv);
	} while (res);
	fail_on_test(ev.flags & CEC_EVENT_FL_INITIAL_STATE);
	fail_on_test(ev.ts == 0);
	fail_on_test(ev.event != CEC_EVENT_STATE_CHANGE);
	fail_on_test(ev.state_change.phys_addr != node->phys_addr);
	fail_on_test(ev.state_change.log_addr_mask == 0);
	return 0;
}

int testTransmit(struct node *node)
{
	struct cec_msg msg = { };
	unsigned i, la = node->log_addr[0];
	unsigned valid_la = 15, invalid_la = 15;
	bool tested_self = false;
	bool tested_valid_la = false;
	bool tested_invalid_la = false;

	if (!(node->caps & CEC_CAP_TRANSMIT)) {
		cec_msg_init(&msg, la, 0);
		fail_on_test(doioctl(node, CEC_TRANSMIT, &msg) != ENOTTY);
		return NOTSUPPORTED;
	}

	/* Check invalid messages */
	cec_msg_init(&msg, la, 0xf);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg) != EINVAL);

	cec_msg_init(&msg, la, 0);
	msg.timeout = 1000;
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg) != EINVAL);

	cec_msg_init(&msg, la, 0);
	msg.len = 0;
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg) != EINVAL);

	cec_msg_init(&msg, la, 0);
	msg.len = CEC_MAX_MSG_SIZE + 1;
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg) != EINVAL);

	cec_msg_init(&msg, la, 0);
	msg.reply = CEC_MSG_CEC_VERSION;
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg) != EINVAL);

	for (i = 0; i < 15; i++) {
		if (tested_self && (node->adap_la_mask & (1 << i)))
			continue;

		memset(&msg, 0xff, sizeof(msg));
		msg.msg[0] = 0xf0 | i;
		msg.len = 1;
		msg.timeout = 0;
		msg.reply = 0;
		msg.flags &= ~CEC_MSG_FL_REPLY_TO_FOLLOWERS;

		fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));

		fail_on_test(msg.len != 1);
		fail_on_test(check_0(msg.msg + msg.len, sizeof(msg.msg) - msg.len));
		fail_on_test(msg.timeout);
		fail_on_test(msg.reply);
		fail_on_test(msg.flags);
		fail_on_test(msg.rx_status);
		fail_on_test(msg.rx_ts);
		fail_on_test(msg.tx_status == 0);
		fail_on_test(msg.tx_ts == 0);
		fail_on_test(msg.tx_ts == ~0ULL);
		fail_on_test(msg.msg[0] != (0xf0 | i));
		fail_on_test(msg.sequence == 0);
		fail_on_test(msg.sequence == ~0U);
		fail_on_test(msg.tx_status & ~0x3f);
		fail_on_test((msg.tx_status & tx_ok_retry_mask) == tx_ok_retry_mask);
		fail_on_test(!(msg.tx_status & tx_ok_retry_mask));

		if (node->adap_la_mask & (1 << i)) {
			// Send message to yourself
			fail_on_test(msg.tx_status !=
				     (CEC_TX_STATUS_NACK | CEC_TX_STATUS_MAX_RETRIES));
			fail_on_test(msg.tx_nack_cnt != 1);
			fail_on_test(msg.tx_arb_lost_cnt);
			fail_on_test(msg.tx_low_drive_cnt);
			fail_on_test(msg.tx_error_cnt);

			cec_msg_init(&msg, i, i);
			cec_msg_give_physical_addr(&msg, true);
			fail_on_test(doioctl(node, CEC_TRANSMIT, &msg) != EINVAL);
		} else if (msg.tx_status & CEC_TX_STATUS_OK) {
			if (tested_valid_la)
				continue;
			tested_valid_la = true;
			valid_la = i;
			// Send message to a remote LA
			memset(&msg, 0xff, sizeof(msg));
			msg.msg[0] = (la << 4) | i;
			msg.timeout = 1001;
			msg.flags &= ~CEC_MSG_FL_REPLY_TO_FOLLOWERS;
			cec_msg_give_physical_addr(&msg, true);
			fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
			fail_on_test(!(msg.tx_status & CEC_TX_STATUS_OK));
			fail_on_test(msg.rx_status & CEC_RX_STATUS_TIMEOUT);
			fail_on_test(!(msg.rx_status & CEC_RX_STATUS_OK));
			fail_on_test(msg.len != 5);
			fail_on_test(check_0(msg.msg + msg.len, sizeof(msg.msg) - msg.len));
			fail_on_test(msg.timeout != 1001);
			fail_on_test(msg.sequence == 0 || msg.sequence == ~0U);
			fail_on_test(msg.reply != CEC_MSG_REPORT_PHYSICAL_ADDR);
			fail_on_test(msg.rx_ts == 0);
			fail_on_test(msg.rx_ts == ~0ULL);
			fail_on_test(msg.flags);
			fail_on_test(msg.rx_status == 0);
			fail_on_test(msg.rx_status & ~0x07);
			fail_on_test(msg.tx_ts == 0);
			fail_on_test(msg.tx_ts == ~0ULL);
			fail_on_test(msg.rx_ts <= msg.tx_ts);
			fail_on_test((msg.tx_status & tx_ok_retry_mask) == tx_ok_retry_mask);
			fail_on_test(msg.tx_nack_cnt == 0xff);
			fail_on_test(msg.tx_arb_lost_cnt == 0xff);
			fail_on_test(msg.tx_low_drive_cnt == 0xff);
			fail_on_test(msg.tx_error_cnt == 0xff);

			memset(&msg, 0xff, sizeof(msg));
			msg.msg[0] = (la << 4) | i;
			msg.timeout = 0;
			msg.flags &= ~CEC_MSG_FL_REPLY_TO_FOLLOWERS;
			cec_msg_give_physical_addr(&msg, false);
			fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
			fail_on_test(msg.timeout);
			fail_on_test(msg.sequence == 0 || msg.sequence == ~0U);
			fail_on_test(msg.reply);
			fail_on_test(check_0(msg.msg + msg.len, sizeof(msg.msg) - msg.len));
			fail_on_test(msg.rx_status);
			fail_on_test(msg.rx_ts);
			fail_on_test(msg.flags);
			fail_on_test(msg.tx_ts == 0);
			fail_on_test(msg.tx_ts == ~0ULL);
			fail_on_test(!(msg.tx_status & CEC_TX_STATUS_OK));
			fail_on_test((msg.tx_status & tx_ok_retry_mask) == tx_ok_retry_mask);
			fail_on_test(msg.tx_nack_cnt == 0xff);
			fail_on_test(msg.tx_arb_lost_cnt == 0xff);
			fail_on_test(msg.tx_low_drive_cnt == 0xff);
			fail_on_test(msg.tx_error_cnt == 0xff);
		} else {
			if (tested_invalid_la)
				continue;
			tested_invalid_la = true;
			invalid_la = i;
			// Send message to a remote non-existent LA
			memset(&msg, 0xff, sizeof(msg));
			msg.msg[0] = (la << 4) | i;
			msg.timeout = 1002;
			msg.flags &= ~CEC_MSG_FL_REPLY_TO_FOLLOWERS;
			cec_msg_give_physical_addr(&msg, true);
			fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
			fail_on_test(msg.timeout != 1002);
			fail_on_test(msg.sequence == 0);
			fail_on_test(msg.sequence == ~0U);
			fail_on_test(msg.len != 2);
			fail_on_test(check_0(msg.msg + msg.len, sizeof(msg.msg) - msg.len));
			fail_on_test(msg.flags);
			fail_on_test(msg.reply != CEC_MSG_REPORT_PHYSICAL_ADDR);
			fail_on_test(!(msg.tx_status & CEC_TX_STATUS_MAX_RETRIES));
			fail_on_test((msg.tx_status & tx_ok_retry_mask) == tx_ok_retry_mask);
			fail_on_test(msg.rx_status);
			fail_on_test(msg.rx_ts);
			fail_on_test(msg.tx_ts == 0);
			fail_on_test(msg.tx_ts == ~0ULL);
			fail_on_test(msg.tx_nack_cnt == 0xff);
			fail_on_test(msg.tx_arb_lost_cnt == 0xff);
			fail_on_test(msg.tx_low_drive_cnt == 0xff);
			fail_on_test(msg.tx_error_cnt == 0xff);

			memset(&msg, 0xff, sizeof(msg));
			msg.msg[0] = (la << 4) | i;
			msg.timeout = 0;
			msg.flags &= ~CEC_MSG_FL_REPLY_TO_FOLLOWERS;
			cec_msg_give_physical_addr(&msg, false);
			fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
			fail_on_test(msg.timeout);
			fail_on_test(msg.sequence == 0);
			fail_on_test(msg.sequence == ~0U);
			fail_on_test(msg.reply);
			fail_on_test(msg.len != 2);
			fail_on_test(check_0(msg.msg + msg.len, sizeof(msg.msg) - msg.len));
			fail_on_test(msg.rx_status);
			fail_on_test(msg.rx_ts);
			fail_on_test(msg.flags);
			fail_on_test(msg.tx_ts == 0);
			fail_on_test(msg.tx_ts == ~0ULL);
			fail_on_test(!(msg.tx_status & CEC_TX_STATUS_MAX_RETRIES));
			fail_on_test((msg.tx_status & tx_ok_retry_mask) == tx_ok_retry_mask);
			fail_on_test(msg.tx_nack_cnt == 0xff);
			fail_on_test(msg.tx_arb_lost_cnt == 0xff);
			fail_on_test(msg.tx_low_drive_cnt == 0xff);
			fail_on_test(msg.tx_error_cnt == 0xff);
		}
	}

	if (tested_valid_la) {
		time_t cur_t = time(NULL), t;
		time_t last_t = cur_t + 7;
		unsigned max_cnt = 0;
		unsigned cnt = 0;
	
		do {
			t = time(NULL);
			if (t != cur_t) {
				if (cnt > max_cnt)
					max_cnt = cnt;
				cnt = 0;
				cur_t = t;
			}
			cec_msg_init(&msg, la, valid_la);
			fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
			cnt++;
		} while (t < last_t);
		// A ping can take 10 * 2.4 + 4.5 + 7 * 2.4 = 45.3 ms (SFT)
		if (max_cnt < 21)
			warn("Could only do %u pings per second to a valid LA, expected at least 21\n",
			     max_cnt);
	}

	if (tested_invalid_la) {
		time_t cur_t = time(NULL), t;
		time_t last_t = cur_t + 7;
		unsigned max_cnt = 0;
		unsigned cnt = 0;
	
		do {
			t = time(NULL);
			if (t != cur_t) {
				if (cnt > max_cnt)
					max_cnt = cnt;
				cnt = 0;
				cur_t = t;
			}
			cec_msg_init(&msg, la, invalid_la);
			fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
			cnt++;
		} while (t < last_t);
		// A ping to an invalid LA can take 2 * (10 * 2.4 + 4.5) + (3 + 7) * 2.4 = 81 ms (SFT)
		if (max_cnt < 12)
			warn("Could only do %u pings per second to an invalid LA, expected at least 12\n",
			     max_cnt);
	}

	return 0;
}

int testReceive(struct node *node)
{
	unsigned la = node->log_addr[0], remote_la = 0;
	struct cec_msg msg;

	if (!(node->caps & CEC_CAP_TRANSMIT)) {
		fail_on_test(doioctl(node, CEC_RECEIVE, &msg) != ENOTTY);
		return NOTSUPPORTED;
	}

	for (unsigned i = 0; i < 15; i++) {
		if (node->remote_la_mask & (1 << i))
			break;
		remote_la++;
	}

	fail_on_test(flush_pending_msgs(node));

	if (remote_la == 15)
		return PRESUMED_OK;

	cec_msg_init(&msg, la, remote_la);
	cec_msg_give_physical_addr(&msg, false);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));

	msg.timeout = 1500;
	fail_on_test(doioctl(node, CEC_RECEIVE, &msg) != ETIMEDOUT);
	fail_on_test(msg.timeout != 1500);

	__u32 mode = CEC_MODE_INITIATOR | CEC_MODE_FOLLOWER;

	fail_on_test(doioctl(node, CEC_S_MODE, &mode));

	cec_msg_init(&msg, la, remote_la);
	cec_msg_give_physical_addr(&msg, false);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));

	memset(&msg, 0xff, sizeof(msg));
	msg.timeout = 1500;
	fail_on_test(doioctl(node, CEC_RECEIVE, &msg));

	fail_on_test(msg.tx_ts);
	fail_on_test(msg.tx_status);
	fail_on_test(msg.tx_arb_lost_cnt);
	fail_on_test(msg.tx_nack_cnt);
	fail_on_test(msg.tx_low_drive_cnt);
	fail_on_test(msg.tx_error_cnt);
	fail_on_test(msg.timeout != 1500);
	fail_on_test(msg.len != 5);
	fail_on_test(msg.msg[1] != CEC_MSG_REPORT_PHYSICAL_ADDR);
	fail_on_test(check_0(msg.msg + msg.len, sizeof(msg.msg) - msg.len));
	fail_on_test(msg.msg[0] != ((remote_la << 4) | 0xf));
	fail_on_test(msg.sequence);
	fail_on_test(msg.rx_ts == 0);
	fail_on_test(msg.flags);
	fail_on_test(msg.reply);
	fail_on_test(msg.rx_status != CEC_RX_STATUS_OK);

	return 0;
}

int testNonBlocking(struct node *node)
{
	unsigned la = node->log_addr[0], remote_la = 0, invalid_remote = 0xf;
	struct cec_msg msg;

	if (!(node->caps & CEC_CAP_TRANSMIT))
		return NOTSUPPORTED;

	for (unsigned i = 0; i < 15; i++) {
		if (node->remote_la_mask & (1 << i))
			break;
		if (invalid_remote == 0xf && !(node->adap_la_mask & (1 << i)))
			invalid_remote = i;
		remote_la++;
	}

	fail_on_test(flush_pending_msgs(node));

	fcntl(node->fd, F_SETFL, fcntl(node->fd, F_GETFL) | O_NONBLOCK);

	fail_on_test(doioctl(node, CEC_RECEIVE, &msg) != EAGAIN);

	cec_msg_init(&msg, la, la);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
	fail_on_test(msg.tx_status != (CEC_TX_STATUS_NACK | CEC_TX_STATUS_MAX_RETRIES));

	if (invalid_remote < 15) {
		__u32 seq;

		// Send non-blocking non-reply message to invalid remote LA
		memset(&msg, 0xff, sizeof(msg));
		msg.msg[0] = (la << 4) | invalid_remote;
		msg.timeout = 0;
		cec_msg_give_physical_addr(&msg, false);
		fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
		fail_on_test(msg.len != 2);
		fail_on_test(check_0(msg.msg + msg.len, sizeof(msg.msg) - msg.len));
		fail_on_test(msg.msg[0] != ((la << 4) | invalid_remote));
		fail_on_test(msg.msg[1] != CEC_MSG_GIVE_PHYSICAL_ADDR);
		fail_on_test(msg.timeout);
		fail_on_test(msg.flags);
		fail_on_test(msg.sequence == 0);
		fail_on_test(msg.sequence == ~0U);
		fail_on_test(msg.reply);
		fail_on_test(msg.tx_status);
		fail_on_test(msg.rx_ts);
		fail_on_test(msg.rx_status);
		fail_on_test(msg.tx_ts);
		fail_on_test(msg.tx_nack_cnt);
		fail_on_test(msg.tx_arb_lost_cnt);
		fail_on_test(msg.tx_low_drive_cnt);
		fail_on_test(msg.tx_error_cnt);
		seq = msg.sequence;

		sleep(1);
		while (true) {
			memset(&msg, 0xff, sizeof(msg));
			msg.timeout = 1500;
			fail_on_test(doioctl(node, CEC_RECEIVE, &msg));
			if (msg.sequence == 0)
				continue;
			fail_on_test(msg.sequence != seq);
			fail_on_test(msg.len != 2);
			fail_on_test(check_0(msg.msg + msg.len, sizeof(msg.msg) - msg.len));
			fail_on_test(msg.msg[0] != ((la << 4) | invalid_remote));
			fail_on_test(msg.msg[1] != CEC_MSG_GIVE_PHYSICAL_ADDR);
			fail_on_test(msg.timeout != 1500);
			fail_on_test(msg.flags);
			fail_on_test(msg.reply);
			fail_on_test(!(msg.tx_status & CEC_TX_STATUS_MAX_RETRIES));
			fail_on_test((msg.tx_status & tx_ok_retry_mask) == tx_ok_retry_mask);
			fail_on_test(msg.tx_nack_cnt == 0xff);
			fail_on_test(msg.tx_arb_lost_cnt == 0xff);
			fail_on_test(msg.tx_low_drive_cnt == 0xff);
			fail_on_test(msg.tx_error_cnt == 0xff);
			fail_on_test(msg.rx_ts);
			fail_on_test(msg.rx_status);
			break;
		}

		// Send non-blocking reply message to invalid remote LA
		memset(&msg, 0xff, sizeof(msg));
		msg.msg[0] = (la << 4) | invalid_remote;
		msg.timeout = 0;
		msg.flags &= ~CEC_MSG_FL_REPLY_TO_FOLLOWERS;
		cec_msg_give_physical_addr(&msg, true);
		fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
		fail_on_test(msg.len != 2);
		fail_on_test(check_0(msg.msg + msg.len, sizeof(msg.msg) - msg.len));
		fail_on_test(msg.msg[0] != ((la << 4) | invalid_remote));
		fail_on_test(msg.msg[1] != CEC_MSG_GIVE_PHYSICAL_ADDR);
		fail_on_test(msg.timeout != 1000);
		fail_on_test(msg.flags);
		fail_on_test(msg.sequence == 0);
		fail_on_test(msg.sequence == ~0U);
		fail_on_test(msg.reply != CEC_MSG_REPORT_PHYSICAL_ADDR);
		fail_on_test(msg.tx_status);
		fail_on_test(msg.rx_ts);
		fail_on_test(msg.rx_status);
		fail_on_test(msg.tx_ts);
		fail_on_test(msg.tx_nack_cnt);
		fail_on_test(msg.tx_arb_lost_cnt);
		fail_on_test(msg.tx_low_drive_cnt);
		fail_on_test(msg.tx_error_cnt);
		seq = msg.sequence;

		sleep(1);
		while (true) {
			memset(&msg, 0xff, sizeof(msg));
			msg.timeout = 1500;
			fail_on_test(doioctl(node, CEC_RECEIVE, &msg));
			if (msg.sequence == 0)
				continue;
			fail_on_test(msg.sequence != seq);
			fail_on_test(msg.len != 2);
			fail_on_test(check_0(msg.msg + msg.len, sizeof(msg.msg) - msg.len));
			fail_on_test(msg.msg[0] != ((la << 4) | invalid_remote));
			fail_on_test(msg.msg[1] != CEC_MSG_GIVE_PHYSICAL_ADDR);
			fail_on_test(msg.timeout != 1500);
			fail_on_test(msg.flags);
			fail_on_test(msg.reply != CEC_MSG_REPORT_PHYSICAL_ADDR);
			fail_on_test(!(msg.tx_status & CEC_TX_STATUS_MAX_RETRIES));
			fail_on_test((msg.tx_status & tx_ok_retry_mask) == tx_ok_retry_mask);
			fail_on_test(msg.tx_nack_cnt == 0xff);
			fail_on_test(msg.tx_arb_lost_cnt == 0xff);
			fail_on_test(msg.tx_low_drive_cnt == 0xff);
			fail_on_test(msg.tx_error_cnt == 0xff);
			fail_on_test(msg.rx_ts);
			fail_on_test(msg.rx_status);
			break;
		}

		__u32 mode = CEC_MODE_INITIATOR | CEC_MODE_FOLLOWER;

		fail_on_test(doioctl(node, CEC_S_MODE, &mode));
	}
	if (remote_la < 15) {
		__u32 seq;

		// Send non-blocking non-reply message to valid remote LA
		memset(&msg, 0xff, sizeof(msg));
		msg.msg[0] = (la << 4) | remote_la;
		msg.timeout = 0;
		cec_msg_give_physical_addr(&msg, false);
		fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
		fail_on_test(msg.len != 2);
		fail_on_test(check_0(msg.msg + msg.len, sizeof(msg.msg) - msg.len));
		fail_on_test(msg.msg[0] != ((la << 4) | remote_la));
		fail_on_test(msg.msg[1] != CEC_MSG_GIVE_PHYSICAL_ADDR);
		fail_on_test(msg.timeout);
		fail_on_test(msg.flags);
		fail_on_test(msg.sequence == 0);
		fail_on_test(msg.sequence == ~0U);
		fail_on_test(msg.reply);
		fail_on_test(msg.tx_status);
		fail_on_test(msg.rx_ts);
		fail_on_test(msg.rx_status);
		fail_on_test(msg.tx_ts);
		fail_on_test(msg.tx_nack_cnt);
		fail_on_test(msg.tx_arb_lost_cnt);
		fail_on_test(msg.tx_low_drive_cnt);
		fail_on_test(msg.tx_error_cnt);
		seq = msg.sequence;

		sleep(1);
		while (true) {
			memset(&msg, 0xff, sizeof(msg));
			msg.timeout = 1500;
			fail_on_test(doioctl(node, CEC_RECEIVE, &msg));
			if (msg.sequence == 0)
				continue;
			fail_on_test(msg.sequence != seq);
			fail_on_test(msg.len != 2);
			fail_on_test(check_0(msg.msg + msg.len, sizeof(msg.msg) - msg.len));
			fail_on_test(msg.msg[0] != ((la << 4) | remote_la));
			fail_on_test(msg.msg[1] != CEC_MSG_GIVE_PHYSICAL_ADDR);
			fail_on_test(msg.timeout != 1500);
			fail_on_test(msg.flags);
			fail_on_test(msg.reply);
			fail_on_test(!(msg.tx_status & CEC_TX_STATUS_OK));
			fail_on_test((msg.tx_status & tx_ok_retry_mask) == tx_ok_retry_mask);
			fail_on_test(msg.tx_nack_cnt == 0xff);
			fail_on_test(msg.tx_arb_lost_cnt == 0xff);
			fail_on_test(msg.tx_low_drive_cnt == 0xff);
			fail_on_test(msg.tx_error_cnt == 0xff);
			fail_on_test(msg.rx_ts || msg.rx_status);
			break;
		}

		// Send non-blocking reply message to valid remote LA
		memset(&msg, 0xff, sizeof(msg));
		msg.msg[0] = (la << 4) | remote_la;
		msg.timeout = 0;
		msg.flags &= ~CEC_MSG_FL_REPLY_TO_FOLLOWERS;
		cec_msg_give_physical_addr(&msg, true);
		fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
		fail_on_test(msg.len != 2);
		fail_on_test(check_0(msg.msg + msg.len, sizeof(msg.msg) - msg.len));
		fail_on_test(msg.msg[0] != ((la << 4) | remote_la));
		fail_on_test(msg.msg[1] != CEC_MSG_GIVE_PHYSICAL_ADDR);
		fail_on_test(msg.timeout != 1000);
		fail_on_test(msg.flags);
		fail_on_test(msg.sequence == 0);
		fail_on_test(msg.sequence == ~0U);
		fail_on_test(msg.reply != CEC_MSG_REPORT_PHYSICAL_ADDR);
		fail_on_test(msg.tx_status);
		fail_on_test(msg.rx_ts);
		fail_on_test(msg.rx_status);
		fail_on_test(msg.tx_ts);
		fail_on_test(msg.tx_nack_cnt);
		fail_on_test(msg.tx_arb_lost_cnt);
		fail_on_test(msg.tx_low_drive_cnt);
		fail_on_test(msg.tx_error_cnt);
		seq = msg.sequence;

		sleep(1);
		while (true) {
			memset(&msg, 0xff, sizeof(msg));
			msg.timeout = 1500;
			fail_on_test(doioctl(node, CEC_RECEIVE, &msg));
			if (msg.sequence == 0)
				continue;
			fail_on_test(msg.sequence != seq);
			fail_on_test(msg.len != 5);
			fail_on_test(check_0(msg.msg + msg.len, sizeof(msg.msg) - msg.len));
			fail_on_test(msg.msg[0] != ((remote_la << 4) | 0xf));
			fail_on_test(msg.msg[1] != CEC_MSG_REPORT_PHYSICAL_ADDR);
			fail_on_test(msg.timeout != 1500);
			fail_on_test(msg.flags);
			fail_on_test(msg.reply != CEC_MSG_REPORT_PHYSICAL_ADDR);
			fail_on_test(!(msg.tx_status & CEC_TX_STATUS_OK));
			fail_on_test((msg.tx_status & tx_ok_retry_mask) == tx_ok_retry_mask);
			fail_on_test(msg.tx_nack_cnt == 0xff);
			fail_on_test(msg.tx_arb_lost_cnt == 0xff);
			fail_on_test(msg.tx_low_drive_cnt == 0xff);
			fail_on_test(msg.tx_error_cnt == 0xff);
			fail_on_test(msg.rx_ts == 0);
			fail_on_test(msg.rx_ts == ~0ULL);
			fail_on_test(msg.rx_status != CEC_RX_STATUS_OK);
			fail_on_test(msg.rx_ts < msg.tx_ts);
			break;
		}

		__u32 mode = CEC_MODE_INITIATOR | CEC_MODE_FOLLOWER;

		fail_on_test(doioctl(node, CEC_S_MODE, &mode));
	}
	if (remote_la == 15 || invalid_remote == 15)
		return PRESUMED_OK;
	return 0;
}

int testModes(struct node *node, struct node *node2)
{
	struct cec_msg msg;
	__u8 me = node->log_addr[0];
	__u8 remote = CEC_LOG_ADDR_INVALID;
	__u32 mode = 0, m;

	for (unsigned i = 0; i < 15; i++) {
		if (node->remote_la_mask & (1 << i)) {
			remote = i;
			break;
		}
	}

	cec_msg_init(&msg, me, 0);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
	fail_on_test(doioctl(node2, CEC_TRANSMIT, &msg));

	fail_on_test(doioctl(node, CEC_G_MODE, &m));
	fail_on_test(m != CEC_MODE_INITIATOR);
	fail_on_test(doioctl(node2, CEC_G_MODE, &m));
	fail_on_test(m != CEC_MODE_INITIATOR);

	fail_on_test(doioctl(node, CEC_S_MODE, &mode));
	fail_on_test(doioctl(node, CEC_G_MODE, &m));
	fail_on_test(m != mode);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg) != EBUSY);
	fail_on_test(doioctl(node2, CEC_TRANSMIT, &msg));

	mode = CEC_MODE_INITIATOR;
	fail_on_test(doioctl(node, CEC_S_MODE, &mode));
	fail_on_test(doioctl(node, CEC_G_MODE, &m));
	fail_on_test(m != mode);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));

	mode = CEC_MODE_EXCL_INITIATOR;
	fail_on_test(doioctl(node, CEC_S_MODE, &mode));
	fail_on_test(doioctl(node, CEC_G_MODE, &m));
	fail_on_test(m != mode);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
	fail_on_test(doioctl(node2, CEC_TRANSMIT, &msg) != EBUSY);

	mode = CEC_MODE_INITIATOR;
	fail_on_test(doioctl(node, CEC_S_MODE, &mode));
	fail_on_test(doioctl(node, CEC_G_MODE, &m));
	fail_on_test(m != mode);

	if (remote == CEC_LOG_ADDR_INVALID)
		return 0;

	fail_on_test(flush_pending_msgs(node));
	cec_msg_init(&msg, me, remote);
	cec_msg_give_physical_addr(&msg, false);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
	msg.timeout = 1200;
	fail_on_test(doioctl(node, CEC_RECEIVE, &msg) != ETIMEDOUT);

	mode = CEC_MODE_INITIATOR | CEC_MODE_FOLLOWER;
	fail_on_test(doioctl(node, CEC_S_MODE, &mode));
	fail_on_test(doioctl(node, CEC_G_MODE, &m));
	fail_on_test(m != mode);

	fail_on_test(flush_pending_msgs(node));
	cec_msg_init(&msg, me, remote);
	cec_msg_give_physical_addr(&msg, false);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
	msg.timeout = 1200;
	fail_on_test(doioctl(node, CEC_RECEIVE, &msg));
	fail_on_test(msg.msg[1] != CEC_MSG_REPORT_PHYSICAL_ADDR);

	mode = CEC_MODE_INITIATOR | CEC_MODE_FOLLOWER;
	fail_on_test(doioctl(node2, CEC_S_MODE, &mode));
	fail_on_test(doioctl(node2, CEC_G_MODE, &m));
	fail_on_test(m != mode);

	fail_on_test(flush_pending_msgs(node));
	fail_on_test(flush_pending_msgs(node2));
	cec_msg_init(&msg, me, remote);
	cec_msg_give_physical_addr(&msg, false);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
	msg.timeout = 1200;
	fail_on_test(doioctl(node, CEC_RECEIVE, &msg));
	fail_on_test(msg.msg[1] != CEC_MSG_REPORT_PHYSICAL_ADDR);
	msg.timeout = 1;
	fail_on_test(doioctl(node2, CEC_RECEIVE, &msg));
	fail_on_test(msg.msg[1] != CEC_MSG_REPORT_PHYSICAL_ADDR);

	mode = CEC_MODE_INITIATOR | CEC_MODE_EXCL_FOLLOWER;
	fail_on_test(doioctl(node, CEC_S_MODE, &mode));
	fail_on_test(doioctl(node, CEC_G_MODE, &m));
	fail_on_test(m != mode);

	fail_on_test(flush_pending_msgs(node));
	fail_on_test(flush_pending_msgs(node2));
	cec_msg_init(&msg, me, remote);
	cec_msg_give_physical_addr(&msg, false);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
	msg.timeout = 1200;
	fail_on_test(doioctl(node, CEC_RECEIVE, &msg));
	fail_on_test(msg.msg[1] != CEC_MSG_REPORT_PHYSICAL_ADDR);
	msg.timeout = 1;
	fail_on_test(doioctl(node2, CEC_RECEIVE, &msg) != ETIMEDOUT);

	mode = CEC_MODE_INITIATOR | CEC_MODE_EXCL_FOLLOWER_PASSTHRU;
	fail_on_test(doioctl(node, CEC_S_MODE, &mode));
	fail_on_test(doioctl(node, CEC_G_MODE, &m));
	fail_on_test(m != mode);

	// Note: this test is the same as for CEC_MODE_EXCL_FOLLOWER.
	// Ideally the remote device would send us a passthrough message,
	// but there is no way to trigger that.
	fail_on_test(flush_pending_msgs(node));
	fail_on_test(flush_pending_msgs(node2));
	cec_msg_init(&msg, me, remote);
	cec_msg_give_physical_addr(&msg, false);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
	msg.timeout = 1200;
	fail_on_test(doioctl(node, CEC_RECEIVE, &msg));
	fail_on_test(msg.msg[1] != CEC_MSG_REPORT_PHYSICAL_ADDR);
	msg.timeout = 1;
	fail_on_test(doioctl(node2, CEC_RECEIVE, &msg) != ETIMEDOUT);

	mode = CEC_MODE_INITIATOR;
	fail_on_test(doioctl(node, CEC_S_MODE, &mode));
	fail_on_test(doioctl(node, CEC_G_MODE, &m));
	fail_on_test(m != mode);

	mode = CEC_MODE_INITIATOR | CEC_MODE_MONITOR;
	fail_on_test(doioctl(node2, CEC_S_MODE, &mode) != EINVAL);
	fail_on_test(doioctl(node2, CEC_G_MODE, &m));
	fail_on_test(m != (CEC_MODE_INITIATOR | CEC_MODE_FOLLOWER));

	bool is_root = getuid() == 0;

	mode = CEC_MODE_MONITOR;
	fail_on_test(doioctl(node2, CEC_S_MODE, &mode) != (is_root ? 0 : EPERM));

	if (is_root) {
		fail_on_test(doioctl(node2, CEC_G_MODE, &m));
		fail_on_test(m != mode);
		fail_on_test(flush_pending_msgs(node2));
		cec_msg_init(&msg, me, remote);
		cec_msg_give_physical_addr(&msg, false);
		fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
		msg.timeout = 1200;
		fail_on_test(doioctl(node2, CEC_RECEIVE, &msg));
		fail_on_test(msg.msg[1] != CEC_MSG_GIVE_PHYSICAL_ADDR);
		while (true) {
			fail_on_test(doioctl(node2, CEC_RECEIVE, &msg));
			if (msg.msg[1] == CEC_MSG_REPORT_PHYSICAL_ADDR)
				break;
			fail_on_test(!cec_msg_is_broadcast(&msg) &&
				     !(node2->adap_la_mask & (1 << cec_msg_destination(&msg))));
		}
	}

	mode = CEC_MODE_MONITOR_ALL;
	int res = doioctl(node2, CEC_S_MODE, &mode);
	if (node2->caps & CEC_CAP_MONITOR_ALL)
		fail_on_test(res != (is_root ? 0 : EPERM));
	else
		fail_on_test(res != EINVAL);

	if (is_root && (node2->caps & CEC_CAP_MONITOR_ALL)) {
		fail_on_test(doioctl(node2, CEC_G_MODE, &m));
		fail_on_test(m != mode);
		fail_on_test(flush_pending_msgs(node2));
		cec_msg_init(&msg, me, remote);
		cec_msg_give_physical_addr(&msg, false);
		fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
		msg.timeout = 1200;
		fail_on_test(doioctl(node2, CEC_RECEIVE, &msg));
		fail_on_test(msg.msg[1] != CEC_MSG_GIVE_PHYSICAL_ADDR);
		while (true) {
			fail_on_test(doioctl(node2, CEC_RECEIVE, &msg));
			if (msg.msg[1] == CEC_MSG_REPORT_PHYSICAL_ADDR)
				break;
		}
	}
	return 0;
}

int testLostMsgs(struct node *node)
{
	struct cec_msg msg;
	struct cec_event ev;
	__u8 me = node->log_addr[0];
	__u8 remote = CEC_LOG_ADDR_INVALID;
	__u32 mode = CEC_MODE_INITIATOR | CEC_MODE_FOLLOWER;

	for (unsigned i = 0; i < 15; i++) {
		if (node->remote_la_mask & (1 << i)) {
			remote = i;
			break;
		}
	}

	fail_on_test(flush_pending_msgs(node));
	fcntl(node->fd, F_SETFL, fcntl(node->fd, F_GETFL) | O_NONBLOCK);

	// Become follower for this test, otherwise all the replies to
	// the GET_CEC_VERSION message would have been 'Feature Abort'ed,
	// unless some other follower was running. By becoming a follower
	// all the CEC_VERSION replies are just ignored.
	fail_on_test(doioctl(node, CEC_S_MODE, &mode));
	cec_msg_init(&msg, me, remote);
	cec_msg_get_cec_version(&msg, true);

	// flush pending events
	while (doioctl(node, CEC_DQEVENT, &ev) == 0) { }

	bool got_busy = false;
	unsigned xfer_cnt = 0;
	unsigned tx_queue_depth = 0;

	do {
		int res;

		do {
			res = doioctl(node, CEC_TRANSMIT, &msg);

			fail_on_test(res && res != EBUSY);
			if (!res)
				xfer_cnt++;

			if (res == EBUSY) {
				struct timeval tv = { 0, 10000 }; // 10 ms

				select(0, NULL, NULL, NULL, &tv);
				got_busy = true;
			} else if (!got_busy) {
				tx_queue_depth++;
			}
		} while (res == EBUSY);
		// Alternate between wait for reply and just transmit
		msg.timeout = msg.timeout ? 0 : 1000;
		msg.reply = CEC_MSG_CEC_VERSION;
	} while (doioctl(node, CEC_DQEVENT, &ev));
	fail_on_test(ev.event != CEC_EVENT_LOST_MSGS);
	fail_on_test(!got_busy);

	/*
	 * No more than max 18 transmits can be queued, but one message
	 * might finish transmitting before the queue fills up, so
	 * check for 19 instead.
	 */
	fail_on_test(tx_queue_depth == 0 || tx_queue_depth > 19);

	unsigned pending_msgs = 0;

	fcntl(node->fd, F_SETFL, fcntl(node->fd, F_GETFL) & ~O_NONBLOCK);
	msg.timeout = 3000;

	while (!doioctl(node, CEC_RECEIVE, &msg))
		pending_msgs++;

	/*
	 * Should be at least the size of the internal message queue and
	 * close to the number of transmitted messages.
	 */
	if (pending_msgs < 18 * 3 || pending_msgs > xfer_cnt || pending_msgs < xfer_cnt - 2)
		return fail("There were %d pending messages for %d transmitted messages\n",
			    pending_msgs, xfer_cnt);

	mode = CEC_MODE_INITIATOR;
	fail_on_test(doioctl(node, CEC_S_MODE, &mode));

	return 0;
}

void testAdapter(struct node &node, struct cec_log_addrs &laddrs,
		 const char *device)
{
	/* Required ioctls */

	printf("\nCEC API:\n");
	printf("\tCEC_ADAP_G_CAPS: %s\n", ok(testCap(&node)));
	printf("\tCEC_DQEVENT: %s\n", ok(testDQEvent(&node)));
	printf("\tCEC_ADAP_G/S_PHYS_ADDR: %s\n", ok(testAdapPhysAddr(&node)));
	if (node.caps & CEC_CAP_PHYS_ADDR)
		doioctl(&node, CEC_ADAP_S_PHYS_ADDR, &node.phys_addr);
	if (node.phys_addr == CEC_PHYS_ADDR_INVALID) {
		fprintf(stderr, "FAIL: without a valid physical address this test cannot proceed.\n");
		fprintf(stderr, "Make sure that this CEC adapter is connected to another HDMI sink or source.\n");
		exit(1);
	}
	printf("\tCEC_ADAP_G/S_LOG_ADDRS: %s\n", ok(testAdapLogAddrs(&node)));
	fcntl(node.fd, F_SETFL, fcntl(node.fd, F_GETFL) & ~O_NONBLOCK);
	sleep(1);
	if (node.caps & CEC_CAP_LOG_ADDRS) {
		struct cec_log_addrs clear = { };

		doioctl(&node, CEC_ADAP_S_LOG_ADDRS, &clear);
		doioctl(&node, CEC_ADAP_S_LOG_ADDRS, &laddrs);
	}
	doioctl(&node, CEC_ADAP_G_LOG_ADDRS, &laddrs);
	if (laddrs.log_addr_mask != node.adap_la_mask)
		printf("\tNew Logical Address Mask   : 0x%04x\n", laddrs.log_addr_mask);
	// The LAs may have changed after these tests, so update these node fields
	node.num_log_addrs = laddrs.num_log_addrs;
	memcpy(node.log_addr, laddrs.log_addr, laddrs.num_log_addrs);
	node.adap_la_mask = laddrs.log_addr_mask;

	printf("\tCEC_TRANSMIT: %s\n", ok(testTransmit(&node)));
	printf("\tCEC_RECEIVE: %s\n", ok(testReceive(&node)));
	__u32 mode = CEC_MODE_INITIATOR;
	doioctl(&node, CEC_S_MODE, &mode);
	printf("\tCEC_TRANSMIT/RECEIVE (non-blocking): %s\n", ok(testNonBlocking(&node)));
	fcntl(node.fd, F_SETFL, fcntl(node.fd, F_GETFL) & ~O_NONBLOCK);
	doioctl(&node, CEC_S_MODE, &mode);

	struct node node2 = node;

	if ((node2.fd = open(device, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", device,
			strerror(errno));
		exit(1);
	}

	printf("\tCEC_G/S_MODE: %s\n", ok(testModes(&node, &node2)));
	close(node2.fd);
	doioctl(&node, CEC_S_MODE, &mode);

	printf("\tCEC_EVENT_LOST_MSGS: %s\n", ok(testLostMsgs(&node)));
	fcntl(node.fd, F_SETFL, fcntl(node.fd, F_GETFL) & ~O_NONBLOCK);
}
