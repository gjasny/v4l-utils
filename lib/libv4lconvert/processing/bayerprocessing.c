/*
#             (C) 2008-2009 Elmar Kleijn <elmar_kleijn@hotmail.com>
#             (C) 2008-2009 Sjoerd Piepenbrink <need4weed@gmail.com>
#             (C) 2008-2009 Hans de Goede <hdegoede@redhat.com>

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

#include "libv4lprocessing-priv.h"
#include "../libv4lconvert-priv.h" /* for PIX_FMT defines */

void bayer_normalize_analyse(unsigned char *buf, int width, int height,
  struct v4lprocessing_data *data)
{
  int value, max = 0, min = 255;
  unsigned char *buf_end = buf + width * height;

  while (buf < buf_end) {
    value = *buf++;
    if (max < value)
      max = value;
    if (min > value)
      min = value;
  }

  data->comp1 = ((data->norm_high_bound - data->norm_low_bound) << 16) / (max - min);
  data->offset1 = min;
  data->offset2 = data->norm_low_bound;
}

void bayer_whitebalance_analyse(unsigned char *src_buffer, int width,
  int height, unsigned int pix_fmt, struct v4lprocessing_data *data)
{
  int i, j, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  float green_avg, x_avg, y_avg, avg_avg;
  unsigned char *buf = src_buffer;

  int start_with_green = pix_fmt == V4L2_PIX_FMT_SGBRG8 ||
    pix_fmt == V4L2_PIX_FMT_SGRBG8;

  for (i = 0; i < height; i += 2) {
    for (j = 0; j < width; j += 2) {
      x1 += *buf++;
      x2 += *buf++;
    }
    for (j = 0; j < width; j += 2) {
      y1 += *buf++;
      y2 += *buf++;
    }
  }

  if (start_with_green) {
    green_avg = (x1 + y2) / 2;
    x_avg = x2;
    y_avg = y1;
  } else {
    green_avg = (x2 + y1) / 2;
    x_avg = x1;
    y_avg = y2;
  }

  avg_avg = (green_avg + x_avg + y_avg) / 3;

  data->comp1 = (avg_avg / green_avg) * 65536;
  data->comp2 = (avg_avg / x_avg) * 65536;
  data->comp3 = (avg_avg / y_avg) * 65536;
}

void bayer_normalize_whitebalance_analyse(unsigned char *buf, int width,
  int height, unsigned int pix_fmt, struct v4lprocessing_data *data)
{
  int i, j, value, max = 0, min = 255, n_fac, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  float green_avg, x_avg, y_avg, avg_avg;

  int start_with_green = pix_fmt == V4L2_PIX_FMT_SGBRG8 ||
    pix_fmt == V4L2_PIX_FMT_SGRBG8;

  for (i = 0; i < height; i += 2) {
    for (j = 0; j < width; j += 2) {
      x1 += *buf;
      value = *buf++;
      if (max < value)
	max = value;
      if (min > value)
	min = value;
      x2 += *buf;
      value = *buf++;
      if (max < value)
	max = value;
      if (min > value)
	min = value;
    }
    for (j = 0; j < width; j += 2) {
      y1 += *buf;
      value = *buf++;
      if (max < value)
	max = value;
      if (min > value)
	min = value;
      y2 += *buf;
      value = *buf++;
      if (max < value)
	max = value;
      if (min > value)
	min = value;
    }
  }

  if (start_with_green) {
    green_avg = (x1 + y2) / 2;
    x_avg = x2;
    y_avg = y1;
  } else {
    green_avg = (x2 + y1) / 2;
    x_avg = x1;
    y_avg = y2;
  }

  n_fac = ((data->norm_high_bound - data->norm_low_bound) << 16) / (max - min);

  avg_avg = (green_avg + x_avg + y_avg) / 3;

  data->comp1 = (avg_avg / green_avg) * n_fac;
  data->comp2 = (avg_avg / x_avg) * n_fac;
  data->comp3 = (avg_avg / y_avg) * n_fac;

  data->offset1 = min;
  data->offset2 = data->norm_low_bound;
}

#define CLIP(color) (unsigned char)(((color)>0xff)?0xff:(((color)<0)?0:(color)))
#define TOP(color) (unsigned char)(((color)>0xff)?0xff:(color))

void bayer_normalize(unsigned char *buf, int width,
  int height, struct v4lprocessing_data *data)
{
  int value;
  unsigned char *buf_end = buf + width * height;

  while (buf < buf_end) {
    value = ((data->comp1 * (*buf - data->offset1)) >> 16) + data->offset2;
    *buf++ = CLIP(value);
  }
}

void bayer_whitebalance(unsigned char *src_buffer, int width,
  int height, unsigned int pix_fmt,
  struct v4lprocessing_data *data)
{
  int i, j, value;
  int limit = width * height;
  unsigned char *buf = src_buffer;

  int start_with_green = pix_fmt == V4L2_PIX_FMT_SGBRG8 ||
    pix_fmt == V4L2_PIX_FMT_SGRBG8;

  if (start_with_green) {
    for (i = 0; i < height; i += 2) {
      for (j = 0; j < width; j += 2) {
	value = (*buf * data->comp1) >> 16;
	*buf++ = TOP(value);
	value = (*buf * data->comp2) >> 16;
	*buf++ = TOP(value);
      }
      for (j = 0; j < width; j += 2) {
	value = (*buf * data->comp3) >> 16;
	*buf++ = TOP(value);
	value = (*buf * data->comp1) >> 16;
	*buf++ = TOP(value);
      }
    }
  } else {
    for (i = 0; i < height; i += 2) {
      for (j = 0; j < width; j += 2) {
	value = (*buf * data->comp2) >> 16;
	*buf++ = TOP(value);
	value = (*buf * data->comp1) >> 16;
	*buf++ = TOP(value);
      }
      for (j = 0; j < width; j += 2) {
	value = (*buf * data->comp1) >> 16;
	*buf++ = TOP(value);
	value = (*buf * data->comp3) >> 16;
	*buf++ = TOP(value);
      }
    }
  }
}

void bayer_normalize_whitebalance(unsigned char *src_buffer, int width,
  int height, unsigned int pix_fmt,
  struct v4lprocessing_data *data)
{
  int i, j, value;
  int limit = width * height;
  unsigned char *buf = src_buffer;

  int start_with_green = pix_fmt == V4L2_PIX_FMT_SGBRG8 ||
    pix_fmt == V4L2_PIX_FMT_SGRBG8;

  if (start_with_green) {
    for (i = 0; i < height; i += 2) {
      for (j = 0; j < width; j += 2) {
	value = ((data->comp1 * (*buf - data->offset1)) >> 16) + data->offset2;
	*buf++ = CLIP(value);
	value = ((data->comp2 * (*buf - data->offset1)) >> 16) + data->offset2;
	*buf++ = CLIP(value);
      }
      for (j = 0; j < width; j += 2) {
	value = ((data->comp3 * (*buf - data->offset1)) >> 16) + data->offset2;
	*buf++ = CLIP(value);
	value = ((data->comp1 * (*buf - data->offset1)) >> 16) + data->offset2;
	*buf++ = CLIP(value);
      }
    }
  } else {
    for (i = 0; i < height; i += 2) {
      for (j = 0; j < width; j += 2) {
	value = ((data->comp2 * (*buf - data->offset1)) >> 16) + data->offset2;
	*buf++ = CLIP(value);
	value = ((data->comp1 * (*buf - data->offset1)) >> 16) + data->offset2;
	*buf++ = CLIP(value);
      }
      for (j = 0; j < width; j += 2) {
	value = ((data->comp1 * (*buf - data->offset1)) >> 16) + data->offset2;
	*buf++ = CLIP(value);
	value = ((data->comp3 * (*buf - data->offset1)) >> 16) + data->offset2;
	*buf++ = CLIP(value);
      }
    }
  }
}
