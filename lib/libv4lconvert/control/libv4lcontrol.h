/*
#             (C) 2008-2009 Elmar Kleijn <elmar_kleijn@hotmail.com>
#             (C) 2008-2009 Sjoerd Piepenbrink <need4weed@gmail.com>
#             (C) 2008-2009 Radjnies Bhansingh <radjnies@gmail.com>
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

#ifndef __LIBV4LCONTROL_H
#define __LIBV4LCONTROL_H

/* Flags */
#define V4LCONTROL_HFLIPPED              0x01
#define V4LCONTROL_VFLIPPED              0x02
#define V4LCONTROL_ROTATED_90_JPEG       0x04

/* Controls */
enum { V4LCONTROL_WHITEBALANCE, V4LCONTROL_NORMALIZE,
  V4LCONTROL_NORM_LOW_BOUND, V4LCONTROL_NORM_HIGH_BOUND, V4LCONTROL_COUNT };

struct v4lcontrol_data;

struct v4lcontrol_data* v4lcontrol_create(int fd);
void v4lcontrol_destroy(struct v4lcontrol_data *data);

/* Functions used by v4lprocessing to get the control state */
int v4lcontrol_get_flags(struct v4lcontrol_data *data);
int v4lcontrol_get_ctrl(struct v4lcontrol_data *data, int ctrl);
/* Check if we must go through the conversion path (and thus alloc conversion
   buffers, etc. in libv4l2). Note this always return 1 if we *may* need
   rotate90 / flipping / processing, as if we actually need this may change
   on the fly while the stream is active. */
int v4lcontrol_needs_conversion(struct v4lcontrol_data *data);

/* Functions used by v4lconvert to pass vidioc calls from libv4l2 */
int v4lcontrol_vidioc_queryctrl(struct v4lcontrol_data *data, void *arg);
int v4lcontrol_vidioc_g_ctrl(struct v4lcontrol_data *data, void *arg);
int v4lcontrol_vidioc_s_ctrl(struct v4lcontrol_data *data, void *arg);

#endif
