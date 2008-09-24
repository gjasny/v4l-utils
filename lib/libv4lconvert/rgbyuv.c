/*

# RGB <-> YUV conversion routines

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

#define RGB2YUV(r,g,b,y,u,v) \
    (y) = ((  8453*(r) + 16594*(g) +  3223*(b) +  524288) >> 15); \
    (u) = (( -4878*(r) -  9578*(g) + 14456*(b) + 4210688) >> 15); \
    (v) = (( 14456*(r) - 12105*(g) -  2351*(b) + 4210688) >> 15)

#define YUV2R(y, u, v) ({ \
  int r = (y) + ((((v)-128)*1436) >> 10); r > 255 ? 255 : r < 0 ? 0 : r; })
#define YUV2G(y, u, v) ({ \
  int g = (y) - ((((u)-128)*352 + ((v)-128)*731) >> 10); g > 255 ? 255 : g < 0 ? 0 : g; })
#define YUV2B(y, u, v) ({ \
  int b = (y) + ((((u)-128)*1814) >> 10); b > 255 ? 255 : b < 0 ? 0 : b; })

#define CLIP(color) (unsigned char)(((color)>0xFF)?0xff:(((color)<0)?0:(color)))

void v4lconvert_yuv420_to_bgr24(const unsigned char *src, unsigned char *dest,
  int width, int height)
{
  int i,j;

  const unsigned char *ysrc = src;
  const unsigned char *usrc = src + width * height;
  const unsigned char *vsrc = usrc + (width * height) / 4;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j += 2) {
#if 1 /* fast slightly less accurate multiplication free code */
      int u1 = (((*usrc - 128) << 7) +  (*usrc - 128)) >> 6;
      int rg = (((*usrc - 128) << 1) +  (*usrc - 128) +
		((*vsrc - 128) << 2) + ((*vsrc - 128) << 1)) >> 3;
      int v1 = (((*vsrc - 128) << 1) +  (*vsrc - 128)) >> 1;

      *dest++ = CLIP(*ysrc + u1);
      *dest++ = CLIP(*ysrc - rg);
      *dest++ = CLIP(*ysrc + v1);
      ysrc++;

      *dest++ = CLIP(*ysrc + u1);
      *dest++ = CLIP(*ysrc - rg);
      *dest++ = CLIP(*ysrc + v1);
#else
      *dest++ = YUV2B(*ysrc, *usrc, *vsrc);
      *dest++ = YUV2G(*ysrc, *usrc, *vsrc);
      *dest++ = YUV2R(*ysrc, *usrc, *vsrc);
      ysrc++;

      *dest++ = YUV2B(*ysrc, *usrc, *vsrc);
      *dest++ = YUV2G(*ysrc, *usrc, *vsrc);
      *dest++ = YUV2R(*ysrc, *usrc, *vsrc);
#endif
      ysrc++;
      usrc++;
      vsrc++;
    }
    /* Rewind u and v for next line */
    if (i&1) {
      usrc -= width / 2;
      vsrc -= width / 2;
    }
  }
}

void v4lconvert_yuv420_to_rgb24(const unsigned char *src, unsigned char *dest,
  int width, int height)
{
  int i,j;

  const unsigned char *ysrc = src;
  const unsigned char *usrc = src + width * height;
  const unsigned char *vsrc = usrc + (width * height) / 4;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j += 2) {
#if 1 /* fast slightly less accurate multiplication free code */
      int u1 = (((*usrc - 128) << 7) +  (*usrc - 128)) >> 6;
      int rg = (((*usrc - 128) << 1) +  (*usrc - 128) +
		((*vsrc - 128) << 2) + ((*vsrc - 128) << 1)) >> 3;
      int v1 = (((*vsrc - 128) << 1) +  (*vsrc - 128)) >> 1;

      *dest++ = CLIP(*ysrc + v1);
      *dest++ = CLIP(*ysrc - rg);
      *dest++ = CLIP(*ysrc + u1);
      ysrc++;

      *dest++ = CLIP(*ysrc + v1);
      *dest++ = CLIP(*ysrc - rg);
      *dest++ = CLIP(*ysrc + u1);
#else
      *dest++ = YUV2R(*ysrc, *usrc, *vsrc);
      *dest++ = YUV2G(*ysrc, *usrc, *vsrc);
      *dest++ = YUV2B(*ysrc, *usrc, *vsrc);
      ysrc++;

      *dest++ = YUV2R(*ysrc, *usrc, *vsrc);
      *dest++ = YUV2G(*ysrc, *usrc, *vsrc);
      *dest++ = YUV2B(*ysrc, *usrc, *vsrc);
#endif
      ysrc++;
      usrc++;
      vsrc++;
    }
    /* Rewind u and v for next line */
    if (i&1) {
      usrc -= width / 2;
      vsrc -= width / 2;
    }
  }
}

void v4lconvert_yuyv_to_bgr24(const unsigned char *src, unsigned char *dest,
  int width, int height)
{
  int j;

  while (--height >= 0) {
    for (j = 0; j < width; j += 2) {
      int u = src[1];
      int v = src[3];
      int u1 = (((u - 128) << 7) +  (u - 128)) >> 6;
      int rg = (((u - 128) << 1) +  (u - 128) +
		((v - 128) << 2) + ((v - 128) << 1)) >> 3;
      int v1 = (((v - 128) << 1) +  (v - 128)) >> 1;

      *dest++ = CLIP(src[0] + u1);
      *dest++ = CLIP(src[0] - rg);
      *dest++ = CLIP(src[0] + v1);

      *dest++ = CLIP(src[2] + u1);
      *dest++ = CLIP(src[2] - rg);
      *dest++ = CLIP(src[2] + v1);
      src += 4;
    }
  }
}

void v4lconvert_yuyv_to_rgb24(const unsigned char *src, unsigned char *dest,
  int width, int height)
{
  int j;

  while (--height >= 0) {
    for (j = 0; j < width; j += 2) {
      int u = src[1];
      int v = src[3];
      int u1 = (((u - 128) << 7) +  (u - 128)) >> 6;
      int rg = (((u - 128) << 1) +  (u - 128) +
		((v - 128) << 2) + ((v - 128) << 1)) >> 3;
      int v1 = (((v - 128) << 1) +  (v - 128)) >> 1;

      *dest++ = CLIP(src[0] + v1);
      *dest++ = CLIP(src[0] - rg);
      *dest++ = CLIP(src[0] + u1);

      *dest++ = CLIP(src[2] + v1);
      *dest++ = CLIP(src[2] - rg);
      *dest++ = CLIP(src[2] + u1);
      src += 4;
    }
  }
}

void v4lconvert_yuyv_to_yuv420(const unsigned char *src, unsigned char *dest,
  int width, int height)
{
  int i, j;
  const unsigned char *src1;
  unsigned char *vdest;

  /* copy the Y values */
  src1 = src;
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j += 2) {
      *dest++ = src1[0];
      *dest++ = src1[2];
      src1 += 4;
    }
  }

  /* copy the U and V values */
  src++;				/* point to V */
  src1 = src + width * 2;		/* next line */
  vdest = dest + width * height / 4;
  for (i = 0; i < height; i += 2) {
    for (j = 0; j < width; j += 2) {
      *dest++  = ((int) src[0] + src1[0]) / 2;	/* U */
      *vdest++ = ((int) src[2] + src1[2]) / 2;	/* V */
      src += 4;
      src1 += 4;
    }
    src = src1;
    src1 += width * 2;
  }
}

void v4lconvert_yvyu_to_bgr24(const unsigned char *src, unsigned char *dest,
  int width, int height)
{
  int j;

  while (--height >= 0) {
    for (j = 0; j < width; j += 2) {
      int u = src[3];
      int v = src[1];
      int u1 = (((u - 128) << 7) +  (u - 128)) >> 6;
      int rg = (((u - 128) << 1) +  (u - 128) +
		((v - 128) << 2) + ((v - 128) << 1)) >> 3;
      int v1 = (((v - 128) << 1) +  (v - 128)) >> 1;

      *dest++ = CLIP(src[0] + u1);
      *dest++ = CLIP(src[0] - rg);
      *dest++ = CLIP(src[0] + v1);

      *dest++ = CLIP(src[2] + u1);
      *dest++ = CLIP(src[2] - rg);
      *dest++ = CLIP(src[2] + v1);
      src += 4;
    }
  }
}

void v4lconvert_yvyu_to_rgb24(const unsigned char *src, unsigned char *dest,
  int width, int height)
{
  int j;

  while (--height >= 0) {
    for (j = 0; j < width; j += 2) {
      int u = src[3];
      int v = src[1];
      int u1 = (((u - 128) << 7) +  (u - 128)) >> 6;
      int rg = (((u - 128) << 1) +  (u - 128) +
		((v - 128) << 2) + ((v - 128) << 1)) >> 3;
      int v1 = (((v - 128) << 1) +  (v - 128)) >> 1;

      *dest++ = CLIP(src[0] + v1);
      *dest++ = CLIP(src[0] - rg);
      *dest++ = CLIP(src[0] + u1);

      *dest++ = CLIP(src[2] + v1);
      *dest++ = CLIP(src[2] - rg);
      *dest++ = CLIP(src[2] + u1);
      src += 4;
    }
  }
}

void v4lconvert_yvyu_to_yuv420(const unsigned char *src, unsigned char *dest,
  int width, int height)
{
  int i, j;
  const unsigned char *src1;
  unsigned char *vdest;

  /* copy the Y values */
  src1 = src;
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j += 2) {
      *dest++ = src1[0];
      *dest++ = src1[2];
      src1 += 4;
    }
  }

  /* copy the U and V values */
  src++;				/* point to V */
  src1 = src + width * 2;		/* next line */
  vdest = dest + width * height / 4;
  for (i = 0; i < height; i += 2) {
    for (j = 0; j < width; j += 2) {
      *dest++  = ((int) src[2] + src1[2]) / 2;	/* U */
      *vdest++ = ((int) src[0] + src1[0]) / 2;	/* V */
      src += 4;
      src1 += 4;
    }
    src = src1;
    src1 += width * 2;
  }
}

void v4lconvert_swap_rgb(const unsigned char *src, unsigned char *dst,
  int width, int height)
{
  int i;

  for (i = 0; i < (width * height); i++) {
    unsigned char tmp0, tmp1;
    tmp0 = *src++;
    tmp1 = *src++;
    *dst++ = *src++;
    *dst++ = tmp1;
    *dst++ = tmp0;
  }
}
