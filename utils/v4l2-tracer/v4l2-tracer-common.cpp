/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2022 Collabora Ltd.
 */

#include "v4l2-tracer-common.h"
#include <iomanip>
#include <iostream>
#include <sstream>

bool is_debug(void)
{
	return (getenv("V4L2_TRACER_OPTION_DEBUG") != nullptr);
}

bool is_verbose(void)
{
	return (getenv("V4L2_TRACER_OPTION_VERBOSE") != nullptr);
}

void print_v4l2_tracer_info(void)
{
	fprintf(stderr, "v4l2-tracer %s%s\n", PACKAGE_VERSION, STRING(GIT_COMMIT_CNT));
	if (strlen(STRING(GIT_SHA)) != 0U)
		fprintf(stderr, "v4l2-tracer SHA: '%s' %s\n", STRING(GIT_SHA), STRING(GIT_COMMIT_DATE));
}

void print_usage(void)
{
	print_v4l2_tracer_info();
	fprintf(stderr, "Usage:\n\tv4l2-tracer [options] trace <tracee>\n"
	        "\tv4l2-tracer [options] retrace <trace_file>.json\n"
	        "\tv4l2-tracer clean <trace_file>.json\n\n"

	        "\tCommon options:\n"
	        "\t\t-c, --compact     Write minimal whitespace in JSON file.\n"
	        "\t\t-g, --debug       Turn on verbose reporting plus additional debug info.\n"
	        "\t\t-h, --help        Display this message.\n"
	        "\t\t-r  --raw         Write decoded video frame data to JSON file.\n"
	        "\t\t-u  --userspace   Trace userspace arguments.\n"
	        "\t\t-v, --verbose     Turn on verbose reporting.\n"
	        "\t\t-y, --yuv         Write decoded video frame data to yuv file.\n\n"

	        "\tRetrace options:\n"
	        "\t\t-d, --video_device <dev>   Retrace with a specific video device.\n"
	        "\t\t                           <dev> must be a digit corresponding to\n"
	        "\t\t                           /dev/video<dev> \n\n"
	        "\t\t-m, --media_device <dev>   Retrace with a specific media device.\n"
	        "\t\t                           <dev> must be a digit corresponding to\n"
	        "\t\t                           /dev/media<dev> \n\n");
}

void add_separator(std::string &str)
{
	if (!str.empty())
		str += "|";
}

void clean_string(size_t idx, std::string substring_to_erase, std::string &str)
{
	std::string temp = substring_to_erase + '|';
	if (str.find(temp) != std::string::npos)
		str.erase(idx, temp.length());
	else
		str.erase(idx, substring_to_erase.length());
}

std::string ver2s(unsigned int version)
{
	const int mask = 0xff;
	const int BUF_SIZE = 16;
	char buf[BUF_SIZE];
	sprintf(buf, "%d.%d.%d", version >> BUF_SIZE, (version >> (BUF_SIZE / 2)) & mask, version & mask);
	return buf;
}

/* Convert a number to an octal string. If num is 0, return an empty string. */
std::string number2s_oct(long num)
{
	const int min_width = 5;
	std::stringstream stream;
	stream << std::setfill ('0') << std::setw(min_width) << std::oct << num;
	return stream.str();
}

/* Convert a number to a hex string. If num is 0, return an empty string. */
std::string number2s(long num)
{
	if (num == 0)
		return "";
	std::stringstream stream;
	stream << std::hex << num;
	return "0x" + stream.str();
}

std::string val2s(long val, const val_def *def)
{
	if (def == nullptr)
		return number2s(val);

	while ((def->val != -1) && (def->val != val))
		def++;

	if (def->val == val)
		return def->str;

	return number2s(val);
}

std::string fl2s(unsigned val, const flag_def *def)
{
	std::string str;

	if (def == nullptr)
		return number2s(val);

	while ((def->flag) != 0U) {
		if ((val & def->flag) != 0U) {
			add_separator(str);
			str += def->str;
			val &= ~def->flag;
		}
		def++;
	}
	if (val != 0U) {
		add_separator(str);
		str += number2s(val);
	}

	return str;
}

std::string fl2s_buffer(__u32 flags)
{
	std::string str;

	switch (flags & V4L2_BUF_FLAG_TIMESTAMP_MASK) {
	case V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN:
		str += "V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN";
		flags &= ~V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN;
		break;
	case V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC:
		str += "V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC";
		flags &= ~V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
		break;
	case V4L2_BUF_FLAG_TIMESTAMP_COPY:
		str += "V4L2_BUF_FLAG_TIMESTAMP_COPY";
		flags &= ~V4L2_BUF_FLAG_TIMESTAMP_COPY;
		break;
	default:
		break;
	}

	/* Since V4L2_BUF_FLAG_TSTAMP_SRC_EOF == 0, at least this flag will always be added. */
	add_separator(str);
	switch (flags & V4L2_BUF_FLAG_TSTAMP_SRC_MASK) {
	case V4L2_BUF_FLAG_TSTAMP_SRC_EOF:
		str += "V4L2_BUF_FLAG_TSTAMP_SRC_EOF";
		flags &= ~V4L2_BUF_FLAG_TSTAMP_SRC_EOF;
		break;
	case V4L2_BUF_FLAG_TSTAMP_SRC_SOE:
		str += "V4L2_BUF_FLAG_TSTAMP_SRC_SOE";
		flags &= ~V4L2_BUF_FLAG_TSTAMP_SRC_SOE;
		break;
	default:
		break;
	}

	if (flags != 0U) {
		add_separator(str);
		const unsigned ts_mask = V4L2_BUF_FLAG_TIMESTAMP_MASK | V4L2_BUF_FLAG_TSTAMP_SRC_MASK;
		str += fl2s(flags & ~ts_mask, v4l2_buf_flag_def);
	}

	return str;
}

std::string fl2s_fwht(__u32 flags)
{
	std::string str;
	switch (flags & V4L2_FWHT_FL_PIXENC_MSK) {
	case V4L2_FWHT_FL_PIXENC_YUV:
		str += "V4L2_FWHT_FL_PIXENC_YUV";
		flags &= ~V4L2_FWHT_FL_PIXENC_YUV;
		break;
	case V4L2_FWHT_FL_PIXENC_RGB:
		str += "V4L2_FWHT_FL_PIXENC_RGB";
		flags &= ~V4L2_FWHT_FL_PIXENC_RGB;
		break;
	case V4L2_FWHT_FL_PIXENC_HSV:
		str += "V4L2_FWHT_FL_PIXENC_HSV";
		flags &= ~V4L2_FWHT_FL_PIXENC_HSV;
		break;
	default:
		break;
	}
	add_separator(str);
	str += fl2s(flags, v4l2_ctrl_fwht_params_flag_def);
	return str;
}

long s2number(const char *char_str)
{
	if (char_str == nullptr)
		return 0;

	std::string str = char_str;

	long num = 0;
	if (str.empty())
		return 0;
	try {
		num = std::strtol(str.c_str(), nullptr, 0); /* base is auto-detected */
	} catch (std::invalid_argument& ia) {
		line_info("\n\tString \'%s\' is invalid.", str.c_str());
	} catch (std::out_of_range& oor) {
		line_info("\n\tString \'%s\' is out of range.", str.c_str());
	}
	return num;
}

long s2val(const char *char_str, const val_def *def)
{
	if (char_str == nullptr)
		return 0;
	std::string str = char_str;

	if (str.empty())
		return 0;

	if (def == nullptr)
		return s2number(char_str);

	while ((def->val != -1) && (def->str != str))
		def++;

	if (def->str == str)
		return def->val;

	return s2number(char_str);
}

unsigned long s2flags(const char *char_str, const flag_def *def)
{
	if (char_str == nullptr)
		return 0;
	std::string str = char_str;

	size_t idx = 0;
	unsigned long flags = 0;

	if (def == nullptr)
		return s2number(char_str);

	while ((def->flag) != 0U) {
		idx = str.find(def->str);
		if (idx == std::string::npos) {
			def++;
			continue;
		}
		/* Stop false substring matches e.g. in V4L2_BUF_CAP_SUPPORTS_MMAP_CACHE_HINTS */
		std::string check = def->str;
		if (check.length() != str.length()) {
			idx = str.find(check + '|');
			if (idx == std::string::npos) {
				def++;
				continue;
			}
		}
		flags += def->flag;
		clean_string(idx, def->str, str);
		def++;
	}
	if (!str.empty())
		flags += s2number(str.c_str());

	return flags;
}

unsigned long s2flags_buffer(const char *char_str)
{
	if (char_str == nullptr)
		return 0;
	std::string str = char_str;

	size_t idx = 0;
	unsigned long flags = 0;

	idx = str.find("V4L2_BUF_FLAG_TIMESTAMP_COPY");
	if (idx != std::string::npos) {
		flags += V4L2_BUF_FLAG_TIMESTAMP_COPY;
		clean_string(idx, "V4L2_BUF_FLAG_TIMESTAMP_COPY", str);
	}
	idx = str.find("V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC");
	if (idx != std::string::npos) {
		flags += V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
		clean_string(idx, "V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC", str);
	}
	idx = str.find("V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN");
	if (idx != std::string::npos) {
		flags += V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN;
		clean_string(idx, "V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN", str);
	}
	idx = str.find("V4L2_BUF_FLAG_TSTAMP_SRC_SOE");
	if (idx != std::string::npos) {
		flags += V4L2_BUF_FLAG_TSTAMP_SRC_SOE;
		clean_string(idx, "V4L2_BUF_FLAG_TSTAMP_SRC_SOE", str);
	}
	idx = str.find("V4L2_BUF_FLAG_TSTAMP_SRC_EOF");
	if (idx != std::string::npos) {
		flags += V4L2_BUF_FLAG_TSTAMP_SRC_EOF;
		clean_string(idx, "V4L2_BUF_FLAG_TSTAMP_SRC_EOF", str);
	}
	if (!str.empty())
		flags += s2flags(str.c_str(), v4l2_buf_flag_def);
	return flags;
}

unsigned long s2flags_fwht(const char *char_str)
{
	if (char_str == nullptr)
		return 0;
	std::string str = char_str;

	size_t idx = 0;
	unsigned long flags = 0;
	idx = str.find("V4L2_FWHT_FL_PIXENC_YUV");
	if (idx != std::string::npos) {
		flags += V4L2_FWHT_FL_PIXENC_YUV;
		clean_string(idx, "V4L2_FWHT_FL_PIXENC_YUV", str);
	}
	idx = str.find("V4L2_FWHT_FL_PIXENC_RGB");
	if (idx != std::string::npos) {
		flags += V4L2_FWHT_FL_PIXENC_RGB;
		clean_string(idx, "V4L2_FWHT_FL_PIXENC_RGB", str);
	}
	idx = str.find("V4L2_FWHT_FL_PIXENC_HSV");
	if (idx != std::string::npos) {
		flags += V4L2_FWHT_FL_PIXENC_HSV;
		clean_string(idx, "V4L2_FWHT_FL_PIXENC_HSV", str);
	}
	if (!str.empty())
		flags += s2flags(str.c_str(), v4l2_ctrl_fwht_params_flag_def);
	return flags;
}

std::string get_path_media(std::string driver)
{
	struct dirent *entry_pointer = nullptr;
	std::string path_media;
	DIR *directory_pointer = opendir("/dev");
	if (directory_pointer == nullptr)
		return path_media;

	while ((entry_pointer = readdir(directory_pointer)) != nullptr) {
		std::string media = "media";
		const char *name = entry_pointer->d_name;
		if ((memcmp(name, media.c_str(), media.length()) != 0) || (isdigit(name[media.length()]) == 0))
			continue;

		std::string media_devname = std::string("/dev/") + name;
		setenv("V4L2_TRACER_PAUSE_TRACE", "true", 0);
		int media_fd = open(media_devname.c_str(), O_RDONLY);
		unsetenv("V4L2_TRACER_PAUSE_TRACE");
		if (media_fd < 0)
			continue;

		struct media_device_info info = {};
		if (ioctl(media_fd, MEDIA_IOC_DEVICE_INFO, &info) || info.driver != driver) {
			setenv("V4L2_TRACER_PAUSE_TRACE", "true", 0);
			close(media_fd);
			unsetenv("V4L2_TRACER_PAUSE_TRACE");
			continue;
		}
		path_media = media_devname;
		setenv("V4L2_TRACER_PAUSE_TRACE", "true", 0);
		close(media_fd);
		unsetenv("V4L2_TRACER_PAUSE_TRACE");
	}
	closedir(directory_pointer);
	return path_media;
}

std::string get_path_video(int media_fd, std::list<std::string> linked_entities)
{
	int err = 0;
	std::string path_video;
	struct media_v2_topology topology = {};

	err = ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, &topology);
	if (err < 0)
		return path_video;

	std::vector<media_v2_interface> ifaces(topology.num_interfaces);
	topology.ptr_interfaces = (uintptr_t) ifaces.data();

	std::vector<media_v2_link> links(topology.num_links);
	topology.ptr_links = (uintptr_t) links.data();

	std::vector<media_v2_entity> ents(topology.num_entities);
	topology.ptr_entities = (uintptr_t) ents.data();

	err = ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, &topology);
	if (err < 0)
		return path_video;

	for (auto &name : linked_entities) {
		/* Find an entity listed in the video device's linked_entities. */
		for (__u32 i = 0; i < topology.num_entities; i++) {
			if (ents[i].name != name)
				continue;
			/* Find the first link connected to that entity. */
			for (__u32 j = 0; j < topology.num_links; j++) {
				if (links[j].sink_id != ents[i].id)
					continue;
				/* Find the interface connected to that link. */
				for (__u32 k = 0; k < topology.num_interfaces; k++) {
					if (ifaces[k].id != links[j].source_id)
						continue;
					std::string video_devname = mi_media_get_device(ifaces[k].devnode.major,
					                                                ifaces[k].devnode.minor);
					if (!video_devname.empty()) {
						path_video = video_devname;
						break;
					}
				}
			}
		}
	}
	return path_video;
}

std::list<std::string> get_linked_entities(int media_fd, std::string path_video)
{
	int err = 0;
	std::list<std::string> linked_entities;
	struct media_v2_topology topology = {};

	err = ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, &topology);
	if (err < 0)
		return linked_entities;

	std::vector<media_v2_interface> ifaces(topology.num_interfaces);
	topology.ptr_interfaces = (uintptr_t) ifaces.data();

	std::vector<media_v2_link> links(topology.num_links);
	topology.ptr_links = (uintptr_t) links.data();

	std::vector<media_v2_entity> ents(topology.num_entities);
	topology.ptr_entities = (uintptr_t) ents.data();

	err = ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, &topology);
	if (err < 0)
		return linked_entities;

	/* find the interface corresponding to the path_video */
	for (__u32 i = 0; i < topology.num_interfaces; i++) {
		if (path_video != mi_media_get_device(ifaces[i].devnode.major, ifaces[i].devnode.minor))
			continue;
		/* find the links from that interface */
		for (__u32 j = 0; j < topology.num_links; j++) {
			if (links[j].source_id != ifaces[i].id)
				continue;
			/* find the entities connected by that link to the interface */
			for (__u32 k = 0; k < topology.num_entities; k++) {
				if (ents[k].id != links[j].sink_id)
					continue;
				linked_entities.push_back(ents[k].name);
			}
		}
		if (linked_entities.size())
			break;
	}
	return linked_entities;
}
