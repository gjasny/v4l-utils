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

static struct v4l2_format vfmt;	/* set_format/get_format for video */
static struct v4l2_frmsizeenum frmsize; /* list frame sizes */
static struct v4l2_frmivalenum frmival; /* list frame intervals */
static unsigned set_fmts;

void vidcap_usage(void)
{
	printf("\nVideo Capture Formats options:\n"
	       "  --list-formats     display supported video formats [VIDIOC_ENUM_FMT]\n"
	       "  --list-formats-mplane\n"
 	       "                     display supported video multi-planar formats\n"
 	       "                     [VIDIOC_ENUM_FMT]\n"
	       "  --list-formats-ext display supported video formats including frame sizes\n"
	       "                     and intervals\n"
	       "  --list-formats-ext-mplane\n"
	       "                     display supported video multi-planar formats including\n"
	       "                     frame sizes and intervals\n"
	       "  --list-framesizes=<f>\n"
	       "                     list supported framesizes for pixelformat <f>\n"
	       "                     [VIDIOC_ENUM_FRAMESIZES]\n"
	       "                     pixelformat is the fourcc value as a string\n"
	       "  --list-frameintervals=width=<w>,height=<h>,pixelformat=<f>\n"
	       "                     list supported frame intervals for pixelformat <f> and\n"
	       "                     the given width and height [VIDIOC_ENUM_FRAMEINTERVALS]\n"
	       "                     pixelformat is the fourcc value as a string\n"
	       "  -V, --get-fmt-video\n"
	       "     		     query the video capture format [VIDIOC_G_FMT]\n"
	       "  -v, --set-fmt-video=width=<w>,height=<h>,pixelformat=<f>\n"
	       "                     set the video capture format [VIDIOC_S_FMT]\n"
	       "                     pixelformat is either the format index as reported by\n"
	       "                     --list-formats, or the fourcc value as a string\n"
	       "  --try-fmt-video=width=<w>,height=<h>,pixelformat=<f>\n"
	       "                     try the video capture format [VIDIOC_TRY_FMT]\n"
	       "                     pixelformat is either the format index as reported by\n"
	       "                     --list-formats, or the fourcc value as a string\n"
	       "  --get-fmt-video-mplane\n"
	       "     		     query the video capture format through the multi-planar API\n"
	       "                     [VIDIOC_G_FMT]\n"
	       "  --set-fmt-video-mplane\n"
	       "  --try-fmt-video-mplane=width=<w>,height=<h>,pixelformat=<f>\n"
	       "                     set/try the video capture format using the multi-planar API\n"
	       "                     [VIDIOC_S/TRY_FMT]\n"
	       "                     pixelformat is either the format index as reported by\n"
	       "                     --list-formats-mplane, or the fourcc value as a string\n"
	       );
}

static std::string frmtype2s(unsigned type)
{
	static const char *types[] = {
		"Unknown",
		"Discrete",
		"Continuous",
		"Stepwise"
	};

	if (type > 3)
		type = 0;
	return types[type];
}

static std::string fract2sec(const struct v4l2_fract &f)
{
	char buf[100];

	sprintf(buf, "%.3f", (1.0 * f.numerator) / f.denominator);
	return buf;
}

static std::string fract2fps(const struct v4l2_fract &f)
{
	char buf[100];

	sprintf(buf, "%.3f", (1.0 * f.denominator) / f.numerator);
	return buf;
}

static void print_frmsize(const struct v4l2_frmsizeenum &frmsize, const char *prefix)
{
	printf("%s\tSize: %s ", prefix, frmtype2s(frmsize.type).c_str());
	if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
		printf("%dx%d", frmsize.discrete.width, frmsize.discrete.height);
	} else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
		printf("%dx%d - %dx%d with step %d/%d",
				frmsize.stepwise.min_width,
				frmsize.stepwise.min_height,
				frmsize.stepwise.max_width,
				frmsize.stepwise.max_height,
				frmsize.stepwise.step_width,
				frmsize.stepwise.step_height);
	}
	printf("\n");
}

static void print_frmival(const struct v4l2_frmivalenum &frmival, const char *prefix)
{
	printf("%s\tInterval: %s ", prefix, frmtype2s(frmival.type).c_str());
	if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
		printf("%ss (%s fps)\n", fract2sec(frmival.discrete).c_str(),
				fract2fps(frmival.discrete).c_str());
	} else if (frmival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
		printf("%ss - %ss with step %ss (%s-%s fps)\n",
				fract2sec(frmival.stepwise.min).c_str(),
				fract2sec(frmival.stepwise.max).c_str(),
				fract2sec(frmival.stepwise.step).c_str(),
				fract2fps(frmival.stepwise.max).c_str(),
				fract2fps(frmival.stepwise.min).c_str());
	}
}

static void print_video_formats_ext(int fd, enum v4l2_buf_type type)
{
	struct v4l2_fmtdesc fmt;
	struct v4l2_frmsizeenum frmsize;
	struct v4l2_frmivalenum frmival;

	fmt.index = 0;
	fmt.type = type;
	while (test_ioctl(fd, VIDIOC_ENUM_FMT, &fmt) >= 0) {
		printf("\tIndex       : %d\n", fmt.index);
		printf("\tType        : %s\n", buftype2s(type).c_str());
		printf("\tPixel Format: '%s'", fcc2s(fmt.pixelformat).c_str());
		if (fmt.flags)
			printf(" (%s)", fmtdesc2s(fmt.flags).c_str());
		printf("\n");
		printf("\tName        : %s\n", fmt.description);
		frmsize.pixel_format = fmt.pixelformat;
		frmsize.index = 0;
		while (test_ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
			print_frmsize(frmsize, "\t");
			if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
				frmival.index = 0;
				frmival.pixel_format = fmt.pixelformat;
				frmival.width = frmsize.discrete.width;
				frmival.height = frmsize.discrete.height;
				while (test_ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) >= 0) {
					print_frmival(frmival, "\t\t");
					frmival.index++;
				}
			}
			frmsize.index++;
		}
		printf("\n");
		fmt.index++;
	}
}

void vidcap_cmd(int ch, char *optarg)
{
	__u32 width, height, pixfmt;
	char *value, *subs;

	switch (ch) {
	case OptSetVideoMplaneFormat:
	case OptTryVideoMplaneFormat:
		set_fmts = parse_fmt(optarg, width, height, pixfmt);
		if (!set_fmts) {
			vidcap_usage();
			exit(1);
		}
		vfmt.fmt.pix_mp.width = width;
		vfmt.fmt.pix_mp.height = height;
		vfmt.fmt.pix_mp.pixelformat = pixfmt;
		break;

	case OptSetVideoFormat:
	case OptTryVideoFormat:
		set_fmts = parse_fmt(optarg, width, height, pixfmt);
		if (!set_fmts) {
			vidcap_usage();
			exit(1);
		}
		vfmt.fmt.pix.width = width;
		vfmt.fmt.pix.height = height;
		vfmt.fmt.pix.pixelformat = pixfmt;
		break;
	case OptListFrameSizes:
		if (strlen(optarg) == 4)
			frmsize.pixel_format = v4l2_fourcc(optarg[0], optarg[1],
					optarg[2], optarg[3]);
		else
			frmsize.pixel_format = strtol(optarg, 0L, 0);
		break;
	case OptListFrameIntervals:
		subs = optarg;
		while (*subs != '\0') {
			static const char *const subopts[] = {
				"width",
				"height",
				"pixelformat",
				NULL
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				frmival.width = strtol(value, 0L, 0);
				break;
			case 1:
				frmival.height = strtol(value, 0L, 0);
				break;
			case 2:
				if (strlen(value) == 4)
					frmival.pixel_format =
						v4l2_fourcc(value[0], value[1],
								value[2], value[3]);
				else
					frmival.pixel_format = strtol(value, 0L, 0);
				break;
			default:
				vidcap_usage();
				break;
			}
		}
		break;
	}
}

void vidcap_set(int fd)
{
	int ret;

	if (options[OptSetVideoFormat] || options[OptTryVideoFormat]) {
		struct v4l2_format in_vfmt;

		in_vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (doioctl(fd, VIDIOC_G_FMT, &in_vfmt) == 0) {
			if (set_fmts & FmtWidth)
				in_vfmt.fmt.pix.width = vfmt.fmt.pix.width;
			if (set_fmts & FmtHeight)
				in_vfmt.fmt.pix.height = vfmt.fmt.pix.height;
			if (set_fmts & FmtPixelFormat) {
				in_vfmt.fmt.pix.pixelformat = vfmt.fmt.pix.pixelformat;
				if (in_vfmt.fmt.pix.pixelformat < 256) {
					in_vfmt.fmt.pix.pixelformat =
						find_pixel_format(fd, in_vfmt.fmt.pix.pixelformat,
								  false, false);
				}
			}
			/* G_FMT might return a bytesperline value > width,
			 * reset this to 0 to force the driver to update it
			 * to the closest value for the new width. */
			in_vfmt.fmt.pix.bytesperline = 0;
			if (options[OptSetVideoFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &in_vfmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &in_vfmt);
			if (ret == 0 && (verbose || options[OptTryVideoFormat]))
				printfmt(in_vfmt);
		}
	}

	if (options[OptSetVideoMplaneFormat] || options[OptTryVideoMplaneFormat]) {
		struct v4l2_format in_vfmt;

		in_vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		if (doioctl(fd, VIDIOC_G_FMT, &in_vfmt) == 0) {
			if (set_fmts & FmtWidth)
				in_vfmt.fmt.pix_mp.width = vfmt.fmt.pix_mp.width;
			if (set_fmts & FmtHeight)
				in_vfmt.fmt.pix_mp.height = vfmt.fmt.pix_mp.height;
			if (set_fmts & FmtPixelFormat) {
				in_vfmt.fmt.pix_mp.pixelformat = vfmt.fmt.pix_mp.pixelformat;
				if (in_vfmt.fmt.pix_mp.pixelformat < 256) {
					in_vfmt.fmt.pix_mp.pixelformat =
						find_pixel_format(fd, in_vfmt.fmt.pix_mp.pixelformat,
								  false, true);
				}
			}
			/* G_FMT might return bytesperline values > width,
			 * reset them to 0 to force the driver to update them
			 * to the closest value for the new width. */
			for (unsigned i = 0; i < in_vfmt.fmt.pix_mp.num_planes; i++)
				in_vfmt.fmt.pix_mp.plane_fmt[i].bytesperline = 0;
			if (options[OptSetVideoMplaneFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &in_vfmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &in_vfmt);
			if (ret == 0 && (verbose || options[OptTryVideoMplaneFormat]))
				printfmt(in_vfmt);
		}
	}
}

void vidcap_get(int fd)
{
	if (options[OptGetVideoFormat]) {
		vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (doioctl(fd, VIDIOC_G_FMT, &vfmt) == 0)
			printfmt(vfmt);
	}

	if (options[OptGetVideoMplaneFormat]) {
		vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		if (doioctl(fd, VIDIOC_G_FMT, &vfmt) == 0)
			printfmt(vfmt);
	}
}

void vidcap_list(int fd)
{
	if (options[OptListFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	}

	if (options[OptListMplaneFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	}

	if (options[OptListFormatsExt]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats_ext(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	}

	if (options[OptListMplaneFormatsExt]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats_ext(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	}

	if (options[OptListFrameSizes]) {
		printf("ioctl: VIDIOC_ENUM_FRAMESIZES\n");
		frmsize.index = 0;
		while (test_ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
			print_frmsize(frmsize, "");
			frmsize.index++;
		}
	}

	if (options[OptListFrameIntervals]) {
		printf("ioctl: VIDIOC_ENUM_FRAMEINTERVALS\n");
		frmival.index = 0;
		while (test_ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) >= 0) {
			print_frmival(frmival, "");
			frmival.index++;
		}
	}
}
