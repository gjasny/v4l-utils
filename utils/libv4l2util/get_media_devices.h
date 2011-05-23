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

/* Device type order will be used by sort */
enum device_type {
	UNKNOWN = 65535,
	V4L_VIDEO = 0,
	V4L_VBI,
	DVB_FRONTEND,
	DVB_DEMUX,
	DVB_DVR,
	DVB_NET,
	DVB_CA,
	SND_CARD,
	SND_CAP,
	SND_OUT,
	SND_CONTROL,
	SND_HW,
	/* TODO: Add dvb full-featured nodes */
};

struct media_devices {
	char *device;
	char *node;
	enum device_type type;
};

struct media_devices *discover_media_devices(unsigned int *md_size);
void free_media_devices(struct media_devices *md, unsigned int md_size);
void display_media_devices(struct media_devices *md, unsigned int size);
char *get_first_alsa_cap_device(struct media_devices *md, unsigned int size,
				char *v4l_device);
