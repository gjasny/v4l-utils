#ifndef _V4L_HELPERS_H_
#define _V4L_HELPERS_H_

#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

struct v4l_fd {
	int fd;
	int (*ioctl)(int fd, unsigned long cmd, ...);
	void *(*mmap)(void *addr, size_t length, int prot, int flags,
		      int fd, int64_t offset);
	int (*munmap)(void *addr, size_t length);
};

/*
 * mmap has a different prototype compared to v4l2_mmap. Because of
 * this we have to make a wrapper for it.
 */
static inline void *v4l_fd_mmap(void *addr, size_t length, int prot, int flags,
		                      int fd, int64_t offset)
{
	return mmap(addr, length, prot, flags, fd, offset);
}

static inline void v4l_fd_init(struct v4l_fd *f, int fd)
{
	f->fd = fd;
	f->ioctl = ioctl;
	f->mmap = v4l_fd_mmap;
	f->munmap = munmap;
}

#define v4l_fd_libv4l2_init(f, fd)		\
	do {					\
		(f)->fd = fd;			\
		(f)->ioctl = v4l2_ioctl;	\
		(f)->mmap = v4l2_mmap;		\
		(f)->munmap = v4l2_munmap;	\
	} while (0)

static inline int v4l_ioctl(struct v4l_fd *f, unsigned long cmd, void *arg)
{
	return f->ioctl(f->fd, cmd, arg) ? errno : 0;
}

static inline void *v4l_mmap(struct v4l_fd *f, size_t length, off_t offset)
{
	return f->mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, f->fd, offset);
}

static inline int v4l_munmap(struct v4l_fd *f, void *start, size_t length)
{
	return f->munmap(start, length) ? errno : 0;
}

static inline bool v4l_buf_type_is_planar(unsigned type)
{
	return V4L2_TYPE_IS_MULTIPLANAR(type);
}

static inline bool v4l_buf_type_is_output(unsigned type)
{
	return V4L2_TYPE_IS_OUTPUT(type);
}

static inline bool v4l_buf_type_is_capture(unsigned type)
{
	return !v4l_buf_type_is_output(type);
}

static inline bool v4l_buf_type_is_video(unsigned type)
{
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return true;
	default:
		return false;
	}
}

static inline bool v4l_buf_type_is_vbi(unsigned type)
{
	return type == V4L2_BUF_TYPE_VBI_CAPTURE ||
	       type == V4L2_BUF_TYPE_VBI_OUTPUT;
}

static inline bool v4l_buf_type_is_sliced_vbi(unsigned type)
{
	return type == V4L2_BUF_TYPE_SLICED_VBI_CAPTURE ||
	       type == V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
}

static inline bool v4l_buf_type_is_overlay(unsigned type)
{
	return type == V4L2_BUF_TYPE_VIDEO_OVERLAY ||
	       type == V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
}

static inline bool v4l_buf_type_is_sdr(unsigned type)
{
	return type == V4L2_BUF_TYPE_SDR_CAPTURE;
}

static inline int v4l_querycap(struct v4l_fd *f, struct v4l2_capability *cap)
{
	return v4l_ioctl(f, VIDIOC_QUERYCAP, cap);
}

static inline __u32 v4l_capability_g_caps(const struct v4l2_capability *cap)
{
	return (cap->capabilities & V4L2_CAP_DEVICE_CAPS) ?
			cap->device_caps : cap->capabilities;
}

static inline __u32 v4l_querycap_g_caps(struct v4l_fd *f)
{
	struct v4l2_capability cap;

	return v4l_querycap(f, &cap) ? 0 : v4l_capability_g_caps(&cap);
}

static inline int v4l_g_fmt(struct v4l_fd *f, struct v4l2_format *fmt, unsigned type)
{
	fmt->type = type;
	return v4l_ioctl(f, VIDIOC_G_FMT, fmt);
}

static inline int v4l_try_fmt(struct v4l_fd *f, struct v4l2_format *fmt)
{
	return v4l_ioctl(f, VIDIOC_TRY_FMT, fmt);
}

static inline int v4l_s_fmt(struct v4l_fd *f, struct v4l2_format *fmt)
{
	return v4l_ioctl(f, VIDIOC_S_FMT, fmt);
}

static inline void v4l_format_init(struct v4l2_format *fmt, unsigned type)
{
	memset(fmt, 0, sizeof(*fmt));
	fmt->type = type;
}

static inline void v4l_format_s_width(struct v4l2_format *fmt, __u32 width)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.width = width;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.width = width;
		break;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		fmt->fmt.win.w.width = width;
		break;
	}
}

static inline __u32 v4l_format_g_width(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.width;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.width;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		return fmt->fmt.win.w.width;
	default:
		return 0;
	}
}

static inline void v4l_format_s_height(struct v4l2_format *fmt, __u32 height)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.height = height;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.height = height;
		break;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		fmt->fmt.win.w.height = height;
		break;
	}
}

static inline __u32 v4l_format_g_height(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.height;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.height;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		return fmt->fmt.win.w.height;
	default:
		return 0;
	}
}

static inline void v4l_format_s_pixelformat(struct v4l2_format *fmt, __u32 pixelformat)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.pixelformat = pixelformat;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.pixelformat = pixelformat;
		break;
	case V4L2_BUF_TYPE_SDR_CAPTURE:
		fmt->fmt.sdr.pixelformat = pixelformat;
		break;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		fmt->fmt.vbi.sample_format = pixelformat;
		break;
	}
}

static inline __u32 v4l_format_g_pixelformat(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.pixelformat;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.pixelformat;
	case V4L2_BUF_TYPE_SDR_CAPTURE:
		return fmt->fmt.sdr.pixelformat;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		return fmt->fmt.vbi.sample_format;
	default:
		return 0;
	}
}

static inline void v4l_format_s_field(struct v4l2_format *fmt, unsigned field)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.field = field;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.field = field;
		break;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		fmt->fmt.win.field = field;
		break;
	}
}

static inline unsigned v4l_format_g_field(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.field;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.field;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		return fmt->fmt.win.field;
	default:
		return V4L2_FIELD_NONE;
	}
}

static inline void v4l_format_s_pix_colorspace(struct v4l2_format *fmt,
					       unsigned colorspace)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.colorspace = colorspace;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.colorspace = colorspace;
		break;
	}
}

static inline unsigned
v4l_format_g_pix_colorspace(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.colorspace;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.colorspace;
	default:
		return 0;
	}
}

static inline void v4l_format_s_num_planes(struct v4l2_format *fmt, __u8 num_planes)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.num_planes = num_planes;
		break;
	}
}

static inline __u8
v4l_format_g_num_planes(const struct v4l2_format *fmt)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.num_planes;
	default:
		return 1;
	}
}

static inline void v4l_format_s_bytesperline(struct v4l2_format *fmt,
					     unsigned plane, __u32 bytesperline)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.bytesperline = bytesperline;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.plane_fmt[plane].bytesperline = bytesperline;
		break;
	}
}

static inline __u32
v4l_format_g_bytesperline(const struct v4l2_format *fmt, unsigned plane)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.bytesperline;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.plane_fmt[plane].bytesperline;
	default:
		return 0;
	}
}

static inline void v4l_format_s_sizeimage(struct v4l2_format *fmt,
					  unsigned plane, __u32 sizeimage)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fmt->fmt.pix.sizeimage = sizeimage;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fmt->fmt.pix_mp.plane_fmt[plane].sizeimage = sizeimage;
		break;
	}
}

static inline __u32
v4l_format_g_sizeimage(const struct v4l2_format *fmt, unsigned plane)
{
	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return fmt->fmt.pix.sizeimage;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return fmt->fmt.pix_mp.plane_fmt[plane].sizeimage;
	default:
		return 0;
	}
}

struct v4l_buffer {
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buf;
};

static inline void v4l_buffer_init(struct v4l_buffer *buf,
		unsigned type, unsigned memory, unsigned index)
{
	memset(buf, 0, sizeof(*buf));
	buf->buf.type = type;
	buf->buf.memory = memory;
	buf->buf.index = index;
	if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
		buf->buf.m.planes = buf->planes;
		buf->buf.length = VIDEO_MAX_PLANES;
	}
}

static inline bool v4l_buffer_is_planar(const struct v4l_buffer *buf)
{
	return v4l_buf_type_is_planar(buf->buf.type);
}

static inline bool v4l_buffer_is_output(const struct v4l_buffer *buf)
{
	return v4l_buf_type_is_output(buf->buf.type);
}

static inline bool v4l_buffer_is_capture(const struct v4l_buffer *buf)
{
	return v4l_buf_type_is_capture(buf->buf.type);
}

static inline bool v4l_buffer_is_video(const struct v4l_buffer *buf)
{
	return v4l_buf_type_is_video(buf->buf.type);
}

static inline bool v4l_buffer_is_vbi(const struct v4l_buffer *buf)
{
	return v4l_buf_type_is_vbi(buf->buf.type);
}

static inline bool v4l_buffer_is_sliced_vbi(const struct v4l_buffer *buf)
{
	return v4l_buf_type_is_sliced_vbi(buf->buf.type);
}

static inline bool v4l_buffer_is_sdr(const struct v4l_buffer *buf)
{
	return v4l_buf_type_is_sdr(buf->buf.type);
}

static inline unsigned v4l_buffer_g_num_planes(const struct v4l_buffer *buf)
{
	if (v4l_buffer_is_planar(buf))
		return buf->buf.length;
	return 1;
}

static inline __u32 v4l_buffer_g_index(const struct v4l_buffer *buf)
{
	return buf->buf.index;
}

static inline void v4l_buffer_s_index(struct v4l_buffer *buf, __u32 index)
{
	buf->buf.index = index;
}

static inline unsigned v4l_buffer_g_type(const struct v4l_buffer *buf)
{
	return buf->buf.type;
}

static inline unsigned v4l_buffer_g_memory(const struct v4l_buffer *buf)
{
	return buf->buf.memory;
}

static inline __u32 v4l_buffer_g_flags(const struct v4l_buffer *buf)
{
	return buf->buf.flags;
}

static inline void v4l_buffer_s_flags(struct v4l_buffer *buf, __u32 flags)
{
	buf->buf.flags = flags;
}

static inline void v4l_buffer_or_flags(struct v4l_buffer *buf, __u32 flags)
{
	buf->buf.flags |= flags;
}

static inline unsigned v4l_buffer_g_field(const struct v4l_buffer *buf)
{
	return buf->buf.field;
}

static inline void v4l_buffer_s_field(struct v4l_buffer *buf, unsigned field)
{
	buf->buf.field = field;
}

static inline __u32 v4l_buffer_g_sequence(const struct v4l_buffer *buf)
{
	return buf->buf.sequence;
}

static inline const struct timeval *v4l_buffer_g_timestamp(const struct v4l_buffer *buf)
{
	return &buf->buf.timestamp;
}

static inline void v4l_buffer_s_timestamp(struct v4l_buffer *buf, const struct timeval *tv)
{
	buf->buf.timestamp = *tv;
}

static inline void v4l_buffer_s_timestamp_ts(struct v4l_buffer *buf, const struct timespec *ts)
{
	buf->buf.timestamp.tv_sec = ts->tv_sec;
	buf->buf.timestamp.tv_usec = ts->tv_nsec / 1000;
}

static inline void v4l_buffer_s_timestamp_clock(struct v4l_buffer *buf)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	v4l_buffer_s_timestamp_ts(buf, &ts);
}

static inline const struct v4l2_timecode *v4l_buffer_g_timecode(const struct v4l_buffer *buf)
{
	return &buf->buf.timecode;
}

static inline void v4l_buffer_s_timecode(struct v4l_buffer *buf, const struct v4l2_timecode *tc)
{
	buf->buf.timecode = *tc;
}

static inline __u32 v4l_buffer_g_timestamp_type(const struct v4l_buffer *buf)
{
	return buf->buf.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK;
}

static inline bool v4l_buffer_is_copy(const struct v4l_buffer *buf)
{
	return v4l_buffer_g_timestamp_type(buf) == V4L2_BUF_FLAG_TIMESTAMP_COPY;
}

static inline __u32 v4l_buffer_g_timestamp_src(const struct v4l_buffer *buf)
{
	return buf->buf.flags & V4L2_BUF_FLAG_TSTAMP_SRC_MASK;
}

static inline void v4l_buffer_s_timestamp_src(struct v4l_buffer *buf, __u32 src)
{
	buf->buf.flags &= ~V4L2_BUF_FLAG_TSTAMP_SRC_MASK;
	buf->buf.flags |= src & V4L2_BUF_FLAG_TSTAMP_SRC_MASK;
}

static inline unsigned v4l_buffer_g_length(const struct v4l_buffer *buf, unsigned plane)
{
	if (v4l_buffer_is_planar(buf))
		return buf->planes[plane].length;
	return buf->buf.length;
}

static inline unsigned v4l_buffer_g_bytesused(const struct v4l_buffer *buf, unsigned plane)
{
	if (v4l_buffer_is_planar(buf))
		return buf->planes[plane].bytesused;
	return buf->buf.bytesused;
}

static inline void v4l_buffer_s_bytesused(struct v4l_buffer *buf, unsigned plane, __u32 bytesused)
{
	if (v4l_buffer_is_planar(buf))
		buf->planes[plane].bytesused = bytesused;
	else
		buf->buf.bytesused = bytesused;
}

static inline unsigned v4l_buffer_g_data_offset(const struct v4l_buffer *buf, unsigned plane)
{
	if (v4l_buffer_is_planar(buf))
		return buf->planes[plane].data_offset;
	return 0;
}

static inline void v4l_buffer_s_data_offset(struct v4l_buffer *buf, unsigned plane, __u32 data_offset)
{
	if (v4l_buffer_is_planar(buf))
		buf->planes[plane].data_offset = data_offset;
}

static inline __u32 v4l_buffer_g_mem_offset(const struct v4l_buffer *buf, unsigned plane)
{
	if (v4l_buffer_is_planar(buf))
		return buf->planes[plane].m.mem_offset;
	return buf->buf.m.offset;
}

static inline void v4l_buffer_s_userptr(struct v4l_buffer *buf, unsigned plane, void *userptr)
{
	if (v4l_buffer_is_planar(buf))
		buf->planes[plane].m.userptr = (unsigned long)userptr;
	else
		buf->buf.m.userptr = (unsigned long)userptr;
}

static inline void *v4l_buffer_g_userptr(const struct v4l_buffer *buf, unsigned plane)
{
	if (v4l_buffer_is_planar(buf))
		return (void *)buf->planes[plane].m.userptr;
	return (void *)buf->buf.m.userptr;
}

static inline void v4l_buffer_s_fd(struct v4l_buffer *buf, unsigned plane, int fd)
{
	if (v4l_buffer_is_planar(buf))
		buf->planes[plane].m.fd = fd;
	else
		buf->buf.m.fd = fd;
}

static inline int v4l_buffer_g_fd(const struct v4l_buffer *buf, unsigned plane)
{
	if (v4l_buffer_is_planar(buf))
		return buf->planes[plane].m.fd;
	return buf->buf.m.fd;
}

static inline int v4l_buffer_prepare_buf(struct v4l_fd *f, struct v4l_buffer *buf)
{
	return v4l_ioctl(f, VIDIOC_PREPARE_BUF, &buf->buf);
}

static inline int v4l_buffer_qbuf(struct v4l_fd *f, struct v4l_buffer *buf)
{
	return v4l_ioctl(f, VIDIOC_QBUF, &buf->buf);
}

static inline int v4l_buffer_dqbuf(struct v4l_fd *f, struct v4l_buffer *buf)
{
	return v4l_ioctl(f, VIDIOC_DQBUF, &buf->buf);
}

static inline int v4l_buffer_querybuf(struct v4l_fd *f, struct v4l_buffer *buf, unsigned index)
{
	v4l_buffer_s_index(buf, index);
	return v4l_ioctl(f, VIDIOC_QUERYBUF, &buf->buf);
}

struct v4l_queue {
	unsigned type;
	unsigned memory;
	unsigned buffers;
	unsigned num_planes;

	__u32 lengths[VIDEO_MAX_PLANES];
	__u32 mem_offsets[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];
	void *mmappings[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];
	unsigned long userptrs[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];
	int fds[VIDEO_MAX_FRAME][VIDEO_MAX_PLANES];
};

static inline void v4l_queue_init(struct v4l_queue *q,
		unsigned type, unsigned memory)
{
	unsigned i, p;

	memset(q, 0, sizeof(*q));
	q->type = type;
	q->memory = memory;
	for (i = 0; i < VIDEO_MAX_FRAME; i++)
		for (p = 0; p < VIDEO_MAX_PLANES; p++)
			q->fds[i][p] = -1;
}

static inline unsigned v4l_queue_g_type(const struct v4l_queue *q)
{
	return q->type;
}

static inline unsigned v4l_queue_g_memory(const struct v4l_queue *q)
{
	return q->memory;
}

static inline bool v4l_queue_is_planar(const struct v4l_queue *q)
{
	return v4l_buf_type_is_planar(q->type);
}

static inline bool v4l_queue_is_output(const struct v4l_queue *q)
{
	return v4l_buf_type_is_output(q->type);
}

static inline bool v4l_queue_is_capture(const struct v4l_queue *q)
{
	return v4l_buf_type_is_capture(q->type);
}

static inline bool v4l_queue_is_video(const struct v4l_queue *q)
{
	return v4l_buf_type_is_video(q->type);
}

static inline bool v4l_queue_is_vbi(const struct v4l_queue *q)
{
	return v4l_buf_type_is_vbi(q->type);
}

static inline bool v4l_queue_is_sliced_vbi(const struct v4l_queue *q)
{
	return v4l_buf_type_is_sliced_vbi(q->type);
}

static inline bool v4l_queue_is_sdr(const struct v4l_queue *q)
{
	return v4l_buf_type_is_sdr(q->type);
}

static inline unsigned v4l_queue_g_buffers(const struct v4l_queue *q)
{
	return q->buffers;
}

static inline unsigned v4l_queue_g_num_planes(const struct v4l_queue *q)
{
	return q->num_planes;
}

static inline __u32 v4l_queue_g_length(const struct v4l_queue *q, unsigned plane)
{
	return q->lengths[plane];
}

static inline __u32 v4l_queue_g_mem_offset(const struct v4l_queue *q, unsigned index, unsigned plane)
{
	return q->mem_offsets[index][plane];
}

static inline void v4l_queue_s_mmapping(struct v4l_queue *q, unsigned index, unsigned plane, void *m)
{
	q->mmappings[index][plane] = m;
}

static inline void *v4l_queue_g_mmapping(const struct v4l_queue *q, unsigned index, unsigned plane)
{
	return q->mmappings[index][plane];
}

static inline void v4l_queue_s_userptr(struct v4l_queue *q, unsigned index, unsigned plane, void *m)
{
	q->userptrs[index][plane] = (unsigned long)m;
}

static inline void *v4l_queue_g_userptr(const struct v4l_queue *q, unsigned index, unsigned plane)
{
	return (void *)q->userptrs[index][plane];
}

static inline void v4l_queue_s_fd(struct v4l_queue *q, unsigned index, unsigned plane, int fd)
{
	q->fds[index][plane] = fd;
}

static inline int v4l_queue_g_fd(const struct v4l_queue *q, unsigned index, unsigned plane)
{
	return q->fds[index][plane];
}

static inline int v4l_queue_querybufs(struct v4l_fd *f, struct v4l_queue *q, unsigned from)
{
	unsigned b, p;
	int ret;

	for (b = from; b < v4l_queue_g_buffers(q); b++) {
		struct v4l_buffer buf;

		v4l_buffer_init(&buf, v4l_queue_g_type(q), v4l_queue_g_memory(q), b);
		ret = v4l_ioctl(f, VIDIOC_QUERYBUF, &buf.buf);
		if (ret)
			return ret;
		if (b == 0) {
			q->num_planes = v4l_buffer_g_num_planes(&buf);
			for (p = 0; p < v4l_queue_g_num_planes(q); p++)
				q->lengths[p] = v4l_buffer_g_length(&buf, p);
		}
		if (q->memory == V4L2_MEMORY_MMAP)
			for (p = 0; p < q->num_planes; p++)
				q->mem_offsets[b][p] = v4l_buffer_g_mem_offset(&buf, p);
	}
	return 0;
}

static inline int v4l_queue_reqbufs(struct v4l_fd *f,
		struct v4l_queue *q, unsigned count)
{
	struct v4l2_requestbuffers reqbufs;
	int ret;

	reqbufs.type = q->type;
	reqbufs.memory = q->memory;
	reqbufs.count = count;
	memset(reqbufs.reserved, 0, sizeof(reqbufs.reserved));
	/*
	 * Problem: if REQBUFS returns an error, did it free any old
	 * buffers or not?
	 */
	ret = v4l_ioctl(f, VIDIOC_REQBUFS, &reqbufs);
	if (ret)
		return ret;
	q->buffers = reqbufs.count;
	return v4l_queue_querybufs(f, q, 0);
}

static inline bool v4l_queue_has_create_bufs(struct v4l_fd *f, const struct v4l_queue *q)
{
	struct v4l2_create_buffers createbufs;

	memset(&createbufs, 0, sizeof(createbufs));
	createbufs.format.type = q->type;
	createbufs.memory = q->memory;
	return v4l_ioctl(f, VIDIOC_CREATE_BUFS, &createbufs) == 0;
}

static inline int v4l_queue_create_bufs(struct v4l_fd *f,
		struct v4l_queue *q, unsigned count, const struct v4l2_format *fmt)
{
	struct v4l2_create_buffers createbufs;
	int ret;

	createbufs.format.type = q->type;
	createbufs.memory = q->memory;
	createbufs.count = count;
	if (fmt) {
		createbufs.format = *fmt;
	} else {
		ret = v4l_ioctl(f, VIDIOC_G_FMT, &createbufs.format);
		if (ret)
			return ret;
	}
	memset(createbufs.reserved, 0, sizeof(createbufs.reserved));
	ret = v4l_ioctl(f, VIDIOC_CREATE_BUFS, &createbufs);
	if (ret)
		return ret;
	q->buffers += createbufs.count;
	return v4l_queue_querybufs(f, q, q->buffers - createbufs.count);
}

static inline int v4l_queue_streamon(struct v4l_fd *f, struct v4l_queue *q)
{
	return v4l_ioctl(f, VIDIOC_STREAMON, &q->type);
}

static inline int v4l_queue_streamoff(struct v4l_fd *f, struct v4l_queue *q)
{
	return v4l_ioctl(f, VIDIOC_STREAMOFF, &q->type);
}

static inline int v4l_queue_mmap_bufs(struct v4l_fd *f,
		struct v4l_queue *q, unsigned from)
{
	unsigned b, p;

	if (q->memory != V4L2_MEMORY_MMAP && q->memory != V4L2_MEMORY_DMABUF)
		return 0;

	for (b = 0; b < v4l_queue_g_buffers(q); b++) {
		for (p = 0; p < v4l_queue_g_num_planes(q); p++) {
			void *m = MAP_FAILED;

			if (q->memory == V4L2_MEMORY_MMAP)
				m = v4l_mmap(f, v4l_queue_g_length(q, p), v4l_queue_g_mem_offset(q, b, p));
			else if (q->memory == V4L2_MEMORY_DMABUF)
				m = mmap(NULL, v4l_queue_g_length(q, p),
						PROT_READ | PROT_WRITE, MAP_SHARED,
						v4l_queue_g_fd(q, b, p), 0);

			if (m == MAP_FAILED)
				return errno;
			v4l_queue_s_mmapping(q, b, p, m);
		}
	}
	return 0;
}
static inline int v4l_queue_munmap_bufs(struct v4l_fd *f, struct v4l_queue *q)
{
	unsigned b, p;
	int ret = 0;

	if (q->memory != V4L2_MEMORY_MMAP && q->memory != V4L2_MEMORY_DMABUF)
		return 0;

	for (b = 0; b < v4l_queue_g_buffers(q); b++) {
		for (p = 0; p < v4l_queue_g_num_planes(q); p++) {
			void *m = v4l_queue_g_mmapping(q, b, p);

			if (m == NULL)
				continue;

			if (q->memory == V4L2_MEMORY_MMAP)
				ret = v4l_munmap(f, m, v4l_queue_g_length(q, p));
			else if (q->memory == V4L2_MEMORY_DMABUF)
				ret = munmap(m, v4l_queue_g_length(q, p)) ? errno : 0;
			if (ret)
				return ret;
			v4l_queue_s_mmapping(q, b, p, NULL);
		}
	}
	return 0;
}

static inline int v4l_queue_alloc_bufs(struct v4l_fd *f,
		struct v4l_queue *q, unsigned from)
{
	unsigned b, p;

	if (q->memory != V4L2_MEMORY_USERPTR)
		return 0;
	for (b = 0; b < v4l_queue_g_buffers(q); b++) {
		for (p = 0; p < v4l_queue_g_num_planes(q); p++) {
			void *m = malloc(v4l_queue_g_length(q, p));

			if (m == NULL)
				return errno;
			v4l_queue_s_userptr(q, b, p, m);
		}
	}
	return 0;
}

static inline int v4l_queue_free_bufs(struct v4l_queue *q)
{
	unsigned b, p;

	if (q->memory != V4L2_MEMORY_USERPTR)
		return 0;
	for (b = 0; b < v4l_queue_g_buffers(q); b++) {
		for (p = 0; p < v4l_queue_g_num_planes(q); p++) {
			free(v4l_queue_g_mmapping(q, b, p));
			v4l_queue_s_mmapping(q, b, p, NULL);
		}
	}
	return 0;
}


static inline bool v4l_queue_has_expbuf(struct v4l_fd *f)
{
	struct v4l2_exportbuffer expbuf;

	memset(&expbuf, 0, sizeof(expbuf));
	return v4l_ioctl(f, VIDIOC_EXPBUF, &expbuf) != ENOTTY;
}

static inline int v4l_queue_export_bufs(struct v4l_fd *f, struct v4l_queue *q)
{
	struct v4l2_exportbuffer expbuf;
	unsigned b, p;
	int ret = 0;

	expbuf.type = q->type;
	expbuf.flags = O_RDWR;
	memset(expbuf.reserved, 0, sizeof(expbuf.reserved));
	for (b = 0; b < v4l_queue_g_buffers(q); b++) {
		expbuf.index = b;
		for (p = 0; p < v4l_queue_g_num_planes(q); p++) {
			expbuf.plane = p;
			ret = v4l_ioctl(f, VIDIOC_EXPBUF, &expbuf);
			if (ret)
				return ret;
			v4l_queue_s_fd(q, b, p, expbuf.fd);
		}
	}
	return 0;
}

static inline void v4l_queue_close_exported_fds(struct v4l_queue *q)
{
	unsigned b, p;

	if (q->memory != V4L2_MEMORY_MMAP)
		return;

	for (b = 0; b < v4l_queue_g_buffers(q); b++) {
		for (p = 0; p < v4l_queue_g_num_planes(q); p++) {
			int fd = v4l_queue_g_fd(q, b, p);

			if (fd != -1) {
				close(fd);
				v4l_queue_s_fd(q, b, p, -1);
			}
		}
	}
}

static inline void v4l_queue_free(struct v4l_fd *f, struct v4l_queue *q)
{
	v4l_queue_streamoff(f, q);
	v4l_queue_free_bufs(q);
	v4l_queue_munmap_bufs(f, q);
	v4l_queue_close_exported_fds(q);
	v4l_queue_reqbufs(f, q, 0);
}

static inline void v4l_queue_buffer_init(const struct v4l_queue *q, struct v4l_buffer *buf, unsigned index)
{
	unsigned p;
		
	v4l_buffer_init(buf, v4l_queue_g_type(q), v4l_queue_g_memory(q), index);
	if (v4l_queue_is_planar(q)) {
		buf->buf.length = v4l_queue_g_num_planes(q);
		buf->buf.m.planes = buf->planes;
	}
	switch (q->memory) {
	case V4L2_MEMORY_USERPTR:
		for (p = 0; p < v4l_queue_g_num_planes(q); p++)
			v4l_buffer_s_userptr(buf, p, v4l_queue_g_userptr(q, index, p));
		break;
	case V4L2_MEMORY_DMABUF:
		for (p = 0; p < v4l_queue_g_num_planes(q); p++)
			v4l_buffer_s_fd(buf, p, v4l_queue_g_fd(q, index, p));
		break;
	default:
		break;
	}
}

#endif
