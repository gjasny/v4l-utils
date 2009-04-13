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

#define V4L2PROCESSING_PROCESS_NONE                              0x00
#define V4L2PROCESSING_PROCESS_NORMALIZE                         0x01
#define V4L2PROCESSING_PROCESS_WHITEBALANCE                      0x02
#define V4L2PROCESSING_PROCESS_NORMALIZE_WHITEBALANCE            0x03
#define V4L2PROCESSING_PROCESS_RGB_NORMALIZE                     0x01
#define V4L2PROCESSING_PROCESS_RGB_WHITEBALANCE                  0x02
#define V4L2PROCESSING_PROCESS_RGB_NORMALIZE_WHITEBALANCE        0x03
#define V4L2PROCESSING_PROCESS_BAYER_NORMALIZE                   0x11
#define V4L2PROCESSING_PROCESS_BAYER_WHITEBALANCE                0x12
#define V4L2PROCESSING_PROCESS_BAYER_NORMALIZE_WHITEBALANCE      0x13

#define V4L2PROCESSING_UPDATE_RATE                               10

struct v4lprocessing_data {
  struct v4lcontrol_data *control;
  int do_process;
  /* Provides the current type of processing */
  int process;
  int norm_low_bound;
  int norm_high_bound;
  /* Counts the number of processed frames until a
     V4L2PROCESSING_UPDATE_RATE overflow happens */
  int processing_data_update;
  /* Multiplication factors and offsets from the analyse functions */
  int comp1;
  int comp2;
  int comp3;
  int comp4;
  int offset1;
  int offset2;
};

/* Processing Bayer */
void bayer_normalize_analyse(unsigned char *src_buffer, int width, int height,
  struct v4lprocessing_data *data);
void bayer_whitebalance_analyse(unsigned char *src_buffer, int width,
  int height, unsigned int pix_fmt,
  struct v4lprocessing_data *data);
void bayer_normalize_whitebalance_analyse(unsigned char *src_buffer,
  int width, int height, unsigned int pix_fmt,
  struct v4lprocessing_data *data);
void bayer_normalize(unsigned char *src_buffer, int width, int height,
  struct v4lprocessing_data *data);
void bayer_whitebalance(unsigned char *src_buffer, int width, int height,
  unsigned int pix_fmt, struct v4lprocessing_data *data);
void bayer_normalize_whitebalance(unsigned char *src_buffer, int width,
  int height, unsigned int pix_fmt,
  struct v4lprocessing_data *data);

/* Processing RGB */
void rgb_normalize_analyse(unsigned char *src_buffer, int width, int height,
  struct v4lprocessing_data *data);
void rgb_whitebalance_analyse(unsigned char *src_buffer, int width, int height,
  struct v4lprocessing_data *data);
void rgb_normalize_whitebalance_analyse(unsigned char *src_buffer,
  int width, int height, struct v4lprocessing_data *data);
void rgb_normalize(unsigned char *src_buffer, int width, int height,
  struct v4lprocessing_data *data);
void rgb_whitebalance(unsigned char *src_buffer, int width, int height,
  struct v4lprocessing_data *data);
void rgb_normalize_whitebalance(unsigned char *src_buffer, int width,
  int height, struct v4lprocessing_data *data);

#endif
