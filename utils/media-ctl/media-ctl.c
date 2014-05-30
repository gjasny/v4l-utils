/*
 * Media controller test application
 *
 * Copyright (C) 2010-2011 Ideas on board SPRL
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

static void media_print_topology_text(struct media_device *media)
{
	static const struct {
		__u32 flag;
		char *name;
	} link_flags[] = {
		{ MEDIA_LNK_FL_ENABLED, "ENABLED" },
		{ MEDIA_LNK_FL_IMMUTABLE, "IMMUTABLE" },
		{ MEDIA_LNK_FL_DYNAMIC, "DYNAMIC" },
	};

	unsigned int nents = media_get_entities_count(media);
	unsigned int i, j, k;
	unsigned int padding;

	printf("Device topology\n");

	for (i = 0; i < nents; ++i) {
		struct media_entity *entity = media_get_entity(media, i);
		const struct media_entity_desc *info = media_entity_get_info(entity);
		const char *devname = media_entity_get_devname(entity);
		unsigned int num_links = media_entity_get_links_count(entity);

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

			if (media_entity_type(entity) == MEDIA_ENT_T_V4L2_SUBDEV)
				v4l2_subdev_print_format(entity, j, V4L2_SUBDEV_FORMAT_ACTIVE);

			for (k = 0; k < num_links; k++) {
				const struct media_link *link = media_entity_get_link(entity, k);
				const struct media_pad *source = link->source;
				const struct media_pad *sink = link->sink;
				bool first = true;
				unsigned int i;

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

				for (i = 0; i < ARRAY_SIZE(link_flags); i++) {
					if (!(link->flags & link_flags[i].flag))
						continue;
					if (!first)
						printf(",");
					printf("%s", link_flags[i].name);
					first = false;
				}

				printf("]\n");
			}
		}
		printf("\n");
	}
}

void media_print_topology(struct media_device *media, int dot)
{
	if (dot)
		media_print_topology_dot(media);
	else
		media_print_topology_text(media);
}

int main(int argc, char **argv)
{
	struct media_device *media;
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
		       (info->media_version << 16) & 0xff,
		       (info->media_version << 8) & 0xff,
		       (info->media_version << 0) & 0xff);
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
		       (info->driver_version << 16) & 0xff,
		       (info->driver_version << 8) & 0xff,
		       (info->driver_version << 0) & 0xff);
	}

	if (media_opts.entity) {
		struct media_entity *entity;

		entity = media_get_entity_by_name(media, media_opts.entity,
						  strlen(media_opts.entity));
		if (entity == NULL) {
			printf("Entity '%s' not found\n", media_opts.entity);
			goto out;
		}

		printf("%s\n", media_entity_get_devname(entity));
	}

	if (media_opts.pad) {
		struct media_pad *pad;

		pad = media_parse_pad(media, media_opts.pad, NULL);
		if (pad == NULL) {
			printf("Pad '%s' not found\n", media_opts.pad);
			goto out;
		}

		v4l2_subdev_print_format(pad->entity, pad->index,
					 V4L2_SUBDEV_FORMAT_ACTIVE);
	}

	if (media_opts.print || media_opts.print_dot) {
		media_print_topology(media, media_opts.print_dot);
		printf("\n");
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

