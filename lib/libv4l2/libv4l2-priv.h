/*
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
# Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#ifndef __LIBV4L2_PRIV_H
#define __LIBV4L2_PRIV_H

#include <stdio.h>
#include <pthread.h>
#include <libv4lconvert.h> /* includes videodev2.h for us */

#include "../libv4lconvert/libv4lsyscall-priv.h"

#define V4L2_MAX_DEVICES 16
/* Warning when making this larger the frame_queued and frame_mapped members of
   the v4l2_dev_info struct can no longer be a bitfield, so the code needs to
   be adjusted! */
#define V4L2_MAX_NO_FRAMES 32
#define V4L2_DEFAULT_NREADBUFFERS 4
#define V4L2_IGNORE_FIRST_FRAME_ERRORS 3
#define V4L2_DEFAULT_FPS 30

#define V4L2_LOG_ERR(...) 			\
	do { 					\
		if (v4l2_log_file) { 		\
			fprintf(v4l2_log_file, "libv4l2: error " __VA_ARGS__); \
			fflush(v4l2_log_file); 	\
		} else 				\
		fprintf(stderr, "libv4l2: error " __VA_ARGS__); \
	} while (0)

#define V4L2_PERROR(format, ...)		\
	do { 					\
		if (errno == ENODEV) {		\
			devices[index].gone = 1;\
			break;			\
		}				\
		V4L2_LOG_ERR(format ": %s\n", ##__VA_ARGS__, strerror(errno)); \
	} while (0)

#define V4L2_LOG_WARN(...) 			\
	do { 					\
		if (v4l2_log_file) { 		\
			fprintf(v4l2_log_file, "libv4l2: warning " __VA_ARGS__); \
			fflush(v4l2_log_file); 	\
		} else 				\
		fprintf(stderr, "libv4l2: warning " __VA_ARGS__); \
	} while (0)

#define V4L2_LOG(...) 				\
	do { 					\
		if (v4l2_log_file) { 		\
			fprintf(v4l2_log_file, "libv4l2: " __VA_ARGS__); \
			fflush(v4l2_log_file); 	\
		} 				\
	} while (0)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

struct v4l2_dev_info {
	int fd;
	int flags;
	int open_count;
	int gone; /* Set to 1 when a device is detached (ENODEV encountered) */
	long page_size;
	/* actual format of the cam */
	struct v4l2_format src_fmt;
	/* fmt as seen by the application (iow after conversion) */
	struct v4l2_format dest_fmt;
	pthread_mutex_t stream_lock;
	unsigned int no_frames;
	unsigned int nreadbuffers;
	int fps;
	int first_frame;
	struct v4lconvert_data *convert;
	unsigned char *convert_mmap_buf;
	size_t convert_mmap_buf_size;
	size_t convert_mmap_frame_size;
	/* Frame bookkeeping is only done when in read or mmap-conversion mode */
	unsigned char *frame_pointers[V4L2_MAX_NO_FRAMES];
	int frame_sizes[V4L2_MAX_NO_FRAMES];
	int frame_queued; /* 1 status bit per frame */
	int frame_info_generation;
	/* mapping tracking of our fake (converting mmap) frame buffers */
	unsigned char frame_map_count[V4L2_MAX_NO_FRAMES];
	/* buffer when doing conversion and using read() for read() */
	int readbuf_size;
	unsigned char *readbuf;
	/* plugin info */
	void *plugin_library;
	void *dev_ops_priv;
	const struct libv4l_dev_ops *dev_ops;
};

/* From v4l2-plugin.c */
void v4l2_plugin_init(int fd, void **plugin_lib_ret, void **plugin_priv_ret,
		      const struct libv4l_dev_ops **dev_ops_ret);
void v4l2_plugin_cleanup(void *plugin_lib, void *plugin_priv,
			 const struct libv4l_dev_ops *dev_ops);

/* From log.c */
extern const char *v4l2_ioctls[];
void v4l2_log_ioctl(unsigned long int request, void *arg, int result);

#endif
