/*

# RGB / YUV flip routines

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

void v4lconvert_flip_rgbbgr24(const unsigned char *src, unsigned char *dst,
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

void v4lconvert_flip_yuv420(const unsigned char *src, unsigned char *dst,
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
