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

#ifndef __LIBV4LCONVERT_PRIV_H
#define __LIBV4LCONVERT_PRIV_H

#include <stdio.h>
#include "libv4lconvert.h"
#include "tinyjpeg.h"

#ifndef V4L2_PIX_FMT_SPCA501
#define V4L2_PIX_FMT_SPCA501 v4l2_fourcc('S','5','0','1') /* YUYV per line */
#endif

#ifndef V4L2_PIX_FMT_SPCA505
#define V4L2_PIX_FMT_SPCA505 v4l2_fourcc('S','5','0','5') /* YYUV per line */
#endif

#ifndef V4L2_PIX_FMT_SPCA508
#define V4L2_PIX_FMT_SPCA508 v4l2_fourcc('S','5','0','8') /* YUVY per line */
#endif

#ifndef V4L2_PIX_FMT_SPCA561
#define V4L2_PIX_FMT_SPCA561 v4l2_fourcc('S','5','6','1')
#endif

#ifndef V4L2_PIX_FMT_PAC207
#define V4L2_PIX_FMT_PAC207 v4l2_fourcc('P','2','0','7')
#endif

#ifndef V4L2_PIX_FMT_PJPG
#define V4L2_PIX_FMT_PJPG v4l2_fourcc('P', 'J', 'P', 'G')
#endif

#ifndef V4L2_PIX_FMT_SGBRG8
#define V4L2_PIX_FMT_SGBRG8 v4l2_fourcc('G','B','R','G')
#endif

#ifndef V4L2_PIX_FMT_SGRBG8
#define V4L2_PIX_FMT_SGRBG8 v4l2_fourcc('G','R','B','G')
#endif

#ifndef V4L2_PIX_FMT_SRGGB8
#define V4L2_PIX_FMT_SRGGB8 v4l2_fourcc('R','G','G','B')
#endif

#ifndef V4L2_PIX_FMT_YVYU
#define V4L2_PIX_FMT_YVYU v4l2_fourcc('Y', 'V', 'Y', 'U')
#endif

#define V4LCONVERT_ERROR_MSG_SIZE 256
#define V4LCONVERT_MAX_FRAMESIZES 16

#define V4LCONVERT_ERR(...) \
  snprintf(data->error_msg, V4LCONVERT_ERROR_MSG_SIZE, \
  "v4l-convert: error " __VA_ARGS__)

/* Card flags */
#define V4LCONVERT_ROTATE_90  0x01
#define V4LCONVERT_ROTATE_180 0x02
#define V4LCONVERT_IS_UVC     0x04

/* Pixformat flags */
#define V4LCONVERT_COMPRESSED 0x01

struct v4lconvert_data {
  int fd;
  int flags; /* bitfield */
  int supported_src_formats; /* bitfield */
  unsigned int no_formats;
  char error_msg[V4LCONVERT_ERROR_MSG_SIZE];
  struct jdec_private *jdec;
  struct v4l2_frmsizeenum framesizes[V4LCONVERT_MAX_FRAMESIZES];
  unsigned int no_framesizes;
  int convert_buf_size;
  int rotate_buf_size;
  int convert_pixfmt_buf_size;
  unsigned char *convert_buf;
  unsigned char *rotate_buf;
  unsigned char *convert_pixfmt_buf;
};

struct v4lconvert_flags_info {
  unsigned short vendor_id;
  unsigned short product_id;
/* We could also use the USB manufacturer and product strings some devices have
  const char *manufacturer;
  const char *product; */
  int flags;
};

struct v4lconvert_pixfmt {
  unsigned int fmt;
  int flags;
};

void v4lconvert_rgb24_to_yuv420(const unsigned char *src, unsigned char *dest,
  const struct v4l2_format *src_fmt, int bgr, int yvu);

void v4lconvert_yuv420_to_rgb24(const unsigned char *src, unsigned char *dst,
  int width, int height, int yvu);

void v4lconvert_yuv420_to_bgr24(const unsigned char *src, unsigned char *dst,
  int width, int height, int yvu);

void v4lconvert_yuyv_to_rgb24(const unsigned char *src, unsigned char *dst,
  int width, int height);

void v4lconvert_yuyv_to_bgr24(const unsigned char *src, unsigned char *dst,
  int width, int height);

void v4lconvert_yuyv_to_yuv420(const unsigned char *src, unsigned char *dst,
  int width, int height, int yvu);

void v4lconvert_yvyu_to_rgb24(const unsigned char *src, unsigned char *dst,
  int width, int height);

void v4lconvert_yvyu_to_bgr24(const unsigned char *src, unsigned char *dst,
  int width, int height);

void v4lconvert_uyvy_to_rgb24(const unsigned char *src, unsigned char *dst,
  int width, int height);

void v4lconvert_uyvy_to_bgr24(const unsigned char *src, unsigned char *dst,
  int width, int height);

void v4lconvert_uyvy_to_yuv420(const unsigned char *src, unsigned char *dst,
  int width, int height, int yvu);

void v4lconvert_swap_rgb(const unsigned char *src, unsigned char *dst,
  int width, int height);

void v4lconvert_swap_uv(const unsigned char *src, unsigned char *dst,
  const struct v4l2_format *src_fmt);

void v4lconvert_spca501_to_yuv420(const unsigned char *src, unsigned char *dst,
  int width, int height, int yvu);

void v4lconvert_spca505_to_yuv420(const unsigned char *src, unsigned char *dst,
  int width, int height, int yvu);

void v4lconvert_spca508_to_yuv420(const unsigned char *src, unsigned char *dst,
  int width, int height, int yvu);

void v4lconvert_decode_spca561(const unsigned char *src, unsigned char *dst,
  int width, int height);

void v4lconvert_decode_sn9c10x(const unsigned char *src, unsigned char *dst,
  int width, int height);

void v4lconvert_decode_pac207(const unsigned char *src, unsigned char *dst,
  int width, int height);

void v4lconvert_bayer_to_rgb24(const unsigned char *bayer,
  unsigned char *rgb, int width, int height, unsigned int pixfmt);

void v4lconvert_bayer_to_bgr24(const unsigned char *bayer,
  unsigned char *rgb, int width, int height, unsigned int pixfmt);

void v4lconvert_bayer_to_yuv420(const unsigned char *bayer, unsigned char *yuv,
  int width, int height, unsigned int src_pixfmt, int yvu);

void v4lconvert_rotate(unsigned char *src, unsigned char *dest,
  int width, int height, unsigned int pix_fmt, int rotate);

void v4lconvert_crop(unsigned char *src, unsigned char *dest,
  const struct v4l2_format *src_fmt, const struct v4l2_format *dest_fmt);

#endif
