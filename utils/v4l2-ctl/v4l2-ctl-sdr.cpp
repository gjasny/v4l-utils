#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <dirent.h>
#include <math.h>

#include "v4l2-ctl.h"

static struct v4l2_format vfmt;	/* set_format/get_format */

void sdr_usage(void)
{
	printf("\nSDR Formats options:\n"
	       "  --list-formats-sdr display supported SDR capture formats [VIDIOC_ENUM_FMT]\n"
	       "  --get-fmt-sdr      query the SDR capture format [VIDIOC_G_FMT]\n"
	       "  --set-fmt-sdr=<f>  set the SDR capture format [VIDIOC_S_FMT]\n"
	       "                     parameter is either the format index as reported by\n"
	       "                     --list-formats-sdr-cap, or the fourcc value as a string\n"
	       "  --try-fmt-sdr=<f>  try the SDR capture format [VIDIOC_TRY_FMT]\n"
	       "                     parameter is either the format index as reported by\n"
	       "                     --list-formats-sdr-cap, or the fourcc value as a string\n"
	       "  --list-formats-sdr-out\n"
	       "                     display supported SDR output formats [VIDIOC_ENUM_FMT]\n"
	       "  --get-fmt-sdr-out  query the SDR output format [VIDIOC_G_FMT]\n"
	       "  --set-fmt-sdr-out=<f>\n"
	       "                     set the SDR output format [VIDIOC_S_FMT]\n"
	       "                     parameter is either the format index as reported by\n"
	       "                     --list-formats-sdr-out, or the fourcc value as a string\n"
	       "  --try-fmt-sdr-out=<f>\n"
	       "                     try the SDR output format [VIDIOC_TRY_FMT]\n"
	       "                     parameter is either the format index as reported by\n"
	       "                     --list-formats-sdr-out, or the fourcc value as a string\n"
	       );
}

void sdr_cmd(int ch, char *optarg)
{
	switch (ch) {
	case OptSetSdrFormat:
	case OptTrySdrFormat:
	case OptSetSdrOutFormat:
	case OptTrySdrOutFormat:
		if (strlen(optarg) == 0) {
			sdr_usage();
			exit(1);
		} else if (strlen(optarg) == 4) {
			vfmt.fmt.sdr.pixelformat = v4l2_fourcc(optarg[0],
					optarg[1], optarg[2], optarg[3]);
		} else {
			vfmt.fmt.sdr.pixelformat = strtol(optarg, 0L, 0);
		}
		break;
	}
}

void sdr_set(int fd)
{
	int ret;

	if (options[OptSetSdrFormat] || options[OptTrySdrFormat]) {
		struct v4l2_format in_vfmt;

		in_vfmt.type = V4L2_BUF_TYPE_SDR_CAPTURE;
		in_vfmt.fmt.sdr.pixelformat = vfmt.fmt.sdr.pixelformat;

		if (in_vfmt.fmt.sdr.pixelformat < 256) {
			struct v4l2_fmtdesc fmt;

			fmt.index = in_vfmt.fmt.sdr.pixelformat;
			fmt.type = V4L2_BUF_TYPE_SDR_CAPTURE;

			if (doioctl(fd, VIDIOC_ENUM_FMT, &fmt))
				fmt.pixelformat = 0;

			in_vfmt.fmt.sdr.pixelformat = fmt.pixelformat;
		}

		if (options[OptSetSdrFormat])
			ret = doioctl(fd, VIDIOC_S_FMT, &in_vfmt);
		else
			ret = doioctl(fd, VIDIOC_TRY_FMT, &in_vfmt);
		if (ret == 0 && (verbose || options[OptTrySdrFormat]))
			printfmt(in_vfmt);
	}
	if (options[OptSetSdrOutFormat] || options[OptTrySdrOutFormat]) {
		struct v4l2_format in_vfmt;

		in_vfmt.type = V4L2_BUF_TYPE_SDR_OUTPUT;
		in_vfmt.fmt.sdr.pixelformat = vfmt.fmt.sdr.pixelformat;

		if (in_vfmt.fmt.sdr.pixelformat < 256) {
			struct v4l2_fmtdesc fmt;

			fmt.index = in_vfmt.fmt.sdr.pixelformat;
			fmt.type = V4L2_BUF_TYPE_SDR_OUTPUT;

			if (doioctl(fd, VIDIOC_ENUM_FMT, &fmt))
				fmt.pixelformat = 0;

			in_vfmt.fmt.sdr.pixelformat = fmt.pixelformat;
		}

		if (options[OptSetSdrOutFormat])
			ret = doioctl(fd, VIDIOC_S_FMT, &in_vfmt);
		else
			ret = doioctl(fd, VIDIOC_TRY_FMT, &in_vfmt);
		if (ret == 0 && (verbose || options[OptTrySdrOutFormat]))
			printfmt(in_vfmt);
	}
}

void sdr_get(int fd)
{
	if (options[OptGetSdrFormat]) {
		vfmt.type = V4L2_BUF_TYPE_SDR_CAPTURE;
		if (doioctl(fd, VIDIOC_G_FMT, &vfmt) == 0)
			printfmt(vfmt);
	}
	if (options[OptGetSdrOutFormat]) {
		vfmt.type = V4L2_BUF_TYPE_SDR_OUTPUT;
		if (doioctl(fd, VIDIOC_G_FMT, &vfmt) == 0)
			printfmt(vfmt);
	}
}

void sdr_list(int fd)
{
	if (options[OptListSdrFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_SDR_CAPTURE);
	}
	if (options[OptListSdrOutFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_SDR_OUTPUT);
	}
}
