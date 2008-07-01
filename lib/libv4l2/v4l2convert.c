/*
# open/close/ioctl/mmap/munmap library call wrapper doing format conversion
# for v4l2 applications which want to be able to simply capture bgr24 / yuv420
# from v4l2 devices with more exotic frame formats.

#             (C) 2008 Hans de Goede <j.w.r.degoede@hhs.nl>

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

#include <stdarg.h>
#include <stdlib.h>
#include <syscall.h>
#include <fcntl.h>
#include <string.h>
/* These headers are not needed by us, but by linux/videodev2.h,
   which is broken on some systems and doesn't include them itself :( */
#include <sys/time.h>
#include <asm/types.h>
#include <linux/ioctl.h>
/* end broken header workaround includes */
#include <linux/videodev2.h>
#include <libv4l2.h>

/* Check that open/read/mmap is not a define */
#if defined open || defined read || defined mmap
#error open/read/mmap is a prepocessor macro !!
#endif

int open (const char *file, int oflag, ...)
{
  int fd;
  struct v4l2_capability cap;

  /* original open code */
  if (oflag & O_CREAT)
  {
    va_list ap;
    mode_t mode;

    va_start (ap, oflag);
    mode = va_arg (ap, mode_t);

    fd = syscall(SYS_open, file, oflag, mode);

    va_end(ap);
  } else
    fd = syscall(SYS_open, file, oflag);
  /* end of original open code */

  if (fd == -1)
    return fd;

  /* check if we're opening a video4linux2 device */
  if (strncmp(file, "/dev/video", 10))
    return fd;

  /* check that this is an v4l2 device, libv4l2 only supports v4l2 devices */
  if (syscall(SYS_ioctl, fd, VIDIOC_QUERYCAP, &cap))
    return fd;

  /* libv4l2 only adds functionality to capture capable devices */
  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    return fd;

  /* Try to Register with libv4l2 (in case of failure pass the fd to the
     application as is) */
  v4l2_fd_open(fd, V4L2_ENABLE_ENUM_FMT_EMULATION);

  return fd;
}

int close(int fd)
{
  return v4l2_close(fd);
}

int dup(int fd)
{
  return v4l2_dup(fd);
}

int ioctl (int fd, unsigned long int request, ...)
{
  void *arg;
  va_list ap;

  va_start (ap, request);
  arg = va_arg (ap, void *);
  va_end (ap);

  return v4l2_ioctl (fd, request, arg);
}

ssize_t read (int fd, void* buffer, size_t n)
{
  return v4l2_read (fd, buffer, n);
}

void mmap(void *start, size_t length, int prot, int flags, int fd,
  off_t offset)
{
  return v4l2_mmap(start, length, prot, flags, fd, offset);
}

int munmap(void *start, size_t length)
{
  return v4l2_munmap(start, length);
}
