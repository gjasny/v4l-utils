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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "v4l2-compliance.h"

static int checkQCtrl(struct node *node, struct test_queryctrl &qctrl)
{
	struct v4l2_querymenu qmenu;
	__u32 fl = qctrl.flags;
	__u32 rw_mask = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_WRITE_ONLY;
	int ret;
	int i;

	qctrl.menu_mask = 0;
	if (check_ustring(qctrl.name, sizeof(qctrl.name)))
		return fail("invalid name\n");
	info("checking v4l2_queryctrl of control '%s' (0x%08x)\n", qctrl.name, qctrl.id);
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
		/* fall through */
	case V4L2_CTRL_TYPE_MENU:
		if (qctrl.step != 1)
			return fail("invalid step value %d\n", qctrl.step);
		if (qctrl.minimum < 0)
			return fail("min < 0\n");
		/* fall through */
	case V4L2_CTRL_TYPE_INTEGER:
		if (qctrl.default_value < qctrl.minimum ||
		    qctrl.default_value > qctrl.maximum)
			return fail("def < min || def > max\n");
		/* fall through */
	case V4L2_CTRL_TYPE_STRING:
		if (qctrl.minimum > qctrl.maximum)
			return fail("min > max\n");
		if (qctrl.step == 0)
			return fail("step == 0\n");
		if (qctrl.step < 0)
			return fail("step < 0\n");
		if ((unsigned)qctrl.step > (unsigned)(qctrl.maximum - qctrl.minimum))
			return fail("step > max - min\n");
		if ((qctrl.maximum - qctrl.minimum) % qctrl.step) {
			// This really should be a fail, but there are so few
			// drivers that do this right that I made it a warning
			// for now.
			warn("(max - min) %% step != 0\n");
		}
		break;
	case V4L2_CTRL_TYPE_CTRL_CLASS:
	case V4L2_CTRL_TYPE_INTEGER64:
	case V4L2_CTRL_TYPE_BUTTON:
		if (qctrl.minimum || qctrl.maximum || qctrl.step || qctrl.default_value)
			return fail("non-zero min/max/step/def\n");
		break;
	default:
		return fail("unknown control type\n");
	}
	if (qctrl.type == V4L2_CTRL_TYPE_STRING && qctrl.default_value)
		return fail("non-zero default value for string\n");
	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_BUTTON:
		if ((fl & rw_mask) != V4L2_CTRL_FLAG_WRITE_ONLY)
			return fail("button control not write only\n");
		/* fall through */
	case V4L2_CTRL_TYPE_BOOLEAN:
	case V4L2_CTRL_TYPE_MENU:
	case V4L2_CTRL_TYPE_STRING:
		if (fl & V4L2_CTRL_FLAG_SLIDER)
			return fail("slider makes only sense for integer controls\n");
		/* fall through */
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
	if (qctrl.type != V4L2_CTRL_TYPE_MENU) {
		memset(&qmenu, 0xff, sizeof(qmenu));
		qmenu.id = qctrl.id;
		qmenu.index = qctrl.minimum;
		ret = doioctl(node, VIDIOC_QUERYMENU, &qmenu);
		if (ret != EINVAL)
			return fail("can do querymenu on a non-menu control\n");
		return 0;
	}
	if (qctrl.maximum >= 32)
		return fail("currently more than 32 menu items are not supported\n");
	for (i = 0; i <= qctrl.maximum + 1; i++) {
		memset(&qmenu, 0xff, sizeof(qmenu));
		qmenu.id = qctrl.id;
		qmenu.index = i;
		ret = doioctl(node, VIDIOC_QUERYMENU, &qmenu);
		if (ret && ret != EINVAL)
			return fail("invalid QUERYMENU return code\n");
		if (ret)
			continue;
		if (i < qctrl.minimum || i > qctrl.maximum)
			return fail("can get menu for out-of-range index\n");
		if (qmenu.index != (__u32)i || qmenu.id != qctrl.id)
			return fail("id or index changed\n");
		if (check_ustring(qmenu.name, sizeof(qmenu.name)))
			return fail("invalid menu name\n");
		if (qmenu.reserved)
			return fail("reserved is non-zero\n");
		qctrl.menu_mask |= 1 << i;
	}
	if (qctrl.menu_mask == 0)
		return fail("no menu items found\n");
	if (!(qctrl.menu_mask & (1 << qctrl.default_value)))
		return fail("the default_value is an invalid menu item\n");
	return 0;
}

int testQueryControls(struct node *node)
{
	struct test_queryctrl qctrl;
	__u32 id = 0;
	int ret;
	__u32 ctrl_class = 0;
	bool found_ctrl_class = false;
	unsigned user_controls = 0;
	unsigned priv_user_controls = 0;
	unsigned user_controls_check = 0;
	unsigned priv_user_controls_check = 0;
	unsigned class_count = 0;

	for (;;) {
		memset(&qctrl, 0xff, sizeof(qctrl));
		qctrl.id = id | V4L2_CTRL_FLAG_NEXT_CTRL;
		ret = doioctl(node, VIDIOC_QUERYCTRL, &qctrl);
		if (ret && ret != EINVAL)
			return fail("invalid queryctrl return code\n");
		if (ret && id == 0)
			return fail("does not support V4L2_CTRL_FLAG_NEXT_CTRL\n");
		if (ret)
			break;
		if (checkQCtrl(node, qctrl))
			return fail("invalid control %08x\n", qctrl.id);
		if (qctrl.id <= id)
			return fail("id did not increase!\n");
		id = qctrl.id;
		if (id >= V4L2_CID_PRIVATE_BASE)
			return fail("no V4L2_CID_PRIVATE_BASE allowed\n");
		if (V4L2_CTRL_ID2CLASS(id) != ctrl_class) {
			if (ctrl_class && !found_ctrl_class)
				return fail("missing control class for class %08x\n", ctrl_class);
			if (ctrl_class && !class_count)
				return fail("no controls in class %08x\n", ctrl_class);
			ctrl_class = V4L2_CTRL_ID2CLASS(id);
			found_ctrl_class = false;
			class_count = 0;
		}
		if (qctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS) {
			found_ctrl_class = true;
		} else {
			class_count++;
		}

		if (ctrl_class == V4L2_CTRL_CLASS_USER &&
		    qctrl.type != V4L2_CTRL_TYPE_INTEGER64 &&
		    qctrl.type != V4L2_CTRL_TYPE_STRING &&
		    qctrl.type != V4L2_CTRL_TYPE_CTRL_CLASS) {
			if (V4L2_CTRL_DRIVER_PRIV(id))
				priv_user_controls_check++;
			else if (id < V4L2_CID_LASTP1)
				user_controls_check++;
		}
		if (V4L2_CTRL_DRIVER_PRIV(id))
			node->priv_controls++;
		else
			node->std_controls++;
		node->controls.push_back(qctrl);
	}
	if (ctrl_class && !found_ctrl_class)
		return fail("missing control class for class %08x\n", ctrl_class);
	if (ctrl_class && !class_count)
		return fail("no controls in class %08x\n", ctrl_class);

	for (id = V4L2_CID_BASE; id < V4L2_CID_LASTP1; id++) {
		memset(&qctrl, 0xff, sizeof(qctrl));
		qctrl.id = id;
		ret = doioctl(node, VIDIOC_QUERYCTRL, &qctrl);
		if (ret && ret != EINVAL)
			return fail("invalid queryctrl return code\n");
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
		ret = doioctl(node, VIDIOC_QUERYCTRL, &qctrl);
		if (ret && ret != EINVAL)
			return fail("invalid queryctrl return code\n");
		if (ret)
			break;
		if (qctrl.id != id)
			return fail("qctrl.id (%08x) != id (%08x)\n",
					qctrl.id, id);
		if (checkQCtrl(node, qctrl))
			return fail("invalid control %08x\n", qctrl.id);
		priv_user_controls++;
	}

	if (user_controls != user_controls_check)
		return fail("expected %d user controls, got %d\n",
			user_controls_check, user_controls);
	if (priv_user_controls != priv_user_controls_check)
		return fail("expected %d private controls, got %d\n",
			priv_user_controls_check, priv_user_controls);
	return 0;
}

static int checkSimpleCtrl(struct v4l2_control &ctrl, struct test_queryctrl &qctrl)
{
	if (ctrl.id != qctrl.id)
		return fail("control id mismatch\n");
	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_INTEGER:
	case V4L2_CTRL_TYPE_BOOLEAN:
	case V4L2_CTRL_TYPE_MENU:
		if (ctrl.value < qctrl.minimum || ctrl.value > qctrl.maximum)
			return fail("returned control value out of range\n");
		if ((ctrl.value - qctrl.minimum) % qctrl.step) {
			// This really should be a fail, but there are so few
			// drivers that do this right that I made it a warning
			// for now.
			warn("returned control value not a multiple of step\n");
		}
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
	qctrl_list::iterator iter;
	struct v4l2_control ctrl;
	int ret;
	int i;

	for (iter = node->controls.begin(); iter != node->controls.end(); ++iter) {
		info("checking control '%s' (0x%08x)\n", iter->name, iter->id);
		ctrl.id = iter->id;
		if (iter->type == V4L2_CTRL_TYPE_INTEGER64 ||
		    iter->type == V4L2_CTRL_TYPE_STRING ||
		    iter->type == V4L2_CTRL_TYPE_CTRL_CLASS) {
			ret = doioctl(node, VIDIOC_G_CTRL, &ctrl);
			if (ret != EINVAL &&
			    !((iter->flags & V4L2_CTRL_FLAG_WRITE_ONLY) && ret == EACCES))
				return fail("g_ctrl allowed for unsupported type\n");
			ctrl.id = iter->id;
			ctrl.value = 0;
			ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
			if (ret != EINVAL &&
			    !((iter->flags & V4L2_CTRL_FLAG_READ_ONLY) && ret == EACCES))
				return fail("s_ctrl allowed for unsupported type\n");
			continue;
		}

		// Get the current value
		ret = doioctl(node, VIDIOC_G_CTRL, &ctrl);
		if ((iter->flags & V4L2_CTRL_FLAG_WRITE_ONLY)) {
			if (ret != EACCES)
				return fail("g_ctrl did not check the write-only flag\n");
			ctrl.id = iter->id;
			ctrl.value = iter->default_value;
		} else if (ret)
			return fail("g_ctrl returned an error\n");
		else if (checkSimpleCtrl(ctrl, *iter))
			return fail("invalid control %08x\n", iter->id);
		
		// Try to set the current value (or the default value for write only controls)
		ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
		if ((iter->flags & V4L2_CTRL_FLAG_READ_ONLY) && ret != EACCES)
			return fail("s_ctrl did not check the read-only flag\n");
		else if (ret)
			return fail("s_ctrl returned an error\n");
		if (ret)
			continue;
		if (checkSimpleCtrl(ctrl, *iter))
			return fail("s_ctrl returned invalid control contents (%08x)\n", iter->id);

		// Try to set value 'minimum - 1'
		if ((unsigned)iter->minimum != 0x80000000) {
			ctrl.id = iter->id;
			ctrl.value = iter->minimum - 1;
			ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
			if (ret && ret != ERANGE)
				return fail("invalid minimum range check\n");
			if (!ret && checkSimpleCtrl(ctrl, *iter))
				return fail("invalid control %08x\n", iter->id);
		}
		// Try to set value 'maximum + 1'
		if ((unsigned)iter->maximum != 0x7fffffff) {
			ctrl.id = iter->id;
			ctrl.value = iter->maximum + 1;
			ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
			if (ret && ret != ERANGE)
				return fail("invalid maximum range check\n");
			if (!ret && checkSimpleCtrl(ctrl, *iter))
				return fail("invalid control %08x\n", iter->id);
		}
		// Try to set non-step value
		if (iter->step > 1 && iter->maximum > iter->minimum) {
			ctrl.id = iter->id;
			ctrl.value = iter->minimum + 1;
			ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
			if (ret == ERANGE)
				warn("returns ERANGE for in-range, but non-step-multiple value\n");
			else if (ret)
				return fail("returns error for in-range, but non-step-multiple value\n");
		}

		if (iter->type == V4L2_CTRL_TYPE_MENU) {
			// check menu items
			for (i = iter->minimum; i <= iter->maximum; i++) {
				unsigned valid = iter->menu_mask & (1 << i);

				ctrl.id = iter->id; 
				ctrl.value = i;
				ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
				if (valid && ret)
					return fail("could not set valid menu item %d\n", i);
				if (!valid && !ret)
					return fail("could set invalid menu item %d\n", i);
				if (ret && ret != EINVAL)
					return fail("setting invalid menu item returned wrong error\n");
			}
		} else {
			// at least min, max and default values should work
			ctrl.id = iter->id; 
			ctrl.value = iter->minimum;
			ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
			if (ret)
				return fail("could not set minimum value\n");
			ctrl.value = iter->maximum;
			ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
			if (ret)
				return fail("could not set maximum value\n");
			ctrl.value = iter->default_value;
			ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
			if (ret)
				return fail("could not set default value\n");
		}
	}
	ctrl.id = 0;
	ret = doioctl(node, VIDIOC_G_CTRL, &ctrl);
	if (ret != EINVAL)
		return fail("g_ctrl accepted invalid control ID\n");
	ctrl.id = 0;
	ctrl.value = 0;
	ret = doioctl(node, VIDIOC_S_CTRL, &ctrl);
	if (ret != EINVAL)
		return fail("s_ctrl accepted invalid control ID\n");
	return 0;
}
