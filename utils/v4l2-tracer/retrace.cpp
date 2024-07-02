/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2022 Collabora Ltd.
 */

#include "retrace.h"

extern struct retrace_context ctx_retrace;

void retrace_mmap(json_object *mmap_obj, bool is_mmap64)
{
	json_object *mmap_args_obj;
	if (is_mmap64)
		json_object_object_get_ex(mmap_obj, "mmap64", &mmap_args_obj);
	else
		json_object_object_get_ex(mmap_obj, "mmap", &mmap_args_obj);

	json_object *len_obj;
	json_object_object_get_ex(mmap_args_obj, "len", &len_obj);
	size_t len = (size_t) json_object_get_int(len_obj);

	json_object *prot_obj;
	json_object_object_get_ex(mmap_args_obj, "prot", &prot_obj);
	int prot = json_object_get_int(prot_obj);

	json_object *flags_obj;
	json_object_object_get_ex(mmap_args_obj, "flags", &flags_obj);
	int flags = s2number(json_object_get_string(flags_obj));

	json_object *fildes_obj;
	json_object_object_get_ex(mmap_args_obj, "fildes", &fildes_obj);
	int fd_trace = json_object_get_int(fildes_obj);

	json_object *off_obj;
	json_object_object_get_ex(mmap_args_obj, "off", &off_obj);
	off_t off = (off_t) json_object_get_int64(off_obj);

	int fd_retrace = get_fd_retrace_from_fd_trace(fd_trace);
	if (fd_retrace < 0) {
		line_info("\n\tBad or missing file descriptor.");
		return;
	}

	/* Only retrace mmap calls that map a buffer. */
	if (!buffer_in_retrace_context(fd_retrace, off))
		return;

	void *buf_address_retrace_pointer = nullptr;
	if (is_mmap64)
		buf_address_retrace_pointer = mmap64(0, len, prot, flags, fd_retrace, off);
	else
		buf_address_retrace_pointer = mmap(0, len, prot, flags, fd_retrace, off);

	if (buf_address_retrace_pointer == MAP_FAILED) {
		if (is_mmap64)
			perror("mmap64");
		else
			perror("mmap");
		debug_line_info();
		print_context();
		exit(EXIT_FAILURE);
	}

	/*
	 * Get and store the original trace address so that it can be matched
	 * with the munmap address later.
	 */
	json_object *buffer_address_obj;
	json_object_object_get_ex(mmap_obj, "buffer_address", &buffer_address_obj);
	long buf_address_trace = json_object_get_int64(buffer_address_obj);
	set_buffer_address_retrace(fd_retrace, off, buf_address_trace,
	                           (long) buf_address_retrace_pointer);

	if (is_verbose() || (errno != 0)) {
		fprintf(stderr, "fd: %d, offset: %lld, ", fd_retrace, (long long)off);
		if (is_mmap64)
			perror("mmap64");
		else
			perror("mmap");

		debug_line_info();
		print_context();
	}
}

void retrace_munmap(json_object *syscall_obj)
{
	json_object *munmap_args_obj;
	json_object_object_get_ex(syscall_obj, "munmap", &munmap_args_obj);

	json_object *start_obj;
	json_object_object_get_ex(munmap_args_obj, "start", &start_obj);
	long start = json_object_get_int64(start_obj);

	json_object *length_obj;
	json_object_object_get_ex(munmap_args_obj, "length", &length_obj);
	size_t length = (size_t) json_object_get_int(length_obj);

	long buffer_address_retrace = get_retrace_address_from_trace_address(start);

	if (buffer_address_retrace < 0)
		return;

	munmap((void *)buffer_address_retrace, length);

	if (is_verbose() || (errno != 0)) {
		fprintf(stderr, "unmapped: %ld, ", buffer_address_retrace);
		perror("munmap");
	}
}

void retrace_open(json_object *jobj, bool is_open64)
{
	json_object *fd_trace_obj;
	json_object_object_get_ex(jobj, "fd", &fd_trace_obj);
	int fd_trace = json_object_get_int(fd_trace_obj);

	json_object *open_args_obj;
	if (is_open64)
		json_object_object_get_ex(jobj, "open64", &open_args_obj);
	else
		json_object_object_get_ex(jobj, "open", &open_args_obj);

	json_object *path_obj;
	std::string path_trace;
	json_object_object_get_ex(open_args_obj, "path", &path_obj);
	if (json_object_get_string(path_obj) != nullptr)
		path_trace = json_object_get_string(path_obj);
	std::string path_retrace = get_path_retrace_from_path_trace(path_trace, jobj);

	/*
	 * Don't fail if a retrace path can't be found.
	 * Try using the same path as in the trace file.
	 */
	if (path_retrace.empty()) {
		line_info("\n\tWarning: can't find retrace device.\
		          \n\tAttempting to use: %s", path_trace.c_str());
		path_retrace = path_trace;
	}

	json_object *oflag_obj;
	json_object_object_get_ex(open_args_obj, "oflag", &oflag_obj);
	int oflag = s2val(json_object_get_string(oflag_obj), open_val_def);

	int mode = 0;
	json_object *mode_obj;
	if (json_object_object_get_ex(open_args_obj, "mode", &mode_obj))
		mode = s2number(json_object_get_string(mode_obj));

	int fd_retrace = 0;
	if (is_open64)
		fd_retrace = open64(path_retrace.c_str(), oflag, mode);
	else
		fd_retrace = open(path_retrace.c_str(), oflag, mode);

	if (fd_retrace <= 0) {
		line_info("\n\tCan't open: %s", path_retrace.c_str());
		exit(fd_retrace);
	}

	add_fd(fd_trace, fd_retrace);

	if (is_verbose() || errno != 0) {
		fprintf(stderr, "path: %s ", path_retrace.c_str());
		if (is_open64)
			perror("open64");
		else
			perror("open");
		debug_line_info();
		print_context();
	}
}

void retrace_close(json_object *jobj)
{
	json_object *fd_trace_obj;
	json_object_object_get_ex(jobj, "fd", &fd_trace_obj);
	int fd_retrace = get_fd_retrace_from_fd_trace(json_object_get_int(fd_trace_obj));

	/* Only close devices that were opened in the retrace context. */
	if (fd_retrace < 0)
		return;

	close(fd_retrace);
	ctx_retrace.retrace_fds.erase(json_object_get_int(fd_trace_obj));

	if (is_verbose() || (errno != 0)) {
		fprintf(stderr, "fd: %d ", fd_retrace);
		perror("close");
		debug_line_info();
		print_context();
	}
}

void retrace_vidioc_reqbufs(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_requestbuffers *ptr = retrace_v4l2_requestbuffers_gen(ioctl_args);
	ioctl(fd_retrace, VIDIOC_REQBUFS, ptr);
	free(ptr);
}

struct v4l2_plane *retrace_v4l2_plane(json_object *plane_obj, __u32 memory)
{
	struct v4l2_plane *ptr = (struct v4l2_plane *) calloc(1, sizeof(v4l2_plane));

	json_object *bytesused_obj;
	json_object_object_get_ex(plane_obj, "bytesused", &bytesused_obj);
	ptr->bytesused = (__u32) json_object_get_int64(bytesused_obj);

	json_object *length_obj;
	json_object_object_get_ex(plane_obj, "length", &length_obj);
	ptr->length = (__u32) json_object_get_int64(length_obj);

	json_object *m_obj;
	json_object_object_get_ex(plane_obj, "m", &m_obj);
	if (memory == V4L2_MEMORY_MMAP) {
		json_object *mem_offset_obj;
		json_object_object_get_ex(m_obj, "mem_offset", &mem_offset_obj);
		ptr->m.mem_offset = (__u32) json_object_get_int64(mem_offset_obj);
	}

	json_object *data_offset_obj;
	json_object_object_get_ex(plane_obj, "data_offset", &data_offset_obj);
	ptr->data_offset = (__u32) json_object_get_int64(data_offset_obj);

	return ptr;
}

struct v4l2_buffer *retrace_v4l2_buffer(json_object *ioctl_args)
{
	struct v4l2_buffer *buf = (struct v4l2_buffer *) calloc(1, sizeof(struct v4l2_buffer));

	json_object *buf_obj;
	json_object_object_get_ex(ioctl_args, "v4l2_buffer", &buf_obj);

	json_object *index_obj;
	json_object_object_get_ex(buf_obj, "index", &index_obj);
	buf->index = (__u32) json_object_get_uint64(index_obj);

	json_object *type_obj;
	json_object_object_get_ex(buf_obj, "type", &type_obj);
	buf->type = s2val(json_object_get_string(type_obj), v4l2_buf_type_val_def);

	json_object *bytesused_obj;
	json_object_object_get_ex(buf_obj, "bytesused", &bytesused_obj);
	buf->bytesused = (__u32) json_object_get_uint64(bytesused_obj);

	json_object *flags_obj;
	json_object_object_get_ex(buf_obj, "flags", &flags_obj);
	buf->flags = (__u32) s2flags_buffer(json_object_get_string(flags_obj));

	json_object *field_obj;
	json_object_object_get_ex(buf_obj, "field", &field_obj);
	buf->field = (__u32) s2val(json_object_get_string(field_obj), v4l2_field_val_def);

	json_object *timestamp_obj;
	json_object_object_get_ex(buf_obj, "timestamp", &timestamp_obj);

	struct timeval tval = {};
	json_object *tv_sec_obj;
	json_object_object_get_ex(timestamp_obj, "tv_sec", &tv_sec_obj);
	tval.tv_sec = json_object_get_int64(tv_sec_obj);
	json_object *tv_usec_obj;
	json_object_object_get_ex(timestamp_obj, "tv_usec", &tv_usec_obj);
	tval.tv_usec = json_object_get_int64(tv_usec_obj);
	buf->timestamp = tval;

	json_object *sequence_obj;
	json_object_object_get_ex(buf_obj, "sequence", &sequence_obj);
	buf->sequence = (__u32) json_object_get_uint64(sequence_obj);

	json_object *memory_obj;
	json_object_object_get_ex(buf_obj, "memory", &memory_obj);
	buf->memory = s2val(json_object_get_string(memory_obj), v4l2_memory_val_def);

	json_object *length_obj;
	json_object_object_get_ex(buf_obj, "length", &length_obj);
	buf->length = (__u32) json_object_get_uint64(length_obj);

	json_object *m_obj;
	json_object_object_get_ex(buf_obj, "m", &m_obj);

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
	    buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		json_object *planes_obj;
		json_object_object_get_ex(m_obj, "planes", &planes_obj);
		 /* TODO add planes > 0 */
		json_object *plane_obj = json_object_array_get_idx(planes_obj, 0);
		buf->m.planes = retrace_v4l2_plane(plane_obj, buf->memory);
	}

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
	    buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		if (buf->memory == V4L2_MEMORY_MMAP) {
			json_object *offset_obj;
			json_object_object_get_ex(m_obj, "offset", &offset_obj);
			buf->m.offset = (__u32) json_object_get_uint64(offset_obj);
		}
	}

	if ((buf->flags & V4L2_BUF_FLAG_REQUEST_FD) != 0U) {
		json_object *request_fd_obj;
		json_object_object_get_ex(buf_obj, "request_fd", &request_fd_obj);
		buf->request_fd = (__s32) get_fd_retrace_from_fd_trace(json_object_get_int(request_fd_obj));
		if (buf->request_fd < 0)
			line_info("\n\tBad or missing file descriptor.\n");
	}

	return buf;
}

void retrace_vidioc_querybuf(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_buffer *buf = retrace_v4l2_buffer(ioctl_args);

	ioctl(fd_retrace, VIDIOC_QUERYBUF, buf);

	if (buf->memory == V4L2_MEMORY_MMAP) {
		__u32 offset = 0;
		if ((buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) ||
		    (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT))
			offset = buf->m.offset;
		if ((buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) ||
		    (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE))
			offset = buf->m.planes->m.mem_offset;
		if (get_buffer_fd_retrace(buf->type, buf->index) == -1)
			add_buffer_retrace(fd_retrace, buf->type, buf->index, offset);
	}

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
	    buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (buf->m.planes != nullptr) {
			free(buf->m.planes);
		}
	}

	if (is_verbose() || (errno != 0)) {
		fprintf(stderr, "%s, index: %d, fd: %d, ",
			val2s(buf->type, v4l2_buf_type_val_def).c_str(),
			buf->index, fd_retrace);
		perror("VIDIOC_QUERYBUF");
		debug_line_info();
		print_context();
	}

	free(buf);
}

void retrace_vidioc_qbuf(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_buffer *ptr = retrace_v4l2_buffer(ioctl_args);

	ioctl(fd_retrace, VIDIOC_QBUF, ptr);

	if (ptr->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
	    ptr->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (ptr->m.planes != nullptr) {
			free(ptr->m.planes);
		}
	}

	if (is_verbose() || (errno != 0)) {
		fprintf(stderr, "%s, index: %d, fd: %d, ",
		        val2s(ptr->type, v4l2_buf_type_val_def).c_str(),
		        ptr->index, fd_retrace);
		perror("VIDIOC_QBUF");
		debug_line_info();
		print_context();
	}

	free(ptr);
}

void retrace_vidioc_dqbuf(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_buffer *buf = retrace_v4l2_buffer(ioctl_args);

	const int poll_timeout_ms = 5000;
	struct pollfd *pfds = (struct pollfd *) calloc(1, sizeof(struct pollfd));
	if (pfds == nullptr)
		exit(EXIT_FAILURE);
	pfds[0].fd = fd_retrace;
	pfds[0].events = POLLIN;
	int ret = poll(pfds, 1, poll_timeout_ms);
	free(pfds);
	if (ret == -1) {
		line_info("\n\tPoll error.");
		perror("");
		exit(EXIT_FAILURE);
	}
	if (ret == 0) {
		line_info("\n\tPoll timed out.");
		exit(EXIT_FAILURE);
	}

	ioctl(fd_retrace, VIDIOC_DQBUF, buf);

	if (is_verbose() || (errno != 0)) {
		fprintf(stderr, "%s, index: %d, fd: %d, ",
		        val2s(buf->type, v4l2_buf_type_val_def).c_str(),
		        buf->index, fd_retrace);
		perror("VIDIOC_DQBUF");
		debug_line_info();
		print_context();
	}

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
	    buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			free(buf->m.planes);

	free(buf);
}

void retrace_vidioc_prepare_buf(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_buffer *buf = retrace_v4l2_buffer(ioctl_args);

	ioctl(fd_retrace, VIDIOC_PREPARE_BUF, buf);

	if (is_verbose() || (errno != 0)) {
		fprintf(stderr, "%s, index: %d, fd: %d, ",
		        val2s(buf->type, v4l2_buf_type_val_def).c_str(),
		        buf->index, fd_retrace);
		perror("VIDIOC_PREPARE_BUF");
		debug_line_info();
		print_context();
	}

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
	    buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			free(buf->m.planes);

	free(buf);
}

void retrace_vidioc_create_bufs(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_create_buffers *ptr = retrace_v4l2_create_buffers_gen(ioctl_args);
	ioctl(fd_retrace, VIDIOC_CREATE_BUFS, ptr);

	if (is_verbose() || (errno != 0)) {
		perror("VIDIOC_CREATE_BUFS");
		debug_line_info();
		print_context();
	}

	free(ptr);
}

void retrace_vidioc_expbuf(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_exportbuffer *ptr = retrace_v4l2_exportbuffer_gen(ioctl_args);
	ioctl(fd_retrace, VIDIOC_EXPBUF, ptr);

	int buf_fd_retrace = ptr->fd;

	/*
	 * If a buffer was previously added to the retrace context using the video device
	 * file descriptor, replace the video fd with the more specific buffer fd from EXPBUF.
	 */
	int fd_found_in_retrace_context = get_buffer_fd_retrace(ptr->type, ptr->index);
	if (fd_found_in_retrace_context != -1)
		remove_buffer_retrace(ptr->type, ptr->index);

	add_buffer_retrace(buf_fd_retrace, ptr->type, ptr->index);

	/* Retrace again to associate the original fd with the current buffer fd. */
	memset(ptr, 0, sizeof(v4l2_exportbuffer));
	ptr = retrace_v4l2_exportbuffer_gen(ioctl_args);
	int buf_fd_trace = ptr->fd;
	add_fd(buf_fd_trace, buf_fd_retrace);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_EXPBUF");

	free(ptr);
}

void retrace_vidioc_streamon(int fd_retrace, json_object *ioctl_args)
{
	json_object *type_obj;
	json_object_object_get_ex(ioctl_args, "type", &type_obj);
	v4l2_buf_type buf_type = (v4l2_buf_type) s2val(json_object_get_string(type_obj),
	                                               v4l2_buf_type_val_def);

	ioctl(fd_retrace, VIDIOC_STREAMON, &buf_type);

	if (is_verbose() || (errno != 0)) {
		fprintf(stderr, "%s, ", val2s(buf_type, v4l2_buf_type_val_def).c_str());
		perror("VIDIOC_STREAMON");
	}
}

void retrace_vidioc_streamoff(int fd_retrace, json_object *ioctl_args)
{
	json_object *type_obj;
	json_object_object_get_ex(ioctl_args, "type", &type_obj);
	v4l2_buf_type buf_type = (v4l2_buf_type) s2val(json_object_get_string(type_obj),
	                                               v4l2_buf_type_val_def);

	ioctl(fd_retrace, VIDIOC_STREAMOFF, &buf_type);

	if (is_verbose() || (errno != 0)) {
		fprintf(stderr, "%s, ", val2s(buf_type, v4l2_buf_type_val_def).c_str());
		perror("VIDIOC_STREAMOFF");
	}
}

void retrace_vidioc_try_fmt(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_format *ptr = retrace_v4l2_format_gen(ioctl_args);
	ioctl(fd_retrace, VIDIOC_TRY_FMT, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_TRY_FMT");

	free(ptr);
}

void retrace_vidioc_g_fmt(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_format *ptr = retrace_v4l2_format_gen(ioctl_args);
	ioctl(fd_retrace, VIDIOC_G_FMT, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_G_FMT");

	free(ptr);
}

void retrace_vidioc_s_fmt(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_format *ptr = retrace_v4l2_format_gen(ioctl_args);

	ioctl(fd_retrace, VIDIOC_S_FMT, ptr);

	if (is_verbose() || (errno != 0)) {
		perror("VIDIOC_S_FMT");
	}

	free(ptr);
}

struct v4l2_streamparm *retrace_v4l2_streamparm(json_object *parent_obj, std::string key_name = "")
{
	struct v4l2_streamparm *ptr = (struct v4l2_streamparm *) calloc(1, sizeof(v4l2_streamparm));

	json_object *v4l2_streamparm_obj;
	json_object_object_get_ex(parent_obj, "v4l2_streamparm", &v4l2_streamparm_obj);

	json_object *type_obj;
	if (json_object_object_get_ex(v4l2_streamparm_obj, "type", &type_obj))
		ptr->type = (__u32) s2val(json_object_get_string(type_obj), v4l2_buf_type_val_def);

	if ((ptr->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) || (ptr->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE))
		ptr->parm.capture = *retrace_v4l2_captureparm_gen(v4l2_streamparm_obj);

	if ((ptr->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) || (ptr->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE))
		ptr->parm.output = *retrace_v4l2_outputparm_gen(v4l2_streamparm_obj);

	return ptr;
}

void retrace_vidioc_g_parm (int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_streamparm *ptr = retrace_v4l2_streamparm(ioctl_args);
	ioctl(fd_retrace, VIDIOC_G_PARM, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_G_PARM");

	free(ptr);
}

void retrace_vidioc_s_parm (int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_streamparm *ptr = retrace_v4l2_streamparm(ioctl_args);
	ioctl(fd_retrace, VIDIOC_S_PARM, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_S_PARM");

	free(ptr);
}

void retrace_vidioc_queryctrl(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_queryctrl *ptr = retrace_v4l2_queryctrl_gen(ioctl_args);
	ioctl(fd_retrace, VIDIOC_QUERYCTRL, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_QUERYCTRL");

	free(ptr);
}

void retrace_vidioc_enuminput(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_input *ptr = retrace_v4l2_input_gen(ioctl_args);
	ioctl(fd_retrace, VIDIOC_ENUMINPUT, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_ENUMINPUT");

	free(ptr);
}

void retrace_vidioc_g_control(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_control *ptr = retrace_v4l2_control_gen(ioctl_args);
	ioctl(fd_retrace, VIDIOC_G_CTRL, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_G_CTRL");

	free(ptr);
}

void retrace_vidioc_s_control(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_control *ptr = retrace_v4l2_control_gen(ioctl_args);
	ioctl(fd_retrace, VIDIOC_S_CTRL, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_S_CTRL");

	free(ptr);
}

void retrace_vidioc_g_tuner(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_tuner *ptr = retrace_v4l2_tuner_gen(ioctl_args);
	ioctl(fd_retrace, VIDIOC_G_TUNER, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_G_TUNER");

	free(ptr);
}

void retrace_vidioc_s_tuner(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_tuner *ptr = retrace_v4l2_tuner_gen(ioctl_args);
	ioctl(fd_retrace, VIDIOC_S_TUNER, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_S_TUNER");

	free(ptr);
}

void retrace_vidioc_g_input(int fd_retrace, json_object *ioctl_args)
{
	int input = 0;
	ioctl(fd_retrace, VIDIOC_G_INPUT, &input);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_G_INPUT");
}

void retrace_vidioc_s_input(int fd_retrace, json_object *ioctl_args)
{
	int input = 0;
	json_object *input_obj;
	if (json_object_object_get_ex(ioctl_args, "input", &input_obj))
		input = json_object_get_int(input_obj);

	ioctl(fd_retrace, VIDIOC_S_INPUT, &input);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_S_INPUT");
}

void retrace_vidioc_g_output(int fd_retrace, json_object *ioctl_args)
{
	int output = 0;
	ioctl(fd_retrace, VIDIOC_G_OUTPUT, &output);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_G_OUTPUT");
}

void retrace_vidioc_s_output(int fd_retrace, json_object *ioctl_args)
{
	int output = 0;
	json_object *output_obj;
	if (json_object_object_get_ex(ioctl_args, "output", &output_obj))
		output = json_object_get_int(output_obj);

	ioctl(fd_retrace, VIDIOC_S_OUTPUT, &output);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_S_OUTPUT");
}

void retrace_vidioc_enumoutput(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_output *ptr = retrace_v4l2_output_gen(ioctl_args);
	ioctl(fd_retrace, VIDIOC_ENUMOUTPUT, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_ENUMOUTPUT");

	free(ptr);
}

void retrace_vidioc_g_crop(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_crop *ptr = retrace_v4l2_crop_gen(ioctl_args);
	ioctl(fd_retrace, VIDIOC_G_CROP, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_G_CROP");

	free(ptr);
}

void retrace_vidioc_s_crop(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_crop *ptr = retrace_v4l2_crop_gen(ioctl_args);
	ioctl(fd_retrace, VIDIOC_S_CROP, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_S_CROP");

	free(ptr);
}

int retrace_v4l2_ext_control_value(json_object *ctrl_obj, const val_def *def)
{
	__s32 value = -1;

	json_object *value_obj;
	if (json_object_object_get_ex(ctrl_obj, "value", &value_obj))
		value = (__s32) s2val(json_object_get_string(value_obj), def);

	return value;
}

__u32 *retrace_v4l2_dynamic_array(json_object *v4l2_ext_control_obj)
{
	__u32 elems = 0;
	json_object *elems_obj;
	if (json_object_object_get_ex(v4l2_ext_control_obj, "elems", &elems_obj))
		elems = (__u32) json_object_get_int64(elems_obj);

	__u32 *ptr = static_cast<__u32 *>(calloc(elems, sizeof(__u32)));
	if (ptr == nullptr) {
		line_info("\n\tMemory allocation failed.");
		return ptr;
	}

	json_object *p_u32_obj;
	if (json_object_object_get_ex(v4l2_ext_control_obj, "p_u32", &p_u32_obj)) {
		for (size_t i = 0; i < elems; i++) {
			if (json_object_array_get_idx(p_u32_obj, i))
				ptr[i] = (__u32) json_object_get_int64(json_object_array_get_idx(p_u32_obj, i));
		}
	}

	return ptr;
}

struct v4l2_ext_control *retrace_v4l2_ext_control(json_object *parent_obj, int ctrl_idx)
{
	struct v4l2_ext_control *p = (struct v4l2_ext_control *) calloc(1, sizeof(v4l2_ext_control));

	json_object *v4l2_ext_control_obj = json_object_array_get_idx(parent_obj, ctrl_idx);
	if (!v4l2_ext_control_obj)
		return p;

	json_object *id_obj;
	if (json_object_object_get_ex(v4l2_ext_control_obj, "id", &id_obj))
		p->id = s2val(json_object_get_string(id_obj), control_val_def);

	json_object *size_obj;
	if (json_object_object_get_ex(v4l2_ext_control_obj, "size", &size_obj))
		p->size = json_object_get_int64(size_obj);

	switch (p->id) {
	case V4L2_CID_STATELESS_H264_DECODE_MODE:
		p->value = retrace_v4l2_ext_control_value(v4l2_ext_control_obj,
		                                          v4l2_stateless_h264_decode_mode_val_def);
		break;
	case V4L2_CID_STATELESS_H264_START_CODE:
		p->value = retrace_v4l2_ext_control_value(v4l2_ext_control_obj,
		                                          v4l2_stateless_h264_start_code_val_def);
		break;
	case V4L2_CID_STATELESS_HEVC_DECODE_MODE:
		p->value = retrace_v4l2_ext_control_value(v4l2_ext_control_obj,
		                                          v4l2_stateless_hevc_decode_mode_val_def);
		break;
	case V4L2_CID_STATELESS_HEVC_START_CODE:
		p->value = retrace_v4l2_ext_control_value(v4l2_ext_control_obj,
		                                          v4l2_stateless_hevc_start_code_val_def);
		break;
	case V4L2_CID_MPEG_VIDEO_DEC_PTS:
	case V4L2_CID_MPEG_VIDEO_DEC_FRAME:
	case V4L2_CID_MPEG_VIDEO_DEC_CONCEAL_COLOR:
	case V4L2_CID_PIXEL_RATE: {
		json_object *value64_obj;

		if (json_object_object_get_ex(v4l2_ext_control_obj, "value64", &value64_obj))
			p->value64 = json_object_get_int64(value64_obj);
		break;
	}
	default:
		if (!p->size) {
			json_object *value_obj;

			if (json_object_object_get_ex(v4l2_ext_control_obj, "value", &value_obj))
				p->value = json_object_get_int(value_obj);
		}
		break;
	}

	/* Don't retrace pointers that were not traced because they were null. */
	if (p->size == 0)
		return p;

	switch (p->id) {
	case V4L2_CID_STATELESS_VP8_FRAME:
		p->ptr = retrace_v4l2_ctrl_vp8_frame_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_H264_SPS:
		p->ptr = retrace_v4l2_ctrl_h264_sps_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_H264_PPS:
		p->ptr = retrace_v4l2_ctrl_h264_pps_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_H264_SCALING_MATRIX:
		p->ptr = retrace_v4l2_ctrl_h264_scaling_matrix_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_H264_PRED_WEIGHTS:
		p->ptr = retrace_v4l2_ctrl_h264_pred_weights_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_H264_SLICE_PARAMS:
		p->ptr = retrace_v4l2_ctrl_h264_slice_params_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_H264_DECODE_PARAMS:
		p->ptr = retrace_v4l2_ctrl_h264_decode_params_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_FWHT_PARAMS:
		p->ptr = retrace_v4l2_ctrl_fwht_params_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_VP9_FRAME:
		p->ptr = retrace_v4l2_ctrl_vp9_frame_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_VP9_COMPRESSED_HDR:
		p->ptr = retrace_v4l2_ctrl_vp9_compressed_hdr_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_HEVC_SPS:
		p->ptr = retrace_v4l2_ctrl_hevc_sps_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_HEVC_PPS:
		p->ptr = retrace_v4l2_ctrl_hevc_pps_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_HEVC_SLICE_PARAMS:
		p->ptr = retrace_v4l2_ctrl_hevc_slice_params_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_HEVC_SCALING_MATRIX:
		p->ptr = retrace_v4l2_ctrl_hevc_scaling_matrix_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_HEVC_DECODE_PARAMS:
		p->ptr = retrace_v4l2_ctrl_hevc_decode_params_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_HEVC_ENTRY_POINT_OFFSETS:
		p->p_u32 = retrace_v4l2_dynamic_array(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_MPEG2_SEQUENCE:
		p->ptr = retrace_v4l2_ctrl_mpeg2_sequence_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_MPEG2_PICTURE:
		p->ptr = retrace_v4l2_ctrl_mpeg2_picture_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_MPEG2_QUANTISATION:
		p->ptr = retrace_v4l2_ctrl_mpeg2_quantisation_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_AV1_SEQUENCE:
		p->ptr = retrace_v4l2_ctrl_av1_sequence_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_AV1_TILE_GROUP_ENTRY:
		p->ptr = retrace_v4l2_ctrl_av1_tile_group_entry_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_AV1_FRAME:
		p->ptr = retrace_v4l2_ctrl_av1_frame_gen(v4l2_ext_control_obj);
		break;
	case V4L2_CID_STATELESS_AV1_FILM_GRAIN:
		p->ptr = retrace_v4l2_ctrl_av1_film_grain_gen(v4l2_ext_control_obj);
		break;
	default:
		line_info("\n\tWarning: cannot retrace control: %s",
		          val2s(p->id, control_val_def).c_str());
		break;
	}

	return p;
}

struct v4l2_ext_controls *retrace_v4l2_ext_controls(json_object *parent_obj)
{
	struct v4l2_ext_controls *ptr = (struct v4l2_ext_controls *) calloc(1, sizeof(v4l2_ext_controls));

	json_object *v4l2_ext_controls_obj;
	json_object_object_get_ex(parent_obj, "v4l2_ext_controls", &v4l2_ext_controls_obj);

	json_object *which_obj;
	if (json_object_object_get_ex(v4l2_ext_controls_obj, "which", &which_obj))
		ptr->which = (__u32) s2val(json_object_get_string(which_obj), which_val_def);

	json_object *count_obj;
	if (json_object_object_get_ex(v4l2_ext_controls_obj, "count", &count_obj))
		ptr->count = (__u32) json_object_get_int64(count_obj);

	json_object *error_idx_obj;
	if (json_object_object_get_ex(v4l2_ext_controls_obj, "error_idx", &error_idx_obj))
		ptr->error_idx = (__u32) json_object_get_int64(error_idx_obj);

	/* request_fd is only valid for V4L2_CTRL_WHICH_REQUEST_VAL */
	if (ptr->which == V4L2_CTRL_WHICH_REQUEST_VAL) {
		json_object *request_fd_obj;
		if (json_object_object_get_ex(v4l2_ext_controls_obj, "request_fd", &request_fd_obj)) {

			int request_fd_trace = json_object_get_int(request_fd_obj);
			int request_fd_retrace = get_fd_retrace_from_fd_trace(request_fd_trace);
			if (request_fd_retrace < 0) {
				line_info("\n\tBad file descriptor.");
				return ptr;
			}
			ptr->request_fd = (__s32) request_fd_retrace;
		}
	}

	json_object *controls_obj;
	if (json_object_object_get_ex(v4l2_ext_controls_obj, "controls", &controls_obj)) {
		ptr->controls = (struct v4l2_ext_control *) calloc(ptr->count, sizeof(v4l2_ext_control));
		for (__u32 i = 0; i < ptr->count; i++) {
			void *temp = retrace_v4l2_ext_control(controls_obj, i);
			if (temp != nullptr) {
				ptr->controls[i] = *(static_cast<struct v4l2_ext_control*>(temp));
				free(temp);
			}
		}
	}

	return ptr;
}

void retrace_vidioc_try_ext_ctrls(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_ext_controls *ptr = retrace_v4l2_ext_controls(ioctl_args);
	ioctl(fd_retrace, VIDIOC_TRY_EXT_CTRLS, ptr);

	free(ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_TRY_EXT_CTRLS");
}

void retrace_vidioc_g_ext_ctrls(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_ext_controls *ptr = retrace_v4l2_ext_controls(ioctl_args);
	ioctl(fd_retrace, VIDIOC_G_EXT_CTRLS, ptr);

	free(ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_G_EXT_CTRLS");
}

void retrace_vidioc_s_ext_ctrls(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_ext_controls *ptr = retrace_v4l2_ext_controls(ioctl_args);
	ioctl(fd_retrace, VIDIOC_S_EXT_CTRLS, ptr);

	free(ptr);

	if (is_verbose() || (errno != 0)) {
		perror("VIDIOC_S_EXT_CTRLS");
		debug_line_info();
		print_context();
	}
}

void retrace_vidioc_enum_framesizes(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_frmsizeenum *ptr = retrace_v4l2_frmsizeenum_gen(ioctl_args);

	ioctl(fd_retrace, VIDIOC_ENUM_FRAMESIZES, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_ENUM_FRAMESIZES");

	free(ptr);
}

void retrace_vidioc_enum_frameintervals(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_frmivalenum *ptr = retrace_v4l2_frmivalenum_gen(ioctl_args);

	ioctl(fd_retrace, VIDIOC_ENUM_FRAMEINTERVALS, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_ENUM_FRAMEINTERVALS");

	free(ptr);
}


void retrace_vidioc_try_encoder_cmd(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_encoder_cmd *ptr = retrace_v4l2_encoder_cmd_gen(ioctl_args);

	ioctl(fd_retrace, VIDIOC_TRY_ENCODER_CMD, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_TRY_ENCODER_CMD");

	free(ptr);
}

void retrace_vidioc_encoder_cmd(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_encoder_cmd *ptr = retrace_v4l2_encoder_cmd_gen(ioctl_args);

	ioctl(fd_retrace, VIDIOC_ENCODER_CMD, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_ENCODER_CMD");

	free(ptr);
}

void retrace_vidioc_g_selection(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_selection *ptr = retrace_v4l2_selection_gen(ioctl_args);

	ioctl(fd_retrace, VIDIOC_G_SELECTION, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_G_SELECTION");

	free(ptr);

}

void retrace_vidioc_s_selection(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_selection *ptr = retrace_v4l2_selection_gen(ioctl_args);

	ioctl(fd_retrace, VIDIOC_S_SELECTION, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_S_SELECTION");

	free(ptr);
}

struct v4l2_decoder_cmd *retrace_v4l2_decoder_cmd(json_object *parent_obj)
{
	struct v4l2_decoder_cmd *ptr = (struct v4l2_decoder_cmd *) calloc(1, sizeof(v4l2_decoder_cmd));

	json_object *v4l2_decoder_cmd_obj;
	json_object_object_get_ex(parent_obj, "v4l2_decoder_cmd", &v4l2_decoder_cmd_obj);

	json_object *cmd_obj;
	/* Since V4L2_DEC_CMD_START is 0, an empty key will be retraced as V4L2_DEC_CMD_START. */
	if (json_object_object_get_ex(v4l2_decoder_cmd_obj, "cmd", &cmd_obj))
		ptr->cmd = (__u32) s2val(json_object_get_string(cmd_obj), decoder_cmd_val_def);

	std::string flags;
	json_object *flags_obj;
	if (json_object_object_get_ex(v4l2_decoder_cmd_obj, "flags", &flags_obj))
		if (json_object_get_string(flags_obj) != nullptr)
			flags = json_object_get_string(flags_obj);

	switch (ptr->cmd) {
	case V4L2_DEC_CMD_START: {
		ptr->flags = s2flags(flags.c_str(), v4l2_decoder_cmd_start_flag_def);

		json_object *start_obj;
		json_object_object_get_ex(v4l2_decoder_cmd_obj, "start", &start_obj);

		json_object *speed_obj;
		if (json_object_object_get_ex(start_obj, "speed", &speed_obj))
			ptr->start.speed = json_object_get_int(speed_obj);

		std::string format;
		json_object *format_obj;
		if (json_object_object_get_ex(start_obj, "format", &format_obj))
			if (json_object_get_string(format_obj) != nullptr)
				format = json_object_get_string(format_obj);

		if (format == "V4L2_DEC_START_FMT_GOP")
			ptr->start.format = V4L2_DEC_START_FMT_GOP;
		else if (format == "V4L2_DEC_START_FMT_NONE")
			ptr->start.format = V4L2_DEC_START_FMT_NONE;
		break;
	}
	case V4L2_DEC_CMD_STOP: {
		ptr->flags = s2flags(flags.c_str(), v4l2_decoder_cmd_stop_flag_def);

		json_object *stop_obj;
		json_object_object_get_ex(v4l2_decoder_cmd_obj, "stop", &stop_obj);

		json_object *pts_obj;
		if (json_object_object_get_ex(stop_obj, "pts", &pts_obj))
			ptr->stop.pts = (__u64) json_object_get_uint64(pts_obj);
		break;
	}
	case V4L2_DEC_CMD_PAUSE: {
		ptr->flags = s2flags(flags.c_str(), v4l2_decoder_cmd_pause_flag_def);
		break;
	}
	default:
		break;
	}

	return ptr;
}

void retrace_vidioc_try_decoder_cmd(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_decoder_cmd *ptr = retrace_v4l2_decoder_cmd(ioctl_args);

	ioctl(fd_retrace, VIDIOC_TRY_DECODER_CMD, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_TRY_DECODER_CMD");

	free (ptr);
}

void retrace_vidioc_dqevent(int fd_retrace)
{
	const int poll_timeout_ms = 200;
	struct pollfd *pfds = (struct pollfd *) calloc(1, sizeof(struct pollfd));
	if (pfds == nullptr)
		exit(EXIT_FAILURE);
	pfds[0].fd = fd_retrace;
	pfds[0].events = POLLIN | POLLPRI;
	int ret = poll(pfds, 1, poll_timeout_ms);

	if (ret == -1) {
		line_info("\n\tPoll error.");
		perror("");
		free(pfds);
		exit(EXIT_FAILURE);
	}
	if (ret == 0) {
		line_info("\n\tPoll timed out.");
		free(pfds);
		exit(EXIT_FAILURE);
	}

	struct v4l2_event event = {};
	ioctl(fd_retrace, VIDIOC_DQEVENT, &event);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_DQEVENT");

	free(pfds);
}

void retrace_vidioc_subscribe_event(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_event_subscription *ptr = retrace_v4l2_event_subscription_gen(ioctl_args);

	ioctl(fd_retrace, VIDIOC_SUBSCRIBE_EVENT, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_SUBSCRIBE_EVENT");

	free (ptr);
}

void retrace_vidioc_unsubscribe(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_event_subscription *ptr = retrace_v4l2_event_subscription_gen(ioctl_args);

	ioctl(fd_retrace, VIDIOC_UNSUBSCRIBE_EVENT, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_UNSUBSCRIBE_EVENT");

	free (ptr);
}

void retrace_vidioc_decoder_cmd(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_decoder_cmd *ptr = retrace_v4l2_decoder_cmd(ioctl_args);

	ioctl(fd_retrace, VIDIOC_DECODER_CMD, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_DECODER_CMD");

	free (ptr);
}

void retrace_vidioc_query_ext_ctrl(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_query_ext_ctrl *ptr = retrace_v4l2_query_ext_ctrl_gen(ioctl_args);

	ioctl(fd_retrace, VIDIOC_QUERY_EXT_CTRL, ptr);

	if (is_verbose())
		perror("VIDIOC_QUERY_EXT_CTRL");

	free(ptr);
}

void retrace_vidioc_enum_fmt(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_fmtdesc *ptr = retrace_v4l2_fmtdesc_gen(ioctl_args);

	ioctl(fd_retrace, VIDIOC_ENUM_FMT, ptr);

	if (is_verbose())
		perror("VIDIOC_ENUM_FMT");

	free(ptr);
}

void retrace_vidioc_querycap(int fd_retrace, json_object *ioctl_args)
{
	struct v4l2_capability *ptr = retrace_v4l2_capability_gen(ioctl_args);

	ioctl(fd_retrace, VIDIOC_QUERYCAP, ptr);

	if (is_verbose() || (errno != 0))
		perror("VIDIOC_QUERYCAP");

	free(ptr);
}

void retrace_media_ioc_request_alloc(int fd_retrace, json_object *ioctl_args)
{
	/* Get the original request file descriptor from the original trace file. */
	json_object *request_fd_trace_obj;
	json_object_object_get_ex(ioctl_args, "request_fd", &request_fd_trace_obj);
	int request_fd_trace = json_object_get_int(request_fd_trace_obj);

	/* Allocate a request in the retrace context. */
	__s32 request_fd_retrace = 0;
	ioctl(fd_retrace, MEDIA_IOC_REQUEST_ALLOC, &request_fd_retrace);

	/* Associate the original request file descriptor with the current request file descriptor. */
	add_fd(request_fd_trace, request_fd_retrace);
}

void retrace_ioctl(json_object *syscall_obj)
{
	__s64 cmd = 0;
	int fd_retrace = 0;

	json_object *fd_trace_obj;
	json_object_object_get_ex(syscall_obj, "fd", &fd_trace_obj);
	fd_retrace = get_fd_retrace_from_fd_trace(json_object_get_int(fd_trace_obj));

	json_object *cmd_obj;
	json_object_object_get_ex(syscall_obj, "ioctl", &cmd_obj);
	cmd = s2val(json_object_get_string(cmd_obj), ioctl_val_def);

	json_object *errno_obj;
	if (json_object_object_get_ex(syscall_obj, "errno", &errno_obj))
		return;

	if (fd_retrace < 0) {
		line_info("\n\tBad file descriptor on %s\n", json_object_get_string(cmd_obj));
		return;
	}

	/* If available, use the ioctl arguments from userspace, otherwise use the driver arguments. */
	json_object *ioctl_args;
	if (json_object_object_get_ex(syscall_obj, "from_userspace", &ioctl_args) == false)
		json_object_object_get_ex(syscall_obj, "from_driver", &ioctl_args);

	switch (cmd) {
	case VIDIOC_QUERYCAP:
		retrace_vidioc_querycap(fd_retrace, ioctl_args);
		break;
	case VIDIOC_ENUM_FMT:
		retrace_vidioc_enum_fmt(fd_retrace, ioctl_args);
		break;
	case VIDIOC_TRY_FMT:
		retrace_vidioc_try_fmt(fd_retrace, ioctl_args);
		break;
	case VIDIOC_G_FMT:
		retrace_vidioc_g_fmt(fd_retrace, ioctl_args);
		break;
	case VIDIOC_S_FMT:
		retrace_vidioc_s_fmt(fd_retrace, ioctl_args);
		break;
	case VIDIOC_REQBUFS:
		retrace_vidioc_reqbufs(fd_retrace, ioctl_args);
		break;
	case VIDIOC_QUERYBUF:
		retrace_vidioc_querybuf(fd_retrace, ioctl_args);
		break;
	case VIDIOC_QBUF:
		retrace_vidioc_qbuf(fd_retrace, ioctl_args);
		break;
	case VIDIOC_EXPBUF:
		retrace_vidioc_expbuf(fd_retrace, ioctl_args);
		break;
	case VIDIOC_DQBUF:
		retrace_vidioc_dqbuf(fd_retrace, ioctl_args);
		break;
	case VIDIOC_STREAMON:
		retrace_vidioc_streamon(fd_retrace, ioctl_args);
		break;
	case VIDIOC_STREAMOFF:
		retrace_vidioc_streamoff(fd_retrace, ioctl_args);
		break;
	case VIDIOC_G_PARM:
		retrace_vidioc_g_parm(fd_retrace, ioctl_args);
		break;
	case VIDIOC_S_PARM:
		retrace_vidioc_s_parm(fd_retrace, ioctl_args);
		break;
	case VIDIOC_ENUMINPUT:
		retrace_vidioc_enuminput(fd_retrace, ioctl_args);
		break;
	case VIDIOC_G_CTRL:
		retrace_vidioc_g_control(fd_retrace, ioctl_args);
		break;
	case VIDIOC_S_CTRL:
		retrace_vidioc_s_control(fd_retrace, ioctl_args);
		break;
	case VIDIOC_G_TUNER:
		retrace_vidioc_g_tuner(fd_retrace, ioctl_args);
		break;
	case VIDIOC_S_TUNER:
		retrace_vidioc_s_tuner(fd_retrace, ioctl_args);
		break;
	case VIDIOC_QUERYCTRL:
		retrace_vidioc_queryctrl(fd_retrace, ioctl_args);
		break;
	case VIDIOC_G_INPUT:
		retrace_vidioc_g_input(fd_retrace, ioctl_args);
		break;
	case VIDIOC_S_INPUT:
		retrace_vidioc_s_input(fd_retrace, ioctl_args);
		break;
	case VIDIOC_G_OUTPUT:
		retrace_vidioc_g_output(fd_retrace, ioctl_args);
		break;
	case VIDIOC_S_OUTPUT:
		retrace_vidioc_s_output(fd_retrace, ioctl_args);
		break;
	case VIDIOC_ENUMOUTPUT:
		retrace_vidioc_enumoutput(fd_retrace, ioctl_args);
		break;
	case VIDIOC_G_CROP:
		retrace_vidioc_g_crop(fd_retrace, ioctl_args);
		break;
	case VIDIOC_S_CROP:
		retrace_vidioc_s_crop(fd_retrace, ioctl_args);
		break;
	case VIDIOC_G_EXT_CTRLS:
		retrace_vidioc_g_ext_ctrls(fd_retrace, ioctl_args);
		break;
	case VIDIOC_TRY_EXT_CTRLS:
		retrace_vidioc_try_ext_ctrls(fd_retrace, ioctl_args);
		break;
	case VIDIOC_S_EXT_CTRLS:
		retrace_vidioc_s_ext_ctrls(fd_retrace, ioctl_args);
		break;
	case VIDIOC_ENUM_FRAMESIZES:
		retrace_vidioc_enum_framesizes(fd_retrace, ioctl_args);
		break;
	case VIDIOC_ENUM_FRAMEINTERVALS:
		retrace_vidioc_enum_frameintervals(fd_retrace, ioctl_args);
		break;
	case VIDIOC_TRY_ENCODER_CMD:
		retrace_vidioc_try_encoder_cmd(fd_retrace, ioctl_args);
		break;
	case VIDIOC_ENCODER_CMD:
		retrace_vidioc_encoder_cmd(fd_retrace, ioctl_args);
		break;
	case VIDIOC_CREATE_BUFS:
		retrace_vidioc_create_bufs(fd_retrace, ioctl_args);
		break;
	case VIDIOC_G_SELECTION:
		retrace_vidioc_g_selection(fd_retrace, ioctl_args);
		break;
	case VIDIOC_S_SELECTION:
		retrace_vidioc_s_selection(fd_retrace, ioctl_args);
		break;
	case VIDIOC_PREPARE_BUF:
		retrace_vidioc_prepare_buf(fd_retrace, ioctl_args);
		break;
	case VIDIOC_TRY_DECODER_CMD:
		retrace_vidioc_try_decoder_cmd(fd_retrace, ioctl_args);
		break;
	case VIDIOC_DQEVENT:
		retrace_vidioc_dqevent(fd_retrace);
		break;
	case VIDIOC_SUBSCRIBE_EVENT:
		retrace_vidioc_subscribe_event(fd_retrace, ioctl_args);
		break;
	case VIDIOC_UNSUBSCRIBE_EVENT:
		retrace_vidioc_unsubscribe(fd_retrace, ioctl_args);
		break;
	case VIDIOC_DECODER_CMD:
		retrace_vidioc_decoder_cmd(fd_retrace, ioctl_args);
		break;
	case VIDIOC_QUERY_EXT_CTRL:
		retrace_vidioc_query_ext_ctrl(fd_retrace, ioctl_args);
		break;
	case MEDIA_IOC_REQUEST_ALLOC:
		retrace_media_ioc_request_alloc(fd_retrace, ioctl_args);
		break;
	case MEDIA_REQUEST_IOC_QUEUE:
		ioctl(fd_retrace, MEDIA_REQUEST_IOC_QUEUE);
		break;
	case MEDIA_REQUEST_IOC_REINIT:
		ioctl(fd_retrace, MEDIA_REQUEST_IOC_REINIT);
		break;
	default:
		if (json_object_get_string(cmd_obj) != nullptr)
			line_info("\n\tWarning: cannot retrace ioctl: \'%s\'\n",
			          json_object_get_string(cmd_obj));
		else
			line_info("\n\tWarning: cannot retrace ioctl.");
		break;
	}
}

void retrace_mem(json_object *mem_obj)
{
	json_object *type_obj;
	json_object_object_get_ex(mem_obj, "mem_dump", &type_obj);
	v4l2_buf_type type = (v4l2_buf_type) s2val(json_object_get_string(type_obj),
	                                           v4l2_buf_type_val_def);
	json_object *bytesused_obj;
	json_object_object_get_ex(mem_obj, "bytesused", &bytesused_obj);
	int bytesused = json_object_get_int64(bytesused_obj);
	if (bytesused == 0)
		return;

	json_object *offset_obj;
	json_object_object_get_ex(mem_obj, "offset", &offset_obj);
	__u32 offset = json_object_get_int64(offset_obj);

	json_object *address_obj;
	json_object_object_get_ex(mem_obj, "address", &address_obj);
	long buffer_address_trace = json_object_get_int64(address_obj);

	long buffer_address_retrace = get_retrace_address_from_trace_address(buffer_address_trace);

	unsigned char *buffer_pointer = (unsigned char *) buffer_address_retrace;

	/* Get the encoded data from the json file and write it to output buffer memory. */
	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE || type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		write_to_output_buffer(buffer_pointer, bytesused, mem_obj);

	debug_line_info("\n\t%s, bytesused: %d, offset: %d, addr: %ld",
			val2s(type, v4l2_buf_type_val_def).c_str(),
			bytesused, offset, buffer_address_retrace);
	print_context();
}

void retrace_object(json_object *jobj)
{
	errno = 0;
	json_object *temp_obj;
	if (json_object_object_get_ex(jobj, "ioctl", &temp_obj)) {
		retrace_ioctl(jobj);
		return;
	}

	if (json_object_object_get_ex(jobj, "open", &temp_obj)) {
		retrace_open(jobj, false);
		return;
	}

	if (json_object_object_get_ex(jobj, "open64", &temp_obj)) {
		retrace_open(jobj, true);
		return;
	}

	if (json_object_object_get_ex(jobj, "close", &temp_obj)) {
		retrace_close(jobj);
		return;
	}

	if (json_object_object_get_ex(jobj, "mmap", &temp_obj)) {
		retrace_mmap(jobj, false);
		return;
	}

	if (json_object_object_get_ex(jobj, "mmap64", &temp_obj)) {
		retrace_mmap(jobj, true);
		return;
	}

	if (json_object_object_get_ex(jobj, "munmap", &temp_obj)) {
		retrace_munmap(jobj);
		return;
	}

	if (json_object_object_get_ex(jobj, "mem_dump", &temp_obj)) {
		retrace_mem(jobj);
		return;
	}

	if (json_object_object_get_ex(jobj, "package_version", &temp_obj)) {
		compare_program_versions(jobj);
		return;
	}

	if (json_object_object_get_ex(jobj, "Trace", &temp_obj)) {
		return;
	}
	if (json_object_object_get_ex(jobj, "write", &temp_obj)) {
		return;
	}
	line_info("\n\tWarning: unexpected JSON object in trace file.");
}

void retrace_array(json_object *root_array_obj)
{
	json_object *jobj;
	struct array_list *array_list_pointer = json_object_get_array(root_array_obj);
	size_t json_objects_in_file = array_list_length(array_list_pointer);

	if (json_objects_in_file < 3)
		line_info("\n\tWarning: trace file may be empty.");

	for (size_t i = 0; i < json_objects_in_file; i++) {
		jobj = (json_object *) array_list_get_idx(array_list_pointer, i);
		retrace_object(jobj);
	}
}

int retrace(std::string trace_filename)
{
	struct stat sb;
	if (stat(trace_filename.c_str(), &sb) == -1) {
		line_info("\n\tTrace file error: \'%s\'", trace_filename.c_str());
		return -EINVAL;
	}

	fprintf(stderr, "Retracing: %s\n", trace_filename.c_str());

	json_object *root_array_obj = json_object_from_file(trace_filename.c_str());

	if (root_array_obj == nullptr) {
		line_info("\n\t%s\tCan't get JSON-object from file: %s",
			  json_util_get_last_err(),
			  trace_filename.c_str());
		return 1;
	}

	retrace_array(root_array_obj);
	json_object_put(root_array_obj);

	return 0;
}
