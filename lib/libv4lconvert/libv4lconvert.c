/*
#             (C) 2008 Hans de Goede <hdegoede@redhat.com>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "libv4lconvert.h"
#include "libv4lconvert-priv.h"
#include "libv4lsyscall-priv.h"

#define MIN(a,b) (((a)<(b))?(a):(b))

/* Note for proper functioning of v4lconvert_enum_fmt the first entries in
  supported_src_pixfmts must match with the entries in supported_dst_pixfmts */
#define SUPPORTED_DST_PIXFMTS \
  { V4L2_PIX_FMT_RGB24,  0 }, \
  { V4L2_PIX_FMT_BGR24,  0 }, \
  { V4L2_PIX_FMT_YUV420, 0 }, \
  { V4L2_PIX_FMT_YVU420, 0 }

static void v4lconvert_get_framesizes(struct v4lconvert_data *data,
  unsigned int pixelformat, int index);

/* Note uncompressed formats must go first so that they are prefered by
   v4lconvert_try_format for low resolutions */
static const struct v4lconvert_pixfmt supported_src_pixfmts[] = {
  SUPPORTED_DST_PIXFMTS,
  { V4L2_PIX_FMT_YUYV,         0 },
  { V4L2_PIX_FMT_YVYU,         0 },
  { V4L2_PIX_FMT_UYVY,         0 },
  { V4L2_PIX_FMT_RGB565,       0 },
  { V4L2_PIX_FMT_SN9C20X_I420, V4LCONVERT_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_SBGGR8,       V4LCONVERT_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_SGBRG8,       V4LCONVERT_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_SGRBG8,       V4LCONVERT_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_SRGGB8,       V4LCONVERT_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_SPCA501,      V4LCONVERT_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_SPCA505,      V4LCONVERT_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_SPCA508,      V4LCONVERT_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_HM12,         V4LCONVERT_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_MJPEG,        V4LCONVERT_COMPRESSED },
  { V4L2_PIX_FMT_JPEG,         V4LCONVERT_COMPRESSED },
  { V4L2_PIX_FMT_SPCA561,      V4LCONVERT_COMPRESSED_AND_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_SN9C10X,      V4LCONVERT_COMPRESSED_AND_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_SN9C2028,     V4LCONVERT_COMPRESSED_AND_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_PAC207,       V4LCONVERT_COMPRESSED_AND_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_MR97310A,     V4LCONVERT_COMPRESSED_AND_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_SQ905C,       V4LCONVERT_COMPRESSED_AND_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_PJPG,         V4LCONVERT_COMPRESSED_AND_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_OV511,        V4LCONVERT_COMPRESSED_AND_NEEDS_CONVERSION },
  { V4L2_PIX_FMT_OV518,        V4LCONVERT_COMPRESSED_AND_NEEDS_CONVERSION },
};

static const struct v4lconvert_pixfmt supported_dst_pixfmts[] = {
  SUPPORTED_DST_PIXFMTS
};

/* List of well known resolutions which we can get by cropping somewhat larger
   resolutions */
static const int v4lconvert_crop_res[][2] = {
  /* low res VGA resolutions, can be made by software cropping SIF resolutions
     for cam/drivers which do not support this in hardware */
  { 320, 240 },
  { 160, 120 },
  /* Some CIF cams (with vv6410 sensor) have slightly larger then usual CIF
     resolutions, make regular CIF resolutions available on these by sw crop */
  { 352, 288 },
  { 176, 144 },
};

struct v4lconvert_data *v4lconvert_create(int fd)
{
  int i, j;
  struct v4lconvert_data *data = calloc(1, sizeof(struct v4lconvert_data));
  struct v4l2_capability cap;
  /* This keeps tracks of devices which have only formats for which apps
     most likely will need conversion and we can thus safely add software
     processing controls without a performance impact. */
  int always_needs_conversion = 1;

  if (!data) {
    fprintf(stderr, "libv4lconvert: error: out of memory!\n");
    return NULL;
  }

  data->fd = fd;
  data->decompress_pid = -1;

  /* Check supported formats */
  for (i = 0; ; i++) {
    struct v4l2_fmtdesc fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };

    fmt.index = i;

    if (SYS_IOCTL(data->fd, VIDIOC_ENUM_FMT, &fmt))
      break;

    for (j = 0; j < ARRAY_SIZE(supported_src_pixfmts); j++)
      if (fmt.pixelformat == supported_src_pixfmts[j].fmt) {
	data->supported_src_formats |= 1 << j;
	v4lconvert_get_framesizes(data, fmt.pixelformat, j);
	if (!(supported_src_pixfmts[j].flags & V4LCONVERT_NEEDS_CONVERSION))
	  always_needs_conversion = 0;
	break;
      }

    if (j == ARRAY_SIZE(supported_src_pixfmts))
      always_needs_conversion = 0;
  }

  data->no_formats = i;

  /* Check if this cam has any special flags */
  if (SYS_IOCTL(data->fd, VIDIOC_QUERYCAP, &cap) == 0) {
    if (!strcmp((char *)cap.driver, "uvcvideo"))
      data->flags |= V4LCONVERT_IS_UVC;
    else if (!strcmp((char *)cap.driver, "sn9c20x"))
      data->flags |= V4LCONVERT_IS_SN9C20X;

    if ((cap.capabilities & 0xff) & ~V4L2_CAP_VIDEO_CAPTURE)
      always_needs_conversion = 0;
  }

  data->control = v4lcontrol_create(fd, always_needs_conversion);
  if (!data->control) {
    free(data);
    return NULL;
  }
  data->control_flags = v4lcontrol_get_flags(data->control);

  data->processing = v4lprocessing_create(fd, data->control);
  if (!data->processing) {
    v4lcontrol_destroy(data->control);
    free(data);
    return NULL;
  }

  return data;
}

void v4lconvert_destroy(struct v4lconvert_data *data)
{
  v4lprocessing_destroy(data->processing);
  v4lcontrol_destroy(data->control);
  if (data->jdec) {
    unsigned char *comps[3] = { NULL, NULL, NULL };
    tinyjpeg_set_components(data->jdec, comps, 3);
    tinyjpeg_free(data->jdec);
  }
  v4lconvert_helper_cleanup(data);
  free(data->convert1_buf);
  free(data->convert2_buf);
  free(data->rotate90_buf);
  free(data->flip_buf);
  free(data->convert_pixfmt_buf);
  free(data);
}

int v4lconvert_supported_dst_format(unsigned int pixelformat)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(supported_dst_pixfmts); i++)
    if (supported_dst_pixfmts[i].fmt == pixelformat)
      break;

  return i != ARRAY_SIZE(supported_dst_pixfmts);
}

int v4lconvert_supported_dst_fmt_only(struct v4lconvert_data *data)
{
  return v4lcontrol_needs_conversion(data->control) &&
	 data->supported_src_formats;
}

/* See libv4lconvert.h for description of in / out parameters */
int v4lconvert_enum_fmt(struct v4lconvert_data *data, struct v4l2_fmtdesc *fmt)
{
  int i, no_faked_fmts = 0;
  unsigned int faked_fmts[ARRAY_SIZE(supported_dst_pixfmts)];

  if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE ||
      (!v4lconvert_supported_dst_fmt_only(data) &&
       fmt->index < data->no_formats))
    return SYS_IOCTL(data->fd, VIDIOC_ENUM_FMT, fmt);

  for (i = 0; i < ARRAY_SIZE(supported_dst_pixfmts); i++)
    if (v4lconvert_supported_dst_fmt_only(data) ||
	!(data->supported_src_formats & (1 << i))) {
      faked_fmts[no_faked_fmts] = supported_dst_pixfmts[i].fmt;
      no_faked_fmts++;
    }

  if (!v4lconvert_supported_dst_fmt_only(data))
    i = fmt->index - data->no_formats;
  else
    i = fmt->index;

  if (i >= no_faked_fmts) {
    errno = EINVAL;
    return -1;
  }

  fmt->flags = V4L2_FMT_FLAG_EMULATED;
  fmt->pixelformat = faked_fmts[i];
  fmt->description[0] = faked_fmts[i] & 0xff;
  fmt->description[1] = (faked_fmts[i] >> 8) & 0xff;
  fmt->description[2] = (faked_fmts[i] >> 16) & 0xff;
  fmt->description[3] = faked_fmts[i] >> 24;
  fmt->description[4] = '\0';
  memset(fmt->reserved, 0, 4);

  return 0;
}

/* Find out what format to use based on the (cached) results of enum
   framesizes instead of doing a zillion try_fmt calls. This function
   currently is intended for use with UVC cams only. This is esp.
   important for UVC based cams as doing try_fmt there actually causes I/O */
static int v4lconvert_do_try_format_uvc(struct v4lconvert_data *data,
  struct v4l2_format *dest_fmt, struct v4l2_format *src_fmt)
{
  int i;
  unsigned int closest_fmt_size_diff = -1;
  int best_framesize = 0;/* Just use the first format if no small enough one */
  int best_format = 0;

  for (i = 0; i < data->no_framesizes; i++) {
    if (data->framesizes[i].discrete.width <= dest_fmt->fmt.pix.width &&
	data->framesizes[i].discrete.height <= dest_fmt->fmt.pix.height) {
      int size_x_diff = dest_fmt->fmt.pix.width -
			data->framesizes[i].discrete.width;
      int size_y_diff = dest_fmt->fmt.pix.height -
			data->framesizes[i].discrete.height;
      unsigned int size_diff = size_x_diff * size_x_diff +
			       size_y_diff * size_y_diff;

      if (size_diff < closest_fmt_size_diff) {
	closest_fmt_size_diff = size_diff;
	best_framesize = i;
      }
    }
  }

  for (i = 0; i < ARRAY_SIZE(supported_src_pixfmts); i++) {
    /* is this format supported? */
    if (!(data->framesizes[best_framesize].pixel_format & (1 << i)))
      continue;

    if (!best_format ||
	supported_src_pixfmts[i].fmt == dest_fmt->fmt.pix.pixelformat ||
	((data->framesizes[best_framesize].discrete.width > 180 ||
	  data->framesizes[best_framesize].discrete.height > 148) &&
	 (supported_src_pixfmts[i].flags & V4LCONVERT_COMPRESSED)))
      best_format = supported_src_pixfmts[i].fmt;
  }

  dest_fmt->fmt.pix.width = data->framesizes[best_framesize].discrete.width;
  dest_fmt->fmt.pix.height = data->framesizes[best_framesize].discrete.height;
  dest_fmt->fmt.pix.field = V4L2_FIELD_NONE; /* UVC has no fields */
  /* Not pretty, the pwc driver doesn't fill these in try_fmt either though,
     so we should be able to get away with this. */
  dest_fmt->fmt.pix.bytesperline = 0;
  dest_fmt->fmt.pix.sizeimage = 0;
  dest_fmt->fmt.pix.colorspace = 0;
  dest_fmt->fmt.pix.priv = 0;

  *src_fmt = *dest_fmt;
  src_fmt->fmt.pix.pixelformat = best_format;

  return 0;
}

static int v4lconvert_do_try_format(struct v4lconvert_data *data,
  struct v4l2_format *dest_fmt, struct v4l2_format *src_fmt)
{
  int i;
  unsigned int closest_fmt_size_diff = -1;
  unsigned int desired_pixfmt = dest_fmt->fmt.pix.pixelformat;
  struct v4l2_format try_fmt, closest_fmt = { .type = 0 };

  if (data->flags & V4LCONVERT_IS_UVC)
    return v4lconvert_do_try_format_uvc(data, dest_fmt, src_fmt);

  for (i = 0; i < ARRAY_SIZE(supported_src_pixfmts); i++) {
    /* is this format supported? */
    if (!(data->supported_src_formats & (1 << i)))
      continue;

    try_fmt = *dest_fmt;
    try_fmt.fmt.pix.pixelformat = supported_src_pixfmts[i].fmt;

    if (!SYS_IOCTL(data->fd, VIDIOC_TRY_FMT, &try_fmt))
    {
      if (try_fmt.fmt.pix.pixelformat == supported_src_pixfmts[i].fmt) {
	int size_x_diff = abs((int)try_fmt.fmt.pix.width -
			      (int)dest_fmt->fmt.pix.width);
	int size_y_diff = abs((int)try_fmt.fmt.pix.height -
			      (int)dest_fmt->fmt.pix.height);
	unsigned int size_diff = size_x_diff * size_x_diff +
				 size_y_diff * size_y_diff;
	if (size_diff < closest_fmt_size_diff ||
	    (size_diff == closest_fmt_size_diff &&
	     (supported_src_pixfmts[i].fmt == desired_pixfmt ||
	      ((try_fmt.fmt.pix.width > 180 || try_fmt.fmt.pix.height > 148) &&
	       (supported_src_pixfmts[i].flags & V4LCONVERT_COMPRESSED))))) {
	  closest_fmt_size_diff = size_diff;
	  closest_fmt = try_fmt;
	}
      }
    }
  }

  if (closest_fmt.type == 0)
    return -1;

  *dest_fmt = closest_fmt;
  if (closest_fmt.fmt.pix.pixelformat != desired_pixfmt)
    dest_fmt->fmt.pix.pixelformat = desired_pixfmt;
  *src_fmt = closest_fmt;

  return 0;
}

void v4lconvert_fixup_fmt(struct v4l2_format *fmt)
{
  switch (fmt->fmt.pix.pixelformat) {
  case V4L2_PIX_FMT_RGB24:
  case V4L2_PIX_FMT_BGR24:
    fmt->fmt.pix.bytesperline = fmt->fmt.pix.width * 3;
    fmt->fmt.pix.sizeimage = fmt->fmt.pix.width * fmt->fmt.pix.height * 3;
    break;
  case V4L2_PIX_FMT_YUV420:
  case V4L2_PIX_FMT_YVU420:
    fmt->fmt.pix.bytesperline = fmt->fmt.pix.width;
    fmt->fmt.pix.sizeimage = fmt->fmt.pix.width * fmt->fmt.pix.height * 3 / 2;
    break;
  }
}

/* See libv4lconvert.h for description of in / out parameters */
int v4lconvert_try_format(struct v4lconvert_data *data,
  struct v4l2_format *dest_fmt, struct v4l2_format *src_fmt)
{
  int i, result;
  unsigned int desired_width = dest_fmt->fmt.pix.width;
  unsigned int desired_height = dest_fmt->fmt.pix.height;
  struct v4l2_format try_src, try_dest, try2_src, try2_dest;

  if (dest_fmt->type == V4L2_BUF_TYPE_VIDEO_CAPTURE &&
      v4lconvert_supported_dst_fmt_only(data) &&
      !v4lconvert_supported_dst_format(dest_fmt->fmt.pix.pixelformat))
    dest_fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;

  try_dest = *dest_fmt;

  /* Can we do conversion to the requested format & type? */
  if (!v4lconvert_supported_dst_format(dest_fmt->fmt.pix.pixelformat) ||
      dest_fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE ||
      v4lconvert_do_try_format(data, &try_dest, &try_src)) {
    result = SYS_IOCTL(data->fd, VIDIOC_TRY_FMT, dest_fmt);
    if (src_fmt)
      *src_fmt = *dest_fmt;
    return result;
  }

  /* In case of a non exact resolution match, try again with a slightly larger
     resolution as some weird devices are not able to crop of the number of
     extra (border) pixels most sensors have compared to standard resolutions,
     which we will then just crop of in software */
  if (try_dest.fmt.pix.width != desired_width ||
      try_dest.fmt.pix.height != desired_height) {
    try2_dest = *dest_fmt;
    try2_dest.fmt.pix.width  = desired_width + 7;
    try2_dest.fmt.pix.height = desired_height + 1;
    result = v4lconvert_do_try_format(data, &try2_dest, &try2_src);
    if (result == 0 &&
	try2_dest.fmt.pix.width >= desired_width &&
	try2_dest.fmt.pix.width <= desired_width + 7 &&
	try2_dest.fmt.pix.height >= desired_height &&
	try2_dest.fmt.pix.height <= desired_height + 1) {
      /* Success! */
      try2_dest.fmt.pix.width = desired_width;
      try2_dest.fmt.pix.height = desired_height;
      try_dest = try2_dest;
      try_src = try2_src;
    }
  }

  /* In case of a non exact resolution match, see if this is a well known
     resolution some apps are hardcoded too and try to give the app what it
     asked for by cropping a slightly larger resolution or adding a small
     black border to a slightly smaller resolution */
  if (try_dest.fmt.pix.width != desired_width ||
      try_dest.fmt.pix.height != desired_height) {
    for (i = 0; i < ARRAY_SIZE(v4lconvert_crop_res); i++) {
      if (v4lconvert_crop_res[i][0] == desired_width &&
	  v4lconvert_crop_res[i][1] == desired_height) {
	try2_dest = *dest_fmt;

	/* Note these are chosen so that cropping to vga res just works for
	   vv6410 sensor cams, which have 356x292 and 180x148 */
	try2_dest.fmt.pix.width = desired_width * 113 / 100;
	try2_dest.fmt.pix.height = desired_height * 124 / 100;
	result = v4lconvert_do_try_format(data, &try2_dest, &try2_src);
	if (result == 0 &&
	    (/* Add a small black border of max 16 pixels */
	     (try2_dest.fmt.pix.width >= desired_width - 16 &&
	      try2_dest.fmt.pix.width <= desired_width &&
	      try2_dest.fmt.pix.height >= desired_height - 16 &&
	      try2_dest.fmt.pix.height <= desired_height) ||
	     /* Standard cropping to max 80% of actual width / height */
	     (try2_dest.fmt.pix.width >= desired_width &&
	      try2_dest.fmt.pix.width <= desired_width * 5 / 4 &&
	      try2_dest.fmt.pix.height >= desired_height &&
	      try2_dest.fmt.pix.height <= desired_height * 5 / 4) ||
	     /* Downscale 2x + cropping to max 80% of actual width / height */
	     (try2_dest.fmt.pix.width >= desired_width * 2 &&
	      try2_dest.fmt.pix.width <= desired_width * 5 / 2 &&
	      try2_dest.fmt.pix.height >= desired_height * 2 &&
	      try2_dest.fmt.pix.height <= desired_height * 5 / 2))) {
	  /* Success! */
	  try2_dest.fmt.pix.width = desired_width;
	  try2_dest.fmt.pix.height = desired_height;
	  try_dest = try2_dest;
	  try_src = try2_src;
	}
	break;
      }
    }
  }

  /* Some applications / libs (*cough* gstreamer *cough*) will not work
     correctly with planar YUV formats when the width is not a multiple of 8
     or the height is not a multiple of 2. With RGB formats these apps require
     the width to be a multiple of 4. We apply the same rounding to all
     formats to not end up with 2 close but different resolutions. */
  try_dest.fmt.pix.width &= ~7;
  try_dest.fmt.pix.height &= ~1;

  /* Are we converting / cropping ? */
  if(try_src.fmt.pix.width != try_dest.fmt.pix.width ||
     try_src.fmt.pix.height != try_dest.fmt.pix.height ||
     try_src.fmt.pix.pixelformat != try_dest.fmt.pix.pixelformat)
    v4lconvert_fixup_fmt(&try_dest);

  *dest_fmt = try_dest;
  if (src_fmt)
    *src_fmt = try_src;

  return 0;
}

/* Is conversion necessary ? */
int v4lconvert_needs_conversion(struct v4lconvert_data *data,
  const struct v4l2_format *src_fmt,  /* in */
  const struct v4l2_format *dest_fmt) /* in */
{
  if (src_fmt->fmt.pix.width != dest_fmt->fmt.pix.width ||
      src_fmt->fmt.pix.height != dest_fmt->fmt.pix.height ||
      src_fmt->fmt.pix.pixelformat != dest_fmt->fmt.pix.pixelformat ||
      (v4lcontrol_needs_conversion(data->control) &&
       v4lconvert_supported_dst_format(dest_fmt->fmt.pix.pixelformat)))
    return 1;

  return 0;
}

static int v4lconvert_processing_needs_double_conversion(
  unsigned int src_pix_fmt, unsigned int dest_pix_fmt)
{
  switch (src_pix_fmt) {
    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_BGR24:
    case V4L2_PIX_FMT_SPCA561:
    case V4L2_PIX_FMT_SN9C10X:
    case V4L2_PIX_FMT_PAC207:
    case V4L2_PIX_FMT_MR97310A:
    case V4L2_PIX_FMT_SN9C2028:
    case V4L2_PIX_FMT_SQ905C:
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_SGBRG8:
    case V4L2_PIX_FMT_SGRBG8:
    case V4L2_PIX_FMT_SRGGB8:
      return 0;
  }
  switch (dest_pix_fmt) {
    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_BGR24:
      return 0;
  }

  return 1;
}

static unsigned char *v4lconvert_alloc_buffer(struct v4lconvert_data *data,
  int needed, unsigned char **buf, int *buf_size)
{
  if (*buf_size < needed) {
    free(*buf);
    *buf = malloc(needed);
    if (*buf == NULL) {
      *buf_size = 0;
      V4LCONVERT_ERR("could not allocate memory\n");
      errno = ENOMEM;
      return NULL;
    }
    *buf_size = needed;
  }
  return *buf;
}

static int v4lconvert_convert_pixfmt(struct v4lconvert_data *data,
  unsigned char *src, int src_size, unsigned char *dest, int dest_size,
  struct v4l2_format *fmt, unsigned int dest_pix_fmt)
{
  unsigned int header_width, header_height;
  int result = 0, jpeg_flags = TINYJPEG_FLAGS_MJPEG_TABLE;
  unsigned char *components[3];
  unsigned int src_pix_fmt = fmt->fmt.pix.pixelformat;
  unsigned int width  = fmt->fmt.pix.width;
  unsigned int height = fmt->fmt.pix.height;

  switch (src_pix_fmt) {
    case V4L2_PIX_FMT_PJPG:
      jpeg_flags |= TINYJPEG_FLAGS_PIXART_JPEG;
      /* Fall through */
    case V4L2_PIX_FMT_MJPEG:
    case V4L2_PIX_FMT_JPEG:
      if (!data->jdec) {
	data->jdec = tinyjpeg_init();
	if (!data->jdec) {
	  V4LCONVERT_ERR("out of memory!\n");
	  errno = ENOMEM;
	  return -1;
	}
      }
      tinyjpeg_set_flags(data->jdec, jpeg_flags);
      if (tinyjpeg_parse_header(data->jdec, src, src_size)) {
	V4LCONVERT_ERR("parsing JPEG header: %s\n",
	  tinyjpeg_get_errorstring(data->jdec));
	errno = EIO;
	return -1;
      }
      tinyjpeg_get_size(data->jdec, &header_width, &header_height);

      if (header_width != width || header_height != height) {
	/* Check for (pixart) rotated JPEG */
	if (header_width == height && header_height == width) {
	  if (!(data->control_flags & V4LCONTROL_ROTATED_90_JPEG)) {
	    V4LCONVERT_ERR("JPEG needs 90° rotation, please report "
			   "this to <hdegoede@redhat.com>\n");
	    errno = EIO;
	    return -1;
	  }
	  fmt->fmt.pix.width = header_width;
	  fmt->fmt.pix.height = header_height;
	} else {
	  V4LCONVERT_ERR("unexpected width / height in JPEG header"
			 "expected: %ux%u, header: %ux%u\n", width, height,
			 header_width, header_height);
	  errno = EIO;
	  return -1;
	}
      } else if ((data->control_flags & V4LCONTROL_ROTATED_90_JPEG)) {
	fprintf(stderr, "libv4lconvert: expected 90° rotated JPEG, but got "
		"normal JPEG, please report this to <hdegoede@redhat.com>\n");
	V4LCONVERT_ERR("expected 90° rotated JPEG, but got normal JPEG\n");
	errno = EAGAIN;
	data->control_flags &= ~V4LCONTROL_ROTATED_90_JPEG;
	return -1;
      }

      components[0] = dest;

      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_RGB24:
	tinyjpeg_set_components(data->jdec, components, 1);
	result = tinyjpeg_decode(data->jdec, TINYJPEG_FMT_RGB24);
	break;
      case V4L2_PIX_FMT_BGR24:
	tinyjpeg_set_components(data->jdec, components, 1);
	result = tinyjpeg_decode(data->jdec, TINYJPEG_FMT_BGR24);
	break;
      case V4L2_PIX_FMT_YUV420:
	components[1] = components[0] + width * height;
	components[2] = components[1] + width * height / 4;
	tinyjpeg_set_components(data->jdec, components, 3);
	result = tinyjpeg_decode(data->jdec, TINYJPEG_FMT_YUV420P);
	break;
      case V4L2_PIX_FMT_YVU420:
	components[2] = components[0] + width * height;
	components[1] = components[2] + width * height / 4;
	tinyjpeg_set_components(data->jdec, components, 3);
	result = tinyjpeg_decode(data->jdec, TINYJPEG_FMT_YUV420P);
	break;
      }

      if (result) {
	/* Pixart webcam's seem to regulary generate corrupt frames, which
	   are best thrown away to avoid flashes in the video stream. Tell
	   the upper layer this is an intermediate fault and it should try
	   again with a new buffer by setting errno to EAGAIN */
	if (src_pix_fmt == V4L2_PIX_FMT_PJPG ||
	    data->flags & V4LCONVERT_IS_SN9C20X) {
	  V4LCONVERT_ERR("decompressing JPEG: %s",
	    tinyjpeg_get_errorstring(data->jdec));
	  errno = EAGAIN;
	  return -1;
	} else {
	/* If the JPEG header checked out ok and we get an error during actual
	   decompression, log the error, but don't return an errorcode to the
	   application, so that the user gets what we managed to decompress */
	  fprintf(stderr, "libv4lconvert: Error decompressing JPEG: %s",
	    tinyjpeg_get_errorstring(data->jdec));
	}
      }
      break;

    /* Custom cam specific YUV formats */
    case V4L2_PIX_FMT_SPCA501:
    case V4L2_PIX_FMT_SPCA505:
    case V4L2_PIX_FMT_SPCA508:
    case V4L2_PIX_FMT_SN9C20X_I420:
    case V4L2_PIX_FMT_OV511:
    case V4L2_PIX_FMT_OV518:
    {
      unsigned char *d;
      int d_size;
      int yvu = 0;

      if (dest_pix_fmt != V4L2_PIX_FMT_YUV420 &&
	  dest_pix_fmt != V4L2_PIX_FMT_YVU420) {
	d = v4lconvert_alloc_buffer(data, width * height * 3 / 2,
	      &data->convert_pixfmt_buf, &data->convert_pixfmt_buf_size);
	if (!d)
	  return -1;
	d_size = width * height * 3 / 2;
      } else {
	d = dest;
	d_size = dest_size;
      }

      if (dest_pix_fmt == V4L2_PIX_FMT_YVU420)
	yvu = 1;

      switch (src_pix_fmt) {
	case V4L2_PIX_FMT_SPCA501:
	  v4lconvert_spca501_to_yuv420(src, d, width, height, yvu);
	  break;
	case V4L2_PIX_FMT_SPCA505:
	  v4lconvert_spca505_to_yuv420(src, d, width, height, yvu);
	  break;
	case V4L2_PIX_FMT_SPCA508:
	  v4lconvert_spca508_to_yuv420(src, d, width, height, yvu);
	  break;
	case V4L2_PIX_FMT_SN9C20X_I420:
	  v4lconvert_sn9c20x_to_yuv420(src, d, width, height, yvu);
	  break;
	case V4L2_PIX_FMT_OV511:
	  if (v4lconvert_helper_decompress(data, LIBDIR "/" LIBSUBDIR "/ov511-decomp",
		     src, src_size, d, d_size, width, height, yvu)) {
	    /* Corrupt frame, better get another one */
	    errno = EAGAIN;
	    return -1;
	  }
	  break;
	case V4L2_PIX_FMT_OV518:
	  if (v4lconvert_helper_decompress(data, LIBDIR "/" LIBSUBDIR "/ov518-decomp",
		     src, src_size, d, d_size, width, height, yvu)) {
	    /* Corrupt frame, better get another one */
	    errno = EAGAIN;
	    return -1;
	  }
	  break;
      }

      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_RGB24:
	v4lconvert_yuv420_to_rgb24(data->convert_pixfmt_buf, dest, width,
				   height, yvu);
	break;
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_yuv420_to_bgr24(data->convert_pixfmt_buf, dest, width,
				   height, yvu);
	break;
      }
      break;
    }

    /* Conexant cx2341x raw video macroblock format */
    case V4L2_PIX_FMT_HM12:
      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_RGB24:
	v4lconvert_hm12_to_rgb24(src, dest, width, height);
	break;
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_hm12_to_bgr24(src, dest, width, height);
	break;
      case V4L2_PIX_FMT_YUV420:
	v4lconvert_hm12_to_yuv420(src, dest, width, height, 0);
	break;
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_hm12_to_yuv420(src, dest, width, height, 1);
	break;
      }
      break;

    /* compressed bayer formats */
    case V4L2_PIX_FMT_SPCA561:
    case V4L2_PIX_FMT_SN9C10X:
    case V4L2_PIX_FMT_PAC207:
    case V4L2_PIX_FMT_MR97310A:
    case V4L2_PIX_FMT_SN9C2028:
    case V4L2_PIX_FMT_SQ905C:
    {
      unsigned char *tmpbuf;
      struct v4l2_format tmpfmt = *fmt;

      tmpbuf = v4lconvert_alloc_buffer(data, width * height,
	    &data->convert_pixfmt_buf, &data->convert_pixfmt_buf_size);
      if (!tmpbuf)
	return -1;

      switch (src_pix_fmt) {
	case V4L2_PIX_FMT_SPCA561:
	  v4lconvert_decode_spca561(src, tmpbuf, width, height);
	  tmpfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SGBRG8;
	  break;
	case V4L2_PIX_FMT_SN9C10X:
	  v4lconvert_decode_sn9c10x(src, tmpbuf, width, height);
	  tmpfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR8;
	  break;
	case V4L2_PIX_FMT_PAC207:
	  if (v4lconvert_decode_pac207(data, src, src_size, tmpbuf,
				       width, height)) {
	    /* Corrupt frame, better get another one */
	    errno = EAGAIN;
	    return -1;
	  }
	  tmpfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR8;
	  break;
	case V4L2_PIX_FMT_MR97310A:
	  v4lconvert_decode_mr97310a(src, tmpbuf, width, height);
	  tmpfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR8;
	  break;
	case V4L2_PIX_FMT_SN9C2028:
	  v4lconvert_decode_sn9c2028(src, tmpbuf, width, height);
	  tmpfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR8;
	  break;
	case V4L2_PIX_FMT_SQ905C:
	  v4lconvert_decode_sq905c(src, tmpbuf, width, height);
	  tmpfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB8;
	  break;
      }
      /* Do processing on the tmp buffer, because doing it on bayer data is
	 cheaper, and bayer == rgb and our dest_fmt may be yuv */
      tmpfmt.fmt.pix.bytesperline = width;
      tmpfmt.fmt.pix.sizeimage = width * height;
      v4lprocessing_processing(data->processing, tmpbuf, &tmpfmt);
      /* Deliberate fall through to raw bayer fmt code! */
      src_pix_fmt = tmpfmt.fmt.pix.pixelformat;
      src = tmpbuf;
    }

    /* Raw bayer formats */
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_SGBRG8:
    case V4L2_PIX_FMT_SGRBG8:
    case V4L2_PIX_FMT_SRGGB8:
      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_RGB24:
	v4lconvert_bayer_to_rgb24(src, dest, width, height, src_pix_fmt);
	break;
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_bayer_to_bgr24(src, dest, width, height, src_pix_fmt);
	break;
      case V4L2_PIX_FMT_YUV420:
	v4lconvert_bayer_to_yuv420(src, dest, width, height, src_pix_fmt, 0);
	break;
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_bayer_to_yuv420(src, dest, width, height, src_pix_fmt, 1);
	break;
      }
      break;

    case V4L2_PIX_FMT_RGB565:
      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_RGB24:
	v4lconvert_rgb565_to_rgb24(src, dest, width, height);
	break;
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_rgb565_to_bgr24(src, dest, width, height);
	break;
      case V4L2_PIX_FMT_YUV420:
	v4lconvert_rgb565_to_yuv420(src, dest, fmt, 0);
	break;
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_rgb565_to_yuv420(src, dest, fmt, 1);
	break;
      }
      break;

    case V4L2_PIX_FMT_RGB24:
      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_swap_rgb(src, dest, width, height);
	break;
      case V4L2_PIX_FMT_YUV420:
	v4lconvert_rgb24_to_yuv420(src, dest, fmt, 0, 0);
	break;
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_rgb24_to_yuv420(src, dest, fmt, 0, 1);
	break;
      }
      break;

    case V4L2_PIX_FMT_BGR24:
      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_RGB24:
	v4lconvert_swap_rgb(src, dest, width, height);
	break;
      case V4L2_PIX_FMT_YUV420:
	v4lconvert_rgb24_to_yuv420(src, dest, fmt, 1, 0);
	break;
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_rgb24_to_yuv420(src, dest, fmt, 1, 1);
	break;
      }
      break;

    case V4L2_PIX_FMT_YUV420:
      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_RGB24:
	v4lconvert_yuv420_to_rgb24(src, dest, width,
				   height, 0);
	break;
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_yuv420_to_bgr24(src, dest, width,
				   height, 0);
	break;
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_swap_uv(src, dest, fmt);
	break;
      }
      break;

    case V4L2_PIX_FMT_YVU420:
      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_RGB24:
	v4lconvert_yuv420_to_rgb24(src, dest, width,
				   height, 1);
	break;
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_yuv420_to_bgr24(src, dest, width,
				   height, 1);
	break;
      case V4L2_PIX_FMT_YUV420:
	v4lconvert_swap_uv(src, dest, fmt);
	break;
      }
      break;

    case V4L2_PIX_FMT_YUYV:
      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_RGB24:
	v4lconvert_yuyv_to_rgb24(src, dest, width, height);
	break;
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_yuyv_to_bgr24(src, dest, width, height);
	break;
      case V4L2_PIX_FMT_YUV420:
	v4lconvert_yuyv_to_yuv420(src, dest, width, height, 0);
	break;
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_yuyv_to_yuv420(src, dest, width, height, 1);
	break;
      }
      break;

    case V4L2_PIX_FMT_YVYU:
      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_RGB24:
	v4lconvert_yvyu_to_rgb24(src, dest, width, height);
	break;
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_yvyu_to_bgr24(src, dest, width, height);
	break;
      case V4L2_PIX_FMT_YUV420:
	/* Note we use yuyv_to_yuv420 not v4lconvert_yvyu_to_yuv420,
	   with the last argument reversed to make it have as we want */
	v4lconvert_yuyv_to_yuv420(src, dest, width, height, 1);
	break;
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_yuyv_to_yuv420(src, dest, width, height, 0);
	break;
      }
      break;

    case V4L2_PIX_FMT_UYVY:
      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_RGB24:
	v4lconvert_uyvy_to_rgb24(src, dest, width, height);
	break;
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_uyvy_to_bgr24(src, dest, width, height);
	break;
      case V4L2_PIX_FMT_YUV420:
	v4lconvert_uyvy_to_yuv420(src, dest, width, height, 0);
	break;
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_uyvy_to_yuv420(src, dest, width, height, 1);
	break;
      }
      break;

    default:
      V4LCONVERT_ERR("Unknown src format in conversion\n");
      errno = EINVAL;
      return -1;
  }

  fmt->fmt.pix.pixelformat = dest_pix_fmt;
  v4lconvert_fixup_fmt(fmt);

  return 0;
}

int v4lconvert_convert(struct v4lconvert_data *data,
  const struct v4l2_format *src_fmt,  /* in */
  const struct v4l2_format *dest_fmt, /* in */
  unsigned char *src, int src_size, unsigned char *dest, int dest_size)
{
  int res, dest_needed, temp_needed, processing, convert = 0;
  int rotate90, vflip, hflip, crop;
  unsigned char *convert1_dest = dest;
  int convert1_dest_size = dest_size;
  unsigned char *convert2_src = src, *convert2_dest = dest;
  int convert2_dest_size = dest_size;
  unsigned char *rotate90_src = src, *rotate90_dest = dest;
  unsigned char *flip_src = src, *flip_dest = dest;
  unsigned char *crop_src = src;
  struct v4l2_format my_src_fmt = *src_fmt;
  struct v4l2_format my_dest_fmt = *dest_fmt;

  processing = v4lprocessing_pre_processing(data->processing);
  rotate90 = data->control_flags & V4LCONTROL_ROTATED_90_JPEG;
  hflip = v4lcontrol_get_ctrl(data->control, V4LCONTROL_HFLIP);
  vflip = v4lcontrol_get_ctrl(data->control, V4LCONTROL_VFLIP);
  crop = my_dest_fmt.fmt.pix.width != my_src_fmt.fmt.pix.width ||
	 my_dest_fmt.fmt.pix.height != my_src_fmt.fmt.pix.height;

  if (/* If no conversion/processing is needed */
      (src_fmt->fmt.pix.pixelformat == dest_fmt->fmt.pix.pixelformat &&
       !processing && !rotate90 && !hflip && !vflip && !crop) ||
      /* or if we should do processing/rotating/flipping but the app tries to
	 use the native cam format, we just return an unprocessed frame copy */
      !v4lconvert_supported_dst_format(dest_fmt->fmt.pix.pixelformat)) {
    int to_copy = MIN(dest_size, src_size);
    memcpy(dest, src, to_copy);
    return to_copy;
  }

  /* When field is V4L2_FIELD_ALTERNATE, each buffer only contains half the
     lines */
  if (my_src_fmt.fmt.pix.field == V4L2_FIELD_ALTERNATE) {
    my_src_fmt.fmt.pix.height /= 2;
    my_dest_fmt.fmt.pix.height /= 2;
  }

  /* sanity check, is the dest buffer large enough? */
  switch (my_dest_fmt.fmt.pix.pixelformat) {
    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_BGR24:
      dest_needed = my_dest_fmt.fmt.pix.width * my_dest_fmt.fmt.pix.height * 3;
      temp_needed = my_src_fmt.fmt.pix.width * my_src_fmt.fmt.pix.height * 3;
      break;
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YVU420:
      dest_needed =
	my_dest_fmt.fmt.pix.width * my_dest_fmt.fmt.pix.height * 3 / 2;
      temp_needed =
	my_src_fmt.fmt.pix.width * my_src_fmt.fmt.pix.height * 3 / 2;
      break;
    default:
      V4LCONVERT_ERR("Unknown dest format in conversion\n");
      errno = EINVAL;
      return -1;
  }

  if (dest_size < dest_needed) {
    V4LCONVERT_ERR("destination buffer too small\n");
    errno = EFAULT;
    return -1;
  }


  /* Sometimes we need foo -> rgb -> bar as video processing (whitebalance,
     etc.) can only be done on rgb data */
  if (processing && v4lconvert_processing_needs_double_conversion(
					     my_src_fmt.fmt.pix.pixelformat,
					     my_dest_fmt.fmt.pix.pixelformat))
    convert = 2;
  else if (my_dest_fmt.fmt.pix.pixelformat != my_src_fmt.fmt.pix.pixelformat)
    convert = 1;

  /* convert_pixfmt (only if convert == 2) -> processing -> convert_pixfmt ->
     rotate -> flip -> crop, all steps are optional */
  if (convert == 2) {
    convert1_dest = v4lconvert_alloc_buffer(data,
		     my_src_fmt.fmt.pix.width * my_src_fmt.fmt.pix.height * 3,
		     &data->convert1_buf, &data->convert1_buf_size);
    if (!convert1_dest)
      return -1;

    convert1_dest_size =
      my_src_fmt.fmt.pix.width * my_src_fmt.fmt.pix.height * 3;
    convert2_src = convert1_dest;
  }

  if (convert && (rotate90 || hflip || vflip || crop)) {
    convert2_dest = v4lconvert_alloc_buffer(data, temp_needed,
		     &data->convert2_buf, &data->convert2_buf_size);
    if (!convert2_dest)
      return -1;

    convert2_dest_size = temp_needed;
    rotate90_src = flip_src = crop_src = convert2_dest;
  }

  if (rotate90 && (hflip || vflip || crop)) {
    rotate90_dest = v4lconvert_alloc_buffer(data, temp_needed,
		    &data->rotate90_buf, &data->rotate90_buf_size);
    if (!rotate90_dest)
      return -1;

    flip_src = crop_src = rotate90_dest;
  }

  if ((vflip || hflip) && crop) {
    flip_dest = v4lconvert_alloc_buffer(data, temp_needed, &data->flip_buf,
					&data->flip_buf_size);
    if (!flip_dest)
      return -1;

    crop_src = flip_dest;
  }

  /* Done setting sources / dest and allocating intermediate buffers,
     real conversion / processing / ... starts here. */
  if (convert == 2) {
    res = v4lconvert_convert_pixfmt(data, src, src_size,
				    convert1_dest, convert1_dest_size,
				    &my_src_fmt,
				    V4L2_PIX_FMT_RGB24);
    if (res)
      return res;

    src_size = my_src_fmt.fmt.pix.sizeimage;
  }

  if (processing)
    v4lprocessing_processing(data->processing, convert2_src, &my_src_fmt);

  if (convert) {
    res = v4lconvert_convert_pixfmt(data, convert2_src, src_size,
				    convert2_dest, convert2_dest_size,
				    &my_src_fmt,
				    my_dest_fmt.fmt.pix.pixelformat);
    if (res)
      return res;

    src_size = my_src_fmt.fmt.pix.sizeimage;

    /* We call processing here again in case the source format was not
       rgb, but the dest is. v4lprocessing checks it self it only actually
       does the processing once per frame. */
    if (processing)
      v4lprocessing_processing(data->processing, convert2_dest, &my_src_fmt);
  }

  if (rotate90)
    v4lconvert_rotate90(rotate90_src, rotate90_dest, &my_src_fmt);

  if (hflip || vflip)
    v4lconvert_flip(flip_src, flip_dest, &my_src_fmt, hflip, vflip);

  if (crop)
    v4lconvert_crop(crop_src, dest, &my_src_fmt, &my_dest_fmt);

  return dest_needed;
}

const char *v4lconvert_get_error_message(struct v4lconvert_data *data)
{
  return data->error_msg;
}

static void v4lconvert_get_framesizes(struct v4lconvert_data *data,
  unsigned int pixelformat, int index)
{
  int i, j, match;
  struct v4l2_frmsizeenum frmsize = { .pixel_format = pixelformat };

  for (i = 0; ; i++) {
    frmsize.index = i;
    if (SYS_IOCTL(data->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize))
      break;

    /* We got a framesize, check we don't have the same one already */
    match = 0;
    for (j = 0; j < data->no_framesizes; j++) {
      if (frmsize.type != data->framesizes[j].type)
	continue;

      switch(frmsize.type) {
	case V4L2_FRMSIZE_TYPE_DISCRETE:
	  if(!memcmp(&frmsize.discrete, &data->framesizes[j].discrete,
		     sizeof(frmsize.discrete)))
	    match = 1;
	  break;
	case V4L2_FRMSIZE_TYPE_CONTINUOUS:
	case V4L2_FRMSIZE_TYPE_STEPWISE:
	  if(!memcmp(&frmsize.stepwise, &data->framesizes[j].stepwise,
		     sizeof(frmsize.stepwise)))
	    match = 1;
	  break;
      }
      if (match)
	break;
    }
    /* Add this framesize if it is not already in our list */
    if (!match) {
      if (data->no_framesizes == V4LCONVERT_MAX_FRAMESIZES) {
	fprintf(stderr,
	  "libv4lconvert: warning more framesizes then I can handle!\n");
	return;
      }
      data->framesizes[data->no_framesizes].type = frmsize.type;
      /* We use the pixel_format member to store a bitmask of all
	 supported src_formats which can do this size */
      data->framesizes[data->no_framesizes].pixel_format = 1 << index;
      switch(frmsize.type) {
	case V4L2_FRMSIZE_TYPE_DISCRETE:
	  data->framesizes[data->no_framesizes].discrete = frmsize.discrete;
	  break;
	case V4L2_FRMSIZE_TYPE_CONTINUOUS:
	case V4L2_FRMSIZE_TYPE_STEPWISE:
	  data->framesizes[data->no_framesizes].stepwise = frmsize.stepwise;
	  break;
      }
      data->no_framesizes++;
    }
    else
      data->framesizes[j].pixel_format |= 1 << index;
  }
}

int v4lconvert_enum_framesizes(struct v4lconvert_data *data,
  struct v4l2_frmsizeenum *frmsize)
{
  if (!v4lconvert_supported_dst_format(frmsize->pixel_format)) {
    if (v4lconvert_supported_dst_fmt_only(data)) {
      errno = EINVAL;
      return -1;
    }
    return SYS_IOCTL(data->fd, VIDIOC_ENUM_FRAMESIZES, frmsize);
  }

  if (frmsize->index >= data->no_framesizes) {
    errno = EINVAL;
    return -1;
  }

  frmsize->type = data->framesizes[frmsize->index].type;
  switch(frmsize->type) {
    case V4L2_FRMSIZE_TYPE_DISCRETE:
      frmsize->discrete = data->framesizes[frmsize->index].discrete;
      /* Apply the same rounding algorithm as v4lconvert_try_format */
      frmsize->discrete.width &= ~7;
      frmsize->discrete.height &= ~1;
      break;
    case V4L2_FRMSIZE_TYPE_CONTINUOUS:
    case V4L2_FRMSIZE_TYPE_STEPWISE:
      frmsize->stepwise = data->framesizes[frmsize->index].stepwise;
      break;
  }

  return 0;
}

int v4lconvert_enum_frameintervals(struct v4lconvert_data *data,
  struct v4l2_frmivalenum *frmival)
{
  int res;
  struct v4l2_format src_fmt, dest_fmt;

  if (!v4lconvert_supported_dst_format(frmival->pixel_format)) {
    if (v4lconvert_supported_dst_fmt_only(data)) {
      errno = EINVAL;
      return -1;
    }
    res = SYS_IOCTL(data->fd, VIDIOC_ENUM_FRAMEINTERVALS, frmival);
    if (res)
      V4LCONVERT_ERR("%s\n", strerror(errno));
    return res;
  }

  /* Check which format we will be using to convert to frmival->pixel_format */
  memset(&dest_fmt, 0, sizeof(dest_fmt));
  dest_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  dest_fmt.fmt.pix.pixelformat = frmival->pixel_format;
  dest_fmt.fmt.pix.width = frmival->width;
  dest_fmt.fmt.pix.height = frmival->height;
  if ((res = v4lconvert_try_format(data, &dest_fmt, &src_fmt))) {
    if (res)
      V4LCONVERT_ERR("trying format: %s\n", strerror(errno));
    return res;
  }

  /* Check the requested format is supported exactly as requested */
  if (dest_fmt.fmt.pix.pixelformat != frmival->pixel_format ||
      dest_fmt.fmt.pix.width  != frmival->width ||
      dest_fmt.fmt.pix.height != frmival->height) {
    int frmival_pixformat = frmival->pixel_format;
    int dest_pixformat = dest_fmt.fmt.pix.pixelformat;
    V4LCONVERT_ERR("Could not find matching framesize for: %c%c%c%c %dx%d "
	    "closest match: %c%c%c%c %dx%d\n",
	    frmival_pixformat & 0xff,
	    (frmival_pixformat >> 8) & 0xff,
	    (frmival_pixformat >> 16) & 0xff,
	    frmival_pixformat >> 24,
	    frmival->width, frmival->height,
	    dest_pixformat & 0xff,
	    (dest_pixformat >> 8) & 0xff,
	    (dest_pixformat >> 16) & 0xff,
	    dest_pixformat >> 24,
	    dest_fmt.fmt.pix.width , dest_fmt.fmt.pix.height);
    errno = EINVAL;
    return -1;
  }

  /* Enumerate the frameintervals of the source format we will be using */
  frmival->pixel_format = src_fmt.fmt.pix.pixelformat;
  frmival->width = src_fmt.fmt.pix.width;
  frmival->height = src_fmt.fmt.pix.height;
  res = SYS_IOCTL(data->fd, VIDIOC_ENUM_FRAMEINTERVALS, frmival);
  if (res) {
    int dest_pixfmt = dest_fmt.fmt.pix.pixelformat;
    int src_pixfmt  = src_fmt.fmt.pix.pixelformat;
    V4LCONVERT_ERR("Could not enum frameival index: %d for: %c%c%c%c %dx%d "
	    "using src: %c%c%c%c %dx%d, error: %s\n",
	    frmival->index,
	    dest_pixfmt & 0xff,
	    (dest_pixfmt >> 8) & 0xff,
	    (dest_pixfmt >> 16) & 0xff,
	    dest_pixfmt >> 24,
	    dest_fmt.fmt.pix.width , dest_fmt.fmt.pix.height,
	    src_pixfmt & 0xff,
	    (src_pixfmt >> 8) & 0xff,
	    (src_pixfmt >> 16) & 0xff,
	    src_pixfmt >> 24,
	    src_fmt.fmt.pix.width, src_fmt.fmt.pix.height, strerror(errno));
  }

  /* Restore the requested format in the frmival struct */
  frmival->pixel_format = dest_fmt.fmt.pix.pixelformat;
  frmival->width = dest_fmt.fmt.pix.width;
  frmival->height = dest_fmt.fmt.pix.height;

  return res;
}

int v4lconvert_vidioc_queryctrl(struct v4lconvert_data *data, void *arg) {
  return v4lcontrol_vidioc_queryctrl(data->control, arg);
}

int v4lconvert_vidioc_g_ctrl(struct v4lconvert_data *data, void *arg) {
  return v4lcontrol_vidioc_g_ctrl(data->control, arg);
}

int v4lconvert_vidioc_s_ctrl(struct v4lconvert_data *data, void *arg){
  return v4lcontrol_vidioc_s_ctrl(data->control, arg);
}
