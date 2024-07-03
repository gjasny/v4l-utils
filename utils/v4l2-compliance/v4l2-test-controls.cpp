/*
    V4L2 API compliance control ioctl tests.

    Copyright (C) 2011  Hans Verkuil <hverkuil@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#include <map>
#include <set>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>

#include "compiler.h"
#include "v4l2-compliance.h"

static int checkQCtrl(struct node *node, struct test_query_ext_ctrl &qctrl)
{
	struct v4l2_querymenu qmenu;
	__u32 fl = qctrl.flags;
	__u32 rw_mask = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_WRITE_ONLY;
	int ret;
	int i;

	qctrl.menu_mask = 0;
	if (check_string(qctrl.name, sizeof(qctrl.name)))
		return fail("invalid name\n");
	info("checking v4l2_query_ext_ctrl of control '%s' (0x%08x)\n", qctrl.name, qctrl.id);
	if (qctrl.id & V4L2_CTRL_FLAG_NEXT_CTRL)
		return fail("V4L2_CTRL_FLAG_NEXT_CTRL not cleared\n");
	if (check_0(qctrl.reserved, sizeof(qctrl.reserved)))
		return fail("non-zero reserved fields\n");
	if (qctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS) {
		if ((qctrl.id & 0xffff) != 1)
			return fail("invalid control ID for a control class\n");
	} else if (qctrl.id < V4L2_CID_PRIVATE_BASE) {
		if ((qctrl.id & 0xffff) < 0x900)
			return fail("invalid control ID\n");
	}
	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_BOOLEAN:
		if (qctrl.maximum > 1)
			return fail("invalid boolean max value\n");
		fallthrough;
	case V4L2_CTRL_TYPE_MENU:
	case V4L2_CTRL_TYPE_INTEGER_MENU:
		if (qctrl.step != 1)
			return fail("invalid step value %lld\n", qctrl.step);
		if (qctrl.minimum < 0)
			return fail("min < 0\n");
		fallthrough;
	case V4L2_CTRL_TYPE_INTEGER:
	case V4L2_CTRL_TYPE_INTEGER64:
		if (qctrl.default_value < qctrl.minimum ||
		    qctrl.default_value > qctrl.maximum)
			return fail("def < min || def > max\n");
		fallthrough;
	case V4L2_CTRL_TYPE_STRING:
		if (qctrl.minimum > qctrl.maximum)
			return fail("min > max\n");
		if (qctrl.step == 0)
			return fail("step == 0\n");
		if (static_cast<unsigned>(qctrl.step) > static_cast<unsigned>(qctrl.maximum - qctrl.minimum) &&
		    qctrl.maximum != qctrl.minimum)
			return fail("step > max - min\n");
		if ((qctrl.maximum - qctrl.minimum) % qctrl.step) {
			// This really should be a fail, but there are so few
			// drivers that do this right that I made it a warning
			// for now.
			warn("%s: (max - min) %% step != 0\n", qctrl.name);
		}
		break;
	case V4L2_CTRL_TYPE_BITMASK:
		if (qctrl.minimum)
			return fail("minimum must be 0 for a bitmask control\n");
		if (qctrl.step)
			return fail("step must be 0 for a bitmask control\n");
		if (!qctrl.maximum)
			return fail("maximum must be non-zero for a bitmask control\n");
		if (qctrl.default_value & ~qctrl.maximum)
			return fail("default_value is out of range for a bitmask control\n");
		break;
	case V4L2_CTRL_TYPE_CTRL_CLASS:
	case V4L2_CTRL_TYPE_BUTTON:
		if (qctrl.minimum || qctrl.maximum || qctrl.step || qctrl.default_value)
			return fail("non-zero min/max/step/def\n");
		break;
	default:
		if (qctrl.type >= V4L2_CTRL_COMPOUND_TYPES)
			break;
		return fail("unknown control type\n");
	}
	if (qctrl.type == V4L2_CTRL_TYPE_STRING && qctrl.default_value)
		return fail("non-zero default value for string\n");
	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_BUTTON:
		if ((fl & rw_mask) != V4L2_CTRL_FLAG_WRITE_ONLY)
			return fail("button control not write only\n");
		fallthrough;
	case V4L2_CTRL_TYPE_BOOLEAN:
	case V4L2_CTRL_TYPE_MENU:
	case V4L2_CTRL_TYPE_INTEGER_MENU:
	case V4L2_CTRL_TYPE_STRING:
	case V4L2_CTRL_TYPE_BITMASK:
		if (fl & V4L2_CTRL_FLAG_SLIDER)
			return fail("slider makes only sense for integer controls\n");
		fallthrough;
	case V4L2_CTRL_TYPE_INTEGER64:
	case V4L2_CTRL_TYPE_INTEGER:
		if ((fl & rw_mask) == rw_mask)
			return fail("can't read nor write this control\n");
		break;
	case V4L2_CTRL_TYPE_CTRL_CLASS:
		if (fl != (V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_WRITE_ONLY))
			return fail("invalid flags for control class\n");
		break;
	}
	if (fl & V4L2_CTRL_FLAG_GRABBED)
		return fail("GRABBED flag set\n");
	if (fl & V4L2_CTRL_FLAG_DISABLED)
		return fail("DISABLED flag set\n");
	if (qctrl.type != V4L2_CTRL_TYPE_MENU && qctrl.type != V4L2_CTRL_TYPE_INTEGER_MENU) {
		memset(&qmenu, 0xff, sizeof(qmenu));
		qmenu.id = qctrl.id;
		qmenu.index = qctrl.minimum;
		ret = doioctl(node, VIDIOC_QUERYMENU, &qmenu);
		if (ret != EINVAL && ret != ENOTTY)
			return fail("can do querymenu on a non-menu control\n");
		return 0;
	}
	bool have_default_value = false;
	for (i = 0; i <= qctrl.maximum + 1; i++) {
		memset(&qmenu, 0xff, sizeof(qmenu));
		qmenu.id = qctrl.id;
		qmenu.index = i;
		ret = doioctl(node, VIDIOC_QUERYMENU, &qmenu);
		if (ret && ret != EINVAL)
			return fail("invalid QUERYMENU return code (%d)\n", ret);
		if (ret)
			continue;
		if (i < qctrl.minimum || i > qctrl.maximum)
			return fail("can get menu for out-of-range index\n");
		if (qmenu.index != static_cast<__u32>(i) || qmenu.id != qctrl.id)
			return fail("id or index changed\n");
		if (qctrl.type == V4L2_CTRL_TYPE_MENU &&
		    check_ustring(qmenu.name, sizeof(qmenu.name)))
			return fail("invalid menu name\n");
		if (qmenu.reserved)
			return fail("reserved is non-zero\n");
		if (i == qctrl.default_value)
			have_default_value = true;
		if (i < 64)
			qctrl.menu_mask |= 1ULL << i;
	}
	if (qctrl.menu_mask == 0)
		return fail("no menu items found\n");
	fail_on_test(!have_default_value);
	return 0;
}

int testQueryExtControls(struct node *node)
{
	struct test_query_ext_ctrl qctrl;
	__u32 id = 0;
	int result = 0;
	int ret;
	__u32 which = 0;
	bool found_ctrl_class = false;
	unsigned user_controls = 0;
	unsigned priv_user_controls = 0;
	unsigned user_controls_check = 0;
	unsigned priv_user_controls_check = 0;
	unsigned class_count = 0;

	for (;;) {
		memset(&qctrl, 0xff, sizeof(qctrl));
		qctrl.id = id | V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
		ret = doioctl(node, VIDIOC_QUERY_EXT_CTRL, &qctrl);
		if (ret == ENOTTY)
			return ret;
		if (ret && ret != EINVAL)
			return fail("invalid query_ext_ctrl return code (%d)\n", ret);
		if (ret)
			break;
		if (checkQCtrl(node, qctrl))
			return fail("invalid control %08x\n", qctrl.id);
		if (qctrl.id <= id)
			return fail("id did not increase!\n");
		id = qctrl.id;
		if (id >= V4L2_CID_PRIVATE_BASE)
			return fail("no V4L2_CID_PRIVATE_BASE allowed\n");
		if (V4L2_CTRL_ID2WHICH(id) != which) {
			if (which && !found_ctrl_class)
				result = fail("missing control class for class %08x\n", which);
			if (which && !class_count)
				return fail("no controls in class %08x\n", which);
			which = V4L2_CTRL_ID2WHICH(id);
			found_ctrl_class = false;
			class_count = 0;
		}
		if (qctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS) {
			found_ctrl_class = true;
		} else {
			class_count++;
		}

		if (which == V4L2_CTRL_CLASS_USER &&
		    qctrl.type < V4L2_CTRL_COMPOUND_TYPES &&
		    !qctrl.nr_of_dims &&
		    qctrl.type != V4L2_CTRL_TYPE_INTEGER64 &&
		    qctrl.type != V4L2_CTRL_TYPE_STRING &&
		    qctrl.type != V4L2_CTRL_TYPE_CTRL_CLASS) {
			if (V4L2_CTRL_DRIVER_PRIV(id))
				priv_user_controls_check++;
			else if (id < V4L2_CID_LASTP1)
				user_controls_check++;
		}
		if (V4L2_CTRL_DRIVER_PRIV(id)) {
			node->priv_controls++;
			if (qctrl.type >= V4L2_CTRL_COMPOUND_TYPES || qctrl.nr_of_dims)
				node->priv_compound_controls++;
		} else {
			node->std_controls++;
			if (qctrl.type >= V4L2_CTRL_COMPOUND_TYPES || qctrl.nr_of_dims)
				node->std_compound_controls++;
		}
		node->controls[qctrl.id] = qctrl;
	}
	if (which && !found_ctrl_class &&
	    (priv_user_controls_check > node->priv_compound_controls ||
	     user_controls_check > node->std_compound_controls))
		result = fail("missing control class for class %08x\n", which);
	if (which && !class_count)
		return fail("no controls in class %08x\n", which);

	id = 0;
	unsigned regular_ctrls = 0;
	for (;;) {
		memset(&qctrl, 0xff, sizeof(qctrl));
		qctrl.id = id | V4L2_CTRL_FLAG_NEXT_CTRL;
		ret = doioctl(node, VIDIOC_QUERY_EXT_CTRL, &qctrl);
		if (ret)
			break;
		id = qctrl.id;
		if (qctrl.type >= V4L2_CTRL_COMPOUND_TYPES || qctrl.nr_of_dims)
			return fail("V4L2_CTRL_FLAG_NEXT_CTRL enumerates compound controls!\n");
		regular_ctrls++;
	}
	unsigned num_compound_ctrls = node->std_compound_controls + node->priv_compound_controls;
	unsigned num_regular_ctrls = node->std_controls + node->priv_controls - num_compound_ctrls;
	if (regular_ctrls != num_regular_ctrls)
		return fail("expected to find %d standard controls, got %d instead\n",
			    num_regular_ctrls, regular_ctrls);

	id = 0;
	unsigned compound_ctrls = 0;
	for (;;) {
		memset(&qctrl, 0xff, sizeof(qctrl));
		qctrl.id = id | V4L2_CTRL_FLAG_NEXT_COMPOUND;
		ret = doioctl(node, VIDIOC_QUERY_EXT_CTRL, &qctrl);
		if (ret)
			break;
		id = qctrl.id;
		if (qctrl.type < V4L2_CTRL_COMPOUND_TYPES && !qctrl.nr_of_dims)
			return fail("V4L2_CTRL_FLAG_NEXT_COMPOUND enumerates regular controls!\n");
		compound_ctrls++;
	}
	if (compound_ctrls != num_compound_ctrls)
		return fail("expected to find %d compound controls, got %d instead\n",
			    num_compound_ctrls, compound_ctrls);

	for (id = V4L2_CID_BASE; id < V4L2_CID_LASTP1; id++) {
		memset(&qctrl, 0xff, sizeof(qctrl));
		qctrl.id = id;
		ret = doioctl(node, VIDIOC_QUERY_EXT_CTRL, &qctrl);
		if (ret && ret != EINVAL)
			return fail("invalid query_ext_ctrl return code (%d)\n", ret);
		if (ret)
			continue;
		if (qctrl.id != id)
			return fail("qctrl.id (%08x) != id (%08x)\n",
					qctrl.id, id);
		if (checkQCtrl(node, qctrl))
			return fail("invalid control %08x\n", qctrl.id);
		user_controls++;
	}

	for (id = V4L2_CID_PRIVATE_BASE; ; id++) {
		memset(&qctrl, 0xff, sizeof(qctrl));
		qctrl.id = id;
		ret = doioctl(node, VIDIOC_QUERY_EXT_CTRL, &qctrl);
		if (ret && ret != EINVAL)
			return fail("invalid query_ext_ctrl return code (%d)\n", ret);
		if (ret)
			break;
		if (qctrl.id != id)
			return fail("qctrl.id (%08x) != id (%08x)\n",
					qctrl.id, id);
		if (checkQCtrl(node, qctrl))
			return fail("invalid control %08x\n", qctrl.id);
		priv_user_controls++;
	}

	if (priv_user_controls + user_controls && node->controls.empty())
		return fail("does not support V4L2_CTRL_FLAG_NEXT_CTRL\n");
	if (user_controls != user_controls_check)
		return fail("expected %d user controls, got %d\n",
			user_controls_check, user_controls);
	if (priv_user_controls != priv_user_controls_check)
		return fail("expected %d private controls, got %d\n",
			priv_user_controls_check, priv_user_controls);
	return result;
}

int testQueryControls(struct node *node)
{
	struct v4l2_queryctrl qctrl;
	unsigned controls = 0;
	unsigned compound_controls = 0;
	__u32 id = 0;
	int ret;

	unsigned num_compound_ctrls = node->std_compound_controls + node->priv_compound_controls;
	unsigned num_regular_ctrls = node->std_controls + node->priv_controls - num_compound_ctrls;

	for (;;) {
		memset(&qctrl, 0xff, sizeof(qctrl));
		qctrl.id = id | V4L2_CTRL_FLAG_NEXT_CTRL;
		ret = doioctl(node, VIDIOC_QUERYCTRL, &qctrl);
		if (ret == ENOTTY)
			return ret;
		if (ret && ret != EINVAL)
			return fail("invalid queryctrl return code (%d)\n", ret);
		if (ret)
			break;
		id = qctrl.id;
		fail_on_test(node->controls.find(qctrl.id) == node->controls.end());
		fail_on_test(qctrl.step < 0);
		controls++;
	}
	fail_on_test(controls != num_regular_ctrls);

	id = 0;
	for (;;) {
		memset(&qctrl, 0xff, sizeof(qctrl));
		qctrl.id = id | V4L2_CTRL_FLAG_NEXT_COMPOUND;
		ret = doioctl(node, VIDIOC_QUERYCTRL, &qctrl);
		if (ret)
			break;
		id = qctrl.id;
		fail_on_test(node->controls.find(qctrl.id) == node->controls.end());
		if (qctrl.type >= V4L2_CTRL_COMPOUND_TYPES)
			fail_on_test(qctrl.step || qctrl.minimum || qctrl.maximum || qctrl.default_value);
		compound_controls++;
	}
	fail_on_test(compound_controls != num_compound_ctrls);

	for (id = V4L2_CID_BASE; id < V4L2_CID_LASTP1; id++) {
		memset(&qctrl, 0xff, sizeof(qctrl));
		qctrl.id = id;
		ret = doioctl(node, VIDIOC_QUERYCTRL, &qctrl);
		if (ret && ret != EINVAL)
			return fail("invalid queryctrl return code (%d)\n", ret);
		if (ret)
			continue;
		if (qctrl.id != id)
			return fail("qctrl.id (%08x) != id (%08x)\n",
					qctrl.id, id);
		fail_on_test(qctrl.step < 0);
	}

	for (id = V4L2_CID_PRIVATE_BASE; ; id++) {
		memset(&qctrl, 0xff, sizeof(qctrl));
		qctrl.id = id;
		ret = doioctl(node, VIDIOC_QUERYCTRL, &qctrl);
		if (ret && ret != EINVAL)
			return fail("invalid queryctrl return code (%d)\n", ret);
		if (ret)
			break;
		if (qctrl.id != id)
			return fail("qctrl.id (%08x) != id (%08x)\n",
					qctrl.id, id);
		fail_on_test(qctrl.step < 0);
	}
	return 0;
}

static int checkSimpleCtrl(const struct v4l2_control &ctrl, const struct test_query_ext_ctrl &qctrl)
{
	if (ctrl.id != qctrl.id)
		return fail("control id mismatch\n");
	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_INTEGER:
	case V4L2_CTRL_TYPE_BOOLEAN:
	case V4L2_CTRL_TYPE_MENU:
	case V4L2_CTRL_TYPE_INTEGER_MENU:
		if (ctrl.value < qctrl.minimum || ctrl.value > qctrl.maximum)
			return fail("returned control value out of range\n");
		if ((ctrl.value - qctrl.minimum) % qctrl.step) {
			// This really should be a fail, but there are so few
			// drivers that do this right that I made it a warning
			// for now.
			warn("%s: returned control value %d not a multiple of step\n",
					qctrl.name, ctrl.value);
		}
		break;
	case V4L2_CTRL_TYPE_BITMASK:
		if (static_cast<__u32>(ctrl.value) & ~qctrl.maximum)
			return fail("returned control value out of range\n");
		break;
	case V4L2_CTRL_TYPE_BUTTON:
		break;
	default:
		return fail("this type should not allow g_ctrl\n");
	}
	return 0;
}

int testSimpleControls(struct node *node)
{
	struct v4l2_control ctrl;
	int ret;
	int i;

	for (auto &control : node->controls) {
		test_query_ext_ctrl &qctrl = control.second;

		if (qctrl.type >= V4L2_CTRL_COMPOUND_TYPES || qctrl.nr_of_dims)
			continue;
		if (is_vivid && V4L2_CTRL_ID2WHICH(qctrl.id) == V4L2_CTRL_CLASS_VIVID)
			continue;

		info("checking control '%s' (0x%08x)\n", qctrl.name, qctrl.id);
		ctrl.id = qctrl.id;
		if (qctrl.type == V4L2_CTRL_TYPE_INTEGER64 ||
		    qctrl.type == V4L2_CTRL_TYPE_STRING ||
		    qctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS) {
			ret = doioctl(node, VIDIOC_G_CTRL, &ctrl);
			if (ret != EINVAL &&
			    !((qctrl.flags & V4L2_CTRL_FLAG_WRITE_ONLY) && ret == EACCES))
				return fail("g_ctrl allowed for unsupported type\n");
			ctrl.id = qctrl.id;
			ctrl.value = 0;
			ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
			if (ret != EINVAL &&
			    !((qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) && ret == EACCES))
				return fail("s_ctrl allowed for unsupported type\n");
			continue;
		}

		// Get the current value
		ret = doioctl(node, VIDIOC_G_CTRL, &ctrl);
		if ((qctrl.flags & V4L2_CTRL_FLAG_WRITE_ONLY)) {
			if (ret != EACCES)
				return fail("g_ctrl did not check the write-only flag\n");
			ctrl.id = qctrl.id;
			ctrl.value = qctrl.default_value;
		} else if (ret)
			return fail("g_ctrl returned an error (%d)\n", ret);
		else if (checkSimpleCtrl(ctrl, qctrl))
			return fail("invalid control %08x\n", qctrl.id);

		// Try to set the current value (or the default value for write only controls)
		ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
		if (qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) {
			if (ret != EACCES)
				return fail("s_ctrl did not check the read-only flag\n");
		} else if (ret == EIO) {
			warn("s_ctrl returned EIO\n");
			ret = 0;
		} else if (ret == EILSEQ) {
			warn("s_ctrl returned EILSEQ\n");
			ret = 0;
		} else if (ret == EACCES && is_uvcvideo) {
			ret = 0;
		} else if (ret) {
			return fail("s_ctrl returned an error (%d)\n", ret);
		}
		if (ret)
			continue;
		if (checkSimpleCtrl(ctrl, qctrl))
			return fail("s_ctrl returned invalid control contents (%08x)\n", qctrl.id);

		// Try to set value 'minimum - 1'
		if (qctrl.minimum > -0x80000000LL && qctrl.minimum <= 0x7fffffffLL) {
			ctrl.id = qctrl.id;
			ctrl.value = qctrl.minimum - 1;
			ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
			if (ret && ret != EIO && ret != EILSEQ && ret != ERANGE &&
			    !(ret == EACCES && is_uvcvideo))
				return fail("invalid minimum range check\n");
			if (!ret && checkSimpleCtrl(ctrl, qctrl))
				return fail("invalid control %08x\n", qctrl.id);
		}
		// Try to set value 'maximum + 1'
		if (qctrl.maximum >= -0x80000000LL && qctrl.maximum < 0x7fffffffLL) {
			ctrl.id = qctrl.id;
			ctrl.value = qctrl.maximum + 1;
			ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
			if (ret && ret != EIO && ret != EILSEQ && ret != ERANGE &&
			    !(ret == EACCES && is_uvcvideo))
				return fail("invalid maximum range check\n");
			if (!ret && checkSimpleCtrl(ctrl, qctrl))
				return fail("invalid control %08x\n", qctrl.id);
		}
		// Try to set non-step value
		if (qctrl.step > 1 && qctrl.maximum > qctrl.minimum) {
			ctrl.id = qctrl.id;
			ctrl.value = qctrl.minimum + 1;
			ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
			if (ret == ERANGE)
				warn("%s: returns ERANGE for in-range, but non-step-multiple value\n",
						qctrl.name);
			else if (ret && ret != EIO && ret != EILSEQ &&
				 !(ret == EACCES && is_uvcvideo))
				return fail("returns error for in-range, but non-step-multiple value\n");
		}

		if (qctrl.type == V4L2_CTRL_TYPE_MENU || qctrl.type == V4L2_CTRL_TYPE_INTEGER_MENU) {
			int max = qctrl.maximum;

			/* Currently menu_mask is a 64-bit value, so we can't reliably
			 * test for menu item > 63. */
			if (max > 63)
				max = 63;
			// check menu items
			for (i = qctrl.minimum; i <= max; i++) {
				bool valid = qctrl.menu_mask & (1ULL << i);

				ctrl.id = qctrl.id;
				ctrl.value = i;
				ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
				if (valid && ret == EACCES && is_uvcvideo)
					continue;
				if (valid && ret)
					return fail("could not set valid menu item %d\n", i);
				if (!valid && !ret)
					return fail("could set invalid menu item %d\n", i);
				if (ret && ret != EINVAL)
					return fail("setting invalid menu item returned wrong error (%d)\n", ret);
			}
		} else {
			// at least min, max and default values should work
			ctrl.id = qctrl.id;
			ctrl.value = qctrl.minimum;
			ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
			if (ret && ret != EIO && ret != EILSEQ &&
			    !(ret == EACCES && is_uvcvideo))
				return fail("could not set minimum value\n");
			ctrl.value = qctrl.maximum;
			ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
			if (ret && ret != EIO && ret != EILSEQ &&
			    !(ret == EACCES && is_uvcvideo))
				return fail("could not set maximum value\n");
			ctrl.value = qctrl.default_value;
			ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
			if (ret && ret != EIO && ret != EILSEQ &&
			    !(ret == EACCES && is_uvcvideo))
				return fail("could not set default value\n");
		}
	}
	ctrl.id = 0;
	ret = doioctl(node, VIDIOC_G_CTRL, &ctrl);
	if (ret != EINVAL && ret != ENOTTY)
		return fail("g_ctrl accepted invalid control ID\n");
	ctrl.id = 0;
	ctrl.value = 0;
	ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
	if (ret != EINVAL && ret != ENOTTY)
		return fail("s_ctrl accepted invalid control ID\n");
	if (ret == ENOTTY && node->controls.empty())
		return ENOTTY;
	return 0;
}

static int checkExtendedCtrl(const struct v4l2_ext_control &ctrl, const struct test_query_ext_ctrl &qctrl)
{
	int len;

	if (ctrl.id != qctrl.id)
		return fail("control id mismatch\n");

	if (qctrl.flags & V4L2_CTRL_FLAG_DYNAMIC_ARRAY) {
		fail_on_test(qctrl.nr_of_dims != 1);
		unsigned tot_elems = qctrl.dims[0];
		fail_on_test(qctrl.elems > tot_elems);
		fail_on_test(!qctrl.elems);
	}
	if (qctrl.nr_of_dims)
		return 0;

	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_INTEGER:
	case V4L2_CTRL_TYPE_INTEGER64:
	case V4L2_CTRL_TYPE_BOOLEAN:
	case V4L2_CTRL_TYPE_MENU:
	case V4L2_CTRL_TYPE_INTEGER_MENU:
		if (ctrl.value < qctrl.minimum || ctrl.value > qctrl.maximum)
			return fail("returned control value out of range\n");
		if ((ctrl.value - qctrl.minimum) % qctrl.step) {
			// This really should be a fail, but there are so few
			// drivers that do this right that I made it a warning
			// for now.
			warn("%s: returned control value %d not a multiple of step\n",
					qctrl.name, ctrl.value);
		}
		break;
	case V4L2_CTRL_TYPE_BITMASK:
		if (static_cast<__u32>(ctrl.value) & ~qctrl.maximum)
			return fail("returned control value out of range\n");
		break;
	case V4L2_CTRL_TYPE_BUTTON:
		break;
	case V4L2_CTRL_TYPE_STRING:
		len = strnlen(ctrl.string, qctrl.maximum + 1);
		if (len == qctrl.maximum + 1)
			return fail("string too long\n");
		if (len < qctrl.minimum)
			return fail("string too short\n");
		if ((len - qctrl.minimum) % qctrl.step)
			return fail("string not a multiple of step\n");
		break;
	default:
		break;
	}
	return 0;
}

static int checkVividDynArray(struct node *node,
			      struct v4l2_ext_control &ctrl,
			      const struct test_query_ext_ctrl &qctrl)
{
	struct v4l2_query_ext_ctrl qextctrl = {};
	unsigned max_elems = qctrl.dims[0];
	unsigned max_size = qctrl.elem_size * max_elems;

	delete [] ctrl.string;
	// Allocate a buffer that's one element more than the max
	ctrl.string = new char[max_size + qctrl.elem_size];
	ctrl.size = qctrl.elem_size;
	ctrl.p_u32[0] = (qctrl.minimum + qctrl.maximum) / 2;
	// Set the last element + 1, must never be overwritten
	ctrl.p_u32[max_elems] = 0xdeadbeef;

	struct v4l2_ext_controls ctrls = {};
	ctrls.count = 1;
	ctrls.controls = &ctrl;
	// Set the array to a single element
	fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls));

	qextctrl.id = ctrl.id;
	// Check that only one element is reported
	fail_on_test(doioctl(node, VIDIOC_QUERY_EXT_CTRL, &qextctrl));
	fail_on_test(qextctrl.elems != 1);

	// If the size is less than elem_size, the ioctl must return -EFAULT
	ctrl.size = 0;
	fail_on_test(doioctl(node, VIDIOC_TRY_EXT_CTRLS, &ctrls) != EFAULT);
	ctrl.size = qctrl.elem_size - 1;
	fail_on_test(doioctl(node, VIDIOC_TRY_EXT_CTRLS, &ctrls) != EFAULT);
	ctrl.size = max_size + qctrl.elem_size;
	fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls));
	// Check that ctrl.size is reset to the current size of the array (1 element)
	fail_on_test(ctrl.size != qctrl.elem_size);
	ctrl.size = max_size + qctrl.elem_size;
	// Attempting to set more than max_elems must result in -ENOSPC
	fail_on_test(doioctl(node, VIDIOC_TRY_EXT_CTRLS, &ctrls) != ENOSPC);
	fail_on_test(ctrl.size != max_size);
	ctrl.size = max_size + qctrl.elem_size;
	fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls) != ENOSPC);
	fail_on_test(ctrl.size != max_size);
	fail_on_test(doioctl(node, VIDIOC_QUERY_EXT_CTRL, &qextctrl));
	// Verify that the number of elements is still 1
	fail_on_test(qextctrl.elems != 1);

	ctrl.size = max_size;
	for (unsigned i = 0; i < max_elems; i++)
		ctrl.p_u32[i] = i;
	// Try the max number of elements
	fail_on_test(doioctl(node, VIDIOC_TRY_EXT_CTRLS, &ctrls));
	// Check that the values are clamped
	for (unsigned i = 0; i < max_elems; i++) {
		unsigned j = i;
		if (j < qctrl.minimum)
			j = qctrl.minimum;
		else if (j > qctrl.maximum)
			j = qctrl.maximum;
		fail_on_test(ctrl.p_u32[i] != j);
	}
	fail_on_test(ctrl.size != max_size);
	fail_on_test(doioctl(node, VIDIOC_QUERY_EXT_CTRL, &qextctrl));
	// Verify that the number of elements is still 1
	fail_on_test(qextctrl.elems != 1);
	memset(ctrl.string, 0xff, max_size);
	fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls));
	// Check that there is still just one element returned.
	fail_on_test(ctrl.size != qctrl.elem_size);
	fail_on_test(ctrl.p_u32[0] != (qctrl.minimum + qctrl.maximum) / 2);

	ctrl.size = max_size;
	for (unsigned i = 0; i < max_elems; i++)
		ctrl.p_u32[i] = i;
	// Set the max number of elements
	fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls));
	for (unsigned i = 0; i < max_elems; i++) {
		unsigned j = i;
		if (j < qctrl.minimum)
			j = qctrl.minimum;
		else if (j > qctrl.maximum)
			j = qctrl.maximum;
		fail_on_test(ctrl.p_u32[i] != j);
	}
	fail_on_test(doioctl(node, VIDIOC_QUERY_EXT_CTRL, &qextctrl));
	// Check that it is updated
	fail_on_test(qextctrl.elems != max_elems);
	memset(ctrl.string, 0xff, max_size);
	ctrl.size = qctrl.elem_size;
	// Check that ENOSPC is returned if the size is too small
	fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls) != ENOSPC);
	// And updated to the actual required size
	fail_on_test(ctrl.size != max_size);
	fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls));
	for (unsigned i = 0; i < max_elems; i++) {
		unsigned j = i;
		if (j < qctrl.minimum)
			j = qctrl.minimum;
		else if (j > qctrl.maximum)
			j = qctrl.maximum;
		fail_on_test(ctrl.p_u32[i] != j);
	}
	// Check that the end of the buffer isn't overwritten
	fail_on_test(ctrl.p_u32[max_elems] != 0xdeadbeef);

	ctrl.size = qctrl.elem_size;
	ctrls.which = V4L2_CTRL_WHICH_DEF_VAL;
	// Check that ENOSPC is returned if the size is too small
	fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls) != ENOSPC);
	// And updated to the actual required size
	fail_on_test(ctrl.size != max_size);
	fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls));
	for (unsigned i = 0; i < max_elems; i++)
		fail_on_test(ctrl.p_u32[i] != 50);
	// Check that the end of the buffer isn't overwritten
	fail_on_test(ctrl.p_u32[max_elems] != 0xdeadbeef);

	ctrls.which = 0;
	ctrl.size = qctrl.elem_size;
	ctrl.p_u32[0] = (qctrl.minimum + qctrl.maximum) / 2;
	// Back to just one element
	fail_on_test(doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls));
	fail_on_test(doioctl(node, VIDIOC_QUERY_EXT_CTRL, &qextctrl));
	// Check this.
	fail_on_test(qextctrl.elems != 1);

	ctrl.size = max_size;
	ctrls.which = V4L2_CTRL_WHICH_DEF_VAL;
	memset(ctrl.string, 0xff, max_size);
	// And updated to the actual required size
	fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls));
	fail_on_test(ctrl.size != qctrl.elem_size);
	fail_on_test(ctrl.p_u32[0] != 50);
	fail_on_test(ctrl.p_u32[1] != 0xffffffff);

	return 0;
}

static int checkVividControls(struct node *node,
			      struct v4l2_ext_control &ctrl,
			      struct v4l2_ext_controls &ctrls,
			      const struct test_query_ext_ctrl &qctrl)
{
	int ret;

	switch (ctrl.id) {
	case VIVID_CID_INTEGER64:
		ctrl.value64 = qctrl.minimum;
		ret = doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls);
		if (ret)
			return fail("could not set minimum value\n");
		ctrl.value64 = qctrl.maximum;
		ret = doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls);
		if (ret)
			return fail("could not set maximum value\n");
		ctrl.value64 = qctrl.default_value;
		ret = doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls);
		if (ret)
			return fail("could not set default value\n");
		return 0;

	case VIVID_CID_STRING:
		delete [] ctrl.string;
		ctrl.string = new char[qctrl.maximum + 1];
		memset(ctrl.string, 'A', qctrl.minimum);
		ctrl.string[qctrl.minimum] = '\0';
		ctrl.size = 3;
		ret = doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls);
		if (ret)
			return fail("could not set minimum string value\n");
		memset(ctrl.string, 'Z', qctrl.maximum);
		ctrl.string[qctrl.maximum] = '\0';
		ctrl.size = 5;
		ret = doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls);
		if (ret)
			return fail("could not set maximum string value\n");
		memset(ctrl.string, ' ', qctrl.minimum);
		ctrl.string[qctrl.minimum] = '\0';
		ctrl.size = 3;
		ret = doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls);
		if (ret)
			return fail("could not set default string value\n");
		return 0;

	case VIVID_CID_AREA:
		ctrl.p_area->width = 200;
		ctrl.p_area->height = 100;
		ret = doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls);
		if (ret)
			return fail("could not set area value\n");
		ctrl.p_area->width = 1000;
		ctrl.p_area->height = 2000;
		ret = doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls);
		if (ret)
			return fail("could not set area value\n");
		return 0;

	default:
		return fail("should not happen\n");
	}
}

int testExtendedControls(struct node *node)
{
	struct v4l2_ext_controls ctrls;
	std::vector<struct v4l2_ext_control> total_vec;
	std::vector<struct v4l2_ext_control> class_vec;
	struct v4l2_ext_control ctrl;
	__u32 which = 0;
	bool multiple_classes = false;
	int ret;

	memset(&ctrls, 0, sizeof(ctrls));
	ret = doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls);
	if (ret == ENOTTY && node->controls.empty())
		return ret;
	if (ret)
		return fail("g_ext_ctrls does not support count == 0\n");
	if (ctrls.which)
		return fail("field which changed\n");
	if (ctrls.count)
		return fail("field count changed\n");
	if (check_0(ctrls.reserved, sizeof(ctrls.reserved)))
		return fail("reserved not zeroed\n");

	memset(&ctrls, 0, sizeof(ctrls));
	ret = doioctl(node, VIDIOC_TRY_EXT_CTRLS, &ctrls);
	if (ret == ENOTTY && node->controls.empty())
		return ret;
	if (ret)
		return fail("try_ext_ctrls does not support count == 0\n");
	if (ctrls.which)
		return fail("field which changed\n");
	if (ctrls.count)
		return fail("field count changed\n");
	if (check_0(ctrls.reserved, sizeof(ctrls.reserved)))
		return fail("reserved not zeroed\n");

	for (auto &control : node->controls) {
		test_query_ext_ctrl &qctrl = control.second;

		if (is_vivid && V4L2_CTRL_ID2WHICH(qctrl.id) == V4L2_CTRL_CLASS_VIVID)
			continue;

		info("checking extended control '%s' (0x%08x)\n", qctrl.name, qctrl.id);
		ctrl.id = qctrl.id;
		ctrl.size = 0;
		ctrl.ptr = nullptr;
		ctrl.reserved2[0] = 0;
		ctrls.count = 1;

		// Either should work, so try both semi-randomly
		ctrls.which = (ctrl.id & 1) ? 0 : V4L2_CTRL_ID2WHICH(ctrl.id);
		ctrls.controls = &ctrl;

		// Get the current value
		ret = doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls);
		if ((qctrl.flags & V4L2_CTRL_FLAG_WRITE_ONLY)) {
			if (ret != EACCES)
				return fail("g_ext_ctrls did not check the write-only flag\n");
			if (ctrls.error_idx != ctrls.count)
				return fail("invalid error index write only control\n");
			ctrl.id = qctrl.id;
			ctrl.value = qctrl.default_value;
		} else {
			if (ret != ENOSPC && (qctrl.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD))
				return fail("did not check against size\n");
			if (ret == ENOSPC && (qctrl.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD)) {
				if (qctrl.type == V4L2_CTRL_TYPE_STRING && ctrls.error_idx != 0)
					return fail("invalid error index string control\n");
				fail_on_test(ctrl.size != qctrl.elem_size * qctrl.elems);
				ctrl.string = new char[ctrl.size];
				ret = doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls);
			}
			if (ret == EIO) {
				warn("g_ext_ctrls returned EIO\n");
				ret = 0;
			} else if (ret == EILSEQ) {
				warn("g_ext_ctrls returned EILSEQ\n");
				ret = 0;
			}
			if (ret)
				return fail("g_ext_ctrls returned an error (%d)\n", ret);
			if (checkExtendedCtrl(ctrl, qctrl))
				return fail("invalid control %08x\n", qctrl.id);
		}

		// Try the current value (or the default value for write only controls)
		ret = doioctl(node, VIDIOC_TRY_EXT_CTRLS, &ctrls);
		if (qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) {
			if (ret != EACCES)
				return fail("try_ext_ctrls did not check the read-only flag\n");
			if (ctrls.error_idx != 0)
				return fail("invalid error index read only control\n");
		} else if (ret) {
			return fail("try_ext_ctrls returned an error (%d)\n", ret);
		}

		// Try to set the current value (or the default value for write only controls)
		ret = doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls);
		if (qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) {
			if (ret != EACCES)
				return fail("s_ext_ctrls did not check the read-only flag\n");
			if (ctrls.error_idx != ctrls.count)
				return fail("invalid error index\n");
		} else {
			if (ret == EIO) {
				warn("s_ext_ctrls returned EIO\n");
				ret = 0;
			} else if (ret == EILSEQ) {
				warn("s_ext_ctrls returned EILSEQ\n");
				ret = 0;
			} else if (ret == EACCES && is_uvcvideo) {
				ret = 0;
			}
			if (ret)
				return fail("s_ext_ctrls returned an error (%d)\n", ret);

			if (checkExtendedCtrl(ctrl, qctrl))
				return fail("s_ext_ctrls returned invalid control contents (%08x)\n", qctrl.id);
		}

		if (is_vivid && (ctrl.id == VIVID_CID_INTEGER64 ||
				 ctrl.id == VIVID_CID_STRING ||
				 ctrl.id == VIVID_CID_AREA) &&
		    checkVividControls(node, ctrl, ctrls, qctrl))
			return fail("vivid control tests failed\n");
		if (is_vivid && ctrl.id == VIVID_CID_U32_DYN_ARRAY &&
		    checkVividDynArray(node, ctrl, qctrl))
			return fail("dynamic array tests failed\n");
		if (qctrl.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD)
			delete [] ctrl.string;
		ctrl.string = nullptr;
	}

	ctrls.which = 0;
	ctrls.count = 1;
	ctrls.controls = &ctrl;
	ctrl.id = 0;
	ctrl.size = 0;
	ret = doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls);
	if (ret != EINVAL)
		return fail("g_ext_ctrls accepted invalid control ID\n");
	fail_on_test(ctrls.error_idx > ctrls.count);
	if (ctrls.error_idx != ctrls.count)
		warn("g_ext_ctrls(0) invalid error_idx %u\n", ctrls.error_idx);
	ctrl.id = 0;
	ctrl.size = 0;
	ctrl.value = 0;
	ret = doioctl(node, VIDIOC_TRY_EXT_CTRLS, &ctrls);
	if (ret != EINVAL)
		return fail("try_ext_ctrls accepted invalid control ID\n");
	if (ctrls.error_idx != 0)
		return fail("try_ext_ctrls(0) invalid error_idx %u\n", ctrls.error_idx);
	ret = doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls);
	if (ret != EINVAL)
		return fail("s_ext_ctrls accepted invalid control ID\n");
	if (ctrls.error_idx != ctrls.count)
		return fail("s_ext_ctrls(0) invalid error_idx %u\n", ctrls.error_idx);

	for (auto &control : node->controls) {
		test_query_ext_ctrl &qctrl = control.second;
		struct v4l2_ext_control ctrl;

		if (qctrl.flags & (V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_WRITE_ONLY))
			continue;

		ctrl.id = qctrl.id;
		ctrl.size = 0;
		if (qctrl.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD) {
			ctrl.size = qctrl.elems * qctrl.elem_size;
			ctrl.string = new char[ctrl.size];
		}
		ctrl.reserved2[0] = 0;
		if (!which)
			which = V4L2_CTRL_ID2WHICH(ctrl.id);
		else if (which != V4L2_CTRL_ID2WHICH(ctrl.id))
			multiple_classes = true;
		total_vec.push_back(ctrl);
	}
	if (total_vec.empty())
		return 0;

	ctrls.count = total_vec.size();
	ctrls.controls = &total_vec[0];
	ret = doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls);
	if (ret)
		return fail("could not get all controls\n");
	ret = doioctl(node, VIDIOC_TRY_EXT_CTRLS, &ctrls);
	if (ret)
		return fail("could not try all controls\n");
	ret = doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls);
	if (ret == EIO) {
		warn("s_ext_ctrls returned EIO\n");
		ret = 0;
	} else if (ret == EILSEQ) {
		warn("s_ext_ctrls returned EILSEQ\n");
		ret = 0;
	} else if (ret == EACCES && is_uvcvideo) {
		ret = 0;
	}
	if (ret)
		return fail("could not set all controls\n");

	if (!which)
		return 0;

	ctrls.which = which;
	ret = doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls);
	if (ret && !multiple_classes)
		return fail("could not get all controls of a specific class\n");
	if (ret != EINVAL && multiple_classes)
		return fail("should get EINVAL when getting mixed-class controls\n");
	if (multiple_classes && ctrls.error_idx != ctrls.count)
		warn("error_idx should be equal to count\n");
	ret = doioctl(node, VIDIOC_TRY_EXT_CTRLS, &ctrls);
	if (ret && !multiple_classes)
		return fail("could not try all controls of a specific class\n");
	if (ret != EINVAL && multiple_classes)
		return fail("should get EINVAL when trying mixed-class controls\n");
	if (multiple_classes && ctrls.error_idx >= ctrls.count)
		return fail("error_idx should be < count\n");
	ret = doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls);
	if (ret == EIO) {
		warn("s_ext_ctrls returned EIO\n");
		ret = 0;
	} else if (ret == EILSEQ) {
		warn("s_ext_ctrls returned EILSEQ\n");
		ret = 0;
	}
	if (ret && !(ret == EACCES && is_uvcvideo) && !multiple_classes)
		return fail("could not set all controls of a specific class\n");
	if (ret != EINVAL && multiple_classes)
		return fail("should get EINVAL when setting mixed-class controls\n");
	if (multiple_classes && ctrls.error_idx != ctrls.count)
		warn("error_idx should be equal to count\n");

	ctrls.which = V4L2_CTRL_WHICH_DEF_VAL;
	fail_on_test(!doioctl(node, VIDIOC_S_EXT_CTRLS, &ctrls));
	fail_on_test(!doioctl(node, VIDIOC_TRY_EXT_CTRLS, &ctrls));
	fail_on_test(doioctl(node, VIDIOC_G_EXT_CTRLS, &ctrls));
	return 0;
}

int testEvents(struct node *node)
{
	for (auto &control : node->controls) {
		test_query_ext_ctrl &qctrl = control.second;
		struct v4l2_event_subscription sub = { 0 };
		struct v4l2_event ev;
		struct timeval timeout = { 0, 100 };
		fd_set set;
		int ret;

		info("checking control event '%s' (0x%08x)\n", qctrl.name, qctrl.id);
		sub.type = V4L2_EVENT_CTRL;
		sub.id = qctrl.id;
		sub.flags = V4L2_EVENT_SUB_FL_SEND_INITIAL;
		ret = doioctl(node, VIDIOC_SUBSCRIBE_EVENT, &sub);
		if (ret)
			return fail("subscribe event for control '%s' failed\n", qctrl.name);
		//if (qctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS)
		FD_ZERO(&set);
		FD_SET(node->g_fd(), &set);
		ret = select(node->g_fd() + 1, nullptr, nullptr, &set, &timeout);
		if (ret == 0) {
			if (qctrl.type != V4L2_CTRL_TYPE_CTRL_CLASS)
				return fail("failed to find event for control '%s'\n", qctrl.name);
		} else if (qctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS) {
			return fail("found event for control class '%s'\n", qctrl.name);
		}
		if (ret) {
			ret = doioctl(node, VIDIOC_DQEVENT, &ev);
			if (ret)
				return fail("couldn't get event for control '%s'\n", qctrl.name);
			if (ev.type != V4L2_EVENT_CTRL || ev.id != qctrl.id)
				return fail("dequeued wrong event\n");
			fail_on_test(check_0(ev.reserved, sizeof(ev.reserved)));
		}
		ret = doioctl(node, VIDIOC_UNSUBSCRIBE_EVENT, &sub);
		if (ret)
			return fail("unsubscribe event for control '%s' failed\n", qctrl.name);
	}
	if (node->cur_io_caps & V4L2_IN_CAP_DV_TIMINGS) {
		struct v4l2_event_subscription sub = { };
		int id;

		if (node->can_capture) {
			fail_on_test(doioctl(node, VIDIOC_G_INPUT, &id));
			if (node->is_video &&
			    node->controls.find(V4L2_CID_DV_RX_POWER_PRESENT) == node->controls.end())
				warn("V4L2_CID_DV_RX_POWER_PRESENT not found for input %d\n", id);
		} else {
			fail_on_test(doioctl(node, VIDIOC_G_OUTPUT, &id));
			if (node->is_video &&
			    node->controls.find(V4L2_CID_DV_TX_HOTPLUG) == node->controls.end())
				warn("V4L2_CID_DV_TX_HOTPLUG not found for output %d\n", id);
			if (node->is_video &&
			    node->controls.find(V4L2_CID_DV_TX_EDID_PRESENT) == node->controls.end())
				warn("V4L2_CID_DV_TX_EDID_PRESENT not found for output %d\n", id);
		}
		if (node->can_capture) {
			sub.type = V4L2_EVENT_SOURCE_CHANGE;
			sub.id = id;
			fail_on_test(doioctl(node, VIDIOC_SUBSCRIBE_EVENT, &sub));
			fail_on_test(doioctl(node, VIDIOC_UNSUBSCRIBE_EVENT, &sub));
		}
	}

	if (node->codec_mask & STATEFUL_ENCODER)
		fail_on_test(node->controls.find(V4L2_CID_MIN_BUFFERS_FOR_OUTPUT) == node->controls.end());

	struct v4l2_event_subscription sub = { 0 };

	sub.type = V4L2_EVENT_EOS;
	bool have_eos = !doioctl(node, VIDIOC_SUBSCRIBE_EVENT, &sub);
	if (have_eos)
		fail_on_test(doioctl(node, VIDIOC_UNSUBSCRIBE_EVENT, &sub));
	else
		fail_on_test(node->codec_mask & STATEFUL_ENCODER);
	sub.type = V4L2_EVENT_SOURCE_CHANGE;
	bool have_source_change = !doioctl(node, VIDIOC_SUBSCRIBE_EVENT, &sub);
	if (have_source_change)
		fail_on_test(doioctl(node, VIDIOC_UNSUBSCRIBE_EVENT, &sub));

	switch (node->codec_mask) {
	case STATEFUL_ENCODER:
		fail_on_test(have_source_change || !have_eos);
		break;
	case STATEFUL_DECODER:
		fail_on_test(!have_source_change || !have_eos);
		break;
	case STATELESS_ENCODER:
	case STATELESS_DECODER:
		fail_on_test(have_source_change || have_eos);
		break;
	default:
		break;
	}
	if (node->can_output && !node->is_m2m)
		fail_on_test(have_source_change);

	if (node->controls.empty())
		return ENOTTY;
	return 0;
}

int testVividDisconnect(struct node *node)
{
	// Test that disconnecting a device will wake up any processes
	// that are using select or poll.
	//
	// This can only be tested with the vivid driver that enabled
	// the DISCONNECT control.

	pid_t pid = fork();
	if (pid == 0) {
		struct timeval tv = { 5, 0 };
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(node->g_fd(), &fds);
		int res = select(node->g_fd() + 1, nullptr, nullptr, &fds, &tv);
		// No POLLPRI seen
		if (res != 1)
			exit(1);
		// POLLPRI seen, but didn't wake up
		if (!tv.tv_sec)
			exit(2);
		v4l2_event ev = {};
		// Woke up on POLLPRI, but VIDIOC_DQEVENT didn't return
		// the ENODEV error.
		if (doioctl(node, VIDIOC_DQEVENT, &ev) != ENODEV)
			exit(3);
		exit(0);
	}
	v4l2_control ctrl = { VIVID_CID_DISCONNECT, 0 };
	sleep(1);
	fail_on_test(doioctl(node, VIDIOC_S_CTRL, &ctrl));
	int wstatus;
	fail_on_test(waitpid(pid, &wstatus, 0) != pid);
	fail_on_test(!WIFEXITED(wstatus));
	if (WEXITSTATUS(wstatus))
		return fail("select child exited with status %d\n", WEXITSTATUS(wstatus));

	node->reopen();

	pid = fork();
	if (pid == 0) {
		struct epoll_event ep_ev;
		int epollfd = epoll_create1(0);

		if (epollfd < 0)
			exit(1);

		ep_ev.events = 0;
		if (epoll_ctl(epollfd, EPOLL_CTL_ADD, node->g_fd(), &ep_ev))
			exit(2);

		ep_ev.events = EPOLLPRI;
		ep_ev.data.fd = node->g_fd();
		if (epoll_ctl(epollfd, EPOLL_CTL_MOD, node->g_fd(), &ep_ev))
			exit(3);
		int ret = epoll_wait(epollfd, &ep_ev, 1, 5000);
		if (ret == 0)
			exit(4);
		if (ret < 0)
			exit(5);
		if (ret != 1)
			exit(6);
		if (!(ep_ev.events & EPOLLPRI))
			exit(7);
		if (!(ep_ev.events & EPOLLERR))
			exit(8);
		v4l2_event ev = {};
		// Woke up on POLLPRI, but VIDIOC_DQEVENT didn't return
		// the ENODEV error.
		if (doioctl(node, VIDIOC_DQEVENT, &ev) != ENODEV)
			exit(9);
		exit(0);
	}
	sleep(1);
	fail_on_test(doioctl(node, VIDIOC_S_CTRL, &ctrl));
	fail_on_test(waitpid(pid, &wstatus, 0) != pid);
	fail_on_test(!WIFEXITED(wstatus));
	if (WEXITSTATUS(wstatus))
		return fail("epoll child exited with status %d\n", WEXITSTATUS(wstatus));
	return 0;
}

int testJpegComp(struct node *node)
{
	struct v4l2_jpegcompression jc;
	bool have_jpegcomp = false;
	const unsigned all_markers =
		V4L2_JPEG_MARKER_DHT | V4L2_JPEG_MARKER_DQT |
		V4L2_JPEG_MARKER_DRI | V4L2_JPEG_MARKER_COM |
		V4L2_JPEG_MARKER_APP;
	int ret;

	memset(&jc, 0, sizeof(jc));
	ret = doioctl(node, VIDIOC_G_JPEGCOMP, &jc);
	if (ret != ENOTTY) {
		warn("The VIDIOC_G_JPEGCOMP ioctl is deprecated!\n");
		if (ret)
			return fail("VIDIOC_G_JPEGCOMP gave an error\n");
		have_jpegcomp = true;
		if (jc.COM_len < 0 || jc.COM_len > static_cast<int>(sizeof(jc.COM_data)))
			return fail("invalid COM_len value\n");
		if (jc.APP_len < 0 || jc.APP_len > static_cast<int>(sizeof(jc.APP_data)))
			return fail("invalid APP_len value\n");
		if (jc.quality < 0 || jc.quality > 100)
			warn("weird quality value: %d\n", jc.quality);
		if (jc.APPn < 0 || jc.APPn > 15)
			return fail("invalid APPn value (%d)\n", jc.APPn);
		if (jc.jpeg_markers & ~all_markers)
			return fail("invalid markers (%x)\n", jc.jpeg_markers);
	}
	ret = doioctl(node, VIDIOC_S_JPEGCOMP, &jc);
	if (ret != ENOTTY) {
		warn("The VIDIOC_S_JPEGCOMP ioctl is deprecated!\n");
		if (ret && ret != EINVAL)
			return fail("VIDIOC_S_JPEGCOMP gave an error\n");
		have_jpegcomp = true;
	}

	return have_jpegcomp ? ret : ENOTTY;
}
