/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2022 Collabora Ltd.
 */

#include "trace.h"
#include <config.h> /* For PROMOTED_MODE_T */
#include <dlfcn.h>
#include <stdarg.h>

extern struct trace_context ctx_trace;

const std::list<unsigned long> ioctls = {
	VIDIOC_QUERYCAP,
	VIDIOC_STREAMON,
	VIDIOC_STREAMOFF,
	VIDIOC_ENUM_FMT,
	VIDIOC_G_FMT,
	VIDIOC_S_FMT,
	VIDIOC_REQBUFS,
	VIDIOC_QUERYBUF,
	VIDIOC_QBUF,
	VIDIOC_EXPBUF,
	VIDIOC_DQBUF,
	VIDIOC_G_PARM,
	VIDIOC_S_PARM,
	VIDIOC_ENUMINPUT,
	VIDIOC_G_CTRL,
	VIDIOC_S_CTRL,
	VIDIOC_QUERYCTRL,
	VIDIOC_G_INPUT,
	VIDIOC_S_INPUT,
	VIDIOC_G_OUTPUT,
	VIDIOC_S_OUTPUT,
	VIDIOC_ENUMOUTPUT,
	VIDIOC_G_CROP,
	VIDIOC_S_CROP,
	VIDIOC_TRY_FMT,
	VIDIOC_G_EXT_CTRLS,
	VIDIOC_S_EXT_CTRLS,
	VIDIOC_TRY_EXT_CTRLS,
	VIDIOC_ENCODER_CMD,
	VIDIOC_TRY_ENCODER_CMD,
	VIDIOC_CREATE_BUFS,
	VIDIOC_PREPARE_BUF,
	VIDIOC_G_SELECTION,
	VIDIOC_S_SELECTION,
	VIDIOC_DECODER_CMD,
	VIDIOC_TRY_DECODER_CMD,
	VIDIOC_QUERY_EXT_CTRL,
	MEDIA_IOC_REQUEST_ALLOC,
	MEDIA_REQUEST_IOC_QUEUE,
	MEDIA_REQUEST_IOC_REINIT,
};

int open(const char *path, int oflag, ...)
{
	errno = 0;
	mode_t mode = 0;

	if ((oflag & O_CREAT) != 0) {
		va_list argp;
		va_start(argp, oflag);
		mode = va_arg(argp, PROMOTED_MODE_T);
		va_end(argp);
	}

	int (*original_open)(const char *path, int oflag, ...) = nullptr;
	original_open = (int (*)(const char*, int, ...)) dlsym(RTLD_NEXT, "open");
	int fd = (*original_open)(path, oflag, mode);

	if (is_debug()) {
		fprintf(stderr, "%s:%s:%d: ", __FILE__, __func__, __LINE__);
		fprintf(stderr, "fd: %d, path: %s\n", fd, path);
	}

	if (getenv("V4L2_TRACER_PAUSE_TRACE") != nullptr)
		return fd;

	if (is_video_or_media_device(path)) {
		trace_open(fd, path, oflag, mode, false);
		add_device(fd, path);
	}

	if (is_debug()) {
		fprintf(stderr, "%s:%s:%d\n", __FILE__, __func__, __LINE__);
		print_devices();
	}

	return fd;
}

int open64(const char *path, int oflag, ...)
{
	errno = 0;
	mode_t mode = 0;
	if ((oflag & O_CREAT) != 0) {
		va_list argp;
		va_start(argp, oflag);
		mode = va_arg(argp, PROMOTED_MODE_T);
		va_end(argp);
	}

	int (*original_open64)(const char *path, int oflag, ...) = nullptr;
	original_open64 = (int (*)(const char*, int, ...)) dlsym(RTLD_NEXT, "open64");
	int fd = (*original_open64)(path, oflag, mode);

	if (is_debug()) {
		fprintf(stderr, "%s:%s:%d: ", __FILE__, __func__, __LINE__);
		fprintf(stderr, "fd: %d, path: %s\n", fd, path);
	}

	if (getenv("V4L2_TRACER_PAUSE_TRACE") != nullptr)
		return fd;

	if (is_video_or_media_device(path)) {
		add_device(fd, path);
		trace_open(fd, path, oflag, mode, true);
	}

	if (is_debug()) {
		fprintf(stderr, "%s:%s:%d\n", __FILE__, __func__, __LINE__);
		print_devices();
	}

	return fd;
}

int close(int fd)
{
	errno = 0;
	int (*original_close)(int fd) = nullptr;
	original_close = (int (*)(int)) dlsym(RTLD_NEXT, "close");

	if (getenv("V4L2_TRACER_PAUSE_TRACE") != nullptr)
		return (*original_close)(fd);

	std::string path = get_device(fd);
	if (is_debug()) {
		fprintf(stderr, "%s:%s:%d: ", __FILE__, __func__, __LINE__);
		fprintf(stderr, "fd: %d, path: %s\n", fd, path.c_str());
	}

	/* Only trace the close if a corresponding open was also traced. */
	if (!path.empty()) {
		json_object *close_obj = json_object_new_object();
		json_object_object_add(close_obj, "fd", json_object_new_int(fd));
		json_object_object_add(close_obj, "close", json_object_new_string(path.c_str()));
		write_json_object_to_json_file(close_obj);
		json_object_put(close_obj);
		ctx_trace.devices.erase(fd);

		/* If we removed the last device, close the json trace file. */
		if (!ctx_trace.devices.size())
			close_json_file();
	}

	if (is_debug()) {
		fprintf(stderr, "%s:%s:%d\n", __FILE__, __func__, __LINE__);
		print_devices();
	}

	return (*original_close)(fd);
}

void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
	errno = 0;
	void *(*original_mmap)(void *addr, size_t len, int prot, int flags, int fildes, off_t off) = nullptr;
	original_mmap = (void*(*)(void*, size_t, int, int, int, off_t)) dlsym(RTLD_NEXT, "mmap");
	void *buf_address_pointer = (*original_mmap)(addr, len, prot, flags, fildes, off);

	set_buffer_address_trace(fildes, off, (unsigned long) buf_address_pointer);

	if (buffer_in_trace_context(fildes, off))
		trace_mmap(addr, len, prot, flags, fildes, off, (unsigned long) buf_address_pointer, false);

	return buf_address_pointer;
}

void *mmap64(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
	errno = 0;
	void *(*original_mmap64)(void *addr, size_t len, int prot, int flags, int fildes, off_t off) = nullptr;
	original_mmap64 = (void*(*)(void*, size_t, int, int, int, off_t)) dlsym(RTLD_NEXT, "mmap64");
	void *buf_address_pointer = (*original_mmap64)(addr, len, prot, flags, fildes, off);

	set_buffer_address_trace(fildes, off, (unsigned long) buf_address_pointer);

	if (buffer_in_trace_context(fildes, off))
		trace_mmap(addr, len, prot, flags, fildes, off, (unsigned long) buf_address_pointer, true);

	return buf_address_pointer;
}

int munmap(void *start, size_t length)
{
	errno = 0;
	int(*original_munmap)(void *start, size_t length) = nullptr;
	original_munmap = (int(*)(void *, size_t)) dlsym(RTLD_NEXT, "munmap");
	int ret = (*original_munmap)(start, length);

	/* Only trace the unmapping if the original mapping was traced. */
	if (!buffer_is_mapped((unsigned long) start))
		return ret;

	json_object *munmap_obj = json_object_new_object();

	if (errno)
		json_object_object_add(munmap_obj, "errno", json_object_new_string(STRERR(errno)));

	json_object *munmap_args = json_object_new_object();
	json_object_object_add(munmap_args, "start", json_object_new_int64((int64_t)start));
	json_object_object_add(munmap_args, "length", json_object_new_uint64(length));
	json_object_object_add(munmap_obj, "munmap", munmap_args);

	write_json_object_to_json_file(munmap_obj);
	json_object_put(munmap_obj);

	return ret;
}

int ioctl(int fd, unsigned long cmd, ...)
{
	errno = 0;
	va_list argp;
	va_start(argp, cmd);
	void *arg = va_arg(argp, void *);
	va_end(argp);

	int (*original_ioctl)(int fd, unsigned long cmd, ...) = nullptr;
	original_ioctl = (int (*)(int, long unsigned int, ...)) dlsym(RTLD_NEXT, "ioctl");

	if (getenv("V4L2_TRACER_PAUSE_TRACE") != nullptr)
		return (*original_ioctl)(fd, cmd, arg);

	/* Don't trace ioctls that are not in the specified ioctls list. */
	if (find(ioctls.begin(), ioctls.end(), cmd) == ioctls.end())
		return (*original_ioctl)(fd, cmd, arg);

	json_object *ioctl_obj = json_object_new_object();
	json_object_object_add(ioctl_obj, "fd", json_object_new_int(fd));
	json_object_object_add(ioctl_obj, "ioctl",
	                       json_object_new_string(val2s(cmd, ioctl_val_def).c_str()));

	/* Don't attempt to trace a nullptr. */
	if (arg == nullptr) {
		int ret = (*original_ioctl)(fd, cmd, arg);
		if (errno)
			json_object_object_add(ioctl_obj, "errno",
			                       json_object_new_string(STRERR(errno)));
		write_json_object_to_json_file(ioctl_obj);
		json_object_put(ioctl_obj);
		return ret;
	}

	/* Get info needed for writing the decoded video data to a yuv file. */
	if (cmd == VIDIOC_S_EXT_CTRLS)
		s_ext_ctrls_setup(static_cast<struct v4l2_ext_controls*>(arg));
	if (cmd == VIDIOC_QBUF)
		qbuf_setup(static_cast<struct v4l2_buffer*>(arg));
	if (cmd == VIDIOC_STREAMOFF)
		streamoff_cleanup(*(static_cast<v4l2_buf_type*>(arg)));

	/* Trace userspace arguments if driver will be reading them i.e. _IOW or _IOWR ioctls */
	if ((cmd & IOC_IN) != 0U) {
		json_object *ioctl_args_userspace = trace_ioctl_args(cmd, arg);
		/* Some ioctls won't have arguments to trace e.g. MEDIA_REQUEST_IOC_QUEUE. */
		if (json_object_object_length(ioctl_args_userspace))
			json_object_object_add(ioctl_obj, "from_userspace", ioctl_args_userspace);
		else
			json_object_put(ioctl_args_userspace);
	}

	/* Make the original ioctl call. */
	int ret = (*original_ioctl)(fd, cmd, arg);

	if (errno)
		json_object_object_add(ioctl_obj, "errno", json_object_new_string(STRERR(errno)));

	/* Trace driver arguments if userspace will be reading them i.e. _IOR or _IOWR ioctls */
	if ((cmd & IOC_OUT) != 0U) {
		json_object *ioctl_args_driver = trace_ioctl_args(cmd, arg);
		/* Some ioctls won't have arguments to trace e.g. MEDIA_REQUEST_IOC_QUEUE. */
		if (json_object_object_length(ioctl_args_driver))
			json_object_object_add(ioctl_obj, "from_driver", ioctl_args_driver);
		else
			json_object_put(ioctl_args_driver);
	}

	write_json_object_to_json_file(ioctl_obj);
	json_object_put(ioctl_obj);

	/* Get additional info from driver for writing the decoded video data to a yuv file. */
	if (cmd == VIDIOC_G_FMT)
		g_fmt_setup_trace(static_cast<struct v4l2_format*>(arg));
	if (cmd == VIDIOC_S_FMT)
		s_fmt_setup(static_cast<struct v4l2_format*>(arg));
	if (cmd == VIDIOC_EXPBUF)
		expbuf_setup(static_cast<struct v4l2_exportbuffer*>(arg));
	if (cmd == VIDIOC_QUERYBUF)
		querybuf_setup(fd, static_cast<struct v4l2_buffer*>(arg));

	/* Get info needed for tracing dynamic arrays */
	if (cmd == VIDIOC_QUERY_EXT_CTRL)
		query_ext_ctrl_setup(fd, static_cast<struct v4l2_query_ext_ctrl*>(arg));

	return ret;
}
