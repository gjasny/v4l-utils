/*
 * lib4lconvert, video4linux2 format conversion lib
 *             (C) 2008 Hans de Goede <hdegoede@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Note: original bayer_to_bgr24 code from :
 * 1394-Based Digital Camera Control Library
 *
 * Bayer pattern decoding functions
 *
 * Written by Damien Douxchamps and Frederic Devernay
 *
 * Note that the original bayer.c in libdc1394 supports many different
 * bayer decode algorithms, for lib4lconvert the one in this file has been
 * chosen (and optimized a bit) and the other algorithm's have been removed,
 * see bayer.c from libdc1394 for all supported algorithms
 */

#include <string.h>
#include "libv4lconvert-priv.h"

/**************************************************************
 *     Color conversion functions for cameras that can        *
 * output raw-Bayer pattern images, such as some Basler and   *
 * Point Grey camera. Most of the algos presented here come   *
 * from http://www-ise.stanford.edu/~tingchen/ and have been  *
 * converted from Matlab to C and extended to all elementary  *
 * patterns.                                                  *
 **************************************************************/

/* insprired by OpenCV's Bayer decoding */
static void v4lconvert_border_bayer_line_to_bgr24(
  const unsigned char* bayer, const unsigned char* adjacent_bayer,
  unsigned char *bgr, int width, int start_with_green, int blue_line)
{
  int t0, t1;

  if (start_with_green) {
    /* First pixel */
    if (blue_line) {
      *bgr++ = bayer[1];
      *bgr++ = bayer[0];
      *bgr++ = adjacent_bayer[0];
    } else {
      *bgr++ = adjacent_bayer[0];
      *bgr++ = bayer[0];
      *bgr++ = bayer[1];
    }
    /* Second pixel */
    t0 = (bayer[0] + bayer[2] + adjacent_bayer[1] + 1) / 3;
    t1 = (adjacent_bayer[0] + adjacent_bayer[2] + 1) >> 1;
    if (blue_line) {
      *bgr++ = bayer[1];
      *bgr++ = t0;
      *bgr++ = t1;
    } else {
      *bgr++ = t1;
      *bgr++ = t0;
      *bgr++ = bayer[1];
    }
    bayer++;
    adjacent_bayer++;
    width -= 2;
  } else {
    /* First pixel */
    t0 = (bayer[1] + adjacent_bayer[0] + 1) >> 1;
    if (blue_line) {
      *bgr++ = bayer[0];
      *bgr++ = t0;
      *bgr++ = adjacent_bayer[1];
    } else {
      *bgr++ = adjacent_bayer[1];
      *bgr++ = t0;
      *bgr++ = bayer[0];
    }
    width--;
  }

  if (blue_line) {
    for ( ; width > 2; width -= 2) {
      t0 = (bayer[0] + bayer[2] + 1) >> 1;
      *bgr++ = t0;
      *bgr++ = bayer[1];
      *bgr++ = adjacent_bayer[1];
      bayer++;
      adjacent_bayer++;

      t0 = (bayer[0] + bayer[2] + adjacent_bayer[1] + 1) / 3;
      t1 = (adjacent_bayer[0] + adjacent_bayer[2] + 1) >> 1;
      *bgr++ = bayer[1];
      *bgr++ = t0;
      *bgr++ = t1;
      bayer++;
      adjacent_bayer++;
    }
  } else {
    for ( ; width > 2; width -= 2) {
      t0 = (bayer[0] + bayer[2] + 1) >> 1;
      *bgr++ = adjacent_bayer[1];
      *bgr++ = bayer[1];
      *bgr++ = t0;
      bayer++;
      adjacent_bayer++;

      t0 = (bayer[0] + bayer[2] + adjacent_bayer[1] + 1) / 3;
      t1 = (adjacent_bayer[0] + adjacent_bayer[2] + 1) >> 1;
      *bgr++ = t1;
      *bgr++ = t0;
      *bgr++ = bayer[1];
      bayer++;
      adjacent_bayer++;
    }
  }

  if (width == 2) {
    /* Second to last pixel */
    t0 = (bayer[0] + bayer[2] + 1) >> 1;
    if (blue_line) {
      *bgr++ = t0;
      *bgr++ = bayer[1];
      *bgr++ = adjacent_bayer[1];
    } else {
      *bgr++ = adjacent_bayer[1];
      *bgr++ = bayer[1];
      *bgr++ = t0;
    }
    /* Last pixel */
    t0 = (bayer[1] + adjacent_bayer[2] + 1) >> 1;
    if (blue_line) {
      *bgr++ = bayer[2];
      *bgr++ = t0;
      *bgr++ = adjacent_bayer[1];
    } else {
      *bgr++ = adjacent_bayer[1];
      *bgr++ = t0;
      *bgr++ = bayer[2];
    }
  } else {
    /* Last pixel */
    if (blue_line) {
      *bgr++ = bayer[0];
      *bgr++ = bayer[1];
      *bgr++ = adjacent_bayer[1];
    } else {
      *bgr++ = adjacent_bayer[1];
      *bgr++ = bayer[1];
      *bgr++ = bayer[0];
    }
  }
}

/* From libdc1394, which on turn was based on OpenCV's Bayer decoding */
static void bayer_to_rgbbgr24(const unsigned char *bayer,
  unsigned char *bgr, int width, int height, unsigned int pixfmt,
	int start_with_green, int blue_line)
{
    /* render the first line */
    v4lconvert_border_bayer_line_to_bgr24(bayer, bayer + width, bgr, width,
      start_with_green, blue_line);
    bgr += width * 3;

    /* reduce height by 2 because of the special case top/bottom line */
    for (height -= 2; height; height--) {
	int t0, t1;
	/* (width - 2) because of the border */
	const unsigned char *bayerEnd = bayer + (width - 2);

	if (start_with_green) {
	    /* OpenCV has a bug in the next line, which was
	       t0 = (bayer[0] + bayer[width * 2] + 1) >> 1; */
	    t0 = (bayer[1] + bayer[width * 2 + 1] + 1) >> 1;
	    /* Write first pixel */
	    t1 = (bayer[0] + bayer[width * 2] + bayer[width + 1] + 1) / 3;
	    if (blue_line) {
	      *bgr++ = t0;
	      *bgr++ = t1;
	      *bgr++ = bayer[width];
	    } else {
	      *bgr++ = bayer[width];
	      *bgr++ = t1;
	      *bgr++ = t0;
	    }

	    /* Write second pixel */
	    t1 = (bayer[width] + bayer[width + 2] + 1) >> 1;
	    if (blue_line) {
	      *bgr++ = t0;
	      *bgr++ = bayer[width + 1];
	      *bgr++ = t1;
	    } else {
	      *bgr++ = t1;
	      *bgr++ = bayer[width + 1];
	      *bgr++ = t0;
	    }
	    bayer++;
	} else {
	    /* Write first pixel */
	    t0 = (bayer[0] + bayer[width * 2] + 1) >> 1;
	    if (blue_line) {
	      *bgr++ = t0;
	      *bgr++ = bayer[width];
	      *bgr++ = bayer[width + 1];
	    } else {
	      *bgr++ = bayer[width + 1];
	      *bgr++ = bayer[width];
	      *bgr++ = t0;
	    }
	}

	if (blue_line) {
	    for (; bayer <= bayerEnd - 2; bayer += 2) {
		t0 = (bayer[0] + bayer[2] + bayer[width * 2] +
		      bayer[width * 2 + 2] + 2) >> 2;
		t1 = (bayer[1] + bayer[width] +
		      bayer[width + 2] + bayer[width * 2 + 1] +
		      2) >> 2;
		*bgr++ = t0;
		*bgr++ = t1;
		*bgr++ = bayer[width + 1];

		t0 = (bayer[2] + bayer[width * 2 + 2] + 1) >> 1;
		t1 = (bayer[width + 1] + bayer[width + 3] +
		      1) >> 1;
		*bgr++ = t0;
		*bgr++ = bayer[width + 2];
		*bgr++ = t1;
	    }
	} else {
	    for (; bayer <= bayerEnd - 2; bayer += 2) {
		t0 = (bayer[0] + bayer[2] + bayer[width * 2] +
		      bayer[width * 2 + 2] + 2) >> 2;
		t1 = (bayer[1] + bayer[width] +
		      bayer[width + 2] + bayer[width * 2 + 1] +
		      2) >> 2;
		*bgr++ = bayer[width + 1];
		*bgr++ = t1;
		*bgr++ = t0;

		t0 = (bayer[2] + bayer[width * 2 + 2] + 1) >> 1;
		t1 = (bayer[width + 1] + bayer[width + 3] +
		      1) >> 1;
		*bgr++ = t1;
		*bgr++ = bayer[width + 2];
		*bgr++ = t0;
	    }
	}

	if (bayer < bayerEnd) {
	    /* write second to last pixel */
	    t0 = (bayer[0] + bayer[2] + bayer[width * 2] +
		  bayer[width * 2 + 2] + 2) >> 2;
	    t1 = (bayer[1] + bayer[width] +
		  bayer[width + 2] + bayer[width * 2 + 1] +
		  2) >> 2;
	    if (blue_line) {
	      *bgr++ = t0;
	      *bgr++ = t1;
	      *bgr++ = bayer[width + 1];
	    } else {
	      *bgr++ = bayer[width + 1];
	      *bgr++ = t1;
	      *bgr++ = t0;
	    }
	    /* write last pixel */
	    t0 = (bayer[2] + bayer[width * 2 + 2] + 1) >> 1;
	    if (blue_line) {
	      *bgr++ = t0;
	      *bgr++ = bayer[width + 2];
	      *bgr++ = bayer[width + 1];
	    } else {
	      *bgr++ = bayer[width + 1];
	      *bgr++ = bayer[width + 2];
	      *bgr++ = t0;
	    }
	    bayer++;
	} else {
	    /* write last pixel */
	    t0 = (bayer[0] + bayer[width * 2] + 1) >> 1;
	    t1 = (bayer[1] + bayer[width * 2 + 1] + bayer[width] + 1) / 3;
	    if (blue_line) {
	      *bgr++ = t0;
	      *bgr++ = t1;
	      *bgr++ = bayer[width + 1];
	    } else {
	      *bgr++ = bayer[width + 1];
	      *bgr++ = t1;
	      *bgr++ = t0;
	    }
	}

	/* skip 2 border pixels */
	bayer += 2;

	blue_line = !blue_line;
	start_with_green = !start_with_green;
    }

    /* render the last line */
    v4lconvert_border_bayer_line_to_bgr24(bayer + width, bayer, bgr, width,
      !start_with_green, !blue_line);
}

void v4lconvert_bayer_to_rgb24(const unsigned char *bayer,
  unsigned char *bgr, int width, int height, unsigned int pixfmt)
{
	bayer_to_rgbbgr24(bayer, bgr, width, height, pixfmt,
		pixfmt == V4L2_PIX_FMT_SGBRG8		/* start with green */
			|| pixfmt == V4L2_PIX_FMT_SGRBG8,
		pixfmt != V4L2_PIX_FMT_SBGGR8		/* blue line */
			&& pixfmt != V4L2_PIX_FMT_SGBRG8);
}

void v4lconvert_bayer_to_bgr24(const unsigned char *bayer,
  unsigned char *bgr, int width, int height, unsigned int pixfmt)
{
	bayer_to_rgbbgr24(bayer, bgr, width, height, pixfmt,
		pixfmt == V4L2_PIX_FMT_SGBRG8		/* start with green */
			|| pixfmt == V4L2_PIX_FMT_SGRBG8,
		pixfmt == V4L2_PIX_FMT_SBGGR8		/* blue line */
			|| pixfmt == V4L2_PIX_FMT_SGBRG8);
}

static void v4lconvert_border_bayer_line_to_y(
  const unsigned char* bayer, const unsigned char* adjacent_bayer,
  unsigned char *y, int width, int start_with_green, int blue_line)
{
  int t0, t1;

  if (start_with_green) {
    /* First pixel */
    if (blue_line) {
      *y++ = (8453*adjacent_bayer[0] + 16594*bayer[0] + 3223*bayer[1] + 524288)
	     >> 15;
    } else {
      *y++ = (8453*bayer[1] + 16594*bayer[0] + 3223*adjacent_bayer[0] + 524288)
	     >> 15;
    }
    /* Second pixel */
    t0 = bayer[0] + bayer[2] + adjacent_bayer[1];
    t1 = adjacent_bayer[0] + adjacent_bayer[2];
    if (blue_line) {
      *y++ = (4226*t1 + 5531*t0 + 3223*bayer[1] + 524288) >> 15;
    } else {
      *y++ = (8453*bayer[1] + 5531*t0 + 1611*t1 + 524288) >> 15;
    }
    bayer++;
    adjacent_bayer++;
    width -= 2;
  } else {
    /* First pixel */
    t0 = bayer[1] + adjacent_bayer[0];
    if (blue_line) {
      *y++ = (8453*adjacent_bayer[1] + 8297*t0 + 3223*bayer[0] + 524288)
	     >> 15;
    } else {
      *y++ = (8453*bayer[0] + 8297*t0 + 3223*adjacent_bayer[1] + 524288)
	     >> 15;
    }
    width--;
  }

  if (blue_line) {
    for ( ; width > 2; width -= 2) {
      t0 = bayer[0] + bayer[2];
      *y++ = (8453*adjacent_bayer[1] + 16594*bayer[1] + 1611*t0 + 524288)
	     >> 15;
      bayer++;
      adjacent_bayer++;

      t0 = bayer[0] + bayer[2] + adjacent_bayer[1];
      t1 = adjacent_bayer[0] + adjacent_bayer[2];
      *y++ = (4226*t1 + 5531*t0 + 3223*bayer[1] + 524288) >> 15;
      bayer++;
      adjacent_bayer++;
    }
  } else {
    for ( ; width > 2; width -= 2) {
      t0 = bayer[0] + bayer[2];
      *y++ = (4226*t0 + 16594*bayer[1] + 3223*adjacent_bayer[1] + 524288)
	     >> 15;
      bayer++;
      adjacent_bayer++;

      t0 = bayer[0] + bayer[2] + adjacent_bayer[1];
      t1 = adjacent_bayer[0] + adjacent_bayer[2];
      *y++ = (8453*bayer[1] + 5531*t0 + 1611*t1 + 524288) >> 15;
      bayer++;
      adjacent_bayer++;
    }
  }

  if (width == 2) {
    /* Second to last pixel */
    t0 = bayer[0] + bayer[2];
    if (blue_line) {
      *y++ = (8453*adjacent_bayer[1] + 16594*bayer[1] + 1611*t0 + 524288)
	     >> 15;
    } else {
      *y++ = (4226*t0 + 16594*bayer[1] + 3223*adjacent_bayer[1] + 524288)
	     >> 15;
    }
    /* Last pixel */
    t0 = bayer[1] + adjacent_bayer[2];
    if (blue_line) {
      *y++ = (8453*adjacent_bayer[1] + 8297*t0 + 3223*bayer[2] + 524288)
	     >> 15;
    } else {
      *y++ = (8453*bayer[2] + 8297*t0 + 3223*adjacent_bayer[1] + 524288)
	     >> 15;
    }
  } else {
    /* Last pixel */
    if (blue_line) {
      *y++ = (8453*adjacent_bayer[1] + 16594*bayer[1] + 3223*bayer[0] + 524288)
	     >> 15;
    } else {
      *y++ = (8453*bayer[0] + 16594*bayer[1] + 3223*adjacent_bayer[1] + 524288)
	     >> 15;
    }
  }
}

void v4lconvert_bayer_to_yuv420(const unsigned char *bayer, unsigned char *yuv,
  int width, int height, unsigned int src_pixfmt, int yvu)
{
  int blue_line = 0, start_with_green = 0, x, y;
  unsigned char *ydst = yuv;
  unsigned char *udst, *vdst;

  if (yvu) {
    vdst = yuv + width * height;
    udst = vdst + width * height / 4;
  } else {
    udst = yuv + width * height;
    vdst = udst + width * height / 4;
  }

  /* First calculate the u and v planes 2x2 pixels at a time */
  switch (src_pixfmt) {
    case V4L2_PIX_FMT_SBGGR8:
      for (y = 0; y < height; y += 2) {
	for (x = 0; x < width; x += 2) {
	  int b, g, r;
	  b  = bayer[x];
	  g  = bayer[x+1];
	  g += bayer[x+width];
	  r  = bayer[x+width+1];
	  *udst++ = (-4878 * r - 4789 * g + 14456 * b + 4210688) >> 15;
	  *vdst++ = (14456 * r - 6052 * g -  2351 * b + 4210688) >> 15;
	}
	bayer += 2 * width;
      }
      blue_line = 1;
      break;

    case V4L2_PIX_FMT_SRGGB8:
      for (y = 0; y < height; y += 2) {
	for (x = 0; x < width; x += 2) {
	  int b, g, r;
	  r  = bayer[x];
	  g  = bayer[x+1];
	  g += bayer[x+width];
	  b  = bayer[x+width+1];
	  *udst++ = (-4878 * r - 4789 * g + 14456 * b + 4210688) >> 15;
	  *vdst++ = (14456 * r - 6052 * g -  2351 * b + 4210688) >> 15;
	}
	bayer += 2 * width;
      }
      break;

    case V4L2_PIX_FMT_SGBRG8:
      for (y = 0; y < height; y += 2) {
	for (x = 0; x < width; x += 2) {
	  int b, g, r;
	  g  = bayer[x];
	  b  = bayer[x+1];
	  r  = bayer[x+width];
	  g += bayer[x+width+1];
	  *udst++ = (-4878 * r - 4789 * g + 14456 * b + 4210688) >> 15;
	  *vdst++ = (14456 * r - 6052 * g -  2351 * b + 4210688) >> 15;
	}
	bayer += 2 * width;
      }
      blue_line = 1;
      start_with_green = 1;
      break;

    case V4L2_PIX_FMT_SGRBG8:
      for (y = 0; y < height; y += 2) {
	for (x = 0; x < width; x += 2) {
	  int b, g, r;
	  g  = bayer[x];
	  r  = bayer[x+1];
	  b  = bayer[x+width];
	  g += bayer[x+width+1];
	  *udst++ = (-4878 * r - 4789 * g + 14456 * b + 4210688) >> 15;
	  *vdst++ = (14456 * r - 6052 * g -  2351 * b + 4210688) >> 15;
	}
	bayer += 2 * width;
      }
      start_with_green = 1;
      break;
  }

  bayer -= width * height;

  /* render the first line */
  v4lconvert_border_bayer_line_to_y(bayer, bayer + width, ydst, width,
    start_with_green, blue_line);
  ydst += width;

  /* reduce height by 2 because of the border */
  for (height -= 2; height; height--) {
    int t0, t1;
    /* (width - 2) because of the border */
    const unsigned char *bayerEnd = bayer + (width - 2);

    if (start_with_green) {
      t0 = bayer[1] + bayer[width * 2 + 1];
      /* Write first pixel */
      t1 = bayer[0] + bayer[width * 2] + bayer[width + 1];
      if (blue_line)
	*ydst++ = (8453*bayer[width] + 5516*t1 + 1661*t0 + 524288) >> 15;
      else
	*ydst++ = (4226*t0 + 5516*t1 + 3223*bayer[width] + 524288) >> 15;

      /* Write second pixel */
      t1 = bayer[width] + bayer[width + 2];
      if (blue_line)
	*ydst++ = (4226*t1 + 16594*bayer[width+1] + 1611*t0 + 524288) >> 15;
      else
	*ydst++ = (4226*t0 + 16594*bayer[width+1] + 1611*t1 + 524288) >> 15;
      bayer++;
    } else {
      /* Write first pixel */
      t0 = bayer[0] + bayer[width * 2];
      if (blue_line) {
	*ydst++ = (8453*bayer[width+1] + 16594*bayer[width] + 1661*t0 +
		   524288) >> 15;
      } else {
	*ydst++ = (4226*t0 + 16594*bayer[width] + 3223*bayer[width+1] +
		   524288) >> 15;
      }
    }

    if (blue_line) {
      for (; bayer <= bayerEnd - 2; bayer += 2) {
	t0 = bayer[0] + bayer[2] + bayer[width * 2] + bayer[width * 2 + 2];
	t1 = bayer[1] + bayer[width] + bayer[width + 2] + bayer[width * 2 + 1];
	*ydst++ = (8453*bayer[width+1] + 4148*t1 + 806*t0 + 524288) >> 15;

	t0 = bayer[2] + bayer[width * 2 + 2];
	t1 = bayer[width + 1] + bayer[width + 3];
	*ydst++ = (4226*t1 + 16594*bayer[width+2] + 1611*t0 + 524288) >> 15;
      }
    } else {
      for (; bayer <= bayerEnd - 2; bayer += 2) {
	t0 = bayer[0] + bayer[2] + bayer[width * 2] + bayer[width * 2 + 2];
	t1 = bayer[1] + bayer[width] + bayer[width + 2] + bayer[width * 2 + 1];
	*ydst++ = (2113*t0 + 4148*t1 + 3223*bayer[width+1] + 524288) >> 15;

	t0 = bayer[2] + bayer[width * 2 + 2];
	t1 = bayer[width + 1] + bayer[width + 3];
	*ydst++ = (4226*t0 + 16594*bayer[width+2] + 1611*t1 + 524288) >> 15;
      }
    }

    if (bayer < bayerEnd) {
      /* Write second to last pixel */
      t0 = bayer[0] + bayer[2] + bayer[width * 2] + bayer[width * 2 + 2];
      t1 = bayer[1] + bayer[width] + bayer[width + 2] + bayer[width * 2 + 1];
      if (blue_line)
	*ydst++ = (8453*bayer[width+1] + 4148*t1 + 806*t0 + 524288) >> 15;
      else
	*ydst++ = (2113*t0 + 4148*t1 + 3223*bayer[width+1] + 524288) >> 15;

      /* write last pixel */
      t0 = bayer[2] + bayer[width * 2 + 2];
      if (blue_line) {
	*ydst++ = (8453*bayer[width+1] + 16594*bayer[width+2] + 1661*t0 +
		   524288) >> 15;
      } else {
	*ydst++ = (4226*t0 + 16594*bayer[width+2] + 3223*bayer[width+1] +
		   524288) >> 15;
      }
      bayer++;
    } else {
      /* write last pixel */
      t0 = bayer[0] + bayer[width * 2];
      t1 = bayer[1] + bayer[width * 2 + 1] + bayer[width];
      if (blue_line)
	*ydst++ = (8453*bayer[width+1] + 5516*t1 + 1661*t0 + 524288) >> 15;
      else
	*ydst++ = (4226*t0 + 5516*t1 + 3223*bayer[width+1] + 524288) >> 15;
    }

    /* skip 2 border pixels */
    bayer += 2;

    blue_line = !blue_line;
    start_with_green = !start_with_green;
  }

  /* render the last line */
  v4lconvert_border_bayer_line_to_y(bayer + width, bayer, ydst, width,
    !start_with_green, !blue_line);
}
