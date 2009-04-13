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

void rgb_normalize_analyse(unsigned char *buf, int width, int height,
  struct v4lprocessing_data *data)
{
  int value, max = 0, min = 255;
  unsigned char *buf_end = buf + width * height * 3;

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

void rgb_whitebalance_analyse(unsigned char *buf, int width, int height,
  struct v4lprocessing_data *data)
{
  int value, x = 0, y = 0, z = 0;
  float x_avg, y_avg, z_avg, avg_avg;
  unsigned char *buf_end = buf + width * height * 3;

  while (buf < buf_end) {
    x += *buf++;
    y += *buf++;
    z += *buf++;
  }

  x_avg = x;
  y_avg = y;
  z_avg = z;
  avg_avg = (x_avg + y_avg + z_avg) / 3;

  data->comp1 = (avg_avg / x_avg) * 65536;
  data->comp2 = (avg_avg / y_avg) * 65536;
  data->comp3 = (avg_avg / z_avg) * 65536;
}

void rgb_normalize_whitebalance_analyse(unsigned char *buf,
  int width, int height, struct v4lprocessing_data *data)
{
  int value, max = 0, min = 255;
  int n_fac, wb_max, x = 0, y = 0, z = 0;
  float x_avg, y_avg, z_avg, avg_avg;
  unsigned char *buf_end = buf + width * height * 3;

  while (buf < buf_end) {
    x += *buf;
    value = *buf++;
    if (max < value)
      max = value;
    if (min > value)
      min = value;
    y += *buf;
    value = *buf++;
    if (max < value)
      max = value;
    if (min > value)
      min = value;
    z += *buf;
    value = *buf++;
    if (max < value)
      max = value;
    if (min > value)
      min = value;
  }

  x_avg = x;
  y_avg = y;
  z_avg = z;
  avg_avg = (x_avg + y_avg + z_avg) / 3;

  n_fac = ((data->norm_high_bound - data->norm_low_bound) << 16) / (max - min);

  data->comp1 = (avg_avg / x_avg) * n_fac;
  data->comp2 = (avg_avg / y_avg) * n_fac;
  data->comp3 = (avg_avg / z_avg) * n_fac;

  data->offset1 = min;
  data->offset2 = data->norm_low_bound;
}

#define CLIP(color) (unsigned char)(((color)>0xff)?0xff:(((color)<0)?0:(color)))
#define TOP(color) (unsigned char)(((color)>0xff)?0xff:(color))

void rgb_normalize(unsigned char *buf, int width, int height,
  struct v4lprocessing_data *data)
{
  int value;
  unsigned char *buf_end = buf + width * height * 3;

  while (buf < buf_end) {
    value = ((data->comp1 * (*buf - data->offset1)) >> 16) +
      data->offset2;
    *buf++ = CLIP(value);
  }
}

void rgb_whitebalance(unsigned char *buf, int width, int height,
  struct v4lprocessing_data *data)
{
  int value;
  unsigned char *buf_end = buf + width * height * 3;

  while (buf < buf_end) {
    value = (*buf * data->comp1) >> 16;
    *buf++ = TOP(value);
    value = (*buf * data->comp2) >> 16;
    *buf++ = TOP(value);
    value = (*buf * data->comp3) >> 16;
    *buf++ = TOP(value);
  }
}

void rgb_normalize_whitebalance(unsigned char *buf, int width, int height,
  struct v4lprocessing_data *data)
{
  int i, value;
  int limit = width * height * 3;
  unsigned char *buf_end = buf + width * height * 3;

  while (buf < buf_end) {
    value = ((data->comp1 * (*buf - data->offset1)) >> 16) + data->offset2;
    *buf++ = CLIP(value);
    value = ((data->comp2 * (*buf - data->offset1)) >> 16) + data->offset2;
    *buf++ = CLIP(value);
    value = ((data->comp3 * (*buf - data->offset1)) >> 16) + data->offset2;
    *buf++ = CLIP(value);
  }
}
