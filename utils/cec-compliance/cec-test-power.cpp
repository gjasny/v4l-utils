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


static bool get_power_status(struct node *node, unsigned me, unsigned la, __u8 &power_status)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_give_device_power_status(&msg, true);
	if (!transmit_timeout(node, &msg) || timed_out_or_abort(&msg))
		return false;
	cec_ops_report_power_status(&msg, &power_status);
	return true;
}

bool util_interactive_ensure_power_state(struct node *node, unsigned me, unsigned la, bool interactive,
						__u8 target_pwr)
{
	interactive_info(true, "Please ensure that the device is in state %s.",
			 power_status2s(target_pwr));

	if (!node->remote[la].has_power_status)
		return true;

	while (interactive) {
		__u8 pwr;

		if (!get_power_status(node, me, la, pwr))
			announce("Failed to retrieve power status.");
		else if (pwr == target_pwr)
			return true;
		else
			announce("The device reported power status %s.", power_status2s(pwr));
		if (!question("Retry?"))
			return false;
	}

	return true;
}


/* Give Device Power Status */

static int power_status_give(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = { };

	cec_msg_init(&msg, me, la);
	cec_msg_give_device_power_status(&msg, true);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	fail_on_test(unrecognized_op(&msg));
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	__u8 power_status;
	cec_ops_report_power_status(&msg, &power_status);
	fail_on_test(power_status >= 4);

	return 0;
}

static int power_status_report(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_report_power_status(&msg, CEC_OP_POWER_STATUS_ON);
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;

	return PRESUMED_OK;
}

struct remote_subtest power_status_subtests[] = {
	{ "Give Device Power Status", CEC_LOG_ADDR_MASK_ALL, power_status_give },
	{ "Report Device Power Status", CEC_LOG_ADDR_MASK_ALL, power_status_report },
};

const unsigned power_status_subtests_size = ARRAY_SIZE(power_status_subtests);


/* One Touch Play */

static int one_touch_play_view_on(struct node *node, unsigned me, unsigned la, bool interactive,
				  __u8 opcode)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	if (opcode == CEC_MSG_IMAGE_VIEW_ON)
		cec_msg_image_view_on(&msg);
	else if (opcode == CEC_MSG_TEXT_VIEW_ON)
		cec_msg_text_view_on(&msg);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(is_tv(la, node->remote[la].prim_type) && unrecognized_op(&msg));
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;
	if (opcode == CEC_MSG_IMAGE_VIEW_ON)
		node->remote[la].has_image_view_on = true;
	else if (opcode == CEC_MSG_TEXT_VIEW_ON)
		node->remote[la].has_text_view_on = true;

	return 0;
}

static int one_touch_play_image_view_on(struct node *node, unsigned me, unsigned la, bool interactive)
{
	return one_touch_play_view_on(node, me, la, interactive, CEC_MSG_IMAGE_VIEW_ON);
}

static int one_touch_play_text_view_on(struct node *node, unsigned me, unsigned la, bool interactive)
{
	return one_touch_play_view_on(node, me, la, interactive, CEC_MSG_TEXT_VIEW_ON);
}

static int one_touch_play_view_on_wakeup(struct node *node, unsigned me, unsigned la, bool interactive,
					 __u8 opcode)
{
	fail_on_test(!util_interactive_ensure_power_state(node, me, la, interactive, CEC_OP_POWER_STATUS_STANDBY));

	int ret = one_touch_play_view_on(node, me, la, interactive, opcode);

	if (ret && ret != PRESUMED_OK)
		return ret;
	fail_on_test(interactive && !question("Did the TV turn on?"));

	if (interactive)
		return 0;
	else
		return PRESUMED_OK;
}

static int one_touch_play_image_view_on_wakeup(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!interactive || !node->remote[la].has_image_view_on)
		return NOTAPPLICABLE;
	return one_touch_play_view_on_wakeup(node, me, la, interactive, CEC_MSG_IMAGE_VIEW_ON);
}

static int one_touch_play_text_view_on_wakeup(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!interactive || !node->remote[la].has_text_view_on)
		return NOTAPPLICABLE;
	return one_touch_play_view_on_wakeup(node, me, la, interactive, CEC_MSG_TEXT_VIEW_ON);
}

static int one_touch_play_view_on_change(struct node *node, unsigned me, unsigned la, bool interactive,
					 __u8 opcode)
{
	struct cec_msg msg = {};
	int ret;

	fail_on_test(!util_interactive_ensure_power_state(node, me, la, interactive, CEC_OP_POWER_STATUS_ON));

	interactive_info(true, "Please switch the TV to another source.");
	ret = one_touch_play_view_on(node, me, la, interactive, opcode);
	if (ret && ret != PRESUMED_OK)
		return ret;
	cec_msg_init(&msg, me, la);
	cec_msg_active_source(&msg, node->phys_addr);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(interactive && !question("Did the TV switch to this source?"));

	if (interactive)
		return 0;
	else
		return PRESUMED_OK;
}

static int one_touch_play_image_view_on_change(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!interactive || !node->remote[la].has_text_view_on)
		return NOTAPPLICABLE;
	return one_touch_play_view_on_change(node, me, la, interactive, CEC_MSG_IMAGE_VIEW_ON);
}

static int one_touch_play_text_view_on_change(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!interactive || !node->remote[la].has_text_view_on)
		return NOTAPPLICABLE;
	return one_touch_play_view_on_change(node, me, la, interactive, CEC_MSG_TEXT_VIEW_ON);
}

static int one_touch_play_req_active_source(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	/* We have now said that we are active source, so receiving a reply to
	   Request Active Source should fail the test. */
	cec_msg_init(&msg, me, la);
	cec_msg_request_active_source(&msg, true);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(!timed_out(&msg));

	return 0;
}

struct remote_subtest one_touch_play_subtests[] = {
	{ "Image View On", CEC_LOG_ADDR_MASK_TV, one_touch_play_image_view_on },
	{ "Text View On", CEC_LOG_ADDR_MASK_TV, one_touch_play_text_view_on },
	{ "Wakeup on Image View On", CEC_LOG_ADDR_MASK_TV, one_touch_play_image_view_on_wakeup },
	{ "Wakeup Text View On", CEC_LOG_ADDR_MASK_TV, one_touch_play_text_view_on_wakeup },
	{ "Input change on Image View On", CEC_LOG_ADDR_MASK_TV, one_touch_play_image_view_on_change },
	{ "Input change on Text View On", CEC_LOG_ADDR_MASK_TV, one_touch_play_text_view_on_change },
	{ "Request Active Source", (__u16)~CEC_LOG_ADDR_MASK_TV, one_touch_play_req_active_source },
};

const unsigned one_touch_play_subtests_size = ARRAY_SIZE(one_touch_play_subtests);


/* Standby / Resume */

/* The default sleep time between power status requests. */
#define SLEEP_POLL_POWER_STATUS 2

static bool wait_changing_power_status(struct node *node, unsigned me, unsigned la, __u8 &new_status,
				       unsigned &unresponsive_time)
{
	__u8 old_status;

	announce("Checking for power status change. This may take up to 60 s.");
	if (!get_power_status(node, me, la, old_status))
		return false;
	for (unsigned i = 0; i < 60 / SLEEP_POLL_POWER_STATUS; i++) {
		__u8 power_status;

		if (!get_power_status(node, me, la, power_status)) {
			/* Some TVs become completely unresponsive when transitioning
			   between power modes. Register that this happens, but continue
			   the test. */
			unresponsive_time = i * SLEEP_POLL_POWER_STATUS;
		} else if (old_status != power_status) {
			new_status = power_status;
			return true;
		}
		sleep(SLEEP_POLL_POWER_STATUS);
	}
	new_status = old_status;
	return false;
}

static bool poll_stable_power_status(struct node *node, unsigned me, unsigned la,
				     __u8 expected_status, unsigned &unresponsive_time)
{
	bool transient = false;
	unsigned time_to_transient = 0;

	/* Some devices can use several seconds to transition from one power
	   state to another, so the power state must be repeatedly polled */
	announce("Waiting for new stable power status. This may take up to 60 s.");
	for (unsigned tries = 0; tries < 60 / SLEEP_POLL_POWER_STATUS; tries++) {
		__u8 power_status;

		if (!get_power_status(node, me, la, power_status)) {
			/* Some TVs become completely unresponsive when transitioning
			   between power modes. Register that this happens, but continue
			   the test. */
			unresponsive_time = tries * SLEEP_POLL_POWER_STATUS;
		}
		if (!transient && (power_status == CEC_OP_POWER_STATUS_TO_ON ||
				   power_status == CEC_OP_POWER_STATUS_TO_STANDBY)) {
			time_to_transient = tries * SLEEP_POLL_POWER_STATUS;
			transient = true;
		}
		if (power_status == expected_status) {
			announce("Transient state after %d s, stable state %s after %d s",
			     time_to_transient, power_status2s(power_status), tries * SLEEP_POLL_POWER_STATUS);
			return true;
		}
		sleep(SLEEP_POLL_POWER_STATUS);
	}
	return false;
}

static int standby_resume_standby(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!node->remote[la].has_power_status)
		return NOTAPPLICABLE;

	struct cec_msg msg = {};
	unsigned unresponsive_time = 0;

	fail_on_test(!util_interactive_ensure_power_state(node, me, la, interactive, CEC_OP_POWER_STATUS_ON));

	announce("Sending Standby message.");
	cec_msg_init(&msg, me, la);
	cec_msg_standby(&msg);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(cec_msg_status_is_abort(&msg));
	fail_on_test(!poll_stable_power_status(node, me, la, CEC_OP_POWER_STATUS_STANDBY, unresponsive_time));
	fail_on_test(interactive && !question("Is the device in standby?"));
	node->remote[la].in_standby = true;

	if (unresponsive_time > 0)
		warn("The device went correctly into standby, but became unresponsive for %d s during the transition.\n",
		     unresponsive_time);

	return 0;
}

static int standby_resume_standby_toggle(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!node->remote[la].in_standby)
		return NOTAPPLICABLE;

	struct cec_msg msg = {};
	unsigned unresponsive_time = 0;
	__u8 new_status;

	node->remote[la].in_standby = false;

	/* Send Standby again to test that it is not acting like a toggle */
	announce("Sending Standby message.");
	cec_msg_init(&msg, me, la);
	cec_msg_standby(&msg);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(cec_msg_status_is_abort(&msg));
	fail_on_test(wait_changing_power_status(node, me, la, new_status, unresponsive_time));
	fail_on_test(new_status != CEC_OP_POWER_STATUS_STANDBY);
	fail_on_test(interactive && !question("Is the device still in standby?"));
	node->remote[la].in_standby = true;
	if (unresponsive_time > 0)
		warn("The device went correctly into standby, but became unresponsive for %d s during the transition.\n",
		     unresponsive_time);

	return 0;
}

static int standby_resume_active_source_nowake(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!node->remote[la].in_standby)
		return NOTAPPLICABLE;

	struct cec_msg msg = {};
	unsigned unresponsive_time = 0;
	__u8 new_status;

	node->remote[la].in_standby = false;

	/* In CEC 2.0 it is specified that a device shall not go out of standby
	   if an Active Source message is received. */
	announce("Sending Active Source message.");
	cec_msg_init(&msg, me, la);
	cec_msg_active_source(&msg, node->phys_addr);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(wait_changing_power_status(node, me, la, new_status, unresponsive_time));
	fail_on_test_v2_warn(node->remote[la].cec_version, new_status != CEC_OP_POWER_STATUS_STANDBY);
	node->remote[la].in_standby = true;
	if (unresponsive_time > 0)
		warn("The device stayed correctly in standby, but became unresponsive for %d s.\n",
		     unresponsive_time);

	return 0;
}

static int wakeup_rc(struct node *node, unsigned me, unsigned la)
{
	struct cec_msg msg = {};
	struct cec_op_ui_command rc_press = {};

	/* Todo: A release should be sent after this */
	cec_msg_init(&msg, me, la);
	rc_press.ui_cmd = 0x6D;  /* Power On Function */
	cec_msg_user_control_pressed(&msg, &rc_press);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(cec_msg_status_is_abort(&msg));

	return 0;
}

static int wakeup_tv(struct node *node, unsigned me, unsigned la)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_image_view_on(&msg);
	fail_on_test(!transmit_timeout(node, &msg));
	if (!cec_msg_status_is_abort(&msg))
		return 0;

	cec_msg_init(&msg, me, la);
	cec_msg_text_view_on(&msg);
	fail_on_test(!transmit_timeout(node, &msg));
	if (!cec_msg_status_is_abort(&msg))
		return 0;

	return wakeup_rc(node, me, la);
}

static int wakeup_source(struct node *node, unsigned me, unsigned la)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_set_stream_path(&msg, node->remote[la].phys_addr);
	fail_on_test(!transmit_timeout(node, &msg));
	if (!cec_msg_status_is_abort(&msg))
		return 0;

	return wakeup_rc(node, me, la);
}

static int standby_resume_wakeup(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!node->remote[la].in_standby)
		return NOTAPPLICABLE;

	int ret;

	if (is_tv(la, node->remote[la].prim_type))
		ret = wakeup_tv(node, me, la);
	else
		ret = wakeup_source(node, me, la);
	if (ret)
		return ret;

	unsigned unresponsive_time = 0;

	announce("Device is woken up");
	fail_on_test(!poll_stable_power_status(node, me, la, CEC_OP_POWER_STATUS_ON, unresponsive_time));
	fail_on_test(interactive && !question("Is the device in On state?"));

	if (unresponsive_time > 0)
		warn("The device went correctly out of standby, but became unresponsive for %d s during the transition.\n",
		     unresponsive_time);

	return 0;
}

struct remote_subtest standby_resume_subtests[] = {
	{ "Standby", CEC_LOG_ADDR_MASK_ALL, standby_resume_standby },
	{ "Repeated Standby message does not wake up", CEC_LOG_ADDR_MASK_ALL, standby_resume_standby_toggle },
	{ "No wakeup on Active Source", CEC_LOG_ADDR_MASK_ALL, standby_resume_active_source_nowake },
	{ "Wake up", CEC_LOG_ADDR_MASK_ALL, standby_resume_wakeup },
};

const unsigned standby_resume_subtests_size = ARRAY_SIZE(standby_resume_subtests);
