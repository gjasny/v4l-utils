/*
    Sliced vbi autodetection demonstration utility
    Copyright (C) 2004  Hans Verkuil  <hverkuil@xs4all.nl>

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

/* This simple utility detects which VBI types are transmitted on
   each field/line. It serves both as example source and as a tool to
   test the sliced VBI implementation.

   Usage: sliced-vbi-detect [device]
   Without a device name as argument it will fallback to /dev/vbi0.
 */

#include <unistd.h>
#include <features.h>		/* Uses _GNU_SOURCE to define getsubopt in stdlib.h */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <math.h>

#include <linux/videodev2.h>

static void detect(int fh, struct v4l2_sliced_vbi_format *fmt)
{
	struct v4l2_sliced_vbi_data *buf = malloc(fmt->io_size);
	int cnt;

	for (cnt = 0; cnt < 5; cnt++) {
		int size = read(fh, buf, fmt->io_size);
		unsigned i;

		if (size <= 0) {
			printf("size = %d\n", size);
			break;
		}
		if (cnt == 0)
			continue;
		for (i = 0; i < size / sizeof(*buf); i++) {
			int field, line;

			line = buf[i].line;
			field = buf[i].field;
			if (buf[i].id == 0)
				continue;
			if (line < 0 || line >= 24) {
				printf("line %d out of range\n", line);
				free(buf);
				return;
			}
			fmt->service_lines[field][line] |= buf[i].id;
		}
	}
	free(buf);
}

static void v2s(int id)
{
	switch (id) {
		case V4L2_SLICED_TELETEXT_B: printf(" TELETEXT"); break;
		case V4L2_SLICED_CAPTION_525: printf(" CC"); break;
		case V4L2_SLICED_WSS_625: printf(" WSS"); break;
		case V4L2_SLICED_VPS: printf(" VPS"); break;
		default: printf(" UNKNOWN %x", id); break;
	}
}

int main(int argc, char **argv)
{
	char *device = "/dev/vbi0";
	struct v4l2_format vbifmt;
	struct v4l2_sliced_vbi_format vbiresult;
	int fh;
	int f, i, b;

	if (argc == 2)
		device = argv[1];
	fh = open(device, O_RDONLY);

	if (fh == -1) {
		fprintf(stderr, "cannot open %s\n", device);
		return 1;
	}
	memset(&vbiresult, 0, sizeof(vbiresult));
	for (i = 0; i < 16; i++) {
		int l;
		int set = 0;

		memset(&vbifmt, 0, sizeof(vbifmt));
		vbifmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
		for (l = 0; l < 24; l++) {
			vbifmt.fmt.sliced.service_lines[0][l] = 1 << i;
			vbifmt.fmt.sliced.service_lines[1][l] = 1 << i;
		}
		if (ioctl(fh, VIDIOC_S_FMT, &vbifmt) < 0) {
			if (errno == EINVAL)
				continue;
			perror("IVTV_IOC_S_VBI_FMT");
			exit(-1);
		}
		vbiresult.io_size = vbifmt.fmt.sliced.io_size;
		for (l = 0; l < 24; l++) {
			set |= vbifmt.fmt.sliced.service_lines[0][l] |
			       vbifmt.fmt.sliced.service_lines[1][l];
		}
		detect(fh, &vbiresult);
	}
	close(fh);
	for (f = 0; f < 2; f++) {
		printf("Field %d:\n", f);
		for (i = 6; i < 24; i++) {
			unsigned set = vbiresult.service_lines[f][i];

			printf("  Line %2d:", i);
			for (b = 0; b < 16; b++) {
				if (set & (1 << b))
					v2s(1 << b);
			}
			printf("\n");
		}
	}
	return 0;
}
