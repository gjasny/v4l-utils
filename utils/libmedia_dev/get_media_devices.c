/*
   Copyright © 2011 by Mauro Carvalho Chehab <mchehab@redhat.com>

   The get_media_devices is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The libv4l2util Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the libv4l2util Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <dirent.h>
#include "get_media_devices.h"

static char *device_type_str[] = {
	[V4L_VIDEO]	= "video",
	[V4L_VBI]	= "vbi",
	[DVB_FRONTEND]	= "dvb frontend",
	[DVB_DEMUX]	= "dvb demux",
	[DVB_DVR]	= "dvb dvr",
	[DVB_NET]	= "dvb net",
	[DVB_CA]	= "dvb conditional access",
	[SND_CARD]	= "sound card",
	[SND_CAP]	= "pcm capture",
	[SND_OUT]	= "pcm output",
	[SND_CONTROL]	= "mixer",
	[SND_HW]	= "sound hardware",
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * struct media_device_entry - Describes one device entry got via sysfs
 *
 * @device:		sysfs name for a device.
 *			PCI devices are like: pci0000:00/0000:00:1b.0
 *			USB devices are like: pci0000:00/0000:00:1d.7/usb1/1-8
 * @node:		Device node, in sysfs or alsa hw identifier
 * @device_type:	Type of the device (V4L_*, DVB_*, SND_*)
 */
struct media_device_entry {
	char *device;
	char *node;
	enum device_type type;
};

/**
 * struct media_devices - Describes all devices found
 *
 * @device:		sysfs name for a device.
 *			PCI devices are like: pci0000:00/0000:00:1b.0
 *			USB devices are like: pci0000:00/0000:00:1d.7/usb1/1-8
 * @node:		Device node, in sysfs or alsa hw identifier
 * @device_type:	Type of the device (V4L_*, DVB_*, SND_*)
 */
struct media_devices {
	struct media_device_entry *md_entry;
	unsigned int md_size;
};

typedef int (*fill_data_t)(struct media_device_entry *md);

#define DEVICE_STR "devices"

static int get_class(char *class,
		     struct media_device_entry **md,
		     unsigned int *md_size,
		     fill_data_t fill)
{
	DIR		*dir;
	struct dirent	*entry;
	char		dname[512];
	char		fname[512];
	char		link[1024];
	int		err = -2;
	struct		media_device_entry *md_ptr = NULL;
	int		size;
	char		*p, *class_node, *device;

	sprintf(dname, "/sys/class/%s", class);
	dir = opendir(dname);
	if (!dir) {
		perror(dname);
		return 0;
	}
	for (entry = readdir(dir); entry; entry = readdir(dir)) {
		sprintf(fname, "%s/%s", dname, entry->d_name);

		size = readlink(fname, link, sizeof(link));
		if (size > 0) {
			link[size] = '\0';

			/* Keep just the name of the cass node */
			p = strrchr(fname, '/');
			if (!p)
				goto error;
			class_node = p + 1;

			/* Canonicalize the device name */

			/* remove the ../../devices/ from the name */
			p = strstr(link, DEVICE_STR);
			if (!p)
				goto error;
			device = p + sizeof(DEVICE_STR);

			/* Remove the subsystem/class_name from the string */
			p = strstr(link, class);
			if (!p)
				goto error;
			*(p -1) = '\0';

			/* Remove USB sub-devices from the path */
			if (strstr(device,"usb")) {
				do {
					p = strrchr(device, '/');
					if (!p)
						goto error;
					if (!strpbrk(p,":."))
						break;
					*p = '\0';
				} while (1);
			}

			/* Don't handle virtual devices */
			if (!strcmp(device,"virtual"))
				continue;

			/* Add one more element to the devices struct */
			*md = realloc(*md, (*md_size + 1) * sizeof(*md_ptr));
			if (!*md)
				goto error;
			md_ptr = (*md) + *md_size;
			(*md_size)++;

			/* Cleans previous data and fills it with device/node */
			md_ptr->type = UNKNOWN;
			md_ptr->device = malloc(strlen(device) + 1);
			strcpy(md_ptr->device, device);
			md_ptr->node = malloc(strlen(class_node) + 1);
			strcpy(md_ptr->node, class_node);

			/* Used to identify the type of node */
			fill(md_ptr);
		}
	}
	err = 0;
error:
	closedir(dir);
	return err;
}

static int add_v4l_class(struct media_device_entry *md)
{
	if (strstr(md->node, "video"))
		md->type = V4L_VIDEO;
	else if (strstr(md->node, "vbi"))
		md->type = V4L_VBI;

	return 0;
};

static int add_snd_class(struct media_device_entry *md)
{
	unsigned c = 65535, d = 65535;
	char *new;

	if (strstr(md->node, "card")) {
		sscanf(md->node, "card%u", &c);
		md->type = SND_CARD;
	} else if (strstr(md->node, "hw")) {
		sscanf(md->node, "hwC%uD%u", &c, &d);
		md->type = SND_HW;
	} else if (strstr(md->node, "control")) {
		sscanf(md->node, "controlC%u", &c);
		md->type = SND_CONTROL;
	} else if (strstr(md->node, "pcm")) {
		sscanf(md->node, "pcmC%uD%u", &c, &d);
		if (md->node[strlen(md->node) - 1] == 'p')
			md->type = SND_OUT;
		else if (md->node[strlen(md->node) - 1] == 'c')
			md->type = SND_CAP;
	}

	if (c == 65535)
		return 0;

	/* Reformat device to be useful for alsa userspace library */
	if (d == 65535) {
		if (asprintf(&new, "hw:%u", c) > 0) {
			free(md->node);
			md->node = new;
		}
		return 0;
	}

	if (asprintf(&new, "hw:%u,%u", c, d) > 0) {
		free(md->node);
		md->node = new;
	}

	return 0;
};

static int add_dvb_class(struct media_device_entry *md)
{
	if (strstr(md->node, "frontend"))
		md->type = DVB_FRONTEND;
	else if (strstr(md->node, "demux"))
		md->type = DVB_DEMUX;
	else if (strstr(md->node, "dvr"))
		md->type = DVB_DVR;
	else if (strstr(md->node, "net"))
		md->type = DVB_NET;
	else if (strstr(md->node, "ca"))
		md->type = DVB_CA;

	return 0;
};

static int sort_media_device_entry(const void *a, const void *b)
{
	const struct media_device_entry *md_a = a;
	const struct media_device_entry *md_b = b;
	int cmp;

	cmp = strcmp(md_a->device, md_b->device);
	if (cmp)
		return cmp;
	cmp = (int)md_a->type - (int)md_b->type;
	if (cmp)
		return cmp;

	return strcmp(md_a->node, md_b->node);
}


/* Public functions */

void free_media_devices(void *opaque)
{
	struct media_devices *md = opaque;
	struct media_device_entry *md_ptr = md->md_entry;
	int i;
	for (i = 0; i < md->md_size; i++) {
		free(md_ptr->node);
		free(md_ptr->device);
		md_ptr++;
	}
	free(md->md_entry);
	free(md);
}

void *discover_media_devices(void)
{
	struct media_devices *md = NULL;
	struct media_device_entry *md_entry = NULL;

	md = calloc(1, sizeof(*md));
	if (!md)
		return NULL;

	md->md_size = 0;
	if (get_class("video4linux", &md_entry, &md->md_size, add_v4l_class))
		goto error;
	if (get_class("sound", &md_entry, &md->md_size, add_snd_class))
		goto error;
	if (get_class("dvb", &md_entry, &md->md_size, add_dvb_class))
		goto error;

	/* There's no media device */
	if (!md_entry)
		goto error;

	qsort(md_entry, md->md_size, sizeof(*md_entry), sort_media_device_entry);

	md->md_entry = md_entry;

	return md;

error:
	free_media_devices(md);
	return NULL;
}

char *media_device_type(enum device_type type)
{
	if ((unsigned int)type >= ARRAY_SIZE(device_type_str))
		return "unknown";
	else return device_type_str[type];
}

void display_media_devices(void *opaque)
{
	struct media_devices *md = opaque;
	struct media_device_entry *md_ptr = md->md_entry;
	int i;
	char *prev = "";

	for (i = 0; i < md->md_size; i++) {
		if (strcmp(prev, md_ptr->device)) {
			printf ("\nDevice %s:\n\t", md_ptr->device);
			prev = md_ptr->device;
		}
		printf ("%s(%s) ", md_ptr->node, media_device_type(md_ptr->type));
		md_ptr++;
	}
	printf("\n");
}

char *get_associated_device(void *opaque,
			    char *last_seek,
			    enum device_type desired_type,
			    char *seek_device,
			    enum device_type seek_type)
{
	struct media_devices *md = opaque;
	struct media_device_entry *md_ptr = md->md_entry;
	int i, found = 0;
	char *prev, *p;

	if (seek_type != NONE && seek_device[0]) {
		/* Get just the device name */
		p = strrchr(seek_device, '/');
		if (p)
			seek_device = p + 1;

		/* Step 1: Find the seek node */
		for (i = 0; i < md->md_size; i++, md_ptr++) {
			if (md_ptr->type == seek_type &&
			    !strcmp(seek_device, md_ptr->node))
				break;
		}
		if (i == md->md_size)
			return NULL;
		i++;
		prev = md_ptr->device;
		md_ptr++;
		/* Step 2: find the associated node */
		for (;i < md->md_size && !strcmp(prev, md_ptr->device); i++, md_ptr++) {
			if (last_seek && !strcmp(md_ptr->node, last_seek)) {
				found = 1;
				continue;
			}
			if (last_seek && !found)
				continue;
			if (md_ptr->type == desired_type)
				return md_ptr->node;
		}
	} else {
		for (i = 0;i < md->md_size; i++, md_ptr++) {
			if (last_seek && !strcmp(md_ptr->node, last_seek)) {
				found = 1;
				continue;
			}
			if (last_seek && !found)
				continue;
			if (md_ptr->type == desired_type)
				return md_ptr->node;
		}
	}

	return NULL;
}

char *get_not_associated_device(void *opaque,
			    char *last_seek,
			    enum device_type desired_type,
			    enum device_type not_desired_type)
{
	struct media_devices *md = opaque;
	struct media_device_entry *md_ptr = md->md_entry;
	int i, skip = 0, found = 0;
	char *prev = "", *result = NULL;

	/* Step 1: Find a device without seek_type node */
	for (i = 0; i < md->md_size; i++, md_ptr++) {
		if (last_seek && !strcmp(md_ptr->node, last_seek)) {
			found = 1;
			continue;
		}
		if (last_seek && !found)
			continue;
		if (strcmp(prev, md_ptr->device)) {
			if (!skip && result)
				break;
			prev = md_ptr->device;
			skip = 0;
			result = NULL;
		}
		if (md_ptr->type == not_desired_type) {
			skip = 1;
		} else if (!skip && !result && md_ptr->type == desired_type) {
			result = md_ptr->node;
		}
	}
	if (skip)
		result = NULL;

	return result;
}
