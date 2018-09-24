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

/* selection specified */
#define SelectionWidth		(1L<<0)
#define SelectionHeight		(1L<<1)
#define SelectionLeft		(1L<<2)
#define SelectionTop 		(1L<<3)
#define SelectionFlags 		(1L<<4)

static __u32 list_mbus_codes_pad;
static __u32 get_fmt_pad;
static __u32 get_sel_pad;
static __u32 get_fps_pad;
static int get_sel_target = -1;
static unsigned int set_selection;
static struct v4l2_subdev_selection vsel;
static unsigned int set_fmt;
static __u32 set_fmt_pad;
static struct v4l2_mbus_framefmt ffmt;
static struct v4l2_subdev_frame_size_enum frmsize;
static struct v4l2_subdev_frame_interval_enum frmival;
static __u32 set_fps_pad;
static double set_fps;

void subdev_usage(void)
{
	printf("\nSub-Device options:\n"
	       "  --list-subdev-mbus-codes <pad>\n"
	       "                      display supported mediabus codes for this pad (0 is default)\n"
	       "                      [VIDIOC_SUBDEV_ENUM_MBUS_CODE]\n"
	       "  --list-subdev-framesizes pad=<pad>,code=<code>\n"
	       "                     list supported framesizes for this pad and code\n"
	       "                     [VIDIOC_SUBDEV_ENUM_FRAME_SIZE]\n"
	       "                     <code> is the value of the mediabus code\n"
	       "  --list-subdev-frameintervals pad=<pad>,width=<w>,height=<h>,code=<code>\n"
	       "                     list supported frame intervals for this pad and code and\n"
	       "                     the given width and height [VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL]\n"
	       "                     <code> is the value of the mediabus code\n"
	       "  --get-subdev-fmt [<pad>]\n"
	       "     		     query the frame format for the given pad [VIDIOC_SUBDEV_G_FMT]\n"
	       "  --get-subdev-selection pad=<pad>,target=<target>\n"
	       "                     query the frame selection rectangle [VIDIOC_SUBDEV_G_SELECTION]\n"
	       "                     See --set-subdev-selection command for the valid <target> values.\n"
	       "  --get-subdev-fps [<pad>]\n"
	       "                     query the frame rate [VIDIOC_SUBDEV_G_FRAME_INTERVAL]\n"
	       "  --set-subdev-fmt   (for testing only, otherwise use media-ctl)\n"
	       "  --try-subdev-fmt pad=<pad>,width=<w>,height=<h>,code=<code>,field=<f>,colorspace=<c>,\n"
	       "                   xfer=<xf>,ycbcr=<y>,hsv=<hsv>,quantization=<q>\n"
	       "                     set the frame format [VIDIOC_SUBDEV_S_FMT]\n"
	       "                     <code> is the value of the mediabus code\n"
	       "                     <f> can be one of the following field layouts:\n"
	       "                       any, none, top, bottom, interlaced, seq_tb, seq_bt,\n"
	       "                       alternate, interlaced_tb, interlaced_bt\n"
	       "                     <c> can be one of the following colorspaces:\n"
	       "                       smpte170m, smpte240m, rec709, 470m, 470bg, jpeg, srgb,\n"
	       "                       oprgb, bt2020, dcip3\n"
	       "                     <xf> can be one of the following transfer functions:\n"
	       "                       default, 709, srgb, oprgb, smpte240m, smpte2084, dcip3, none\n"
	       "                     <y> can be one of the following Y'CbCr encodings:\n"
	       "                       default, 601, 709, xv601, xv709, bt2020, bt2020c, smpte240m\n"
	       "                     <hsv> can be one of the following HSV encodings:\n"
	       "                       default, 180, 256\n"
	       "                     <q> can be one of the following quantization methods:\n"
	       "                       default, full-range, lim-range\n"
	       "  --set-subdev-selection (for testing only, otherwise use media-ctl)\n"
	       "  --try-subdev-selection pad=<pad>,target=<target>,flags=<flags>,\n"
	       "                         top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video capture selection rectangle [VIDIOC_SUBDEV_S_SELECTION]\n"
	       "                     target=crop|crop_bounds|crop_default|compose|compose_bounds|\n"
	       "                            compose_default|compose_padded|native_size\n"
	       "                     flags=le|ge|keep-config\n"
	       "  --set-subdev-fps pad=<pad>,fps=<fps> (for testing only, otherwise use media-ctl)\n"
	       "                     set the frame rate [VIDIOC_SUBDEV_S_FRAME_INTERVAL]\n"
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
	case OptGetSubDevFPS:
		if (optarg)
			get_fps_pad = strtoul(optarg, 0L, 0);
		break;
	case OptSetSubDevFormat:
	case OptTrySubDevFormat:
		ffmt.field = V4L2_FIELD_ANY;
		subs = optarg;
		while (*subs != '\0') {
			static const char *const subopts[] = {
				"width",
				"height",
				"code",
				"field",
				"colorspace",
				"ycbcr",
				"hsv",
				"quantization",
				"xfer",
				"pad",
				NULL
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				ffmt.width = strtoul(value, 0L, 0);
				set_fmt |= FmtWidth;
				break;
			case 1:
				ffmt.height = strtoul(value, 0L, 0);
				set_fmt |= FmtHeight;
				break;
			case 2:
				ffmt.code = strtoul(value, 0L, 0);
				set_fmt |= FmtPixelFormat;
				break;
			case 3:
				ffmt.field = parse_field(value);
				set_fmt |= FmtField;
				break;
			case 4:
				ffmt.colorspace = parse_colorspace(value);
				if (ffmt.colorspace)
					set_fmt |= FmtColorspace;
				else
					fprintf(stderr, "unknown colorspace %s\n", value);
				break;
			case 5:
				ffmt.ycbcr_enc = parse_ycbcr(value);
				set_fmt |= FmtYCbCr;
				break;
			case 6:
				ffmt.ycbcr_enc = parse_hsv(value);
				set_fmt |= FmtYCbCr;
				break;
			case 7:
				ffmt.quantization = parse_quantization(value);
				set_fmt |= FmtQuantization;
				break;
			case 8:
				ffmt.xfer_func = parse_xfer_func(value);
				set_fmt |= FmtXferFunc;
				break;
			case 9:
				set_fmt_pad = strtoul(value, 0L, 0);
				break;
			default:
				fprintf(stderr, "Unknown option\n");
				subdev_usage();
				exit(1);
			}
		}
		break;
	case OptSetSubDevSelection:
	case OptTrySubDevSelection:
		subs = optarg;

		while (*subs != '\0') {
			static const char *const subopts[] = {
				"target",
				"flags",
				"left",
				"top",
				"width",
				"height",
				"pad",
				NULL
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				if (parse_selection_target(value, vsel.target)) {
					fprintf(stderr, "Unknown selection target\n");
					subdev_usage();
					exit(1);
				}
				break;
			case 1:
				vsel.flags = parse_selection_flags(value);
				set_selection |= SelectionFlags;
				break;
			case 2:
				vsel.r.left = strtol(value, 0L, 0);
				set_selection |= SelectionLeft;
				break;
			case 3:
				vsel.r.top = strtol(value, 0L, 0);
				set_selection |= SelectionTop;
				break;
			case 4:
				vsel.r.width = strtoul(value, 0L, 0);
				set_selection |= SelectionWidth;
				break;
			case 5:
				vsel.r.height = strtoul(value, 0L, 0);
				set_selection |= SelectionHeight;
				break;
			case 6:
				vsel.pad = strtoul(value, 0L, 0);
				break;
			default:
				fprintf(stderr, "Unknown option\n");
				subdev_usage();
				exit(1);
			}
		}
		break;
	case OptSetSubDevFPS:
		subs = optarg;

		while (*subs != '\0') {
			static const char *const subopts[] = {
				"pad",
				"fps",
				NULL
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				set_fps_pad = strtoul(value, 0L, 0);
				break;
			case 1:
				set_fps = strtod(value, NULL);
				break;
			default:
				fprintf(stderr, "Unknown option\n");
				subdev_usage();
				exit(1);
			}
		}
		break;
	default:
		break;
	}
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

void subdev_set(cv4l_fd &_fd)
{
	int fd = _fd.g_fd();

	if (options[OptSetSubDevFormat] || options[OptTrySubDevFormat]) {
		struct v4l2_subdev_format fmt;

		memset(&fmt, 0, sizeof(fmt));
		fmt.pad = set_fmt_pad;
		fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;

		if (doioctl(fd, VIDIOC_SUBDEV_G_FMT, &fmt) == 0) {
			int ret;

			if (set_fmt & FmtWidth)
				fmt.format.width = ffmt.width;
			if (set_fmt & FmtHeight)
				fmt.format.height = ffmt.height;
			if (set_fmt & FmtPixelFormat)
				fmt.format.code = ffmt.code;
			if (set_fmt & FmtField)
				fmt.format.field = ffmt.field;
			if (set_fmt & FmtColorspace)
				fmt.format.colorspace = ffmt.colorspace;
			if (set_fmt & FmtXferFunc)
				fmt.format.xfer_func = ffmt.xfer_func;
			if (set_fmt & FmtYCbCr)
				fmt.format.ycbcr_enc = ffmt.ycbcr_enc;
			if (set_fmt & FmtQuantization)
				fmt.format.quantization = ffmt.quantization;

			if (options[OptSetSubDevFormat])
				printf("Note: --set-subdev-fmt is only for testing.\n"
				       "Normally media-ctl is used to configure the video pipeline.\n");
			else
				fmt.which = V4L2_SUBDEV_FORMAT_TRY;

			printf("ioctl: VIDIOC_SUBDEV_S_FMT (pad=%u)\n", fmt.pad);
			ret = doioctl(fd, VIDIOC_SUBDEV_S_FMT, &fmt);
			if (ret == 0 && (verbose || !options[OptSetSubDevFormat]))
				print_framefmt(fmt.format);
		}
	}
	if (options[OptSetSubDevSelection] || options[OptTrySubDevSelection]) {
		struct v4l2_subdev_selection sel;

		memset(&sel, 0, sizeof(sel));
		sel.pad = vsel.pad;
		sel.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		sel.target = vsel.target;

		if (doioctl(fd, VIDIOC_SUBDEV_G_SELECTION, &sel) == 0) {
			if (set_selection & SelectionWidth)
				sel.r.width = vsel.r.width;
			if (set_selection & SelectionHeight)
				sel.r.height = vsel.r.height;
			if (set_selection & SelectionLeft)
				sel.r.left = vsel.r.left;
			if (set_selection & SelectionTop)
				sel.r.top = vsel.r.top;
			sel.flags = (set_selection & SelectionFlags) ? vsel.flags : 0;

			if (options[OptSetSubDevSelection])
				printf("Note: --set-subdev-selection is only for testing.\n"
				       "Normally media-ctl is used to configure the video pipeline.\n");
			else
				sel.which = V4L2_SUBDEV_FORMAT_TRY;

			printf("ioctl: VIDIOC_SUBDEV_S_SELECTION (pad=%u)\n", sel.pad);
			int ret = doioctl(fd, VIDIOC_SUBDEV_S_SELECTION, &sel);
			if (ret == 0 && (verbose || !options[OptSetSubDevSelection]))
				print_subdev_selection(sel);
		}
	}
	if (options[OptSetSubDevFPS]) {
		struct v4l2_subdev_frame_interval fival;

		memset(&fival, 0, sizeof(fival));
		fival.pad = set_fps_pad;

		if (set_fps <= 0) {
			fprintf(stderr, "invalid fps %f\n", set_fps);
			subdev_usage();
			exit(1);
		}
		fival.interval.numerator = 1000;
		fival.interval.denominator = set_fps * fival.interval.numerator;
		printf("Note: --set-subdev-fps is only for testing.\n"
		       "Normally media-ctl is used to configure the video pipeline.\n");
		printf("ioctl: VIDIOC_SUBDEV_S_FRAME_INTERVAL (pad=%u)\n", fival.pad);
		if (doioctl(fd, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &fival) == 0) {
			if (!fival.interval.denominator || !fival.interval.numerator)
				printf("\tFrames per second: invalid (%d/%d)\n",
					fival.interval.denominator, fival.interval.numerator);
			else
				printf("\tFrames per second: %.3f (%d/%d)\n",
					(1.0 * fival.interval.denominator) / fival.interval.numerator,
					fival.interval.denominator, fival.interval.numerator);
		}
	}
}

void subdev_get(cv4l_fd &_fd)
{
	int fd = _fd.g_fd();

	if (options[OptGetSubDevFormat]) {
		struct v4l2_subdev_format fmt;

		memset(&fmt, 0, sizeof(fmt));
		fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		fmt.pad = get_fmt_pad;

		printf("ioctl: VIDIOC_SUBDEV_G_FMT (pad=%u)\n", fmt.pad);
		if (doioctl(fd, VIDIOC_SUBDEV_G_FMT, &fmt) == 0)
			print_framefmt(fmt.format);
	}

	if (options[OptGetSubDevSelection]) {
		struct v4l2_subdev_selection sel;
		unsigned idx = 0;

		memset(&sel, 0, sizeof(sel));
		sel.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		sel.pad = get_sel_pad;

		printf("ioctl: VIDIOC_SUBDEV_G_SELECTION (pad=%u)\n", sel.pad);
		if (options[OptAll] || get_sel_target == -1) {
			while (valid_seltarget_at_idx(idx)) {
				sel.target = seltarget_at_idx(idx);
				if (test_ioctl(fd, VIDIOC_SUBDEV_G_SELECTION, &sel) == 0)
					print_subdev_selection(sel);
				idx++;
			}
		} else {
			sel.target = get_sel_target;
			if (doioctl(fd, VIDIOC_SUBDEV_G_SELECTION, &sel) == 0)
				print_subdev_selection(sel);
		}
	}
	if (options[OptGetSubDevFPS]) {
		struct v4l2_subdev_frame_interval fival;

		memset(&fival, 0, sizeof(fival));
		fival.pad = get_fps_pad;

		printf("ioctl: VIDIOC_SUBDEV_G_FRAME_INTERVAL (pad=%u)\n", fival.pad);
		if (doioctl(fd, VIDIOC_SUBDEV_G_FRAME_INTERVAL, &fival) == 0) {
			if (!fival.interval.denominator || !fival.interval.numerator)
				printf("\tFrames per second: invalid (%d/%d)\n",
					fival.interval.denominator, fival.interval.numerator);
			else
				printf("\tFrames per second: %.3f (%d/%d)\n",
					(1.0 * fival.interval.denominator) / fival.interval.numerator,
					fival.interval.denominator, fival.interval.numerator);
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

static void print_mbus_codes(int fd, __u32 pad)
{
	struct v4l2_subdev_mbus_code_enum mbus_code;

	memset(&mbus_code, 0, sizeof(mbus_code));
	mbus_code.pad = pad;
	mbus_code.which = V4L2_SUBDEV_FORMAT_TRY;

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

void subdev_list(cv4l_fd &_fd)
{
	int fd = _fd.g_fd();

	if (options[OptListSubDevMBusCodes]) {
		printf("ioctl: VIDIOC_SUBDEV_ENUM_MBUS_CODE (pad=%u)\n",
		       list_mbus_codes_pad);
		print_mbus_codes(fd, list_mbus_codes_pad);
	}
	if (options[OptListSubDevFrameSizes]) {
		printf("ioctl: VIDIOC_SUBDEV_ENUM_FRAME_SIZE (pad=%u)\n",
		       frmsize.pad);
		frmsize.index = 0;
		frmsize.which = V4L2_SUBDEV_FORMAT_TRY;
		while (test_ioctl(fd, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &frmsize) >= 0) {
			print_frmsize(frmsize);
			frmsize.index++;
		}
	}
	if (options[OptListSubDevFrameIntervals]) {
		printf("ioctl: VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL (pad=%u)\n",
		       frmival.pad);
		frmival.index = 0;
		frmival.which = V4L2_SUBDEV_FORMAT_TRY;
		while (test_ioctl(fd, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &frmival) >= 0) {
			print_frmival(frmival);
			frmival.index++;
		}
	}
}
