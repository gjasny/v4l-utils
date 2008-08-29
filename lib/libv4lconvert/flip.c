/*

# RGB / YUV flip/rotate routines

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

#include "libv4lconvert-priv.h"

void v4lconvert_rotate180_rgbbgr24(const unsigned char *src, unsigned char *dst,
  int width, int height)
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

void v4lconvert_rotate180_yuv420(const unsigned char *src, unsigned char *dst,
  int width, int height)
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

void v4lconvert_rotate90_rgbbgr24(const unsigned char *src, unsigned char *dst,
  int destwidth, int destheight)
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

void v4lconvert_rotate90_yuv420(const unsigned char *src, unsigned char *dst,
  int destwidth, int destheight)
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
