/*
#             (C) 2008 Hans de Goede <hdegoede@redhat.com>

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

#ifndef __LIBV4L1_PRIV_H
#define __LIBV4L1_PRIV_H

#include <stdio.h>
#include <pthread.h>

#include "../libv4lconvert/libv4lsyscall-priv.h"

#define V4L1_MAX_DEVICES 16
#define V4L1_NO_FRAMES 4
#define V4L1_FRAME_BUF_SIZE (4096 * 4096)

extern FILE *v4l1_log_file;

#define V4L1_LOG_ERR(...) \
  do { \
    if (v4l1_log_file) { \
      fprintf(v4l1_log_file, "libv4l1: error " __VA_ARGS__); \
      fflush(v4l1_log_file); \
    } else \
      fprintf(stderr, "libv4l1: error " __VA_ARGS__); \
  } while(0)

#define V4L1_LOG_WARN(...) \
  do { \
    if (v4l1_log_file) { \
      fprintf(v4l1_log_file, "libv4l1: warning " __VA_ARGS__); \
      fflush(v4l1_log_file); \
    } else \
      fprintf(stderr, "libv4l1: warning " __VA_ARGS__); \
  } while(0)

#define V4L1_LOG(...) \
  do { \
    if (v4l1_log_file) { \
      fprintf(v4l1_log_file, "libv4l1: " __VA_ARGS__); \
      fflush(v4l1_log_file); \
    } \
  } while(0)

struct v4l1_dev_info {
  int fd;
  int flags;
  int open_count;
  int v4l1_frame_buf_map_count;
  pthread_mutex_t stream_lock;
  unsigned int depth;
  unsigned int v4l1_pal;    /* VIDEO_PALETTE */
  unsigned int v4l2_pixfmt; /* V4L2_PIX_FMT */
  unsigned int min_width, min_height, max_width, max_height;
  unsigned int width, height;
  unsigned char *v4l1_frame_pointer;
};

/* From log.c */
void v4l1_log_ioctl(unsigned long int request, void *arg, int result);

#endif
