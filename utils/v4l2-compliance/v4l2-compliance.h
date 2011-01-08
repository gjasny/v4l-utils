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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _V4L2_COMPLIANCE_H_
#define _V4L2_COMPLIANCE_H_

#include <string>
#include <linux/videodev2.h>

extern int verbose;
extern unsigned caps;

struct node {
	int fd;
	unsigned caps;
	unsigned tuners;
	unsigned modulators;
	unsigned audio_inputs;
	unsigned audio_outputs;
};

#define fail(fmt, args...) 			\
({ 						\
	if (verbose)				\
 		printf("\t\tfail: " fmt, ##args);	\
	1;					\
})

int doioctl(struct node *node, unsigned long int request, void *parm, const char *name);
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
int testInput(struct node *node);
int testInputAudio(struct node *node);

// Output ioctl tests
int testOutput(struct node *node);
int testOutputAudio(struct node *node);

#endif
