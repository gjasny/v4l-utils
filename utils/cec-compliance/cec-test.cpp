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

struct remote_test {
	const char *name;
	const unsigned tags;
	const vec_remote_subtests &subtests;
};

static int deck_status_get(struct node *node, unsigned me, unsigned la, __u8 &deck_status)
{
	struct cec_msg msg;
	deck_status = 0;

	cec_msg_init(&msg, me, la);
	cec_msg_give_deck_status(&msg, true, CEC_OP_STATUS_REQ_ONCE);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out_or_abort(&msg));
	cec_ops_deck_status(&msg, &deck_status);

	return OK;
}

static int test_play_mode(struct node *node, unsigned me, unsigned la, __u8 play_mode, __u8 expected)
{
	struct cec_msg msg;
	__u8 deck_status;

	cec_msg_init(&msg, me, la);
	cec_msg_play(&msg, play_mode);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(cec_msg_status_is_abort(&msg)); /* Assumes deck has media. */
	fail_on_test(deck_status_get(node, me, la, deck_status));
	fail_on_test(deck_status != expected);

	return OK;
}


/* System Information */

int system_info_polling(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	fail_on_test(doioctl(node, CEC_TRANSMIT, &msg));
	if (node->remote_la_mask & (1 << la)) {
		if (!cec_msg_status_is_ok(&msg)) {
			fail("Polling a valid remote LA failed\n");
			return FAIL_CRITICAL;
		}
	} else {
		if (cec_msg_status_is_ok(&msg)) {
			fail("Polling an invalid remote LA was successful\n");
			return FAIL_CRITICAL;
		}
		return OK_NOT_SUPPORTED;
	}

	return 0;
}

int system_info_phys_addr(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_give_physical_addr(&msg, true);
	if (!transmit_timeout(node, &msg) || timed_out_or_abort(&msg)) {
		fail_or_warn(node, "Give Physical Addr timed out\n");
		return node->in_standby ? 0 : FAIL_CRITICAL;
	}
	fail_on_test(node->remote[la].phys_addr != ((msg.msg[2] << 8) | msg.msg[3]));
	fail_on_test(node->remote[la].prim_type != msg.msg[4]);
	return 0;
}

int system_info_version(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_get_cec_version(&msg, true);
	if (!transmit_timeout(node, &msg) || timed_out_or_abort(&msg))
		return fail_or_warn(node, "Get CEC Version timed out\n");

	/* This needs to be kept in sync with newer CEC versions */
	fail_on_test(msg.msg[2] < CEC_OP_CEC_VERSION_1_3A ||
		     msg.msg[2] > CEC_OP_CEC_VERSION_2_0);
	fail_on_test(node->remote[la].cec_version != msg.msg[2]);

	return 0;
}

int system_info_get_menu_lang(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;
	char language[4];

	cec_msg_init(&msg, me, la);
	cec_msg_get_menu_language(&msg, true);
	if (!transmit_timeout(node, &msg) || timed_out(&msg))
		return fail_or_warn(node, "Get Menu Languages timed out\n");

	/* Devices other than TVs shall send Feature Abort [Unregcognized Opcode]
	   in reply to Get Menu Language. */
	fail_on_test(!is_tv(la, node->remote[la].prim_type) && !unrecognized_op(&msg));

	if (unrecognized_op(&msg)) {
		if (is_tv(la, node->remote[la].prim_type))
			warn("TV did not respond to Get Menu Language.\n");
		return OK_NOT_SUPPORTED;
	}
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return OK_PRESUMED;
	cec_ops_set_menu_language(&msg, language);
	fail_on_test(strcmp(node->remote[la].language, language));

	return 0;
}

static int system_info_set_menu_lang(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_set_menu_language(&msg, "eng");
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;

	return OK_PRESUMED;
}

int system_info_give_features(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_give_features(&msg, true);
	if (!transmit_timeout(node, &msg) || timed_out(&msg))
		return fail_or_warn(node, "Give Features timed out\n");
	if (unrecognized_op(&msg)) {
		if (node->remote[la].cec_version < CEC_OP_CEC_VERSION_2_0)
			return OK_NOT_SUPPORTED;
		fail_on_test_v2(node->remote[la].cec_version, true);
	}
	if (refused(&msg))
		return OK_REFUSED;
	if (node->remote[la].cec_version < CEC_OP_CEC_VERSION_2_0)
		info("Device has CEC Version < 2.0 but supports Give Features.\n");

	/* RC Profile and Device Features are assumed to be 1 byte. As of CEC 2.0 only
	   1 byte is used, but this might be extended in future versions. */
	__u8 cec_version, all_device_types;
	const __u8 *rc_profile, *dev_features;

	cec_ops_report_features(&msg, &cec_version, &all_device_types, &rc_profile, &dev_features);
	fail_on_test(rc_profile == nullptr || dev_features == nullptr);
	info("All Device Types: \t\t%s\n", cec_all_dev_types2s(all_device_types).c_str());
	info("RC Profile: \t%s", cec_rc_src_prof2s(*rc_profile, "").c_str());
	info("Device Features: \t%s", cec_dev_feat2s(*dev_features, "").c_str());

	if (!(cec_has_playback(1 << la) || cec_has_record(1 << la) || cec_has_tuner(1 << la)) &&
	    (*dev_features & CEC_OP_FEAT_DEV_HAS_SET_AUDIO_RATE)) {
		return fail("Only Playback, Recording or Tuner devices shall set the Set Audio Rate bit\n");
	}
	if (!(cec_has_playback(1 << la) || cec_has_record(1 << la)) &&
	    (*dev_features & CEC_OP_FEAT_DEV_HAS_DECK_CONTROL))
		return fail("Only Playback and Recording devices shall set the Supports Deck Control bit\n");
	if (!cec_has_tv(1 << la) && node->remote[la].has_rec_tv)
		return fail("Only TVs shall set the Record TV Screen bit\n");
	if (cec_has_playback(1 << la) && (*dev_features & CEC_OP_FEAT_DEV_SINK_HAS_ARC_TX))
		return fail("A Playback device cannot set the Sink Supports ARC Tx bit\n");
	if (cec_has_tv(1 << la) && (*dev_features & CEC_OP_FEAT_DEV_SOURCE_HAS_ARC_RX))
		return fail("A TV cannot set the Source Supports ARC Rx bit\n");

	fail_on_test(cec_version != node->remote[la].cec_version);
	fail_on_test(node->remote[la].rc_profile != *rc_profile);
	fail_on_test(node->remote[la].dev_features != *dev_features);
	fail_on_test(node->remote[la].all_device_types != all_device_types);
	return 0;
}

static const vec_remote_subtests system_info_subtests{
	{ "Polling Message", CEC_LOG_ADDR_MASK_ALL, system_info_polling },
	{ "Give Physical Address", CEC_LOG_ADDR_MASK_ALL, system_info_phys_addr },
	{ "Give CEC Version", CEC_LOG_ADDR_MASK_ALL, system_info_version },
	{ "Get Menu Language", CEC_LOG_ADDR_MASK_ALL, system_info_get_menu_lang },
	{ "Set Menu Language", CEC_LOG_ADDR_MASK_ALL, system_info_set_menu_lang },
	{ "Give Device Features", CEC_LOG_ADDR_MASK_ALL, system_info_give_features },
};

/* Core behavior */

int core_unknown(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;
	const __u8 unknown_opcode = 0xfe;

	/* Unknown opcodes should be responded to with Feature Abort, with abort
	   reason Unknown Opcode.

	   For CEC 2.0 and before, 0xfe is an unused opcode. The test possibly
	   needs to be updated for future CEC versions. */
	cec_msg_init(&msg, me, la);
	msg.len = 2;
	msg.msg[1] = unknown_opcode;
	if (!transmit_timeout(node, &msg) || timed_out(&msg))
		return fail_or_warn(node, "Unknown Opcode timed out\n");
	fail_on_test(!cec_msg_status_is_abort(&msg));

	__u8 abort_msg, reason;

	cec_ops_feature_abort(&msg, &abort_msg, &reason);
	fail_on_test(reason != CEC_OP_ABORT_UNRECOGNIZED_OP);
	fail_on_test(abort_msg != 0xfe);

	/* Unknown opcodes that are broadcast should be ignored */
	cec_msg_init(&msg, me, CEC_LOG_ADDR_BROADCAST);
	msg.len = 2;
	msg.msg[1] = unknown_opcode;
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(!timed_out(&msg));

	return 0;
}

int core_abort(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	/* The Abort message should always be responded to with Feature Abort
	   (with any abort reason) */
	cec_msg_init(&msg, me, la);
	cec_msg_abort(&msg);
	if (!transmit_timeout(node, &msg) || timed_out(&msg))
		return fail_or_warn(node, "Abort timed out\n");
	fail_on_test(!cec_msg_status_is_abort(&msg));
	return 0;
}

static const vec_remote_subtests core_subtests{
	{ "Wake up", CEC_LOG_ADDR_MASK_ALL, standby_resume_wakeup, true },
	{ "Feature aborts unknown messages", CEC_LOG_ADDR_MASK_ALL, core_unknown },
	{ "Feature aborts Abort message", CEC_LOG_ADDR_MASK_ALL, core_abort },
};

/* Vendor Specific Commands */

int vendor_specific_commands_id(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_give_device_vendor_id(&msg, true);
	if (!transmit(node, &msg))
		return fail_or_warn(node, "Give Device Vendor ID timed out\n");
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return OK_PRESUMED;
	fail_on_test(node->remote[la].vendor_id !=
		     (__u32)((msg.msg[2] << 16) | (msg.msg[3] << 8) | msg.msg[4]));

	return 0;
}

static const vec_remote_subtests vendor_specific_subtests{
	{ "Give Device Vendor ID", CEC_LOG_ADDR_MASK_ALL, vendor_specific_commands_id },
};

/* Device OSD Transfer */

static int device_osd_transfer_set(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_set_osd_name(&msg, "Whatever");
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg)) {
		if (is_tv(la, node->remote[la].prim_type) &&
		    node->remote[la].cec_version >= CEC_OP_CEC_VERSION_2_0)
			warn("TV feature aborted Set OSD Name\n");
		return OK_NOT_SUPPORTED;
	}
	if (refused(&msg))
		return OK_REFUSED;

	return OK_PRESUMED;
}

int device_osd_transfer_give(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	/* Todo: CEC 2.0: devices with several logical addresses shall report
	   the same for each logical address. */
	cec_msg_init(&msg, me, la);
	cec_msg_give_osd_name(&msg, true);
	if (!transmit_timeout(node, &msg) || timed_out(&msg))
		return fail_or_warn(node, "Give OSD Name timed out\n");
	fail_on_test(!is_tv(la, node->remote[la].prim_type) && unrecognized_op(&msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return OK_PRESUMED;
	char osd_name[15];
	cec_ops_set_osd_name(&msg, osd_name);
	fail_on_test(!osd_name[0]);
	fail_on_test(strcmp(node->remote[la].osd_name, osd_name));
	fail_on_test(msg.len != strlen(osd_name) + 2);

	return 0;
}

static const vec_remote_subtests device_osd_transfer_subtests{
	{ "Set OSD Name", CEC_LOG_ADDR_MASK_ALL, device_osd_transfer_set },
	{ "Give OSD Name", CEC_LOG_ADDR_MASK_ALL, device_osd_transfer_give },
};

/* OSD Display */

static int osd_string_set_default(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;
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
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg)) {
		warn("The device is in an unsuitable state or cannot display the complete message.\n");
		unsuitable = true;
	}
	node->remote[la].has_osd = true;
	if (!interactive)
		return OK_PRESUMED;

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

	struct cec_msg msg;
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

	return OK_PRESUMED;
}

static int osd_string_invalid(struct node *node, unsigned me, unsigned la, bool interactive)
{
	if (!node->remote[la].has_osd)
		return NOTAPPLICABLE;

	struct cec_msg msg;

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

static const vec_remote_subtests osd_string_subtests{
	{ "Set OSD String with default timeout", CEC_LOG_ADDR_MASK_TV, osd_string_set_default },
	{ "Set OSD String with no timeout", CEC_LOG_ADDR_MASK_TV, osd_string_set_until_clear },
	{ "Set OSD String with invalid operand", CEC_LOG_ADDR_MASK_TV, osd_string_invalid },
};

/* Routing Control */

static int routing_control_inactive_source(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;
	int response;

	interactive_info(true, "Please make sure that the TV is currently viewing this source.");
	mode_set_follower(node);
	cec_msg_init(&msg, me, la);
	cec_msg_inactive_source(&msg, node->phys_addr);
	fail_on_test(!transmit(node, &msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	// It may take a bit of time for the Inactive Source message to take
	// effect, so sleep a bit.
	response = util_receive(node, CEC_LOG_ADDR_TV, 10000, &msg,
				CEC_MSG_INACTIVE_SOURCE,
				CEC_MSG_ACTIVE_SOURCE, CEC_MSG_SET_STREAM_PATH);
	if (me == CEC_LOG_ADDR_TV) {
		// Inactive Source should be ignored by all other devices
		if (response >= 0)
			return fail("Unexpected reply to Inactive Source\n");
		fail_on_test(response >= 0);
	} else {
		if (response < 0)
			warn("Expected Active Source or Set Stream Path reply to Inactive Source\n");
		fail_on_test(interactive && !question("Did the TV switch away from or stop showing this source?"));
	}

	return 0;
}

static int routing_control_active_source(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	interactive_info(true, "Please switch the TV to another source.");
	cec_msg_init(&msg, me, la);
	cec_msg_active_source(&msg, node->phys_addr);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(interactive && !question("Did the TV switch to this source?"));

	if (interactive)
		return 0;

	return OK_PRESUMED;
}

static int routing_control_req_active_source(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

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
	struct cec_msg msg;
	__u16 phys_addr;

	/* Send Set Stream Path with the remote physical address. We expect the
	   source to eventually send Active Source. The timeout of long_timeout
	   seconds is necessary because the device might have to wake up from standby.

	   In CEC 2.0 it is mandatory for sources to send Active Source. */
	if (is_tv(la, node->remote[la].prim_type))
		interactive_info(true, "Please ensure that the device is in standby.");
	announce("Sending Set Stream Path and waiting for reply. This may take up to %llu s.", (long long)long_timeout);
	cec_msg_init(&msg, me, la);
	cec_msg_set_stream_path(&msg, node->remote[la].phys_addr);
	msg.reply = CEC_MSG_ACTIVE_SOURCE;
	fail_on_test(!transmit_timeout(node, &msg, long_timeout * 1000));
	if (timed_out(&msg) && is_tv(la, node->remote[la].prim_type))
		return OK_NOT_SUPPORTED;
	if (timed_out(&msg) && node->remote[la].cec_version < CEC_OP_CEC_VERSION_2_0) {
		warn("Device did not respond to Set Stream Path.\n");
		return OK_NOT_SUPPORTED;
	}
	fail_on_test_v2(node->remote[la].cec_version, timed_out(&msg));
	cec_ops_active_source(&msg, &phys_addr);
	fail_on_test(phys_addr != node->remote[la].phys_addr);
	if (is_tv(la, node->remote[la].prim_type))
		fail_on_test(interactive && !question("Did the device go out of standby?"));

	if (interactive || node->remote[la].cec_version >= CEC_OP_CEC_VERSION_2_0)
		return 0;

	return OK_PRESUMED;
}

static const vec_remote_subtests routing_control_subtests{
	{ "Active Source", CEC_LOG_ADDR_MASK_TV, routing_control_active_source },
	{ "Request Active Source", CEC_LOG_ADDR_MASK_ALL, routing_control_req_active_source },
	{ "Inactive Source", CEC_LOG_ADDR_MASK_TV, routing_control_inactive_source },
	{ "Set Stream Path", CEC_LOG_ADDR_MASK_ALL, routing_control_set_stream_path },
};

/* Remote Control Passthrough */

static int rc_passthrough_user_ctrl_pressed(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;
	struct cec_op_ui_command rc_press;

	cec_msg_init(&msg, me, la);
	rc_press.ui_cmd = CEC_OP_UI_CMD_VOLUME_UP; // Volume up key (the key is not crucial here)
	cec_msg_user_control_pressed(&msg, &rc_press);
	fail_on_test(!transmit_timeout(node, &msg));
	/* Mandatory for all except devices which have taken logical address 15 */
	fail_on_test_v2(node->remote[la].cec_version,
			unrecognized_op(&msg) && !(cec_is_unregistered(1 << la)));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;

	return OK_PRESUMED;
}

static int rc_passthrough_user_ctrl_released(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_user_control_released(&msg);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test_v2(node->remote[la].cec_version,
			cec_msg_status_is_abort(&msg) && !(la & CEC_LOG_ADDR_MASK_UNREGISTERED));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	node->remote[la].has_remote_control_passthrough = true;

	return OK_PRESUMED;
}

static const vec_remote_subtests rc_passthrough_subtests{
	{ "User Control Pressed", CEC_LOG_ADDR_MASK_ALL, rc_passthrough_user_ctrl_pressed },
	{ "User Control Released", CEC_LOG_ADDR_MASK_ALL, rc_passthrough_user_ctrl_released },
};

/* Device Menu Control */

/*
  TODO: These are very rudimentary tests which should be expanded.
 */

static int dev_menu_ctl_request(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_menu_request(&msg, true, CEC_OP_MENU_REQUEST_QUERY);
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return OK_PRESUMED;
	if (node->remote[la].cec_version >= CEC_OP_CEC_VERSION_2_0)
		warn("The Device Menu Control feature is deprecated in CEC 2.0\n");

	return 0;
}

static const vec_remote_subtests dev_menu_ctl_subtests{
	{ "Menu Request", static_cast<__u16>(~CEC_LOG_ADDR_MASK_TV), dev_menu_ctl_request },
	{ "User Control Pressed", CEC_LOG_ADDR_MASK_ALL, rc_passthrough_user_ctrl_pressed },
	{ "User Control Released", CEC_LOG_ADDR_MASK_ALL, rc_passthrough_user_ctrl_released },
};

/* Deck Control */

static int deck_ctl_give_status(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_give_deck_status(&msg, true, CEC_OP_STATUS_REQ_ONCE);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));

	fail_on_test_v2(node->remote[la].cec_version,
	                node->remote[la].has_deck_ctl && cec_msg_status_is_abort(&msg));
	fail_on_test_v2(node->remote[la].cec_version,
	                !node->remote[la].has_deck_ctl && !unrecognized_op(&msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	if (cec_msg_status_is_abort(&msg))
		return OK_PRESUMED;

	__u8 deck_info;

	cec_ops_deck_status(&msg, &deck_info);
	fail_on_test(deck_info < CEC_OP_DECK_INFO_PLAY || deck_info > CEC_OP_DECK_INFO_OTHER);

	cec_msg_init(&msg, me, la);
	cec_msg_give_deck_status(&msg, true, CEC_OP_STATUS_REQ_ON);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(timed_out(&msg));
	cec_ops_deck_status(&msg, &deck_info);
	fail_on_test(deck_info < CEC_OP_DECK_INFO_PLAY || deck_info > CEC_OP_DECK_INFO_OTHER);

	cec_msg_init(&msg, me, la);
	cec_msg_give_deck_status(&msg, true, CEC_OP_STATUS_REQ_OFF);
	/*
	 * Reply would not normally be expected for CEC_OP_STATUS_REQ_OFF.
	 * If a reply is received, then the follower failed to turn off
	 * status reporting as required.
	 */
	msg.reply = CEC_MSG_DECK_STATUS;
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(!timed_out(&msg));

	return OK;
}

static int deck_ctl_give_status_invalid(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_give_deck_status(&msg, true, 0); /* Invalid Operand */
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	fail_on_test(!cec_msg_status_is_abort(&msg));
	fail_on_test(abort_reason(&msg) != CEC_OP_ABORT_INVALID_OP);

	cec_msg_init(&msg, me, la);
	cec_msg_give_deck_status(&msg, true, 4); /* Invalid Operand */
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(!cec_msg_status_is_abort(&msg));
	fail_on_test(abort_reason(&msg) != CEC_OP_ABORT_INVALID_OP);

	return OK;
}

static int deck_ctl_deck_ctl(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;
	__u8 deck_status;

	cec_msg_init(&msg, me, la);
	cec_msg_deck_control(&msg, CEC_OP_DECK_CTL_MODE_STOP);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test_v2(node->remote[la].cec_version,
	                node->remote[la].has_deck_ctl && unrecognized_op(&msg));
	fail_on_test_v2(node->remote[la].cec_version,
	                !node->remote[la].has_deck_ctl && !unrecognized_op(&msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	fail_on_test(deck_status_get(node, me, la, deck_status));
	if (cec_msg_status_is_abort(&msg)) {
		if (!incorrect_mode(&msg))
			return FAIL;
		if (deck_status == CEC_OP_DECK_INFO_NO_MEDIA)
			info("Stop: no media.\n");
		else
			warn("Deck has media but returned Feature Abort with Incorrect Mode.");
		return OK;
	}
	fail_on_test(deck_status != CEC_OP_DECK_INFO_STOP && deck_status != CEC_OP_DECK_INFO_NO_MEDIA);

	cec_msg_init(&msg, me, la);
	cec_msg_deck_control(&msg, CEC_OP_DECK_CTL_MODE_SKIP_FWD);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(deck_status_get(node, me, la, deck_status));
	/*
	 * If there is no media, Skip Forward should Feature Abort with Incorrect Mode
	 * even if Stop did not.  If Skip Forward does not Feature Abort, the deck
	 * is assumed to have media.
	 */
	if (incorrect_mode(&msg)) {
		fail_on_test(deck_status != CEC_OP_DECK_INFO_NO_MEDIA);
		return OK;
	}
	fail_on_test(cec_msg_status_is_abort(&msg));
	/* Wait for Deck to finish Skip Forward. */
	for (int i = 0; deck_status == CEC_OP_DECK_INFO_SKIP_FWD && i < long_timeout; i++) {
		sleep(1);
		fail_on_test(deck_status_get(node, me, la, deck_status));
	}
	fail_on_test(deck_status != CEC_OP_DECK_INFO_PLAY);

	cec_msg_init(&msg, me, la);
	cec_msg_deck_control(&msg, CEC_OP_DECK_CTL_MODE_SKIP_REV);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(cec_msg_status_is_abort(&msg)); /* Assumes deck has media. */
	fail_on_test(deck_status_get(node, me, la, deck_status));
	/* Wait for Deck to finish Skip Reverse. */
	for (int i = 0; deck_status == CEC_OP_DECK_INFO_SKIP_REV && i < long_timeout; i++) {
		sleep(1);
		fail_on_test(deck_status_get(node, me, la, deck_status));
	}
	fail_on_test(deck_status != CEC_OP_DECK_INFO_PLAY);

	cec_msg_init(&msg, me, la);
	cec_msg_deck_control(&msg, CEC_OP_DECK_CTL_MODE_EJECT);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(cec_msg_status_is_abort(&msg));
	fail_on_test(deck_status_get(node, me, la, deck_status));
	fail_on_test(deck_status != CEC_OP_DECK_INFO_NO_MEDIA);

	return OK;
}

static int deck_ctl_deck_ctl_invalid(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_deck_control(&msg, 0); /* Invalid Deck Control operand */
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	fail_on_test(!cec_msg_status_is_abort(&msg));
	fail_on_test(abort_reason(&msg) != CEC_OP_ABORT_INVALID_OP);

	cec_msg_init(&msg, me, la);
	cec_msg_deck_control(&msg, 5); /* Invalid Deck Control operand */
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(!cec_msg_status_is_abort(&msg));
	fail_on_test(abort_reason(&msg) != CEC_OP_ABORT_INVALID_OP);

	return OK;
}

static int deck_ctl_play(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;
	__u8 deck_status;

	cec_msg_init(&msg, me, la);
	cec_msg_play(&msg, CEC_OP_PLAY_MODE_PLAY_FWD);
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test_v2(node->remote[la].cec_version,
	                node->remote[la].has_deck_ctl && unrecognized_op(&msg));
	fail_on_test_v2(node->remote[la].cec_version,
	                !node->remote[la].has_deck_ctl && !unrecognized_op(&msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	if (refused(&msg))
		return OK_REFUSED;
	fail_on_test(deck_status_get(node, me, la, deck_status));
	if (cec_msg_status_is_abort(&msg)) {
		if (!incorrect_mode(&msg))
			return FAIL;
		if (deck_status == CEC_OP_DECK_INFO_NO_MEDIA)
			info("Play Still: no media.\n");
		else
			warn("Deck has media but returned Feature Abort with Incorrect Mode.");
		return OK;
	}
	fail_on_test(deck_status != CEC_OP_DECK_INFO_PLAY);

	fail_on_test(test_play_mode(node, me, la, CEC_OP_PLAY_MODE_PLAY_STILL, CEC_OP_DECK_INFO_STILL));
	fail_on_test(test_play_mode(node, me, la, CEC_OP_PLAY_MODE_PLAY_REV, CEC_OP_DECK_INFO_PLAY_REV));
	fail_on_test(test_play_mode(node, me, la, CEC_OP_PLAY_MODE_PLAY_FAST_FWD_MIN, CEC_OP_DECK_INFO_FAST_FWD));
	fail_on_test(test_play_mode(node, me, la, CEC_OP_PLAY_MODE_PLAY_FAST_REV_MIN, CEC_OP_DECK_INFO_FAST_REV));
	fail_on_test(test_play_mode(node, me, la, CEC_OP_PLAY_MODE_PLAY_FAST_FWD_MED, CEC_OP_DECK_INFO_FAST_FWD));
	fail_on_test(test_play_mode(node, me, la, CEC_OP_PLAY_MODE_PLAY_FAST_REV_MED, CEC_OP_DECK_INFO_FAST_REV));
	fail_on_test(test_play_mode(node, me, la, CEC_OP_PLAY_MODE_PLAY_FAST_FWD_MAX, CEC_OP_DECK_INFO_FAST_FWD));
	fail_on_test(test_play_mode(node, me, la, CEC_OP_PLAY_MODE_PLAY_FAST_REV_MAX, CEC_OP_DECK_INFO_FAST_REV));
	fail_on_test(test_play_mode(node, me, la, CEC_OP_PLAY_MODE_PLAY_SLOW_FWD_MIN, CEC_OP_DECK_INFO_SLOW));
	fail_on_test(test_play_mode(node, me, la, CEC_OP_PLAY_MODE_PLAY_SLOW_REV_MIN, CEC_OP_DECK_INFO_SLOW_REV));
	fail_on_test(test_play_mode(node, me, la, CEC_OP_PLAY_MODE_PLAY_SLOW_FWD_MED, CEC_OP_DECK_INFO_SLOW));
	fail_on_test(test_play_mode(node, me, la, CEC_OP_PLAY_MODE_PLAY_SLOW_REV_MED, CEC_OP_DECK_INFO_SLOW_REV));
	fail_on_test(test_play_mode(node, me, la, CEC_OP_PLAY_MODE_PLAY_SLOW_FWD_MAX, CEC_OP_DECK_INFO_SLOW));
	fail_on_test(test_play_mode(node, me, la, CEC_OP_PLAY_MODE_PLAY_SLOW_REV_MAX, CEC_OP_DECK_INFO_SLOW_REV));

	cec_msg_init(&msg, me, la);
	cec_msg_deck_control(&msg, CEC_OP_DECK_CTL_MODE_STOP);
	fail_on_test(!transmit_timeout(node, &msg));

	return OK;
}

static int deck_ctl_play_invalid(struct node *node, unsigned me, unsigned la, bool interactive)
{
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_play(&msg, 0); /* Invalid Operand */
	fail_on_test(!transmit_timeout(node, &msg));
	if (unrecognized_op(&msg))
		return OK_NOT_SUPPORTED;
	fail_on_test(!cec_msg_status_is_abort(&msg));
	fail_on_test(abort_reason(&msg) != CEC_OP_ABORT_INVALID_OP);

	cec_msg_init(&msg, me, la);
	cec_msg_play(&msg, 4); /* Invalid Operand */
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(!cec_msg_status_is_abort(&msg));
	fail_on_test(abort_reason(&msg) != CEC_OP_ABORT_INVALID_OP);

	cec_msg_init(&msg, me, la);
	cec_msg_play(&msg, 0x26); /* Invalid Operand */
	fail_on_test(!transmit_timeout(node, &msg));
	fail_on_test(!cec_msg_status_is_abort(&msg));
	fail_on_test(abort_reason(&msg) != CEC_OP_ABORT_INVALID_OP);

	return OK;
}

static const vec_remote_subtests deck_ctl_subtests{
	{
		"Give Deck Status",
		CEC_LOG_ADDR_MASK_PLAYBACK | CEC_LOG_ADDR_MASK_RECORD,
		deck_ctl_give_status,
	},
	{
		"Give Deck Status Invalid Operand",
		CEC_LOG_ADDR_MASK_PLAYBACK | CEC_LOG_ADDR_MASK_RECORD,
		deck_ctl_give_status_invalid,
	},
	{
		"Deck Control",
		CEC_LOG_ADDR_MASK_PLAYBACK | CEC_LOG_ADDR_MASK_RECORD,
		deck_ctl_deck_ctl,
	},
	{
		"Deck Control Invalid Operand",
		CEC_LOG_ADDR_MASK_PLAYBACK | CEC_LOG_ADDR_MASK_RECORD,
		deck_ctl_deck_ctl_invalid,
	},
	{
		"Play",
		CEC_LOG_ADDR_MASK_PLAYBACK | CEC_LOG_ADDR_MASK_RECORD,
		deck_ctl_play,
	},
	{
		"Play Invalid Operand",
		CEC_LOG_ADDR_MASK_PLAYBACK | CEC_LOG_ADDR_MASK_RECORD,
		deck_ctl_play_invalid,
	},
};

static const char *hec_func_state2s(__u8 hfs)
{
	switch (hfs) {
	case CEC_OP_HEC_FUNC_STATE_NOT_SUPPORTED:
		return "HEC Not Supported";
	case CEC_OP_HEC_FUNC_STATE_INACTIVE:
		return "HEC Inactive";
	case CEC_OP_HEC_FUNC_STATE_ACTIVE:
		return "HEC Active";
	case CEC_OP_HEC_FUNC_STATE_ACTIVATION_FIELD:
		return "HEC Activation Field";
	default:
		return "Unknown";
	}
}

static const char *host_func_state2s(__u8 hfs)
{
	switch (hfs) {
	case CEC_OP_HOST_FUNC_STATE_NOT_SUPPORTED:
		return "Host Not Supported";
	case CEC_OP_HOST_FUNC_STATE_INACTIVE:
		return "Host Inactive";
	case CEC_OP_HOST_FUNC_STATE_ACTIVE:
		return "Host Active";
	default:
		return "Unknown";
	}
}

static const char *enc_func_state2s(__u8 efs)
{
	switch (efs) {
	case CEC_OP_ENC_FUNC_STATE_EXT_CON_NOT_SUPPORTED:
		return "Ext Con Not Supported";
	case CEC_OP_ENC_FUNC_STATE_EXT_CON_INACTIVE:
		return "Ext Con Inactive";
	case CEC_OP_ENC_FUNC_STATE_EXT_CON_ACTIVE:
		return "Ext Con Active";
	default:
		return "Unknown";
	}
}

static const char *cdc_errcode2s(__u8 cdc_errcode)
{
	switch (cdc_errcode) {
	case CEC_OP_CDC_ERROR_CODE_NONE:
		return "No error";
	case CEC_OP_CDC_ERROR_CODE_CAP_UNSUPPORTED:
		return "Initiator does not have requested capability";
	case CEC_OP_CDC_ERROR_CODE_WRONG_STATE:
		return "Initiator is in wrong state";
	case CEC_OP_CDC_ERROR_CODE_OTHER:
		return "Other error";
	default:
		return "Unknown";
	}
}

static int cdc_hec_discover(struct node *node, unsigned me, unsigned la, bool print)
{
	/* TODO: For future use cases, it might be necessary to store the results
	   from the HEC discovery to know which HECs are possible to form, etc. */
	struct cec_msg msg;
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
		info("Received CDC HEC State report from device %d (%s):\n", from, cec_la2s(from));
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
			info("HEC Support Field    : %s\n", oss.str().c_str());
		}
	}

	mode = CEC_MODE_INITIATOR;
	doioctl(node, CEC_S_MODE, &mode);

	if (has_cdc)
		return 0;
	return OK_NOT_SUPPORTED;
}

static const vec_remote_subtests cdc_subtests{
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

static const vec_remote_subtests post_test_subtests{
	{ "Recognized/unrecognized message consistency", CEC_LOG_ADDR_MASK_ALL, post_test_check_recognized },
};

static const remote_test tests[] = {
	{ "Core", TAG_CORE, core_subtests },
	{ "Give Device Power Status feature", TAG_POWER_STATUS, power_status_subtests },
	{ "System Information feature", TAG_SYSTEM_INFORMATION, system_info_subtests },
	{ "Vendor Specific Commands feature", TAG_VENDOR_SPECIFIC_COMMANDS, vendor_specific_subtests },
	{ "Device OSD Transfer feature", TAG_DEVICE_OSD_TRANSFER, device_osd_transfer_subtests },
	{ "OSD String feature", TAG_OSD_DISPLAY, osd_string_subtests },
	{ "Remote Control Passthrough feature", TAG_REMOTE_CONTROL_PASSTHROUGH, rc_passthrough_subtests },
	{ "Device Menu Control feature", TAG_DEVICE_MENU_CONTROL, dev_menu_ctl_subtests },
	{ "Deck Control feature", TAG_DECK_CONTROL, deck_ctl_subtests },
	{ "Tuner Control feature", TAG_TUNER_CONTROL, tuner_ctl_subtests },
	{ "One Touch Record feature", TAG_ONE_TOUCH_RECORD, one_touch_rec_subtests },
	{ "Timer Programming feature", TAG_TIMER_PROGRAMMING, timer_prog_subtests },
	{ "Capability Discovery and Control feature", TAG_CAP_DISCOVERY_CONTROL, cdc_subtests },
	{ "Dynamic Auto Lipsync feature", TAG_DYNAMIC_AUTO_LIPSYNC, dal_subtests },
	{ "Audio Return Channel feature", TAG_ARC_CONTROL, arc_subtests },
	{ "System Audio Control feature", TAG_SYSTEM_AUDIO_CONTROL, sac_subtests },
	{ "Audio Rate Control feature", TAG_AUDIO_RATE_CONTROL, audio_rate_ctl_subtests },
	{ "Routing Control feature", TAG_ROUTING_CONTROL, routing_control_subtests },
	{ "Standby/Resume and Power Status", TAG_POWER_STATUS | TAG_STANDBY_RESUME, standby_resume_subtests },
	{ "Post-test checks", TAG_CORE, post_test_subtests },
};

static std::map<std::string, int> mapTests;
static std::map<std::string, bool> mapTestsNoWarnings;

void collectTests()
{
	std::map<std::string, __u64> mapTestFuncs;

	for (const auto &test : tests) {
		for (const auto &subtest : test.subtests) {
			std::string name = safename(subtest.name);
			auto func = (__u64)subtest.test_fn;

			if (mapTestFuncs.find(name) != mapTestFuncs.end() &&
			    mapTestFuncs[name] != func) {
				fprintf(stderr, "Duplicate subtest name, but different tests: %s\n", subtest.name);
				std::exit(EXIT_FAILURE);
			}
			mapTestFuncs[name] = func;
			mapTests[name] = DONT_CARE;
			mapTestsNoWarnings[name] = false;
		}
	}
}

void listTests()
{
	for (const auto &test : tests) {
		printf("%s:\n", test.name);
		for (const auto &subtest : test.subtests) {
			printf("\t%s\n", safename(subtest.name).c_str());
		}
	}
}

int setExpectedResult(char *optarg, bool no_warnings)
{
	char *equal = std::strchr(optarg, '=');

	if (!equal || equal == optarg || !isdigit(equal[1]))
		return 1;
	*equal = 0;
	std::string name = safename(optarg);
	if (mapTests.find(name) == mapTests.end())
		return 1;
	mapTests[name] = strtoul(equal + 1, nullptr, 0);
	mapTestsNoWarnings[name] = no_warnings;
	return 0;
}

void testRemote(struct node *node, unsigned me, unsigned la, unsigned test_tags,
		bool interactive, bool show_ts)
{
	printf("testing CEC local LA %d (%s) to remote LA %d (%s):\n",
	       me, cec_la2s(me), la, cec_la2s(la));

	if (!util_interactive_ensure_power_state(node, me, la, interactive, CEC_OP_POWER_STATUS_ON))
		return;
	if (!node->remote[la].has_power_status) {
		announce("The device didn't support Give Device Power Status.");
		announce("Assuming that the device is powered on.");
	}
	if (la != CEC_LOG_ADDR_UNREGISTERED &&
	    node->remote[la].phys_addr == CEC_PHYS_ADDR_INVALID) {
		announce("The device has an invalid physical address, this");
		announce("makes it impossible to run the compliance test.");
		ok(FAIL_CRITICAL);
		return;
	}

	/* Ensure that the remote device knows the initiator's primary device type.*/
	struct cec_msg msg;

	cec_msg_init(&msg, me, la);
	cec_msg_report_physical_addr(&msg, node->phys_addr, node->prim_devtype);
	transmit_timeout(node, &msg);

	int ret = 0;

	for (const auto &test : tests) {
		if ((test.tags & test_tags) != test.tags)
			continue;

		printf("\t%s:\n", test.name);
		for (const auto &subtest : test.subtests) {
			const char *name = subtest.name;

			if (subtest.for_cec20 &&
			    (node->remote[la].cec_version < CEC_OP_CEC_VERSION_2_0 || !node->has_cec20))
				continue;

			if (subtest.in_standby) {
				struct cec_log_addrs laddrs = { };
				doioctl(node, CEC_ADAP_G_LOG_ADDRS, &laddrs);

				if (!laddrs.log_addr_mask)
					continue;
			}
			std::string start_ts = current_ts();
			node->in_standby = subtest.in_standby;
			mode_set_initiator(node);
			unsigned old_warnings = warnings;
			ret = subtest.test_fn(node, me, la, interactive);
			bool has_warnings = old_warnings < warnings;
			if (!(subtest.la_mask & (1 << la)) && !ret)
				ret = OK_UNEXPECTED;

			if (mapTests[safename(name)] != DONT_CARE) {
				if (show_ts)
					printf("\t    %s: %s: ", start_ts.c_str(), name);
				else
					printf("\t    %s: ", name);
				if (ret != mapTests[safename(name)])
					printf("%s (Expected '%s', got '%s')\n",
					       ok(FAIL),
					       result_name(mapTests[safename(name)], false),
					       result_name(ret, false));
				else if (has_warnings && mapTestsNoWarnings[safename(name)])
					printf("%s (Expected no warnings, but got %d)\n",
					       ok(FAIL), warnings - old_warnings);
				else if (ret == FAIL)
					printf("%s\n", ok(OK_EXPECTED_FAIL));
				else
					printf("%s\n", ok(ret));
			} else if (ret != NOTAPPLICABLE) {
				if (show_ts)
					printf("\t    %s: %s: %s\n", start_ts.c_str(), name, ok(ret));
				else
					printf("\t    %s: %s\n", name, ok(ret));
			}
			if (ret == FAIL_CRITICAL)
				return;
		}
		printf("\n");
	}
}
