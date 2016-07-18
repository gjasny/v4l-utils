/*
    V4L2 API compliance test tool.

    Copyright (C) 2008, 2010  Hans Verkuil <hverkuil@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#ifndef _V4L2_COMPLIANCE_H_
#define _V4L2_COMPLIANCE_H_

#include <stdarg.h>
#include <cerrno>
#include <string>
#include <map>
#include <set>
#include <map>
#include <linux/videodev2.h>

#ifdef ANDROID
#include <android-config.h>
#else
#include <config.h>
#endif

#ifndef NO_LIBV4L2
#include <libv4l2.h>
#endif

#include <cv4l-helpers.h>

#if !defined(ENODATA) && (defined(__FreeBSD__) || defined(__FreeBSD_kernel__))
#define ENODATA ENOTSUP
#endif

extern bool show_info;
extern bool show_warnings;
extern int kernel_version;
extern unsigned warnings;

struct test_query_ext_ctrl: v4l2_query_ext_ctrl {
	__u64 menu_mask;
};

typedef std::map<__u32, struct test_query_ext_ctrl> qctrl_map;
typedef std::map<__u32, __u32> pixfmt_map;
typedef std::set<__u64> frmsizes_set;
typedef std::map<__u32, unsigned> frmsizes_count_map;

struct base_node;

#define V4L2_BUF_TYPE_LAST V4L2_BUF_TYPE_SDR_OUTPUT

struct base_node {
	bool is_video;
	bool is_radio;
	bool is_vbi;
	bool is_sdr;
	bool is_touch;
	bool is_m2m;
	bool is_planar;
	bool can_capture;
	bool can_output;
	bool can_scale;
	const char *device;
	struct node *node2;	/* second open filehandle */
	bool has_outputs;
	bool has_inputs;
	unsigned tuners;
	unsigned modulators;
	unsigned inputs;
	unsigned audio_inputs;
	unsigned outputs;
	unsigned audio_outputs;
	unsigned cur_io_caps;
	unsigned std_controls;
	unsigned priv_controls;
	__u32 fbuf_caps;
	pixfmt_map buftype_pixfmts[V4L2_BUF_TYPE_LAST + 1];
	frmsizes_set frmsizes;
	frmsizes_count_map frmsizes_count;
	bool has_frmintervals;
	__u32 valid_buftypes;
	__u32 valid_buftype;
	__u32 valid_memorytype;
};

struct node : public base_node, public cv4l_fd {
	node() : base_node() {}

	qctrl_map controls;
	pixfmt_map buftype_pixfmts[V4L2_BUF_TYPE_LAST + 1];
};

#define info(fmt, args...) 					\
	do {							\
		if (show_info)					\
 			printf("\t\tinfo: " fmt, ##args);	\
	} while (0)

#define warn(fmt, args...) 					\
	do {							\
		warnings++;					\
		if (show_warnings)				\
 			printf("\t\twarn: %s(%d): " fmt, __FILE__, __LINE__, ##args);	\
	} while (0)

#define warn_once(fmt, args...)						\
	do {								\
		static bool show;					\
									\
		if (!show) {						\
			show = true;					\
			warnings++;					\
			if (show_warnings)				\
				printf("\t\twarn: %s(%d): " fmt,	\
					__FILE__, __LINE__, ##args); 	\
		}							\
	} while (0)

#define fail(fmt, args...) 						\
({ 									\
 	printf("\t\tfail: %s(%d): " fmt, __FILE__, __LINE__, ##args);	\
	1;								\
})

#define fail_on_test(test) 				\
	do {						\
	 	if (test)				\
			return fail("%s\n", #test);	\
	} while (0)

static inline int check_fract(const struct v4l2_fract *f)
{
	if (f->numerator && f->denominator)
		return 0;
	return 1;
}

static inline double fract2f(const struct v4l2_fract *f)
{
	return (double)f->numerator / (double)f->denominator;
}

#define doioctl(n, r, p) v4l_named_ioctl((n)->g_v4l_fd(), #r, r, p)

std::string cap2s(unsigned cap);
std::string buftype2s(int type);
std::string fcc2s(unsigned int val);

static inline std::string buftype2s(enum v4l2_buf_type type)
{
       return buftype2s((int)type);
}

const char *ok(int res);
int check_string(const char *s, size_t len);
int check_ustring(const __u8 *s, int len);
int check_0(const void *p, int len);
int restoreFormat(struct node *node);
std::string pixfmt2s(unsigned id);

// Debug ioctl tests
int testRegister(struct node *node);
int testLogStatus(struct node *node);

// Input ioctl tests
int testTuner(struct node *node);
int testTunerFreq(struct node *node);
int testTunerHwSeek(struct node *node);
int testEnumInputAudio(struct node *node);
int testInput(struct node *node);
int testInputAudio(struct node *node);

// Output ioctl tests
int testModulator(struct node *node);
int testModulatorFreq(struct node *node);
int testEnumOutputAudio(struct node *node);
int testOutput(struct node *node);
int testOutputAudio(struct node *node);

// Control ioctl tests
int testQueryExtControls(struct node *node);
int testQueryControls(struct node *node);
int testSimpleControls(struct node *node);
int testExtendedControls(struct node *node);
int testEvents(struct node *node);
int testJpegComp(struct node *node);

// I/O configuration ioctl tests
int testStd(struct node *node);
int testTimings(struct node *node);
int testTimingsCap(struct node *node);
int testEdid(struct node *node);

// Format ioctl tests
int testEnumFormats(struct node *node);
int testParm(struct node *node);
int testFBuf(struct node *node);
int testGetFormats(struct node *node);
int testTryFormats(struct node *node);
int testSetFormats(struct node *node);
int testSlicedVBICap(struct node *node);
int testCropping(struct node *node);
int testComposing(struct node *node);
int testScaling(struct node *node);

// Codec ioctl tests
int testEncoder(struct node *node);
int testEncIndex(struct node *node);
int testDecoder(struct node *node);

// Buffer ioctl tests
int testReqBufs(struct node *node);
int testReadWrite(struct node *node);
int testExpBuf(struct node *node);
int testMmap(struct node *node, unsigned frame_count);
int testUserPtr(struct node *node, unsigned frame_count);
int testDmaBuf(struct node *expbuf_node, struct node *node, unsigned frame_count);
void streamAllFormats(struct node *node);

// Color tests
int testColorsAllFormats(struct node *node, unsigned component,
			 unsigned skip, unsigned perc);

#endif
