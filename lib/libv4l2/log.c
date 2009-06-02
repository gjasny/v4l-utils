/*
#             (C) 2008 Elmar Kleijn <elmar_kleijn@hotmail.com>
#             (C) 2008 Sjoerd Piepenbrink <need4weed@gmail.com>
#             (C) 2008 Hans de Goede <hdegoede@redhat.com>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../libv4lconvert/libv4lsyscall-priv.h"
#include <linux/videodev2.h>
#include "libv4l2.h"
#include "libv4l2-priv.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

FILE *v4l2_log_file = NULL;

static const char *v4l2_ioctls[] = {
	/* start v4l2 ioctls */
	[_IOC_NR(VIDIOC_QUERYCAP)]         = "VIDIOC_QUERYCAP",
	[_IOC_NR(VIDIOC_RESERVED)]         = "VIDIOC_RESERVED",
	[_IOC_NR(VIDIOC_ENUM_FMT)]         = "VIDIOC_ENUM_FMT",
	[_IOC_NR(VIDIOC_G_FMT)]            = "VIDIOC_G_FMT",
	[_IOC_NR(VIDIOC_S_FMT)]            = "VIDIOC_S_FMT",
	[_IOC_NR(VIDIOC_REQBUFS)]          = "VIDIOC_REQBUFS",
	[_IOC_NR(VIDIOC_QUERYBUF)]         = "VIDIOC_QUERYBUF",
	[_IOC_NR(VIDIOC_G_FBUF)]           = "VIDIOC_G_FBUF",
	[_IOC_NR(VIDIOC_S_FBUF)]           = "VIDIOC_S_FBUF",
	[_IOC_NR(VIDIOC_OVERLAY)]          = "VIDIOC_OVERLAY",
	[_IOC_NR(VIDIOC_QBUF)]             = "VIDIOC_QBUF",
	[_IOC_NR(VIDIOC_DQBUF)]            = "VIDIOC_DQBUF",
	[_IOC_NR(VIDIOC_STREAMON)]         = "VIDIOC_STREAMON",
	[_IOC_NR(VIDIOC_STREAMOFF)]        = "VIDIOC_STREAMOFF",
	[_IOC_NR(VIDIOC_G_PARM)]           = "VIDIOC_G_PARM",
	[_IOC_NR(VIDIOC_S_PARM)]           = "VIDIOC_S_PARM",
	[_IOC_NR(VIDIOC_G_STD)]            = "VIDIOC_G_STD",
	[_IOC_NR(VIDIOC_S_STD)]            = "VIDIOC_S_STD",
	[_IOC_NR(VIDIOC_ENUMSTD)]          = "VIDIOC_ENUMSTD",
	[_IOC_NR(VIDIOC_ENUMINPUT)]        = "VIDIOC_ENUMINPUT",
	[_IOC_NR(VIDIOC_G_CTRL)]           = "VIDIOC_G_CTRL",
	[_IOC_NR(VIDIOC_S_CTRL)]           = "VIDIOC_S_CTRL",
	[_IOC_NR(VIDIOC_G_TUNER)]          = "VIDIOC_G_TUNER",
	[_IOC_NR(VIDIOC_S_TUNER)]          = "VIDIOC_S_TUNER",
	[_IOC_NR(VIDIOC_G_AUDIO)]          = "VIDIOC_G_AUDIO",
	[_IOC_NR(VIDIOC_S_AUDIO)]          = "VIDIOC_S_AUDIO",
	[_IOC_NR(VIDIOC_QUERYCTRL)]        = "VIDIOC_QUERYCTRL",
	[_IOC_NR(VIDIOC_QUERYMENU)]        = "VIDIOC_QUERYMENU",
	[_IOC_NR(VIDIOC_G_INPUT)]          = "VIDIOC_G_INPUT",
	[_IOC_NR(VIDIOC_S_INPUT)]          = "VIDIOC_S_INPUT",
	[_IOC_NR(VIDIOC_G_OUTPUT)]         = "VIDIOC_G_OUTPUT",
	[_IOC_NR(VIDIOC_S_OUTPUT)]         = "VIDIOC_S_OUTPUT",
	[_IOC_NR(VIDIOC_ENUMOUTPUT)]       = "VIDIOC_ENUMOUTPUT",
	[_IOC_NR(VIDIOC_G_AUDOUT)]         = "VIDIOC_G_AUDOUT",
	[_IOC_NR(VIDIOC_S_AUDOUT)]         = "VIDIOC_S_AUDOUT",
	[_IOC_NR(VIDIOC_G_MODULATOR)]      = "VIDIOC_G_MODULATOR",
	[_IOC_NR(VIDIOC_S_MODULATOR)]      = "VIDIOC_S_MODULATOR",
	[_IOC_NR(VIDIOC_G_FREQUENCY)]      = "VIDIOC_G_FREQUENCY",
	[_IOC_NR(VIDIOC_S_FREQUENCY)]      = "VIDIOC_S_FREQUENCY",
	[_IOC_NR(VIDIOC_CROPCAP)]          = "VIDIOC_CROPCAP",
	[_IOC_NR(VIDIOC_G_CROP)]           = "VIDIOC_G_CROP",
	[_IOC_NR(VIDIOC_S_CROP)]           = "VIDIOC_S_CROP",
	[_IOC_NR(VIDIOC_G_JPEGCOMP)]       = "VIDIOC_G_JPEGCOMP",
	[_IOC_NR(VIDIOC_S_JPEGCOMP)]       = "VIDIOC_S_JPEGCOMP",
	[_IOC_NR(VIDIOC_QUERYSTD)]         = "VIDIOC_QUERYSTD",
	[_IOC_NR(VIDIOC_TRY_FMT)]          = "VIDIOC_TRY_FMT",
	[_IOC_NR(VIDIOC_ENUMAUDIO)]        = "VIDIOC_ENUMAUDIO",
	[_IOC_NR(VIDIOC_ENUMAUDOUT)]       = "VIDIOC_ENUMAUDOUT",
	[_IOC_NR(VIDIOC_G_PRIORITY)]       = "VIDIOC_G_PRIORITY",
	[_IOC_NR(VIDIOC_S_PRIORITY)]       = "VIDIOC_S_PRIORITY",
	[_IOC_NR(VIDIOC_G_SLICED_VBI_CAP)] = "VIDIOC_G_SLICED_VBI_CAP",
	[_IOC_NR(VIDIOC_LOG_STATUS)]       = "VIDIOC_LOG_STATUS",
	[_IOC_NR(VIDIOC_G_EXT_CTRLS)]      = "VIDIOC_G_EXT_CTRLS",
	[_IOC_NR(VIDIOC_S_EXT_CTRLS)]      = "VIDIOC_S_EXT_CTRLS",
	[_IOC_NR(VIDIOC_TRY_EXT_CTRLS)]    = "VIDIOC_TRY_EXT_CTRLS",
#ifdef VIDIOC_ENUM_FRAMESIZES
	[_IOC_NR(VIDIOC_ENUM_FRAMESIZES)]  = "VIDIOC_ENUM_FRAMESIZES",
	[_IOC_NR(VIDIOC_ENUM_FRAMEINTERVALS)] = "VIDIOC_ENUM_FRAMEINTERVALS",
#endif
};

void v4l2_log_ioctl(unsigned long int request, void *arg, int result)
{
  const char *ioctl_str;
  char buf[40];
  int saved_errno = errno;

  if (!v4l2_log_file)
    return;

  if (_IOC_TYPE(request) == 'V' && _IOC_NR(request) < ARRAY_SIZE(v4l2_ioctls))
    ioctl_str = v4l2_ioctls[_IOC_NR(request)];
  else {
    snprintf(buf, sizeof(buf), "unknown request: %c %d",
      (int)_IOC_TYPE(request), (int)_IOC_NR(request));
    ioctl_str = buf;
  }

  fprintf(v4l2_log_file, "request == %s\n", ioctl_str);

  switch (request) {
    case VIDIOC_ENUM_FMT:
      {
	struct v4l2_fmtdesc *fmt = arg;
	fprintf(v4l2_log_file, "  index: %u, description: %s\n",
	  fmt->index, (result < 0) ? "" : (const char *)fmt->description);
      }
      break;
    case VIDIOC_G_FMT:
    case VIDIOC_S_FMT:
    case VIDIOC_TRY_FMT:
      {
	struct v4l2_format *fmt = arg;
	int pixfmt = fmt->fmt.pix.pixelformat;

	if (fmt->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
	  fprintf(v4l2_log_file, "  pixelformat: %c%c%c%c %ux%u\n",
	    pixfmt & 0xff,
	    (pixfmt >> 8) & 0xff,
	    (pixfmt >> 16) & 0xff,
	    pixfmt >> 24,
	    fmt->fmt.pix.width,
	    fmt->fmt.pix.height);
	  fprintf(v4l2_log_file, "  field: %d bytesperline: %d imagesize%d\n",
	    (int)fmt->fmt.pix.field, (int)fmt->fmt.pix.bytesperline,
	    (int)fmt->fmt.pix.sizeimage);
	  fprintf(v4l2_log_file, "  colorspace: %d, priv: %x\n",
	    (int)fmt->fmt.pix.colorspace, (int)fmt->fmt.pix.priv);
	}
      }
      break;
    case VIDIOC_REQBUFS:
      {
	struct v4l2_requestbuffers *req = arg;

	fprintf(v4l2_log_file, "  count: %u type: %d memory: %d\n",
	  req->count, (int)req->type, (int)req->memory);
      }
      break;
#ifdef VIDIOC_ENUM_FRAMESIZES
    case VIDIOC_ENUM_FRAMESIZES:
      {
	struct v4l2_frmsizeenum *frmsize = arg;
	int pixfmt = frmsize->pixel_format;

	fprintf(v4l2_log_file, "  index: %u pixelformat: %c%c%c%c\n",
	  frmsize->index,
	  pixfmt & 0xff,
	  (pixfmt >> 8) & 0xff,
	  (pixfmt >> 16) & 0xff,
	  pixfmt >> 24);
	switch (frmsize->type) {
	  case V4L2_FRMSIZE_TYPE_DISCRETE:
	    fprintf(v4l2_log_file, "  %ux%u\n", frmsize->discrete.width,
	      frmsize->discrete.height);
	    break;
	  case V4L2_FRMSIZE_TYPE_CONTINUOUS:
	  case V4L2_FRMSIZE_TYPE_STEPWISE:
	    fprintf(v4l2_log_file, "  %ux%u -> %ux%u\n",
	      frmsize->stepwise.min_width, frmsize->stepwise.min_height,
	      frmsize->stepwise.max_width, frmsize->stepwise.max_height);
	    break;
	}
      }
      break;
    case VIDIOC_ENUM_FRAMEINTERVALS:
      {
	struct v4l2_frmivalenum *frmival = arg;
	int pixfmt = frmival->pixel_format;

	fprintf(v4l2_log_file, "  index: %u pixelformat: %c%c%c%c %ux%u:\n",
	  frmival->index,
	  pixfmt & 0xff,
	  (pixfmt >> 8) & 0xff,
	  (pixfmt >> 16) & 0xff,
	  pixfmt >> 24,
	  frmival->width,
	  frmival->height);
	switch (frmival->type) {
	  case V4L2_FRMIVAL_TYPE_DISCRETE:
	    fprintf(v4l2_log_file, "  %u/%u\n", frmival->discrete.numerator,
	      frmival->discrete.denominator);
	    break;
	  case V4L2_FRMIVAL_TYPE_CONTINUOUS:
	  case V4L2_FRMIVAL_TYPE_STEPWISE:
	    fprintf(v4l2_log_file, "  %u/%u -> %u/%u\n",
	      frmival->stepwise.min.numerator,
	      frmival->stepwise.min.denominator,
	      frmival->stepwise.max.numerator,
	      frmival->stepwise.max.denominator);
	    break;
	}
      }
      break;
#endif
  }

  if (result < 0)
    fprintf(v4l2_log_file, "result == %d (%s)\n", result, strerror(saved_errno));
  else
    fprintf(v4l2_log_file, "result == %d\n", result);

  fflush(v4l2_log_file);
}
