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
	typedef std::set<__u32> id_set;
	id_set has_default_set;
	id_set entity_set;
	id_set remote_ent_set;
	struct media_entity_desc ent;
	struct media_links_enum links;
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

		entity_set.insert(ent.id);
		memset(&links, 0, sizeof(links));
		links.entity = ent.id;
		fail_on_test(doioctl(node, MEDIA_IOC_ENUM_LINKS, &links));
		fail_on_test(links.entity != ent.id);
		fail_on_test(links.pads);
		fail_on_test(links.links);
		links.pads = (struct media_pad_desc *)4;
		fail_on_test(ent.pads && doioctl(node, MEDIA_IOC_ENUM_LINKS, &links) != EFAULT);
		links.pads = NULL;
		links.links = (struct media_link_desc *)4;
		fail_on_test(ent.links && doioctl(node, MEDIA_IOC_ENUM_LINKS, &links) != EFAULT);
		links.links = NULL;
		links.pads = new media_pad_desc[ent.pads];
		links.links = new media_link_desc[ent.links];
		fail_on_test(doioctl(node, MEDIA_IOC_ENUM_LINKS, &links));

		bool found_source = false;
		for (unsigned i = 0; i < ent.pads; i++) {
			fail_on_test(links.pads[i].entity != ent.id);
			fail_on_test(links.pads[i].index != i);
			__u32 fl = links.pads[i].flags;
			fail_on_test(!(fl & (MEDIA_PAD_FL_SINK | MEDIA_PAD_FL_SOURCE)));
			fail_on_test((fl & (MEDIA_PAD_FL_SINK | MEDIA_PAD_FL_SOURCE)) ==
				     (MEDIA_PAD_FL_SINK | MEDIA_PAD_FL_SOURCE));
			if (fl & MEDIA_PAD_FL_SOURCE)
				found_source = true;
			else
				fail_on_test(found_source);
		}
		bool found_enabled = false;
		for (unsigned i = 0; i < ent.links; i++) {
			bool is_sink = links.links[i].sink.entity == ent.id;
			__u32 fl = links.links[i].flags;
			__u32 remote_ent;

			fail_on_test(links.links[i].source.entity != ent.id &&
				     links.links[i].sink.entity != ent.id);
			if (fl & MEDIA_LNK_FL_IMMUTABLE) {
				fail_on_test(!(fl & MEDIA_LNK_FL_ENABLED));
				fail_on_test(fl & MEDIA_LNK_FL_DYNAMIC);
			}
			if (fl & MEDIA_LNK_FL_DYNAMIC)
				fail_on_test(!(fl & MEDIA_LNK_FL_IMMUTABLE));
			if (is_sink && (fl & MEDIA_LNK_FL_ENABLED)) {
				// only one incoming link can be enabled
				fail_on_test(found_enabled);
				found_enabled = true;
			}
			// This ioctl only returns data links
			fail_on_test(fl & MEDIA_LNK_FL_LINK_TYPE);
			if (is_sink)
				remote_ent = links.links[i].source.entity;
			else
				remote_ent = links.links[i].sink.entity;
			remote_ent_set.insert(remote_ent);
		}
	}

	memset(&links, 0, sizeof(links));
	fail_on_test(doioctl(node, MEDIA_IOC_ENUM_LINKS, &links) != EINVAL);

	for (id_set::const_iterator iter = remote_ent_set.begin();
	     iter != remote_ent_set.end(); ++iter)
		fail_on_test(entity_set.find(*iter) == entity_set.end());

	return 0;
}
