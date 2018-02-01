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

struct mbus_name {
	const char *name;
	__u32 code;
};

static struct mbus_name mbus_names[] = {
	{ "Fixed", MEDIA_BUS_FMT_FIXED },
#include "media-bus-format-names.h"
	{ NULL, 0 }
};

static __u32 list_mbus_codes_pad;
static __u32 get_fmt_pad;
static __u32 get_sel_pad;
static int get_sel_target = -1;
static struct v4l2_subdev_frame_size_enum frmsize;
static struct v4l2_subdev_frame_interval_enum frmival;

void subdev_usage(void)
{
	printf("\nSub-Device options:\n"
	       "  --list-subdev-mbus-codes=<pad>\n"
	       "                      display supported mediabus codes for this pad (0 is default)\n"
	       "                      [VIDIOC_SUBDEV_ENUM_MBUS_CODE]\n"
	       "  --list-subdev-framesizes=pad=<pad>,code=<code>\n"
	       "                     list supported framesizes for this pad and code\n"
	       "                     [VIDIOC_SUBDEV_ENUM_FRAME_SIZE]\n"
	       "                     <code> is the value of the mediabus code\n"
	       "  --list-subdev-frameintervals=pad=<pad>,width=<w>,height=<h>,code=<code>\n"
	       "                     list supported frame intervals for this pad and code and\n"
	       "                     the given width and height [VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL]\n"
	       "                     <code> is the value of the mediabus code\n"
	       "  --get-subdev-fmt[=<pad>]\n"
	       "     		     query the frame format for the given pad [VIDIOC_SUBDEV_G_FMT]\n"
	       "  --get-subdev-selection=pad=<pad>,target=<target>\n"
	       "                     query the frame selection rectangle [VIDIOC_SUBDEV_G_SELECTION]\n"
	       "                     target=crop|crop_bounds|crop_default|compose|compose_bounds|\n"
	       "                            compose_default|compose_padded|native_size\n"
	       );
}

void subdev_cmd(int ch, char *optarg)
{
	char *value, *subs;

	switch (ch) {
	case OptListSubDevMBusCodes:
		if (optarg)
			list_mbus_codes_pad = strtoul(optarg, 0L, 0);
		break;
	case OptListSubDevFrameSizes:
		subs = optarg;
		while (*subs != '\0') {
			static const char *const subopts[] = {
				"pad",
				"code",
				NULL
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				frmsize.pad = strtoul(value, 0L, 0);
				break;
			case 1:
				frmsize.code = strtoul(value, 0L, 0);
				break;
			default:
				subdev_usage();
				exit(1);
			}
		}
		break;
	case OptListSubDevFrameIntervals:
		subs = optarg;
		while (*subs != '\0') {
			static const char *const subopts[] = {
				"pad",
				"code",
				"width",
				"height",
				NULL
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				frmival.pad = strtoul(value, 0L, 0);
				break;
			case 1:
				frmival.code = strtoul(value, 0L, 0);
				break;
			case 2:
				frmival.width = strtoul(value, 0L, 0);
				break;
			case 3:
				frmival.height = strtoul(value, 0L, 0);
				break;
			default:
				subdev_usage();
				exit(1);
			}
		}
		break;
	case OptGetSubDevFormat:
		if (optarg)
			get_fmt_pad = strtoul(optarg, 0L, 0);
		break;
	case OptGetSubDevSelection:
		subs = optarg;
		while (*subs != '\0') {
			static const char *const subopts[] = {
				"pad",
				"target",
				NULL
			};
			unsigned int target;

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				get_sel_pad = strtoul(value, 0L, 0);
				break;
			case 1:
				if (parse_selection_target(value, target)) {
					fprintf(stderr, "Unknown selection target\n");
					subdev_usage();
					exit(1);
				}
				get_sel_target = target;
				break;
			default:
				subdev_usage();
				exit(1);
			}
		}
		break;
	default:
		break;
	}
}

static const char *which2s(__u32 which)
{
	return which ? "V4L2_SUBDEV_FORMAT_ACTIVE" : "V4L2_SUBDEV_FORMAT_TRY";
}

void subdev_set(int fd, __u32 which)
{
}

static bool is_rgb_or_hsv_code(__u32 code)
{
	return code < 0x2000 || code >= 0x3000;
}

static void print_framefmt(const struct v4l2_mbus_framefmt &fmt)
{
	__u32 colsp = fmt.colorspace;
	__u32 ycbcr_enc = fmt.ycbcr_enc;
	unsigned int i;

	for (i = 0; mbus_names[i].name; i++)
		if (mbus_names[i].code == fmt.code)
			break;
	printf("\tWidth/Height      : %u/%u\n", fmt.width, fmt.height);
	printf("\tMediabus Code     : ");
	if (mbus_names[i].name)
		printf("0x%04x (MEDIA_BUS_FMT_%s)\n",
		       fmt.code, mbus_names[i].name);
	else
		printf("0x%04x\n", fmt.code);

	printf("\tField             : %s\n", field2s(fmt.field).c_str());
	printf("\tColorspace        : %s\n", colorspace2s(colsp).c_str());
	printf("\tTransfer Function : %s", xfer_func2s(fmt.xfer_func).c_str());
	if (fmt.xfer_func == V4L2_XFER_FUNC_DEFAULT)
		printf(" (maps to %s)",
		       xfer_func2s(V4L2_MAP_XFER_FUNC_DEFAULT(colsp)).c_str());
	printf("\n");
	printf("\tYCbCr/HSV Encoding: %s", ycbcr_enc2s(ycbcr_enc).c_str());
	if (ycbcr_enc == V4L2_YCBCR_ENC_DEFAULT) {
		ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(colsp);
		printf(" (maps to %s)", ycbcr_enc2s(ycbcr_enc).c_str());
	}
	printf("\n");
	printf("\tQuantization      : %s", quantization2s(fmt.quantization).c_str());
	if (fmt.quantization == V4L2_QUANTIZATION_DEFAULT)
		printf(" (maps to %s)",
		       quantization2s(V4L2_MAP_QUANTIZATION_DEFAULT(is_rgb_or_hsv_code(fmt.code),
								    colsp, ycbcr_enc)).c_str());
	printf("\n");
}

static void print_subdev_selection(const struct v4l2_subdev_selection &sel)
{
	printf("Selection: %s, Left %d, Top %d, Width %d, Height %d, Flags: %s\n",
			seltarget2s(sel.target).c_str(),
			sel.r.left, sel.r.top, sel.r.width, sel.r.height,
			selflags2s(sel.flags).c_str());
}

void subdev_get(int fd, __u32 which)
{
	if (options[OptGetSubDevFormat]) {
		struct v4l2_subdev_format fmt;

		memset(&fmt, 0, sizeof(fmt));
		fmt.which = which;
		fmt.pad = get_fmt_pad;

		printf("ioctl: VIDIOC_SUBDEV_G_FMT (%s, pad=%u)\n",
		       which2s(which), fmt.pad);
		if (doioctl(fd, VIDIOC_SUBDEV_G_FMT, &fmt) == 0)
			print_framefmt(fmt.format);
	}

	if (options[OptGetSubDevSelection]) {
		struct v4l2_subdev_selection sel;
		int t = 0;

		memset(&sel, 0, sizeof(sel));
		sel.which = which;
		sel.pad = get_sel_pad;

		printf("ioctl: VIDIOC_SUBDEV_G_SELECTION (%s, pad=%u)\n",
		       which2s(which), sel.pad);
		if (options[OptAll] || get_sel_target == -1) {
			while (selection_targets_def[t].str != NULL) {
				sel.target = selection_targets_def[t].flag;
				if (test_ioctl(fd, VIDIOC_SUBDEV_G_SELECTION, &sel) == 0)
					print_subdev_selection(sel);
				t++;
			}
		} else {
			sel.target = get_sel_target;
			if (doioctl(fd, VIDIOC_SUBDEV_G_SELECTION, &sel) == 0)
				print_subdev_selection(sel);
		}
	}
}

static void print_mbus_code(__u32 code)
{
	unsigned int i;

	for (i = 0; mbus_names[i].name; i++)
		if (mbus_names[i].code == code)
			break;
	if (mbus_names[i].name)
		printf("\t0x%04x: MEDIA_BUS_FMT_%s\n",
		       mbus_names[i].code, mbus_names[i].name);
	else
		printf("\t0x%04x\n", code);
}

static void print_mbus_codes(int fd, __u32 pad, __u32 which)
{
	struct v4l2_subdev_mbus_code_enum mbus_code;

	memset(&mbus_code, 0, sizeof(mbus_code));
	mbus_code.pad = pad;
	mbus_code.which = which;

	for (;;) {
		int ret = test_ioctl(fd, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_code);

		if (ret)
			break;
		print_mbus_code(mbus_code.code);
		mbus_code.index++;
	}
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

static void print_frmsize(const struct v4l2_subdev_frame_size_enum &frmsize)
{
	printf("\tSize Range: %dx%d - %dx%d\n",
	       frmsize.min_width, frmsize.min_height,
	       frmsize.max_width, frmsize.max_height);
}

static void print_frmival(const struct v4l2_subdev_frame_interval_enum &frmival)
{
	printf("\tInterval: %ss (%s fps)\n", fract2sec(frmival.interval).c_str(),
	       fract2fps(frmival.interval).c_str());
}

void subdev_list(int fd, __u32 which)
{
	if (options[OptListSubDevMBusCodes]) {
		printf("ioctl: VIDIOC_SUBDEV_ENUM_MBUS_CODE (%s, pad=%u)\n",
		       which2s(which), list_mbus_codes_pad);
		print_mbus_codes(fd, list_mbus_codes_pad, which);
	}
	if (options[OptListSubDevFrameSizes]) {
		printf("ioctl: VIDIOC_SUBDEV_ENUM_FRAME_SIZE (%s, pad=%u)\n",
		       which2s(which), frmsize.pad);
		frmsize.index = 0;
		frmsize.which = which;
		while (test_ioctl(fd, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &frmsize) >= 0) {
			print_frmsize(frmsize);
			frmsize.index++;
		}
	}
	if (options[OptListSubDevFrameIntervals]) {
		printf("ioctl: VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL (%s, pad=%u)\n",
		       which2s(which), frmival.pad);
		frmival.index = 0;
		frmival.which = which;
		while (test_ioctl(fd, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &frmival) >= 0) {
			print_frmival(frmival);
			frmival.index++;
		}
	}
}
