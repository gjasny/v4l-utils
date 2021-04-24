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

void selection_usage()
{
	printf("\nSelection/Cropping options:\n"
	       "  --get-cropcap      query the crop capabilities [VIDIOC_CROPCAP]\n"
	       "  --get-crop	     query the video capture crop window [VIDIOC_G_CROP]\n"
	       "  --set-crop top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video capture crop window [VIDIOC_S_CROP]\n"
	       "  --get-cropcap-output\n"
	       "                     query crop capabilities for video output [VIDIOC_CROPCAP]\n"
	       "  --get-crop-output  query the video output crop window [VIDIOC_G_CROP]\n"
	       "  --set-crop-output top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video output crop window [VIDIOC_S_CROP]\n"
	       "  --get-cropcap-overlay\n"
	       "                     query crop capabilities for video overlay [VIDIOC_CROPCAP]\n"
	       "  --get-crop-overlay query the video overlay crop window [VIDIOC_G_CROP]\n"
	       "  --set-crop-overlay top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video overlay crop window [VIDIOC_S_CROP]\n"
	       "  --get-cropcap-output-overlay\n"
	       "                     query the crop capabilities for video output overlays\n"
	       "                     [VIDIOC_CROPCAP]\n"
	       "  --get-crop-output-overlay\n"
	       "                     query the video output overlay crop window [VIDIOC_G_CROP]\n"
	       "  --set-crop-output-overlay top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video output overlay crop window [VIDIOC_S_CROP]\n"
	       "  --get-selection target=<target>\n"
	       "                     query the video capture selection rectangle [VIDIOC_G_SELECTION]\n"
	       "                     See --set-selection command for the valid <target> values.\n"
	       "  --set-selection target=<target>,flags=<flags>,top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video capture selection rectangle [VIDIOC_S_SELECTION]\n"
	       "                     target=crop|crop_bounds|crop_default|compose|compose_bounds|\n"
	       "                            compose_default|compose_padded|native_size\n"
	       "                     flags=le|ge|keep-config\n"
	       "  --get-selection-output target=<target>\n"
	       "                     query the video output selection rectangle [VIDIOC_G_SELECTION]\n"
	       "                     See --set-selection command for the valid <target> values.\n"
	       "  --set-selection-output target=<target>,flags=<flags>,top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video output selection rectangle [VIDIOC_S_SELECTION]\n"
	       "                     See --set-selection command for the arguments.\n"
	       );
}

static void do_crop(int fd, unsigned int set_crop, const struct v4l2_rect &vcrop, v4l2_buf_type type)
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
		static constexpr const char *subopts[] = {
			"left",
			"top",
			"width",
			"height",
			nullptr
		};

		switch (parse_subopt(&subs, subopts, &value)) {
		case 0:
			vcrop.left = strtol(value, nullptr, 0);
			set_crop |= CropLeft;
			break;
		case 1:
			vcrop.top = strtol(value, nullptr, 0);
			set_crop |= CropTop;
			break;
		case 2:
			vcrop.width = strtol(value, nullptr, 0);
			set_crop |= CropWidth;
			break;
		case 3:
			vcrop.height = strtol(value, nullptr, 0);
			set_crop |= CropHeight;
			break;
		default:
			selection_usage();
			std::exit(EXIT_FAILURE);
		}
	}
}

static void do_selection(int fd, unsigned int set_selection, const struct v4l2_selection &vsel,
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

static int parse_selection(char *optarg, unsigned int &set_sel, v4l2_selection &vsel)
{
	char *value;
	char *subs = optarg;

	while (*subs != '\0') {
		static constexpr const char *subopts[] = {
			"target",
			"flags",
			"left",
			"top",
			"width",
			"height",
			nullptr
		};

		switch (parse_subopt(&subs, subopts, &value)) {
		case 0:
			if (parse_selection_target(value, vsel.target)) {
				fprintf(stderr, "Unknown selection target\n");
				selection_usage();
				std::exit(EXIT_FAILURE);
			}
			break;
		case 1:
			vsel.flags = parse_selection_flags(value);
			set_sel |= SelectionFlags;
			break;
		case 2:
			vsel.r.left = strtol(value, nullptr, 0);
			set_sel |= SelectionLeft;
			break;
		case 3:
			vsel.r.top = strtol(value, nullptr, 0);
			set_sel |= SelectionTop;
			break;
		case 4:
			vsel.r.width = strtoul(value, nullptr, 0);
			set_sel |= SelectionWidth;
			break;
		case 5:
			vsel.r.height = strtoul(value, nullptr, 0);
			set_sel |= SelectionHeight;
			break;
		default:
			fprintf(stderr, "Unknown option\n");
			selection_usage();
			std::exit(EXIT_FAILURE);
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
				std::exit(EXIT_FAILURE);
			}
			get_sel_target = gsel.target;
			break;
		}
	}
}

void selection_set(cv4l_fd &_fd)
{
	int fd = _fd.g_fd();

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

void selection_get(cv4l_fd &_fd)
{
	int fd = _fd.g_fd();

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
		unsigned idx = 0;

		memset(&sel, 0, sizeof(sel));
		sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (options[OptAll] || get_sel_target == -1) {
			while (valid_seltarget_at_idx(idx)) {
				sel.target = seltarget_at_idx(idx);
				if (doioctl(fd, VIDIOC_G_SELECTION, &sel) == 0)
					print_selection(sel);
				idx++;
			}
		} else {
			sel.target = get_sel_target;
			if (doioctl(fd, VIDIOC_G_SELECTION, &sel) == 0)
				print_selection(sel);
		}
	}

	if (options[OptGetOutputSelection]) {
		struct v4l2_selection sel;
		unsigned idx = 0;

		memset(&sel, 0, sizeof(sel));
		sel.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

		if (options[OptAll] || get_sel_target == -1) {
			while (valid_seltarget_at_idx(idx)) {
				sel.target = seltarget_at_idx(idx);
				if (doioctl(fd, VIDIOC_G_SELECTION, &sel) == 0)
					print_selection(sel);
				idx++;
			}
		} else {
			sel.target = get_sel_target;
			if (doioctl(fd, VIDIOC_G_SELECTION, &sel) == 0)
				print_selection(sel);
		}
	}
}
