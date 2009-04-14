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

#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syscall.h>
#include "libv4lcontrol.h"
#include "libv4lcontrol-priv.h"

/* These headers are not needed by us, but by linux/videodev2.h,
   which is broken on some systems and doesn't include them itself :( */
#include <sys/time.h>
#include <linux/types.h>
#include <linux/ioctl.h>
/* end broken header workaround includes */
#include <linux/videodev2.h>

#define ARRAY_SIZE(x) ((int)sizeof(x)/(int)sizeof((x)[0]))

/* Workaround these potentially missing from videodev2.h */
#ifndef V4L2_IN_ST_HFLIP
#define V4L2_IN_ST_HFLIP       0x00000010 /* Frames are flipped horizontally */
#endif

#ifndef V4L2_IN_ST_VFLIP
#define V4L2_IN_ST_VFLIP       0x00000020 /* Frames are flipped vertically */
#endif


/* List of cams which need special flags */
static const struct v4lcontrol_flags_info v4lcontrol_flags[] = {
/* First: Upside down devices */
  /* Philips SPC200NC */
  { 0x0471, 0x0325, 0, V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED, 0 },
  /* Philips SPC300NC */
  { 0x0471, 0x0326, 0, V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED, 0 },
  /* Philips SPC210NC */
  { 0x0471, 0x032d, 0, V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED, 0 },
  /* Genius E-M 112 */
  { 0x093a, 0x2476, 0, V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED, 0 },
/* Second: devices which can benifit from software video processing */
  /* Pac207 based devices */
  { 0x041e, 0x4028, 0,    0, V4LCONTROL_WANTS_WB },
  { 0x093a, 0x2460, 0x1f, 0, V4LCONTROL_WANTS_WB },
  { 0x145f, 0x013a, 0,    0, V4LCONTROL_WANTS_WB },
  { 0x2001, 0xf115, 0,    0, V4LCONTROL_WANTS_WB },
  /* Pac7302 based devices */
  { 0x093a, 0x2620, 0x0f, V4LCONTROL_ROTATED_90_JPEG, V4LCONTROL_WANTS_WB },
};

static void v4lcontrol_init_flags(struct v4lcontrol_data *data)
{
  struct stat st;
  FILE *f;
  char sysfs_name[512];
  unsigned short vendor_id = 0;
  unsigned short product_id = 0;
  int i, minor;
  char c, *s, buf[32];
  struct v4l2_input input;

  if ((syscall(SYS_ioctl, data->fd, VIDIOC_G_INPUT, &input.index) == 0) &&
      (syscall(SYS_ioctl, data->fd, VIDIOC_ENUMINPUT, &input) == 0)) {
    if (input.status & V4L2_IN_ST_HFLIP)
      data->flags |= V4LCONTROL_HFLIPPED;
    if (input.status & V4L2_IN_ST_VFLIP)
      data->flags |= V4LCONTROL_VFLIPPED;
  }

  if (fstat(data->fd, &st) || !S_ISCHR(st.st_mode)) {
    return; /* Should never happen */
  }

  /* <Sigh> find ourselve in sysfs */
  for (i = 0; i < 256; i++) {
    snprintf(sysfs_name, sizeof(sysfs_name),
	     "/sys/class/video4linux/video%d/dev", i);
    f = fopen(sysfs_name, "r");
    if (!f)
      continue;

    s = fgets(buf, sizeof(buf), f);
    fclose(f);

    if (s && sscanf(buf, "%*d:%d%c", &minor, &c) == 2 && c == '\n' &&
	minor == minor(st.st_rdev))
      break;
  }
  if (i == 256)
    return; /* Not found, sysfs not mounted? */

  /* Get vendor and product ID */
  snprintf(sysfs_name, sizeof(sysfs_name),
	   "/sys/class/video4linux/video%d/device/modalias", i);
  f = fopen(sysfs_name, "r");
  if (f) {
    s = fgets(buf, sizeof(buf), f);
    fclose(f);

    if (!s ||
	sscanf(s, "usb:v%4hxp%4hx%c", &vendor_id, &product_id, &c) != 3 ||
	c != 'd')
      return; /* Not an USB device */
  } else {
    /* Try again assuming the device link points to the usb
       device instead of the usb interface (bug in older versions
       of gspca) */

    /* Get product ID */
    snprintf(sysfs_name, sizeof(sysfs_name),
	     "/sys/class/video4linux/video%d/device/idVendor", i);
    f = fopen(sysfs_name, "r");
    if (!f)
      return; /* Not an USB device (or no sysfs) */

    s = fgets(buf, sizeof(buf), f);
    fclose(f);

    if (!s || sscanf(s, "%04hx%c", &vendor_id, &c) != 2 || c != '\n')
      return; /* Should never happen */

    /* Get product ID */
    snprintf(sysfs_name, sizeof(sysfs_name),
	     "/sys/class/video4linux/video%d/device/idProduct", i);
    f = fopen(sysfs_name, "r");
    if (!f)
      return; /* Should never happen */

    s = fgets(buf, sizeof(buf), f);
    fclose(f);

    if (!s || sscanf(s, "%04hx%c", &product_id, &c) != 2 || c != '\n')
      return; /* Should never happen */
  }

  for (i = 0; i < ARRAY_SIZE(v4lcontrol_flags); i++)
    if (v4lcontrol_flags[i].vendor_id == vendor_id &&
	v4lcontrol_flags[i].product_id ==
	  (product_id & ~v4lcontrol_flags[i].product_mask)) {
      data->flags |= v4lcontrol_flags[i].flags;
      data->controls = v4lcontrol_flags[i].controls;
      break;
    }
}

struct v4lcontrol_data *v4lcontrol_create(int fd)
{
  int shm_fd;
  int init = 0;
  char *s, shm_name[256];
  struct v4l2_capability cap;

  struct v4lcontrol_data *data = calloc(1, sizeof(struct v4lcontrol_data));

  if (!data)
    return NULL;

  syscall(SYS_ioctl, fd, VIDIOC_QUERYCAP, &cap);
  snprintf(shm_name, 256, "/%s:%s", cap.bus_info, cap.card);

  /* Open the shared memory object identified by shm_name */
  if ((shm_fd = shm_open(shm_name, (O_CREAT | O_EXCL | O_RDWR),
			 (S_IREAD | S_IWRITE))) >= 0)
    init = 1;
  else if ((shm_fd = shm_open(shm_name, O_RDWR, (S_IREAD | S_IWRITE))) < 0)
    goto error;

  /* Set the shared memory size */
  ftruncate(shm_fd, V4LCONTROL_SHM_SIZE);

  /* Retreive a pointer to the shm object */
  data->shm_values = mmap(NULL, V4LCONTROL_SHM_SIZE, (PROT_READ | PROT_WRITE),
			  MAP_SHARED, shm_fd, 0);
  close(shm_fd);

  if (data->shm_values == MAP_FAILED)
    goto error;

  if (init) {
    /* Initialize the new shm object we created */
    memset(data->shm_values, 0, sizeof(V4LCONTROL_SHM_SIZE));
    data->shm_values[V4LCONTROL_WHITEBALANCE] = 1;
    data->shm_values[V4LCONTROL_NORM_HIGH_BOUND] = 255;
  }

  data->fd = fd;

  v4lcontrol_init_flags(data);

  /* Allow overriding through environment */
  if ((s = getenv("LIBV4LCONTROL_FLAGS")))
    data->flags = strtol(s, NULL, 0);

  if ((s = getenv("LIBV4LCONTROL_CONTROLS")))
    data->controls = strtol(s, NULL, 0);

  return data;

error:
  free(data);
  return NULL;
}

void v4lcontrol_destroy(struct v4lcontrol_data *data)
{
  munmap(data->shm_values, V4LCONTROL_SHM_SIZE);
  free(data);
}

/* FIXME get better CID's for normalize */
struct v4l2_queryctrl fake_controls[V4LCONTROL_COUNT] = {
{
  .id = V4L2_CID_AUTO_WHITE_BALANCE,
  .type = V4L2_CTRL_TYPE_BOOLEAN,
  .name =  "Whitebalance",
  .minimum = 0,
  .maximum = 1,
  .step = 1,
  .default_value = 1,
  .flags = 0
},
{
  .id = V4L2_CID_DO_WHITE_BALANCE,
  .type = V4L2_CTRL_TYPE_BOOLEAN,
  .name =  "Normalize",
  .minimum = 0,
  .maximum = 1,
  .step = 1,
  .default_value = 0,
  .flags = 0
},
{
  .id = V4L2_CID_BLACK_LEVEL,
  .type = V4L2_CTRL_TYPE_INTEGER,
  .name =  "Normalize: low bound",
  .minimum = 0,
  .maximum = 127,
  .step = 1,
  .default_value = 0,
  .flags = 0
},
{
  .id = V4L2_CID_WHITENESS,
  .type = V4L2_CTRL_TYPE_INTEGER,
  .name =  "Normalize: high bound",
  .minimum = 128,
  .maximum = 255,
  .step = 1,
  .default_value = 255,
  .flags = 0
},
};

int v4lcontrol_vidioc_queryctrl(struct v4lcontrol_data *data, void *arg)
{
  int i;
  struct v4l2_queryctrl *ctrl = arg;

  for (i = 0; i < V4LCONTROL_COUNT; i++)
    if ((data->controls & (1 << i)) &&
	ctrl->id == fake_controls[i].id) {
      memcpy(ctrl, &fake_controls[i], sizeof(struct v4l2_queryctrl));
      return 0;
    }

  return syscall(SYS_ioctl, data->fd, VIDIOC_QUERYCTRL, arg);
}

int v4lcontrol_vidioc_g_ctrl(struct v4lcontrol_data *data, void *arg)
{
  int i;
  struct v4l2_control *ctrl = arg;

  for (i = 0; i < V4LCONTROL_COUNT; i++)
    if ((data->controls & (1 << i)) &&
	ctrl->id == fake_controls[i].id) {
      ctrl->value = data->shm_values[i];
      return 0;
    }

  return syscall(SYS_ioctl, data->fd, VIDIOC_G_CTRL, arg);
}

int v4lcontrol_vidioc_s_ctrl(struct v4lcontrol_data *data, void *arg)
{
  int i;
  struct v4l2_control *ctrl = arg;

  for (i = 0; i < V4LCONTROL_COUNT; i++)
    if ((data->controls & (1 << i)) &&
	ctrl->id == fake_controls[i].id) {
      if (ctrl->value > fake_controls[i].maximum ||
	  ctrl->value < fake_controls[i].minimum) {
	errno = EINVAL;
	return -1;
      }

      data->shm_values[i] = ctrl->value;
      return 0;
    }

  return syscall(SYS_ioctl, data->fd, VIDIOC_S_CTRL, arg);
}

int v4lcontrol_get_flags(struct v4lcontrol_data *data)
{
  return data->flags;
}

int v4lcontrol_get_ctrl(struct v4lcontrol_data *data, int ctrl)
{
  if (data->controls & (1 << ctrl))
    return data->shm_values[ctrl];

  return 0;
}

/* See the comment about this in libv4lconvert.h */
int v4lcontrol_needs_conversion(struct v4lcontrol_data *data) {
  return data->flags || data->controls;
}
