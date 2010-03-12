/*
   Copyright (C) 2006 Mauro Carvalho Chehab <mchehab@infradead.org>

   The libv4l2util Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The libv4l2util Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
  */

#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <linux/videodev2.h>

struct drv_list {
	void		*curr;
	struct drv_list	*next;
};

struct v4l2_t_buf {
	void		*start;
	size_t		length;
};

typedef int v4l2_recebe_buffer (struct v4l2_buffer *v4l2_buf, struct v4l2_t_buf *buf);

struct v4l2_driver {
	int				fd;	/* Driver descriptor */

	int				debug;

	/* V4L2 structs */
	struct v4l2_capability		cap;
	struct v4l2_streamparm		parm;

	/* Several lists to be used to store enumbered values */
	struct drv_list			*stds,*inputs,*fmt_caps;

	/* Stream control */
	struct v4l2_requestbuffers	reqbuf;
	struct v4l2_buffer		**v4l2_bufs;
	struct v4l2_t_buf 		*bufs;
	uint32_t			sizeimage,n_bufs;

	/* Queue control */
	uint32_t			waitq, currq;
};

enum v4l2_direction {
	V4L2_GET		= 1,	// Bit 1
	V4L2_SET		= 2,	// Bit 2
	V4L2_SET_GET		= 3,	// Bits 1 and 2 - sets then gets and compare
	V4L2_TRY		= 4,	// Bit 3
	V4L2_TRY_SET		= 6,	// Bits 3 and 2 - try then sets
	V4L2_TRY_SET_GET	= 7,	// Bits 3, 2 and 1- try, sets and gets
};

int v4l2_open (char *device, int debug, struct v4l2_driver *drv);
int v4l2_close (struct v4l2_driver *drv);
int v4l2_enum_stds (struct v4l2_driver *drv);
int v4l2_enum_input (struct v4l2_driver *drv);
int v4l2_enum_fmt (struct v4l2_driver *drv,enum v4l2_buf_type type);
int v4l2_get_parm (struct v4l2_driver *drv);
int v4l2_setget_std (struct v4l2_driver *drv, enum v4l2_direction dir, v4l2_std_id *id);
int v4l2_setget_input (struct v4l2_driver *drv, enum v4l2_direction dir, struct v4l2_input *input);
int v4l2_gettryset_fmt_cap (struct v4l2_driver *drv, enum v4l2_direction dir,
		      struct v4l2_format *fmt,uint32_t width, uint32_t height,
		      uint32_t pixelformat, enum v4l2_field field);
int v4l2_mmap_bufs(struct v4l2_driver *drv, unsigned int num_buffers);
int v4l2_free_bufs(struct v4l2_driver *drv);
int v4l2_start_streaming(struct v4l2_driver *drv);
int v4l2_stop_streaming(struct v4l2_driver *drv);
int v4l2_rcvbuf(struct v4l2_driver *drv, v4l2_recebe_buffer *v4l2_rec_buf);
int v4l2_getset_freq (struct v4l2_driver *drv, enum v4l2_direction dir,
		      double *freq);

