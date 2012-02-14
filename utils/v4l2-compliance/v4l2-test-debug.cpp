/*
    V4L2 API compliance debug ioctl tests.

    Copyright (C) 2008, 2010  Hans Verkuil <hverkuil@xs4all.nl>

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
#include <sys/time.h>
#include <math.h>
#include "v4l2-compliance.h"

int testChipIdent(struct node *node)
{
	struct v4l2_dbg_chip_ident chip;
	int ret;

	memset(&chip, 0, sizeof(chip));
	chip.match.type = V4L2_CHIP_MATCH_HOST;
	chip.match.addr = 0;
	ret = doioctl(node, VIDIOC_DBG_G_CHIP_IDENT, &chip);
	// Must return either 0 (OK) or EINVAL (not supported)
	if (ret == 0) {
		struct v4l2_dbg_chip_ident orig;

		memset(&orig, 0, sizeof(orig));
		// set invalid match_type
		chip.match.type = V4L2_CHIP_MATCH_I2C_ADDR + 1;
		chip.match.addr = 0xdeadbeef;
		chip.ident = 0xdeadbeef;
		chip.revision = 0xdeadbeef;
		orig = chip;
		ret = doioctl(node, VIDIOC_DBG_G_CHIP_IDENT, &chip);
		if (ret != EINVAL)
			return fail("Invalid match_type accepted\n");
		fail_on_test(memcmp(&orig, &chip, sizeof(chip)));
		return 0;
	}
	return ret;
}

int testRegister(struct node *node)
{
	struct v4l2_dbg_register reg;
	struct v4l2_dbg_chip_ident chip;
	int ret;
	int uid = getuid();

	reg.match.type = V4L2_CHIP_MATCH_HOST;
	reg.match.addr = 0;
	reg.reg = 0;
	ret = doioctl(node, VIDIOC_DBG_G_REGISTER, &reg);
	if (ret == ENOTTY)
		return ret;
	// Not allowed to call VIDIOC_DBG_G_REGISTER unless root
	fail_on_test(uid && ret != EPERM);
	fail_on_test(uid == 0 && ret);
	chip.match.type = V4L2_CHIP_MATCH_HOST;
	chip.match.addr = 0;
	fail_on_test(doioctl(node, VIDIOC_DBG_G_CHIP_IDENT, &chip));
	if (uid) {
		// Don't test S_REGISTER as root, don't want to risk
		// messing with registers in the compliance test.
		reg.reg = reg.val = 0;
		ret = doioctl(node, VIDIOC_DBG_S_REGISTER, &reg);
		fail_on_test(ret != ENOTTY && ret != EINVAL && ret != EPERM);
	}
	return 0;
}

int testLogStatus(struct node *node)
{
	return doioctl(node, VIDIOC_LOG_STATUS, NULL);
}
