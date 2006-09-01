/*
   v4l-ioctl-test - This small utility sends dumb v4l2 ioctl to a device.
   It is meant to check ioctl debug messages generated and to check
	if a function is implemented by that device.
   flag INTERNAL will send v4l internal messages, defined at v4l2-common.h
	and v4l_decoder.h. These messages shouldn't be handled by video
   driver itself, but for internal video and/or audio decoders.

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

/*
 * Internal ioctl doesn't work anymore, due to the changes at
 * v4l2-dev.h.
 */
//#define INTERNAL 1 /* meant for testing ioctl debug msgs */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "linux/videodev.h"

#ifdef INTERNAL
typedef __u8 u8;
typedef __u32 u32;
#include <linux/version.h>
#include "../linux/include/media/v4l2-common.h"
#include <linux/video_decoder.h>
#else
typedef u_int32_t u32;
#endif

/* All possible parameters used on v4l ioctls */
union v4l_parms {
	int		i;
	unsigned long	l;
	u32		u_32;

#ifdef CONFIG_VIDEO_V4L1_COMPAT
	/* V4L1 structs */
	struct vbi_format p_vbi_format;
	struct video_audio p_video_audio;
	struct video_buffer p_video_buffer;
	struct video_capability p_video_capability;
	struct video_capture p_video_capture;
	struct video_channel p_video_channel;
	struct video_code p_video_code;
	struct video_info p_video_info;
	struct video_key p_video_key;
	struct video_mbuf p_video_mbuf;
	struct video_mmap p_video_mmap;
	struct video_picture p_video_picture;
	struct video_play_mode p_video_play_mode;
	struct video_tuner p_video_tuner;
	struct video_unit p_video_unit;
	struct video_window p_video_window;
#endif

	/* V4L2 structs */
	struct v4l2_audioout p_v4l2_audioout;
	struct v4l2_audio p_v4l2_audio;
	struct v4l2_buffer p_v4l2_buffer;
	struct v4l2_control p_v4l2_control;
	struct v4l2_cropcap p_v4l2_cropcap;
	struct v4l2_crop p_v4l2_crop;
	struct v4l2_fmtdesc p_v4l2_fmtdesc;
	struct v4l2_format p_v4l2_format;
	struct v4l2_frequency p_v4l2_frequency;
	struct v4l2_input p_v4l2_input;
	struct v4l2_modulator p_v4l2_modulator;
	struct v4l2_output p_v4l2_output;
	struct v4l2_queryctrl p_v4l2_queryctrl;
	struct v4l2_querymenu p_v4l2_querymenu;
	struct v4l2_requestbuffers p_v4l2_requestbuffers;
	struct v4l2_standard p_v4l2_standard;
	struct v4l2_streamparm p_v4l2_streamparm;
	struct v4l2_tuner p_v4l2_tuner;

#ifdef INTERNAL
	/* Decoder structs */

	struct video_decoder_capability p_video_decoder_capability;
	struct video_decoder_init p_video_decoder_init;

	/* Internal V4L2 structs */
	struct v4l2_register p_v4l2_register;
	struct v4l2_sliced_vbi_data p_v4l2_sliced_vbi_data;
#endif
};

/* All defined ioctls */
int ioctls[] = {
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	/* V4L ioctls */
	VIDIOCCAPTURE,/* int */
	VIDIOCGAUDIO,/* struct video_audio */
	VIDIOCGCAP,/* struct video_capability */
	VIDIOCGCAPTURE,/* struct video_capture */
	VIDIOCGCHAN,/* struct video_channel */
	VIDIOCGFBUF,/* struct video_buffer */
	VIDIOCGFREQ,/* unsigned long */
	VIDIOCGMBUF,/* struct video_mbuf */
	VIDIOCGPICT,/* struct video_picture */
	VIDIOCGPLAYINFO,/* struct video_info */
	VIDIOCGTUNER,/* struct video_tuner */
	VIDIOCGUNIT,/* struct video_unit */
	VIDIOCGVBIFMT,/* struct vbi_format */
	VIDIOCGWIN,/* struct video_window */
	VIDIOCKEY,/* struct video_key */
	VIDIOCMCAPTURE,/* struct video_mmap */
	VIDIOCSAUDIO,/* struct video_audio */
	VIDIOCSCAPTURE,/* struct video_capture */
	VIDIOCSCHAN,/* struct video_channel */
	VIDIOCSFBUF,/* struct video_buffer */
	VIDIOCSFREQ,/* unsigned long */
	VIDIOCSMICROCODE,/* struct video_code */
	VIDIOCSPICT,/* struct video_picture */
	VIDIOCSPLAYMODE,/* struct video_play_mode */
	VIDIOCSTUNER,/* struct video_tuner */
	VIDIOCSVBIFMT,/* struct vbi_format */
	VIDIOCSWIN,/* struct video_window */
	VIDIOCSWRITEMODE,/* int */
	VIDIOCSYNC,/* int */
#endif
	/* V4L2 ioctls */

	VIDIOC_CROPCAP,/* struct v4l2_cropcap */
	VIDIOC_DQBUF,/* struct v4l2_buffer */
	VIDIOC_ENUMAUDIO,/* struct v4l2_audio */
	VIDIOC_ENUMAUDOUT,/* struct v4l2_audioout */
	VIDIOC_ENUM_FMT,/* struct v4l2_fmtdesc */
	VIDIOC_ENUMINPUT,/* struct v4l2_input */
	VIDIOC_ENUMOUTPUT,/* struct v4l2_output */
	VIDIOC_ENUMSTD,/* struct v4l2_standard */
//	VIDIOC_G_AUDIO_OLD,/* struct v4l2_audio */
//	VIDIOC_G_AUDOUT_OLD,/* struct v4l2_audioout */
	VIDIOC_G_CROP,/* struct v4l2_crop */
	VIDIOC_G_CTRL,/* struct v4l2_control */
	VIDIOC_G_FMT,/* struct v4l2_format */
	VIDIOC_G_FREQUENCY,/* struct v4l2_frequency */
	VIDIOC_G_MODULATOR,/* struct v4l2_modulator */
	VIDIOC_G_PARM,/* struct v4l2_streamparm */
	VIDIOC_G_TUNER,/* struct v4l2_tuner */
//	VIDIOC_OVERLAY_OLD,/* int */
	VIDIOC_QBUF,/* struct v4l2_buffer */
	VIDIOC_QUERYBUF,/* struct v4l2_buffer */
	VIDIOC_QUERYCTRL,/* struct v4l2_queryctrl */
	VIDIOC_QUERYMENU,/* struct v4l2_querymenu */
	VIDIOC_REQBUFS,/* struct v4l2_requestbuffers */
	VIDIOC_S_CTRL,/* struct v4l2_control */
	VIDIOC_S_FMT,/* struct v4l2_format */
	VIDIOC_S_INPUT,/* int */
	VIDIOC_S_OUTPUT,/* int */
	VIDIOC_S_PARM,/* struct v4l2_streamparm */
	VIDIOC_TRY_FMT,/* struct v4l2_format */

#ifdef INTERNAL
	/* V4L2 internal ioctls */
	AUDC_SET_RADIO,/* no args */
	TDA9887_SET_CONFIG,/* int */
	TUNER_SET_STANDBY,/* int */
	TUNER_SET_TYPE_ADDR,/* int */

	VIDIOC_INT_AUDIO_CLOCK_FREQ,/* u32 */
	VIDIOC_INT_G_CHIP_IDENT,/* enum v4l2_chip_ident * */
	VIDIOC_INT_I2S_CLOCK_FREQ,/* u32 */
	VIDIOC_INT_S_REGISTER,/* struct v4l2_register */
	VIDIOC_INT_S_VBI_DATA,/* struct v4l2_sliced_vbi_data */

	/* Decoder ioctls */
	 DECODER_ENABLE_OUTPUT,/* int */
	 DECODER_GET_CAPABILITIES,/* struct video_decoder_capability */
	 DECODER_GET_STATUS,/* int */
	 DECODER_INIT,/* struct video_decoder_init */
	 DECODER_SET_GPIO,/* int */
	 DECODER_SET_INPUT,/* int */
	 DECODER_SET_NORM,/* int */
	 DECODER_SET_OUTPUT,/* int */
	 DECODER_SET_PICTURE,/* struct video_picture */
	 DECODER_SET_VBI_BYPASS,/* int */
#endif
};
#define S_IOCTLS sizeof(ioctls)/sizeof(ioctls[0])

/********************************************************************/
int main (void)
{
	int fd=0, ret=0;
	unsigned i;
	char *device="/dev/video0";
	union v4l_parms p;

	if ((fd = open(device, O_RDONLY)) < 0) {
		perror("Couldn't open video0");
		return(-1);
	}

	for (i=0;i<S_IOCTLS;i++) {
		memset(&p,0,sizeof(p));
		ret=ioctl(fd,ioctls[i], (void *) &p);
		printf("%i: ioctl=0x%08x, return=%d\n",i, ioctls[i], ret);
	}

	close (fd);

	return (0);
}
