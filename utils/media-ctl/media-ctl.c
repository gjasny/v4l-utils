/*
 * Media controller test application
 *
 * Copyright (C) 2010-2014 Ideas on board SPRL
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/media.h>
#include <linux/types.h>
#include <linux/v4l2-mediabus.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>

#include "mediactl.h"
#include "options.h"
#include "tools.h"
#include "v4l2subdev.h"

/* -----------------------------------------------------------------------------
 * Printing
 */

struct flag_name {
	__u32 flag;
	char *name;
};

static void print_flags(const struct flag_name *flag_names, unsigned int num_entries, __u32 flags)
{
	bool first = true;
	unsigned int i;

	for (i = 0; i < num_entries; i++) {
		if (!(flags & flag_names[i].flag))
			continue;
		if (!first)
			printf(",");
		printf("%s", flag_names[i].name);
		flags &= ~flag_names[i].flag;
		first = false;
	}

	if (flags) {
		if (!first)
			printf(",");
		printf("0x%x", flags);
	}
}

static void v4l2_subdev_print_format(struct media_entity *entity,
	unsigned int pad, enum v4l2_subdev_format_whence which)
{
	struct v4l2_mbus_framefmt format;
	struct v4l2_rect rect;
	int ret;

	ret = v4l2_subdev_get_format(entity, &format, pad, which);
	if (ret != 0)
		return;

	printf("\t\t[fmt:%s/%ux%u",
	       v4l2_subdev_pixelcode_to_string(format.code),
	       format.width, format.height);

	if (format.field)
		printf(" field:%s", v4l2_subdev_field_to_string(format.field));

	ret = v4l2_subdev_get_selection(entity, &rect, pad,
					V4L2_SEL_TGT_CROP_BOUNDS,
					which);
	if (ret == 0)
		printf("\n\t\t crop.bounds:(%u,%u)/%ux%u", rect.left, rect.top,
		       rect.width, rect.height);

	ret = v4l2_subdev_get_selection(entity, &rect, pad,
					V4L2_SEL_TGT_CROP,
					which);
	if (ret == 0)
		printf("\n\t\t crop:(%u,%u)/%ux%u", rect.left, rect.top,
		       rect.width, rect.height);

	ret = v4l2_subdev_get_selection(entity, &rect, pad,
					V4L2_SEL_TGT_COMPOSE_BOUNDS,
					which);
	if (ret == 0)
		printf("\n\t\t compose.bounds:(%u,%u)/%ux%u",
		       rect.left, rect.top, rect.width, rect.height);

	ret = v4l2_subdev_get_selection(entity, &rect, pad,
					V4L2_SEL_TGT_COMPOSE,
					which);
	if (ret == 0)
		printf("\n\t\t compose:(%u,%u)/%ux%u",
		       rect.left, rect.top, rect.width, rect.height);

	printf("]\n");
}

static const char *v4l2_dv_type_to_string(unsigned int type)
{
	static const struct {
		__u32 type;
		const char *name;
	} types[] = {
		{ V4L2_DV_BT_656_1120, "BT.656/1120" },
	};

	static char unknown[20];
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(types); i++) {
		if (types[i].type == type)
			return types[i].name;
	}

	sprintf(unknown, "Unknown (%u)", type);
	return unknown;
}

static const struct flag_name bt_standards[] = {
	{ V4L2_DV_BT_STD_CEA861, "CEA-861" },
	{ V4L2_DV_BT_STD_DMT, "DMT" },
	{ V4L2_DV_BT_STD_CVT, "CVT" },
	{ V4L2_DV_BT_STD_GTF, "GTF" },
	{ V4L2_DV_BT_STD_SDI, "SDI" },
};

static const struct flag_name bt_capabilities[] = {
	{ V4L2_DV_BT_CAP_INTERLACED, "interlaced" },
	{ V4L2_DV_BT_CAP_PROGRESSIVE, "progressive" },
	{ V4L2_DV_BT_CAP_REDUCED_BLANKING, "reduced-blanking" },
	{ V4L2_DV_BT_CAP_CUSTOM, "custom" },
};

static const struct flag_name bt_flags[] = {
	{ V4L2_DV_FL_REDUCED_BLANKING, "reduced-blanking" },
	{ V4L2_DV_FL_CAN_REDUCE_FPS, "can-reduce-fps" },
	{ V4L2_DV_FL_REDUCED_FPS, "reduced-fps" },
	{ V4L2_DV_FL_HALF_LINE, "half-line" },
	{ V4L2_DV_FL_IS_CE_VIDEO, "CE-video" },
	{ V4L2_DV_FL_FIRST_FIELD_EXTRA_LINE, "first-field-extra-line" },
};

static void v4l2_subdev_print_dv_timings(const struct v4l2_dv_timings *timings,
					 const char *name)
{
	printf("\t\t[dv.%s:%s", name, v4l2_dv_type_to_string(timings->type));

	switch (timings->type) {
	case V4L2_DV_BT_656_1120: {
		const struct v4l2_bt_timings *bt = &timings->bt;
		unsigned int htotal, vtotal;

		htotal = V4L2_DV_BT_FRAME_WIDTH(bt);
		vtotal = V4L2_DV_BT_FRAME_HEIGHT(bt);

		printf(" %ux%u%s%llu (%ux%u)",
		       bt->width, bt->height, bt->interlaced ? "i" : "p",
		       (htotal * vtotal) > 0 ? (bt->pixelclock / (htotal * vtotal)) : 0,
		       htotal, vtotal);

		printf(" stds:");
		print_flags(bt_standards, ARRAY_SIZE(bt_standards),
			    bt->standards);
		printf(" flags:");
		print_flags(bt_flags, ARRAY_SIZE(bt_flags),
			    bt->flags);

		break;
	}
	}

	printf("]\n");
}

static void v4l2_subdev_print_pad_dv(struct media_entity *entity,
	unsigned int pad, enum v4l2_subdev_format_whence which)
{
	struct v4l2_dv_timings_cap caps;
	int ret;

	caps.pad = pad;
	ret = v4l2_subdev_get_dv_timings_caps(entity, &caps);
	if (ret != 0)
		return;

	printf("\t\t[dv.caps:%s", v4l2_dv_type_to_string(caps.type));

	switch (caps.type) {
	case V4L2_DV_BT_656_1120:
		printf(" min:%ux%u@%llu max:%ux%u@%llu",
		       caps.bt.min_width, caps.bt.min_height, caps.bt.min_pixelclock,
		       caps.bt.max_width, caps.bt.max_height, caps.bt.max_pixelclock);

		printf(" stds:");
		print_flags(bt_standards, ARRAY_SIZE(bt_standards),
			    caps.bt.standards);
		printf(" caps:");
		print_flags(bt_capabilities, ARRAY_SIZE(bt_capabilities),
			    caps.bt.capabilities);

		break;
	}

	printf("]\n");
}

static void v4l2_subdev_print_subdev_dv(struct media_entity *entity)
{
	struct v4l2_dv_timings timings;
	int ret;

	ret = v4l2_subdev_query_dv_timings(entity, &timings);
	switch (ret) {
	case -ENOLINK:
		printf("\t\t[dv.query:no-link]\n");
		break;
	case -ENOLCK:
		printf("\t\t[dv.query:no-lock]\n");
		break;
	case -ERANGE:
		printf("\t\t[dv.query:out-of-range]\n");
		break;
	case 0:
		v4l2_subdev_print_dv_timings(&timings, "detect");
		break;
	default:
		return;
	}

	ret = v4l2_subdev_get_dv_timings(entity, &timings);
	if (ret == 0)
		v4l2_subdev_print_dv_timings(&timings, "current");
}

static const char *media_entity_type_to_string(unsigned type)
{
	static const struct {
		__u32 type;
		const char *name;
	} types[] = {
		{ MEDIA_ENT_T_DEVNODE, "Node" },
		{ MEDIA_ENT_T_V4L2_SUBDEV, "V4L2 subdev" },
	};

	unsigned int i;

	type &= MEDIA_ENT_TYPE_MASK;

	for (i = 0; i < ARRAY_SIZE(types); i++) {
		if (types[i].type == type)
			return types[i].name;
	}

	return "Unknown";
}

static const char *media_entity_subtype_to_string(unsigned type)
{
	static const char *node_types[] = {
		"Unknown",
		"V4L",
		"FB",
		"ALSA",
		"DVB",
	};
	static const char *subdev_types[] = {
		"Unknown",
		"Sensor",
		"Flash",
		"Lens",
		"Decoder",
		"Tuner",
	};

	unsigned int subtype = type & MEDIA_ENT_SUBTYPE_MASK;

	switch (type & MEDIA_ENT_TYPE_MASK) {
	case MEDIA_ENT_T_DEVNODE:
		if (subtype >= ARRAY_SIZE(node_types))
			subtype = 0;
		return node_types[subtype];

	case MEDIA_ENT_T_V4L2_SUBDEV:
		if (subtype >= ARRAY_SIZE(subdev_types))
			subtype = 0;
		return subdev_types[subtype];
	default:
		return node_types[0];
	}
}

static const char *media_pad_type_to_string(unsigned flag)
{
	static const struct {
		__u32 flag;
		const char *name;
	} flags[] = {
		{ MEDIA_PAD_FL_SINK, "Sink" },
		{ MEDIA_PAD_FL_SOURCE, "Source" },
	};

	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(flags); i++) {
		if (flags[i].flag & flag)
			return flags[i].name;
	}

	return "Unknown";
}

static void media_print_topology_dot(struct media_device *media)
{
	unsigned int nents = media_get_entities_count(media);
	unsigned int i, j;

	printf("digraph board {\n");
	printf("\trankdir=TB\n");

	for (i = 0; i < nents; ++i) {
		struct media_entity *entity = media_get_entity(media, i);
		const struct media_entity_desc *info = media_entity_get_info(entity);
		const char *devname = media_entity_get_devname(entity);
		unsigned int num_links = media_entity_get_links_count(entity);
		unsigned int npads;

		if (!devname)
			devname = "";

		switch (media_entity_type(entity)) {
		case MEDIA_ENT_T_DEVNODE:
			printf("\tn%08x [label=\"%s\\n%s\", shape=box, style=filled, "
			       "fillcolor=yellow]\n",
			       info->id, info->name, devname);
			break;

		case MEDIA_ENT_T_V4L2_SUBDEV:
			printf("\tn%08x [label=\"{{", info->id);

			for (j = 0, npads = 0; j < info->pads; ++j) {
				const struct media_pad *pad = media_entity_get_pad(entity, j);

				if (!(pad->flags & MEDIA_PAD_FL_SINK))
					continue;

				printf("%s<port%u> %u", npads ? " | " : "", j, j);
				npads++;
			}

			printf("} | %s", info->name);
			if (devname)
				printf("\\n%s", devname);
			printf(" | {");

			for (j = 0, npads = 0; j < info->pads; ++j) {
				const struct media_pad *pad = media_entity_get_pad(entity, j);

				if (!(pad->flags & MEDIA_PAD_FL_SOURCE))
					continue;

				printf("%s<port%u> %u", npads ? " | " : "", j, j);
				npads++;
			}

			printf("}}\", shape=Mrecord, style=filled, fillcolor=green]\n");
			break;

		default:
			continue;
		}

		for (j = 0; j < num_links; j++) {
			const struct media_link *link = media_entity_get_link(entity, j);
			const struct media_pad *source = link->source;
			const struct media_pad *sink = link->sink;

			if (source->entity != entity)
				continue;

			printf("\tn%08x", media_entity_get_info(source->entity)->id);
			if (media_entity_type(source->entity) == MEDIA_ENT_T_V4L2_SUBDEV)
				printf(":port%u", source->index);
			printf(" -> ");
			printf("n%08x", media_entity_get_info(sink->entity)->id);
			if (media_entity_type(sink->entity) == MEDIA_ENT_T_V4L2_SUBDEV)
				printf(":port%u", sink->index);

			if (link->flags & MEDIA_LNK_FL_IMMUTABLE)
				printf(" [style=bold]");
			else if (!(link->flags & MEDIA_LNK_FL_ENABLED))
				printf(" [style=dashed]");
			printf("\n");
		}
	}

	printf("}\n");
}

static void media_print_pad_text(struct media_entity *entity,
				 const struct media_pad *pad)
{
	if (media_entity_type(entity) != MEDIA_ENT_T_V4L2_SUBDEV)
		return;

	v4l2_subdev_print_format(entity, pad->index, V4L2_SUBDEV_FORMAT_ACTIVE);
	v4l2_subdev_print_pad_dv(entity, pad->index, V4L2_SUBDEV_FORMAT_ACTIVE);

	if (pad->flags & MEDIA_PAD_FL_SOURCE)
		v4l2_subdev_print_subdev_dv(entity);
}

static void media_print_topology_text_entity(struct media_device *media,
					     struct media_entity *entity)
{
	static const struct flag_name link_flags[] = {
		{ MEDIA_LNK_FL_ENABLED, "ENABLED" },
		{ MEDIA_LNK_FL_IMMUTABLE, "IMMUTABLE" },
		{ MEDIA_LNK_FL_DYNAMIC, "DYNAMIC" },
	};
	const struct media_entity_desc *info = media_entity_get_info(entity);
	const char *devname = media_entity_get_devname(entity);
	unsigned int num_links = media_entity_get_links_count(entity);
	unsigned int j, k;
	unsigned int padding;

	padding = printf("- entity %u: ", info->id);
	printf("%s (%u pad%s, %u link%s)\n", info->name,
	       info->pads, info->pads > 1 ? "s" : "",
	       num_links, num_links > 1 ? "s" : "");
	printf("%*ctype %s subtype %s flags %x\n", padding, ' ',
	       media_entity_type_to_string(info->type),
	       media_entity_subtype_to_string(info->type),
	       info->flags);
	if (devname)
		printf("%*cdevice node name %s\n", padding, ' ', devname);

	for (j = 0; j < info->pads; j++) {
		const struct media_pad *pad = media_entity_get_pad(entity, j);

		printf("\tpad%u: %s\n", j, media_pad_type_to_string(pad->flags));

		media_print_pad_text(entity, pad);

		for (k = 0; k < num_links; k++) {
			const struct media_link *link = media_entity_get_link(entity, k);
			const struct media_pad *source = link->source;
			const struct media_pad *sink = link->sink;

			if (source->entity == entity && source->index == j)
				printf("\t\t-> \"%s\":%u [",
				       media_entity_get_info(sink->entity)->name,
				       sink->index);
			else if (sink->entity == entity && sink->index == j)
				printf("\t\t<- \"%s\":%u [",
				       media_entity_get_info(source->entity)->name,
				       source->index);
			else
				continue;

			print_flags(link_flags, ARRAY_SIZE(link_flags), link->flags);

			printf("]\n");
		}
	}
	printf("\n");
}

static void media_print_topology_text(struct media_device *media)
{
	unsigned int nents = media_get_entities_count(media);
	unsigned int i;

	printf("Device topology\n");

	for (i = 0; i < nents; ++i)
		media_print_topology_text_entity(
			media, media_get_entity(media, i));
}

int main(int argc, char **argv)
{
	struct media_device *media;
	struct media_entity *entity = NULL;
	int ret = -1;

	if (parse_cmdline(argc, argv))
		return EXIT_FAILURE;

	media = media_device_new(media_opts.devname);
	if (media == NULL) {
		printf("Failed to create media device\n");
		goto out;
	}

	if (media_opts.verbose)
		media_debug_set_handler(media,
			(void (*)(void *, ...))fprintf, stdout);

	/* Enumerate entities, pads and links. */
	ret = media_device_enumerate(media);
	if (ret < 0) {
		printf("Failed to enumerate %s (%d)\n", media_opts.devname, ret);
		goto out;
	}

	if (media_opts.print) {
		const struct media_device_info *info = media_get_info(media);

		printf("Media controller API version %u.%u.%u\n\n",
		       (info->media_version >> 16) & 0xff,
		       (info->media_version >> 8) & 0xff,
		       (info->media_version >> 0) & 0xff);
		printf("Media device information\n"
		       "------------------------\n"
		       "driver          %s\n"
		       "model           %s\n"
		       "serial          %s\n"
		       "bus info        %s\n"
		       "hw revision     0x%x\n"
		       "driver version  %u.%u.%u\n\n",
		       info->driver, info->model,
		       info->serial, info->bus_info,
		       info->hw_revision,
		       (info->driver_version >> 16) & 0xff,
		       (info->driver_version >> 8) & 0xff,
		       (info->driver_version >> 0) & 0xff);
	}

	if (media_opts.entity) {
		entity = media_get_entity_by_name(media, media_opts.entity);
		if (entity == NULL) {
			printf("Entity '%s' not found\n", media_opts.entity);
			goto out;
		}
	}

	if (media_opts.fmt_pad) {
		struct media_pad *pad;

		pad = media_parse_pad(media, media_opts.fmt_pad, NULL);
		if (pad == NULL) {
			printf("Pad '%s' not found\n", media_opts.fmt_pad);
			goto out;
		}

		v4l2_subdev_print_format(pad->entity, pad->index,
					 V4L2_SUBDEV_FORMAT_ACTIVE);
	}

	if (media_opts.dv_pad) {
		struct v4l2_dv_timings timings;
		struct media_pad *pad;

		pad = media_parse_pad(media, media_opts.dv_pad, NULL);
		if (pad == NULL) {
			printf("Pad '%s' not found\n", media_opts.dv_pad);
			goto out;
		}

		ret = v4l2_subdev_query_dv_timings(pad->entity, &timings);
		if (ret < 0) {
			printf("Failed to query DV timings: %s\n", strerror(-ret));
			goto out;
		}

		ret = v4l2_subdev_set_dv_timings(pad->entity, &timings);
		if (ret < 0) {
			printf("Failed to set DV timings: %s\n", strerror(-ret));
			goto out;
		}
	}

	if (media_opts.print_dot) {
		media_print_topology_dot(media);
	} else if (media_opts.print) {
		if (entity)
			media_print_topology_text_entity(media, entity);
		else
			media_print_topology_text(media);
	} else if (entity) {
		const char *devname;

		devname = media_entity_get_devname(entity);
		if (devname)
			printf("%s\n", devname);
	}

	if (media_opts.reset) {
		if (media_opts.verbose)
			printf("Resetting all links to inactive\n");
		ret = media_reset_links(media);
		if (ret) {
			printf("Unable to reset links: %s (%d)\n",
			       strerror(-ret), -ret);
			goto out;
		}
	}

	if (media_opts.links) {
		ret = media_parse_setup_links(media, media_opts.links);
		if (ret) {
			printf("Unable to parse link: %s (%d)\n",
			       strerror(-ret), -ret);
			goto out;
		}
	}

	if (media_opts.formats) {
		ret = v4l2_subdev_parse_setup_formats(media,
						      media_opts.formats);
		if (ret) {
			printf("Unable to setup formats: %s (%d)\n",
			       strerror(-ret), -ret);
			goto out;
		}
	}

	if (media_opts.interactive) {
		while (1) {
			char buffer[32];
			char *end;

			printf("Enter a link to modify or enter to stop\n");
			if (fgets(buffer, sizeof buffer, stdin) == NULL)
				break;

			if (buffer[0] == '\n')
				break;

			ret = media_parse_setup_link(media, buffer, &end);
			if (ret)
				printf("Unable to parse link: %s (%d)\n",
				       strerror(-ret), -ret);
		}
	}

	ret = 0;

out:
	if (media)
		media_device_unref(media);

	return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}

