/*
   Copyright (C) 2006 Mauro Carvalho Chehab <mchehab@infradead.org>

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

  */

#include <sys/time.h>
#include <linux/videodev2.h>

struct drv_list {
	void		*curr;
	struct drv_list	*next;
};

struct v4l2_driver {
	int			fd;	/* Driver descriptor */

	int			debug;

	struct v4l2_capability	cap;

	struct v4l2_streamparm	parm;

	struct drv_list		*stds,*inputs,*fmt_caps;
};

enum v4l2_direction {
	V4L2_GET     = 1,	// Bit 1
	V4L2_SET     = 2,	// Bit 2
	V4L2_SET_GET = 3,	// Bits 1 and 2 - sets then gets and compare
};

int v4l2_open (char *device, int debug, struct v4l2_driver *drv);
int v4l2_close (struct v4l2_driver *drv);
int v4l2_enum_stds (struct v4l2_driver *drv);
int v4l2_enum_input (struct v4l2_driver *drv);
int v4l2_enum_fmt_cap (struct v4l2_driver *drv);
int v4l2_get_parm (struct v4l2_driver *drv);
int v4l2_setget_std (struct v4l2_driver *drv, enum v4l2_direction dir, v4l2_std_id *id);
int v4l2_setget_input (struct v4l2_driver *drv, enum v4l2_direction dir, struct v4l2_input *input);
