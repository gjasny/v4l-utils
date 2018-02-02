/*
    V4L2 API subdev ioctl tests.

    Copyright (C) 2018  Hans Verkuil <hans.verkuil@cisco.com>

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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <linux/media.h>
#include "v4l2-compliance.h"

int testMediaDeviceInfo(struct node *node)
{
	struct media_device_info mdinfo;

	memset(&mdinfo, 0xff, sizeof(mdinfo));
	fail_on_test(doioctl(node, MEDIA_IOC_DEVICE_INFO, &mdinfo));
	fail_on_test(check_0(mdinfo.reserved, sizeof(mdinfo.reserved)));
	fail_on_test(check_string(mdinfo.driver, sizeof(mdinfo.driver)));
	fail_on_test(mdinfo.model[0] && check_string(mdinfo.model, sizeof(mdinfo.model)));
	fail_on_test(mdinfo.serial[0] && check_string(mdinfo.serial, sizeof(mdinfo.serial)));
	fail_on_test(mdinfo.bus_info[0] && check_string(mdinfo.bus_info, sizeof(mdinfo.bus_info)));
	fail_on_test(mdinfo.media_version == 0);
	fail_on_test(mdinfo.driver_version != mdinfo.media_version);
	return 0;
}
