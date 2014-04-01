#ifndef _CV4L_HELPERS_H_
#define _CV4L_HELPERS_H_

#include <v4l-helpers.h>

class cv4l_buffer;

class cv4l_queue : v4l_queue {
	friend class cv4l_buffer;
public:
	cv4l_queue(v4l_fd *_fd, unsigned type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
				unsigned memory = V4L2_MEMORY_MMAP)
	{
		fd = _fd;
		v4l_queue_init(this, type, memory);
	}
	virtual ~cv4l_queue()
	{
		if (fd->fd != -1)
			v4l_queue_free(fd, this);
	}

	void init(unsigned type, unsigned memory)
	{
		if (fd->fd != -1)
			v4l_queue_free(fd, this);
		v4l_queue_init(this, type, memory);
	}
	unsigned g_type() const { return v4l_queue_g_type(this); }
	unsigned g_memory() const { return v4l_queue_g_memory(this); }
	bool is_planar() const { return v4l_queue_is_planar(this); }
	bool is_output() const { return v4l_queue_is_output(this); }
	bool is_capture() const { return v4l_queue_is_capture(this); }
	bool is_video() const { return v4l_queue_is_video(this); }
	bool is_vbi() const { return v4l_queue_is_vbi(this); }
	bool is_sliced_vbi() const { return v4l_queue_is_sliced_vbi(this); }
	bool is_sdr() const { return v4l_queue_is_sdr(this); }
	unsigned g_buffers() const { return v4l_queue_g_buffers(this); }
	unsigned g_num_planes() const { return v4l_queue_g_num_planes(this); }
	unsigned g_length(unsigned plane) const { return v4l_queue_g_length(this, plane); }
	unsigned g_mem_offset(unsigned index, unsigned plane) const { return v4l_queue_g_mem_offset(this, index, plane); }
	void s_mmapping(unsigned index, unsigned plane, void *m) { v4l_queue_s_mmapping(this, index, plane, m); }
	void *g_mmapping(unsigned index, unsigned plane) const { return v4l_queue_g_mmapping(this, index, plane); }
	void s_userptr(unsigned index, unsigned plane, void *m) { v4l_queue_s_userptr(this, index, plane, m); }
	void *g_userptr(unsigned index, unsigned plane) const { return v4l_queue_g_userptr(this, index, plane); }
	void s_fd(unsigned index, unsigned plane, int fd) { v4l_queue_s_fd(this, index, plane, fd); }
	int g_fd(unsigned index, unsigned plane) const { return v4l_queue_g_fd(this, index, plane); }

	int reqbufs(unsigned count = 0)
	{
		return v4l_queue_reqbufs(fd, this, count);
	}
	bool has_create_bufs() const
	{
		return v4l_queue_has_create_bufs(fd, this);
	}
	int create_bufs(unsigned count, const v4l2_format *fmt = NULL)
	{
		return v4l_queue_create_bufs(fd, this, count, fmt);
	}
	int streamon()
	{
		return v4l_queue_streamon(fd, this);
	}
	int streamoff()
	{
		return v4l_queue_streamoff(fd, this);
	}
	int mmap_bufs(unsigned from = 0)
	{
		return v4l_queue_mmap_bufs(fd, this, from);
	}
	int munmap_bufs()
	{
		return v4l_queue_munmap_bufs(fd, this);
	}
	int alloc_bufs(unsigned from = 0)
	{
		return v4l_queue_alloc_bufs(fd, this, from);
	}
	int free_bufs()
	{
		return v4l_queue_free_bufs(this);
	}
	bool has_expbuf()
	{
		return v4l_queue_has_expbuf(fd);
	}
	int export_bufs()
	{
		return v4l_queue_export_bufs(fd, this);
	}
	void close_exported_fds()
	{
		v4l_queue_close_exported_fds(this);
	}
	void buffer_init(struct v4l_buffer *buf, unsigned index) const
	{
		v4l_queue_buffer_init(this, buf, index);
	}

protected:
	v4l_fd *fd;
};

class cv4l_buffer : public v4l_buffer {
public:
	cv4l_buffer(unsigned type = 0, unsigned memory = 0, unsigned index = 0)
	{
		init(type, memory, index);
	}
	cv4l_buffer(const cv4l_queue &q, unsigned index = 0)
	{
		init(q, index);
	}
	cv4l_buffer(const cv4l_buffer &b)
	{
		init(b);
	}
	virtual ~cv4l_buffer() {}
	void init(unsigned type = 0, unsigned memory = 0, unsigned index = 0)
	{
		v4l_buffer_init(this, type, memory, index);
	}
	void init(const cv4l_queue &q, unsigned index = 0)
	{
		q.buffer_init(this, index);
	}
	void init(const cv4l_buffer &b)
	{
		*this = b;
		if (is_planar())
			buf.m.planes = planes;
	}
	__u32 g_index() const { return v4l_buffer_g_index(this); }
	void s_index(unsigned index) { v4l_buffer_s_index(this, index); }
	unsigned g_type() const { return v4l_buffer_g_type(this); }
	unsigned g_memory() const { return v4l_buffer_g_memory(this); }
	__u32 g_flags() const { return v4l_buffer_g_flags(this); }
	void s_flags(__u32 flags) { v4l_buffer_s_flags(this, flags); }
	void or_flags(__u32 flags) { v4l_buffer_or_flags(this, flags); }
	unsigned g_field() const { return v4l_buffer_g_field(this); }
	void s_field(unsigned field) { v4l_buffer_s_field(this, field); }
	__u32 g_sequence() const { return v4l_buffer_g_sequence(this); }
	__u32 g_timestamp_type() const { return v4l_buffer_g_timestamp_type(this); }
	__u32 g_timestamp_src() const { return v4l_buffer_g_timestamp_src(this); }
	void s_timestamp_src(__u32 src) { v4l_buffer_s_timestamp_src(this, src); }
	bool ts_is_copy() const { return v4l_buffer_is_copy(this); }
	const timeval &g_timestamp() const { return *v4l_buffer_g_timestamp(this); }
	void s_timestamp(const timeval &tv) { v4l_buffer_s_timestamp(this, &tv); }
	void s_timestamp_ts(const timespec &ts) { v4l_buffer_s_timestamp_ts(this, &ts); }
	void s_timestamp_clock() { v4l_buffer_s_timestamp_clock(this); }
	const v4l2_timecode &g_timecode() const { return *v4l_buffer_g_timecode(this); }
	void s_timecode(const v4l2_timecode &tc) { v4l_buffer_s_timecode(this, &tc); }
	__u32 g_mem_offset(unsigned plane = 0) const
	{
		return v4l_buffer_g_mem_offset(this, plane);
	}
	void *g_userptr(unsigned plane = 0) const
	{
		return v4l_buffer_g_userptr(this, plane);
	}
	int g_fd(unsigned plane = 0) const
	{
		return v4l_buffer_g_fd(this, plane);
	}
	__u32 g_bytesused(unsigned plane = 0) const
	{
		return v4l_buffer_g_bytesused(this, plane);
	}
	__u32 g_length(unsigned plane = 0) const
	{
		return v4l_buffer_g_length(this, plane);
	}
	__u32 g_data_offset(unsigned plane = 0) const
	{
		return v4l_buffer_g_data_offset(this, plane);
	}
	void s_userptr(void *userptr, unsigned plane = 0)
	{
		v4l_buffer_s_userptr(this, plane, userptr);
	}
	void s_fd(int fd, unsigned plane = 0)
	{
		v4l_buffer_s_fd(this, plane, fd);
	}
	void s_bytesused(__u32 bytesused, unsigned plane = 0)
	{
		v4l_buffer_s_bytesused(this, plane, bytesused);
	}
	void s_data_offset(__u32 data_offset, unsigned plane = 0)
	{
		v4l_buffer_s_data_offset(this, plane, data_offset);
	}
	bool is_planar() const { return v4l_buffer_is_planar(this); }
	bool is_output() const { return v4l_buffer_is_output(this); }
	bool is_capture() const { return v4l_buffer_is_capture(this); }
	bool is_video() const { return v4l_buffer_is_video(this); }
	bool is_vbi() const { return v4l_buffer_is_vbi(this); }
	bool is_sliced_vbi() const { return v4l_buffer_is_sliced_vbi(this); }
	bool is_sdr() const { return v4l_buffer_is_sdr(this); }
	unsigned g_num_planes() const
	{
		return v4l_buffer_g_num_planes(this);
	}

	int querybuf(v4l_fd *fd, unsigned index)
	{
		return v4l_buffer_querybuf(fd, this, index);
	}
	int querybuf(const cv4l_queue &q, unsigned index)
	{
		return querybuf(q.fd, index);
	}
	int dqbuf(v4l_fd *fd)
	{
		return v4l_buffer_dqbuf(fd, this);
	}
	int dqbuf(const cv4l_queue &q)
	{
		return dqbuf(q.fd);
	}
	int qbuf(v4l_fd *fd)
	{
		return v4l_buffer_qbuf(fd, this);
	}
	int qbuf(const cv4l_queue &q)
	{
		return qbuf(q.fd);
	}
	int prepare_buf(v4l_fd *fd)
	{
		return v4l_buffer_prepare_buf(fd, this);
	}
	int prepare_buf(const cv4l_queue &q)
	{
		return prepare_buf(q.fd);
	}
};

#endif
