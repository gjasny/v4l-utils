/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2022 Collabora Ltd.
 */

#ifndef RETRACE_H
#define RETRACE_H

#include "v4l2-tracer-common.h"
#include "retrace-gen.h"

struct buffer_retrace {
	int fd;
	__u32 type;
	__u32 index;
	__u32 offset;
	long address_trace;
	long address_retrace;
};

struct retrace_context {
	/* Key is a file descriptor from the trace, value is the corresponding fd in the retrace. */
	std::unordered_map<int, int> retrace_fds;
	/* List of output and capture buffers being retraced. */
	std::list<struct buffer_retrace> buffers;
};

int retrace(std::string trace_filename);

bool buffer_in_retrace_context(int fd, __u32 offset = 0);
int get_buffer_fd_retrace(__u32 type, __u32 index);
void add_buffer_retrace(int fd, __u32 type, __u32 index, __u32 offset = 0);
void remove_buffer_retrace(__u32 type, __u32 index);
void set_buffer_address_retrace(int fd, __u32 offset, long address_trace, long address_retrace);
long get_retrace_address_from_trace_address(long address_trace);
void add_fd(int fd_trace, int fd_retrace);
int get_fd_retrace_from_fd_trace(int fd_trace);
std::string get_path_retrace_from_path_trace(std::string path_trace, json_object *jobj);
void write_to_output_buffer(unsigned char *buffer_pointer, int bytesused, json_object *mem_obj);
void compare_program_versions(json_object *v4l2_tracer_info_obj);
void print_context(void);

#endif
