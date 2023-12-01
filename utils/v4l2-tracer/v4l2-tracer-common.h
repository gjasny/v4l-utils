/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2022 Collabora Ltd.
 */

#ifndef V4L2_TRACER_COMMON_H
#define V4L2_TRACER_COMMON_H

#include "v4l2-info.h"
#include "codec-fwht.h"
#include "media-info.h"
#include <algorithm>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <json.h>
#include <linux/media.h>
#include <linux/videodev2.h>
#include <list>
#include <poll.h>
#include <pthread.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#define STR(x) #x
#define STRING(x) STR(x)

#ifdef HAVE_STRERRORNAME_NP
#define STRERR(x) strerrorname_np(x)
#else
#define STRERR(x) strerror(x)
#endif

#ifndef __FILE_NAME__
#define __FILE_NAME__ __FILE__
#endif

#define line_info(fmt, args...)					\
	do {								\
		fprintf(stderr, "%s:%s:%d " fmt "\n",			\
		        __FILE_NAME__, __func__, __LINE__, ##args);	\
	} while (0)							\

#define debug_line_info(fmt, args...)		\
	do {					\
		if (is_debug())			\
			line_info(fmt, ##args);	\
	} while (0)				\

struct val_def {
	__s64 val;
	const char *str;
};

bool is_debug(void);
bool is_verbose(void);
void print_v4l2_tracer_info(void);
void print_usage(void);
std::string ver2s(unsigned int version);
std::string number2s_oct(long num);
std::string number2s(long num);
std::string val2s(long val, const val_def *def);
std::string fl2s(unsigned val, const flag_def *def);
std::string fl2s_buffer(__u32 flags);
std::string fl2s_fwht(__u32 flags);
long s2number(const char *char_str);
long s2val(const char *char_str, const val_def *def);
unsigned long s2flags(const char *char_str, const flag_def *def);
unsigned long s2flags_buffer(const char *char_str);
unsigned long s2flags_fwht(const char *char_str);
std::string which2s(unsigned long which);
std::string get_path_media(std::string driver);
std::string get_path_video(int media_fd, std::list<std::string> linked_entities);
std::list<std::string> get_linked_entities(int media_fd, std::string path_video);

constexpr val_def which_val_def[] = {
	{ V4L2_CTRL_WHICH_CUR_VAL,	"V4L2_CTRL_WHICH_CUR_VAL" },
	{ V4L2_CTRL_WHICH_DEF_VAL,	"V4L2_CTRL_WHICH_DEF_VAL" },
	{ V4L2_CTRL_WHICH_REQUEST_VAL,	"V4L2_CTRL_WHICH_REQUEST_VAL" },
	{ -1, "" }
};

constexpr val_def open_val_def[] = {
	{ O_RDONLY,	"O_RDONLY" },
	{ O_WRONLY,	"O_WRONLY" },
	{ O_RDWR,	"O_RDWR" },
	{ -1, "" }
};

#include "v4l2-tracer-info-gen.h"

#endif
