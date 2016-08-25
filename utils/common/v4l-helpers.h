/*
 * V4L2 C helper header providing wrappers to simplify access to the various
 * v4l2 functions.
 *
 * Copyright 2014-2016 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * Alternatively you can redistribute this file under the terms of the
 * BSD license as stated below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. The names of its contributors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _V4L_HELPERS_H_
#define _V4L_HELPERS_H_

#include <linux/videodev2.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct v4l_fd {
	int fd;
	struct v4l2_capability cap;
	char devname[128];
	__u32 type;
	__u32 caps;
	bool trace;
	bool direct;
	bool have_query_ext_ctrl;
	bool have_ext_ctrls;
	bool have_next_ctrl;
	bool have_selection;

	int (*open)(struct v4l_fd *f, const char *file, int oflag, ...);
	int (*close)(struct v4l_fd *f);
	int (*ioctl)(struct v4l_fd *f, unsigned long cmd, ...);
	ssize_t (*read)(struct v4l_fd *f, void *buffer, size_t n);
	ssize_t (*write)(struct v4l_fd *f, const void *buffer, size_t n);
	void *(*mmap)(void *addr, size_t length, int prot, int flags,
		      struct v4l_fd *f, off_t offset);
	int (*munmap)(struct v4l_fd *f, void *addr, size_t length);
};

#ifdef __LIBV4L2_H

static inline int v4l_wrap_open(struct v4l_fd *f, const char *file, int oflag, ...)
{
 	return f->direct ? open(file, oflag) : v4l2_open(file, oflag);
}

static inline int v4l_wrap_close(struct v4l_fd *f)
{
	return f->direct ? close(f->fd) : v4l2_close(f->fd);
}

static inline ssize_t v4l_wrap_read(struct v4l_fd *f, void *buffer, size_t n)
{
	return f->direct ? read(f->fd, buffer, n) : v4l2_read(f->fd, buffer, n);
}

static inline ssize_t v4l_wrap_write(struct v4l_fd *f, const void *buffer, size_t n)
{
	return f->direct ? write(f->fd, buffer, n) : v4l2_write(f->fd, buffer, n);
}

static inline int v4l_wrap_ioctl(struct v4l_fd *f, unsigned long cmd, ...)
{
	void *arg;
	va_list ap;

	va_start(ap, cmd);
	arg = va_arg(ap, void *);
	va_end(ap);
	return f->direct ? ioctl(f->fd, cmd, arg) : v4l2_ioctl(f->fd, cmd, arg);
}

static inline void *v4l_wrap_mmap(void *start, size_t length, int prot, int flags,
		struct v4l_fd *f, off_t offset)
{
 	return f->direct ? mmap(start, length, prot, flags, f->fd, offset) :
		v4l2_mmap(start, length, prot, flags, f->fd, offset);
}

static inline int v4l_wrap_munmap(struct v4l_fd *f, void *start, size_t length)
{
 	return f->direct ? munmap(start, length) : v4l2_munmap(start, length);
}

static inline bool v4l_fd_g_direct(const struct v4l_fd *f)
{
	return f->direct;
}

static inline void v4l_fd_s_direct(struct v4l_fd *f, bool direct)
{
	f->direct = direct;
}

#else

static inline int v4l_wrap_open(struct v4l_fd *f, const char *file, int oflag, ...)
{
 	return open(file, oflag);
}

static inline int v4l_wrap_close(struct v4l_fd *f)
{
	return close(f->fd);
}

static inline ssize_t v4l_wrap_read(struct v4l_fd *f, void *buffer, size_t n)
{
	return read(f->fd, buffer, n);
}

static inline ssize_t v4l_wrap_write(struct v4l_fd *f, const void *buffer, size_t n)
{
	return write(f->fd, buffer, n);
}

static inline int v4l_wrap_ioctl(struct v4l_fd *f, unsigned long cmd, ...)
{
	void *arg;
	va_list ap;

	va_start(ap, cmd);
	arg = va_arg(ap, void *);
	va_end(ap);
	return ioctl(f->fd, cmd, arg);
}

static inline void *v4l_wrap_mmap(void *start, size_t length, int prot, int flags,
		struct v4l_fd *f, off_t offset)
{
 	return mmap(start, length, prot, flags, f->fd, offset);
}

static inline int v4l_wrap_munmap(struct v4l_fd *f, void *start, size_t length)
{
 	return munmap(start, length);
}

static inline bool v4l_fd_g_direct(const struct v4l_fd *f)
{
	return true;
}

static inline void v4l_fd_s_direct(struct v4l_fd *f, bool direct)
{
}

#endif

static inline void v4l_fd_init(struct v4l_fd *f)
{
	memset(f, 0, sizeof(*f));
	f->fd = -1;
	f->open = v4l_wrap_open;
	f->close = v4l_wrap_close;
	f->ioctl = v4l_wrap_ioctl;
	f->read = v4l_wrap_read;
	f->write = v4l_wrap_write;
	f->mmap = v4l_wrap_mmap;
	f->munmap = v4l_wrap_munmap;
}

static inline bool v4l_fd_g_trace(const struct v4l_fd *f)
{
	return f->trace;
}

static inline void v4l_fd_s_trace(struct v4l_fd *f, bool trace)
{
	f->trace = trace;
}

static inline int v4l_named_ioctl(struct v4l_fd *f,
		const char *cmd_name, unsigned long cmd, void *arg)
{
	int retval;
	int e;

	retval = f->ioctl(f, cmd, arg);
	e = retval == 0 ? 0 : errno;
	if (f->trace)
		fprintf(stderr, "\t\t%s returned %d (%s)\n",
				cmd_name, retval, strerror(e));
	return retval == -1 ? e : (retval ? -1 : 0);
}

#define v4l_ioctl(f, cmd, arg) v4l_named_ioctl(f, #cmd, cmd, arg)

static inline void *v4l_mmap(struct v4l_fd *f, size_t length, off_t offset)
{
	return f->mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, f, offset);
}

static inline int v4l_munmap(struct v4l_fd *f, void *start, size_t length)
{
	return f->munmap(f, start, length);
}

static inline ssize_t v4l_read(struct v4l_fd *f, void *buffer, size_t n)
{
	return f->read(f, buffer, n);
}

static inline ssize_t v4l_write(struct v4l_fd *f, const void *buffer, size_t n)
{
	return f->write(f, buffer, n);
}

static inline int v4l_close(struct v4l_fd *f)
{
	int res = f->close(f);

	f->caps = f->type = 0;
	f->fd = -1;
	return res;
}

static inline int v4l_querycap(struct v4l_fd *f, struct v4l2_capability *cap)
{
	return v4l_ioctl(f, VIDIOC_QUERYCAP, cap);
}

static inline __u32 v4l_capability_g_caps(const struct v4l2_capability *cap)
{
	return (cap->capabilities & V4L2_CAP_DEVICE_CAPS) ?
			cap->device_caps : cap->capabilities;
}

static inline __u32 v4l_g_type(const struct v4l_fd *f)
{
	return f->type;
}

static inline void v4l_s_type(struct v4l_fd *f, __u32 type)
{
	f->type = type;
}

static inline __u32 v4l_g_selection_type(const struct v4l_fd *f)
{
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return V4L2_BUF_TYPE_VIDEO_OUTPUT;
	return f->type;
}

static inline __u32 v4l_g_caps(const struct v4l_fd *f)
{
	return f->caps;
}

static inline bool v4l_has_vid_cap(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE |
				V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE);
}

static inline bool v4l_has_vid_out(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
				V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE);
}

static inline bool v4l_has_vid_m2m(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & (V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE);
}

static inline bool v4l_has_vid_mplane(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & (V4L2_CAP_VIDEO_CAPTURE_MPLANE |
				V4L2_CAP_VIDEO_OUTPUT_MPLANE |
				V4L2_CAP_VIDEO_M2M_MPLANE);
}

static inline bool v4l_has_overlay_cap(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_VIDEO_OVERLAY;
}

static inline bool v4l_has_overlay_out(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_VIDEO_OUTPUT_OVERLAY;
}

static inline bool v4l_has_raw_vbi_cap(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_VBI_CAPTURE;
}

static inline bool v4l_has_sliced_vbi_cap(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_SLICED_VBI_CAPTURE;
}

static inline bool v4l_has_vbi_cap(const struct v4l_fd *f)
{
	return v4l_has_raw_vbi_cap(f) || v4l_has_sliced_vbi_cap(f);
}

static inline bool v4l_has_raw_vbi_out(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_VBI_OUTPUT;
}

static inline bool v4l_has_sliced_vbi_out(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_SLICED_VBI_OUTPUT;
}

static inline bool v4l_has_vbi_out(const struct v4l_fd *f)
{
	return v4l_has_raw_vbi_out(f) || v4l_has_sliced_vbi_out(f);
}

static inline bool v4l_has_vbi(const struct v4l_fd *f)
{
	return v4l_has_vbi_cap(f) || v4l_has_vbi_out(f);
}

static inline bool v4l_has_radio_rx(const struct v4l_fd *f)
{
	return (v4l_g_caps(f) & V4L2_CAP_RADIO) &&
	       (v4l_g_caps(f) & V4L2_CAP_TUNER);
}

static inline bool v4l_has_radio_tx(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_MODULATOR;
}

static inline bool v4l_has_rds_cap(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_RDS_CAPTURE;
}

static inline bool v4l_has_rds_out(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_RDS_OUTPUT;
}

static inline bool v4l_has_sdr_cap(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_SDR_CAPTURE;
}

static inline bool v4l_has_sdr_out(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_SDR_OUTPUT;
}

static inline bool v4l_has_touch(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_TOUCH;
}

static inline bool v4l_has_hwseek(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_HW_FREQ_SEEK;
}

static inline bool v4l_has_rw(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_READWRITE;
}

static inline bool v4l_has_streaming(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_STREAMING;
}

static inline bool v4l_has_ext_pix_format(const struct v4l_fd *f)
{
	return v4l_g_caps(f) & V4L2_CAP_EXT_PIX_FORMAT;
}

static inline __u32 v4l_determine_type(const struct v4l_fd *f)
{
	if (v4l_has_vid_mplane(f))
		return v4l_has_vid_cap(f) ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
					    V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	if (v4l_has_vid_cap(f))
		return V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (v4l_has_vid_out(f))
		return V4L2_BUF_TYPE_VIDEO_OUTPUT;
	if (v4l_has_raw_vbi_cap(f))
		return V4L2_BUF_TYPE_VBI_CAPTURE;
	if (v4l_has_sliced_vbi_cap(f))
		return V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
	if (v4l_has_raw_vbi_out(f))
		return V4L2_BUF_TYPE_VBI_OUTPUT;
	if (v4l_has_sliced_vbi_out(f))
		return V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
	if (v4l_has_sdr_cap(f))
		return V4L2_BUF_TYPE_SDR_CAPTURE;
	if (v4l_has_sdr_out(f))
		return V4L2_BUF_TYPE_SDR_OUTPUT;
	return 0;
}

static inline int v4l_open(struct v4l_fd *f, const char *devname, bool non_blocking)
{
	struct v4l2_query_ext_ctrl qec;
	struct v4l2_ext_controls ec;
	struct v4l2_queryctrl qc;
	struct v4l2_selection sel;

	memset(&qec, 0, sizeof(qec));
	qec.id = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
	memset(&ec, 0, sizeof(ec));
	memset(&qc, 0, sizeof(qc));
	qc.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	memset(&sel, 0, sizeof(sel));

	f->fd = f->open(f, devname, O_RDWR | (non_blocking ? O_NONBLOCK : 0));

	if (f->fd < 0)
		return f->fd;
	if (f->devname != devname) {
		strncpy(f->devname, devname, sizeof(f->devname));
		f->devname[sizeof(f->devname) - 1] = '\0';
	}
	if (v4l_querycap(f, &f->cap)) {
		v4l_close(f);
		return -1;
	}
	f->caps = v4l_capability_g_caps(&f->cap);
	f->type = v4l_determine_type(f);

	f->have_query_ext_ctrl = v4l_ioctl(f, VIDIOC_QUERY_EXT_CTRL, &qec) == 0;
	f->have_ext_ctrls = v4l_ioctl(f, VIDIOC_TRY_EXT_CTRLS, &ec) == 0;
	f->have_next_ctrl = v4l_ioctl(f, VIDIOC_QUERYCTRL, &qc) == 0;
	sel.type = v4l_g_selection_type(f);
	sel.target = sel.type == V4L2_BUF_TYPE_VIDEO_CAPTURE ?
			V4L2_SEL_TGT_CROP : V4L2_SEL_TGT_COMPOSE;
	f->have_selection = v4l_ioctl(f, VIDIOC_G_SELECTION, &sel) != ENOTTY;

	return f->fd;
}

static inline int v4l_reopen(struct v4l_fd *f, bool non_blocking)
{
	f->close(f);
	return v4l_open(f, f->devname, non_blocking);
}

static inline void v4l_format_init(struct v4l2_format *fmt, unsigned type)
{
	memset(fmt, 0, sizeof(*fmt));
	fmt->type = type;
	if (fmt->type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
	    fmt->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		fmt->fmt.pix.priv = V4L2_PIX_FMT_PRIV_MAGIC;
}

static inline void v4l_format_s_width(struct v4l2_format *fmt, __u32 width)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.width = width;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.width = width;
		break;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		fmt->fmt.win.w.width = width;
		break;
	}
}

static inline __u32 v4l_format_g_width(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.width;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.width;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		return fmt->fmt.win.w.width;
	default:
		return 0;
	}
}

static inline void v4l_format_s_height(struct v4l2_format *fmt, __u32 height)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.height = height;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.height = height;
		break;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		fmt->fmt.win.w.height = height;
		break;
	}
}

static inline __u32 v4l_format_g_height(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.height;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.height;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		return fmt->fmt.win.w.height;
	default:
		return 0;
	}
}

static inline void v4l_format_s_pixelformat(struct v4l2_format *fmt, __u32 pixelformat)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.pixelformat = pixelformat;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.pixelformat = pixelformat;
		break;
	case V4L2_BUF_TYPE_SDR_CAPTURE:
	case V4L2_BUF_TYPE_SDR_OUTPUT:
		fmt->fmt.sdr.pixelformat = pixelformat;
		break;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		fmt->fmt.vbi.sample_format = pixelformat;
		break;
	}
}

static inline __u32 v4l_format_g_pixelformat(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.pixelformat;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.pixelformat;
	case V4L2_BUF_TYPE_SDR_CAPTURE:
	case V4L2_BUF_TYPE_SDR_OUTPUT:
		return fmt->fmt.sdr.pixelformat;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		return fmt->fmt.vbi.sample_format;
	default:
		return 0;
	}
}

static inline void v4l_format_s_field(struct v4l2_format *fmt, unsigned field)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.field = field;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.field = field;
		break;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		fmt->fmt.win.field = field;
		break;
	}
}

static inline unsigned v4l_format_g_field(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.field;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.field;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		return fmt->fmt.win.field;
	default:
		return V4L2_FIELD_NONE;
	}
}

static inline unsigned v4l_format_g_first_field(const struct v4l2_format *fmt,
						v4l2_std_id std)
{
	unsigned field = v4l_format_g_field(fmt);

	if (field != V4L2_FIELD_ALTERNATE)
		return field;
	if (std & V4L2_STD_525_60)
		return V4L2_FIELD_BOTTOM;
	return V4L2_FIELD_TOP;
}

static inline unsigned v4l_format_g_flds_per_frm(const struct v4l2_format *fmt)
{
	unsigned field = v4l_format_g_field(fmt);

	if (field == V4L2_FIELD_ALTERNATE ||
	    field == V4L2_FIELD_TOP || field == V4L2_FIELD_BOTTOM)
		return 2;
	return 1;
}

static inline void v4l_format_s_frame_height(struct v4l2_format *fmt, __u32 height)
{
	if (V4L2_FIELD_HAS_T_OR_B(v4l_format_g_field(fmt)))
		height /= 2;
	v4l_format_s_height(fmt, height);
}

static inline __u32 v4l_format_g_frame_height(const struct v4l2_format *fmt)
{
	__u32 height = v4l_format_g_height(fmt);

	if (V4L2_FIELD_HAS_T_OR_B(v4l_format_g_field(fmt)))
		return height * 2;
	return height;
}

static inline void v4l_format_s_colorspace(struct v4l2_format *fmt,
					       unsigned colorspace)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.colorspace = colorspace;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.colorspace = colorspace;
		break;
	}
}

static inline unsigned
v4l_format_g_colorspace(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.colorspace;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.colorspace;
	default:
		return 0;
	}
}

static inline void v4l_format_s_xfer_func(struct v4l2_format *fmt,
					       unsigned xfer_func)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.xfer_func = xfer_func;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.xfer_func = xfer_func;
		break;
	}
}

static inline unsigned
v4l_format_g_xfer_func(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.xfer_func;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.xfer_func;
	default:
		return 0;
	}
}

static inline void v4l_format_s_ycbcr_enc(struct v4l2_format *fmt,
					       unsigned ycbcr_enc)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.ycbcr_enc = ycbcr_enc;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.ycbcr_enc = ycbcr_enc;
		break;
	}
}

static inline unsigned
v4l_format_g_ycbcr_enc(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.ycbcr_enc;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.ycbcr_enc;
	default:
		return 0;
	}
}

static inline void v4l_format_s_quantization(struct v4l2_format *fmt,
					       unsigned quantization)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.quantization = quantization;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.quantization = quantization;
		break;
	}
}

static inline unsigned
v4l_format_g_quantization(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.quantization;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.quantization;
	default:
		return 0;
	}
}

static inline void v4l_format_s_flags(struct v4l2_format *fmt,
					       unsigned flags)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.flags = flags;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.flags = flags;
		break;
	}
}

static inline unsigned
v4l_format_g_flags(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.flags;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.flags;
	default:
		return 0;
	}
}

static inline void v4l_format_s_num_planes(struct v4l2_format *fmt, __u8 num_planes)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.num_planes = num_planes;
		break;
	}
}

static inline __u8
v4l_format_g_num_planes(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.num_planes;
	default:
		return 1;
	}
}

static inline void v4l_format_s_bytesperline(struct v4l2_format *fmt,
					     unsigned plane, __u32 bytesperline)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		if (plane == 0)
			fmt->fmt.pix.bytesperline = bytesperline;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.plane_fmt[plane].bytesperline = bytesperline;
		break;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		/* This assumes V4L2_PIX_FMT_GREY which is always the case */
		if (plane == 0)
			fmt->fmt.vbi.samples_per_line = bytesperline;
		break;
	}
}

static inline __u32
v4l_format_g_bytesperline(const struct v4l2_format *fmt, unsigned plane)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return plane ? 0 : fmt->fmt.pix.bytesperline;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.plane_fmt[plane].bytesperline;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		/* This assumes V4L2_PIX_FMT_GREY which is always the case */
		return plane ? 0 : fmt->fmt.vbi.samples_per_line;
	default:
		return 0;
	}
}

static inline void v4l_format_s_sizeimage(struct v4l2_format *fmt,
					  unsigned plane, __u32 sizeimage)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		if (plane == 0)
			fmt->fmt.pix.sizeimage = sizeimage;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.plane_fmt[plane].sizeimage = sizeimage;
		break;
	}
}

static inline __u32
v4l_format_g_sizeimage(const struct v4l2_format *fmt, unsigned plane)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return plane ? 0 : fmt->fmt.pix.sizeimage;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.plane_fmt[plane].sizeimage;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		/* This assumes V4L2_PIX_FMT_GREY which is always the case */
		return plane ? 0 : fmt->fmt.vbi.samples_per_line *
			(fmt->fmt.vbi.count[0] + fmt->fmt.vbi.count[1]);
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		return plane ? 0 : fmt->fmt.sliced.io_size;
	case V4L2_BUF_TYPE_SDR_CAPTURE:
	case V4L2_BUF_TYPE_SDR_OUTPUT:
		return plane ? 0 : fmt->fmt.sdr.buffersize;
	default:
		return 0;
	}
}

static inline int v4l_g_fmt(struct v4l_fd *f, struct v4l2_format *fmt, unsigned type)
{
	v4l_format_init(fmt, type ? type : f->type);
	return v4l_ioctl(f, VIDIOC_G_FMT, fmt);
}

static inline int v4l_try_fmt(struct v4l_fd *f, struct v4l2_format *fmt, bool zero_bpl)
{
	/*
	 * Some drivers allow applications to set bytesperline to a larger value.
	 * In most cases you just want the driver to fill in the bytesperline field
	 * and so you have to zero bytesperline first.
	 */
	if (zero_bpl) {
		__u8 p;

		for (p = 0; p < v4l_format_g_num_planes(fmt); p++)
			v4l_format_s_bytesperline(fmt, p, 0);
	}
	return v4l_ioctl(f, VIDIOC_TRY_FMT, fmt);
}

static inline int v4l_s_fmt(struct v4l_fd *f, struct v4l2_format *fmt, bool zero_bpl)
{
	/*
	 * Some drivers allow applications to set bytesperline to a larger value.
	 * In most cases you just want the driver to fill in the bytesperline field
	 * and so you have to zero bytesperline first.
	 */
	if (zero_bpl) {
		__u8 p;

		for (p = 0; p < v4l_format_g_num_planes(fmt); p++)
			v4l_format_s_bytesperline(fmt, p, 0);
	}
	return v4l_ioctl(f, VIDIOC_S_FMT, fmt);
}

struct v4l_buffer {
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buf;
};

static inline void v4l_buffer_init(struct v4l_buffer *buf,
		unsigned type, unsigned memory, unsigned index)
{
	memset(buf, 0, sizeof(*buf));
	buf->buf.type = type;
	buf->buf.memory = memory;
	buf->buf.index = index;
	if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
		buf->buf.m.planes = buf->planes;
		buf->buf.length = VIDEO_MAX_PLANES;
	}
}

static inline bool v4l_type_is_planar(unsigned type)
{
       return V4L2_TYPE_IS_MULTIPLANAR(type);
}

static inline bool v4l_type_is_output(unsigned type)
{
       return V4L2_TYPE_IS_OUTPUT(type);
}

static inline bool v4l_type_is_capture(unsigned type)
{
       return !v4l_type_is_output(type);
}

static inline bool v4l_type_is_video(unsigned type)
{
       switch (type) {
       case V4L2_BUF_TYPE_VIDEO_CAPTURE:
       case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
       case V4L2_BUF_TYPE_VIDEO_OUTPUT:
       case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
               return true;
       default:
               return false;
       }
}

static inline bool v4l_type_is_raw_vbi(unsigned type)
{
       return type == V4L2_BUF_TYPE_VBI_CAPTURE ||
              type == V4L2_BUF_TYPE_VBI_OUTPUT;
}

static inline bool v4l_type_is_sliced_vbi(unsigned type)
{
       return type == V4L2_BUF_TYPE_SLICED_VBI_CAPTURE ||
              type == V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
}

static inline bool v4l_type_is_vbi(unsigned type)
{
       return v4l_type_is_raw_vbi(type) || v4l_type_is_sliced_vbi(type);
}

static inline bool v4l_type_is_overlay(unsigned type)
{
       return type == V4L2_BUF_TYPE_VIDEO_OVERLAY ||
              type == V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
}

static inline bool v4l_type_is_sdr(unsigned type)
{
       return type == V4L2_BUF_TYPE_SDR_CAPTURE ||
	      type == V4L2_BUF_TYPE_SDR_OUTPUT;
}

static inline unsigned v4l_type_invert(unsigned type)
{
	if (v4l_type_is_planar(type))
		return v4l_type_is_output(type) ?
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	return v4l_type_is_output(type) ?
		V4L2_BUF_TYPE_VIDEO_CAPTURE :
		V4L2_BUF_TYPE_VIDEO_OUTPUT;
}

static inline unsigned v4l_buffer_g_num_planes(const struct v4l_buffer *buf)
{
	if (v4l_type_is_planar(buf->buf.type))
		return buf->buf.length;
	return 1;
}

static inline __u32 v4l_buffer_g_index(const struct v4l_buffer *buf)
{
	return buf->buf.index;
}

static inline void v4l_buffer_s_index(struct v4l_buffer *buf, __u32 index)
{
	buf->buf.index = index;
}

static inline unsigned v4l_buffer_g_type(const struct v4l_buffer *buf)
{
	return buf->buf.type;
}

static inline unsigned v4l_buffer_g_memory(const struct v4l_buffer *buf)
{
	return buf->buf.memory;
}

static inline __u32 v4l_buffer_g_flags(const struct v4l_buffer *buf)
{
	return buf->buf.flags;
}

static inline void v4l_buffer_s_flags(struct v4l_buffer *buf, __u32 flags)
{
	buf->buf.flags = flags;
}

static inline void v4l_buffer_or_flags(struct v4l_buffer *buf, __u32 flags)
{
	buf->buf.flags |= flags;
}

static inline unsigned v4l_buffer_g_field(const struct v4l_buffer *buf)
{
	return buf->buf.field;
}

static inline void v4l_buffer_s_field(struct v4l_buffer *buf, unsigned field)
{
	buf->buf.field = field;
}

static inline __u32 v4l_buffer_g_sequence(const struct v4l_buffer *buf)
{
	return buf->buf.sequence;
}

static inline const struct timeval *v4l_buffer_g_timestamp(const struct v4l_buffer *buf)
{
	return &buf->buf.timestamp;
}

static inline void v4l_buffer_s_timestamp(struct v4l_buffer *buf, const struct timeval *tv)
{
	buf->buf.timestamp = *tv;
}

static inline void v4l_buffer_s_timestamp_ts(struct v4l_buffer *buf, const struct timespec *ts)
{
	buf->buf.timestamp.tv_sec = ts->tv_sec;
	buf->buf.timestamp.tv_usec = ts->tv_nsec / 1000;
}

static inline void v4l_buffer_s_timestamp_clock(struct v4l_buffer *buf)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	v4l_buffer_s_timestamp_ts(buf, &ts);
}

static inline const struct v4l2_timecode *v4l_buffer_g_timecode(const struct v4l_buffer *buf)
{
	return &buf->buf.timecode;
}

static inline void v4l_buffer_s_timecode(struct v4l_buffer *buf, const struct v4l2_timecode *tc)
{
	buf->buf.timecode = *tc;
}

static inline __u32 v4l_buffer_g_timestamp_type(const struct v4l_buffer *buf)
{
	return buf->buf.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK;
}

static inline bool v4l_buffer_is_copy(const struct v4l_buffer *buf)
{
	return v4l_buffer_g_timestamp_type(buf) == V4L2_BUF_FLAG_TIMESTAMP_COPY;
}

static inline __u32 v4l_buffer_g_timestamp_src(const struct v4l_buffer *buf)
{
	return buf->buf.flags & V4L2_BUF_FLAG_TSTAMP_SRC_MASK;
}

static inline void v4l_buffer_s_timestamp_src(struct v4l_buffer *buf, __u32 src)
{
	buf->buf.flags &= ~V4L2_BUF_FLAG_TSTAMP_SRC_MASK;
	buf->buf.flags |= src & V4L2_BUF_FLAG_TSTAMP_SRC_MASK;
}

static inline unsigned v4l_buffer_g_length(const struct v4l_buffer *buf, unsigned plane)
{
	if (v4l_type_is_planar(buf->buf.type))
		return buf->planes[plane].length;
	return plane ? 0 : buf->buf.length;
}

static inline void v4l_buffer_s_length(struct v4l_buffer *buf, unsigned plane, unsigned length)
{
	if (v4l_type_is_planar(buf->buf.type))
		buf->planes[plane].length = length;
	else if (plane == 0)
		buf->buf.length = length;
}

static inline unsigned v4l_buffer_g_bytesused(const struct v4l_buffer *buf, unsigned plane)
{
	if (v4l_type_is_planar(buf->buf.type))
		return buf->planes[plane].bytesused;
	return plane ? 0 : buf->buf.bytesused;
}

static inline void v4l_buffer_s_bytesused(struct v4l_buffer *buf, unsigned plane, __u32 bytesused)
{
	if (v4l_type_is_planar(buf->buf.type))
		buf->planes[plane].bytesused = bytesused;
	else if (plane == 0)
		buf->buf.bytesused = bytesused;
}

static inline unsigned v4l_buffer_g_data_offset(const struct v4l_buffer *buf, unsigned plane)
{
	if (v4l_type_is_planar(buf->buf.type))
		return buf->planes[plane].data_offset;
	return 0;
}

static inline void v4l_buffer_s_data_offset(struct v4l_buffer *buf, unsigned plane, __u32 data_offset)
{
	if (v4l_type_is_planar(buf->buf.type))
		buf->planes[plane].data_offset = data_offset;
}

static inline __u32 v4l_buffer_g_mem_offset(const struct v4l_buffer *buf, unsigned plane)
{
	if (v4l_type_is_planar(buf->buf.type))
		return buf->planes[plane].m.mem_offset;
	return plane ? 0 : buf->buf.m.offset;
}

static inline void v4l_buffer_s_userptr(struct v4l_buffer *buf, unsigned plane, void *userptr)
{
	if (v4l_type_is_planar(buf->buf.type))
		buf->planes[plane].m.userptr = (unsigned long)userptr;
	else if (plane == 0)
		buf->buf.m.userptr = (unsigned long)userptr;
}

static inline void *v4l_buffer_g_userptr(const struct v4l_buffer *buf, unsigned plane)
{
	if (v4l_type_is_planar(buf->buf.type))
		return (void *)buf->planes[plane].m.userptr;
	return plane ? NULL : (void *)buf->buf.m.userptr;
}

static inline void v4l_buffer_s_fd(struct v4l_buffer *buf, unsigned plane, int fd)
{
	if (v4l_type_is_planar(buf->buf.type))
		buf->planes[plane].m.fd = fd;
	else if (plane == 0)
		buf->buf.m.fd = fd;
}

static inline int v4l_buffer_g_fd(const struct v4l_buffer *buf, unsigned plane)
{
	if (v4l_type_is_planar(buf->buf.type))
		return buf->planes[plane].m.fd;
	return plane ? -1 : buf->buf.m.fd;
}

static inline int v4l_buffer_prepare_buf(struct v4l_fd *f, struct v4l_buffer *buf)
{
	return v4l_ioctl(f, VIDIOC_PREPARE_BUF, &buf->buf);
}

static inline int v4l_buffer_qbuf(struct v4l_fd *f, struct v4l_buffer *buf)
{
	return v4l_ioctl(f, VIDIOC_QBUF, &buf->buf);
}

static inline int v4l_buffer_dqbuf(struct v4l_fd *f, struct v4l_buffer *buf)
{
	return v4l_ioctl(f, VIDIOC_DQBUF, &buf->buf);
}

static inline int v4l_buffer_querybuf(struct v4l_fd *f, struct v4l_buffer *buf, unsigned index)
{
	v4l_buffer_s_index(buf, index);
	return v4l_ioctl(f, VIDIOC_QUERYBUF, &buf->buf);
}

struct v4l_queue {
	unsigned type;
	unsigned memory;
	unsigned buffers;
	unsigned num_planes;

	__u32 lengths[VIDEO_MAX_PLANES];
	__u32 mem_offsets[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];
	void *mmappings[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];
	unsigned long userptrs[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];
	int fds[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];
};

static inline void v4l_queue_init(struct v4l_queue *q,
		unsigned type, unsigned memory)
{
	unsigned i, p;

	memset(q, 0, sizeof(*q));
	q->type = type;
	q->memory = memory;
	for (i = 0; i < VIDEO_MAX_FRAME; i++)
		for (p = 0; p < VIDEO_MAX_PLANES; p++)
			q->fds[i][p] = -1;
}

static inline unsigned v4l_queue_g_type(const struct v4l_queue *q) { return q->type; }
static inline unsigned v4l_queue_g_memory(const struct v4l_queue *q) { return q->memory; }
static inline unsigned v4l_queue_g_buffers(const struct v4l_queue *q) { return q->buffers; }
static inline unsigned v4l_queue_g_num_planes(const struct v4l_queue *q) { return q->num_planes; }

static inline __u32 v4l_queue_g_length(const struct v4l_queue *q, unsigned plane)
{
	return q->lengths[plane];
}

static inline __u32 v4l_queue_g_mem_offset(const struct v4l_queue *q, unsigned index, unsigned plane)
{
	return q->mem_offsets[index][plane];
}

static inline void v4l_queue_s_mmapping(struct v4l_queue *q, unsigned index, unsigned plane, void *m)
{
	q->mmappings[index][plane] = m;
}

static inline void *v4l_queue_g_mmapping(const struct v4l_queue *q, unsigned index, unsigned plane)
{
	if (index >= v4l_queue_g_buffers(q) || plane >= v4l_queue_g_num_planes(q))
		return NULL;
	return q->mmappings[index][plane];
}

static inline void v4l_queue_s_userptr(struct v4l_queue *q, unsigned index, unsigned plane, void *m)
{
	q->userptrs[index][plane] = (unsigned long)m;
}

static inline void *v4l_queue_g_userptr(const struct v4l_queue *q, unsigned index, unsigned plane)
{
	if (index >= v4l_queue_g_buffers(q) || plane >= v4l_queue_g_num_planes(q))
		return NULL;
	return (void *)q->userptrs[index][plane];
}

static inline void v4l_queue_s_fd(struct v4l_queue *q, unsigned index, unsigned plane, int fd)
{
	q->fds[index][plane] = fd;
}

static inline int v4l_queue_g_fd(const struct v4l_queue *q, unsigned index, unsigned plane)
{
	return q->fds[index][plane];
}

static inline void *v4l_queue_g_dataptr(const struct v4l_queue *q, unsigned index, unsigned plane)
{
	if (q->memory == V4L2_MEMORY_USERPTR)
		return v4l_queue_g_userptr(q, index, plane);
	return v4l_queue_g_mmapping(q, index, plane);
}

static inline int v4l_queue_querybufs(struct v4l_fd *f, struct v4l_queue *q, unsigned from)
{
	unsigned b, p;
	int ret;

	for (b = from; b < v4l_queue_g_buffers(q); b++) {
		struct v4l_buffer buf;

		v4l_buffer_init(&buf, v4l_queue_g_type(q), v4l_queue_g_memory(q), b);
		ret = v4l_ioctl(f, VIDIOC_QUERYBUF, &buf.buf);
		if (ret)
			return ret;
		if (b == 0) {
			q->num_planes = v4l_buffer_g_num_planes(&buf);
			for (p = 0; p < v4l_queue_g_num_planes(q); p++)
				q->lengths[p] = v4l_buffer_g_length(&buf, p);
		}
		if (q->memory == V4L2_MEMORY_MMAP)
			for (p = 0; p < q->num_planes; p++)
				q->mem_offsets[b][p] = v4l_buffer_g_mem_offset(&buf, p);
	}
	return 0;
}

static inline int v4l_queue_reqbufs(struct v4l_fd *f,
		struct v4l_queue *q, unsigned count)
{
	struct v4l2_requestbuffers reqbufs;
	int ret;

	reqbufs.type = q->type;
	reqbufs.memory = q->memory;
	reqbufs.count = count;
	memset(reqbufs.reserved, 0, sizeof(reqbufs.reserved));
	/*
	 * Problem: if REQBUFS returns an error, did it free any old
	 * buffers or not?
	 */
	ret = v4l_ioctl(f, VIDIOC_REQBUFS, &reqbufs);
	if (ret)
		return ret;
	q->buffers = reqbufs.count;
	return v4l_queue_querybufs(f, q, 0);
}

static inline bool v4l_queue_has_create_bufs(struct v4l_fd *f, const struct v4l_queue *q)
{
	struct v4l2_create_buffers createbufs;

	memset(&createbufs, 0, sizeof(createbufs));
	createbufs.format.type = q->type;
	createbufs.memory = q->memory;
	return v4l_ioctl(f, VIDIOC_CREATE_BUFS, &createbufs) == 0;
}

static inline int v4l_queue_create_bufs(struct v4l_fd *f,
		struct v4l_queue *q, unsigned count, const struct v4l2_format *fmt)
{
	struct v4l2_create_buffers createbufs;
	int ret;

	createbufs.format.type = q->type;
	createbufs.memory = q->memory;
	createbufs.count = count;
	if (fmt) {
		createbufs.format = *fmt;
	} else {
		ret = v4l_g_fmt(f, &createbufs.format, q->type);
		if (ret)
			return ret;
	}
	memset(createbufs.reserved, 0, sizeof(createbufs.reserved));
	ret = v4l_ioctl(f, VIDIOC_CREATE_BUFS, &createbufs);
	if (ret)
		return ret;
	q->buffers += createbufs.count;
	return v4l_queue_querybufs(f, q, q->buffers - createbufs.count);
}

static inline int v4l_queue_mmap_bufs(struct v4l_fd *f,
		struct v4l_queue *q, unsigned from)
{
	unsigned b, p;

	if (q->memory != V4L2_MEMORY_MMAP && q->memory != V4L2_MEMORY_DMABUF)
		return 0;

	for (b = 0; b < v4l_queue_g_buffers(q); b++) {
		for (p = 0; p < v4l_queue_g_num_planes(q); p++) {
			void *m = MAP_FAILED;

			if (q->memory == V4L2_MEMORY_MMAP)
				m = v4l_mmap(f, v4l_queue_g_length(q, p), v4l_queue_g_mem_offset(q, b, p));
			else if (q->memory == V4L2_MEMORY_DMABUF)
				m = mmap(NULL, v4l_queue_g_length(q, p),
						PROT_READ | PROT_WRITE, MAP_SHARED,
						v4l_queue_g_fd(q, b, p), 0);

			if (m == MAP_FAILED)
				return errno;
			v4l_queue_s_mmapping(q, b, p, m);
		}
	}
	return 0;
}
static inline int v4l_queue_munmap_bufs(struct v4l_fd *f, struct v4l_queue *q)
{
	unsigned b, p;
	int ret = 0;

	if (q->memory != V4L2_MEMORY_MMAP && q->memory != V4L2_MEMORY_DMABUF)
		return 0;

	for (b = 0; b < v4l_queue_g_buffers(q); b++) {
		for (p = 0; p < v4l_queue_g_num_planes(q); p++) {
			void *m = v4l_queue_g_mmapping(q, b, p);

			if (m == NULL)
				continue;

			if (q->memory == V4L2_MEMORY_MMAP)
				ret = v4l_munmap(f, m, v4l_queue_g_length(q, p));
			else if (q->memory == V4L2_MEMORY_DMABUF)
				ret = munmap(m, v4l_queue_g_length(q, p)) ? errno : 0;
			if (ret)
				return ret;
			v4l_queue_s_mmapping(q, b, p, NULL);
		}
	}
	return 0;
}

static inline int v4l_queue_alloc_bufs(struct v4l_fd *f,
		struct v4l_queue *q, unsigned from)
{
	unsigned b, p;

	if (q->memory != V4L2_MEMORY_USERPTR)
		return 0;
	for (b = 0; b < v4l_queue_g_buffers(q); b++) {
		for (p = 0; p < v4l_queue_g_num_planes(q); p++) {
			void *m = malloc(v4l_queue_g_length(q, p));

			if (m == NULL)
				return errno;
			v4l_queue_s_userptr(q, b, p, m);
		}
	}
	return 0;
}

static inline int v4l_queue_free_bufs(struct v4l_queue *q)
{
	unsigned b, p;

	if (q->memory != V4L2_MEMORY_USERPTR)
		return 0;
	for (b = 0; b < v4l_queue_g_buffers(q); b++) {
		for (p = 0; p < v4l_queue_g_num_planes(q); p++) {
			free(v4l_queue_g_userptr(q, b, p));
			v4l_queue_s_userptr(q, b, p, NULL);
		}
	}
	return 0;
}

static inline int v4l_queue_obtain_bufs(struct v4l_fd *f,
		struct v4l_queue *q, unsigned from)
{
	if (q->memory == V4L2_MEMORY_USERPTR)
		return v4l_queue_alloc_bufs(f, q, from);
	return v4l_queue_mmap_bufs(f, q, from);
}

static inline int v4l_queue_release_bufs(struct v4l_fd *f, struct v4l_queue *q)
{
	if (q->memory == V4L2_MEMORY_USERPTR)
		return v4l_queue_free_bufs(q);
	return v4l_queue_munmap_bufs(f, q);
}


static inline bool v4l_queue_has_expbuf(struct v4l_fd *f)
{
	struct v4l2_exportbuffer expbuf;

	memset(&expbuf, 0, sizeof(expbuf));
	return v4l_ioctl(f, VIDIOC_EXPBUF, &expbuf) != ENOTTY;
}

static inline int v4l_queue_export_bufs(struct v4l_fd *f, struct v4l_queue *q)
{
	struct v4l2_exportbuffer expbuf;
	unsigned b, p;
	int ret = 0;

	expbuf.type = q->type;
	expbuf.flags = O_RDWR;
	memset(expbuf.reserved, 0, sizeof(expbuf.reserved));
	for (b = 0; b < v4l_queue_g_buffers(q); b++) {
		expbuf.index = b;
		for (p = 0; p < v4l_queue_g_num_planes(q); p++) {
			expbuf.plane = p;
			ret = v4l_ioctl(f, VIDIOC_EXPBUF, &expbuf);
			if (ret)
				return ret;
			v4l_queue_s_fd(q, b, p, expbuf.fd);
		}
	}
	return 0;
}

static inline void v4l_queue_close_exported_fds(struct v4l_queue *q)
{
	unsigned b, p;

	if (q->memory != V4L2_MEMORY_MMAP)
		return;

	for (b = 0; b < v4l_queue_g_buffers(q); b++) {
		for (p = 0; p < v4l_queue_g_num_planes(q); p++) {
			int fd = v4l_queue_g_fd(q, b, p);

			if (fd != -1) {
				close(fd);
				v4l_queue_s_fd(q, b, p, -1);
			}
		}
	}
}

static inline void v4l_queue_free(struct v4l_fd *f, struct v4l_queue *q)
{
	v4l_ioctl(f, VIDIOC_STREAMOFF, &q->type);
	v4l_queue_release_bufs(f, q);
	v4l_queue_close_exported_fds(q);
	v4l_queue_reqbufs(f, q, 0);
}

static inline void v4l_queue_buffer_init(const struct v4l_queue *q, struct v4l_buffer *buf, unsigned index)
{
	unsigned p;
		
	v4l_buffer_init(buf, v4l_queue_g_type(q), v4l_queue_g_memory(q), index);
	if (v4l_type_is_planar(q->type)) {
		buf->buf.length = v4l_queue_g_num_planes(q);
		buf->buf.m.planes = buf->planes;
	}
	switch (q->memory) {
	case V4L2_MEMORY_USERPTR:
		for (p = 0; p < v4l_queue_g_num_planes(q); p++) {
			v4l_buffer_s_userptr(buf, p, v4l_queue_g_userptr(q, index, p));
			v4l_buffer_s_length(buf, p, v4l_queue_g_length(q, p));
		}
		break;
	case V4L2_MEMORY_DMABUF:
		for (p = 0; p < v4l_queue_g_num_planes(q); p++)
			v4l_buffer_s_fd(buf, p, v4l_queue_g_fd(q, index, p));
		break;
	default:
		break;
	}
}

static inline int v4l_query_ext_ctrl(v4l_fd *f, struct v4l2_query_ext_ctrl *qec,
		bool next_ctrl, bool next_compound)
{
	struct v4l2_queryctrl qc;
	int ret;

	if (next_compound && !f->have_query_ext_ctrl) {
		if (!next_ctrl)
			return -EINVAL;
		next_compound = false;
	}
	if (next_compound)
		qec->id |= V4L2_CTRL_FLAG_NEXT_COMPOUND;
	if (next_ctrl) {
		if (f->have_next_ctrl)
			qec->id |= V4L2_CTRL_FLAG_NEXT_CTRL;
		else
			qec->id = qec->id ? qec->id + 1 : V4L2_CID_BASE;
	}
	if (f->have_query_ext_ctrl)
		return v4l_ioctl(f, VIDIOC_QUERY_EXT_CTRL, qec);

	for (;;) {
		if (qec->id == V4L2_CID_LASTP1 && next_ctrl)
			qec->id = V4L2_CID_PRIVATE_BASE;
		qc.id = qec->id;
		ret = v4l_ioctl(f, VIDIOC_QUERYCTRL, &qc);
		if (!ret)
			break;
		if (ret != EINVAL)
			return ret;
		if (!next_ctrl || f->have_next_ctrl)
			return ret;
		if (qec->id >= V4L2_CID_PRIVATE_BASE)
			return ret;
		qec->id++;
	}
	qec->id = qc.id;
	qec->type = qc.type;
	memcpy(qec->name, qc.name, sizeof(qec->name));
	qec->minimum = qc.minimum;
	if (qc.type == V4L2_CTRL_TYPE_BITMASK) {
		qec->maximum = (__u32)qc.maximum;
		qec->default_value = (__u32)qc.default_value;
	} else {
		qec->maximum = qc.maximum;
		qec->default_value = qc.default_value;
	}
	qec->step = qc.step;
	qec->flags = qc.flags;
	qec->elems = 1;
	qec->nr_of_dims = 0;
	memset(qec->dims, 0, sizeof(qec->dims));
	switch (qec->type) {
	case V4L2_CTRL_TYPE_INTEGER64:
		qec->elem_size = sizeof(__s64);
		qec->minimum = 0x8000000000000000ULL;
		qec->maximum = 0x7fffffffffffffffULL;
		qec->step = 1;
		break;
	case V4L2_CTRL_TYPE_STRING:
		qec->elem_size = qc.maximum + 1;
		break;
	default:
		qec->elem_size = sizeof(__s32);
		break;
	}
	memset(qec->reserved, 0, sizeof(qec->reserved));
	return 0;
}

static inline int v4l_g_ext_ctrls(v4l_fd *f, struct v4l2_ext_controls *ec)
{
	unsigned i;

	if (f->have_ext_ctrls)
		return v4l_ioctl(f, VIDIOC_G_EXT_CTRLS, ec);
	if (ec->count == 0)
		return 0;
	for (i = 0; i < ec->count; i++) {
		struct v4l2_control c = { ec->controls[i].id, 0 };
		int ret = v4l_ioctl(f, VIDIOC_G_CTRL, &c);

		if (ret) {
			ec->error_idx = i;
			return ret;
		}
		ec->controls[i].value = c.value;
	}
	return 0;
}

static inline int v4l_s_ext_ctrls(v4l_fd *f, struct v4l2_ext_controls *ec)
{
	unsigned i;

	if (f->have_ext_ctrls)
		return v4l_ioctl(f, VIDIOC_S_EXT_CTRLS, ec);
	if (ec->count == 0)
		return 0;
	for (i = 0; i < ec->count; i++) {
		struct v4l2_control c = { ec->controls[i].id, ec->controls[i].value };
		int ret = v4l_ioctl(f, VIDIOC_S_CTRL, &c);

		if (ret) {
			ec->error_idx = i;
			return ret;
		}
	}
	return 0;
}

static inline int v4l_try_ext_ctrls(v4l_fd *f, struct v4l2_ext_controls *ec)
{
	unsigned i;

	if (f->have_ext_ctrls)
		return v4l_ioctl(f, VIDIOC_TRY_EXT_CTRLS, ec);
	if (ec->count == 0)
		return 0;
	for (i = 0; i < ec->count; i++) {
		struct v4l2_queryctrl qc;
		int ret;

		memset(&qc, 0, sizeof(qc));
		qc.id = ec->controls[i].id;
		ret = v4l_ioctl(f, VIDIOC_QUERYCTRL, &qc);

		if (ret || qc.type == V4L2_CTRL_TYPE_STRING ||
			   qc.type == V4L2_CTRL_TYPE_INTEGER64) {
			ec->error_idx = i;
			return ret ? ret : EINVAL;
		}
	}
	return 0;
}

static inline int v4l_g_selection(v4l_fd *f, struct v4l2_selection *sel)
{
	struct v4l2_cropcap cc;
	struct v4l2_crop crop;
	int ret;

	if (f->have_selection)
		return v4l_ioctl(f, VIDIOC_G_SELECTION, sel);
	crop.type = sel->type;
	cc.type = sel->type;
	ret = v4l_ioctl(f, VIDIOC_CROPCAP, &cc);
	if (ret)
		return ret;
	ret = v4l_ioctl(f, VIDIOC_G_CROP, &crop);
	if (ret)
		return ret;
	if (sel->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		switch (sel->target) {
		case V4L2_SEL_TGT_CROP:
			sel->r = crop.c;
			return 0;
		case V4L2_SEL_TGT_CROP_DEFAULT:
			sel->r = cc.defrect;
			return 0;
		case V4L2_SEL_TGT_CROP_BOUNDS:
			sel->r = cc.bounds;
			return 0;
		default:
			return EINVAL;
		}
	}
	switch (sel->target) {
	case V4L2_SEL_TGT_COMPOSE:
		sel->r = crop.c;
		return 0;
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
		sel->r = cc.defrect;
		return 0;
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		sel->r = cc.bounds;
		return 0;
	default:
		return EINVAL;
	}
}

static inline int v4l_s_selection(v4l_fd *f, struct v4l2_selection *sel)
{
	struct v4l2_crop crop;
	int ret;

	if (f->have_selection)
		return v4l_ioctl(f, VIDIOC_S_SELECTION, sel);
	crop.type = sel->type;
	ret = v4l_ioctl(f, VIDIOC_G_CROP, &crop);
	if (ret)
		return ret;
	if (sel->type == V4L2_BUF_TYPE_VIDEO_CAPTURE &&
	    sel->target == V4L2_SEL_TGT_CROP) {
		crop.c = sel->r;
		return v4l_ioctl(f, VIDIOC_S_CROP, &crop);
	}
	if (sel->type == V4L2_BUF_TYPE_VIDEO_OUTPUT &&
	    sel->target == V4L2_SEL_TGT_COMPOSE) {
		crop.c = sel->r;
		return v4l_ioctl(f, VIDIOC_S_CROP, &crop);
	}
	return EINVAL;
}

static inline void v4l_frame_selection(struct v4l2_selection *sel, bool to_frame)
{
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
		if (!V4L2_TYPE_IS_OUTPUT(sel->type))
			return;
		break;
	case V4L2_SEL_TGT_COMPOSE:
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
	case V4L2_SEL_TGT_COMPOSE_PADDED:
		if (V4L2_TYPE_IS_OUTPUT(sel->type))
			return;
		break;
	default:
		return;
	}
	if (to_frame) {
		sel->r.top *= 2;
		sel->r.height *= 2;
	} else {
		sel->r.top /= 2;
		sel->r.height /= 2;
	}
}

static inline int v4l_g_frame_selection(v4l_fd *f, struct v4l2_selection *sel, __u32 field)
{
	int ret = v4l_g_selection(f, sel);

	if (V4L2_FIELD_HAS_T_OR_B(field))
		v4l_frame_selection(sel, true);
	return ret;
}

static inline int v4l_s_frame_selection(v4l_fd *f, struct v4l2_selection *sel, __u32 field)
{
	int ret;

	if (V4L2_FIELD_HAS_T_OR_B(field))
		v4l_frame_selection(sel, false);
	ret = v4l_s_selection(f, sel);
	if (V4L2_FIELD_HAS_T_OR_B(field))
		v4l_frame_selection(sel, true);
	return ret;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
