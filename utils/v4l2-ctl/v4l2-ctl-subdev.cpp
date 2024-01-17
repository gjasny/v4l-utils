#include "v4l2-ctl.h"

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

struct mbus_name {
	const char *name;
	__u32 code;
};

static const struct mbus_name mbus_names[] = {
	{ "Fixed", MEDIA_BUS_FMT_FIXED },
#include "media-bus-format-names.h"
	{ nullptr, 0 }
};

/* selection specified */
#define SelectionWidth		(1L<<0)
#define SelectionHeight		(1L<<1)
#define SelectionLeft		(1L<<2)
#define SelectionTop 		(1L<<3)
#define SelectionFlags 		(1L<<4)

static __u32 list_mbus_codes_pad;
static __u32 list_mbus_codes_stream = 0;
static __u32 get_fmt_pad;
static __u32 get_fmt_stream = 0;
static __u32 get_sel_pad;
static __u32 get_sel_stream = 0;
static __u32 get_fps_pad;
static __u32 get_fps_stream = 0;
static int get_sel_target = -1;
static unsigned int set_selection;
static struct v4l2_subdev_selection vsel;
static unsigned int set_fmt;
static __u32 set_fmt_pad;
static __u32 set_fmt_stream = 0;
static struct v4l2_mbus_framefmt ffmt;
static struct v4l2_subdev_frame_size_enum frmsize;
static struct v4l2_subdev_frame_interval_enum frmival;
static __u32 set_fps_pad;
static __u32 set_fps_stream = 0;
static double set_fps;
static struct v4l2_subdev_routing routing;
static struct v4l2_subdev_route routes[NUM_ROUTES_MAX];

void subdev_usage()
{
	printf("\nSub-Device options:\n"
	       "Note: all parameters below (pad, code, etc.) are optional unless otherwise noted and default to 0\n"
	       "  --list-subdev-mbus-codes pad=<pad>,stream=<stream>\n"
	       "                      display supported mediabus codes for this pad and stream\n"
	       "                      [VIDIOC_SUBDEV_ENUM_MBUS_CODE]\n"
	       "  --list-subdev-framesizes pad=<pad>,stream=<stream>,code=<code>\n"
	       "                     list supported framesizes for this pad, stream and code\n"
	       "                     [VIDIOC_SUBDEV_ENUM_FRAME_SIZE]\n"
	       "                     <code> is the value of the mediabus code\n"
	       "  --list-subdev-frameintervals pad=<pad>,stream=<stream>,width=<w>,height=<h>,code=<code>\n"
	       "                     list supported frame intervals for this pad, stream, code and\n"
	       "                     the given width and height [VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL]\n"
	       "                     <code> is the value of the mediabus code\n"
	       "  --get-subdev-fmt pad=<pad>,stream=<stream>\n"
	       "     		     query the frame format for the given pad and stream [VIDIOC_SUBDEV_G_FMT]\n"
	       "  --get-subdev-selection pad=<pad>,stream=<stream>,target=<target>\n"
	       "                     query the frame selection rectangle [VIDIOC_SUBDEV_G_SELECTION]\n"
	       "                     See --set-subdev-selection command for the valid <target> values.\n"
	       "  --get-subdev-fps pad=<pad>,stream=<stream>\n"
	       "                     query the frame rate [VIDIOC_SUBDEV_G_FRAME_INTERVAL]\n"
	       "  --set-subdev-fmt   (for testing only, otherwise use media-ctl)\n"
	       "  --try-subdev-fmt pad=<pad>,stream=<stream>,width=<w>,height=<h>,code=<code>,field=<f>,colorspace=<c>,\n"
	       "                   xfer=<xf>,ycbcr=<y>,hsv=<hsv>,quantization=<q>\n"
	       "                     set the frame format for the given pad and stream [VIDIOC_SUBDEV_S_FMT]\n"
	       "                     <pad> the pad to get the format from\n"
	       "                     <stream> the stream to get the format\n"
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
	       "  --try-subdev-selection pad=<pad>,stream=<stream>,target=<target>,flags=<flags>,\n"
	       "                         top=<x>,left=<y>,width=<w>,height=<h>\n"
	       "                     set the video capture selection rectangle [VIDIOC_SUBDEV_S_SELECTION]\n"
	       "                     target=crop|crop_bounds|crop_default|compose|compose_bounds|\n"
	       "                            compose_default|compose_padded|native_size\n"
	       "                     flags=le|ge|keep-config\n"
	       "  --set-subdev-fps pad=<pad>,stream=<stream>,fps=<fps> (for testing only, otherwise use media-ctl)\n"
	       "                     set the frame rate [VIDIOC_SUBDEV_S_FRAME_INTERVAL]\n"
	       "  --get-routing      Print the route topology\n"
	       "  --set-routing      (for testing only, otherwise use media-ctl)\n"
	       "  --try-routing <routes>\n"
	       "                     Comma-separated list of route descriptors to setup\n"
	       "\n"
	       "Routes are defined as\n"
	       "	routes		= route { ',' route } ;\n"
	       "	route		= sink '->' source '[' flags ']' ;\n"
	       "	sink		= sink-pad '/' sink-stream ;\n"
	       "	source		= source-pad '/' source-stream ;\n"
	       "\n"
	       "where\n"
	       "	sink-pad	= Pad numeric identifier for sink\n"
	       "	sink-stream	= Stream numeric identifier for sink\n"
	       "	source-pad	= Pad numeric identifier for source\n"
	       "	source-stream	= Stream numeric identifier for source\n"
	       "	flags		= Route flags (0: inactive, 1: active)\n"
	       );
}

void subdev_cmd(int ch, char *optarg)
{
	char *value, *subs;
	char *endp;

	switch (ch) {
	case OptListSubDevMBusCodes:
		if (optarg) {
			/* Legacy pad-only parsing */
			list_mbus_codes_pad = strtoul(optarg, &endp, 0);
			if (*endp == 0)
				break;
		}

		subs = optarg;
		while (subs && *subs != '\0') {
			static constexpr const char *subopts[] = {
				"pad",
				"stream",
				nullptr
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				list_mbus_codes_pad = strtoul(value, nullptr, 0);
				break;
			case 1:
				list_mbus_codes_stream = strtoul(value, nullptr, 0);
				break;
			default:
				subdev_usage();
				std::exit(EXIT_FAILURE);
			}
		}
		break;
	case OptListSubDevFrameSizes:
		subs = optarg;
		while (*subs != '\0') {
			static constexpr const char *subopts[] = {
				"pad",
				"stream",
				"code",
				nullptr
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				frmsize.pad = strtoul(value, nullptr, 0);
				break;
			case 1:
				frmsize.stream = strtoul(value, nullptr, 0);
				break;
			case 2:
				frmsize.code = strtoul(value, nullptr, 0);
				break;
			default:
				subdev_usage();
				std::exit(EXIT_FAILURE);
			}
		}
		break;
	case OptListSubDevFrameIntervals:
		subs = optarg;
		while (*subs != '\0') {
			static constexpr const char *subopts[] = {
				"pad",
				"stream",
				"code",
				"width",
				"height",
				nullptr
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				frmival.pad = strtoul(value, nullptr, 0);
				break;
			case 1:
				frmival.stream = strtoul(value, nullptr, 0);
				break;
			case 2:
				frmival.code = strtoul(value, nullptr, 0);
				break;
			case 3:
				frmival.width = strtoul(value, nullptr, 0);
				break;
			case 4:
				frmival.height = strtoul(value, nullptr, 0);
				break;
			default:
				subdev_usage();
				std::exit(EXIT_FAILURE);
			}
		}
		break;
	case OptGetSubDevFormat:
		if (optarg) {
			/* Legacy pad-only parsing */
			get_fmt_pad = strtoul(optarg, &endp, 0);
			if (*endp == 0)
				break;
		}

		subs = optarg;
		while (subs && *subs != '\0') {
			static constexpr const char *subopts[] = {
				"pad",
				"stream",
				nullptr
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				get_fmt_pad = strtoul(value, nullptr, 0);
				break;
			case 1:
				get_fmt_stream = strtoul(value, nullptr, 0);
				break;
			default:
				subdev_usage();
				std::exit(EXIT_FAILURE);
			}
		}
		break;
	case OptGetSubDevSelection:
		subs = optarg;
		while (*subs != '\0') {
			static constexpr const char *subopts[] = {
				"pad",
				"stream",
				"target",
				nullptr
			};
			unsigned int target;

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				get_sel_pad = strtoul(value, nullptr, 0);
				break;
			case 1:
				get_sel_stream = strtoul(value, nullptr, 0);
				break;
			case 2:
				if (parse_selection_target(value, target)) {
					fprintf(stderr, "Unknown selection target\n");
					subdev_usage();
					std::exit(EXIT_FAILURE);
				}
				get_sel_target = target;
				break;
			default:
				subdev_usage();
				std::exit(EXIT_FAILURE);
			}
		}
		break;
	case OptGetSubDevFPS:
		if (optarg) {
			/* Legacy pad-only parsing */
			get_fps_pad = strtoul(optarg, &endp, 0);
			if (*endp == 0)
				break;
		}

		subs = optarg;
		while (subs && *subs != '\0') {
			static constexpr const char *subopts[] = {
				"pad",
				"stream",
				nullptr
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				get_fps_pad = strtoul(value, nullptr, 0);
				break;
			case 1:
				get_fps_stream = strtoul(value, nullptr, 0);
				break;
			default:
				subdev_usage();
				std::exit(EXIT_FAILURE);
			}
		}
		break;
	case OptSetSubDevFormat:
	case OptTrySubDevFormat:
		ffmt.field = V4L2_FIELD_ANY;
		subs = optarg;
		while (*subs != '\0') {
			static constexpr const char *subopts[] = {
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
				"stream",
				nullptr
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				ffmt.width = strtoul(value, nullptr, 0);
				set_fmt |= FmtWidth;
				break;
			case 1:
				ffmt.height = strtoul(value, nullptr, 0);
				set_fmt |= FmtHeight;
				break;
			case 2:
				ffmt.code = strtoul(value, nullptr, 0);
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
				set_fmt_pad = strtoul(value, nullptr, 0);
				break;
			case 10:
				set_fmt_stream = strtoul(value, nullptr, 0);
				break;
			default:
				fprintf(stderr, "Unknown option\n");
				subdev_usage();
				std::exit(EXIT_FAILURE);
			}
		}
		break;
	case OptSetSubDevSelection:
	case OptTrySubDevSelection:
		subs = optarg;

		while (*subs != '\0') {
			static constexpr const char *subopts[] = {
				"target",
				"flags",
				"left",
				"top",
				"width",
				"height",
				"pad",
				"stream",
				nullptr
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				if (parse_selection_target(value, vsel.target)) {
					fprintf(stderr, "Unknown selection target\n");
					subdev_usage();
					std::exit(EXIT_FAILURE);
				}
				break;
			case 1:
				vsel.flags = parse_selection_flags(value);
				set_selection |= SelectionFlags;
				break;
			case 2:
				vsel.r.left = strtol(value, nullptr, 0);
				set_selection |= SelectionLeft;
				break;
			case 3:
				vsel.r.top = strtol(value, nullptr, 0);
				set_selection |= SelectionTop;
				break;
			case 4:
				vsel.r.width = strtoul(value, nullptr, 0);
				set_selection |= SelectionWidth;
				break;
			case 5:
				vsel.r.height = strtoul(value, nullptr, 0);
				set_selection |= SelectionHeight;
				break;
			case 6:
				vsel.pad = strtoul(value, nullptr, 0);
				break;
			case 7:
				vsel.stream = strtoul(value, nullptr, 0);
				break;
			default:
				fprintf(stderr, "Unknown option\n");
				subdev_usage();
				std::exit(EXIT_FAILURE);
			}
		}
		break;
	case OptSetSubDevFPS:
		subs = optarg;

		while (*subs != '\0') {
			static constexpr const char *subopts[] = {
				"pad",
				"stream",
				"fps",
				nullptr
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				set_fps_pad = strtoul(value, nullptr, 0);
				break;
			case 1:
				set_fps_stream = strtoul(value, nullptr, 0);
				break;
			case 2:
				set_fps = strtod(value, nullptr);
				break;
			default:
				fprintf(stderr, "Unknown option\n");
				subdev_usage();
				std::exit(EXIT_FAILURE);
			}
		}
		break;
	case OptSetRouting:
	case OptTryRouting: {
		struct v4l2_subdev_route *r;
		char *end, *ref, *tok;
		unsigned int flags;

		memset(&routing, 0, sizeof(routing));
		memset(routes, 0, sizeof(routes[0]) * NUM_ROUTES_MAX);
		routing.which = ch == OptSetRouting ? V4L2_SUBDEV_FORMAT_ACTIVE :
				      V4L2_SUBDEV_FORMAT_TRY;
		routing.num_routes = 0;
		routing.routes = (__u64)routes;

		if (!optarg)
			break;

		r = (v4l2_subdev_route *)routing.routes;
		ref = end = strdup(optarg);
		while ((tok = strsep(&end, ",")) != NULL) {
			if (sscanf(tok, "%u/%u -> %u/%u [%u]",
				   &r->sink_pad, &r->sink_stream,
				   &r->source_pad, &r->source_stream,
				   &flags) != 5) {
				free(ref);
				fprintf(stderr, "Invalid route information specified\n");
				subdev_usage();
				std::exit(EXIT_FAILURE);
			}

			if (flags & ~(V4L2_SUBDEV_ROUTE_FL_ACTIVE)) {
				fprintf(stderr, "Invalid route flags specified: %#x\n", flags);
				subdev_usage();
				std::exit(EXIT_FAILURE);
			}

			r->flags = flags;

			r++;
			routing.num_routes++;
		}
		free(ref);
		break;
	}
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

		if (!_fd.has_streams() && set_fmt_stream) {
			printf("Streams API not supported.\n");
			return;
		}

		memset(&fmt, 0, sizeof(fmt));
		fmt.pad = set_fmt_pad;
		fmt.stream = set_fmt_stream;
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
			if (set_fmt & FmtColorspace) {
				fmt.format.colorspace = ffmt.colorspace;
				fmt.format.flags |= V4L2_MBUS_FRAMEFMT_SET_CSC;
			}
			if (set_fmt & FmtXferFunc) {
				fmt.format.xfer_func = ffmt.xfer_func;
				fmt.format.flags |= V4L2_MBUS_FRAMEFMT_SET_CSC;
			}
			if (set_fmt & FmtYCbCr) {
				fmt.format.ycbcr_enc = ffmt.ycbcr_enc;
				fmt.format.flags |= V4L2_MBUS_FRAMEFMT_SET_CSC;
			}
			if (set_fmt & FmtQuantization) {
				fmt.format.quantization = ffmt.quantization;
				fmt.format.flags |= V4L2_MBUS_FRAMEFMT_SET_CSC;
			}

			if (options[OptSetSubDevFormat])
				printf("Note: --set-subdev-fmt is only for testing.\n"
				       "Normally media-ctl is used to configure the video pipeline.\n");
			else
				fmt.which = V4L2_SUBDEV_FORMAT_TRY;

			printf("ioctl: VIDIOC_SUBDEV_S_FMT (pad=%u,stream=%u)\n", fmt.pad, fmt.stream);
			ret = doioctl(fd, VIDIOC_SUBDEV_S_FMT, &fmt);
			if (ret == 0 && (verbose || !options[OptSetSubDevFormat]))
				print_framefmt(fmt.format);
		}
	}
	if (options[OptSetSubDevSelection] || options[OptTrySubDevSelection]) {
		struct v4l2_subdev_selection sel;

		if (!_fd.has_streams() && vsel.stream) {
			printf("Streams API not supported.\n");
			return;
		}

		memset(&sel, 0, sizeof(sel));
		sel.pad = vsel.pad;
		sel.stream = vsel.stream;
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

			printf("ioctl: VIDIOC_SUBDEV_S_SELECTION (pad=%u,stream=%u)\n", sel.pad, sel.stream);
			int ret = doioctl(fd, VIDIOC_SUBDEV_S_SELECTION, &sel);
			if (ret == 0 && (verbose || !options[OptSetSubDevSelection]))
				print_subdev_selection(sel);
		}
	}
	if (options[OptSetSubDevFPS]) {
		struct v4l2_subdev_frame_interval fival;

		if (!_fd.has_streams() && set_fps_stream) {
			printf("Streams API not supported.\n");
			return;
		}

		memset(&fival, 0, sizeof(fival));
		fival.pad = set_fps_pad;
		fival.stream = set_fps_stream;
		if (_fd.has_ival_uses_which())
			fival.which = V4L2_SUBDEV_FORMAT_ACTIVE;

		if (set_fps <= 0) {
			fprintf(stderr, "invalid fps %f\n", set_fps);
			subdev_usage();
			std::exit(EXIT_FAILURE);
		}
		fival.interval.numerator = 1000;
		fival.interval.denominator = static_cast<uint32_t>(set_fps * fival.interval.numerator);
		printf("Note: --set-subdev-fps is only for testing.\n"
		       "Normally media-ctl is used to configure the video pipeline.\n");
		printf("ioctl: VIDIOC_SUBDEV_S_FRAME_INTERVAL (pad=%u,stream=%u)\n", fival.pad, fival.stream);
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
	if (options[OptSetRouting] || options[OptTryRouting]) {
		if (!_fd.has_streams()) {
			printf("Streams API not supported.\n");
			return;
		}

		if (doioctl(fd, VIDIOC_SUBDEV_S_ROUTING, &routing) == 0)
			printf("Routing set\n");
	}
}

struct flag_name {
	__u32 flag;
	const char *name;
};

static void print_flags(const struct flag_name *flag_names, unsigned int num_entries, __u32 flags)
{
	bool first = true;
	unsigned int i;

	for (i = 0; i < num_entries; i++) {
		if (!(flags & flag_names[i].flag))
			continue;
		if (!first)
			printf(",");
		printf("%s", flag_names[i].name);
		flags &= ~flag_names[i].flag;
		first = false;
	}

	if (flags) {
		if (!first)
			printf(",");
		printf("0x%x", flags);
	}
}

static void print_routes(const struct v4l2_subdev_routing *r)
{
	unsigned int i;
	struct v4l2_subdev_route *routes = (struct v4l2_subdev_route *)r->routes;

	static const struct flag_name route_flags[] = {
		{ V4L2_SUBDEV_ROUTE_FL_ACTIVE, "ACTIVE" },
	};

	for (i = 0; i < r->num_routes; i++) {
		printf("%u/%u -> %u/%u [",
		       routes[i].sink_pad, routes[i].sink_stream,
		       routes[i].source_pad, routes[i].source_stream);
		print_flags(route_flags, ARRAY_SIZE(route_flags), routes[i].flags);
		printf("]\n");
	}
}

void subdev_get(cv4l_fd &_fd)
{
	int fd = _fd.g_fd();

	if (options[OptGetSubDevFormat]) {
		struct v4l2_subdev_format fmt;

		if (!_fd.has_streams() && get_fmt_stream) {
			printf("Streams API not supported.\n");
			return;
		}

		memset(&fmt, 0, sizeof(fmt));
		fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		fmt.pad = get_fmt_pad;
		fmt.stream = get_fmt_stream;

		printf("ioctl: VIDIOC_SUBDEV_G_FMT (pad=%u,stream=%u)\n", fmt.pad, fmt.stream);
		if (doioctl(fd, VIDIOC_SUBDEV_G_FMT, &fmt) == 0)
			print_framefmt(fmt.format);
	}

	if (options[OptGetSubDevSelection]) {
		struct v4l2_subdev_selection sel;
		unsigned idx = 0;

		if (!_fd.has_streams() && get_sel_stream) {
			printf("Streams API not supported.\n");
			return;
		}

		memset(&sel, 0, sizeof(sel));
		sel.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		sel.pad = get_sel_pad;
		sel.stream = get_sel_stream;

		printf("ioctl: VIDIOC_SUBDEV_G_SELECTION (pad=%u,stream=%u)\n", sel.pad, sel.stream);
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

		if (!_fd.has_streams() && get_fps_stream) {
			printf("Streams API not supported.\n");
			return;
		}

		memset(&fival, 0, sizeof(fival));
		fival.pad = get_fps_pad;
		fival.stream = get_fps_stream;
		if (_fd.has_ival_uses_which())
			fival.which = V4L2_SUBDEV_FORMAT_ACTIVE;

		printf("ioctl: VIDIOC_SUBDEV_G_FRAME_INTERVAL (pad=%u,stream=%u)\n", fival.pad, fival.stream);
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

	if (options[OptGetRouting]) {
		if (!_fd.has_streams()) {
			printf("Streams API not supported.\n");
			return;
		}

		memset(&routing, 0, sizeof(routing));
		memset(routes, 0, sizeof(routes[0]) * NUM_ROUTES_MAX);
		routing.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		routing.num_routes = NUM_ROUTES_MAX;
		routing.routes = (__u64)routes;

		if (doioctl(fd, VIDIOC_SUBDEV_G_ROUTING, &routing) == 0)
			print_routes(&routing);
	}
}

static void print_mbus_code(__u32 code)
{
	unsigned int i;

	for (i = 0; mbus_names[i].name; i++)
		if (mbus_names[i].code == code)
			break;
	if (mbus_names[i].name)
		printf("\t0x%04x: MEDIA_BUS_FMT_%s",
		       mbus_names[i].code, mbus_names[i].name);
	else
		printf("\t0x%04x", code);
}

static void print_mbus_codes(int fd, __u32 pad, __u32 stream)
{
	struct v4l2_subdev_mbus_code_enum mbus_code = {};

	mbus_code.pad = pad;
	mbus_code.stream = stream;
	mbus_code.which = V4L2_SUBDEV_FORMAT_TRY;

	for (;;) {
		int ret = test_ioctl(fd, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_code);

		if (ret)
			break;
		print_mbus_code(mbus_code.code);
		if (mbus_code.flags) {
			bool is_hsv = mbus_code.code == MEDIA_BUS_FMT_AHSV8888_1X32;

			printf(", %s", mbus2s(mbus_code.flags, is_hsv).c_str());
		}
		printf("\n");
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
		if (!_fd.has_streams() && list_mbus_codes_stream) {
			printf("Streams API not supported.\n");
			return;
		}

		printf("ioctl: VIDIOC_SUBDEV_ENUM_MBUS_CODE (pad=%u,stream=%u)\n",
		       list_mbus_codes_pad, list_mbus_codes_stream);
		print_mbus_codes(fd, list_mbus_codes_pad, list_mbus_codes_stream);
	}
	if (options[OptListSubDevFrameSizes]) {
		if (!_fd.has_streams() && frmsize.stream) {
			printf("Streams API not supported.\n");
			return;
		}

		printf("ioctl: VIDIOC_SUBDEV_ENUM_FRAME_SIZE (pad=%u,stream=%u)\n",
		       frmsize.pad, frmsize.stream);
		frmsize.index = 0;
		frmsize.which = V4L2_SUBDEV_FORMAT_TRY;
		while (test_ioctl(fd, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &frmsize) >= 0) {
			print_frmsize(frmsize);
			frmsize.index++;
		}
	}
	if (options[OptListSubDevFrameIntervals]) {
		if (!_fd.has_streams() && frmival.stream) {
			printf("Streams API not supported.\n");
			return;
		}

		printf("ioctl: VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL (pad=%u,stream=%u)\n",
		       frmival.pad, frmival.stream);
		frmival.index = 0;
		frmival.which = V4L2_SUBDEV_FORMAT_TRY;
		while (test_ioctl(fd, VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &frmival) >= 0) {
			print_frmival(frmival);
			frmival.index++;
		}
	}
}
