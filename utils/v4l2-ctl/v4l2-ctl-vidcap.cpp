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

static struct v4l2_frmsizeenum frmsize; /* list frame sizes */
static struct v4l2_frmivalenum frmival; /* list frame intervals */
static unsigned set_fmts;
static __u32 width, height, pixfmt, field, flags;
static __u32 bytesperline[VIDEO_MAX_PLANES];

void vidcap_usage(void)
{
	printf("\nVideo Capture Formats options:\n"
	       "  --list-formats     display supported video formats [VIDIOC_ENUM_FMT]\n"
	       "  --list-formats-ext display supported video formats including frame sizes\n"
	       "                     and intervals\n"
	       "  --list-framesizes=<f>\n"
	       "                     list supported framesizes for pixelformat <f>\n"
	       "                     [VIDIOC_ENUM_FRAMESIZES]\n"
	       "                     pixelformat is the fourcc value as a string\n"
	       "  --list-frameintervals=width=<w>,height=<h>,pixelformat=<f>\n"
	       "                     list supported frame intervals for pixelformat <f> and\n"
	       "                     the given width and height [VIDIOC_ENUM_FRAMEINTERVALS]\n"
	       "                     pixelformat is the fourcc value as a string\n"
	       "  --list-fields      list supported fields for the current format\n"
	       "  -V, --get-fmt-video\n"
	       "     		     query the video capture format [VIDIOC_G_FMT]\n"
	       "  -v, --set-fmt-video\n"
	       "  --try-fmt-video=width=<w>,height=<h>,pixelformat=<pf>,field=<f>,colorspace=<c>,\n"
	       "                  xfer=<xf>,ycbcr=<y>,quantization=<q>,premul-alpha,bytesperline=<bpl>\n"
	       "                     set/try the video capture format [VIDIOC_S/TRY_FMT]\n"
	       "                     pixelformat is either the format index as reported by\n"
	       "                       --list-formats, or the fourcc value as a string.\n"
	       "                     The bytesperline option can be used multiple times, once for each plane.\n"
	       "                     premul-alpha sets V4L2_PIX_FMT_FLAG_PREMUL_ALPHA.\n"
	       "                     <f> can be one of the following field layouts:\n"
	       "                       any, none, top, bottom, interlaced, seq_tb, seq_bt,\n"
	       "                       alternate, interlaced_tb, interlaced_bt\n"
	       "                     <c> can be one of the following colorspaces:\n"
	       "                       smpte170m, smpte240m, rec709, 470m, 470bg, jpeg, srgb,\n"
	       "                       adobergb, bt2020, dcip3\n"
	       "                     <xf> can be one of the following transfer functions:\n"
	       "                       default, 709, srgb, adobergb, smpte240m, smpte2084, dcip3, none\n"
	       "                     <y> can be one of the following Y'CbCr encodings:\n"
	       "                       default, 601, 709, xv601, xv709, bt2020, bt2020c, smpte240m\n"
	       "                     <q> can be one of the following quantization methods:\n"
	       "                       default, full-range, lim-range\n"
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
	} else if (frmival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
		printf("%ss - %ss (%s-%s fps)\n",
				fract2sec(frmival.stepwise.min).c_str(),
				fract2sec(frmival.stepwise.max).c_str(),
				fract2fps(frmival.stepwise.max).c_str(),
				fract2fps(frmival.stepwise.min).c_str());
	} else if (frmival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
		printf("%ss - %ss with step %ss (%s-%s fps)\n",
				fract2sec(frmival.stepwise.min).c_str(),
				fract2sec(frmival.stepwise.max).c_str(),
				fract2sec(frmival.stepwise.step).c_str(),
				fract2fps(frmival.stepwise.max).c_str(),
				fract2fps(frmival.stepwise.min).c_str());
	}
}

static void print_video_formats_ext(int fd, __u32 type)
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

static void print_video_fields(int fd)
{
	struct v4l2_format fmt;
	struct v4l2_format tmp;

	memset(&fmt, 0, sizeof(fmt));
	fmt.fmt.pix.priv = priv_magic;
	fmt.type = vidcap_buftype;
	if (test_ioctl(fd, VIDIOC_G_FMT, &fmt) < 0)
		return;

	printf("Supported Video Fields:\n");
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

void vidcap_cmd(int ch, char *optarg)
{
	__u32 colorspace, xfer_func, ycbcr, quantization;
	char *value, *subs;

	switch (ch) {
	case OptSetVideoFormat:
	case OptTryVideoFormat:
		set_fmts = parse_fmt(optarg, width, height, pixfmt, field, colorspace,
				xfer_func, ycbcr, quantization, flags, bytesperline);
		if (!set_fmts ||
		    (set_fmts & (FmtColorspace | FmtYCbCr | FmtQuantization | FmtXferFunc))) {
			vidcap_usage();
			exit(1);
		}
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
				exit(1);
			}
		}
		break;
	}
}

void vidcap_set(int fd)
{
	int ret;

	if (options[OptSetVideoFormat] || options[OptTryVideoFormat]) {
		struct v4l2_format vfmt;

		memset(&vfmt, 0, sizeof(vfmt));
		vfmt.fmt.pix.priv = priv_magic;
		vfmt.type = vidcap_buftype;

		if (doioctl(fd, VIDIOC_G_FMT, &vfmt) == 0) {
			if (is_multiplanar) {
				if (set_fmts & FmtWidth)
					vfmt.fmt.pix_mp.width = width;
				if (set_fmts & FmtHeight)
					vfmt.fmt.pix_mp.height = height;
				if (set_fmts & FmtPixelFormat) {
					vfmt.fmt.pix_mp.pixelformat = pixfmt;
					if (vfmt.fmt.pix_mp.pixelformat < 256) {
						vfmt.fmt.pix_mp.pixelformat =
							find_pixel_format(fd, vfmt.fmt.pix_mp.pixelformat,
									false, true);
					}
				}
				if (set_fmts & FmtField)
					vfmt.fmt.pix_mp.field = field;
				if (set_fmts & FmtFlags)
					vfmt.fmt.pix_mp.flags = flags;
				if (set_fmts & FmtBytesPerLine) {
					for (unsigned i = 0; i < VIDEO_MAX_PLANES; i++)
						vfmt.fmt.pix_mp.plane_fmt[i].bytesperline =
							bytesperline[i];
				} else {
					/* G_FMT might return bytesperline values > width,
					 * reset them to 0 to force the driver to update them
					 * to the closest value for the new width. */
					for (unsigned i = 0; i < vfmt.fmt.pix_mp.num_planes; i++)
						vfmt.fmt.pix_mp.plane_fmt[i].bytesperline = 0;
				}
			} else {
				if (set_fmts & FmtWidth)
					vfmt.fmt.pix.width = width;
				if (set_fmts & FmtHeight)
					vfmt.fmt.pix.height = height;
				if (set_fmts & FmtPixelFormat) {
					vfmt.fmt.pix.pixelformat = pixfmt;
					if (vfmt.fmt.pix.pixelformat < 256) {
						vfmt.fmt.pix.pixelformat =
							find_pixel_format(fd, vfmt.fmt.pix.pixelformat,
									false, false);
					}
				}
				if (set_fmts & FmtField)
					vfmt.fmt.pix.field = field;
				if (set_fmts & FmtFlags)
					vfmt.fmt.pix.flags = flags;
				if (set_fmts & FmtBytesPerLine) {
					vfmt.fmt.pix.bytesperline = bytesperline[0];
				} else {
					/* G_FMT might return a bytesperline value > width,
					 * reset this to 0 to force the driver to update it
					 * to the closest value for the new width. */
					vfmt.fmt.pix.bytesperline = 0;
				}
			}

			if (options[OptSetVideoFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &vfmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &vfmt);
			if (ret == 0 && (verbose || options[OptTryVideoFormat]))
				printfmt(vfmt);
		}
	}
}

void vidcap_get(int fd)
{
	if (options[OptGetVideoFormat]) {
		struct v4l2_format vfmt;

		memset(&vfmt, 0, sizeof(vfmt));
		vfmt.fmt.pix.priv = priv_magic;
		vfmt.type = vidcap_buftype;
		if (doioctl(fd, VIDIOC_G_FMT, &vfmt) == 0)
			printfmt(vfmt);
	}
}

void vidcap_list(int fd)
{
	if (options[OptListFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, vidcap_buftype);
	}

	if (options[OptListFormatsExt]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats_ext(fd, vidcap_buftype);
	}

	if (options[OptListFields]) {
		print_video_fields(fd);
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
