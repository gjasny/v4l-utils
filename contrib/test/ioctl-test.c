/*
   ioctl-test - This small utility sends dumb v4l2 ioctl to a device.

   It is meant to check ioctl debug messages generated and to check
   if a function is implemented by that device. By compiling it as a
   32-bit binary on a 64-bit architecture it can also be used to test
   the 32-to-64 bit compat layer.

   In addition it also checks that all the ioctl values haven't changed.
   This tool should be run whenever a new ioctl is added, or when fields
   are added to existing structs used by ioctls.

   To compile as a 32-bit executable when on a 64-bit architecture use:

   	gcc -o ioctl-test32 ioctl-test.c -I../../include -m32

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
	struct v4l2_create_buffers p_v4l2_create_buffers;
	struct v4l2_selection p_v4l2_selection;
	struct v4l2_decoder_cmd p_v4l2_decoder_cmd;
	struct v4l2_enum_dv_timings p_v4l2_enum_dv_timings;
	struct v4l2_dv_timings_cap p_v4l2_dv_timings_cap;
	struct v4l2_frequency_band p_v4l2_frequency_band;
	struct v4l2_exportbuffer p_v4l2_exportbuffer;
};

#define ioc(cmd32, cmd64, cmd) { cmd32, cmd64, cmd, #cmd }

/* All defined ioctls */
static const struct {
	u_int32_t cmd32;	/* The 32-bit ioctl value, should never change */
	u_int32_t cmd64;	/* The 64-bit ioctl value, should never change */
	u_int32_t cmd;
	const char *name;
} ioctls[] = {
	/* V4L2 ioctls */
	ioc(0x80685600, 0x80685600, VIDIOC_QUERYCAP),		/* struct v4l2_capability */
	ioc(0x00005601, 0x00005601, VIDIOC_RESERVED),
	ioc(0xc0405602, 0xc0405602, VIDIOC_ENUM_FMT),		/* struct v4l2_fmtdesc */
	ioc(0xc0cc5604, 0xc0d05604, VIDIOC_G_FMT),		/* struct v4l2_format */
	ioc(0xc0cc5605, 0xc0d05605, VIDIOC_S_FMT),		/* struct v4l2_format */
	ioc(0xc0145608, 0xc0145608, VIDIOC_REQBUFS),		/* struct v4l2_requestbuffers */
	ioc(0xc0445609, 0xc0585609, VIDIOC_QUERYBUF),		/* struct v4l2_buffer */
	ioc(0x802c560a, 0x8030560a, VIDIOC_G_FBUF),		/* struct v4l2_framebuffer */
	ioc(0x402c560b, 0x4030560b, VIDIOC_S_FBUF),		/* struct v4l2_framebuffer */
	ioc(0x4004560e, 0x4004560e, VIDIOC_OVERLAY),		/* int */
	ioc(0xc044560f, 0xc058560f, VIDIOC_QBUF),		/* struct v4l2_buffer */
	ioc(0xc0405610, 0xc0405610, VIDIOC_EXPBUF),		/* struct v4l2_exportbuffer */
	ioc(0xc0445611, 0xc0585611, VIDIOC_DQBUF),		/* struct v4l2_buffer */
	ioc(0x40045612, 0x40045612, VIDIOC_STREAMON),		/* int */
	ioc(0x40045613, 0x40045613, VIDIOC_STREAMOFF),		/* int */
	ioc(0xc0cc5615, 0xc0cc5615, VIDIOC_G_PARM),		/* struct v4l2_streamparm */
	ioc(0xc0cc5616, 0xc0cc5616, VIDIOC_S_PARM),		/* struct v4l2_streamparm */
	ioc(0x80085617, 0x80085617, VIDIOC_G_STD),		/* v4l2_std_id */
	ioc(0x40085618, 0x40085618, VIDIOC_S_STD),		/* v4l2_std_id */
	ioc(0xc0405619, 0xc0485619, VIDIOC_ENUMSTD),		/* struct v4l2_standard */
	ioc(0xc04c561a, 0xc050561a, VIDIOC_ENUMINPUT),		/* struct v4l2_input */
	ioc(0xc008561b, 0xc008561b, VIDIOC_G_CTRL),		/* struct v4l2_control */
	ioc(0xc008561c, 0xc008561c, VIDIOC_S_CTRL),		/* struct v4l2_control */
	ioc(0xc054561d, 0xc054561d, VIDIOC_G_TUNER),		/* struct v4l2_tuner */
	ioc(0x4054561e, 0x4054561e, VIDIOC_S_TUNER),		/* struct v4l2_tuner */
	ioc(0x80345621, 0x80345621, VIDIOC_G_AUDIO),		/* struct v4l2_audio */
	ioc(0x40345622, 0x40345622, VIDIOC_S_AUDIO),		/* struct v4l2_audio */
	ioc(0xc0445624, 0xc0445624, VIDIOC_QUERYCTRL),		/* struct v4l2_queryctrl */
	ioc(0xc02c5625, 0xc02c5625, VIDIOC_QUERYMENU),		/* struct v4l2_querymenu */
	ioc(0x80045626, 0x80045626, VIDIOC_G_INPUT),		/* int */
	ioc(0xc0045627, 0xc0045627, VIDIOC_S_INPUT),		/* int */
	ioc(0x8004562e, 0x8004562e, VIDIOC_G_OUTPUT),		/* int */
	ioc(0xc004562f, 0xc004562f, VIDIOC_S_OUTPUT),		/* int */
	ioc(0xc0485630, 0xc0485630, VIDIOC_ENUMOUTPUT),		/* struct v4l2_output */
	ioc(0x80345631, 0x80345631, VIDIOC_G_AUDOUT),		/* struct v4l2_audioout */
	ioc(0x40345632, 0x40345632, VIDIOC_S_AUDOUT),		/* struct v4l2_audioout */
	ioc(0xc0445636, 0xc0445636, VIDIOC_G_MODULATOR),	/* struct v4l2_modulator */
	ioc(0x40445637, 0x40445637, VIDIOC_S_MODULATOR),	/* struct v4l2_modulator */
	ioc(0xc02c5638, 0xc02c5638, VIDIOC_G_FREQUENCY),	/* struct v4l2_frequency */
	ioc(0x402c5639, 0x402c5639, VIDIOC_S_FREQUENCY),	/* struct v4l2_frequency */
	ioc(0xc02c563a, 0xc02c563a, VIDIOC_CROPCAP),		/* struct v4l2_cropcap */
	ioc(0xc014563b, 0xc014563b, VIDIOC_G_CROP),		/* struct v4l2_crop */
	ioc(0x4014563c, 0x4014563c, VIDIOC_S_CROP),		/* struct v4l2_crop */
	ioc(0x808c563d, 0x808c563d, VIDIOC_G_JPEGCOMP),		/* struct v4l2_jpegcompression */
	ioc(0x408c563e, 0x408c563e, VIDIOC_S_JPEGCOMP),		/* struct v4l2_jpegcompression */
	ioc(0x8008563f, 0x8008563f, VIDIOC_QUERYSTD),		/* v4l2_std_id */
	ioc(0xc0cc5640, 0xc0d05640, VIDIOC_TRY_FMT),		/* struct v4l2_format */
	ioc(0xc0345641, 0xc0345641, VIDIOC_ENUMAUDIO),		/* struct v4l2_audio */
	ioc(0xc0345642, 0xc0345642, VIDIOC_ENUMAUDOUT),		/* struct v4l2_audioout */
	ioc(0x80045643, 0x80045643, VIDIOC_G_PRIORITY),		/* enum v4l2_priority */
	ioc(0x40045644, 0x40045644, VIDIOC_S_PRIORITY),		/* enum v4l2_priority */
	ioc(0xc0745645, 0xc0745645, VIDIOC_G_SLICED_VBI_CAP),	/* struct v4l2_sliced_vbi_cap */
	ioc(0x00005646, 0x00005646, VIDIOC_LOG_STATUS),
	ioc(0xc0185647, 0xc0205647, VIDIOC_G_EXT_CTRLS),	/* struct v4l2_ext_controls */
	ioc(0xc0185648, 0xc0205648, VIDIOC_S_EXT_CTRLS),	/* struct v4l2_ext_controls */
	ioc(0xc0185649, 0xc0205649, VIDIOC_TRY_EXT_CTRLS),	/* struct v4l2_ext_controls */
	ioc(0xc02c564a, 0xc02c564a, VIDIOC_ENUM_FRAMESIZES),	/* struct v4l2_frmsizeenum */
	ioc(0xc034564b, 0xc034564b, VIDIOC_ENUM_FRAMEINTERVALS),/* struct v4l2_frmivalenum */
	ioc(0x8818564c, 0x8818564c, VIDIOC_G_ENC_INDEX),	/* struct v4l2_enc_idx */
	ioc(0xc028564d, 0xc028564d, VIDIOC_ENCODER_CMD),	/* struct v4l2_encoder_cmd */
	ioc(0xc028564e, 0xc028564e, VIDIOC_TRY_ENCODER_CMD),	/* struct v4l2_encoder_cmd */
	ioc(0x4038564f, 0x4038564f, VIDIOC_DBG_S_REGISTER),	/* struct v4l2_register */
	ioc(0xc0385650, 0xc0385650, VIDIOC_DBG_G_REGISTER),	/* struct v4l2_register */
	ioc(0xc02c5651, 0xc02c5651, VIDIOC_DBG_G_CHIP_IDENT),	/* struct v4l2_dbg_chip_ident */
	ioc(0x40305652, 0x40305652, VIDIOC_S_HW_FREQ_SEEK),	/* struct v4l2_hw_freq_seek */
	ioc(0xc0405653, 0xc0405653, VIDIOC_ENUM_DV_PRESETS),	/* struct v4l2_dv_enum_preset */
	ioc(0xc0145654, 0xc0145654, VIDIOC_S_DV_PRESET),	/* struct v4l2_dv_preset */
	ioc(0xc0145655, 0xc0145655, VIDIOC_G_DV_PRESET),	/* struct v4l2_dv_preset */
	ioc(0x80145656, 0x80145656, VIDIOC_QUERY_DV_PRESET),	/* struct v4l2_dv_preset */
	ioc(0xc0845657, 0xc0845657, VIDIOC_S_DV_TIMINGS),	/* struct v4l2_dv_timings */
	ioc(0xc0845658, 0xc0845658, VIDIOC_G_DV_TIMINGS),	/* struct v4l2_dv_timings */
	ioc(0x80785659, 0x80885659, VIDIOC_DQEVENT),		/* struct v4l2_event */
	ioc(0x4020565a, 0x4020565a, VIDIOC_SUBSCRIBE_EVENT),	/* struct v4l2_event_subscription */
	ioc(0x4020565b, 0x4020565b, VIDIOC_UNSUBSCRIBE_EVENT),	/* struct v4l2_event_subscription */
	ioc(0xc0f8565c, 0xc100565c, VIDIOC_CREATE_BUFS),	/* struct v4l2_create_buffers */
	ioc(0xc044565d, 0xc058565d, VIDIOC_PREPARE_BUF),	/* struct v4l2_buffer */
	ioc(0xc040565e, 0xc040565e, VIDIOC_G_SELECTION),	/* struct v4l2_selection */
	ioc(0xc040565f, 0xc040565f, VIDIOC_S_SELECTION),	/* struct v4l2_selection */
	ioc(0xc0485660, 0xc0485660, VIDIOC_DECODER_CMD),	/* struct v4l2_decoder_cmd */
	ioc(0xc0485661, 0xc0485661, VIDIOC_TRY_DECODER_CMD),	/* struct v4l2_decoder_cmd */
	ioc(0xc0945662, 0xc0945662, VIDIOC_ENUM_DV_TIMINGS),	/* struct v4l2_enum_dv_timings */
	ioc(0x80845663, 0x80845663, VIDIOC_QUERY_DV_TIMINGS),	/* struct v4l2_dv_timings */
	ioc(0xc0905664, 0xc0905664, VIDIOC_DV_TIMINGS_CAP),	/* struct v4l2_dv_timings_cap */
	ioc(0xc0405665, 0xc0405665, VIDIOC_ENUM_FREQ_BANDS),	/* struct v4l2_frequency_band */
};
#define S_IOCTLS sizeof(ioctls)/sizeof(ioctls[0])

/********************************************************************/

int main(int argc, char **argv)
{
	int fd = 0;
	unsigned i;
	unsigned maxlen = 0;
	int cmd_errors = 0;
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
	
	for (i = 0; i < S_IOCTLS; i++) {
		unsigned cmp_cmd;

		if (strlen(ioctls[i].name) > maxlen)
			maxlen = strlen(ioctls[i].name);
		if (sizeof(long) == 4)
			cmp_cmd = ioctls[i].cmd32;
		else
			cmp_cmd = ioctls[i].cmd64;
		if (cmp_cmd != ioctls[i].cmd) {
			fprintf(stderr, "%s: ioctl is 0x%08x but should be 0x%08x!\n",
					ioctls[i].name, ioctls[i].cmd, cmp_cmd);
			cmd_errors = 1;
		}
	}
	if (cmd_errors)
		return -1;

	if ((fd = open(device, O_RDONLY|O_NONBLOCK)) < 0) {
		fprintf(stderr, "Couldn't open %s\n", device);
		return -1;
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
		ioctl(fd, cmd, (void *)&p[sizeof(marker)]);
		perror(buf);
		if (memcmp(p, marker, sizeof(marker)))
			fprintf(stderr, "%s: front marker overwritten!\n", name);
		if (memcmp(p + sizeof(marker) + sz, marker, sizeof(marker)))
			fprintf(stderr, "%s: back marker overwritten!\n", name);
	}
	close (fd);
	return 0;
}
