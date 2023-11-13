/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2022 Collabora Ltd.
 */

#ifndef TRACE_H
#define TRACE_H

#include "v4l2-tracer-common.h"
#include "trace-gen.h"

struct buffer_trace {
	int fd;
	__u32 type;
	__u32 index;
	__u32 offset;
	__u32 bytesused;
	long display_order;
	unsigned long address;
};

struct h264_info {
	int pic_order_cnt_lsb;
	int max_pic_order_cnt_lsb;
};

struct trace_context {
	__u32 elems;
	__u32 width;
	__u32 height;
	FILE *trace_file;
	__u32 pixelformat;
	std::string media_device;
	__u32 compression_format;
	union {
		struct h264_info h264;
	} fmt;
	std::string trace_filename;
	std::list<long> decode_order;
	std::list<struct buffer_trace> buffers;
	std::unordered_map<int, std::string> devices; /* key:fd, value: path of the device */
};

void trace_open(int fd, const char *path, int oflag, mode_t mode, bool is_open64);
void trace_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off, unsigned long buf_address, bool is_mmap64);
void trace_mem(int fd, __u32 offset, __u32 type, int index, __u32 bytesused, unsigned long start);
void trace_mem_encoded(int fd, __u32 offset);
void trace_mem_decoded(void);
json_object *trace_ioctl_args(unsigned long cmd, void *arg);

bool is_video_or_media_device(const char *path);
void add_device(int fd, std::string path);
std::string get_device(int fd);
void print_devices(void);
bool buffer_in_trace_context(int fd, __u32 offset = 0);
__u32 get_buffer_type_trace(int fd, __u32 offset = 0);
int get_buffer_index_trace(int fd, __u32 offset);
long get_buffer_bytesused_trace(int fd, __u32 offset);
void set_buffer_address_trace(int fd, __u32 offset, unsigned long address);
unsigned long get_buffer_address_trace(int fd, __u32 offset);
bool buffer_is_mapped(unsigned long buffer_address);
unsigned get_expected_length_trace(void);
void s_ext_ctrls_setup(struct v4l2_ext_controls *ext_controls);
void qbuf_setup(struct v4l2_buffer *buf);
void dqbuf_setup(struct v4l2_buffer *buf);
void streamoff_cleanup(v4l2_buf_type buf_type);
void g_fmt_setup_trace(struct v4l2_format *format);
void s_fmt_setup(struct v4l2_format *format);
void expbuf_setup(struct v4l2_exportbuffer *export_buffer);
void querybuf_setup(int fd, struct v4l2_buffer *buf);
void query_ext_ctrl_setup(int fd, struct v4l2_query_ext_ctrl *ptr);
void write_json_object_to_json_file(json_object *jobj);
void close_json_file(void);

#endif
