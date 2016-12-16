/*
 * Copyright 2016 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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

#include "cec-compliance.h"


/* Dynamic Auto Lipsync */

static int dal_request_current_latency(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_request_current_latency(&msg, true, node->remote[la].phys_addr);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test_v2(node->remote[la].cec_version,
			timed_out(&msg) && is_tv(la, node->remote[la].prim_type));
	if (timed_out(&msg))
		return NOTSUPPORTED;

	/* When the device supports Dynamic Auto Lipsync but does not implement
	   CEC 1.4b, or 2.0, a very strict subset of CEC can be supported. If we
	   get here and the version is < 1.4, we know that the device is not
	   complying to this specification. */
	if (node->remote[la].cec_version < CEC_OP_CEC_VERSION_1_4) {
		warn("CEC 2.0 specifies that devices with CEC version < 1.4 which implement\n");
		warn("Dynamic Auto Lipsync shall only implement a very strict subset of CEC.\n");
	}

	__u16 phys_addr;
	__u8 video_latency, low_latency_mode, audio_out_compensated, audio_out_delay;

	cec_ops_report_current_latency(&msg, &phys_addr, &video_latency, &low_latency_mode,
				       &audio_out_compensated, &audio_out_delay);
	fail_on_test(phys_addr != node->remote[la].phys_addr);
	info("Video latency: %d\n", video_latency);
	info("Low latency mode: %d\n", low_latency_mode);
	info("Audio output compensation: %d\n", audio_out_compensated);
	info("Audio out delay: %d\n", audio_out_delay);

	return 0;
}

static int dal_req_current_latency_invalid(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	/* Test that there is no reply when the physical address operand is not the
	   physical address of the remote device. */
	cec_msg_init(&msg, me, la);
	cec_msg_request_current_latency(&msg, true, CEC_PHYS_ADDR_INVALID);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(!timed_out(&msg));

	return 0;
}

struct remote_subtest dal_subtests[] = {
	{ "Request Current Latency", CEC_LOG_ADDR_MASK_ALL, dal_request_current_latency },
	{ "Request Current Latency with invalid PA", CEC_LOG_ADDR_MASK_ALL, dal_req_current_latency_invalid },
};

const unsigned dal_subtests_size = ARRAY_SIZE(dal_subtests);


/* Audio Return Channel Control */

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

static bool util_receive(struct node *node, unsigned la, struct cec_msg *msg,
			 __u8 sent_msg, __u8 reply1, __u8 reply2 = 0)
{
        while (1) {
		memset(msg, 0, sizeof(*msg));
		msg->timeout = 1;
		if (doioctl(node, CEC_RECEIVE, msg))
			break;
		if (cec_msg_initiator(msg) != la)
			continue;

		if (msg->msg[1] == CEC_MSG_FEATURE_ABORT) {
			__u8 reason, abort_msg;

			cec_ops_feature_abort(msg, &abort_msg, &reason);
			if (abort_msg != sent_msg)
				continue;
			return true;
		}

		if (msg->msg[1] == reply1 || (reply2 && msg->msg[1] == reply2))
			return true;
	}

	return false;
}

static int arc_initiate_tx(struct node *node, unsigned me, unsigned la, bool interactive)
{
	/* Check if we are upstream from the device. If we are, then the device is
	   an HDMI source, which means that it is an ARC receiver, not a transmitter. */
	if (pa_is_upstream_from(node->phys_addr, node->remote[la].phys_addr))
		return NOTAPPLICABLE;

	struct cec_msg msg = {};

	/*
	 * Note that this is a special case: INITIATE_ARC can reply with two possible
	 * messages: CEC_MSG_REPORT_ARC_INITIATED or CEC_MSG_REPORT_ARC_TERMINATED.
	 * It's the only message that behaves like this.
	 */
	cec_msg_init(&msg, me, la);
	cec_msg_initiate_arc(&msg, true);
	fail_on_test(!transmit_timeout(node, &msg));
	if (timed_out(&msg)) {
		fail_on_test_v2(node->remote[la].cec_version, node->remote[la].has_arc_tx);
		warn("Timed out waiting for Report ARC Initiated/Terminated.\n");
		return PRESUMED_OK;
	}
	if (unrecognized_op(&msg)) {
		fail_on_test_v2(node->remote[la].cec_version, node->remote[la].has_arc_tx);
		return NOTSUPPORTED;
	}
	if (cec_msg_opcode(&msg) == CEC_MSG_REPORT_ARC_INITIATED) {
		fail_on_test(!pa_are_adjacent(node->phys_addr, node->remote[la].phys_addr));
		fail_on_test_v2(node->remote[la].cec_version, !node->remote[la].has_arc_tx);
		node->remote[la].arc_initiated = true;
	}
	else if (cec_msg_opcode(&msg) == CEC_MSG_REPORT_ARC_TERMINATED)
		announce("Device supports ARC but is not ready to initiate.");
	else if (refused(&msg))
		return REFUSED;
	else if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static int arc_terminate_tx(struct node *node, unsigned me, unsigned la, bool interactive)
{
	/* Check if we are upstream from the device. If we are, then the device is
	   an HDMI source, which means that it is an ARC receiver, not a transmitter. */
	if (pa_is_upstream_from(node->phys_addr, node->remote[la].phys_addr))
		return NOTAPPLICABLE;
	if (!node->remote[la].arc_initiated)
		return NOTAPPLICABLE;

	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_terminate_arc(&msg, true);
	fail_on_test(!transmit_timeout(node, &msg));
	if (timed_out(&msg)) {
		warn("Timed out waiting for Report ARC Terminated.\n");
		return PRESUMED_OK;
	}
	fail_on_test(unrecognized_op(&msg));
	if (cec_msg_status_is_abort(&msg)) {
		warn("Received Feature Abort for Terminate ARC (but the message was recognized).\n");
		if (refused(&msg))
			return REFUSED;
		return PRESUMED_OK;
	}

	return 0;
}

static int arc_initiate_rx(struct node *node, unsigned me, unsigned la, bool interactive)
{
	/* Check if the DUT is upstream from us. If it is, then it is an
	   HDMI sink, which means that it is an ARC transmitter, not receiver. */
	if (pa_is_upstream_from(node->remote[la].phys_addr, node->phys_addr))
		return NOTAPPLICABLE;

	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_request_arc_initiation(&msg, true);

	bool unsupported = false;

	fail_on_test(!transmit_timeout(node, &msg));
	if (timed_out(&msg) || unrecognized_op(&msg))
		unsupported = true;
	else if (cec_msg_status_is_abort(&msg)) {
		__u8 abort_msg, reason;

		cec_ops_feature_abort(&msg, &abort_msg, &reason);
		if (reason == CEC_OP_ABORT_INCORRECT_MODE) {
			announce("The device supports ARC but is not ready to initiate.");
			return 0;
		}
		else {
			warn("Device responded Feature Abort with unexpected abort reason. Assuming no ARC support.\n");
			unsupported = true;
		}
	}
	if (unsupported) {
		fail_on_test_v2(node->remote[la].cec_version, node->remote[la].has_arc_rx);
		return NOTSUPPORTED;
	}
	fail_on_test(!pa_are_adjacent(node->phys_addr, node->remote[la].phys_addr));
	fail_on_test_v2(node->remote[la].cec_version, !node->remote[la].has_arc_rx);

	cec_msg_init(&msg, me, la);
	cec_msg_report_arc_initiated(&msg);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(unrecognized_op(&msg));
	node->remote[la].arc_initiated = true;

	return 0;
}

static int arc_terminate_rx(struct node *node, unsigned me, unsigned la, bool interactive)
{
	/* Check if the DUT is upstream from us. If it is, then it is an
	   HDMI sink, which means that it is an ARC transmitter, not receiver. */
	if (pa_is_upstream_from(node->remote[la].phys_addr, node->phys_addr))
		return NOTAPPLICABLE;
	if (!node->remote[la].arc_initiated)
		return NOTAPPLICABLE;

	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_request_arc_termination(&msg, true);
	fail_on_test(!transmit_timeout(node, &msg));
	if (timed_out(&msg)) {
		warn("Timed out waiting for Terminate ARC.\n");
		return PRESUMED_OK;
	}
	fail_on_test(unrecognized_op(&msg));
	if (cec_msg_status_is_abort(&msg)) {
		warn("Received Feature Abort for Request ARC Termination (but the message was recognized).\n");
		if (refused(&msg))
			return REFUSED;
		return PRESUMED_OK;
	}

	cec_msg_init(&msg, me, la);
	cec_msg_report_arc_terminated(&msg);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(unrecognized_op(&msg));

	return 0;
}

struct remote_subtest arc_subtests[] = {
	{ "Initiate ARC (RX)", CEC_LOG_ADDR_MASK_ALL, arc_initiate_rx },
	{ "Terminate ARC (RX)", CEC_LOG_ADDR_MASK_ALL, arc_terminate_rx },
	{ "Initiate ARC (TX)", CEC_LOG_ADDR_MASK_ALL, arc_initiate_tx },
	{ "Terminate ARC (TX)", CEC_LOG_ADDR_MASK_ALL, arc_terminate_tx },
};

const unsigned arc_subtests_size = ARRAY_SIZE(arc_subtests);


/* System Audio Control */

static int sac_request_sad_probe(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};
	__u8 audio_format_id = 0;
	__u8 audio_format_code = 1;

	cec_msg_init(&msg, me, la);
	cec_msg_request_short_audio_descriptor(&msg, true, 1, &audio_format_id, &audio_format_code);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;
	node->remote[la].has_sad = true;

	return 0;
}

static int sac_request_sad_invalid(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!node->remote[la].has_sad)
		return NOTAPPLICABLE;

	struct cec_msg msg = {};
	__u8 audio_format_id = CEC_OP_AUD_FMT_ID_CEA861;
	__u8 audio_format_code = 63; // This is outside the range of CEA861-F

	cec_msg_init(&msg, me, la);
	cec_msg_request_short_audio_descriptor(&msg, true, 1, &audio_format_id, &audio_format_code);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	fail_on_test(!cec_msg_status_is_abort(&msg));
	if (abort_reason(&msg) != CEC_OP_ABORT_INVALID_OP) {
		warn("Expected Feature Abort [Invalid operand] in reply to Request Short\n");
		warn("Audio Descriptor with invalid audio format as operand.\n");
	}

	return 0;
}

static int sac_sad_format_check(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!node->remote[la].has_sad)
		return NOTAPPLICABLE;

	struct cec_msg msg = {};
	__u8 audio_format_id;
	__u8 audio_format_code;

	for (unsigned int id = 0; id <= 1; id++) {
		audio_format_id = id;
		for (unsigned int fmt_code = 1; fmt_code <= 14; fmt_code++) {
			audio_format_code = fmt_code;

			cec_msg_init(&msg, me, la);
			cec_msg_request_short_audio_descriptor(&msg, true, 1, &audio_format_id, &audio_format_code);
			fail_on_test(!transmit_timeout(node, &msg));
			fail_on_test(timed_out(&msg));
			fail_on_test(unrecognized_op(&msg) || refused(&msg));

			if (cec_msg_status_is_abort(&msg) &&
			    abort_reason(&msg) == CEC_OP_ABORT_INVALID_OP)
				continue;

			__u8 num_descriptors;
			__u32 descriptors[4] = {};

			cec_ops_report_short_audio_descriptor(&msg, &num_descriptors, descriptors);
			fail_on_test(num_descriptors == 0);
			if (id == 1 && node->remote[la].cec_version < CEC_OP_CEC_VERSION_2_0)
				warn("The device has CEC version < 2.0 but reports audio format(s) introduced in CEC 2.0.\n");

			for (int j = 0; j < num_descriptors; j++) {
				struct short_audio_desc sad;

				sad_decode(&sad, descriptors[j]);
				if ((id == 0 && sad.format_code != fmt_code) ||
				    (id == 1 && sad.extension_type_code != fmt_code))
					return fail("Different audio format code reported than requested.\n");
				info("Supports format %s\n", short_audio_desc2s(sad).c_str());

				/* We need to store the ID and Code for one of the audio formats found,
				   for use in later test(s) */
				node->remote[la].supp_format_id = audio_format_id;
				node->remote[la].supp_format_code = audio_format_code;
			}
		}
	}

	return 0;
}

static int sac_sad_req_multiple(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!node->remote[la].has_sad || node->remote[la].supp_format_code == 0)
		return NOTAPPLICABLE;

	/* Check that if we got a response to a Request Short Audio Descriptor
	   with a single format, we also get a response when the same audio format
	   occurs in a request together with other formats. */
	struct cec_msg msg = {};
	__u8 audio_format_id[4];
	__u8 audio_format_code[4];

	for (int i = 0; i < 4; i++) {
		if (node->remote[la].supp_format_code <= 12)
			audio_format_code[i] = node->remote[la].supp_format_code - 1 + i;
		else
			audio_format_code[i] = node->remote[la].supp_format_code - 1 - i;
	}
	cec_msg_init(&msg, me, la);
	cec_msg_request_short_audio_descriptor(&msg, true, 4, audio_format_id, audio_format_code);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	fail_on_test(cec_msg_status_is_abort(&msg));

	return 0;
}

static int sac_set_system_audio_mode_direct(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_set_system_audio_mode(&msg, CEC_OP_SYS_AUD_STATUS_ON);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test_v2(node->remote[la].cec_version,
			unrecognized_op(&msg) && is_tv(la, node->remote[la].prim_type));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	node->remote[la].has_set_sys_audio_mode = true;

	return PRESUMED_OK;
}

static int sac_set_system_audio_mode_broadcast_on(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, CEC_LOG_ADDR_BROADCAST);
	cec_msg_set_system_audio_mode(&msg, CEC_OP_SYS_AUD_STATUS_ON);
	fail_on_test(!transmit_timeout(node, &msg));

	return PRESUMED_OK;
}

static int sac_system_audio_mode_status(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	/* The device shall not feature abort System Audio Status if it did not
	   feature abort Set System Audio Mode.

	   The message is mandatory for TVs in CEC 2.0. */
	cec_msg_init(&msg, me, la);
	cec_msg_system_audio_mode_status(&msg, CEC_OP_SYS_AUD_STATUS_ON);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test_v2(node->remote[la].cec_version,
			is_tv(la, node->remote[la].prim_type) && unrecognized_op(&msg));
	if (unrecognized_op(&msg) && !node->remote[la].has_set_sys_audio_mode)
		return NOTSUPPORTED;
	fail_on_test(unrecognized_op(&msg));
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static int sac_set_system_audio_mode_broadcast_off(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, CEC_LOG_ADDR_BROADCAST);
	cec_msg_set_system_audio_mode(&msg, CEC_OP_SYS_AUD_STATUS_OFF);
	fail_on_test(!transmit_timeout(node, &msg));

	return PRESUMED_OK;
}

static int sac_system_audio_mode_req_on(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};
	__u8 status;

	/* Send a System Audio Mode Request to the audio system. This notifies the
	   audio system that our device has SAC capabilities, so it should enable
	   the feature right away by sending Set System Audio Mode with On as status.

	   The message is mandatory for audio systems in CEC 2.0. */
	cec_msg_init(&msg, me, la);
	cec_msg_system_audio_mode_request(&msg, true, node->phys_addr);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	fail_on_test_v2(node->remote[la].cec_version,
			cec_has_audiosystem(1 << la) && unrecognized_op(&msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;
	node->remote[la].has_sys_audio_mode_req = true;
	cec_ops_set_system_audio_mode(&msg, &status);
	fail_on_test(status != CEC_OP_SYS_AUD_STATUS_ON);

	return 0;
}

static int sac_give_system_audio_mode_status(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};
	__u8 system_audio_status;

	/* The device shall not feature abort Give System Audio Mode Status if it did not
	   feature abort System Audio Mode Request.

	   The message is mandatory for audio systems in CEC 2.0. */
	cec_msg_init(&msg, me, la);
	cec_msg_give_system_audio_mode_status(&msg, true);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	fail_on_test_v2(node->remote[la].cec_version,
			cec_has_audiosystem(1 << la) && unrecognized_op(&msg));
	if (unrecognized_op(&msg) && !node->remote[la].has_sys_audio_mode_req)
		return NOTSUPPORTED;
	fail_on_test(unrecognized_op(&msg));
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;
	cec_ops_system_audio_mode_status(&msg, &system_audio_status);
	fail_on_test(system_audio_status != CEC_OP_SYS_AUD_STATUS_ON);

	return 0;
}

static int sac_give_audio_status(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	/* Give Audio Status is mandatory for audio systems in CEC 2.0, except
	   for systems that lack external controls for volume/mute status. */
	cec_msg_init(&msg, me, la);
	cec_msg_give_audio_status(&msg, true);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	fail_on_test_v2(node->remote[la].cec_version,
			cec_has_audiosystem(1 << la) && unrecognized_op(&msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	cec_ops_report_audio_status(&msg, &node->remote[la].mute, &node->remote[la].volume);
	fail_on_test(node->remote[la].volume > 100);
	info("Volume: %d %s\n", node->remote[la].volume, node->remote[la].mute ? "(muted)" : "");

	return 0;
}

static int sac_util_send_user_control_press(struct node *node, unsigned me, unsigned la, __u8 ui_cmd)
{
	struct cec_msg msg = {};
	struct cec_op_ui_command rc_press = {};

	/* The device shall not feature abort
	     - User Control Pressed ["Volume Up"]
	     - User Control Pressed ["Volume Down"]
	     - User Control Pressed ["Mute"]
	   if it did not feature abort System Audio Mode Request.

	   The messages are mandatory for audio systems and TVs in CEC 2.0,
	   and it is mandatory for audio systems to send Report Audio Status
	   back to the TV in CEC 2.0.

	   It is recommended for devices to not send Report Audio Status back
	   more often than once every 500ms. We therefore sleep a second before
	   each User Control Pressed is sent. */
	__u32 mode = CEC_MODE_INITIATOR | CEC_MODE_FOLLOWER;
	bool got_response;

	sleep(1);
	doioctl(node, CEC_S_MODE, &mode);
	cec_msg_init(&msg, me, la);
	rc_press.ui_cmd = ui_cmd;
	cec_msg_user_control_pressed(&msg, &rc_press);
	fail_on_test(!transmit(node, &msg));
	cec_msg_init(&msg, me, la);
	cec_msg_user_control_released(&msg);
	fail_on_test(!transmit_timeout(node, &msg));
	got_response = util_receive(node, la, &msg, CEC_MSG_USER_CONTROL_PRESSED, CEC_MSG_REPORT_AUDIO_STATUS);
	mode = CEC_MODE_INITIATOR;
	doioctl(node, CEC_S_MODE, &mode);

	fail_on_test_v2(node->remote[la].cec_version, !got_response &&
			cec_has_audiosystem(1 << la));
	fail_on_test_v2(node->remote[la].cec_version, unrecognized_op(&msg) &&
			(is_tv(la, node->remote[la].prim_type) || cec_has_audiosystem(1 << la)));
	if (unrecognized_op(&msg) && !node->remote[la].has_sys_audio_mode_req)
		return NOTSUPPORTED;
	fail_on_test(unrecognized_op(&msg));
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;
	if (got_response) {
		cec_ops_report_audio_status(&msg, &node->remote[la].mute, &node->remote[la].volume);
		return 0;
	}

	return PRESUMED_OK;
}

static int sac_user_control_press_vol_up(struct node *node, unsigned me, unsigned la, bool interactive)
{
	__u8 ret, old_volume = node->remote[la].volume;

	if ((ret = sac_util_send_user_control_press(node, me, la, 0x41)))
		return ret;
	/* Check that if not already at the highest, the volume was increased. */
	fail_on_test_v2(node->remote[la].cec_version,
			la == CEC_LOG_ADDR_AUDIOSYSTEM &&
			old_volume < 100 && node->remote[la].volume <= old_volume);

	return 0;
}

static int sac_user_control_press_vol_down(struct node *node, unsigned me, unsigned la, bool interactive)
{
	__u8 ret, old_volume = node->remote[la].volume;

	if ((ret = sac_util_send_user_control_press(node, me, la, 0x42)))
		return ret;
	/* Check that if not already at the lowest, the volume was lowered. */
	fail_on_test_v2(node->remote[la].cec_version,
			la == CEC_LOG_ADDR_AUDIOSYSTEM &&
			old_volume > 0 && node->remote[la].volume >= old_volume);

	return 0;
}

static int sac_user_control_press_mute(struct node *node, unsigned me, unsigned la, bool interactive)
{
	__u8 ret, old_mute = node->remote[la].mute;

	if ((ret = sac_util_send_user_control_press(node, me, la, 0x43)))
		return ret;
	/* Check that mute has been toggled from what it was before. */
	fail_on_test_v2(node->remote[la].cec_version,
			la == CEC_LOG_ADDR_AUDIOSYSTEM &&
			node->remote[la].mute == old_mute);

	return 0;
}

static int sac_user_control_release(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	/* The device shall not feature abort User Control Released if it did not
	   feature abort System Audio Mode Request

	   The message is mandatory for audio systems and TVs in CEC 2.0. */
	cec_msg_init(&msg, me, la);
	cec_msg_user_control_released(&msg);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test_v2(node->remote[la].cec_version, unrecognized_op(&msg) &&
			(is_tv(la, node->remote[la].prim_type) || cec_has_audiosystem(1 << la)));
	if (unrecognized_op(&msg) && !node->remote[la].has_sys_audio_mode_req)
		return NOTSUPPORTED;
	fail_on_test(unrecognized_op(&msg));
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return PRESUMED_OK;
}

static int sac_system_audio_mode_req_off(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!node->remote[la].has_sys_audio_mode_req)
		return NOTAPPLICABLE;

	struct cec_msg msg = {};
	__u8 status;

	cec_msg_init(&msg, me, la);
	cec_msg_system_audio_mode_request(&msg, true, CEC_PHYS_ADDR_INVALID);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	fail_on_test_v2(node->remote[la].cec_version,
			cec_has_audiosystem(1 << la) && unrecognized_op(&msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;
	cec_ops_set_system_audio_mode(&msg, &status);
	fail_on_test(status != CEC_OP_SYS_AUD_STATUS_OFF);

	return 0;
}

struct remote_subtest sac_subtests[] = {
	{ "Request Short Audio Descriptor",
	  CEC_LOG_ADDR_MASK_AUDIOSYSTEM,
	  sac_request_sad_probe },
	{ "Request Short Audio Descriptor, invalid",
	  CEC_LOG_ADDR_MASK_AUDIOSYSTEM,
	  sac_request_sad_invalid },
	{ "Report Short Audio Descriptor consistency",
	  CEC_LOG_ADDR_MASK_AUDIOSYSTEM,
	  sac_sad_format_check },
	{ "Report Short Audio Descriptor, multiple requests in one",
	  CEC_LOG_ADDR_MASK_AUDIOSYSTEM,
	  sac_sad_req_multiple },
	{ "Set System Audio Mode (directly addressed)",
	  CEC_LOG_ADDR_MASK_TV,
	  sac_set_system_audio_mode_direct },
	{ "Set System Audio Mode (broadcast on)",
	  CEC_LOG_ADDR_MASK_TV,
	  sac_set_system_audio_mode_broadcast_on },
	{ "System Audio Mode Status",
	  CEC_LOG_ADDR_MASK_TV,
	  sac_system_audio_mode_status },
	{ "System Audio Mode Request (on)",
	  CEC_LOG_ADDR_MASK_AUDIOSYSTEM,
	  sac_system_audio_mode_req_on },
	{ "Give System Audio Mode Status",
	  CEC_LOG_ADDR_MASK_AUDIOSYSTEM,
	  sac_give_system_audio_mode_status },
	{ "Give Audio Status",
	  CEC_LOG_ADDR_MASK_AUDIOSYSTEM,
	  sac_give_audio_status },
	{ "User Control Pressed (Volume Up)",
	  CEC_LOG_ADDR_MASK_AUDIOSYSTEM | CEC_LOG_ADDR_MASK_TV,
	  sac_user_control_press_vol_up },
	{ "User Control Pressed (Volume Down)",
	  CEC_LOG_ADDR_MASK_AUDIOSYSTEM | CEC_LOG_ADDR_MASK_TV,
	  sac_user_control_press_vol_down },
	{ "User Control Pressed (Mute)",
	  CEC_LOG_ADDR_MASK_AUDIOSYSTEM | CEC_LOG_ADDR_MASK_TV,
	  sac_user_control_press_mute },
	{ "User Control Released",
	  CEC_LOG_ADDR_MASK_AUDIOSYSTEM | CEC_LOG_ADDR_MASK_TV,
	  sac_user_control_release },
	{ "Set System Audio Mode (broadcast off)",
	  CEC_LOG_ADDR_MASK_TV,
	  sac_set_system_audio_mode_broadcast_off },
	{ "System Audio Mode Request (off)",
	  CEC_LOG_ADDR_MASK_AUDIOSYSTEM,
	  sac_system_audio_mode_req_off },
};

const unsigned sac_subtests_size = ARRAY_SIZE(sac_subtests);


/* Audio Rate Control */

/*
  TODO: These are very rudimentary tests which should be expanded.
 */

static int audio_rate_ctl_set_audio_rate(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_set_audio_rate(&msg, CEC_OP_AUD_RATE_WIDE_STD);
	fail_on_test(!transmit_timeout(node, &msg));
	/* CEC 2.0: Devices shall use the device feature bit to indicate support. */
	fail_on_test_v2(node->remote[la].cec_version,
			node->remote[la].has_aud_rate && unrecognized_op(&msg));
	fail_on_test_v2(node->remote[la].cec_version,
			!node->remote[la].has_aud_rate && !unrecognized_op(&msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;

	return PRESUMED_OK;
}

struct remote_subtest audio_rate_ctl_subtests[] = {
	{ "Set Audio Rate",
	  CEC_LOG_ADDR_MASK_PLAYBACK | CEC_LOG_ADDR_MASK_RECORD | CEC_LOG_ADDR_MASK_TUNER,
	  audio_rate_ctl_set_audio_rate },
};

const unsigned audio_rate_ctl_subtests_size = ARRAY_SIZE(audio_rate_ctl_subtests);
