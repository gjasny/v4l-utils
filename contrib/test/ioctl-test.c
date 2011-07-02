/*
   v4l-ioctl-test - This small utility sends dumb v4l2 ioctl to a device.
   It is meant to check ioctl debug messages generated and to check
	if a function is implemented by that device.

   Copyright (C) 2005 Mauro Carvalho Chehab <mchehab@infradead.org>

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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "linux/videodev2.h"

/* All possible parameters used on v4l ioctls */
union v4l_parms {
	int		i;
	unsigned long	ulong;
	u_int32_t	u32;
	v4l2_std_id	id;
	enum v4l2_priority prio;

	/* V4L2 structs */
	struct v4l2_capability p_v4l2_capability;
	struct v4l2_fmtdesc p_v4l2_fmtdesc;
	struct v4l2_format p_v4l2_format;
	struct v4l2_requestbuffers p_v4l2_requestbuffers;
	struct v4l2_buffer p_v4l2_buffer;
	struct v4l2_framebuffer p_v4l2_framebuffer;
	struct v4l2_streamparm p_v4l2_streamparm;
	struct v4l2_standard p_v4l2_standard;
	struct v4l2_input p_v4l2_input;
	struct v4l2_control p_v4l2_control;
	struct v4l2_tuner p_v4l2_tuner;
	struct v4l2_audio p_v4l2_audio;
	struct v4l2_queryctrl p_v4l2_queryctrl;
	struct v4l2_querymenu p_v4l2_querymenu;
	struct v4l2_output p_v4l2_output;
	struct v4l2_audioout p_v4l2_audioout;
	struct v4l2_modulator p_v4l2_modulator;
	struct v4l2_frequency p_v4l2_frequency;
	struct v4l2_cropcap p_v4l2_cropcap;
	struct v4l2_crop p_v4l2_crop;
	struct v4l2_jpegcompression p_v4l2_jpegcompression;
	struct v4l2_sliced_vbi_cap p_v4l2_sliced_vbi_cap;
	struct v4l2_ext_controls p_v4l2_ext_controls;
	struct v4l2_frmsizeenum p_v4l2_frmsizeenum;
	struct v4l2_frmivalenum p_v4l2_frmivalenum;
	struct v4l2_enc_idx p_v4l2_enc_idx;
	struct v4l2_encoder_cmd p_v4l2_encoder_cmd;
	struct v4l2_dbg_register p_v4l2_dbg_register;
	struct v4l2_dbg_chip_ident p_v4l2_dbg_chip_ident;
	struct v4l2_hw_freq_seek p_v4l2_hw_freq_seek;
	struct v4l2_dv_enum_preset p_v4l2_dv_enum_preset;
	struct v4l2_dv_preset p_v4l2_dv_preset;
	struct v4l2_dv_timings p_v4l2_dv_timings;
	struct v4l2_event p_v4l2_event;
	struct v4l2_event_subscription p_v4l2_event_subscription;
};

#define ioc(cmd) { cmd, #cmd }

/* All defined ioctls */
static const struct {
	u_int32_t cmd;
	const char *name;
} ioctls[] = {
	/* V4L2 ioctls */
	ioc(VIDIOC_QUERYCAP),		/* struct v4l2_capability */
	ioc(VIDIOC_RESERVED),
	ioc(VIDIOC_ENUM_FMT),		/* struct v4l2_fmtdesc */
	ioc(VIDIOC_G_FMT),		/* struct v4l2_format */
	ioc(VIDIOC_S_FMT),		/* struct v4l2_format */
	ioc(VIDIOC_REQBUFS),		/* struct v4l2_requestbuffers */
	ioc(VIDIOC_QUERYBUF),		/* struct v4l2_buffer */
	ioc(VIDIOC_G_FBUF),		/* struct v4l2_framebuffer */
	ioc(VIDIOC_S_FBUF),		/* struct v4l2_framebuffer */
	ioc(VIDIOC_OVERLAY),		/* int */
	ioc(VIDIOC_QBUF),		/* struct v4l2_buffer */
	ioc(VIDIOC_DQBUF),		/* struct v4l2_buffer */
	ioc(VIDIOC_STREAMON),		/* int */
	ioc(VIDIOC_STREAMOFF),		/* int */
	ioc(VIDIOC_G_PARM),		/* struct v4l2_streamparm */
	ioc(VIDIOC_S_PARM),		/* struct v4l2_streamparm */
	ioc(VIDIOC_G_STD),		/* v4l2_std_id */
	ioc(VIDIOC_S_STD),		/* v4l2_std_id */
	ioc(VIDIOC_ENUMSTD),		/* struct v4l2_standard */
	ioc(VIDIOC_ENUMINPUT),		/* struct v4l2_input */
	ioc(VIDIOC_G_CTRL),		/* struct v4l2_control */
	ioc(VIDIOC_S_CTRL),		/* struct v4l2_control */
	ioc(VIDIOC_G_TUNER),		/* struct v4l2_tuner */
	ioc(VIDIOC_S_TUNER),		/* struct v4l2_tuner */
	ioc(VIDIOC_G_AUDIO),		/* struct v4l2_audio */
	ioc(VIDIOC_S_AUDIO),		/* struct v4l2_audio */
	ioc(VIDIOC_QUERYCTRL),		/* struct v4l2_queryctrl */
	ioc(VIDIOC_QUERYMENU),		/* struct v4l2_querymenu */
	ioc(VIDIOC_G_INPUT),		/* int */
	ioc(VIDIOC_S_INPUT),		/* int */
	ioc(VIDIOC_G_OUTPUT),		/* int */
	ioc(VIDIOC_S_OUTPUT),		/* int */
	ioc(VIDIOC_ENUMOUTPUT),		/* struct v4l2_output */
	ioc(VIDIOC_G_AUDOUT),		/* struct v4l2_audioout */
	ioc(VIDIOC_S_AUDOUT),		/* struct v4l2_audioout */
	ioc(VIDIOC_G_MODULATOR),	/* struct v4l2_modulator */
	ioc(VIDIOC_S_MODULATOR),	/* struct v4l2_modulator */
	ioc(VIDIOC_G_FREQUENCY),	/* struct v4l2_frequency */
	ioc(VIDIOC_S_FREQUENCY),	/* struct v4l2_frequency */
	ioc(VIDIOC_CROPCAP),		/* struct v4l2_cropcap */
	ioc(VIDIOC_G_CROP),		/* struct v4l2_crop */
	ioc(VIDIOC_S_CROP),		/* struct v4l2_crop */
	ioc(VIDIOC_G_JPEGCOMP),		/* struct v4l2_jpegcompression */
	ioc(VIDIOC_S_JPEGCOMP),		/* struct v4l2_jpegcompression */
	ioc(VIDIOC_QUERYSTD),		/* v4l2_std_id */
	ioc(VIDIOC_TRY_FMT),		/* struct v4l2_format */
	ioc(VIDIOC_ENUMAUDIO),		/* struct v4l2_audio */
	ioc(VIDIOC_ENUMAUDOUT),		/* struct v4l2_audioout */
	ioc(VIDIOC_G_PRIORITY),		/* enum v4l2_priority */
	ioc(VIDIOC_S_PRIORITY),		/* enum v4l2_priority */
	ioc(VIDIOC_G_SLICED_VBI_CAP),	/* struct v4l2_sliced_vbi_cap */
	ioc(VIDIOC_LOG_STATUS),
	ioc(VIDIOC_G_EXT_CTRLS),	/* struct v4l2_ext_controls */
	ioc(VIDIOC_S_EXT_CTRLS),	/* struct v4l2_ext_controls */
	ioc(VIDIOC_TRY_EXT_CTRLS),	/* struct v4l2_ext_controls */
	ioc(VIDIOC_ENUM_FRAMESIZES),	/* struct v4l2_frmsizeenum */
	ioc(VIDIOC_ENUM_FRAMEINTERVALS),/* struct v4l2_frmivalenum */
	ioc(VIDIOC_G_ENC_INDEX),	/* struct v4l2_enc_idx */
	ioc(VIDIOC_ENCODER_CMD),	/* struct v4l2_encoder_cmd */
	ioc(VIDIOC_TRY_ENCODER_CMD),	/* struct v4l2_encoder_cmd */
	ioc(VIDIOC_DBG_S_REGISTER),	/* struct v4l2_register */
	ioc(VIDIOC_DBG_G_REGISTER),	/* struct v4l2_register */
	ioc(VIDIOC_DBG_G_CHIP_IDENT),	/* struct v4l2_dbg_chip_ident */
	ioc(VIDIOC_S_HW_FREQ_SEEK),	/* struct v4l2_hw_freq_seek */
	ioc(VIDIOC_ENUM_DV_PRESETS),	/* struct v4l2_dv_enum_preset */
	ioc(VIDIOC_S_DV_PRESET),	/* struct v4l2_dv_preset */
	ioc(VIDIOC_G_DV_PRESET),	/* struct v4l2_dv_preset */
	ioc(VIDIOC_QUERY_DV_PRESET),	/* struct v4l2_dv_preset */
	ioc(VIDIOC_S_DV_TIMINGS),	/* struct v4l2_dv_timings */
	ioc(VIDIOC_G_DV_TIMINGS),	/* struct v4l2_dv_timings */
	ioc(VIDIOC_DQEVENT),		/* struct v4l2_event */
	ioc(VIDIOC_SUBSCRIBE_EVENT),	/* struct v4l2_event_subscription */
	ioc(VIDIOC_UNSUBSCRIBE_EVENT),	/* struct v4l2_event_subscription */
};
#define S_IOCTLS sizeof(ioctls)/sizeof(ioctls[0])

/********************************************************************/

int main(int argc, char **argv)
{
	int fd = 0, ret = 0;
	unsigned i;
	unsigned maxlen = 0;
	char *device = "/dev/video0";
	char marker[8] = { 0xde, 0xad, 0xbe, 0xef, 0xad, 0xbc, 0xcb, 0xda };
	char p[sizeof(union v4l_parms) + 2 * sizeof(marker)];
	static const char *dirs[] = {
		"  IO",
		" IOW",
		" IOR",
		"IOWR"
	};

	if (argv[1])
		device = argv[1];
	if ((fd = open(device, O_RDONLY|O_NONBLOCK)) < 0) {
		fprintf(stderr, "Couldn't open %s\n", device);
		return -1;
	}

	for (i = 0; i < S_IOCTLS; i++) {
		if (strlen(ioctls[i].name) > maxlen)
			maxlen = strlen(ioctls[i].name);
	}

	for (i = 0; i < S_IOCTLS; i++) {
		char buf[maxlen + 100];
		const char *name = ioctls[i].name;
		u_int32_t cmd = ioctls[i].cmd;
		int dir = _IOC_DIR(cmd);
		char type = _IOC_TYPE(cmd);
		int nr = _IOC_NR(cmd);
		int sz = _IOC_SIZE(cmd);

		/* Check whether the front and back markers aren't overwritten.
		   Useful to verify the compat32 code. */
		memset(&p, 0, sizeof(p));
		memcpy(p, marker, sizeof(marker));
		memcpy(p + sz + sizeof(marker), marker, sizeof(marker));
		sprintf(buf, "ioctl 0x%08x = %s('%c', %2d, %4d) = %-*s",
			cmd, dirs[dir], type, nr, sz, maxlen, name);
		errno = 0;
		ret = ioctl(fd, cmd, (void *)&p[sizeof(marker)]);
		perror(buf);
		if (memcmp(p, marker, sizeof(marker)))
			fprintf(stderr, "%s: front marker overwritten!\n", name);
		if (memcmp(p + sizeof(marker) + sz, marker, sizeof(marker)))
			fprintf(stderr, "%s: back marker overwritten!\n", name);
	}
	close (fd);
	return 0;
}
