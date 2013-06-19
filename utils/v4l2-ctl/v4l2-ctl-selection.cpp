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

/* crop specified */
#define CropWidth		(1L<<0)
#define CropHeight		(1L<<1)
#define CropLeft		(1L<<2)
#define CropTop 		(1L<<3)

/* selection specified */
#define SelectionWidth		(1L<<0)
#define SelectionHeight		(1L<<1)
#define SelectionLeft		(1L<<2)
#define SelectionTop 		(1L<<3)
#define SelectionFlags 		(1L<<4)

/* bitfield for fmts */
static unsigned int set_crop;
static unsigned int set_crop_out;
static unsigned int set_crop_overlay;
static unsigned int set_crop_out_overlay;
static unsigned int set_selection;
static unsigned int set_selection_out;
static int get_sel_target;
static struct v4l2_rect vcrop; 	/* crop rect */
static struct v4l2_rect vcrop_out; 	/* crop rect */
static struct v4l2_rect vcrop_overlay; 	/* crop rect */
static struct v4l2_rect vcrop_out_overlay; 	/* crop rect */
static struct v4l2_selection vselection; 	/* capture selection */
static struct v4l2_selection vselection_out;	/* output selection */

void selection_usage(void)
{
	printf("\nSelection/Cropping options:\n"
	       "  --get-cropcap      query the crop capabilities [VIDIOC_CROPCAP]\n"
	       "  --get-crop	     query the video capture crop window [VIDIOC_G_CROP]\n"
	       "  --set-crop=top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video capture crop window [VIDIOC_S_CROP]\n"
	       "  --get-cropcap-output\n"
	       "                     query crop capabilities for video output [VIDIOC_CROPCAP]\n"
	       "  --get-crop-output  query the video output crop window [VIDIOC_G_CROP]\n"
	       "  --set-crop-output=top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video output crop window [VIDIOC_S_CROP]\n"
	       "  --get-cropcap-overlay\n"
	       "                     query crop capabilities for video overlay [VIDIOC_CROPCAP]\n"
	       "  --get-crop-overlay query the video overlay crop window [VIDIOC_G_CROP]\n"
	       "  --set-crop-overlay=top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video overlay crop window [VIDIOC_S_CROP]\n"
	       "  --get-cropcap-output-overlay\n"
	       "                     query the crop capabilities for video output overlays\n"
	       "                     [VIDIOC_CROPCAP]\n"
	       "  --get-crop-output-overlay\n"
	       "                     query the video output overlay crop window [VIDIOC_G_CROP]\n"
	       "  --set-crop-output-overlay=top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video output overlay crop window [VIDIOC_S_CROP]\n"
	       "  --get-selection=target=<target>\n"
	       "                     query the video capture selection rectangle [VIDIOC_G_SELECTION]\n"
	       "                     See --set-selection command for the valid <target> values.\n"
	       "  --set-selection=target=<target>,flags=<flags>,top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video capture selection rectangle [VIDIOC_S_SELECTION]\n"
	       "                     target=crop|crop_bounds|crop_default|compose|compose_bounds|\n"
	       "                            compose_default|compose_padded\n"
	       "                     flags=le|ge\n"
	       "  --get-selection-output=target=<target>\n"
	       "                     query the video output selection rectangle [VIDIOC_G_SELECTION]\n"
	       "                     See --set-selection command for the valid <target> values.\n"
	       "  --set-selection-output=target=<target>,flags=<flags>,top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video output selection rectangle [VIDIOC_S_SELECTION]\n"
	       "                     See --set-selection command for the arguments.\n"
	       );
}

static void do_crop(int fd, unsigned int set_crop, struct v4l2_rect &vcrop, v4l2_buf_type type)
{
	struct v4l2_crop in_crop;

	in_crop.type = type;
	if (doioctl(fd, VIDIOC_G_CROP, &in_crop) == 0) {
		if (set_crop & CropWidth)
			in_crop.c.width = vcrop.width;
		if (set_crop & CropHeight)
			in_crop.c.height = vcrop.height;
		if (set_crop & CropLeft)
			in_crop.c.left = vcrop.left;
		if (set_crop & CropTop)
			in_crop.c.top = vcrop.top;
		doioctl(fd, VIDIOC_S_CROP, &in_crop);
	}
}

static void parse_crop(char *optarg, unsigned int &set_crop, v4l2_rect &vcrop)
{
	char *value;
	char *subs = optarg;

	while (*subs != '\0') {
		static const char *const subopts[] = {
			"left",
			"top",
			"width",
			"height",
			NULL
		};

		switch (parse_subopt(&subs, subopts, &value)) {
		case 0:
			vcrop.left = strtol(value, 0L, 0);
			set_crop |= CropLeft;
			break;
		case 1:
			vcrop.top = strtol(value, 0L, 0);
			set_crop |= CropTop;
			break;
		case 2:
			vcrop.width = strtol(value, 0L, 0);
			set_crop |= CropWidth;
			break;
		case 3:
			vcrop.height = strtol(value, 0L, 0);
			set_crop |= CropHeight;
			break;
		default:
			selection_usage();
			exit(1);
		}
	}
}

static void do_selection(int fd, unsigned int set_selection, struct v4l2_selection &vsel,
			 v4l2_buf_type type)
{
	struct v4l2_selection in_selection;

	in_selection.type = type;
	in_selection.target = vsel.target;

	if (doioctl(fd, VIDIOC_G_SELECTION, &in_selection) == 0) {
		if (set_selection & SelectionWidth)
			in_selection.r.width = vsel.r.width;
		if (set_selection & SelectionHeight)
			in_selection.r.height = vsel.r.height;
		if (set_selection & SelectionLeft)
			in_selection.r.left = vsel.r.left;
		if (set_selection & SelectionTop)
			in_selection.r.top = vsel.r.top;
		in_selection.flags = (set_selection & SelectionFlags) ? vsel.flags : 0;
		doioctl(fd, VIDIOC_S_SELECTION, &in_selection);
	}
}

static int parse_selection_target(const char *s, unsigned int &target)
{
	if (!strcmp(s, "crop")) target = V4L2_SEL_TGT_CROP_ACTIVE;
	else if (!strcmp(s, "crop_default")) target = V4L2_SEL_TGT_CROP_DEFAULT;
	else if (!strcmp(s, "crop_bounds")) target = V4L2_SEL_TGT_CROP_BOUNDS;
	else if (!strcmp(s, "compose")) target = V4L2_SEL_TGT_COMPOSE_ACTIVE;
	else if (!strcmp(s, "compose_default")) target = V4L2_SEL_TGT_COMPOSE_DEFAULT;
	else if (!strcmp(s, "compose_bounds")) target = V4L2_SEL_TGT_COMPOSE_BOUNDS;
	else if (!strcmp(s, "compose_padded")) target = V4L2_SEL_TGT_COMPOSE_PADDED;
	else return -EINVAL;

	return 0;
}

static int parse_selection_flags(const char *s)
{
	if (!strcmp(s, "le")) return V4L2_SEL_FLAG_LE;
	if (!strcmp(s, "ge")) return V4L2_SEL_FLAG_GE;
	return 0;
}

static int parse_selection(char *optarg, unsigned int &set_sel, v4l2_selection &vsel)
{
	char *value;
	char *subs = optarg;

	while (*subs != '\0') {
		static const char *const subopts[] = {
			"target",
			"flags",
			"left",
			"top",
			"width",
			"height",
			NULL
		};

		switch (parse_subopt(&subs, subopts, &value)) {
		case 0:
			if (parse_selection_target(value, vsel.target)) {
				fprintf(stderr, "Unknown selection target\n");
				selection_usage();
				exit(1);
			}
			break;
		case 1:
			vsel.flags = parse_selection_flags(value);
			set_sel |= SelectionFlags;
			break;
		case 2:
			vsel.r.left = strtol(value, 0L, 0);
			set_sel |= SelectionLeft;
			break;
		case 3:
			vsel.r.top = strtol(value, 0L, 0);
			set_sel |= SelectionTop;
			break;
		case 4:
			vsel.r.width = strtol(value, 0L, 0);
			set_sel |= SelectionWidth;
			break;
		case 5:
			vsel.r.height = strtol(value, 0L, 0);
			set_sel |= SelectionHeight;
			break;
		default:
			fprintf(stderr, "Unknown option\n");
			selection_usage();
			exit(1);
		}
	}

	return 0;
}

static void printcrop(const struct v4l2_crop &crop)
{
	printf("Crop: Left %d, Top %d, Width %d, Height %d\n",
			crop.c.left, crop.c.top, crop.c.width, crop.c.height);
}

static void printcropcap(const struct v4l2_cropcap &cropcap)
{
	printf("Crop Capability %s:\n", buftype2s(cropcap.type).c_str());
	printf("\tBounds      : Left %d, Top %d, Width %d, Height %d\n",
			cropcap.bounds.left, cropcap.bounds.top, cropcap.bounds.width, cropcap.bounds.height);
	printf("\tDefault     : Left %d, Top %d, Width %d, Height %d\n",
			cropcap.defrect.left, cropcap.defrect.top, cropcap.defrect.width, cropcap.defrect.height);
	printf("\tPixel Aspect: %u/%u\n", cropcap.pixelaspect.numerator, cropcap.pixelaspect.denominator);
}

static const flag_def selection_targets_def[] = {
	{ V4L2_SEL_TGT_CROP_ACTIVE, "crop" },
	{ V4L2_SEL_TGT_CROP_DEFAULT, "crop_default" },
	{ V4L2_SEL_TGT_CROP_BOUNDS, "crop_bounds" },
	{ V4L2_SEL_TGT_COMPOSE_ACTIVE, "compose" },
	{ V4L2_SEL_TGT_COMPOSE_DEFAULT, "compose_default" },
	{ V4L2_SEL_TGT_COMPOSE_BOUNDS, "compose_bounds" },
	{ V4L2_SEL_TGT_COMPOSE_PADDED, "compose_padded" },
	{ 0, NULL }
};

static std::string seltarget2s(__u32 target)
{
	int i = 0;

	while (selection_targets_def[i].str != NULL) {
		if (selection_targets_def[i].flag == target)
			return selection_targets_def[i].str;
		i++;
	}
	return "Unknown";
}

static void print_selection(const struct v4l2_selection &sel)
{
	printf("Selection: %s, Left %d, Top %d, Width %d, Height %d\n",
			seltarget2s(sel.target).c_str(),
			sel.r.left, sel.r.top, sel.r.width, sel.r.height);
}

void selection_cmd(int ch, char *optarg)
{
	switch (ch) {
		case OptSetCrop:
			parse_crop(optarg, set_crop, vcrop);
			break;
		case OptSetOutputCrop:
			parse_crop(optarg, set_crop_out, vcrop_out);
			break;
		case OptSetOverlayCrop:
			parse_crop(optarg, set_crop_overlay, vcrop_overlay);
			break;
		case OptSetOutputOverlayCrop:
			parse_crop(optarg, set_crop_out_overlay, vcrop_out_overlay);
			break;
		case OptSetSelection:
			parse_selection(optarg, set_selection, vselection);
			break;
		case OptSetOutputSelection:
			parse_selection(optarg, set_selection_out, vselection_out);
			break;
		case OptGetOutputSelection:
		case OptGetSelection: {
			struct v4l2_selection gsel;
			unsigned int get_sel;

			if (parse_selection(optarg, get_sel, gsel)) {
				fprintf(stderr, "Unknown selection target\n");
				selection_usage();
				exit(1);
			}
			get_sel_target = gsel.target;
			break;
		}
	}
}

void selection_set(int fd)
{
	if (options[OptSetCrop]) {
		do_crop(fd, set_crop, vcrop, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	}

	if (options[OptSetOutputCrop]) {
		do_crop(fd, set_crop_out, vcrop_out, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	}

	if (options[OptSetOverlayCrop]) {
		do_crop(fd, set_crop_overlay, vcrop_overlay, V4L2_BUF_TYPE_VIDEO_OVERLAY);
	}

	if (options[OptSetOutputOverlayCrop]) {
		do_crop(fd, set_crop_out_overlay, vcrop_out_overlay, V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY);
	}

	if (options[OptSetSelection]) {
		do_selection(fd, set_selection, vselection, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	}

	if (options[OptSetOutputSelection]) {
		do_selection(fd, set_selection_out, vselection_out, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	}
}

void selection_get(int fd)
{
	if (options[OptGetCropCap]) {
		struct v4l2_cropcap cropcap;

		cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (doioctl(fd, VIDIOC_CROPCAP, &cropcap) == 0)
			printcropcap(cropcap);
	}

	if (options[OptGetOutputCropCap]) {
		struct v4l2_cropcap cropcap;

		cropcap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (doioctl(fd, VIDIOC_CROPCAP, &cropcap) == 0)
			printcropcap(cropcap);
	}

	if (options[OptGetOverlayCropCap]) {
		struct v4l2_cropcap cropcap;

		cropcap.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
		if (doioctl(fd, VIDIOC_CROPCAP, &cropcap) == 0)
			printcropcap(cropcap);
	}

	if (options[OptGetOutputOverlayCropCap]) {
		struct v4l2_cropcap cropcap;

		cropcap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
		if (doioctl(fd, VIDIOC_CROPCAP, &cropcap) == 0)
			printcropcap(cropcap);
	}

	if (options[OptGetCrop]) {
		struct v4l2_crop crop;

		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (doioctl(fd, VIDIOC_G_CROP, &crop) == 0)
			printcrop(crop);
	}

	if (options[OptGetOutputCrop]) {
		struct v4l2_crop crop;

		crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (doioctl(fd, VIDIOC_G_CROP, &crop) == 0)
			printcrop(crop);
	}

	if (options[OptGetOverlayCrop]) {
		struct v4l2_crop crop;

		crop.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
		if (doioctl(fd, VIDIOC_G_CROP, &crop) == 0)
			printcrop(crop);
	}

	if (options[OptGetOutputOverlayCrop]) {
		struct v4l2_crop crop;

		crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
		if (doioctl(fd, VIDIOC_G_CROP, &crop) == 0)
			printcrop(crop);
	}

	if (options[OptGetSelection]) {
		struct v4l2_selection sel;
		int t = 0;

		memset(&sel, 0, sizeof(sel));
		sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (options[OptAll] || get_sel_target == -1) {
			while (selection_targets_def[t].str != NULL) {
				sel.target = selection_targets_def[t].flag;
				if (doioctl(fd, VIDIOC_G_SELECTION, &sel) == 0)
					print_selection(sel);
				t++;
			}
		} else {
			sel.target = get_sel_target;
			if (doioctl(fd, VIDIOC_G_SELECTION, &sel) == 0)
				print_selection(sel);
		}
	}

	if (options[OptGetOutputSelection]) {
		struct v4l2_selection sel;
		int t = 0;

		memset(&sel, 0, sizeof(sel));
		sel.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

		if (options[OptAll] || get_sel_target == -1) {
			while (selection_targets_def[t].str != NULL) {
				sel.target = selection_targets_def[t].flag;
				if (doioctl(fd, VIDIOC_G_SELECTION, &sel) == 0)
					print_selection(sel);
				t++;
			}
		} else {
			sel.target = get_sel_target;
			if (doioctl(fd, VIDIOC_G_SELECTION, &sel) == 0)
				print_selection(sel);
		}
	}
}
