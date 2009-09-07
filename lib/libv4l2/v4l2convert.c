/*
# open/close/ioctl/mmap/munmap library call wrapper doing format conversion
# for v4l2 applications which want to be able to simply capture bgr24 / yuv420
# from v4l2 devices with more exotic frame formats.

#             (C) 2008 Hans de Goede <hdegoede@redhat.com>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#define _LARGEFILE64_SOURCE 1

#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "../libv4lconvert/libv4lsyscall-priv.h"
#include <linux/videodev2.h>
#include <libv4l2.h>

/* Check that open/read/mmap is not a define */
#if defined open || defined read || defined mmap
#error open/read/mmap is a prepocessor macro !!
#endif

#if __GNUC__ >= 4
#define LIBV4L_PUBLIC __attribute__ ((visibility("default")))
#else
#define LIBV4L_PUBLIC
#endif

LIBV4L_PUBLIC int open (const char *file, int oflag, ...)
{
  int fd;
  struct v4l2_capability cap;
  int v4l_device = 0;

  /* check if we're opening a video4linux2 device */
  if (!strncmp(file, "/dev/video", 10) || !strncmp(file, "/dev/v4l/", 9)) {
    /* Some apps open the device read only, but we need rw rights as the
       buffers *MUST* be mapped rw */
    oflag = (oflag & ~O_ACCMODE) | O_RDWR;
    v4l_device = 1;
  }

  /* original open code */
  if (oflag & O_CREAT)
  {
    va_list ap;
    mode_t mode;

    va_start (ap, oflag);
    mode = va_arg (ap, mode_t);

    fd = SYS_OPEN(file, oflag, mode);

    va_end(ap);
  } else
    fd = SYS_OPEN(file, oflag, 0);
  /* end of original open code */

  if (fd == -1 || !v4l_device)
    return fd;

  /* check that this is an v4l2 device, libv4l2 only supports v4l2 devices */
  if (SYS_IOCTL(fd, VIDIOC_QUERYCAP, &cap))
    return fd;

  /* libv4l2 only adds functionality to capture capable devices */
  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    return fd;

  /* Try to Register with libv4l2 (in case of failure pass the fd to the
     application as is) */
  v4l2_fd_open(fd, 0);

  return fd;
}

#ifdef linux
LIBV4L_PUBLIC int open64(const char *file, int oflag, ...)
{
  int fd;

  /* original open code */
  if (oflag & O_CREAT)
  {
    va_list ap;
    mode_t mode;

    va_start (ap, oflag);
    mode = va_arg (ap, mode_t);

    fd = open(file, oflag | O_LARGEFILE, mode);

    va_end(ap);
  } else
    fd = open(file, oflag | O_LARGEFILE);
  /* end of original open code */

  return fd;
}
#endif

LIBV4L_PUBLIC int close(int fd)
{
  return v4l2_close(fd);
}

LIBV4L_PUBLIC int dup(int fd)
{
  return v4l2_dup(fd);
}

LIBV4L_PUBLIC int ioctl (int fd, unsigned long int request, ...)
{
  void *arg;
  va_list ap;

  va_start (ap, request);
  arg = va_arg (ap, void *);
  va_end (ap);

  return v4l2_ioctl (fd, request, arg);
}

LIBV4L_PUBLIC ssize_t read (int fd, void* buffer, size_t n)
{
  return v4l2_read (fd, buffer, n);
}

LIBV4L_PUBLIC void *mmap(void *start, size_t length, int prot, int flags, int fd,
  __off_t offset)
{
  return v4l2_mmap(start, length, prot, flags, fd, offset);
}

#ifdef linux
LIBV4L_PUBLIC void *mmap64(void *start, size_t length, int prot, int flags, int fd,
  __off64_t offset)
{
  return v4l2_mmap(start, length, prot, flags, fd, offset);
}
#endif

LIBV4L_PUBLIC int munmap(void *start, size_t length)
{
  return v4l2_munmap(start, length);
}

