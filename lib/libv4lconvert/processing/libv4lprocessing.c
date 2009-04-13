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
#include <syscall.h>
#include <unistd.h>
#include "libv4lprocessing.h"
#include "libv4lprocessing-priv.h"
#include "../libv4lconvert-priv.h" /* for PIX_FMT defines */

struct v4lprocessing_data *v4lprocessing_create(struct v4lcontrol_data* control)
{
  struct v4lprocessing_data *data =
    calloc(1, sizeof(struct v4lprocessing_data));

  if (!data)
    return NULL;

  data->control = control;

  return data;
}

void v4lprocessing_destroy(struct v4lprocessing_data *data)
{
  free(data);
}

static int v4lprocessing_get_process(struct v4lprocessing_data *data,
  unsigned int pix_fmt)
{
  int process = V4L2PROCESSING_PROCESS_NONE;

  switch(pix_fmt) {
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_SGBRG8:
    case V4L2_PIX_FMT_SGRBG8:
    case V4L2_PIX_FMT_SRGGB8:
      if (v4lcontrol_get_ctrl(data->control, V4LCONTROL_NORMALIZE)) {
	process |= V4L2PROCESSING_PROCESS_BAYER_NORMALIZE;
      }
      if (v4lcontrol_get_ctrl(data->control, V4LCONTROL_WHITEBALANCE)) {
	process |= V4L2PROCESSING_PROCESS_BAYER_WHITEBALANCE;
      }
      break;
    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_BGR24:
      if (v4lcontrol_get_ctrl(data->control, V4LCONTROL_NORMALIZE)) {
	process |= V4L2PROCESSING_PROCESS_RGB_NORMALIZE;
      }
      if (v4lcontrol_get_ctrl(data->control, V4LCONTROL_WHITEBALANCE)) {
	process |= V4L2PROCESSING_PROCESS_RGB_WHITEBALANCE;
      }
      break;
  }

  return process;
}

static void v4lprocessing_update_processing_data(
  struct v4lprocessing_data *data,
  unsigned int pix_fmt, unsigned char *buf, unsigned int width,
  unsigned int height)
{
  switch (data->process) {
    case V4L2PROCESSING_PROCESS_BAYER_NORMALIZE:
      bayer_normalize_analyse(buf, width, height, data);
      break;

    case V4L2PROCESSING_PROCESS_BAYER_WHITEBALANCE:
      bayer_whitebalance_analyse(buf, width, height, pix_fmt, data);
      break;

    case V4L2PROCESSING_PROCESS_BAYER_NORMALIZE_WHITEBALANCE:
      bayer_normalize_whitebalance_analyse(buf, width, height, pix_fmt, data);
      break;

    case V4L2PROCESSING_PROCESS_RGB_NORMALIZE:
      rgb_normalize_analyse(buf, width, height, data);
      break;

    case V4L2PROCESSING_PROCESS_RGB_WHITEBALANCE:
      rgb_whitebalance_analyse(buf, width, height, data);
      break;

    case V4L2PROCESSING_PROCESS_RGB_NORMALIZE_WHITEBALANCE:
      rgb_normalize_whitebalance_analyse(buf, width, height, data);
      break;
  }
}

int v4lprocessing_pre_processing(struct v4lprocessing_data *data)
{
  data->do_process =
    v4lcontrol_get_ctrl(data->control, V4LCONTROL_WHITEBALANCE) ||
    v4lcontrol_get_ctrl(data->control, V4LCONTROL_NORMALIZE);

  if (!data->do_process)
    data->process = V4L2PROCESSING_PROCESS_NONE;

  return data->do_process;
}

void v4lprocessing_processing(struct v4lprocessing_data *data,
  unsigned char *buf, const struct v4l2_format *fmt)
{
  int low_bound, high_bound, process;

  if (!data->do_process)
    return;

  process = v4lprocessing_get_process(data, fmt->fmt.pix.pixelformat);
  if (process == V4L2PROCESSING_PROCESS_NONE) {
    return;
  }

  low_bound = v4lcontrol_get_ctrl(data->control, V4LCONTROL_NORM_LOW_BOUND);
  high_bound = v4lcontrol_get_ctrl(data->control, V4LCONTROL_NORM_HIGH_BOUND);

  if (process != data->process || low_bound != data->norm_low_bound ||
      high_bound != data->norm_high_bound) {
    data->process = process;
    data->norm_low_bound  = low_bound;
    data->norm_high_bound = high_bound;
    data->processing_data_update = V4L2PROCESSING_UPDATE_RATE;
  }

  if (data->processing_data_update == V4L2PROCESSING_UPDATE_RATE) {
    data->processing_data_update = 0;
    v4lprocessing_update_processing_data(data, fmt->fmt.pix.pixelformat, buf, fmt->fmt.pix.width, fmt->fmt.pix.height);
  } else
    data->processing_data_update++;

  switch (data->process) {
    case V4L2PROCESSING_PROCESS_BAYER_NORMALIZE:
      bayer_normalize(buf, fmt->fmt.pix.width, fmt->fmt.pix.height, data);
      break;

    case V4L2PROCESSING_PROCESS_BAYER_WHITEBALANCE:
      bayer_whitebalance(buf, fmt->fmt.pix.width, fmt->fmt.pix.height, fmt->fmt.pix.pixelformat, data);
      break;

    case V4L2PROCESSING_PROCESS_BAYER_NORMALIZE_WHITEBALANCE:
      bayer_normalize_whitebalance(buf, fmt->fmt.pix.width, fmt->fmt.pix.height, fmt->fmt.pix.pixelformat,
	data);
      break;

    case V4L2PROCESSING_PROCESS_RGB_NORMALIZE:
      rgb_normalize(buf, fmt->fmt.pix.width, fmt->fmt.pix.height, data);
      break;

    case V4L2PROCESSING_PROCESS_RGB_WHITEBALANCE:
      rgb_whitebalance(buf, fmt->fmt.pix.width, fmt->fmt.pix.height, data);
      break;

    case V4L2PROCESSING_PROCESS_RGB_NORMALIZE_WHITEBALANCE:
      rgb_normalize_whitebalance(buf, fmt->fmt.pix.width, fmt->fmt.pix.height, data);
      break;
  }
  data->do_process = 0;
}
