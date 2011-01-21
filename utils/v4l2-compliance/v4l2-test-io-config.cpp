/*
    V4L2 API compliance input/output configuration ioctl tests.

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

static int checkStd(struct node *node, bool has_std, v4l2_std_id mask)
{
	v4l2_std_id std;
	int ret;

	ret = doioctl(node, VIDIOC_G_STD, &std);
	if (ret && has_std)
		return fail("STD cap set, but could not get standard\n");
	if (!ret && !has_std)
		return fail("STD cap not set, but could still get a standard\n");
	if (!ret && has_std) {
		if (std & ~mask)
			warn("current standard is invalid according to the input standard mask\n");
		if (std == 0)
			return fail("Standard == 0?!\n");
	}
	ret = doioctl(node, VIDIOC_S_STD, &std);
	if (ret && has_std)
		return fail("STD cap set, but could not set standard\n");
	if (!ret && !has_std)
		return fail("STD cap not set, but could still set a standard\n");
	return 0;
}

int testStd(struct node *node)
{
	int ret;
	unsigned i, o;

	for (i = 0; i < node->inputs; i++) {
		struct v4l2_input input;

		input.index = i;
		ret = doioctl(node, VIDIOC_ENUMINPUT, &input);
		if (ret)
			return fail("could not enumerate input %d?!\n", i);
		ret = doioctl(node, VIDIOC_S_INPUT, &input.index);
		if (ret)
			return fail("could not select input %d.\n", i);
		if (checkStd(node, input.capabilities & V4L2_IN_CAP_STD, input.std))
			return fail("STD failed for input %d.\n", i);
	}

	for (o = 0; o < node->outputs; o++) {
		struct v4l2_output output;

		output.index = o;
		ret = doioctl(node, VIDIOC_ENUMOUTPUT, &output);
		if (ret)
			return fail("could not enumerate output %d?!\n", o);
		ret = doioctl(node, VIDIOC_S_OUTPUT, &output.index);
		if (ret)
			return fail("could not select output %d.\n", o);
		if (checkStd(node, output.capabilities & V4L2_OUT_CAP_STD, output.std))
			return fail("STD failed for output %d.\n", o);
	}
	return 0;
}
