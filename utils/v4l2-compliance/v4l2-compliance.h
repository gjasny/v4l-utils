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

#include <string>
#include <list>
#include <linux/videodev2.h>
#include <libv4l2.h>

extern int verbose;
extern int wrapper;
extern int kernel_version;
extern unsigned caps;
extern unsigned warnings;

struct test_queryctrl: v4l2_queryctrl {
	unsigned menu_mask;
};

typedef std::list<test_queryctrl> qctrl_list;

struct node {
	int fd;
	bool is_video;
	bool is_radio;
	bool is_vbi;
	unsigned caps;
	bool has_outputs;
	bool has_inputs;
	unsigned tuners;
	unsigned modulators;
	unsigned inputs;
	unsigned audio_inputs;
	unsigned outputs;
	unsigned audio_outputs;
	unsigned std_controls;
	unsigned priv_controls;
	qctrl_list controls;
};

#define info(fmt, args...) 					\
	do {							\
		if (verbose > 1)				\
 			printf("\t\tinfo: " fmt, ##args);	\
	} while (0)

#define warn(fmt, args...) 					\
	do {							\
		warnings++;					\
		if (verbose)					\
 			printf("\t\twarn: " fmt, ##args);	\
	} while (0)

#define fail(fmt, args...) 			\
({ 						\
 	printf("\t\tfail: " fmt, ##args);	\
	1;					\
})

static inline int test_open(const char *file, int oflag)
{
 	return wrapper ? v4l2_open(file, oflag) : open(file, oflag);
}

static inline int test_close(int fd)
{
	return wrapper ? v4l2_close(fd) : close(fd);
}

static inline int test_ioctl(int fd, int cmd, void *arg)
{
	return wrapper ? v4l2_ioctl(fd, cmd, arg) : ioctl(fd, cmd, arg);
}

int doioctl_name(struct node *node, unsigned long int request, void *parm, const char *name);
#define doioctl(n, r, p) doioctl_name(n, r, p, #r)

std::string cap2s(unsigned cap);
const char *ok(int res);
int check_string(const char *s, size_t len);
int check_ustring(const __u8 *s, int len);
int check_0(const void *p, int len);

// Debug ioctl tests
int testChipIdent(struct node *node);
int testRegister(struct node *node);
int testLogStatus(struct node *node);

// Input ioctl tests
int testTuner(struct node *node);
int testTunerFreq(struct node *node);
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
int testQueryControls(struct node *node);
int testSimpleControls(struct node *node);
int testExtendedControls(struct node *node);

// I/O configuration ioctl tests
int testStd(struct node *node);
int testPresets(struct node *node);
int testCustomTimings(struct node *node);

#endif
