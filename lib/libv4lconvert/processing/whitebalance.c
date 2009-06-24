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

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "libv4lprocessing.h"
#include "libv4lprocessing-priv.h"
#include "../libv4lconvert-priv.h" /* for PIX_FMT defines */

#define CLIP256(color) (((color)>0xff)?0xff:(((color)<0)?0:(color)))
#define CLIP(color, min, max) (((color)>(max))?(max):(((color)<(min))?(min):(color)))

static int whitebalance_active(struct v4lprocessing_data *data) {
  int wb;

  wb = v4lcontrol_get_ctrl(data->control, V4LCONTROL_WHITEBALANCE);
  if (!wb) {
    /* Reset cached color averages */
    data->green_avg = 0;
  }

  return wb;
}

static int whitebalance_calculate_lookup_tables_generic(
  struct v4lprocessing_data *data, int green_avg, int comp1_avg, int comp2_avg)
{
  int i, avg_avg;
  const int max_step = 64;

  /* Clip averages (restricts maximum white balance correction) */
  green_avg = CLIP(green_avg, 512, 3072);
  comp1_avg = CLIP(comp1_avg, 512, 3072);
  comp2_avg = CLIP(comp2_avg, 512, 3072);

  /* First frame ? */
  if (data->green_avg == 0) {
    data->green_avg = green_avg;
    data->comp1_avg = comp1_avg;
    data->comp2_avg = comp2_avg;
  } else {
    /* Slowly adjust the averages used for the correction, so that we
       do not get a sudden change in colors */
    if (abs(data->green_avg - green_avg) > max_step) {
      if (data->green_avg < green_avg)
	data->green_avg += max_step;
      else
	data->green_avg -= max_step;
    }
    else
      data->green_avg = green_avg;

    if (abs(data->comp1_avg - comp1_avg) > max_step) {
      if (data->comp1_avg < comp1_avg)
	data->comp1_avg += max_step;
      else
	data->comp1_avg -= max_step;
    }
    else
      data->comp1_avg = comp1_avg;

    if (abs(data->comp2_avg - comp2_avg) > max_step) {
      if (data->comp2_avg < comp2_avg)
	data->comp2_avg += max_step;
      else
	data->comp2_avg -= max_step;
    }
    else
      data->comp2_avg = comp2_avg;
  }

  if (abs(data->green_avg - data->comp1_avg) < max_step &&
      abs(data->green_avg - data->comp2_avg) < max_step &&
      abs(data->comp1_avg - data->comp2_avg) < max_step)
    return 0;

  avg_avg = (data->green_avg + data->comp1_avg + data->comp2_avg) / 3;

  for (i = 0; i < 256; i++) {
    data->comp1[i] = CLIP256(data->comp1[i] * avg_avg / data->comp1_avg);
    data->green[i] = CLIP256(data->green[i] * avg_avg / data->green_avg);
    data->comp2[i] = CLIP256(data->comp2[i] * avg_avg / data->comp2_avg);
  }

  return 1;
}

static int whitebalance_calculate_lookup_tables_bayer(
  struct v4lprocessing_data *data, unsigned char *buf,
  const struct v4l2_format *fmt, int starts_with_green)
{
  int x, y, a1 = 0, a2 = 0, b1 = 0, b2 = 0;
  int green_avg, comp1_avg, comp2_avg;

  for (y = 0; y < fmt->fmt.pix.height; y += 2) {
    for (x = 0; x < fmt->fmt.pix.width; x += 2) {
      a1 += *buf++;
      a2 += *buf++;
    }
    buf += fmt->fmt.pix.bytesperline - fmt->fmt.pix.width;
    for (x = 0; x < fmt->fmt.pix.width; x += 2) {
      b1 += *buf++;
      b2 += *buf++;
    }
    buf += fmt->fmt.pix.bytesperline - fmt->fmt.pix.width;
  }

  if (starts_with_green) {
    green_avg = a1 / 2 + b2 / 2;
    comp1_avg = a2;
    comp2_avg = b1;
  } else {
    green_avg = a2 / 2 + b1 / 2;
    comp1_avg = a1;
    comp2_avg = b2;
  }

  /* Norm avg to ~ 0 - 4095 */
  green_avg /= fmt->fmt.pix.width * fmt->fmt.pix.height / 64;
  comp1_avg /= fmt->fmt.pix.width * fmt->fmt.pix.height / 64;
  comp2_avg /= fmt->fmt.pix.width * fmt->fmt.pix.height / 64;

  return whitebalance_calculate_lookup_tables_generic(data, green_avg,
						      comp1_avg, comp2_avg);
}

static int whitebalance_calculate_lookup_tables_rgb(
  struct v4lprocessing_data *data, unsigned char *buf,
  const struct v4l2_format *fmt)
{
  int x, y, green_avg = 0, comp1_avg = 0, comp2_avg = 0;

  for (y = 0; y < fmt->fmt.pix.height; y++) {
    for (x = 0; x < fmt->fmt.pix.width; x++) {
      comp1_avg += *buf++;
      green_avg += *buf++;
      comp2_avg += *buf++;
    }
    buf += fmt->fmt.pix.bytesperline - fmt->fmt.pix.width * 3;
  }

  /* Norm avg to ~ 0 - 4095 */
  green_avg /= fmt->fmt.pix.width * fmt->fmt.pix.height / 16;
  comp1_avg /= fmt->fmt.pix.width * fmt->fmt.pix.height / 16;
  comp2_avg /= fmt->fmt.pix.width * fmt->fmt.pix.height / 16;

  return whitebalance_calculate_lookup_tables_generic(data, green_avg,
						      comp1_avg, comp2_avg);
}


static int whitebalance_calculate_lookup_tables(
  struct v4lprocessing_data *data,
  unsigned char *buf, const struct v4l2_format *fmt)
{
  switch (fmt->fmt.pix.pixelformat) {
    case V4L2_PIX_FMT_SGBRG8:
    case V4L2_PIX_FMT_SGRBG8: /* Bayer patterns starting with green */
      return whitebalance_calculate_lookup_tables_bayer(data, buf, fmt, 1);

    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_SRGGB8: /* Bayer patterns *NOT* starting with green */
      return whitebalance_calculate_lookup_tables_bayer(data, buf, fmt, 0);

    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_BGR24:
      return whitebalance_calculate_lookup_tables_rgb(data, buf, fmt);
  }

  return 0; /* Should never happen */
}

struct v4lprocessing_filter whitebalance_filter = {
  whitebalance_active, whitebalance_calculate_lookup_tables };
