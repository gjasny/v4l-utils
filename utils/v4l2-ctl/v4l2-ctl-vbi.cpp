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
#include <config.h>

#include <linux/videodev2.h>
#include <libv4l2.h>
#include <string>

#include "v4l2-ctl.h"

	struct v4l2_format vbi_fmt;	/* set_format/get_format for sliced VBI */
	struct v4l2_format vbi_fmt_out;	/* set_format/get_format for sliced VBI output */
	struct v4l2_format raw_fmt;	/* set_format/get_format for VBI */
	struct v4l2_format raw_fmt_out;	/* set_format/get_format for VBI output */

void vbi_usage(void)
{
	printf("\nVBI Formats options:\n"
	       "  --get-sliced-vbi-cap\n"
	       "		     query the sliced VBI capture capabilities\n"
	       "                     [VIDIOC_G_SLICED_VBI_CAP]\n"
	       "  --get-sliced-vbi-out-cap\n"
	       "		     query the sliced VBI output capabilities\n"
	       "                     [VIDIOC_G_SLICED_VBI_CAP]\n"
	       "  -B, --get-fmt-sliced-vbi\n"
	       "		     query the sliced VBI capture format [VIDIOC_G_FMT]\n"
	       "  --get-fmt-sliced-vbi-out\n"
	       "		     query the sliced VBI output format [VIDIOC_G_FMT]\n"
	       "  -b, --set-fmt-sliced-vbi\n"
	       "  --try-fmt-sliced-vbi\n"
	       "  --set-fmt-sliced-vbi-out\n"
	       "  --try-fmt-sliced-vbi-out=<mode>\n"
	       "                     set/try the sliced VBI capture/output format to <mode>\n"
	       "                     [VIDIOC_S/TRY_FMT], <mode> is a comma separated list of:\n"
	       "                     off:      turn off sliced VBI (cannot be combined with\n"
	       "                               other modes)\n"
	       "                     teletext: teletext (PAL/SECAM)\n"
	       "                     cc:       closed caption (NTSC)\n"
	       "                     wss:      widescreen signal (PAL/SECAM)\n"
	       "                     vps:      VPS (PAL/SECAM)\n"
	       "  --get-fmt-vbi      query the VBI capture format [VIDIOC_G_FMT]\n"
	       "  --get-fmt-vbi-out  query the VBI output format [VIDIOC_G_FMT]\n"
	       );
}

static void print_sliced_vbi_cap(struct v4l2_sliced_vbi_cap &cap)
{
	printf("\tType           : %s\n", buftype2s(cap.type).c_str());
	printf("\tService Set    : %s\n",
			service2s(cap.service_set).c_str());
	for (int i = 0; i < 24; i++) {
		printf("\tService Line %2d: %8s / %-8s\n", i,
				service2s(cap.service_lines[0][i]).c_str(),
				service2s(cap.service_lines[1][i]).c_str());
	}
}

void vbi_cmd(int ch, char *optarg)
{
	char *value, *subs;
	bool found_off = false;
	v4l2_format *fmt = &vbi_fmt;

	switch (ch) {
	case OptSetSlicedVbiOutFormat:
	case OptTrySlicedVbiOutFormat:
		fmt = &vbi_fmt_out;
		/* fall through */
	case OptSetSlicedVbiFormat:
	case OptTrySlicedVbiFormat:
		fmt->fmt.sliced.service_set = 0;
		subs = optarg;
		while (*subs != '\0') {
			static const char *const subopts[] = {
				"off",
				"teletext",
				"cc",
				"wss",
				"vps",
				NULL
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				found_off = true;
				break;
			case 1:
				fmt->fmt.sliced.service_set |=
					V4L2_SLICED_TELETEXT_B;
				break;
			case 2:
				fmt->fmt.sliced.service_set |=
					V4L2_SLICED_CAPTION_525;
				break;
			case 3:
				fmt->fmt.sliced.service_set |=
					V4L2_SLICED_WSS_625;
				break;
			case 4:
				fmt->fmt.sliced.service_set |=
					V4L2_SLICED_VPS;
				break;
			default:
				vbi_usage();
				break;
			}
		}
		if (found_off && fmt->fmt.sliced.service_set) {
			fprintf(stderr, "Sliced VBI mode 'off' cannot be combined with other modes\n");
			vbi_usage();
			exit(1);
		}
		break;
	}
}

void vbi_set(int fd)
{
	int ret;

	if (options[OptSetSlicedVbiFormat] || options[OptTrySlicedVbiFormat]) {
		vbi_fmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
		if (options[OptSetSlicedVbiFormat])
			ret = doioctl(fd, VIDIOC_S_FMT, &vbi_fmt);
		else
			ret = doioctl(fd, VIDIOC_TRY_FMT, &vbi_fmt);
		if (ret == 0 && (verbose || options[OptTrySlicedVbiFormat]))
			printfmt(vbi_fmt);
	}

	if (options[OptSetSlicedVbiOutFormat] || options[OptTrySlicedVbiOutFormat]) {
		vbi_fmt_out.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
		if (options[OptSetSlicedVbiOutFormat])
			ret = doioctl(fd, VIDIOC_S_FMT, &vbi_fmt_out);
		else
			ret = doioctl(fd, VIDIOC_TRY_FMT, &vbi_fmt_out);
		if (ret == 0 && (verbose || options[OptTrySlicedVbiOutFormat]))
			printfmt(vbi_fmt_out);
	}
}

void vbi_get(int fd)
{
	if (options[OptGetSlicedVbiFormat]) {
		vbi_fmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
		if (doioctl(fd, VIDIOC_G_FMT, &vbi_fmt) == 0)
			printfmt(vbi_fmt);
	}

	if (options[OptGetSlicedVbiOutFormat]) {
		vbi_fmt_out.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
		if (doioctl(fd, VIDIOC_G_FMT, &vbi_fmt_out) == 0)
			printfmt(vbi_fmt_out);
	}

	if (options[OptGetVbiFormat]) {
		raw_fmt.type = V4L2_BUF_TYPE_VBI_CAPTURE;
		if (doioctl(fd, VIDIOC_G_FMT, &raw_fmt) == 0)
			printfmt(raw_fmt);
	}

	if (options[OptGetVbiOutFormat]) {
		raw_fmt_out.type = V4L2_BUF_TYPE_VBI_OUTPUT;
		if (doioctl(fd, VIDIOC_G_FMT, &raw_fmt_out) == 0)
			printfmt(raw_fmt_out);
	}
}

void vbi_list(int fd)
{
	if (options[OptGetSlicedVbiCap]) {
		struct v4l2_sliced_vbi_cap cap;

		cap.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
		if (doioctl(fd, VIDIOC_G_SLICED_VBI_CAP, &cap) == 0) {
			print_sliced_vbi_cap(cap);
		}
	}

	if (options[OptGetSlicedVbiOutCap]) {
		struct v4l2_sliced_vbi_cap cap;

		cap.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
		if (doioctl(fd, VIDIOC_G_SLICED_VBI_CAP, &cap) == 0) {
			print_sliced_vbi_cap(cap);
		}
	}
}
