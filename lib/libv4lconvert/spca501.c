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

#include "libv4lconvert-priv.h"

/* YUYV per line */
void v4lconvert_spca501_to_yuv420(const unsigned char *src, unsigned char *dst,
  int width, int height, int yvu)
{
  int i,j;
  unsigned long *lsrc = (unsigned long *)src;

  for (i = 0; i < height; i += 2) {
    /* -128 - 127 --> 0 - 255 and copy first line Y */
    unsigned long *ldst = (unsigned long *)(dst + i * width);
    for (j = 0; j < width; j += sizeof(long)) {
      *ldst = *lsrc++;
      *ldst++ ^= 0x8080808080808080ULL;
    }

    /* -128 - 127 --> 0 - 255 and copy 1 line U */
    if (yvu)
      ldst = (unsigned long *)(dst + (width * height * 5) / 4 + i * width / 4);
    else
      ldst = (unsigned long *)(dst + width * height + i * width / 4);
    for (j = 0; j < width/2; j += sizeof(long)) {
      *ldst = *lsrc++;
      *ldst++ ^= 0x8080808080808080ULL;
    }

    /* -128 - 127 --> 0 - 255 and copy second line Y */
    ldst = (unsigned long *)(dst + i * width + width);
    for (j = 0; j < width; j += sizeof(long)) {
      *ldst = *lsrc++;
      *ldst++ ^= 0x8080808080808080ULL;
    }

    /* -128 - 127 --> 0 - 255 and copy 1 line V */
    if (yvu)
      ldst = (unsigned long *)(dst + width * height + i * width / 4);
    else
      ldst = (unsigned long *)(dst + (width * height * 5) / 4 + i * width / 4);
    for (j = 0; j < width/2; j += sizeof(long)) {
      *ldst = *lsrc++;
      *ldst++ ^= 0x8080808080808080ULL;
    }
  }
}

/* YYUV per line */
void v4lconvert_spca505_to_yuv420(const unsigned char *src, unsigned char *dst,
  int width, int height, int yvu)
{
  int i,j;
  unsigned long *lsrc = (unsigned long *)src;

  for (i = 0; i < height; i += 2) {
    /* -128 - 127 --> 0 - 255 and copy 2 lines of Y */
    unsigned long *ldst = (unsigned long *)(dst + i * width);
    for (j = 0; j < width*2; j += sizeof(long)) {
      *ldst = *lsrc++;
      *ldst++ ^= 0x8080808080808080ULL;
    }

    /* -128 - 127 --> 0 - 255 and copy 1 line U */
    if (yvu)
      ldst = (unsigned long *)(dst + (width * height * 5) / 4 + i * width / 4);
    else
      ldst = (unsigned long *)(dst + width * height + i * width / 4);
    for (j = 0; j < width/2; j += sizeof(long)) {
      *ldst = *lsrc++;
      *ldst++ ^= 0x8080808080808080ULL;
    }

    /* -128 - 127 --> 0 - 255 and copy 1 line V */
    if (yvu)
      ldst = (unsigned long *)(dst + width * height + i * width / 4);
    else
      ldst = (unsigned long *)(dst + (width * height * 5) / 4 + i * width / 4);
    for (j = 0; j < width/2; j += sizeof(long)) {
      *ldst = *lsrc++;
      *ldst++ ^= 0x8080808080808080ULL;
    }
  }
}

/* YUVY per line */
void v4lconvert_spca508_to_yuv420(const unsigned char *src, unsigned char *dst,
  int width, int height, int yvu)
{
  int i,j;
  unsigned long *lsrc = (unsigned long *)src;

  for (i = 0; i < height; i += 2) {
    /* -128 - 127 --> 0 - 255 and copy first line Y */
    unsigned long *ldst = (unsigned long *)(dst + i * width);
    for (j = 0; j < width; j += sizeof(long)) {
      *ldst = *lsrc++;
      *ldst++ ^= 0x8080808080808080ULL;
    }

    /* -128 - 127 --> 0 - 255 and copy 1 line U */
    if (yvu)
      ldst = (unsigned long *)(dst + (width * height * 5) / 4 + i * width / 4);
    else
      ldst = (unsigned long *)(dst + width * height + i * width / 4);
    for (j = 0; j < width/2; j += sizeof(long)) {
      *ldst = *lsrc++;
      *ldst++ ^= 0x8080808080808080ULL;
    }

    /* -128 - 127 --> 0 - 255 and copy 1 line V */
    if (yvu)
      ldst = (unsigned long *)(dst + width * height + i * width / 4);
    else
      ldst = (unsigned long *)(dst + (width * height * 5) / 4 + i * width / 4);
    for (j = 0; j < width/2; j += sizeof(long)) {
      *ldst = *lsrc++;
      *ldst++ ^= 0x8080808080808080ULL;
    }

    /* -128 - 127 --> 0 - 255 and copy second line Y */
    ldst = (unsigned long *)(dst + i * width + width);
    for (j = 0; j < width; j += sizeof(long)) {
      *ldst = *lsrc++;
      *ldst++ ^= 0x8080808080808080ULL;
    }
  }
}
