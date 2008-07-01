/*
# open/close/ioctl/mmap/munmap library call wrapper doing v4l1 api emulation
# for v4l2 devices

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

#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <libv4l1.h>

/* Check that open/read/mmap is not a define */
#if defined open || defined read || defined mmap
#error open/read/mmap is a prepocessor macro !!
#endif

int open (const char *file, int oflag, ...)
{
  int fd;

  if (oflag & O_CREAT)
  {
    va_list ap;
    mode_t mode;

    va_start (ap, oflag);
    mode = va_arg (ap, mode_t);

    fd = v4l1_open(file, oflag, mode);

    va_end(ap);
  } else
    fd = v4l1_open(file, oflag);

  return fd;
}

int close(int fd) {
  return v4l1_close(fd);
}

int dup(int fd)
{
  return v4l1_dup(fd);
}

int ioctl (int fd, unsigned long int request, ...)
{
  void *arg;
  va_list ap;

  va_start (ap, request);
  arg = va_arg (ap, void *);
  va_end (ap);

  return v4l1_ioctl (fd, request, arg);
}

ssize_t read(int fd, void* buffer, size_t n)
{
  return v4l1_read (fd, buffer, n);
}

void mmap(void *start, size_t length, int prot, int flags, int fd,
  off_t offset)
{
  return v4l1_mmap(start, length, prot, flags, fd, offset);
}

int munmap(void *start, size_t length)
{
  return v4l1_munmap(start, length);
}
