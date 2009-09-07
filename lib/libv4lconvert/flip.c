/*

# RGB / YUV flip/rotate routines

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

#include <string.h>
#include "libv4lconvert-priv.h"

static void v4lconvert_vflip_rgbbgr24(unsigned char *src, unsigned char *dest,
  struct v4l2_format *fmt)
{
  int y;

  src += fmt->fmt.pix.height * fmt->fmt.pix.bytesperline;
  for (y = 0; y < fmt->fmt.pix.height; y++) {
    src -= fmt->fmt.pix.bytesperline;
    memcpy(dest, src, fmt->fmt.pix.width * 3);
    dest += fmt->fmt.pix.width * 3;
  }
}

static void v4lconvert_vflip_yuv420(unsigned char *src, unsigned char *dest,
  struct v4l2_format *fmt)
{
  int y;

  /* First flip the Y plane */
  src += fmt->fmt.pix.height * fmt->fmt.pix.bytesperline;
  for (y = 0; y < fmt->fmt.pix.height; y++) {
    src -= fmt->fmt.pix.bytesperline;
    memcpy(dest, src, fmt->fmt.pix.width);
    dest += fmt->fmt.pix.width;
  }

  /* Now flip the U plane */
  src += fmt->fmt.pix.height * fmt->fmt.pix.bytesperline * 5 / 4;
  for (y = 0; y < fmt->fmt.pix.height / 2; y++) {
    src -= fmt->fmt.pix.bytesperline / 2;
    memcpy(dest, src, fmt->fmt.pix.width / 2);
    dest += fmt->fmt.pix.width / 2;
  }

  /* Last flip the V plane */
  src += fmt->fmt.pix.height * fmt->fmt.pix.bytesperline / 2;
  for (y = 0; y < fmt->fmt.pix.height / 2; y++) {
    src -= fmt->fmt.pix.bytesperline / 2;
    memcpy(dest, src, fmt->fmt.pix.width / 2);
    dest += fmt->fmt.pix.width / 2;
  }
}

static void v4lconvert_hflip_rgbbgr24(unsigned char *src, unsigned char *dest,
  struct v4l2_format *fmt)
{
  int x, y;

  for (y = 0; y < fmt->fmt.pix.height; y++) {
    src += fmt->fmt.pix.width * 3;
    for (x = 0; x < fmt->fmt.pix.width; x++) {
      src -= 3;
      dest[0] = src[0];
      dest[1] = src[1];
      dest[2] = src[2];
      dest += 3;
    }
    src += fmt->fmt.pix.bytesperline;
  }
}

static void v4lconvert_hflip_yuv420(unsigned char *src, unsigned char *dest,
  struct v4l2_format *fmt)
{
  int x, y;

  /* First flip the Y plane */
  for (y = 0; y < fmt->fmt.pix.height; y++) {
    src += fmt->fmt.pix.width;
    for (x = 0; x < fmt->fmt.pix.width; x++)
      *dest++ = *--src;
    src += fmt->fmt.pix.bytesperline;
  }

  /* Now flip the U plane */
  for (y = 0; y < fmt->fmt.pix.height / 2; y++) {
    src += fmt->fmt.pix.width / 2;
    for (x = 0; x < fmt->fmt.pix.width / 2; x++)
      *dest++ = *--src;
    src += fmt->fmt.pix.bytesperline / 2;
  }

  /* Last flip the V plane */
  for (y = 0; y < fmt->fmt.pix.height / 2; y++) {
    src += fmt->fmt.pix.width / 2;
    for (x = 0; x < fmt->fmt.pix.width / 2; x++)
      *dest++ = *--src;
    src += fmt->fmt.pix.bytesperline / 2;
  }
}

static void v4lconvert_rotate180_rgbbgr24(const unsigned char *src,
  unsigned char *dst, int width, int height)
{
  int i;

  src += 3 * width * height - 3;

  for (i = 0; i < width * height; i++) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst += 3;
    src -= 3;
  }
}

static void v4lconvert_rotate180_yuv420(const unsigned char *src,
  unsigned char *dst, int width, int height)
{
  int i;

  /* First flip x and y of the Y plane */
  src += width * height - 1;
  for (i = 0; i < width * height; i++)
    *dst++ = *src--;

  /* Now flip the U plane */
  src += width * height * 5 / 4;
  for (i = 0; i < width * height / 4; i++)
    *dst++ = *src--;

  /* Last flip the V plane */
  src += width * height / 2;
  for (i = 0; i < width * height / 4; i++)
    *dst++ = *src--;
}

static void v4lconvert_rotate90_rgbbgr24(const unsigned char *src,
  unsigned char *dst, int destwidth, int destheight)
{
  int x, y;
#define srcwidth destheight
#define srcheight destwidth

  for (y = 0; y < destheight; y++)
    for (x = 0; x < destwidth; x++) {
      int offset = ((srcheight - x - 1) * srcwidth + y) * 3;
      *dst++ = src[offset++];
      *dst++ = src[offset++];
      *dst++ = src[offset];
    }
}

static void v4lconvert_rotate90_yuv420(const unsigned char *src,
  unsigned char *dst, int destwidth, int destheight)
{
  int x, y;

  /* Y-plane */
  for (y = 0; y < destheight; y++)
    for (x = 0; x < destwidth; x++) {
      int offset = (srcheight - x - 1) * srcwidth + y;
      *dst++ = src[offset];
    }

  /* U-plane */
  src += srcwidth * srcheight;
  destwidth /= 2;
  destheight /= 2;
  for (y = 0; y < destheight; y++)
    for (x = 0; x < destwidth; x++) {
      int offset = (srcheight - x - 1) * srcwidth + y;
      *dst++ = src[offset];
    }

  /* V-plane */
  src += srcwidth * srcheight;
  for (y = 0; y < destheight; y++)
    for (x = 0; x < destwidth; x++) {
      int offset = (srcheight - x - 1) * srcwidth + y;
      *dst++ = src[offset];
    }
}

void v4lconvert_rotate90(unsigned char *src, unsigned char *dest,
  struct v4l2_format *fmt)
{
  int tmp;

  tmp = fmt->fmt.pix.width;
  fmt->fmt.pix.width = fmt->fmt.pix.height;
  fmt->fmt.pix.height = tmp;

  switch (fmt->fmt.pix.pixelformat) {
    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_BGR24:
      v4lconvert_rotate90_rgbbgr24(src, dest, fmt->fmt.pix.width,
				   fmt->fmt.pix.height);
      break;
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YVU420:
      v4lconvert_rotate90_yuv420(src, dest, fmt->fmt.pix.width,
				 fmt->fmt.pix.height);
      break;
  }
  v4lconvert_fixup_fmt(fmt);
}

void v4lconvert_flip(unsigned char *src, unsigned char *dest,
  struct v4l2_format *fmt, int hflip, int vflip)
{
  if (vflip && hflip) {
    switch (fmt->fmt.pix.pixelformat) {
      case V4L2_PIX_FMT_RGB24:
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_rotate180_rgbbgr24(src, dest, fmt->fmt.pix.width,
				      fmt->fmt.pix.height);
	break;
      case V4L2_PIX_FMT_YUV420:
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_rotate180_yuv420(src, dest, fmt->fmt.pix.width,
				    fmt->fmt.pix.height);
	break;
    }
  } else if (hflip) {
    switch (fmt->fmt.pix.pixelformat) {
      case V4L2_PIX_FMT_RGB24:
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_hflip_rgbbgr24(src, dest, fmt);
	break;
      case V4L2_PIX_FMT_YUV420:
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_hflip_yuv420(src, dest, fmt);
	break;
    }
  } else if (vflip) {
    switch (fmt->fmt.pix.pixelformat) {
      case V4L2_PIX_FMT_RGB24:
      case V4L2_PIX_FMT_BGR24:
	v4lconvert_vflip_rgbbgr24(src, dest, fmt);
	break;
      case V4L2_PIX_FMT_YUV420:
      case V4L2_PIX_FMT_YVU420:
	v4lconvert_vflip_yuv420(src, dest, fmt);
	break;
    }
  }

  /* Our newly written data has no padding */
  v4lconvert_fixup_fmt(fmt);
}
