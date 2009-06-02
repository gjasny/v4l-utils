/*
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
#include "../libv4lsyscall-priv.h"

static int autogain_active(struct v4lprocessing_data *data) {
  return v4lcontrol_get_ctrl(data->control, V4LCONTROL_AUTOGAIN);
}

/* auto gain and exposure algorithm based on the knee algorithm described here:
   http://ytse.tricolour.net/docs/LowLightOptimization.html */
static int autogain_calculate_lookup_tables(
  struct v4lprocessing_data *data,
  unsigned char *buf, const struct v4l2_format *fmt)
{
  int x, y, target, steps, avg_lum = 0;
  int gain, exposure, orig_gain, orig_exposure;
  struct v4l2_control ctrl;
  struct v4l2_queryctrl gainctrl, expoctrl;
  const int deadzone = 8;

  ctrl.id = V4L2_CID_EXPOSURE;
  expoctrl.id = V4L2_CID_EXPOSURE;
  if (SYS_IOCTL(data->fd, VIDIOC_QUERYCTRL, &expoctrl) ||
      SYS_IOCTL(data->fd, VIDIOC_G_CTRL, &ctrl))
    return 0;
  exposure = orig_exposure = ctrl.value;

  ctrl.id = V4L2_CID_GAIN;
  gainctrl.id = V4L2_CID_GAIN;
  if (SYS_IOCTL(data->fd, VIDIOC_QUERYCTRL, &gainctrl) ||
      SYS_IOCTL(data->fd, VIDIOC_G_CTRL, &ctrl))
    return 0;
  gain = orig_gain = ctrl.value;

  switch (fmt->fmt.pix.pixelformat) {
    case V4L2_PIX_FMT_SGBRG8:
    case V4L2_PIX_FMT_SGRBG8:
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_SRGGB8:
      buf += fmt->fmt.pix.height * fmt->fmt.pix.bytesperline / 4 +
	     fmt->fmt.pix.width / 4;

      for (y = 0; y < fmt->fmt.pix.height / 2; y++) {
	for (x = 0; x < fmt->fmt.pix.width / 2; x++) {
	  avg_lum += *buf++;
	}
	buf += fmt->fmt.pix.bytesperline - fmt->fmt.pix.width / 2;
      }
      avg_lum /= fmt->fmt.pix.height * fmt->fmt.pix.width / 4;
      break;

    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_BGR24:
      buf += fmt->fmt.pix.height * fmt->fmt.pix.bytesperline / 4 +
	     fmt->fmt.pix.width * 3 / 4;

      for (y = 0; y < fmt->fmt.pix.height / 2; y++) {
	for (x = 0; x < fmt->fmt.pix.width / 2; x++) {
	  avg_lum += *buf++;
	  avg_lum += *buf++;
	  avg_lum += *buf++;
	}
	buf += fmt->fmt.pix.bytesperline - fmt->fmt.pix.width * 3 / 2;
      }
      avg_lum /= fmt->fmt.pix.height * fmt->fmt.pix.width * 3 / 4;
      break;
  }

  /* If we are off a multiple of deadzone, do multiple steps to reach the
     desired lumination fast (with the risc of a slight overshoot) */
  target = v4lcontrol_get_ctrl(data->control, V4LCONTROL_AUTOGAIN_TARGET);
  steps = abs(target - avg_lum) / deadzone;

  for (x = 0; x < steps; x++) {
    if (avg_lum > target) {
      if (exposure > expoctrl.default_value)
	exposure--;
      else if (gain > gainctrl.default_value)
	gain--;
      else if (exposure > expoctrl.minimum)
	exposure--;
      else if (gain > gainctrl.minimum)
	gain--;
      else
	break;
    } else {
      if (gain < gainctrl.default_value)
	gain++;
      else if (exposure < expoctrl.default_value)
	exposure++;
      else if (gain < gainctrl.maximum)
	gain++;
      else if (exposure < expoctrl.maximum)
	exposure++;
      else
	break;
    }
  }

  if (gain != orig_gain) {
    ctrl.id = V4L2_CID_GAIN;
    ctrl.value = gain;
    SYS_IOCTL(data->fd, VIDIOC_S_CTRL, &ctrl);
  }
  if (exposure != orig_exposure) {
    ctrl.id = V4L2_CID_EXPOSURE;
    ctrl.value = exposure;
    SYS_IOCTL(data->fd, VIDIOC_S_CTRL, &ctrl);
  }

  return 0;
}

struct v4lprocessing_filter autogain_filter = {
  autogain_active, autogain_calculate_lookup_tables };
