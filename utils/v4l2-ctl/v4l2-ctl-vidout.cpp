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

static struct v4l2_format vfmt_out;	/* set_format/get_format for video */
static unsigned set_fmts_out;

void vidout_usage(void)
{
	printf("\nVideo Output Formats options:\n"
	       "  --list-formats-out display supported video output formats [VIDIOC_ENUM_FMT]\n"
	       "  --list-fields-out  list supported fields for the current output format\n"
	       "  --get-fmt-video-out\n"
	       "     		     query the video output format [VIDIOC_G_FMT]\n"
	       "  --set-fmt-video-out\n"
	       "  --try-fmt-video-out=width=<w>,height=<h>,pixelformat=<pf>,field=<f>\n"
	       "                     set/try the video output format [VIDIOC_TRY_FMT]\n"
	       "                     pixelformat is either the format index as reported by\n"
	       "                     --list-formats-out, or the fourcc value as a string.\n"
	       "                     <f> can be one of:\n"
	       "                     any, none, top, bottom, interlaced, seq_tb, seq_bt,\n"
	       "                     alternate, interlaced_tb, interlaced_bt\n"
	       );
}

static void print_video_out_fields(int fd)
{
	struct v4l2_format fmt;
	struct v4l2_format tmp;

	fmt.type = vidout_buftype;
	if (test_ioctl(fd, VIDIOC_G_FMT, &fmt) < 0)
		return;

	printf("Supported Video Output Fields:\n");
	for (__u32 f = V4L2_FIELD_NONE; f <= V4L2_FIELD_INTERLACED_BT; f++) {
		bool ok;

		tmp = fmt;
		if (is_multiplanar)
			tmp.fmt.pix_mp.field = f;
		else
			tmp.fmt.pix.field = f;
		if (test_ioctl(fd, VIDIOC_TRY_FMT, &tmp) < 0)
			continue;
		if (is_multiplanar)
			ok = tmp.fmt.pix_mp.field == f;
		else
			ok = tmp.fmt.pix.field == f;
		if (ok)
			printf("\t%s\n", field2s(f).c_str());
	}
}

void vidout_cmd(int ch, char *optarg)
{
	__u32 width, height, field, pixfmt;

	switch (ch) {
	case OptSetVideoOutFormat:
	case OptTryVideoOutFormat:
		set_fmts_out = parse_fmt(optarg, width, height, field, pixfmt);
		if (!set_fmts_out) {
			vidcap_usage();
			exit(1);
		}
		if (is_multiplanar) {
			vfmt_out.fmt.pix_mp.width = width;
			vfmt_out.fmt.pix_mp.height = height;
			vfmt_out.fmt.pix_mp.field = field;
			vfmt_out.fmt.pix_mp.pixelformat = pixfmt;
		} else {
			vfmt_out.fmt.pix.width = width;
			vfmt_out.fmt.pix.height = height;
			vfmt_out.fmt.pix.field = field;
			vfmt_out.fmt.pix.pixelformat = pixfmt;
		}
		break;
	}
}

void vidout_set(int fd)
{
	int ret;

	if (options[OptSetVideoOutFormat] || options[OptTryVideoOutFormat]) {
		struct v4l2_format vfmt;

		vfmt.type = vidout_buftype;
		if (doioctl(fd, VIDIOC_G_FMT, &vfmt) == 0) {
			if (is_multiplanar) {
				if (set_fmts_out & FmtWidth)
					vfmt.fmt.pix_mp.width = vfmt_out.fmt.pix_mp.width;
				if (set_fmts_out & FmtHeight)
					vfmt.fmt.pix_mp.height = vfmt_out.fmt.pix_mp.height;
				if (set_fmts_out & FmtPixelFormat) {
					vfmt.fmt.pix_mp.pixelformat = vfmt_out.fmt.pix_mp.pixelformat;
					if (vfmt.fmt.pix_mp.pixelformat < 256) {
						vfmt.fmt.pix_mp.pixelformat =
							find_pixel_format(fd, vfmt.fmt.pix_mp.pixelformat,
									true, true);
					}
				}
				/* G_FMT might return bytesperline values > width,
				 * reset them to 0 to force the driver to update them
				 * to the closest value for the new width. */
				for (unsigned i = 0; i < vfmt.fmt.pix_mp.num_planes; i++)
					vfmt.fmt.pix_mp.plane_fmt[i].bytesperline = 0;
			} else {
				if (set_fmts_out & FmtWidth)
					vfmt.fmt.pix.width = vfmt_out.fmt.pix.width;
				if (set_fmts_out & FmtHeight)
					vfmt.fmt.pix.height = vfmt_out.fmt.pix.height;
				if (set_fmts_out & FmtPixelFormat) {
					vfmt.fmt.pix.pixelformat = vfmt_out.fmt.pix.pixelformat;
					if (vfmt.fmt.pix.pixelformat < 256) {
						vfmt.fmt.pix.pixelformat =
							find_pixel_format(fd, vfmt.fmt.pix.pixelformat,
									true, false);
					}
				}
				/* G_FMT might return a bytesperline value > width,
				 * reset this to 0 to force the driver to update it
				 * to the closest value for the new width. */
				vfmt.fmt.pix.bytesperline = 0;
			}

			if (options[OptSetVideoOutFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &vfmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &vfmt);
			if (ret == 0 && (verbose || options[OptTryVideoOutFormat]))
				printfmt(vfmt);
		}
	}
}

void vidout_get(int fd)
{
	if (options[OptGetVideoOutFormat]) {
		vfmt_out.type = vidout_buftype;
		if (doioctl(fd, VIDIOC_G_FMT, &vfmt_out) == 0)
			printfmt(vfmt_out);
	}
}

void vidout_list(int fd)
{
	if (options[OptListOutFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, vidout_buftype);
	}

	if (options[OptListOutFields]) {
		print_video_out_fields(fd);
	}
}
