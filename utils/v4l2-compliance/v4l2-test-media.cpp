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
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <set>

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

int testMediaEnum(struct node *node)
{
	std::set<__u32> has_default_set;
	struct media_entity_desc ent;
	unsigned num_entities = 0;
	__u32 last_id = 0;
	int ret;

	memset(&ent, 0, sizeof(ent));
	// Entity ID 0 can never occur
	ent.id = 0;
	fail_on_test(doioctl(node, MEDIA_IOC_ENUM_ENTITIES, &ent) != EINVAL);
	for (;;) {
		memset(&ent, 0xff, sizeof(ent));
		ent.id = last_id | MEDIA_ENT_ID_FLAG_NEXT;
		ret = doioctl(node, MEDIA_IOC_ENUM_ENTITIES, &ent);
		if (ret == EINVAL)
			break;
		num_entities++;
		fail_on_test(ent.id & MEDIA_ENT_ID_FLAG_NEXT);
		fail_on_test(!ent.id);
		fail_on_test(ent.id <= last_id);
		last_id = ent.id;
		fail_on_test(check_string(ent.name, sizeof(ent.name)));
		fail_on_test(ent.revision);
		fail_on_test(ent.group_id);
		fail_on_test(!ent.type);
		fail_on_test(ent.type == ~0U);
		fail_on_test(ent.flags == ~0U);
		fail_on_test(ent.pads == 0xffff);
		fail_on_test(ent.links == 0xffff);

		if (ent.flags & MEDIA_ENT_FL_DEFAULT) {
			fail_on_test(has_default_set.find(ent.type) != has_default_set.end());
			has_default_set.insert(ent.type);
		}
		if (!(ent.flags & MEDIA_ENT_FL_CONNECTOR)) {
			fail_on_test(ent.dev.major == ~0U);
			fail_on_test(ent.dev.minor == ~0U);
			char dev_path[100];
			fail_on_test(snprintf(dev_path, sizeof(dev_path), "/sys/dev/char/%d:%d",
				     ent.dev.major, ent.dev.minor) == -1);
			DIR *dp = opendir(dev_path);
			if (dp == NULL)	
				return fail("couldn't find %s for entity %u\n",
					    dev_path, ent.id);
			closedir(dp);
		}
		fail_on_test(doioctl(node, MEDIA_IOC_ENUM_ENTITIES, &ent));
	}

	return 0;
}
