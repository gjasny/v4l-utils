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

static unsigned int set_fbuf;
static unsigned int set_overlay_fmt;
static unsigned int set_overlay_fmt_out;
static struct v4l2_format overlay_fmt;	/* set_format/get_format video overlay */
static struct v4l2_format overlay_fmt_out;	/* set_format/get_format video overlay output */
static struct v4l2_framebuffer fbuf;   /* fbuf */
static int overlay;			/* overlay */

void overlay_usage(void)
{
	printf("\nVideo Overlay options:\n"
	       "  --list-formats-overlay\n"
	       "                     display supported overlay formats [VIDIOC_ENUM_FMT]\n"
	       "  --overlay=<on>     turn overlay on (1) or off (0) (VIDIOC_OVERLAY)\n"
	       "  --get-fmt-overlay  query the video overlay format [VIDIOC_G_FMT]\n"
	       "  --get-fmt-output-overlay\n"
	       "     		     query the video output overlay format [VIDIOC_G_FMT]\n"
	       "  --set-fmt-overlay\n"
	       "  --try-fmt-overlay\n"
	       "  --set-fmt-output-overlay\n"
	       "  --try-fmt-output-overlay=chromakey=<key>,global_alpha=<alpha>,\n"
	       "                           top=<t>,left=<l>,width=<w>,height=<h>,field=<f>\n"
	       "     		     set/try the video or video output overlay format\n"
	       "                     [VIDIOC_S/TRY_FMT], <f> can be one of:\n"
	       "                     any, none, top, bottom, interlaced, seq_tb, seq_bt,\n"
	       "                     alternate, interlaced_tb, interlaced_bt\n"
	       "  --get-fbuf         query the overlay framebuffer data [VIDIOC_G_FBUF]\n"
	       "  --set-fbuf=chromakey=<b>,global_alpha=<b>,local_alpha=<b>,local_inv_alpha=<b>\n"
	       "		     set the overlay framebuffer [VIDIOC_S_FBUF]\n"
	       "                     b = 0 or 1\n"
	       );
}

static std::string fbufcap2s(unsigned cap)
{
	std::string s;

	if (cap & V4L2_FBUF_CAP_EXTERNOVERLAY)
		s += "\t\t\tExtern Overlay\n";
	if (cap & V4L2_FBUF_CAP_CHROMAKEY)
		s += "\t\t\tChromakey\n";
	if (cap & V4L2_FBUF_CAP_GLOBAL_ALPHA)
		s += "\t\t\tGlobal Alpha\n";
	if (cap & V4L2_FBUF_CAP_LOCAL_ALPHA)
		s += "\t\t\tLocal Alpha\n";
	if (cap & V4L2_FBUF_CAP_LOCAL_INV_ALPHA)
		s += "\t\t\tLocal Inverted Alpha\n";
	if (cap & V4L2_FBUF_CAP_LIST_CLIPPING)
		s += "\t\t\tClipping List\n";
	if (cap & V4L2_FBUF_CAP_BITMAP_CLIPPING)
		s += "\t\t\tClipping Bitmap\n";
	if (s.empty()) s += "\t\t\t\n";
	return s;
}

static std::string fbufflags2s(unsigned fl)
{
	std::string s;

	if (fl & V4L2_FBUF_FLAG_PRIMARY)
		s += "\t\t\tPrimary Graphics Surface\n";
	if (fl & V4L2_FBUF_FLAG_OVERLAY)
		s += "\t\t\tOverlay Matches Capture/Output Size\n";
	if (fl & V4L2_FBUF_FLAG_CHROMAKEY)
		s += "\t\t\tChromakey\n";
	if (fl & V4L2_FBUF_FLAG_GLOBAL_ALPHA)
		s += "\t\t\tGlobal Alpha\n";
	if (fl & V4L2_FBUF_FLAG_LOCAL_ALPHA)
		s += "\t\t\tLocal Alpha\n";
	if (fl & V4L2_FBUF_FLAG_LOCAL_INV_ALPHA)
		s += "\t\t\tLocal Inverted Alpha\n";
	if (s.empty()) s += "\t\t\t\n";
	return s;
}

static void printfbuf(const struct v4l2_framebuffer &fb)
{
	int is_ext = fb.capability & V4L2_FBUF_CAP_EXTERNOVERLAY;

	printf("Framebuffer Format:\n");
	printf("\tCapability    : %s", fbufcap2s(fb.capability).c_str() + 3);
	printf("\tFlags         : %s", fbufflags2s(fb.flags).c_str() + 3);
	if (fb.base)
		printf("\tBase          : 0x%p\n", fb.base);
	printf("\tWidth         : %d\n", fb.fmt.width);
	printf("\tHeight        : %d\n", fb.fmt.height);
	printf("\tPixel Format  : '%s'\n", fcc2s(fb.fmt.pixelformat).c_str());
	if (!is_ext) {
		printf("\tBytes per Line: %d\n", fb.fmt.bytesperline);
		printf("\tSize image    : %d\n", fb.fmt.sizeimage);
		printf("\tColorspace    : %s\n", colorspace2s(fb.fmt.colorspace).c_str());
		if (fb.fmt.priv)
			printf("\tCustom Info   : %08x\n", fb.fmt.priv);
	}
}

static enum v4l2_field parse_field(const char *s)
{
	if (!strcmp(s, "any")) return V4L2_FIELD_ANY;
	if (!strcmp(s, "none")) return V4L2_FIELD_NONE;
	if (!strcmp(s, "top")) return V4L2_FIELD_TOP;
	if (!strcmp(s, "bottom")) return V4L2_FIELD_BOTTOM;
	if (!strcmp(s, "interlaced")) return V4L2_FIELD_INTERLACED;
	if (!strcmp(s, "seq_tb")) return V4L2_FIELD_SEQ_TB;
	if (!strcmp(s, "seq_bt")) return V4L2_FIELD_SEQ_BT;
	if (!strcmp(s, "alternate")) return V4L2_FIELD_ALTERNATE;
	if (!strcmp(s, "interlaced_tb")) return V4L2_FIELD_INTERLACED_TB;
	if (!strcmp(s, "interlaced_bt")) return V4L2_FIELD_INTERLACED_BT;
	return V4L2_FIELD_ANY;
}

void overlay_cmd(int ch, char *optarg)
{
	unsigned int *set_overlay_fmt_ptr = NULL;
	struct v4l2_format *overlay_fmt_ptr = NULL;
	char *value, *subs;

	switch (ch) {
	case OptSetOverlayFormat:
	case OptTryOverlayFormat:
	case OptSetOutputOverlayFormat:
	case OptTryOutputOverlayFormat:
		switch (ch) {
		case OptSetOverlayFormat:
		case OptTryOverlayFormat:
			set_overlay_fmt_ptr = &set_overlay_fmt;
			overlay_fmt_ptr = &overlay_fmt;
			break;
		case OptSetOutputOverlayFormat:
		case OptTryOutputOverlayFormat:
			set_overlay_fmt_ptr = &set_overlay_fmt_out;
			overlay_fmt_ptr = &overlay_fmt_out;
			break;
		}
		subs = optarg;
		while (*subs != '\0') {
			static const char *const subopts[] = {
				"chromakey",
				"global_alpha",
				"left",
				"top",
				"width",
				"height",
				"field",
				NULL
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				overlay_fmt_ptr->fmt.win.chromakey = strtol(value, 0L, 0);
				*set_overlay_fmt_ptr |= FmtChromaKey;
				break;
			case 1:
				overlay_fmt_ptr->fmt.win.global_alpha = strtol(value, 0L, 0);
				*set_overlay_fmt_ptr |= FmtGlobalAlpha;
				break;
			case 2:
				overlay_fmt_ptr->fmt.win.w.left = strtol(value, 0L, 0);
				*set_overlay_fmt_ptr |= FmtLeft;
				break;
			case 3:
				overlay_fmt_ptr->fmt.win.w.top = strtol(value, 0L, 0);
				*set_overlay_fmt_ptr |= FmtTop;
				break;
			case 4:
				overlay_fmt_ptr->fmt.win.w.width = strtol(value, 0L, 0);
				*set_overlay_fmt_ptr |= FmtWidth;
				break;
			case 5:
				overlay_fmt_ptr->fmt.win.w.height = strtol(value, 0L, 0);
				*set_overlay_fmt_ptr |= FmtHeight;
				break;
			case 6:
				overlay_fmt_ptr->fmt.win.field = parse_field(value);
				*set_overlay_fmt_ptr |= FmtField;
				break;
			default:
				overlay_usage();
				break;
			}
		}
		break;
	case OptSetFBuf:
		subs = optarg;
		while (*subs != '\0') {
			static const char *const subopts[] = {
				"chromakey",
				"global_alpha",
				"local_alpha",
				"local_inv_alpha",
				NULL
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				fbuf.flags |= strtol(value, 0L, 0) ? V4L2_FBUF_FLAG_CHROMAKEY : 0;
				set_fbuf |= V4L2_FBUF_FLAG_CHROMAKEY;
				break;
			case 1:
				fbuf.flags |= strtol(value, 0L, 0) ? V4L2_FBUF_FLAG_GLOBAL_ALPHA : 0;
				set_fbuf |= V4L2_FBUF_FLAG_GLOBAL_ALPHA;
				break;
			case 2:
				fbuf.flags |= strtol(value, 0L, 0) ? V4L2_FBUF_FLAG_LOCAL_ALPHA : 0;
				set_fbuf |= V4L2_FBUF_FLAG_LOCAL_ALPHA;
				break;
			case 3:
				fbuf.flags |= strtol(value, 0L, 0) ? V4L2_FBUF_FLAG_LOCAL_INV_ALPHA : 0;
				set_fbuf |= V4L2_FBUF_FLAG_LOCAL_INV_ALPHA;
				break;
			default:
				overlay_usage();
				break;
			}
		}
		break;
	case OptOverlay:
		overlay = strtol(optarg, 0L, 0);
		break;
	}
}

void overlay_set(int fd)
{
	int ret;

	if (options[OptSetOverlayFormat] || options[OptTryOverlayFormat]) {
		struct v4l2_format fmt;

		fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
		if (doioctl(fd, VIDIOC_G_FMT, &fmt) == 0) {
			if (set_overlay_fmt & FmtChromaKey)
				fmt.fmt.win.chromakey = overlay_fmt.fmt.win.chromakey;
			if (set_overlay_fmt & FmtGlobalAlpha)
				fmt.fmt.win.global_alpha = overlay_fmt.fmt.win.global_alpha;
			if (set_overlay_fmt & FmtLeft)
				fmt.fmt.win.w.left = overlay_fmt.fmt.win.w.left;
			if (set_overlay_fmt & FmtTop)
				fmt.fmt.win.w.top = overlay_fmt.fmt.win.w.top;
			if (set_overlay_fmt & FmtWidth)
				fmt.fmt.win.w.width = overlay_fmt.fmt.win.w.width;
			if (set_overlay_fmt & FmtHeight)
				fmt.fmt.win.w.height = overlay_fmt.fmt.win.w.height;
			if (set_overlay_fmt & FmtField)
				fmt.fmt.win.field = overlay_fmt.fmt.win.field;
			if (options[OptSetOverlayFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &fmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &fmt);
			if (ret == 0 && (verbose || options[OptTryOverlayFormat]))
				printfmt(fmt);
		}
	}

	if (options[OptSetOutputOverlayFormat] || options[OptTryOutputOverlayFormat]) {
		struct v4l2_format fmt;

		fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
		if (doioctl(fd, VIDIOC_G_FMT, &fmt) == 0) {
			if (set_overlay_fmt_out & FmtChromaKey)
				fmt.fmt.win.chromakey = overlay_fmt_out.fmt.win.chromakey;
			if (set_overlay_fmt_out & FmtGlobalAlpha)
				fmt.fmt.win.global_alpha = overlay_fmt_out.fmt.win.global_alpha;
			if (set_overlay_fmt_out & FmtLeft)
				fmt.fmt.win.w.left = overlay_fmt_out.fmt.win.w.left;
			if (set_overlay_fmt_out & FmtTop)
				fmt.fmt.win.w.top = overlay_fmt_out.fmt.win.w.top;
			if (set_overlay_fmt_out & FmtWidth)
				fmt.fmt.win.w.width = overlay_fmt_out.fmt.win.w.width;
			if (set_overlay_fmt_out & FmtHeight)
				fmt.fmt.win.w.height = overlay_fmt_out.fmt.win.w.height;
			if (set_overlay_fmt_out & FmtField)
				fmt.fmt.win.field = overlay_fmt_out.fmt.win.field;
			if (options[OptSetOutputOverlayFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &fmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &fmt);
			if (ret == 0 && (verbose || options[OptTryOutputOverlayFormat]))
				printfmt(fmt);
		}
	}

	if (options[OptSetFBuf]) {
		struct v4l2_framebuffer fb;

		if (doioctl(fd, VIDIOC_G_FBUF, &fb) == 0) {
			fb.flags &= ~set_fbuf;
			fb.flags |= fbuf.flags;
			doioctl(fd, VIDIOC_S_FBUF, &fb);
		}
	}

	if (options[OptOverlay]) {
		doioctl(fd, VIDIOC_OVERLAY, &overlay);
	}
}

void overlay_get(int fd)
{
	if (options[OptGetOverlayFormat]) {
		struct v4l2_format fmt;

		fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
		if (doioctl(fd, VIDIOC_G_FMT, &fmt) == 0)
			printfmt(fmt);
	}

	if (options[OptGetOutputOverlayFormat]) {
		struct v4l2_format fmt;

		fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
		if (doioctl(fd, VIDIOC_G_FMT, &fmt) == 0)
			printfmt(fmt);
	}

	if (options[OptGetFBuf]) {
		struct v4l2_framebuffer fb;
		if (doioctl(fd, VIDIOC_G_FBUF, &fb) == 0)
			printfbuf(fb);
	}
}

void overlay_list(int fd)
{
	if (options[OptListOverlayFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_VIDEO_OVERLAY);
	}
}
