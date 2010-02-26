/*
   driver-test.c - This program tests V4L2 kernel drivers

   Copyright (C) 2006 Mauro Carvalho Chehab <mchehab@infradead.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 */

#include "../../utils/libv4l2util/v4l2_driver.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static int recebe_buffer (struct v4l2_buffer *v4l2_buf, struct v4l2_t_buf *buf)
{

	return 0;
}

int main(void)
{
	struct v4l2_driver drv;
	struct drv_list *cur;
	unsigned int count = 10, i;
	double freq;

	if (v4l2_open ("/dev/video0", 1,&drv)<0) {
		perror("open /dev/video0");
		return -1;
	}
	if (v4l2_enum_stds (&drv)<0) {
		perror("enum_stds");
		printf("Error! Driver is not reporting supported STD, frames/sec and number of lines!\n Trying to continue anyway...\n");
	} else {
		/* Tries all video standards */
		for (cur=drv.stds;cur!=NULL;cur=cur->next) {
			v4l2_std_id id=((struct v4l2_standard *)cur->curr)->id;
			if (cur->curr)
				if (v4l2_setget_std (&drv, V4L2_SET_GET, &id))
					perror("set_std");
		}
	}

	if (v4l2_enum_input (&drv)<0) {
		perror("enum_input");
	}

	/* Tries all video inputs */
	for (cur=drv.inputs;cur!=NULL;cur=cur->next) {
		struct v4l2_input input;
		input.index=((struct v4l2_input* )cur->curr)->index;
		if (cur->curr)
			if (v4l2_setget_input (&drv, V4L2_SET_GET, &input))
				perror("set_input");
	}

	if (v4l2_enum_fmt (&drv,V4L2_BUF_TYPE_VIDEO_CAPTURE)<0) {
		perror("enum_fmt_cap");
	}

	/* Tries all formats */
	for (cur=drv.fmt_caps;cur!=NULL;cur=cur->next) {
		struct v4l2_format fmt;
		memset (&fmt,0,sizeof(fmt));

		uint32_t	   pixelformat=((struct v4l2_fmtdesc *)cur->curr)->pixelformat;
		if (cur->curr) {
			if (v4l2_gettryset_fmt_cap (&drv,V4L2_SET,&fmt, 640, 480,
						pixelformat,V4L2_FIELD_ANY))
				perror("set_input");
		}
	}

	if (v4l2_get_parm (&drv)<0) {
		perror("get_parm");
	}


	v4l2_getset_freq (&drv,V4L2_GET, &freq);

	freq=0;
	v4l2_getset_freq (&drv,V4L2_SET, &freq);

	freq=121250000; /* 121.250 MHz */
	v4l2_getset_freq (&drv,V4L2_SET, &freq);

	printf("Preparing for frames...\n");
	fflush (stdout);
	sleep(1);

	v4l2_mmap_bufs(&drv, 2);

	v4l2_start_streaming(&drv);

	printf("Waiting for frames...\n");

	for (i=0;i<count;i++) {
		fd_set fds;
		struct timeval tv;
		int r;

		FD_ZERO (&fds);
		FD_SET (drv.fd, &fds);

		/* Timeout. */
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select (drv.fd + 1, &fds, NULL, NULL, &tv);
		if (-1 == r) {
			if (EINTR == errno)
				continue;

			perror ("select");
			return errno;
		}

		if (0 == r) {
			fprintf (stderr, "select timeout\n");
			return errno;
		}

		if (v4l2_rcvbuf(&drv, recebe_buffer))
			break;
	}

	printf("stopping streaming\n");
	v4l2_stop_streaming(&drv);

	if (v4l2_close (&drv)<0) {
		perror("close");
		return -1;
	}
	return 0;
}
