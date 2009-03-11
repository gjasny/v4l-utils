/*

# RGB and YUV crop routines

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

#include <string.h>
#include "libv4lconvert-priv.h"

static void v4lconvert_crop_rgbbgr24(unsigned char *src, unsigned char *dest,
  const struct v4l2_format *src_fmt, const struct v4l2_format *dest_fmt)
{
  int x;
  int startx = (src_fmt->fmt.pix.width - dest_fmt->fmt.pix.width) / 2;
  int starty = (src_fmt->fmt.pix.height - dest_fmt->fmt.pix.height) / 2;

  src += starty * src_fmt->fmt.pix.bytesperline + 3 * startx;

  for (x = 0; x < dest_fmt->fmt.pix.height; x++) {
    memcpy(dest, src, dest_fmt->fmt.pix.width * 3);
    src += src_fmt->fmt.pix.bytesperline;
    dest += dest_fmt->fmt.pix.bytesperline;
  }
}

static void v4lconvert_crop_yuv420(unsigned char *src, unsigned char *dest,
  const struct v4l2_format *src_fmt, const struct v4l2_format *dest_fmt)
{
  int x;
  int startx = (src_fmt->fmt.pix.width - dest_fmt->fmt.pix.width) / 2;
  int starty = (src_fmt->fmt.pix.height - dest_fmt->fmt.pix.height) / 2;
  unsigned char *mysrc = src + starty * src_fmt->fmt.pix.bytesperline + startx;

  /* Y */
  for (x = 0; x < dest_fmt->fmt.pix.height; x++) {
    memcpy(dest, mysrc, dest_fmt->fmt.pix.width);
    mysrc += src_fmt->fmt.pix.bytesperline;
    dest += dest_fmt->fmt.pix.bytesperline;
  }

  /* U */
  mysrc = src + src_fmt->fmt.pix.height * src_fmt->fmt.pix.bytesperline +
	  (starty / 2) * src_fmt->fmt.pix.bytesperline / 2 + startx / 2;
  for (x = 0; x < dest_fmt->fmt.pix.height / 2; x++) {
    memcpy(dest, mysrc, dest_fmt->fmt.pix.width / 2);
    mysrc += src_fmt->fmt.pix.bytesperline / 2;
    dest += dest_fmt->fmt.pix.bytesperline / 2;
  }

  /* V */
  mysrc = src + src_fmt->fmt.pix.height * src_fmt->fmt.pix.bytesperline * 5 / 4
	  + (starty / 2) * src_fmt->fmt.pix.bytesperline / 2 + startx / 2;
  for (x = 0; x < dest_fmt->fmt.pix.height / 2; x++) {
    memcpy(dest, mysrc, dest_fmt->fmt.pix.width / 2);
    mysrc += src_fmt->fmt.pix.bytesperline / 2;
    dest += dest_fmt->fmt.pix.bytesperline / 2;
  }
}

void v4lconvert_crop(unsigned char *src, unsigned char *dest,
  const struct v4l2_format *src_fmt, const struct v4l2_format *dest_fmt)
{
  switch (dest_fmt->fmt.pix.pixelformat) {
    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_BGR24:
      v4lconvert_crop_rgbbgr24(src, dest, src_fmt, dest_fmt);
      break;
    case V4L2_PIX_FMT_YUV420:
      v4lconvert_crop_yuv420(src, dest, src_fmt, dest_fmt);
      break;
  }
}
