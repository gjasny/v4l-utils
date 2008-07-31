/*
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

/* MAKING CHANGES TO THIS FILE??   READ THIS FIRST!!!

   This file implements libv4l2, which offers v4l2_ prefixed versions of
   open/close/etc. The API is 100% the same as directly opening /dev/videoX
   using regular open/close/etc, the big difference is that format conversion
   is done if necessary when capturing. That is if you (try to) set a capture
   format which is not supported by the cam, but is supported by libv4lconvert,
   then the try_fmt / set_fmt will succeed as if the cam supports the format
   and on dqbuf / read the data will be converted for you and returned in
   the request format.

   Important note to people making changes to this file: All functions
   (v4l2_close, v4l2_ioctl, etc.) are designed to function as their regular
   counterpart when they get passed a fd that is not "registered" by libv4l2,
   there are 2 reasons for this:
   1) This allows us to get completely out of the way when dealing with non
      capture devices.
   2) libv4l2 is the base of the v4l2convert.so wrapper lib, which is a .so
      which can be LD_PRELOAD-ed and the overrules the libc's open/close/etc,
      and when opening /dev/videoX or /dev/v4l/ calls v4l2_open.  Because we
      behave as the regular counterpart when the fd is not known (instead of say
      throwing an error), v4l2convert.so can simply call the v4l2_ prefixed
      function for all wrapped functions (except for v4l2_open which will fail
      when not called on a v4l2 device). This way the wrapper does not have to
      keep track of which fd's are being handled by libv4l2, as libv4l2 already
      keeps track of this itself.

      This also means that libv4l2 may not use any of the regular functions
      it mimics, as for example open could be a symbol in v4l2convert.so, which
      in turn will call v4l2_open, so therefor v4l2_open (for example) may not
      use the regular open()!

   Another important note: libv4l2 does conversion for capture usage only, if
   any calls are made which are passed a v4l2_buffer or v4l2_format with a
   v4l2_buf_type which is different from V4L2_BUF_TYPE_VIDEO_CAPTURE, then
   the v4l2_ methods behave exactly the same as their regular counterparts.
   When modifications are made, one should be carefull that this behavior is
   preserved.
*/
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "libv4l2.h"
#include "libv4l2-priv.h"

/* Note these flags are stored together with the flags passed to v4l2_fd_open()
   in v4l2_dev_info's flags member, so care should be taken that the do not
   use the same bits! */
#define V4L2_STREAMON        0x0100

#define V4L2_MMAP_OFFSET_MAGIC      0xABCDEF00u

static pthread_mutex_t v4l2_open_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct v4l2_dev_info devices[V4L2_MAX_DEVICES] = { { .fd = -1 },
  { .fd = -1 }, { .fd = -1 }, { .fd = -1 }, { .fd = -1 }, { .fd = -1 },
  { .fd = -1 }, { .fd = -1 }, { .fd = -1 }, { .fd = -1 }, { .fd = -1 },
  { .fd = -1 }, { .fd = -1 }, { .fd = -1 }, { .fd = -1 }, { .fd = -1 }};
static int devices_used = 0;


static int v4l2_request_read_buffers(int index)
{
  int result;
  struct v4l2_requestbuffers req;

  /* No-op if already done */
  if (devices[index].no_frames)
    return 0;

  /* Request buffers */
  req.count = devices[index].nreadbuffers;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if ((result = syscall(SYS_ioctl, devices[index].fd, VIDIOC_REQBUFS, &req)) < 0){
    int saved_err = errno;
    V4L2_LOG_ERR("requesting buffers: %s\n", strerror(errno));
    errno = saved_err;
    return result;
  }

  devices[index].no_frames = MIN(req.count, V4L2_MAX_NO_FRAMES);
  return 0;
}

static int v4l2_unrequest_read_buffers(int index)
{
  int result;
  struct v4l2_requestbuffers req;

  /* No-op of already done */
  if (devices[index].no_frames == 0)
    return 0;

  /* (Un)Request buffers */
  req.count = 0;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if ((result = syscall(SYS_ioctl, devices[index].fd, VIDIOC_REQBUFS, &req)) < 0) {
    int saved_err = errno;
    V4L2_LOG_ERR("unrequesting buffers: %s\n", strerror(errno));
    errno = saved_err;
    return result;
  }

  devices[index].no_frames = MIN(req.count, V4L2_MAX_NO_FRAMES);
  if (devices[index].no_frames) {
    V4L2_LOG_ERR("number of buffers > 0 after requesting 0 buffers\n");
    errno = EBUSY;
    return -1;
  }
  return 0;
}

static int v4l2_map_buffers(int index)
{
  int result = 0;
  unsigned int i;
  struct v4l2_buffer buf;

  for (i = 0; i < devices[index].no_frames; i++) {
    if (devices[index].frame_pointers[i] != MAP_FAILED)
      continue;

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    result = syscall(SYS_ioctl, devices[index].fd, VIDIOC_QUERYBUF, &buf);
    if (result) {
      int saved_err = errno;
      V4L2_LOG_ERR("querying buffer %u: %s\n", i, strerror(errno));
      errno = saved_err;
      break;
    }

    devices[index].frame_pointers[i] = (void *)syscall(SYS_mmap2, NULL,
      (size_t)buf.length, PROT_READ, MAP_SHARED, devices[index].fd,
      (__off_t)(buf.m.offset >> MMAP2_PAGE_SHIFT));
    if (devices[index].frame_pointers[i] == MAP_FAILED) {
      int saved_err = errno;
      V4L2_LOG_ERR("mmapping buffer %u: %s\n", i, strerror(errno));
      errno = saved_err;
      result = -1;
      break;
    }
    V4L2_LOG("mapped buffer %u at %p\n", i,
      devices[index].frame_pointers[i]);

    devices[index].frame_sizes[i] = buf.length;
  }

  return result;
}

static void v4l2_unmap_buffers(int index)
{
  unsigned int i;

  /* unmap the buffers */
  for (i = 0; i < devices[index].no_frames; i++) {
    if (devices[index].frame_pointers[i] != MAP_FAILED) {
      syscall(SYS_munmap, devices[index].frame_pointers[i],
	      devices[index].frame_sizes[i]);
      devices[index].frame_pointers[i] = MAP_FAILED;
      V4L2_LOG("unmapped buffer %u\n", i);
    }
  }
}

static int v4l2_streamon(int index)
{
  int result;
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (!(devices[index].flags & V4L2_STREAMON)) {
    if ((result = syscall(SYS_ioctl, devices[index].fd, VIDIOC_STREAMON,
			  &type))) {
      int saved_err = errno;
      V4L2_LOG_ERR("turning on stream: %s\n", strerror(errno));
      errno = saved_err;
      return result;
    }
    devices[index].flags |= V4L2_STREAMON;
  }

  return 0;
}

static int v4l2_streamoff(int index)
{
  int result;
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (devices[index].flags & V4L2_STREAMON) {
    if ((result = syscall(SYS_ioctl, devices[index].fd, VIDIOC_STREAMOFF,
			  &type))) {
      int saved_err = errno;
      V4L2_LOG_ERR("turning off stream: %s\n", strerror(errno));
      errno = saved_err;
      return result;
    }
    devices[index].flags &= ~V4L2_STREAMON;

    /* Stream off also unqueues all our buffers! */
    devices[index].frame_queued = 0;
  }

  return 0;
}

static int v4l2_queue_read_buffer(int index, int buffer_index)
{
  int result;
  struct v4l2_buffer buf;

  if (devices[index].frame_queued & (1 << buffer_index))
    return 0;

  buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index  = buffer_index;
  if ((result = syscall(SYS_ioctl, devices[index].fd, VIDIOC_QBUF, &buf))) {
    int saved_err = errno;
    V4L2_LOG_ERR("queuing buf %d: %s\n", buffer_index, strerror(errno));
    errno = saved_err;
    return result;
  }

  devices[index].frame_queued |= 1 << buffer_index;
  return 0;
}

static int v4l2_dequeue_read_buffer(int index, int *bytesused)
{
  int result;
  struct v4l2_buffer buf;

  buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  if ((result = syscall(SYS_ioctl, devices[index].fd, VIDIOC_DQBUF, &buf))) {
    int saved_err = errno;
    V4L2_LOG_ERR("dequeuing buf: %s\n", strerror(errno));
    errno = saved_err;
    return result;
  }

  devices[index].frame_queued &= ~(1 << buf.index);
  *bytesused = buf.bytesused;
  return buf.index;
}

static int v4l2_queue_read_buffers(int index)
{
  unsigned int i;
  int last_error = EIO, queued = 0;

  for (i = 0; i < devices[index].no_frames; i++) {
    /* Don't queue unmapped buffers (should never happen) */
    if (devices[index].frame_pointers[i] != MAP_FAILED) {
      if (v4l2_queue_read_buffer(index, i)) {
	last_error = errno;
	continue;
      }
      queued++;
    }
  }

  if (!queued) {
    errno = last_error;
    return -1;
  }
  return 0;
}

static int v4l2_activate_read_stream(int index)
{
  int result;

  if ((result = v4l2_request_read_buffers(index)))
    return result;

  if ((result = v4l2_map_buffers(index)))
    return result;

  if ((result = v4l2_queue_read_buffers(index)))
    return result;

  return result = v4l2_streamon(index);
}

static int v4l2_deactivate_read_stream(int index)
{
  int result;

  if ((result = v4l2_streamoff(index)))
    return result;

  /* No need to unqueue our buffers, streamoff does that for us */

  v4l2_unmap_buffers(index);

  if ((result = v4l2_unrequest_read_buffers(index)))
    return result;

  return 0;
}


int v4l2_open (const char *file, int oflag, ...)
{
  int fd;

  /* original open code */
  if (oflag & O_CREAT)
  {
    va_list ap;
    mode_t mode;

    va_start (ap, oflag);
    mode = va_arg (ap, mode_t);

    fd = syscall(SYS_open, file, oflag, mode);

    va_end(ap);
  }
  else
    fd = syscall(SYS_open, file, oflag);
  /* end of original open code */

  if (fd == -1)
    return fd;

  if (v4l2_fd_open(fd, 0) == -1) {
    int saved_err = errno;
    syscall(SYS_close, fd);
    errno = saved_err;
    return -1;
  }

  return fd;
}

int v4l2_fd_open(int fd, int v4l2_flags)
{
  int i, index;
  char *lfname;
  struct v4l2_capability cap;
  struct v4l2_format fmt;
  struct v4lconvert_data *convert;

  /* If no log file was set by the app, see if one was specified through the
     environment */
  if (!v4l2_log_file && (lfname = getenv("LIBV4L2_LOG_FILENAME")))
    v4l2_log_file = fopen(lfname, "w");

  /* check that this is an v4l2 device */
  if (syscall(SYS_ioctl, fd, VIDIOC_QUERYCAP, &cap)) {
    int saved_err = errno;
    V4L2_LOG_ERR("getting capabilities: %s\n", strerror(errno));
    errno = saved_err;
    return -1;
  }

  /* we only add functionality for video capture devices, and we do not
     handle devices which don't do mmap */
  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
      !(cap.capabilities & V4L2_CAP_STREAMING))
    return fd;

  /* Get current cam format */
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (syscall(SYS_ioctl, fd, VIDIOC_G_FMT, &fmt)) {
    int saved_err = errno;
    V4L2_LOG_ERR("getting pixformat: %s\n", strerror(errno));
    errno = saved_err;
    return -1;
  }

  /* init libv4lconvert */
  if (!(convert = v4lconvert_create(fd)))
    return -1;

  /* So we have a v4l2 capture device, register it in our devices array */
  pthread_mutex_lock(&v4l2_open_mutex);
  for (index = 0; index < V4L2_MAX_DEVICES; index++)
    if(devices[index].fd == -1) {
      devices[index].fd = fd;
      break;
    }
  pthread_mutex_unlock(&v4l2_open_mutex);

  if (index == V4L2_MAX_DEVICES) {
    V4L2_LOG_ERR("attempting to open more then %d video devices\n",
      V4L2_MAX_DEVICES);
    errno = EBUSY;
    return -1;
  }

  devices[index].flags = v4l2_flags;
  devices[index].open_count = 1;
  devices[index].src_fmt = fmt;
  devices[index].dest_fmt = fmt;

  pthread_mutex_init(&devices[index].stream_lock, NULL);

  devices[index].no_frames = 0;
  devices[index].nreadbuffers = V4L2_DEFAULT_NREADBUFFERS;
  devices[index].io = v4l2_io_none;
  devices[index].convert = convert;
  devices[index].convert_mmap_buf = MAP_FAILED;
  for (i = 0; i < V4L2_MAX_NO_FRAMES; i++) {
    devices[index].frame_pointers[i] = MAP_FAILED;
    devices[index].frame_map_count[i] = 0;
  }
  devices[index].frame_queued = 0;

  if (index >= devices_used)
    devices_used = index + 1;

  V4L2_LOG("open: %d\n", fd);

  return fd;
}

/* Is this an fd for which we are emulating v4l1 ? */
static int v4l2_get_index(int fd)
{
  int index;

  /* We never handle fd -1 */
  if (fd == -1)
    return -1;

  for (index = 0; index < devices_used; index++)
    if (devices[index].fd == fd)
      break;

  if (index == devices_used)
    return -1;

  return index;
}


int v4l2_close(int fd)
{
  int index, result, i;

  if ((index = v4l2_get_index(fd)) == -1)
    return syscall(SYS_close, fd);

  /* Abuse stream_lock to stop 2 closes from racing and trying to free the
     resources twice */
  pthread_mutex_lock(&devices[index].stream_lock);
  devices[index].open_count--;
  result = devices[index].open_count != 0;
  pthread_mutex_unlock(&devices[index].stream_lock);

  if (result)
    return 0;

  /* Free resources */
  v4l2_unmap_buffers(index);
  v4lconvert_destroy(devices[index].convert);
  if (devices[index].convert_mmap_buf != MAP_FAILED) {
    for (i = 0; i < V4L2_MAX_NO_FRAMES; i++)
      if (devices[index].frame_map_count[i])
	break;

    if (i != V4L2_MAX_NO_FRAMES)
      V4L2_LOG("v4l2 mmap buffers still mapped on close()\n");
    else
      syscall(SYS_munmap, devices[index].convert_mmap_buf,
	      devices[index].no_frames * V4L2_FRAME_BUF_SIZE);
    devices[index].convert_mmap_buf = MAP_FAILED;
  }

  /* Remove the fd from our list of managed fds before closing it, because as
     soon as we've done the actual close the fd maybe returned by an open in
     another thread and we don't want to intercept calls to this new fd. */
  devices[index].fd = -1;

  /* Since we've marked the fd as no longer used, and freed the resources,
     redo the close in case it was interrupted */
  do {
    result = syscall(SYS_close, fd);
  } while (result == -1 && errno == EINTR);

  V4L2_LOG("close: %d\n", fd);

  return result;
}

int v4l2_dup(int fd)
{
  int index;

  if ((index = v4l2_get_index(fd)) == -1)
    return syscall(SYS_dup, fd);

  devices[index].open_count++;

  return fd;
}


int v4l2_ioctl (int fd, unsigned long int request, ...)
{
  void *arg;
  va_list ap;
  int result, converting, index, saved_err;
  int is_capture_request = 0, stream_needs_locking = 0;

  va_start (ap, request);
  arg = va_arg (ap, void *);
  va_end (ap);

  if ((index = v4l2_get_index(fd)) == -1)
    return syscall(SYS_ioctl, fd, request, arg);

  /* Appearantly the kernel and / or glibc ignore the 32 most significant bits
     when long = 64 bits, and some applications pass an int holding the req to
     ioctl, causing it to get sign extended, depending upon this behavior */
  request = (unsigned int)request;

  /* Is this a capture request and do we need to take the stream lock? */
  switch (request) {
    case VIDIOC_QUERYCAP:
      is_capture_request = 1;
      break;
    case VIDIOC_ENUM_FMT:
      if (((struct v4l2_fmtdesc *)arg)->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
	is_capture_request = 1;
      break;
    case VIDIOC_TRY_FMT:
      if (((struct v4l2_format *)arg)->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
	is_capture_request = 1;
      break;
    case VIDIOC_S_FMT:
    case VIDIOC_G_FMT:
      if (((struct v4l2_format *)arg)->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
	is_capture_request = 1;
	stream_needs_locking = 1;
      }
      break;
    case VIDIOC_REQBUFS:
      if (((struct v4l2_requestbuffers *)arg)->type ==
	  V4L2_BUF_TYPE_VIDEO_CAPTURE) {
	is_capture_request = 1;
	stream_needs_locking = 1;
      }
      break;
    case VIDIOC_QUERYBUF:
    case VIDIOC_QBUF:
    case VIDIOC_DQBUF:
      if (((struct v4l2_buffer *)arg)->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
	is_capture_request = 1;
	stream_needs_locking = 1;
      }
      break;
    case VIDIOC_STREAMON:
    case VIDIOC_STREAMOFF:
      if (*((enum v4l2_buf_type *)arg) == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
	is_capture_request = 1;
	stream_needs_locking = 1;
      }
  }

  if (!is_capture_request) {
    result = syscall(SYS_ioctl, fd, request, arg);
    saved_err = errno;
    v4l2_log_ioctl(request, arg, result);
    errno = saved_err;
    return result;
  }


  if (stream_needs_locking)
    pthread_mutex_lock(&devices[index].stream_lock);

  converting = devices[index].src_fmt.fmt.pix.pixelformat !=
	       devices[index].dest_fmt.fmt.pix.pixelformat;


  switch (request) {
    case VIDIOC_QUERYCAP:
      {
	struct v4l2_capability *cap = arg;

	result = syscall(SYS_ioctl, devices[index].fd, VIDIOC_QUERYCAP, cap);
	if (result == 0)
	  /* We always support read() as we fake it using mmap mode */
	  cap->capabilities |= V4L2_CAP_READWRITE;
      }
      break;

    case VIDIOC_ENUM_FMT:
      result = v4lconvert_enum_fmt(devices[index].convert, arg);
      break;

    case VIDIOC_TRY_FMT:
      result = v4lconvert_try_format(devices[index].convert, arg, NULL);
      break;

    case VIDIOC_S_FMT:
      {
	struct v4l2_format src_fmt, *dest_fmt = arg;

	if (!memcmp(&devices[index].dest_fmt, dest_fmt, sizeof(*dest_fmt))) {
	  result = 0;
	  break;
	}

	/* Don't allow changing the format when mmap-ed IO is active, we could
	   allow this to happen in certain special circumstances, but it is
	   best to consistently deny this so that application developers do not
	   go expect this to work, because in their test setup it happens to
	   work. This also keeps the code much saner. */
	if (devices[index].io == v4l2_io_mmap) {
	  errno = EBUSY;
	  result = -1;
	  break;
	}

	if (devices[index].flags & V4L2_DISABLE_CONVERSION) {
	  result = syscall(SYS_ioctl, devices[index].fd, VIDIOC_TRY_FMT,
			   dest_fmt);
	  src_fmt = *dest_fmt;
	} else {
	  result = v4lconvert_try_format(devices[index].convert, dest_fmt,
					 &src_fmt);
	}

	if (result)
	  break;

	/* Maybe after try format has adjusted width/height etc, to whats
	   available nothing has changed (on the cam side) ? */
	if (!memcmp(&devices[index].src_fmt, &src_fmt, sizeof(src_fmt))) {
	  devices[index].dest_fmt = *dest_fmt;
	  result = 0;
	  break;
	}

	if (devices[index].io == v4l2_io_read) {
	  V4L2_LOG("deactivating read-stream for format change\n");
	  if ((result = v4l2_deactivate_read_stream(index))) {
	    /* Undo what we've done to leave things in a consisten state */
	    if (v4l2_activate_read_stream(index))
	      V4L2_LOG_ERR(
		"reactivating stream after deactivate failure (AAIIEEEE)\n");
	    break;
	  }
	}

	result = syscall(SYS_ioctl, devices[index].fd, VIDIOC_S_FMT, &src_fmt);
	if (result) {
	  saved_err = errno;
	  V4L2_LOG_ERR("setting pixformat: %s\n", strerror(errno));
	  /* Report to the app dest_fmt has not changed */
	  *dest_fmt = devices[index].dest_fmt;
	  errno = saved_err;
	  break;
	}

	devices[index].src_fmt = src_fmt;
	devices[index].dest_fmt = *dest_fmt;
      }
      break;

    case VIDIOC_G_FMT:
      {
	struct v4l2_format* fmt = arg;

	*fmt = devices[index].dest_fmt;
	result = 0;
      }
      break;

    case VIDIOC_REQBUFS:
      {
	int i;
	struct v4l2_requestbuffers *req = arg;

	/* Don't allow mixing read / mmap io, either we control the buffers
	   (read based io), or the app does */
	if (devices[index].io == v4l2_io_read) {
	  V4L2_LOG_ERR("to change from read io to mmap io open and close the device first!\n");
	  errno = EBUSY;
	  result = -1;
	  break;
	}

	/* Are any of our fake (convert_mmap_buf) buffers still mapped ? */
	for (i = 0; i < V4L2_MAX_NO_FRAMES; i++)
	  if (devices[index].frame_map_count[i])
	    break;

	if (i != V4L2_MAX_NO_FRAMES) {
	  errno = EBUSY;
	  result = -1;
	  break;
	}

	/* IMPROVEME (maybe?) add support for userptr's? */
	if (req->memory != V4L2_MEMORY_MMAP) {
	  errno = EINVAL;
	  result = -1;
	  break;
	}

	/* No more buffers then we can manage please */
	if (req->count > V4L2_MAX_NO_FRAMES)
	  req->count = V4L2_MAX_NO_FRAMES;

	/* Stop stream and unmap our real mapping of the buffers
	   (only relevant when we're converting, otherwise a no-op) */
	v4l2_streamoff(index);
	v4l2_unmap_buffers(index);

	result = syscall(SYS_ioctl, devices[index].fd, VIDIOC_REQBUFS, req);
	if (result < 0)
	  break;
	result = 0; /* some drivers return the number of buffers on success */

	/* If we got more frames then we can handle lie to the app */
	if (req->count > V4L2_MAX_NO_FRAMES)
	  req->count = V4L2_MAX_NO_FRAMES;

	/* Force reallocation of convert_mmap_buf to fit the new no_frames */
	syscall(SYS_munmap, devices[index].convert_mmap_buf,
		devices[index].no_frames * V4L2_FRAME_BUF_SIZE);
	devices[index].convert_mmap_buf = MAP_FAILED;
	devices[index].no_frames = req->count;
	devices[index].io = req->count? v4l2_io_mmap:v4l2_io_none;
      }
      break;

    case VIDIOC_QUERYBUF:
      {
	struct v4l2_buffer *buf = arg;

	/* Do a real query even when converting to let the driver fill in
	   things like buf->field */
	result = syscall(SYS_ioctl, devices[index].fd, VIDIOC_QUERYBUF, buf);
	if (result || !converting)
	  break;

	buf->m.offset = V4L2_MMAP_OFFSET_MAGIC | buf->index;
	buf->length = V4L2_FRAME_BUF_SIZE;
	if (devices[index].frame_map_count[buf->index])
	  buf->flags |= V4L2_BUF_FLAG_MAPPED;
	else
	  buf->flags &= ~V4L2_BUF_FLAG_MAPPED;
      }
      break;

    case VIDIOC_QBUF:
      result = syscall(SYS_ioctl, devices[index].fd, VIDIOC_QBUF, arg);
      break;

    case VIDIOC_DQBUF:
      {
	struct v4l2_buffer *buf = arg;

	result = syscall(SYS_ioctl, devices[index].fd, VIDIOC_DQBUF, buf);
	if (result) {
	  V4L2_LOG_ERR("dequeing buffer: %s\n", strerror(errno));
	  break;
	}

	if (!converting)
	  break;

	/* An application can do a DQBUF before mmap-ing in the buffer,
	   but we need the buffer _now_ to write our converted data
	   to it! */
	if (devices[index].convert_mmap_buf == MAP_FAILED) {
	  devices[index].convert_mmap_buf = (void *)syscall(SYS_mmap2,
						   (size_t)(
						     devices[index].no_frames *
						     V4L2_FRAME_BUF_SIZE),
						   PROT_READ|PROT_WRITE,
						   MAP_ANONYMOUS|MAP_PRIVATE,
						   -1, 0);
	  if (devices[index].convert_mmap_buf == MAP_FAILED) {
	    saved_err = errno;
	    V4L2_LOG_ERR("allocating conversion buffer\n");
	    errno = saved_err;
	    result = -1;
	    break;
	  }
	}

	/* Make sure we have the real v4l2 buffers mapped before trying to
	   read from them */
	if ((result = v4l2_map_buffers(index)))
	  break;

	result = v4lconvert_convert(devices[index].convert,
		   &devices[index].src_fmt, &devices[index].dest_fmt,
		   devices[index].frame_pointers[buf->index],
		   buf->bytesused,
		   devices[index].convert_mmap_buf +
		     buf->index * V4L2_FRAME_BUF_SIZE,
		   V4L2_FRAME_BUF_SIZE);
	if (result < 0) {
	  V4L2_LOG_ERR("converting / decoding frame data: %s\n",
			v4lconvert_get_error_message(devices[index].convert));
	  break;
	}

	buf->bytesused = result;
	buf->m.offset = V4L2_MMAP_OFFSET_MAGIC | buf->index;
	buf->length = V4L2_FRAME_BUF_SIZE;
	if (devices[index].frame_map_count[buf->index])
	  buf->flags |= V4L2_BUF_FLAG_MAPPED;
	else
	  buf->flags &= ~V4L2_BUF_FLAG_MAPPED;

	result = 0;
      }
      break;

    case VIDIOC_STREAMON:
    case VIDIOC_STREAMOFF:
      if (devices[index].io != v4l2_io_mmap) {
	errno = EINVAL;
	result = -1;
	break;
      }

      if (request == VIDIOC_STREAMON)
	result = v4l2_streamon(index);
      else
	result = v4l2_streamoff(index);
      break;

    default:
      result = syscall(SYS_ioctl, fd, request, arg);
  }

  if (stream_needs_locking)
    pthread_mutex_unlock(&devices[index].stream_lock);

  saved_err = errno;
  v4l2_log_ioctl(request, arg, result);
  errno = saved_err;

  return result;
}


ssize_t v4l2_read (int fd, void* buffer, size_t n)
{
  ssize_t result;
  int index, bytesused = 0, frame_index;

  if ((index = v4l2_get_index(fd)) == -1)
    return syscall(SYS_read, fd, buffer, n);

  pthread_mutex_lock(&devices[index].stream_lock);

  if (devices[index].io == v4l2_io_mmap) {
    V4L2_LOG_ERR("to change from mmap io to read io first do request_buffers with a count of 0\n");
    errno = EBUSY;
    result = -1;
    goto leave;
  }
  devices[index].io = v4l2_io_read;

  if ((result = v4l2_activate_read_stream(index)))
    goto leave;

  if ((frame_index = v4l2_dequeue_read_buffer(index, &bytesused)) < 0) {
    result = -1;
    goto leave;
  }

  result = v4lconvert_convert(devices[index].convert,
	       &devices[index].src_fmt, &devices[index].dest_fmt,
	       devices[index].frame_pointers[frame_index], bytesused,
	       buffer, n);

  v4l2_queue_read_buffer(index, frame_index);

  if (result < 0)
    V4L2_LOG_ERR("converting / decoding frame data: %s\n",
		 v4lconvert_get_error_message(devices[index].convert));

leave:
  pthread_mutex_unlock(&devices[index].stream_lock);

  return result;
}

void *v4l2_mmap(void *start, size_t length, int prot, int flags, int fd,
  __off64_t offset)
{
  int index;
  unsigned int buffer_index;
  void *result;

  if ((index = v4l2_get_index(fd)) == -1 ||
      /* Check if the mmap data matches our answer to QUERY_BUF, if it doesn't
	 let the kernel handle it (to allow for mmap based non capture use) */
      start || length != V4L2_FRAME_BUF_SIZE ||
      ((unsigned int)offset & ~0xFFu) != V4L2_MMAP_OFFSET_MAGIC) {
    if (index != -1)
      V4L2_LOG("Passing mmap(%p, %d, ..., %x, through to the driver\n",
	start, (int)length, (int)offset);

    if (offset & ((1 << MMAP2_PAGE_SHIFT) - 1)) {
      errno = EINVAL;
      return MAP_FAILED;
    }

    return (void *)syscall(SYS_mmap2, start, length, prot, flags, fd,
			   (__off_t)(offset >> MMAP2_PAGE_SHIFT));
  }

  pthread_mutex_lock(&devices[index].stream_lock);

  buffer_index = offset & 0xff;
  if (buffer_index >= devices[index].no_frames ||
      devices[index].io != v4l2_io_mmap ||
      /* Got magic offset and not converting ?? */
      devices[index].src_fmt.fmt.pix.pixelformat ==
      devices[index].dest_fmt.fmt.pix.pixelformat) {
    errno = EINVAL;
    result = MAP_FAILED;
    goto leave;
  }

  if (devices[index].convert_mmap_buf == MAP_FAILED) {
    devices[index].convert_mmap_buf = (void *)syscall(SYS_mmap2, NULL,
					     (size_t)(
					       devices[index].no_frames *
					       V4L2_FRAME_BUF_SIZE),
					     PROT_READ|PROT_WRITE,
					     MAP_ANONYMOUS|MAP_PRIVATE,
					     -1, 0);
    if (devices[index].convert_mmap_buf == MAP_FAILED) {
      int saved_err = errno;
      V4L2_LOG_ERR("allocating conversion buffer\n");
      errno = saved_err;
      result = MAP_FAILED;
      goto leave;
    }
  }

  devices[index].frame_map_count[buffer_index]++;

  result = devices[index].convert_mmap_buf +
    buffer_index * V4L2_FRAME_BUF_SIZE;

  V4L2_LOG("Fake (conversion) mmap buf %u, seen by app at: %p\n",
    buffer_index, result);

leave:
  pthread_mutex_unlock(&devices[index].stream_lock);

  return result;
}

int v4l2_munmap(void *_start, size_t length)
{
  int index;
  unsigned int buffer_index;
  unsigned char *start = _start;

  /* Is this memory ours? */
  if (start != MAP_FAILED && length == V4L2_FRAME_BUF_SIZE) {
    for (index = 0; index < devices_used; index++)
      if (devices[index].fd != -1 &&
	  devices[index].convert_mmap_buf != MAP_FAILED &&
	  start >= devices[index].convert_mmap_buf &&
	  (start - devices[index].convert_mmap_buf) % length == 0)
	break;

    if (index != devices_used) {
      int unmapped = 0;

      pthread_mutex_lock(&devices[index].stream_lock);

      buffer_index = (start - devices[index].convert_mmap_buf) / length;

      /* Redo our checks now that we have the lock, things may have changed */
      if (devices[index].convert_mmap_buf != MAP_FAILED &&
	  start >= devices[index].convert_mmap_buf &&
	  (start - devices[index].convert_mmap_buf) % length == 0 &&
	  buffer_index < devices[index].no_frames) {
	if (devices[index].frame_map_count[buffer_index] > 0)
	  devices[index].frame_map_count[buffer_index]--;
	unmapped = 1;
      }

      pthread_mutex_unlock(&devices[index].stream_lock);

      if (unmapped) {
	V4L2_LOG("v4l2 fake buffer munmap %p, %d\n", start, (int)length);
	return 0;
      }
    }
  }

  V4L2_LOG("v4l2 unknown munmap %p, %d\n", start, (int)length);

  return syscall(SYS_munmap, _start, length);
}

/* Misc utility functions */
int v4l2_set_control(int fd, int cid, int value)
{
  struct v4l2_queryctrl qctrl = { .id = cid };
  struct v4l2_control ctrl = { .id = cid };
  int result;

  if ((result = syscall(SYS_ioctl, fd, VIDIOC_QUERYCTRL, &qctrl)))
    return result;

  if (!(qctrl.flags & V4L2_CTRL_FLAG_DISABLED) &&
      !(qctrl.flags & V4L2_CTRL_FLAG_GRABBED)) {
    if (qctrl.type == V4L2_CTRL_TYPE_BOOLEAN)
      ctrl.value = value? 1:0;
    else
      ctrl.value = (value * (qctrl.maximum - qctrl.minimum) + 32767) / 65535 +
		   qctrl.minimum;

    result = syscall(SYS_ioctl, fd, VIDIOC_S_CTRL, &ctrl);
  }

  return result;
}

int v4l2_get_control(int fd, int cid)
{
  struct v4l2_queryctrl qctrl = { .id = cid };
  struct v4l2_control ctrl = { .id = cid };

  if (syscall(SYS_ioctl, fd, VIDIOC_QUERYCTRL, &qctrl))
    return 0;

  if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED)
    return 0;

  if (syscall(SYS_ioctl, fd, VIDIOC_G_CTRL, &ctrl))
    return 0;

  return ((ctrl.value - qctrl.minimum) * 65535 +
	  (qctrl.maximum - qctrl.minimum) / 2) /
	 (qctrl.maximum - qctrl.minimum);
}
