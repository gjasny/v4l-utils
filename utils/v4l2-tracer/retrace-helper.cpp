/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2022 Collabora Ltd.
 */

#include "retrace.h"

struct retrace_context ctx_retrace = {};

bool buffer_in_retrace_context(int fd, __u32 offset)
{
	bool buffer_in_retrace_context = false;
	for (auto &b : ctx_retrace.buffers) {
		if ((b.fd == fd) && (b.offset == offset)) {
			buffer_in_retrace_context = true;
			break;
		}
	}
	return buffer_in_retrace_context;
}

int get_buffer_fd_retrace(__u32 type, __u32 index)
{
	int fd = -1;
	for (auto &b : ctx_retrace.buffers) {
		if ((b.type == type) && (b.index == index)) {
			fd = b.fd;
			break;
		}
	}
	return fd;
}

void add_buffer_retrace(int fd, __u32 type, __u32 index, __u32 offset)
{
	struct buffer_retrace buf = {};
	buf.fd = fd;
	buf.type = type;
	buf.index = index;
	buf.offset = offset;
	ctx_retrace.buffers.push_front(buf);
}

void remove_buffer_retrace(__u32 type, __u32 index)
{
	for (auto it = ctx_retrace.buffers.begin(); it != ctx_retrace.buffers.end(); ++it) {
		if ((it->type == type) && (it->index == index)) {
			ctx_retrace.buffers.erase(it);
			break;
		}
	}
}

void set_buffer_address_retrace(int fd, __u32 offset, long address_trace, long address_retrace)
{
	for (auto &b : ctx_retrace.buffers) {
		if ((b.fd == fd) && (b.offset == offset)) {
			b.address_trace = address_trace;
			b.address_retrace = address_retrace;
			break;
		}
	}
}

long get_retrace_address_from_trace_address(long address_trace)
{
	long address_retrace = 0;
	for (auto &b : ctx_retrace.buffers) {
		if (b.address_trace == address_trace) {
			address_retrace = b.address_retrace;
			break;
		}
	}
	return address_retrace;
}

void print_buffers_retrace(void)
{
	for (auto &b : ctx_retrace.buffers) {
		fprintf(stderr, "fd: %d, offset: %d, address_trace:%ld, address_retrace:%ld\n",
		        b.fd, b.offset, b.address_trace, b.address_retrace);
	}
}

void add_fd(int fd_trace, int fd_retrace)
{
	std::pair<int, int> new_pair;
	new_pair = std::make_pair(fd_trace, fd_retrace);
	ctx_retrace.retrace_fds.insert(new_pair);
}

int get_fd_retrace_from_fd_trace(int fd_trace)
{
	int fd_retrace = -1;
	auto it = ctx_retrace.retrace_fds.find(fd_trace);
	if (it != ctx_retrace.retrace_fds.end())
		fd_retrace = it->second;
	return fd_retrace;
}

void print_fds(void)
{
	if (ctx_retrace.retrace_fds.empty())
		fprintf(stderr, "all devices closed\n");
	for (auto retrace_fd : ctx_retrace.retrace_fds)
		fprintf(stderr, "fd_trace: %d, fd_retrace: %d\n", retrace_fd.first, retrace_fd.second);
}

std::string get_path_retrace_from_path_trace(std::string path_trace, json_object *open_obj)
{
	bool is_media = path_trace.find("media") != std::string::npos;
	bool is_video = path_trace.find("video") != std::string::npos;

	std::string path_media;
	std::string path_video;

	/* If user set the media or video path just return that path. */
	if (is_media && (getenv("V4L2_TRACER_OPTION_SET_MEDIA_DEVICE") != nullptr)) {
		path_media = getenv("V4L2_TRACER_OPTION_SET_MEDIA_DEVICE");
		debug_line_info("\n\tUse path set by user: %s", path_media.c_str());
		return path_media;
	}
	if (is_video && (getenv("V4L2_TRACER_OPTION_SET_VIDEO_DEVICE") != nullptr)) {
		path_video = getenv("V4L2_TRACER_OPTION_SET_VIDEO_DEVICE");
		debug_line_info("\n\tUse path set by user: %s", path_video.c_str());
		return path_video;
	}

	std::string driver;
	json_object *driver_obj;
	if (json_object_object_get_ex(open_obj, "driver", &driver_obj))
		if (json_object_get_string(driver_obj) != nullptr)
			driver = json_object_get_string(driver_obj);
	if (driver.empty())
		return "";

	path_media = get_path_media(driver);
	if (path_media.empty()) {
		line_info("\n\tWarning: driver: \'%s\' not found.", driver.c_str());
		return "";
	}

	if (is_media)
		return path_media;

	if (is_video) {
		std::list<std::string> linked_entities;
		json_object *le_obj;
		if (json_object_object_get_ex(open_obj, "linked_entities", &le_obj)) {
			for (size_t i = 0; i < array_list_length(json_object_get_array(le_obj)); i++) {
				std::string ename;
				if (json_object_get_string(json_object_array_get_idx(le_obj, i)) != nullptr)
					ename = json_object_get_string(json_object_array_get_idx(le_obj, i));
				linked_entities.push_back(ename);
			}
		}
		if (linked_entities.size() == 0)
			return "";

		setenv("V4L2_TRACER_PAUSE_TRACE", "true", 0);
		int media_fd = open(path_media.c_str(), O_RDONLY);
		unsetenv("V4L2_TRACER_PAUSE_TRACE");

		std::string path_video = get_path_video(media_fd, linked_entities);
		setenv("V4L2_TRACER_PAUSE_TRACE", "true", 0);
		close(media_fd);
		unsetenv("V4L2_TRACER_PAUSE_TRACE");
		return path_video;
	}

	return "";
}

void write_to_output_buffer(unsigned char *buffer_pointer, int bytesused, json_object *mem_obj)
{
	int byteswritten = 0;
	const int hex_base = 16;
	json_object *line_obj;
	size_t number_of_lines;
	std::string compressed_video_data;

	json_object *mem_array_obj;
	json_object_object_get_ex(mem_obj, "mem_array", &mem_array_obj);
	number_of_lines = json_object_array_length(mem_array_obj);

	for (long unsigned int i = 0; i < number_of_lines; i++) {
		line_obj = json_object_array_get_idx(mem_array_obj, i);
		if (json_object_get_string(line_obj) != nullptr)
			compressed_video_data = json_object_get_string(line_obj);

		for (long unsigned i = 0; i < compressed_video_data.length(); i++) {
			if (std::isspace(compressed_video_data[i]) != 0)
				continue;
			try {
				/* Two values from the string e.g. "D9" are needed to write one byte. */
				*buffer_pointer = (char) std::stoi(compressed_video_data.substr(i,2), nullptr, hex_base);
				buffer_pointer++;
				i++;
				byteswritten++;
			} catch (std::invalid_argument& ia) {
				line_info("\n\t\'%s\' is an invalid argument.\n",
				          compressed_video_data.substr(i,2).c_str());
			} catch (std::out_of_range& oor) {
				line_info("\n\t\'%s\' is out of range.\n",
				          compressed_video_data.substr(i,2).c_str());
			}
		}
	}
	debug_line_info("\n\tbytesused: %d, byteswritten: %d", bytesused, byteswritten);
}

void compare_program_versions(json_object *v4l2_tracer_info_obj)
{
	json_object *package_version_obj;
	json_object_object_get_ex(v4l2_tracer_info_obj, "package_version", &package_version_obj);
	std::string package_version_trace;
	if (json_object_get_string(package_version_obj) != nullptr)
		package_version_trace = json_object_get_string(package_version_obj);
	std::string package_version_retrace = PACKAGE_VERSION;
	if (package_version_trace != package_version_retrace) {
		line_info("\n\tWarning: trace package version \'%s\' does not match current: \'%s\'",
		          package_version_trace.c_str(), package_version_retrace.c_str());
		print_v4l2_tracer_info();
		return;
	}

	json_object *git_sha_obj;
	json_object_object_get_ex(v4l2_tracer_info_obj, "git_sha", &git_sha_obj);
	std::string git_sha_trace;
	if (json_object_get_string(git_sha_obj) != nullptr)
		git_sha_trace = json_object_get_string(git_sha_obj);
	std::string git_sha_retrace = (STRING(GIT_SHA));
	if (git_sha_trace != git_sha_retrace) {
		line_info("\n\tWarning: sha in trace file \'%s\' does not match current sha: \'%s\'",
		          git_sha_trace.c_str(),  git_sha_retrace.c_str());
		print_v4l2_tracer_info();
		return;
	}
}

void print_context(void)
{
	if (!is_debug())
		return;
	print_fds();
	print_buffers_retrace();
	fprintf(stderr, "\n");
}
