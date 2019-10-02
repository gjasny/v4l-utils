// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
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
#include <sstream>

#include "cec-compliance.h"

int testFuzzing(struct node &node, unsigned me, unsigned la)
{
	printf("test fuzzing CEC local LA %d (%s) to remote LA %d (%s):\n\n",
	       me, cec_la2s(me), la, cec_la2s(la));

	if (node.remote[la].in_standby) {
		announce("The remote device is in standby. It should be powered on when fuzzing. Aborting.");
		return 0;
	}
	if (!node.remote[la].has_power_status) {
		announce("The device didn't support Give Device Power Status.");
		announce("Assuming that the device is powered on.");
	}

	unsigned int cnt = 0;

	for (;;) {
		cec_msg msg;
		__u8 cmd;
		unsigned offset = 2;

		cec_msg_init(&msg, me, la);
		msg.msg[1] = cmd = random() & 0xff;
		if (msg.msg[1] == CEC_MSG_STANDBY)
			continue;
		msg.len = (random() & 0xf) + 2;
		if (msg.msg[1] == CEC_MSG_VENDOR_COMMAND_WITH_ID &&
		    node.remote[la].vendor_id != CEC_VENDOR_ID_NONE) {
			msg.len += 3;
			offset += 3;
			msg.msg[2] = (node.remote[la].vendor_id & 0xff0000) >> 16;
			msg.msg[3] = (node.remote[la].vendor_id & 0xff00) >> 8;
			msg.msg[4] = node.remote[la].vendor_id & 0xff;
		}
		if (msg.len > CEC_MAX_MSG_SIZE)
			continue;

		const char *name = cec_opcode2s(msg.msg[1]);

		printf("Send message %u:", cnt);
		for (unsigned int i = 0; i < offset; i++)
			printf(" %02x", msg.msg[i]);
		for (unsigned int i = offset; i < msg.len; i++) {
			msg.msg[i] = random() & 0xff;
			printf(" %02x", msg.msg[i]);
		}
		if (name)
			printf(" (%s)", name);
		printf(": ");
		msg.reply = CEC_MSG_FEATURE_ABORT;
		fail_on_test(!transmit_timeout(&node, &msg, 1200));
		printf("%s", timed_out(&msg) ? "Timed out" : "Feature Abort");

		if (cec_msg_status_is_abort(&msg)) {
			__u8 abort_msg, reason;

			cec_ops_feature_abort(&msg, &abort_msg, &reason);
			fail_on_test(abort_msg != cmd);
			switch (reason) {
			case CEC_OP_ABORT_UNRECOGNIZED_OP:
				printf(" (Unrecognized Op)");
				break;
			case CEC_OP_ABORT_UNDETERMINED:
				printf(" (Undetermined)");
				break;
			case CEC_OP_ABORT_INVALID_OP:
				printf(" (Invalid Op)");
				break;
			case CEC_OP_ABORT_NO_SOURCE:
				printf(" (No Source)");
				break;
			case CEC_OP_ABORT_REFUSED:
				printf(" (Refused)");
				break;
			case CEC_OP_ABORT_INCORRECT_MODE:
				printf(" (Incorrect Mode)");
				break;
			default:
				printf(" (0x%02x)\n", reason);
				fail("Invalid reason\n");
				break;
			}
		}
		printf("\n");
		if (++cnt % 10)
			continue;
		if (la == CEC_LOG_ADDR_BROADCAST)
			continue;
		cec_msg_init(&msg, me, la);
		cec_msg_get_cec_version(&msg, true);
		printf("Query CEC Version: ");
		fail_on_test(!transmit_timeout(&node, &msg) || timed_out_or_abort(&msg));
		printf("OK\n");
	}
}
