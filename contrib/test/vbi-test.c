/*
   v4l-ioctl-test - This small utility checks VBI format

   Copyright (C) 2006 Mauro Carvalho Chehab <mchehab@infradead.org>

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#ifdef CONFIG_VIDEO_V4L1_COMPAT
#include "../../lib/include/libv4l1-videodev.h"
#endif

/* All possible parameters used on v4l ioctls */
union v4l_parms {
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	/* V4L1 structs */
	struct vbi_format v1;
#endif

	/* V4L2 structs */
	struct v4l2_format v2;
};

/* All defined ioctls */
int ioctls[] = {
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	/* V4L ioctls */

	VIDIOCGVBIFMT,/* struct vbi_format */
#endif

	/* V4L2 ioctls */

	VIDIOC_G_FMT,/* struct v4l2_format */
};
#define S_IOCTLS sizeof(ioctls)/sizeof(ioctls[0])

/********************************************************************/
int main (void)
{
	int fd=0, ret=0;
	char *device="/dev/video0";
	union v4l_parms p;

	if ((fd = open(device, O_RDONLY)) < 0) {
		perror("Couldn't open video0");
		return(-1);
	}


#ifdef CONFIG_VIDEO_V4L1_COMPAT
	/* V4L1 call */
	memset(&p,0,sizeof(p));
	ret=ioctl(fd,VIDIOCGVBIFMT, (void *) &p);

	printf ("V4L1 call: ret=%i: sampling_rate=%d, samples_per_line=%d, "
		"sample_format=%d, start=%d/%d, count=%d/%d, flags=%d\n", ret,
		p.v1.sampling_rate,p.v1.samples_per_line, p.v1.sample_format,
		p.v1.start[0],p.v1.start[1],p.v1.count[0],p.v1.count[1],p.v1.flags);
#endif


	/* V4L2 call */
	memset(&p,0,sizeof(p));
	p.v2.type=V4L2_BUF_TYPE_VBI_CAPTURE;
	ret=ioctl(fd,VIDIOC_G_FMT, (void *) &p);

	printf ("V4L2 call: ret=%i: sampling_rate=%d, samples_per_line=%d, "
		"sample_format=%d, offset=%d, start=%d/%d, count=%d/%d\n", ret,
		p.v2.fmt.vbi.sampling_rate,p.v2.fmt.vbi.samples_per_line,
		p.v2.fmt.vbi.sample_format,p.v2.fmt.vbi.offset,
		p.v2.fmt.vbi.start[0],p.v2.fmt.vbi.start[1],
		p.v2.fmt.vbi.count[0],p.v2.fmt.vbi.count[1]);

	close (fd);

	return (0);
}
