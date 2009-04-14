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
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __LIBV4LCONTROL_PRIV_H
#define __LIBV4LCONTROL_PRIV_H

#define V4LCONTROL_SHM_SIZE 4096

#define V4LCONTROL_WANTS_WB    (1 << V4LCONTROL_WHITEBALANCE)
#define V4LCONTROL_WANTS_NORM  ((1 << V4LCONTROL_NORMALIZE) | \
				(1 << V4LCONTROL_NORM_LOW_BOUND) | \
				(1 << V4LCONTROL_NORM_HIGH_BOUND))

struct v4lcontrol_data {
  int fd;                   /* Device fd */
  int flags;                /* Special flags for this device */
  int controls;             /* Which controls to use for this device */
  unsigned int *shm_values; /* shared memory control value store */
};

struct v4lcontrol_flags_info {
  unsigned short vendor_id;
  unsigned short product_id;
  unsigned short product_mask;
/* We could also use the USB manufacturer and product strings some devices have
  const char *manufacturer;
  const char *product; */
  int flags;
  int controls;
};

#endif
