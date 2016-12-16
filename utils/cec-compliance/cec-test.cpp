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
#include <sstream>

#include "cec-compliance.h"

#define test_case(name, tags, subtests) {name, tags, subtests, ARRAY_SIZE(subtests)}
#define test_case_ext(name, tags, subtests) {name, tags, subtests, subtests##_size}

struct remote_test {
	const char *name;
	const unsigned tags;
	struct remote_subtest *subtests;
	unsigned num_subtests;
};


/* System Information */

static int system_info_polling(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = { };

	cec_msg_init(&msg, 0xf, la);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
	if (node->remote_la_mask & (1 << la)) {
		if (!cec_msg_status_is_ok(&msg))
			return FAIL_CRITICAL;
	} else {
		if (cec_msg_status_is_ok(&msg))
			return FAIL_CRITICAL;
		return NOTSUPPORTED;
	}

	return 0;
}

static int system_info_phys_addr(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = { };

	cec_msg_init(&msg, me, la);
	cec_msg_give_physical_addr(&msg, true);
	if (!transmit_timeout(node, &msg) || timed_out_or_abort(&msg))
		return FAIL_CRITICAL;

	return 0;
}

static int system_info_version(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_get_cec_version(&msg, true);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;

	/* This needs to be kept in sync with newer CEC versions */
	fail_on_test(msg.msg[2] < CEC_OP_CEC_VERSION_1_3A ||
		     msg.msg[2] > CEC_OP_CEC_VERSION_2_0);

	return 0;
}

static int system_info_get_menu_lang(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};
	char language[4];

	cec_msg_init(&msg, me, la);
	cec_msg_get_menu_language(&msg, true);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));

	/* Devices other than TVs shall send Feature Abort [Unregcognized Opcode]
	   in reply to Get Menu Language. */
	fail_on_test(!is_tv(la, node->remote[la].prim_type) && !unrecognized_op(&msg));

	if (unrecognized_op(&msg)) {
		if (is_tv(la, node->remote[la].prim_type))
			warn("TV did not respond to Get Menu Language.\n");
		return NOTSUPPORTED;
	}
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;
	cec_ops_set_menu_language(&msg, language);
	language[3] = 0;

	return 0;
}

static int system_info_set_menu_lang(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_set_menu_language(&msg, "eng");
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;

	return PRESUMED_OK;
}

static int system_info_give_features(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = { };

	cec_msg_init(&msg, me, la);
	cec_msg_give_features(&msg, true);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	if (unrecognized_op(&msg)) {
		if (node->remote[la].cec_version < CEC_OP_CEC_VERSION_2_0)
			return NOTSUPPORTED;
		fail_on_test_v2(node->remote[la].cec_version, true);
	}
	if (refused(&msg))
		return REFUSED;
	if (node->remote[la].cec_version < CEC_OP_CEC_VERSION_2_0)
		info("Device has CEC Version < 2.0 but supports Give Features.\n");

	/* RC Profile and Device Features are assumed to be 1 byte. As of CEC 2.0 only
	   1 byte is used, but this might be extended in future versions. */
	__u8 cec_version, all_device_types;
	const __u8 *rc_profile, *dev_features;

	cec_ops_report_features(&msg, &cec_version, &all_device_types, &rc_profile, &dev_features);
	fail_on_test(cec_version != node->remote[la].cec_version);
	fail_on_test(rc_profile == NULL || dev_features == NULL);
	info("All Device Types: \t\t%s\n", all_dev_types2s(all_device_types).c_str());
	info("RC Profile: \t%s", rc_src_prof2s(*rc_profile).c_str());
	info("Device Features: \t%s", dev_feat2s(*dev_features).c_str());

	if (!(cec_has_playback(1 << la) || cec_has_record(1 << la) || cec_has_tuner(1 << la)) &&
		node->remote[la].has_aud_rate) {
		return fail("Only Playback, Recording or Tuner devices shall set the Set Audio Rate bit\n");
	}
	if (!(cec_has_playback(1 << la) || cec_has_record(1 << la)) && node->remote[la].has_deck_ctl)
		return fail("Only Playback and Recording devices shall set the Supports Deck Control bit\n");
	if (!cec_has_tv(1 << la) && node->remote[la].has_rec_tv)
		return fail("Only TVs shall set the Record TV Screen bit\n");

	return 0;
}

static struct remote_subtest system_info_subtests[] = {
	{ "Polling Message", CEC_LOG_ADDR_MASK_ALL, system_info_polling },
	{ "Give Physical Address", CEC_LOG_ADDR_MASK_ALL, system_info_phys_addr },
	{ "Give CEC Version", CEC_LOG_ADDR_MASK_ALL, system_info_version },
	{ "Get Menu Language", CEC_LOG_ADDR_MASK_ALL, system_info_get_menu_lang },
	{ "Set Menu Language", CEC_LOG_ADDR_MASK_ALL, system_info_set_menu_lang },
	{ "Give Device Features", CEC_LOG_ADDR_MASK_ALL, system_info_give_features },
};


/* Core behavior */

static int core_unknown(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = { };

	/* Unknown opcodes should be responded to with Feature Abort, with abort
	   reason Unknown Opcode.

	   For CEC 2.0 and before, 0xfe is an unused opcode. The test possibly
	   needs to be updated for future CEC versions. */
	cec_msg_init(&msg, me, la);
	msg.len = 2;
	msg.msg[1] = 0xfe;
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	fail_on_test(!cec_msg_status_is_abort(&msg));

	__u8 abort_msg, reason;

	cec_ops_feature_abort(&msg, &abort_msg, &reason);
	fail_on_test(reason != CEC_OP_ABORT_UNRECOGNIZED_OP);
	fail_on_test(abort_msg != 0xfe);
	return 0;
}

static int core_abort(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	/* The Abort message should always be responded to with Feature Abort
	   (with any abort reason) */
	cec_msg_init(&msg, me, la);
	cec_msg_abort(&msg);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	fail_on_test(!cec_msg_status_is_abort(&msg));
	return 0;
}

static struct remote_subtest core_subtests[] = {
	{ "Feature aborts unknown messages", CEC_LOG_ADDR_MASK_ALL, core_unknown },
	{ "Feature aborts Abort message", CEC_LOG_ADDR_MASK_ALL, core_abort },
};


/* Vendor Specific Commands */

static int vendor_specific_commands_id(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_give_device_vendor_id(&msg, true);
	fail_on_test(!transmit(node, &msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static struct remote_subtest vendor_specific_subtests[] = {
	{ "Give Device Vendor ID", CEC_LOG_ADDR_MASK_ALL, vendor_specific_commands_id },
};


/* Device OSD Transfer */

static int device_osd_transfer_set(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = { };

	cec_msg_init(&msg, me, la);
	cec_msg_set_osd_name(&msg, "Whatever");
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg)) {
		if (is_tv(la, node->remote[la].prim_type) &&
		    node->remote[la].cec_version >= CEC_OP_CEC_VERSION_2_0)
			warn("TV feature aborted Set OSD Name\n");
		return NOTSUPPORTED;
	}
	if (refused(&msg))
		return REFUSED;

	return PRESUMED_OK;
}

static int device_osd_transfer_give(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = { };

	/* Todo: CEC 2.0: devices with several logical addresses shall report
	   the same for each logical address. */
	cec_msg_init(&msg, me, la);
	cec_msg_give_osd_name(&msg, true);
	fail_on_test(!transmit(node, &msg));
	fail_on_test(timed_out(&msg));
	fail_on_test(!is_tv(la, node->remote[la].prim_type) && unrecognized_op(&msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static struct remote_subtest device_osd_transfer_subtests[] = {
	{ "Set OSD Name", CEC_LOG_ADDR_MASK_TV, device_osd_transfer_set },
	{ "Give OSD Name", CEC_LOG_ADDR_MASK_ALL, device_osd_transfer_give },
};


/* OSD Display */

static int osd_string_set_default(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = { };
	char osd[14];
	bool unsuitable = false;

	sprintf(osd, "Rept %x from %x", la, me);

	interactive_info(true, "You should see \"%s\" appear on the screen", osd);
	cec_msg_init(&msg, me, la);
	cec_msg_set_osd_string(&msg, CEC_OP_DISP_CTL_DEFAULT, osd);
	fail_on_test(!transmit_timeout(node, &msg));
	/* In CEC 2.0 it is mandatory for a TV to support this if it reports so
	   in its Device Features. */
	fail_on_test_v2(node->remote[la].cec_version,
			unrecognized_op(&msg) &&
			(node->remote[la].dev_features & CEC_OP_FEAT_DEV_HAS_SET_OSD_STRING));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	else if (refused(&msg))
		return REFUSED;
	else {
		warn("The device is in an unsuitable state or cannot display the complete message.\n");
		unsuitable = true;
	}
	node->remote[la].has_osd = true;
	if (!interactive)
		return PRESUMED_OK;

	/* The CEC 1.4b CTS specifies that one should wait at least 20 seconds for the
	   string to be cleared on the remote device */
	interactive_info(true, "Waiting 20s for OSD string to be cleared on the remote device");
	sleep(20);
	fail_on_test(!unsuitable && interactive && !question("Did the string appear and then disappear?"));

	return 0;
}

static int osd_string_set_until_clear(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!node->remote[la].has_osd)
		return NOTAPPLICABLE;

	struct cec_msg msg = { };
	char osd[14];
	bool unsuitable = false;

	strcpy(osd, "Appears 1 sec");
	// Make sure the string is the maximum possible length
	fail_on_test(strlen(osd) != 13);

	interactive_info(true, "You should see \"%s\" appear on the screen for approximately three seconds.", osd);
	cec_msg_init(&msg, me, la);
	cec_msg_set_osd_string(&msg, CEC_OP_DISP_CTL_UNTIL_CLEARED, osd);
	fail_on_test(!transmit(node, &msg));
	if (cec_msg_status_is_abort(&msg) && !unrecognized_op(&msg)) {
		warn("The device is in an unsuitable state or cannot display the complete message.\n");
		unsuitable = true;
	}
	sleep(3);

	cec_msg_init(&msg, me, la);
	cec_msg_set_osd_string(&msg, CEC_OP_DISP_CTL_CLEAR, "");
	fail_on_test(!transmit_timeout(node, &msg, 250));
	fail_on_test(cec_msg_status_is_abort(&msg));
	fail_on_test(!unsuitable && interactive && !question("Did the string appear?"));

	if (interactive)
		return 0;
	else
		return PRESUMED_OK;
}

static int osd_string_invalid(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!node->remote[la].has_osd)
		return NOTAPPLICABLE;

	struct cec_msg msg = { };

	/* Send Set OSD String with an Display Control operand. A Feature Abort is
	   expected in reply. */
	interactive_info(true, "You should observe no change on the on screen display");
	cec_msg_init(&msg, me, la);
	cec_msg_set_osd_string(&msg, 0xff, "");
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	fail_on_test(!cec_msg_status_is_abort(&msg));
	fail_on_test(interactive && question("Did the display change?"));

	return 0;
}

static struct remote_subtest osd_string_subtests[] = {
	{ "Set OSD String with default timeout", CEC_LOG_ADDR_MASK_TV, osd_string_set_default },
	{ "Set OSD String with no timeout", CEC_LOG_ADDR_MASK_TV, osd_string_set_until_clear },
	{ "Set OSD String with invalid operand", CEC_LOG_ADDR_MASK_TV, osd_string_invalid },
};


/* Routing Control */

static int routing_control_inactive_source(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	interactive_info(true, "Please make sure that the TV is currently viewing this source.");
	cec_msg_init(&msg, me, la);
	cec_msg_inactive_source(&msg, node->phys_addr);
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	fail_on_test(interactive && !question("Did the TV switch away from or stop showing this source?"));

	if (interactive)
		return 0;
	else
		return PRESUMED_OK;
}

static int routing_control_active_source(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	interactive_info(true, "Please switch the TV to another source.");
	cec_msg_init(&msg, me, la);
	cec_msg_active_source(&msg, node->phys_addr);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(interactive && !question("Did the TV switch to this source?"));

	if (interactive)
		return 0;
	else
		return PRESUMED_OK;
}

static int routing_control_req_active_source(struct node *node, unsigned me, unsigned la, bool interactive)
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

static int routing_control_set_stream_path(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};
	__u16 phys_addr;

	/* Send Set Stream Path with the remote physical address. We expect the
	   source to eventually send Active Source. The timeout of 60 seconds is
	   necessary because the device might have to wake up from standby.

	   In CEC 2.0 it is mandatory for sources to send Active Source. */
	if (is_tv(la, node->remote[la].prim_type))
		interactive_info(true, "Please ensure that the device is in standby.");
	announce("Sending Set Stream Path and waiting for reply. This may take up to 60 s.");
	cec_msg_init(&msg, me, la);
	cec_msg_set_stream_path(&msg, node->remote[la].phys_addr);
	msg.reply = CEC_MSG_ACTIVE_SOURCE;
	fail_on_test(!transmit_timeout(node, &msg, 60000));
	if (timed_out(&msg) && is_tv(la, node->remote[la].prim_type))
		return NOTSUPPORTED;
	if (timed_out(&msg) && node->remote[la].cec_version < CEC_OP_CEC_VERSION_2_0) {
		warn("Device did not respond to Set Stream Path.\n");
		return NOTSUPPORTED;
	}
	fail_on_test_v2(node->remote[la].cec_version, timed_out(&msg));
	cec_ops_active_source(&msg, &phys_addr);
	fail_on_test(phys_addr != node->remote[la].phys_addr);
	if (is_tv(la, node->remote[la].prim_type))
		fail_on_test(interactive && !question("Did the device go out of standby?"));

	if (interactive || node->remote[la].cec_version >= CEC_OP_CEC_VERSION_2_0)
		return 0;
	else
		return PRESUMED_OK;
	return 0;
}

static struct remote_subtest routing_control_subtests[] = {
	{ "Inactive Source", CEC_LOG_ADDR_MASK_TV, routing_control_inactive_source },
	{ "Active Source", CEC_LOG_ADDR_MASK_TV, routing_control_active_source },
	{ "Request Active Source", CEC_LOG_ADDR_MASK_ALL, routing_control_req_active_source },
	{ "Set Stream Path", CEC_LOG_ADDR_MASK_ALL, routing_control_set_stream_path },
};


/* Remote Control Passthrough */

static int rc_passthrough_user_ctrl_pressed(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};
	struct cec_op_ui_command rc_press;

	cec_msg_init(&msg, me, la);
	rc_press.ui_cmd = 0x41; // Volume up key (the key is not crucial here)
	cec_msg_user_control_pressed(&msg, &rc_press);
	fail_on_test(!transmit_timeout(node, &msg));
	/* Mandatory for all except devices which have taken logical address 15 */
	fail_on_test_v2(node->remote[la].cec_version,
			unrecognized_op(&msg) && !(cec_is_unregistered(1 << la)));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;

	return PRESUMED_OK;
}

static int rc_passthrough_user_ctrl_released(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_user_control_released(&msg);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test_v2(node->remote[la].cec_version,
			cec_msg_status_is_abort(&msg) && !(la & CEC_LOG_ADDR_MASK_UNREGISTERED));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	node->remote[la].has_remote_control_passthrough = true;

	return PRESUMED_OK;
}

static struct remote_subtest rc_passthrough_subtests[] = {
	{ "User Control Pressed", CEC_LOG_ADDR_MASK_ALL, rc_passthrough_user_ctrl_pressed },
	{ "User Control Released", CEC_LOG_ADDR_MASK_ALL, rc_passthrough_user_ctrl_released },
};


/* Device Menu Control */

/*
  TODO: These are very rudimentary tests which should be expanded.
 */

static int dev_menu_ctl_request(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_menu_request(&msg, true, CEC_OP_MENU_REQUEST_QUERY);
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;
	if (node->remote[la].cec_version >= CEC_OP_CEC_VERSION_2_0)
		warn("The Device Menu Control feature is deprecated in CEC 2.0\n");

	return 0;
}

static struct remote_subtest dev_menu_ctl_subtests[] = {
	{ "Menu Request", (__u16)~CEC_LOG_ADDR_MASK_TV, dev_menu_ctl_request },
	{ "User Control Pressed", CEC_LOG_ADDR_MASK_ALL, rc_passthrough_user_ctrl_pressed },
	{ "User Control Released", CEC_LOG_ADDR_MASK_ALL, rc_passthrough_user_ctrl_released },
};


/* Deck Control */

/*
  TODO: These are very rudimentary tests which should be expanded.
 */

static int deck_ctl_give_status(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_give_deck_status(&msg, true, CEC_OP_STATUS_REQ_ONCE);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	if (is_playback_or_rec(la)) {
		fail_on_test_v2(node->remote[la].cec_version,
				node->remote[la].has_deck_ctl && cec_msg_status_is_abort(&msg));
		fail_on_test_v2(node->remote[la].cec_version,
				!node->remote[la].has_deck_ctl && !unrecognized_op(&msg));
	}
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static int deck_ctl_deck_status(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_deck_status(&msg, CEC_OP_DECK_INFO_STOP);
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static int deck_ctl_deck_ctl(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_deck_control(&msg, CEC_OP_DECK_CTL_MODE_STOP);
	fail_on_test(!transmit_timeout(node, &msg));
	if (is_playback_or_rec(la)) {
		fail_on_test_v2(node->remote[la].cec_version,
				node->remote[la].has_deck_ctl && unrecognized_op(&msg));
		fail_on_test_v2(node->remote[la].cec_version,
				!node->remote[la].has_deck_ctl && !unrecognized_op(&msg));
	}
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return PRESUMED_OK;
}

static int deck_ctl_play(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_play(&msg, CEC_OP_PLAY_MODE_PLAY_STILL);
	fail_on_test(!transmit_timeout(node, &msg));
	if (is_playback_or_rec(la)) {
		fail_on_test_v2(node->remote[la].cec_version,
				node->remote[la].has_deck_ctl && unrecognized_op(&msg));
		fail_on_test_v2(node->remote[la].cec_version,
				!node->remote[la].has_deck_ctl && !unrecognized_op(&msg));
	}
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return PRESUMED_OK;
}

static struct remote_subtest deck_ctl_subtests[] = {
	{ "Give Deck Status",
	  CEC_LOG_ADDR_MASK_PLAYBACK | CEC_LOG_ADDR_MASK_RECORD,
	  deck_ctl_give_status },
	{ "Deck Status",
	  CEC_LOG_ADDR_MASK_ALL,
	  deck_ctl_deck_status },
	{ "Deck Control",
	  CEC_LOG_ADDR_MASK_PLAYBACK | CEC_LOG_ADDR_MASK_RECORD,
	  deck_ctl_deck_ctl },
	{ "Play",
	  CEC_LOG_ADDR_MASK_PLAYBACK | CEC_LOG_ADDR_MASK_RECORD,
	  deck_ctl_play },
};


/* Tuner Control */

/*
  TODO: These are very rudimentary tests which should be expanded.
 */

static int tuner_ctl_give_status(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_give_tuner_device_status(&msg, true, CEC_OP_STATUS_REQ_ONCE);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static int tuner_ctl_sel_analog_service(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	node->remote[la].bcast_sys = ~0;
	for (unsigned sys = 0; sys <= 8; sys++) {
		cec_msg_init(&msg, me, la);
		cec_msg_select_analogue_service(&msg, CEC_OP_ANA_BCAST_TYPE_CABLE,
						7668, sys); // 479.25 MHz analog frequency
		fail_on_test(!transmit_timeout(node, &msg));
		if (unrecognized_op(&msg))
			return NOTSUPPORTED;
		if (abort_reason(&msg) == CEC_OP_ABORT_INVALID_OP) {
			info("Tuner supports %s, but cannot select that service.\n",
			     bcast_system2s(sys));
			node->remote[la].bcast_sys = sys;
			continue;
		}
		if (cec_msg_status_is_abort(&msg))
			continue;
		info("Tuner supports %s\n", bcast_system2s(sys));
		node->remote[la].bcast_sys = sys;
	}

	if (node->remote[la].bcast_sys == (__u8)~0)
		warn("No analog broadcast format supported\n");
	else
		return 0;

	return PRESUMED_OK;
}

static int tuner_ctl_sel_digital_service(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};
	struct cec_op_digital_service_id digital_service_id = {};

	digital_service_id.service_id_method = CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL;
	digital_service_id.channel.channel_number_fmt = CEC_OP_CHANNEL_NUMBER_FMT_1_PART;
	digital_service_id.channel.minor = 1;

	__u8 bcast_systems[] = {
		CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_GEN,
		CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_GEN,
		CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_GEN,
		CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_BS,
		CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_CS,
		CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ARIB_T,
		CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_CABLE,
		CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_SAT,
		CEC_OP_DIG_SERVICE_BCAST_SYSTEM_ATSC_T,
		CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_C,
		CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_S,
		CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_S2,
		CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_T,
	};

	node->remote[la].dig_bcast_sys = ~0;
	for (unsigned i = 0; i < ARRAY_SIZE(bcast_systems); i++) {
		__u8 sys = bcast_systems[i];

		digital_service_id.dig_bcast_system = sys;
		cec_msg_init(&msg, me, la);
		cec_msg_select_digital_service(&msg, &digital_service_id);
		fail_on_test(!transmit_timeout(node, &msg));
		if (unrecognized_op(&msg))
			return NOTSUPPORTED;
		if (abort_reason(&msg) == CEC_OP_ABORT_INVALID_OP) {
			info("Tuner supports %s, but cannot select that service.\n",
			     dig_bcast_system2s(sys));
			node->remote[la].dig_bcast_sys = sys;
			continue;
		}
		if (cec_msg_status_is_abort(&msg))
			continue;
		info("Tuner supports %s\n", dig_bcast_system2s(sys));
		node->remote[la].dig_bcast_sys = sys;
	}

	if (node->remote[la].dig_bcast_sys == (__u8)~0)
		warn("No digital broadcast system supported\n");
	else
		return 0;

	return PRESUMED_OK;
}

static int tuner_ctl_tuner_dev_status(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};
	struct cec_op_tuner_device_info tuner_dev_info = {};

	tuner_dev_info.rec_flag = CEC_OP_REC_FLAG_NOT_USED;
	tuner_dev_info.tuner_display_info = CEC_OP_TUNER_DISPLAY_INFO_NONE;
	tuner_dev_info.is_analog = false;
	tuner_dev_info.digital.service_id_method = CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL;
	tuner_dev_info.digital.dig_bcast_system = CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_C;
	tuner_dev_info.digital.channel.channel_number_fmt = CEC_OP_CHANNEL_NUMBER_FMT_1_PART;
	tuner_dev_info.digital.channel.minor = 1;

	cec_msg_init(&msg, me, la);

	cec_msg_tuner_device_status(&msg, &tuner_dev_info);
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static int tuner_ctl_step_dec(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_tuner_step_decrement(&msg);
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;

	return PRESUMED_OK;
}

static int tuner_ctl_step_inc(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_tuner_step_increment(&msg);
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;

	return PRESUMED_OK;
}

static struct remote_subtest tuner_ctl_subtests[] = {
	{ "Give Tuner Device Status", CEC_LOG_ADDR_MASK_TUNER, tuner_ctl_give_status },
	{ "Select Analogue Service", CEC_LOG_ADDR_MASK_TUNER, tuner_ctl_sel_analog_service },
	{ "Select Digital Service", CEC_LOG_ADDR_MASK_TUNER, tuner_ctl_sel_digital_service },
	{ "Tuner Device Status", CEC_LOG_ADDR_MASK_ALL, tuner_ctl_tuner_dev_status },
	{ "Tuner Step Decrement", CEC_LOG_ADDR_MASK_TUNER, tuner_ctl_step_dec },
	{ "Tuner Step Increment", CEC_LOG_ADDR_MASK_TUNER, tuner_ctl_step_inc },
};


/* One Touch Record */

/*
  TODO: These are very rudimentary tests which should be expanded.

  - The HDMI CEC 1.4b spec details that Standby shall not be acted upon while the
    device is recording, but it should remember that it received Standby.
 */

static int one_touch_rec_tv_screen(struct node *node, unsigned me, unsigned la, bool interactive)
{
	/*
	  TODO:
	  - Page 36 in HDMI CEC 1.4b spec lists additional behaviors that should be
	    checked for.
	  - The TV should ignore this message when received from other LA than Recording or
	    Reserved.
	 */
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_record_tv_screen(&msg, true);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test_v2(node->remote[la].cec_version,
			node->remote[la].has_rec_tv && unrecognized_op(&msg));
	fail_on_test_v2(node->remote[la].cec_version,
			!node->remote[la].has_rec_tv && !unrecognized_op(&msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static int one_touch_rec_on(struct node *node, unsigned me, unsigned la, bool interactive)
{
	/*
	  TODO: Page 36 in HDMI CEC 1.4b spec lists additional behaviors that should be
	  checked for.
	 */
	struct cec_msg msg = {};
	struct cec_op_record_src rec_src = {};

	rec_src.type = CEC_OP_RECORD_SRC_OWN;
	cec_msg_init(&msg, me, la);
	cec_msg_record_on(&msg, true, &rec_src);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	fail_on_test(cec_has_record(1 << la) && unrecognized_op(&msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static int one_touch_rec_off(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_record_off(&msg, false);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(cec_has_record(1 << la) && unrecognized_op(&msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;
	if (timed_out(&msg))
		return PRESUMED_OK;
	else
		return 0;
}

static int one_touch_rec_status(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_record_status(&msg, CEC_OP_RECORD_STATUS_DIG_SERVICE);
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static struct remote_subtest one_touch_rec_subtests[] = {
	{ "Record TV Screen", CEC_LOG_ADDR_MASK_TV, one_touch_rec_tv_screen },
	{ "Record On", CEC_LOG_ADDR_MASK_RECORD, one_touch_rec_on },
	{ "Record Off", CEC_LOG_ADDR_MASK_RECORD, one_touch_rec_off },
	{ "Record Status", CEC_LOG_ADDR_MASK_ALL, one_touch_rec_status },
};


/* Timer Programming */

/*
  TODO: These are very rudimentary tests which should be expanded.
 */

static int timer_prog_set_analog_timer(struct node *node, unsigned me, unsigned la, bool interactive)
{
	/* TODO: Check the timer status for possible errors, etc. */

	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_set_analogue_timer(&msg, true, 1, 1, 0, 0, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY,
				     CEC_OP_ANA_BCAST_TYPE_CABLE,
				     7668, // 479.25 MHz
				     node->remote[la].bcast_sys);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	if (timed_out(&msg)) {
		warn("Timed out waiting for Timer Status. Assuming timer was set.\n");
		return PRESUMED_OK;
	}
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static int timer_prog_set_digital_timer(struct node *node, unsigned me, unsigned la, bool interactive)
{
	/* TODO: Check the timer status for possible errors, etc. */

	struct cec_msg msg = {};
	struct cec_op_digital_service_id digital_service_id = {};

	digital_service_id.service_id_method = CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL;
	digital_service_id.channel.channel_number_fmt = CEC_OP_CHANNEL_NUMBER_FMT_1_PART;
	digital_service_id.channel.minor = 1;
	digital_service_id.dig_bcast_system = node->remote[la].dig_bcast_sys;
	cec_msg_init(&msg, me, la);
	cec_msg_set_digital_timer(&msg, true, 1, 1, 0, 0, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY,
				    &digital_service_id);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	if (timed_out(&msg)) {
		warn("Timed out waiting for Timer Status. Assuming timer was set.\n");
		return PRESUMED_OK;
	}
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static int timer_prog_set_ext_timer(struct node *node, unsigned me, unsigned la, bool interactive)
{
	/* TODO: Check the timer status. */

	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_set_ext_timer(&msg, true, 1, 1, 0, 0, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY,
			      CEC_OP_EXT_SRC_PHYS_ADDR, 0, node->phys_addr);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	if (timed_out(&msg)) {
		warn("Timed out waiting for Timer Status. Assuming timer was set.\n");
		return PRESUMED_OK;
	}
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static int timer_prog_clear_analog_timer(struct node *node, unsigned me, unsigned la, bool interactive)
{
	/* TODO: Check the timer cleared status. */

	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_clear_analogue_timer(&msg, true, 1, 1, 0, 0, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY,
				     CEC_OP_ANA_BCAST_TYPE_CABLE,
				     7668, // 479.25 MHz
				     node->remote[la].bcast_sys);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	if (timed_out(&msg)) {
		warn("Timed out waiting for Timer Cleared Status. Assuming timer was cleared.\n");
		return PRESUMED_OK;
	}
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static int timer_prog_clear_digital_timer(struct node *node, unsigned me, unsigned la, bool interactive)
{
	/* TODO: Check the timer cleared status. */

	struct cec_msg msg = {};
	struct cec_op_digital_service_id digital_service_id = {};

	digital_service_id.service_id_method = CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL;
	digital_service_id.channel.channel_number_fmt = CEC_OP_CHANNEL_NUMBER_FMT_1_PART;
	digital_service_id.channel.minor = 1;
	digital_service_id.dig_bcast_system = node->remote[la].dig_bcast_sys;
	cec_msg_init(&msg, me, la);
	cec_msg_clear_digital_timer(&msg, true, 1, 1, 0, 0, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY,
				    &digital_service_id);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	if (timed_out(&msg)) {
		warn("Timed out waiting for Timer Cleared Status. Assuming timer was cleared.\n");
		return PRESUMED_OK;
	}
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static int timer_prog_clear_ext_timer(struct node *node, unsigned me, unsigned la, bool interactive)
{
	/* TODO: Check the timer cleared status. */

	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_clear_ext_timer(&msg, true, 1, 1, 0, 0, 1, 0, CEC_OP_REC_SEQ_ONCE_ONLY,
				CEC_OP_EXT_SRC_PHYS_ADDR, 0, node->phys_addr);
	fail_on_test(!transmit_timeout(node, &msg, 10000));
	if (timed_out(&msg)) {
		warn("Timed out waiting for Timer Cleared Status. Assuming timer was cleared.\n");
		return PRESUMED_OK;
	}
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return PRESUMED_OK;

	return 0;
}

static int timer_prog_set_prog_title(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_set_timer_program_title(&msg, "Super-Hans II");
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;

	return PRESUMED_OK;
}

static int timer_prog_timer_status(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_timer_status(&msg, CEC_OP_TIMER_OVERLAP_WARNING_NO_OVERLAP,
			     CEC_OP_MEDIA_INFO_NO_MEDIA,
			     CEC_OP_PROG_INFO_ENOUGH_SPACE,
			     0, 0, 0);
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;

	return PRESUMED_OK;
}

static int timer_prog_timer_clear_status(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg = {};

	cec_msg_init(&msg, me, la);
	cec_msg_timer_cleared_status(&msg, CEC_OP_TIMER_CLR_STAT_CLEARED);
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return NOTSUPPORTED;
	if (refused(&msg))
		return REFUSED;

	return PRESUMED_OK;
}

static struct remote_subtest timer_prog_subtests[] = {
	{ "Set Analogue Timer", CEC_LOG_ADDR_MASK_RECORD, timer_prog_set_analog_timer },
	{ "Set Digital Timer", CEC_LOG_ADDR_MASK_RECORD, timer_prog_set_digital_timer },
	{ "Set Timer Program Title", CEC_LOG_ADDR_MASK_RECORD, timer_prog_set_prog_title },
	{ "Set External Timer", CEC_LOG_ADDR_MASK_RECORD, timer_prog_set_ext_timer },
	{ "Clear Analogue Timer", CEC_LOG_ADDR_MASK_RECORD, timer_prog_clear_analog_timer },
	{ "Clear Digital Timer", CEC_LOG_ADDR_MASK_RECORD, timer_prog_clear_digital_timer },
	{ "Clear External Timer", CEC_LOG_ADDR_MASK_RECORD, timer_prog_clear_ext_timer },
	{ "Timer Status", CEC_LOG_ADDR_MASK_RECORD, timer_prog_timer_status },
	{ "Timer Cleared Status", CEC_LOG_ADDR_MASK_RECORD, timer_prog_timer_clear_status },
};

static int cdc_hec_discover(struct node *node, unsigned me, unsigned la, bool print)
{
	/* TODO: For future use cases, it might be necessary to store the results
	   from the HEC discovery to know which HECs are possible to form, etc. */
	struct cec_msg msg = {};
	__u32 mode = CEC_MODE_INITIATOR | CEC_MODE_FOLLOWER;
	bool has_cdc = false;

	doioctl(node, CEC_S_MODE, &mode);
	cec_msg_init(&msg, me, la);
	cec_msg_cdc_hec_discover(&msg);
	fail_on_test(!transmit(node, &msg));

	/* The spec describes that we shall wait for messages
	   up to 1 second, and extend the deadline for every received
	   message. The maximum time to wait for incoming state reports
	   is 5 seconds. */
	unsigned ts_start = get_ts_ms();
	while (get_ts_ms() - ts_start < 5000) {
		__u8 from;

		memset(&msg, 0, sizeof(msg));
		msg.timeout = 1000;
		if (doioctl(node, CEC_RECEIVE, &msg))
			break;
		from = cec_msg_initiator(&msg);
		if (msg.msg[1] == CEC_MSG_FEATURE_ABORT) {
			if (from == la)
				return fail("Device replied Feature Abort to broadcast message\n");
			else
				warn("Device %d replied Feature Abort to broadcast message\n", cec_msg_initiator(&msg));
		}
		if (msg.msg[1] != CEC_MSG_CDC_MESSAGE)
			continue;
		if (msg.msg[4] != CEC_MSG_CDC_HEC_REPORT_STATE)
			continue;

		__u16 phys_addr, target_phys_addr, hec_field;
		__u8 hec_func_state, host_func_state, enc_func_state, cdc_errcode, has_field;

		cec_ops_cdc_hec_report_state(&msg, &phys_addr, &target_phys_addr,
					     &hec_func_state, &host_func_state,
					     &enc_func_state, &cdc_errcode,
					     &has_field, &hec_field);

		if (target_phys_addr != node->phys_addr)
			continue;
		if (phys_addr == node->remote[la].phys_addr)
			has_cdc = true;
		if (!print)
			continue;

		from = cec_msg_initiator(&msg);
		info("Received CDC HEC State report from device %d (%s):\n", from, la2s(from));
		info("Physical address                 : %x.%x.%x.%x\n",
		     cec_phys_addr_exp(phys_addr));
		info("Target physical address          : %x.%x.%x.%x\n",
		     cec_phys_addr_exp(target_phys_addr));
		info("HEC Functionality State          : %s\n", hec_func_state2s(hec_func_state));
		info("Host Functionality State         : %s\n", host_func_state2s(host_func_state));
		info("ENC Functionality State          : %s\n", enc_func_state2s(enc_func_state));
		info("CDC Error Code                   : %s\n", cdc_errcode2s(cdc_errcode));

		if (has_field) {
			std::ostringstream oss;

			/* Bit 14 indicates whether or not the device's HDMI
			   output has HEC support/is active. */
			if (!hec_field)
				oss << "None";
			else {
				if (hec_field & (1 << 14))
					oss << "out, ";
				for (int i = 13; i >= 0; i--) {
					if (hec_field & (1 << i))
						oss << "in" << (14 - i) << ", ";
				}
				oss << "\b\b ";
			}
			info("HEC Suppport Field    : %s\n", oss.str().c_str());
		}
	}

	mode = CEC_MODE_INITIATOR;
	doioctl(node, CEC_S_MODE, &mode);

	if (has_cdc)
		return 0;
	return NOTSUPPORTED;
}

static struct remote_subtest cdc_subtests[] = {
	{ "CDC_HEC_Discover", CEC_LOG_ADDR_MASK_ALL, cdc_hec_discover },
};


/* Post-test checks */

static int post_test_check_recognized(struct node *node, unsigned me, unsigned la, bool interactive)
{
	bool fail = false;

	for (unsigned i = 0; i < 256; i++) {
		if (node->remote[la].recognized_op[i] && node->remote[la].unrecognized_op[i]) {
			struct cec_msg msg = {};
			msg.msg[1] = i;
			fail("Opcode %s has been both recognized by and has been replied\n", opcode2s(&msg).c_str());
			fail("Feature Abort [Unrecognized Opcode] to by the device.\n");
			fail = true;
		}
	}
	fail_on_test(fail);

	return 0;
}

static struct remote_subtest post_test_subtests[] = {
	{ "Recognized/unrecognized message consistency", CEC_LOG_ADDR_MASK_ALL, post_test_check_recognized },
};


static struct remote_test tests[] = {
	test_case("Core",
		  TAG_CORE,
		  core_subtests),
	test_case_ext("Give Device Power Status feature",
		      TAG_POWER_STATUS,
		      power_status_subtests),
	test_case("System Information feature",
		  TAG_SYSTEM_INFORMATION,
		  system_info_subtests),
	test_case("Vendor Specific Commands feature",
		  TAG_VENDOR_SPECIFIC_COMMANDS,
		  vendor_specific_subtests),
	test_case("Device OSD Transfer feature",
		  TAG_DEVICE_OSD_TRANSFER,
		  device_osd_transfer_subtests),
	test_case("OSD String feature",
		  TAG_OSD_DISPLAY,
		  osd_string_subtests),
	test_case("Remote Control Passthrough feature",
		  TAG_REMOTE_CONTROL_PASSTHROUGH,
		  rc_passthrough_subtests),
	test_case("Device Menu Control feature",
		  TAG_DEVICE_MENU_CONTROL,
		  dev_menu_ctl_subtests),
	test_case("Deck Control feature",
		  TAG_DECK_CONTROL,
		  deck_ctl_subtests),
	test_case("Tuner Control feature",
		  TAG_TUNER_CONTROL,
		  tuner_ctl_subtests),
	test_case("One Touch Record feature",
		  TAG_ONE_TOUCH_RECORD,
		  one_touch_rec_subtests),
	test_case("Timer Programming feature",
		  TAG_TIMER_PROGRAMMING,
		  timer_prog_subtests),
	test_case("Capability Discovery and Control feature",
		  TAG_CAP_DISCOVERY_CONTROL,
		  cdc_subtests),
	test_case_ext("Dynamic Auto Lipsync feature",
		      TAG_DYNAMIC_AUTO_LIPSYNC,
		      dal_subtests),
	test_case_ext("Audio Return Channel feature",
		      TAG_ARC_CONTROL,
		      arc_subtests),
	test_case_ext("System Audio Control feature",
		      TAG_SYSTEM_AUDIO_CONTROL,
		      sac_subtests),
	test_case_ext("Audio Rate Control feature",
		      TAG_AUDIO_RATE_CONTROL,
		      audio_rate_ctl_subtests),
	test_case_ext("One Touch Play feature",
		      TAG_ONE_TOUCH_PLAY,
		      one_touch_play_subtests),
	test_case("Routing Control feature",
		  TAG_ROUTING_CONTROL,
		  routing_control_subtests),
	test_case_ext("Standby/Resume and Power Status",
		      TAG_POWER_STATUS | TAG_STANDBY_RESUME,
		      standby_resume_subtests),
	test_case("Post-test checks",
		  TAG_CORE,
		  post_test_subtests),
};

static const unsigned num_tests = sizeof(tests) / sizeof(struct remote_test);

void testRemote(struct node *node, unsigned me, unsigned la, unsigned test_tags,
		bool interactive)
{
	printf("testing CEC local LA %d (%s) to remote LA %d (%s):\n",
	       me, la2s(me), la, la2s(la));

	if (!util_interactive_ensure_power_state(node, me, la, interactive, CEC_OP_POWER_STATUS_ON))
		return;
	if (node->remote[la].in_standby && !interactive) {
		announce("The remote device is in standby. It should be powered on when testing. Aborting.");
		return;
	}
	if (!node->remote[la].has_power_status) {
		announce("The device didn't support Give Device Power Status.");
		announce("Assuming that the device is powered on.");
	}

	int ret = 0;

	for (unsigned i = 0; i < num_tests; i++) {
		if ((tests[i].tags & test_tags) != tests[i].tags)
			continue;
		printf("\t%s:\n", tests[i].name);
		for (unsigned j = 0; j < tests[i].num_subtests; j++) {
			ret = tests[i].subtests[j].test_fn(node, me, la, interactive);
			if (!(tests[i].subtests[j].la_mask & (1 << la)) && !ret) {
				printf("\t    %s: OK (Unexpected)\n",
				       tests[i].subtests[j].name);
			}
			else if (ret != NOTAPPLICABLE)
				printf("\t    %s: %s\n", tests[i].subtests[j].name, ok(ret));
			if (ret == FAIL_CRITICAL)
				return;
		}
		printf("\n");
	}
}
