/*
 *             (C) 2012 Mauro Carvalho Chehab <mchehab@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#include <config.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <linux/videodev2.h>

#include "libv4l-plugin.h"

#define SYS_IOCTL(fd, cmd, arg) \
	syscall(SYS_ioctl, (int)(fd), (unsigned long)(cmd), (void *)(arg))


#if HAVE_VISIBILITY
#define PLUGIN_PUBLIC __attribute__ ((visibility("default")))
#else
#define PLUGIN_PUBLIC
#endif

struct mplane_plugin {
	union {
		struct {
			unsigned int		mplane_capture : 1;
			unsigned int		mplane_output  : 1;
		};
		unsigned int mplane;
	};
};

#define SIMPLE_CONVERT_IOCTL(fd, cmd, arg, __struc) ({		\
	int __ret;						\
	struct __struc *req = arg;				\
	uint32_t type = req->type;				\
	req->type = convert_type(type);				\
	__ret = SYS_IOCTL(fd, cmd, arg);			\
	req->type = type;					\
	__ret;							\
	})

static void *plugin_init(int fd)
{
	struct v4l2_capability cap;
	int ret;
	struct mplane_plugin plugin, *ret_plugin;

	memset(&plugin, 0, sizeof(plugin));

	/* Check if device needs mplane plugin */
	memset(&cap, 0, sizeof(cap));
	ret = SYS_IOCTL(fd, VIDIOC_QUERYCAP, &cap);
	if (ret) {
		perror("Failed to query video capabilities");
		return NULL;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
	    (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE))
		plugin.mplane_capture = 1;

	if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) &&
	    (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE))
		plugin.mplane_output = 1;

	/* Device doesn't need it. return NULL to disable the plugin */
	if (!plugin.mplane)
		return NULL;

	/* Allocate and initialize private data */
	ret_plugin = calloc(1, sizeof(*ret_plugin));
	if (!ret_plugin) {
		perror("Couldn't allocate memory for plugin");
		return NULL;
	}
	*ret_plugin = plugin;

	printf("Using mplane plugin for %s%s\n",
	       plugin.mplane_capture ? "capture " : "",
	       plugin.mplane_output ? "output " : "");

	return ret_plugin;
}

static void plugin_close(void *dev_ops_priv) {
	if (dev_ops_priv == NULL)
		return;

	free(dev_ops_priv);
}

static int querycap_ioctl(int fd, unsigned long int cmd,
			  struct v4l2_capability *arg)
{
	struct v4l2_capability *cap = arg;
	int ret;
	
	ret = SYS_IOCTL(fd, cmd, cap);
	if (ret)
		return ret;

	/* Report mplane as normal caps */
	if (cap->capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		cap->capabilities |= V4L2_CAP_VIDEO_CAPTURE;

	if (cap->capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
		cap->capabilities |= V4L2_CAP_VIDEO_OUTPUT;

	/*
	 * Don't report mplane caps, as this will be handled via
	 * this plugin
	 */
	cap->capabilities &= ~(V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			       V4L2_CAP_VIDEO_CAPTURE_MPLANE);

	return 0;
}

static int convert_type(int type)
{
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		return V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	default:
		return type;
	}
}

static int try_set_fmt_ioctl(int fd, unsigned long int cmd,
			     struct v4l2_format *arg)
{
	struct v4l2_format fmt = { 0 };
	struct v4l2_format *org = arg;
	int ret;

	switch (arg->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		break;
	default:
		return SYS_IOCTL(fd, cmd, &fmt);
	}

	fmt.fmt.pix_mp.width = org->fmt.pix.width;
	fmt.fmt.pix_mp.height = org->fmt.pix.height;
	fmt.fmt.pix_mp.pixelformat = org->fmt.pix.pixelformat;
	fmt.fmt.pix_mp.field = org->fmt.pix.field;
	fmt.fmt.pix_mp.colorspace = org->fmt.pix.colorspace;
	fmt.fmt.pix_mp.num_planes = 1;
	fmt.fmt.pix_mp.plane_fmt[0].bytesperline = org->fmt.pix.bytesperline;
	fmt.fmt.pix_mp.plane_fmt[0].sizeimage = org->fmt.pix.sizeimage;

	ret = SYS_IOCTL(fd, cmd, &fmt);
	if (ret)
		return ret;

	org->fmt.pix.width = fmt.fmt.pix_mp.width;
	org->fmt.pix.height = fmt.fmt.pix_mp.height;
	org->fmt.pix.pixelformat = fmt.fmt.pix_mp.pixelformat;
	org->fmt.pix.field = fmt.fmt.pix_mp.field;
	org->fmt.pix.colorspace = fmt.fmt.pix_mp.colorspace;
	org->fmt.pix.bytesperline = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
	org->fmt.pix.sizeimage = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

	return 0;
}

static int create_bufs_ioctl(int fd, unsigned long int cmd,
			     struct v4l2_create_buffers *arg)
{
	struct v4l2_format fmt = { 0 };
	struct v4l2_format *org = &arg->format;
	int ret;

	switch (arg->format.type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		break;
	default:
		return SYS_IOCTL(fd, cmd, &fmt);
	}

	fmt.fmt.pix_mp.width = org->fmt.pix.width;
	fmt.fmt.pix_mp.height = org->fmt.pix.height;
	fmt.fmt.pix_mp.pixelformat = org->fmt.pix.pixelformat;
	fmt.fmt.pix_mp.field = org->fmt.pix.field;
	fmt.fmt.pix_mp.colorspace = org->fmt.pix.colorspace;
	fmt.fmt.pix_mp.num_planes = 1;
	fmt.fmt.pix_mp.plane_fmt[0].bytesperline = org->fmt.pix.bytesperline;
	fmt.fmt.pix_mp.plane_fmt[0].sizeimage = org->fmt.pix.sizeimage;

	ret = SYS_IOCTL(fd, cmd, &arg);
	if (ret)
		return ret;

	org->fmt.pix.width = fmt.fmt.pix_mp.width;
	org->fmt.pix.height = fmt.fmt.pix_mp.height;
	org->fmt.pix.pixelformat = fmt.fmt.pix_mp.pixelformat;
	org->fmt.pix.field = fmt.fmt.pix_mp.field;
	org->fmt.pix.colorspace = fmt.fmt.pix_mp.colorspace;
	org->fmt.pix.bytesperline = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
	org->fmt.pix.sizeimage = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

	return 0;
}

static int get_fmt_ioctl(int fd, unsigned long int cmd, struct v4l2_format *arg)
{
	struct v4l2_format fmt = { 0 };
	struct v4l2_format *org = arg;
	int ret;

	switch (arg->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		break;
	default:
		return SYS_IOCTL(fd, cmd, &fmt);
	}

	ret = SYS_IOCTL(fd, cmd, &fmt);
	if (ret)
		return ret;

	org->fmt.pix.width = fmt.fmt.pix_mp.width;
	org->fmt.pix.height = fmt.fmt.pix_mp.height;
	org->fmt.pix.pixelformat = fmt.fmt.pix_mp.pixelformat;
	org->fmt.pix.field = fmt.fmt.pix_mp.field;
	org->fmt.pix.colorspace = fmt.fmt.pix_mp.colorspace;
	org->fmt.pix.bytesperline = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
	org->fmt.pix.sizeimage = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

	/*
	 * If the device doesn't support just one plane, there's
	 * nothing we can do, except return an error condition.
	 */
	if (fmt.fmt.pix_mp.num_planes > 1) {
		errno = -EINVAL;
		return -1;
	}

	return ret;
}

static int buf_ioctl(int fd, unsigned long int cmd, struct v4l2_buffer *arg)
{
	struct v4l2_buffer buf = *arg;
	struct v4l2_plane plane = { 0 };
	int ret;

	buf.type = convert_type(arg->type);

	if (buf.type == arg->type)
		return SYS_IOCTL(fd, cmd, &buf);

	memcpy(&plane.m, &arg->m, sizeof(plane.m));
	plane.length = arg->length;
	plane.bytesused = arg->bytesused;
	
	buf.m.planes = &plane;
	buf.length = 1;

	ret = SYS_IOCTL(fd, cmd, &buf);

	arg->index = buf.index;
	arg->flags = buf.flags;
	arg->field = buf.field;
	arg->timestamp = buf.timestamp;
	arg->timecode = buf.timecode;
	arg->sequence = buf.sequence;

	arg->length = plane.length;
	arg->bytesused = plane.bytesused;

	return ret;
}

static int plugin_ioctl(void *dev_ops_priv, int fd,
			unsigned long int cmd, void *arg)
{
	struct mplane_plugin *plugin = dev_ops_priv;
	switch (cmd) {
	case VIDIOC_QUERYCAP:
		return querycap_ioctl(fd, cmd, arg);
	case VIDIOC_TRY_FMT:
	case VIDIOC_S_FMT:
		return try_set_fmt_ioctl(fd, cmd, arg);
	case VIDIOC_G_FMT:
		return get_fmt_ioctl(fd, cmd, arg);
	case VIDIOC_ENUM_FMT:
		return SIMPLE_CONVERT_IOCTL(fd, cmd, arg, v4l2_fmtdesc);
	case VIDIOC_S_PARM:
	case VIDIOC_G_PARM:
		return SIMPLE_CONVERT_IOCTL(fd, cmd, arg, v4l2_streamparm);
	case VIDIOC_CROPCAP:
		return SIMPLE_CONVERT_IOCTL(fd, cmd, arg, v4l2_cropcap);
	case VIDIOC_S_CROP:
	case VIDIOC_G_CROP:
		return SIMPLE_CONVERT_IOCTL(fd, cmd, arg, v4l2_crop);
	case VIDIOC_QBUF:
	case VIDIOC_DQBUF:
	case VIDIOC_QUERYBUF:
	case VIDIOC_PREPARE_BUF:
		return buf_ioctl(fd, cmd, arg);
	case VIDIOC_CREATE_BUFS:
		return create_bufs_ioctl(fd, cmd, arg);
	case VIDIOC_REQBUFS:
		return SIMPLE_CONVERT_IOCTL(fd, cmd, arg, v4l2_requestbuffers);
	case VIDIOC_STREAMON:
	case VIDIOC_STREAMOFF:
	{
		int type, ret;

		/*
		 * If the device has both capture and output, weird things
		 * could happen. For now, let's not consider this case. If this
		 * is ever happens in practice, the logic should be changed to
		 * track reqbufs, in order to identify what's required.
		 */
		if (plugin->mplane_capture) {
			type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			ret = SYS_IOCTL(fd, cmd, &type);
		} else if (plugin->mplane_output) {
			type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
			ret = SYS_IOCTL(fd, cmd, &type);
		} else {
			ret = -1;
			errno = EINVAL;
		}

		return ret;
	}
	/* CASE VIDIOC_EXPBUF: */
	default:
		return SYS_IOCTL(fd, cmd, arg);
	}
}

PLUGIN_PUBLIC const struct libv4l_dev_ops libv4l2_plugin = {
	.init = &plugin_init,
	.close = &plugin_close,
	.ioctl = &plugin_ioctl,
};
