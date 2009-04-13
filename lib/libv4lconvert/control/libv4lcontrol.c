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

static void v4lcontrol_init(struct v4lcontrol_data *data, int first_time)
{
  /* FIXME: Temporary spoof future communication with driver by always enabling
     the fake controls */
  data->controls = (1 << V4LCONTROL_WHITEBALANCE) |
    (1 << V4LCONTROL_NORMALIZE) | (1 << V4LCONTROL_NORM_LOW_BOUND) |
    (1 << V4LCONTROL_NORM_HIGH_BOUND);

  if (first_time) {
    /* Initialize the new shm object when created */
    memset(data->shm_values, 0, sizeof(V4LCONTROL_SHM_SIZE));
    data->shm_values[V4LCONTROL_WHITEBALANCE] = 1;
    data->shm_values[V4LCONTROL_NORM_HIGH_BOUND] = 255;
  }
}

struct v4lcontrol_data *v4lcontrol_create(int fd)
{
  int shm_fd;
  int init = 0;
  char shm_name[256];
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

  v4lcontrol_init(data, init); /* Set the driver defined fake controls */

  data->fd = fd;

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

int v4lcontrol_get_ctrl(struct v4lcontrol_data *data, int ctrl)
{
  if (data->controls & (1 << ctrl))
    return data->shm_values[ctrl];

  return 0;
}
