/*
#             (C) 2008 Hans de Goede <j.w.r.degoede@hhs.nl>

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
#include <syscall.h>
#include <unistd.h>
#include "libv4lconvert.h"
#include "libv4lconvert-priv.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define ARRAY_SIZE(x) ((int)sizeof(x)/(int)sizeof((x)[0]))

/* Note for proper functioning of v4lconvert_enum_fmt the first entries in
  supported_src_pixfmts must match with the entries in supported_dst_pixfmts */
#define SUPPORTED_DST_PIXFMTS \
  { V4L2_PIX_FMT_RGB24,  0 }, \
  { V4L2_PIX_FMT_BGR24,  0 }, \
  { V4L2_PIX_FMT_YUV420, 0 }, \
  { V4L2_PIX_FMT_YVU420, 0 }

static void v4lconvert_get_framesizes(struct v4lconvert_data *data,
  unsigned int pixelformat);

/* Note uncompressed formats must go first so that they are prefered by
   v4lconvert_try_format for low resolutions */
static const struct v4lconvert_pixfmt supported_src_pixfmts[] = {
  SUPPORTED_DST_PIXFMTS,
  { V4L2_PIX_FMT_YUYV,    0 },
  { V4L2_PIX_FMT_YVYU,    0 },
  { V4L2_PIX_FMT_SBGGR8,  0 },
  { V4L2_PIX_FMT_SGBRG8,  0 },
  { V4L2_PIX_FMT_SGRBG8,  0 },
  { V4L2_PIX_FMT_SRGGB8,  0 },
  { V4L2_PIX_FMT_SPCA501, 0 },
  { V4L2_PIX_FMT_SPCA505, 0 },
  { V4L2_PIX_FMT_SPCA508, 0 },
  { V4L2_PIX_FMT_MJPEG,   V4LCONVERT_COMPRESSED },
  { V4L2_PIX_FMT_JPEG,    V4LCONVERT_COMPRESSED },
  { V4L2_PIX_FMT_SPCA561, V4LCONVERT_COMPRESSED },
  { V4L2_PIX_FMT_SN9C10X, V4LCONVERT_COMPRESSED },
  { V4L2_PIX_FMT_PAC207,  V4LCONVERT_COMPRESSED },
  { V4L2_PIX_FMT_PJPG,    V4LCONVERT_COMPRESSED },
};

static const struct v4lconvert_pixfmt supported_dst_pixfmts[] = {
  SUPPORTED_DST_PIXFMTS
};

/* List of cams which need special flags */
static const struct v4lconvert_flags_info v4lconvert_flags[] = {
  { "SPC 200NC      ", V4LCONVERT_ROTATE_180 },
  { "SPC 300NC      ", V4LCONVERT_ROTATE_180 }, /* Unconfirmed ! */
  { "SPC210NC       ", V4LCONVERT_ROTATE_180 },
  { "USB Camera (0471:0326)", V4LCONVERT_ROTATE_180 }, /* SPC300NC */
  { "USB Camera (093a:2476)", V4LCONVERT_ROTATE_180 }, /* Genius E-M 112 */
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

  if (!data)
    return NULL;

  data->fd = fd;

  /* Check supported formats */
  for (i = 0; ; i++) {
    struct v4l2_fmtdesc fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };

    fmt.index = i;

    if (syscall(SYS_ioctl, fd, VIDIOC_ENUM_FMT, &fmt))
      break;

    for (j = 0; j < ARRAY_SIZE(supported_src_pixfmts); j++)
      if (fmt.pixelformat == supported_src_pixfmts[j].fmt) {
	data->supported_src_formats |= 1 << j;
	break;
      }

    v4lconvert_get_framesizes(data, fmt.pixelformat);
  }

  data->no_formats = i;

  /* Check if this cam has any special flags */
  if (syscall(SYS_ioctl, fd, VIDIOC_QUERYCAP, &cap) == 0) {
    for (i = 0; i < ARRAY_SIZE(v4lconvert_flags); i++)
      if (!strcmp((const char *)v4lconvert_flags[i].card, (char *)cap.card)) {
	data->flags = v4lconvert_flags[i].flags;
	break;
      }
  }

  return data;
}

void v4lconvert_destroy(struct v4lconvert_data *data)
{
  if (data->jdec) {
    unsigned char *comps[3] = { NULL, NULL, NULL };
    tinyjpeg_set_components(data->jdec, comps, 3);
    tinyjpeg_free(data->jdec);
  }
  free(data->convert_buf);
  free(data->rotate_buf);
  free(data->convert_pixfmt_buf);
  free(data);
}

static int v4lconvert_supported_dst_format(unsigned int pixelformat)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(supported_dst_pixfmts); i++)
    if (supported_dst_pixfmts[i].fmt == pixelformat)
      break;

  return i != ARRAY_SIZE(supported_dst_pixfmts);
}

/* See libv4lconvert.h for description of in / out parameters */
int v4lconvert_enum_fmt(struct v4lconvert_data *data, struct v4l2_fmtdesc *fmt)
{
  int i, no_faked_fmts = 0;
  unsigned int faked_fmts[ARRAY_SIZE(supported_dst_pixfmts)];

  if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE ||
      fmt->index < data->no_formats ||
      !data->supported_src_formats)
    return syscall(SYS_ioctl, data->fd, VIDIOC_ENUM_FMT, fmt);

  for (i = 0; i < ARRAY_SIZE(supported_dst_pixfmts); i++)
    if (!(data->supported_src_formats & (1 << i))) {
      faked_fmts[no_faked_fmts] = supported_dst_pixfmts[i].fmt;
      no_faked_fmts++;
    }

  i = fmt->index - data->no_formats;
  if (i >= no_faked_fmts) {
    errno = EINVAL;
    return -1;
  }

  fmt->flags = 0;
  fmt->pixelformat = faked_fmts[i];
  fmt->description[0] = faked_fmts[i] & 0xff;
  fmt->description[1] = (faked_fmts[i] >> 8) & 0xff;
  fmt->description[2] = (faked_fmts[i] >> 16) & 0xff;
  fmt->description[3] = faked_fmts[i] >> 24;
  fmt->description[4] = '\0';
  memset(fmt->reserved, 0, 4);

  return 0;
}

static int v4lconvert_do_try_format(struct v4lconvert_data *data,
  struct v4l2_format *dest_fmt, struct v4l2_format *src_fmt)
{
  int i;
  unsigned int closest_fmt_size_diff = -1;
  unsigned int desired_pixfmt = dest_fmt->fmt.pix.pixelformat;
  struct v4l2_format try_fmt, closest_fmt = { .type = 0 };

  for (i = 0; i < ARRAY_SIZE(supported_src_pixfmts); i++) {
    /* is this format supported? */
    if (!(data->supported_src_formats & (1 << i)))
      continue;

    try_fmt = *dest_fmt;
    try_fmt.fmt.pix.pixelformat = supported_src_pixfmts[i].fmt;

    if (!syscall(SYS_ioctl, data->fd, VIDIOC_TRY_FMT, &try_fmt))
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

static void v4lconvert_fixup_fmt(struct v4l2_format *fmt)
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
  struct v4l2_format try_src, try_dest = *dest_fmt;

  /* Can we do conversion to the requested format & type? */
  if (!v4lconvert_supported_dst_format(dest_fmt->fmt.pix.pixelformat) ||
      dest_fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE ||
      v4lconvert_do_try_format(data, &try_dest, &try_src)) {
    result = syscall(SYS_ioctl, data->fd, VIDIOC_TRY_FMT, dest_fmt);
    if (src_fmt)
      *src_fmt = *dest_fmt;
    return result;
  }

  /* In case of a non exact resolution match, see if this is a well known
     resolution some apps are hardcoded too and try to give the app what it
     asked for by cropping a slightly larger resolution */
  if (try_dest.fmt.pix.width != desired_width ||
      try_dest.fmt.pix.height != desired_height) {
    for (i = 0; i < ARRAY_SIZE(v4lconvert_crop_res); i++) {
      if (v4lconvert_crop_res[i][0] == desired_width &&
	  v4lconvert_crop_res[i][1] == desired_height) {
	struct v4l2_format try2_src, try2_dest = *dest_fmt;

	/* Note these are chosen so that cropping to vga res just works for
	   vv6410 sensor cams, which have 356x292 and 180x148 */
	try2_dest.fmt.pix.width = desired_width * 113 / 100;
	try2_dest.fmt.pix.height = desired_height * 124 / 100;
	result = v4lconvert_do_try_format(data, &try2_dest, &try2_src);
	if (result == 0 &&
	    ((try2_dest.fmt.pix.width >= desired_width &&
	      try2_dest.fmt.pix.width <= desired_width * 5 / 4 &&
	      try2_dest.fmt.pix.height >= desired_height &&
	      try2_dest.fmt.pix.height <= desired_height * 5 / 4) ||
	     (try2_dest.fmt.pix.width >= desired_width * 2 &&
	      try2_dest.fmt.pix.width <= desired_width * 5 / 2 &&
	      try2_dest.fmt.pix.height >= desired_height &&
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

  /* Are we converting? */
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
  if(src_fmt->fmt.pix.width != dest_fmt->fmt.pix.width ||
     src_fmt->fmt.pix.height != dest_fmt->fmt.pix.height ||
     src_fmt->fmt.pix.pixelformat != dest_fmt->fmt.pix.pixelformat)
    return 1; /* Formats differ */

  if (!(data->flags & (V4LCONVERT_ROTATE_90|V4LCONVERT_ROTATE_180)))
    return 0; /* Formats identical and we don't need flip */

  /* Formats are identical, but we need flip, do we support the dest_fmt? */
  if (!v4lconvert_supported_dst_format(dest_fmt->fmt.pix.pixelformat))
    return 0; /* Needs flip but we cannot do it :( */
  else
    return 1; /* Needs flip and thus conversion */
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
  unsigned char *src, int src_size, unsigned char *dest,
  const struct v4l2_format *src_fmt, unsigned int dest_pix_fmt)
{
  unsigned int header_width, header_height;
  int result = 0, jpeg_flags = TINYJPEG_FLAGS_MJPEG_TABLE;
  unsigned char *components[3];
  unsigned int src_pix_fmt = src_fmt->fmt.pix.pixelformat;
  unsigned int width  = src_fmt->fmt.pix.width;
  unsigned int height = src_fmt->fmt.pix.height;

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
	  if (!(data->flags & V4LCONVERT_ROTATE_90)) {
	    V4LCONVERT_ERR("JPEG needs 90 degree rotation");
	    data->flags |= V4LCONVERT_ROTATE_90;
	    errno = EAGAIN;
	    return -1;
	  }
	} else {
	  V4LCONVERT_ERR("unexpected width / height in JPEG header"
			 "expected: %ux%u, header: %ux%u\n", width, height,
			 header_width, header_height);
	  errno = EIO;
	  return -1;
	}
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
	if (src_pix_fmt == V4L2_PIX_FMT_PJPG) {
	  V4LCONVERT_ERR("Error decompressing JPEG: %s",
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

    /* YUYV line by line formats */
    case V4L2_PIX_FMT_SPCA501:
    case V4L2_PIX_FMT_SPCA505:
    case V4L2_PIX_FMT_SPCA508:
    {
      unsigned char *d;
      int yvu = 0;

      if (dest_pix_fmt != V4L2_PIX_FMT_YUV420 &&
	  dest_pix_fmt != V4L2_PIX_FMT_YVU420) {
	d = v4lconvert_alloc_buffer(data, width * height * 3 / 2,
	      &data->convert_pixfmt_buf, &data->convert_pixfmt_buf_size);
	if (!d)
	  return -1;
      } else
	d = dest;

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

    /* compressed bayer formats */
    case V4L2_PIX_FMT_SPCA561:
    case V4L2_PIX_FMT_SN9C10X:
    case V4L2_PIX_FMT_PAC207:
    {
      unsigned char *tmpbuf;
      unsigned int bayer_fmt = 0;

      tmpbuf = v4lconvert_alloc_buffer(data, width * height,
	    &data->convert_pixfmt_buf, &data->convert_pixfmt_buf_size);
      if (!tmpbuf)
	return -1;

      switch (src_pix_fmt) {
	case V4L2_PIX_FMT_SPCA561:
	  v4lconvert_decode_spca561(src, tmpbuf, width, height);
	  bayer_fmt = V4L2_PIX_FMT_SGBRG8;
	  break;
	case V4L2_PIX_FMT_SN9C10X:
	  v4lconvert_decode_sn9c10x(src, tmpbuf, width, height);
	  bayer_fmt = V4L2_PIX_FMT_SBGGR8;
	  break;
	case V4L2_PIX_FMT_PAC207:
	  v4lconvert_decode_pac207(src, tmpbuf, width, height);
	  bayer_fmt = V4L2_PIX_FMT_SBGGR8;
	  break;
      }

      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_RGB24:
	v4lconvert_bayer_to_rgb24(tmpbuf, dest, width, height, bayer_fmt);
	break;
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_bayer_to_bgr24(tmpbuf, dest, width, height, bayer_fmt);
	break;
      case V4L2_PIX_FMT_YUV420:
	v4lconvert_bayer_to_yuv420(tmpbuf, dest, width, height, bayer_fmt, 0);
	break;
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_bayer_to_yuv420(tmpbuf, dest, width, height, bayer_fmt, 1);
	break;
      }
      break;
    }

    case V4L2_PIX_FMT_RGB24:
      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_swap_rgb(src, dest, width, height);
	break;
      case V4L2_PIX_FMT_YUV420:
	v4lconvert_rgb24_to_yuv420(src, dest, src_fmt, 0, 0);
	break;
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_rgb24_to_yuv420(src, dest, src_fmt, 0, 1);
	break;
      }
      break;

    case V4L2_PIX_FMT_BGR24:
      switch (dest_pix_fmt) {
      case V4L2_PIX_FMT_RGB24:
	v4lconvert_swap_rgb(src, dest, width, height);
	break;
      case V4L2_PIX_FMT_YUV420:
	v4lconvert_rgb24_to_yuv420(src, dest, src_fmt, 1, 0);
	break;
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_rgb24_to_yuv420(src, dest, src_fmt, 1, 1);
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
	v4lconvert_swap_uv(src, dest, src_fmt);
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
	v4lconvert_swap_uv(src, dest, src_fmt);
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
	v4lconvert_yvyu_to_yuv420(src, dest, width, height, 0);
	break;
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_yvyu_to_yuv420(src, dest, width, height, 1);
	break;
      }
      break;

    default:
      V4LCONVERT_ERR("Unknown src format in conversion\n");
      errno = EINVAL;
      return -1;
  }
  return 0;
}

int v4lconvert_convert(struct v4lconvert_data *data,
  const struct v4l2_format *src_fmt,  /* in */
  const struct v4l2_format *dest_fmt, /* in */
  unsigned char *src, int src_size, unsigned char *dest, int dest_size)
{
  int res, dest_needed, temp_needed, convert = 0, rotate = 0, crop = 0;
  unsigned char *convert_dest = dest, *rotate_src = src, *rotate_dest = dest;
  unsigned char *crop_src = src;
  struct v4l2_format my_src_fmt = *src_fmt;
  struct v4l2_format my_dest_fmt = *dest_fmt;

  /* Special case when no conversion is needed */
  if (!v4lconvert_needs_conversion(data, src_fmt, dest_fmt)) {
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

  if (my_dest_fmt.fmt.pix.pixelformat != my_src_fmt.fmt.pix.pixelformat)
    convert = 1;

  if (data->flags & V4LCONVERT_ROTATE_90)
    rotate += 90;
  if (data->flags & V4LCONVERT_ROTATE_180)
    rotate += 180;

  if (my_dest_fmt.fmt.pix.width != my_src_fmt.fmt.pix.width ||
      my_dest_fmt.fmt.pix.height != my_src_fmt.fmt.pix.height)
    crop = 1;

  /* convert_pixfmt -> rotate -> crop, all steps are optional */
  if (convert && (rotate || crop)) {
    convert_dest = v4lconvert_alloc_buffer(data, temp_needed,
		     &data->convert_buf, &data->convert_buf_size);
    if (!convert_dest)
      return -1;

    rotate_src = crop_src = convert_dest;
  }

  if (rotate && crop) {
    rotate_dest = v4lconvert_alloc_buffer(data, temp_needed,
		    &data->rotate_buf, &data->rotate_buf_size);
    if (!rotate_dest)
      return -1;

    crop_src = rotate_dest;
  }

  if (convert) {
    res = v4lconvert_convert_pixfmt(data, src, src_size, convert_dest,
				    &my_src_fmt,
				    my_dest_fmt.fmt.pix.pixelformat);
    if (res)
      return res;

    my_src_fmt.fmt.pix.pixelformat = my_dest_fmt.fmt.pix.pixelformat;
    v4lconvert_fixup_fmt(&my_src_fmt);
  }

  if (rotate)
    v4lconvert_rotate(rotate_src, rotate_dest,
		      my_src_fmt.fmt.pix.width, my_src_fmt.fmt.pix.height,
		      my_src_fmt.fmt.pix.pixelformat, rotate);

  if (crop)
    v4lconvert_crop(crop_src, dest, &my_src_fmt, &my_dest_fmt);

  return dest_needed;
}

const char *v4lconvert_get_error_message(struct v4lconvert_data *data)
{
  return data->error_msg;
}

static void v4lconvert_get_framesizes(struct v4lconvert_data *data,
  unsigned int pixelformat)
{
  int i, j, match;
  struct v4l2_frmsizeenum frmsize = { .pixel_format = pixelformat };

  for (i = 0; ; i++) {
    frmsize.index = i;
    if (syscall(SYS_ioctl, data->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize))
      break;

    /* We got a framesize, check we don't have the same one already */
    match = 0;
    for (j = 0; j < data->no_framesizes && !match; j++) {
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
    }
    /* Add this framesize if it is not already in our list */
    if (!match) {
      if (data->no_framesizes == V4LCONVERT_MAX_FRAMESIZES) {
	fprintf(stderr,
	  "libv4lconvert: warning more framesizes then I can handle!\n");
	return;
      }
      data->framesizes[data->no_framesizes].type = frmsize.type;
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
  }
}

int v4lconvert_enum_framesizes(struct v4lconvert_data *data,
  struct v4l2_frmsizeenum *frmsize)
{
  if (!v4lconvert_supported_dst_format(frmsize->pixel_format))
    return syscall(SYS_ioctl, data->fd, VIDIOC_ENUM_FRAMESIZES, frmsize);

  if (frmsize->index >= data->no_framesizes) {
    errno = EINVAL;
    return -1;
  }

  frmsize->type = data->framesizes[frmsize->index].type;
  switch(frmsize->type) {
    case V4L2_FRMSIZE_TYPE_DISCRETE:
      frmsize->discrete = data->framesizes[frmsize->index].discrete;
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

  if (!v4lconvert_supported_dst_format(frmival->pixel_format))
    return syscall(SYS_ioctl, data->fd, VIDIOC_ENUM_FRAMEINTERVALS, frmival);

  /* Check which format we will be using to convert to frmival->pixel_format */
  memset(&dest_fmt, 0, sizeof(dest_fmt));
  dest_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  dest_fmt.fmt.pix.pixelformat = frmival->pixel_format;
  dest_fmt.fmt.pix.width = frmival->width;
  dest_fmt.fmt.pix.height = frmival->height;
  if ((res = v4lconvert_try_format(data, &dest_fmt, &src_fmt)))
    return res;

  /* Check the requested format is supported exactly as requested */
  if (dest_fmt.fmt.pix.pixelformat != frmival->pixel_format ||
      dest_fmt.fmt.pix.width  != frmival->width ||
      dest_fmt.fmt.pix.height != frmival->height) {
    errno = EINVAL;
    return -1;
  }

  /* Enumerate the frameintervals of the source format we will be using */
  frmival->pixel_format = src_fmt.fmt.pix.pixelformat;
  frmival->width = src_fmt.fmt.pix.width;
  frmival->height = src_fmt.fmt.pix.height;
  res = syscall(SYS_ioctl, data->fd, VIDIOC_ENUM_FRAMEINTERVALS, frmival);

  /* Restore the requested format in the frmival struct */
  frmival->pixel_format = dest_fmt.fmt.pix.pixelformat;
  frmival->width = dest_fmt.fmt.pix.width;
  frmival->height = dest_fmt.fmt.pix.height;

  return res;
}
