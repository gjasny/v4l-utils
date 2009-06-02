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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __LIBV4LPROCESSING_H
#define __LIBV4LPROCESSING_H

#include "../libv4lsyscall-priv.h"
#include <linux/videodev2.h>

struct v4lprocessing_data;
struct v4lcontrol_data;

struct v4lprocessing_data *v4lprocessing_create(int fd, struct v4lcontrol_data *data);
void v4lprocessing_destroy(struct v4lprocessing_data *data);

/* Prepare to process 1 frame, returns 1 if processing is necesary,
   return 0 if no processing will be done */
int v4lprocessing_pre_processing(struct v4lprocessing_data *data);

/* Do the actual processing, this is a nop if v4lprocessing_pre_processing()
   returned 0, or if called more then 1 time after a single
   v4lprocessing_pre_processing() call. */
void v4lprocessing_processing(struct v4lprocessing_data *data,
  unsigned char *buf, const struct v4l2_format *fmt);

#endif
