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

#include <math.h>
#include "libv4lprocessing.h"
#include "libv4lprocessing-priv.h"

#define CLIP(color) (unsigned char)(((color)>0xff)?0xff:(((color)<0)?0:(color)))

static int gamma_active(struct v4lprocessing_data *data) {
  int gamma = v4lcontrol_get_ctrl(data->control, V4LCONTROL_GAMMA);

  return gamma && gamma != 1000;
}

static int gamma_calculate_lookup_tables(
  struct v4lprocessing_data *data,
  unsigned char *buf, const struct v4l2_format *fmt)
{
  int i, x, gamma;

  gamma = v4lcontrol_get_ctrl(data->control, V4LCONTROL_GAMMA);

  if (gamma != data->last_gamma) {
    for (i = 0; i < 256; i++) {
      x = powf(i / 255.0, 1000.0 / gamma) * 255;
      data->gamma_table[i] = CLIP(x);
    }
    data->last_gamma = gamma;
  }

  for (i = 0; i < 256; i++) {
    data->comp1[i] = data->gamma_table[data->comp1[i]];
    data->green[i] = data->gamma_table[data->green[i]];
    data->comp2[i] = data->gamma_table[data->comp2[i]];
  }

  return 1;
}

struct v4lprocessing_filter gamma_filter = {
  gamma_active, gamma_calculate_lookup_tables };
