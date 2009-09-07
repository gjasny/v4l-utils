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

#ifndef __LIBV4LPROCESSING_PRIV_H
#define __LIBV4LPROCESSING_PRIV_H

#include "../control/libv4lcontrol.h"
#include "../libv4lsyscall-priv.h"

#define V4L2PROCESSING_UPDATE_RATE                               10

struct v4lprocessing_data {
  struct v4lcontrol_data *control;
  int fd;
  int do_process;
  int controls_changed;
  /* True if any of the lookup tables does not contain
     linear 0-255 */
  int lookup_table_active;
  /* Counts the number of processed frames until a
     V4L2PROCESSING_UPDATE_RATE overflow happens */
  int lookup_table_update_counter;
  /* RGB/BGR lookup tables */
  unsigned char comp1[256];
  unsigned char green[256];
  unsigned char comp2[256];
  /* Filter private data for filters which need it */
  /* whitebalance.c data */
  int green_avg;
  int comp1_avg;
  int comp2_avg;
  /* gamma.c data */
  int last_gamma;
  unsigned char gamma_table[256];
};

struct v4lprocessing_filter {
  /* Returns 1 if the filter is active */
  int (*active)(struct v4lprocessing_data *data);
  /* Returns 1 if any of the lookup tables was changed */
  int (*calculate_lookup_tables)(struct v4lprocessing_data *data,
    unsigned char *buf, const struct v4l2_format *fmt);
};

extern struct v4lprocessing_filter whitebalance_filter;
extern struct v4lprocessing_filter autogain_filter;
extern struct v4lprocessing_filter gamma_filter;

#endif
