#include <cctype>

#include <endian.h>

#include "v4l2-ctl.h"

static struct v4l2_frmsizeenum frmsize; /* list frame sizes */
static struct v4l2_frmivalenum frmival; /* list frame intervals */
static unsigned set_fmts;
static __u32 width, height, pixfmt, field, flags;
static __u32 ycbcr, quantization, xfer_func, colorspace;
static __u32 bytesperline[VIDEO_MAX_PLANES];
static __u32 sizeimage[VIDEO_MAX_PLANES];
static unsigned mbus_code;

void vidcap_usage()
{
	printf("\nVideo Capture Formats options:\n"
	       "  --list-formats [<mbus_code>]\n"
	       "		     display supported video formats. <mbus_code> is an optional\n"
	       "		     media bus code, if the device has capability V4L2_CAP_IO_MC\n"
	       "		     then only formats that support this media bus code are listed\n"
	       "		     [VIDIOC_ENUM_FMT]\n"
	       "  --list-formats-ext [<mbus_code>]\n"
	       "		     display supported video formats including frame sizes and intervals\n"
	       "		     <mbus_code> is an optional media bus code, if the device has\n"
	       "		     capability V4L2_CAP_IO_MC then only formats that support this\n"
	       "		     media bus code are listed [VIDIOC_ENUM_FMT]\n"
	       "  --list-framesizes <f>\n"
	       "                     list supported framesizes for pixelformat <f>\n"
	       "                     [VIDIOC_ENUM_FRAMESIZES]\n"
	       "                     pixelformat is the fourcc value as a string\n"
	       "  --list-frameintervals width=<w>,height=<h>,pixelformat=<f>\n"
	       "                     list supported frame intervals for pixelformat <f> and\n"
	       "                     the given width and height [VIDIOC_ENUM_FRAMEINTERVALS]\n"
	       "                     pixelformat is the fourcc value as a string\n"
	       "  --list-fields      list supported fields for the current format\n"
	       "  -V, --get-fmt-video\n"
	       "     		     query the video capture format [VIDIOC_G_FMT]\n"
	       "  -v, --set-fmt-video\n"
	       "  --try-fmt-video width=<w>,height=<h>,pixelformat=<pf>,field=<f>,colorspace=<c>,\n"
	       "                  xfer=<xf>,ycbcr=<y>,hsv=<hsv>,quantization=<q>,\n"
	       "                  premul-alpha,bytesperline=<bpl>,sizeimage=<sz>\n"
	       "                     set/try the video capture format [VIDIOC_S/TRY_FMT]\n"
	       "                     pixelformat is either the format index as reported by\n"
	       "                       --list-formats, or the fourcc value as a string.\n"
	       "                     The bytesperline and sizeimage options can be used multiple times,\n"
	       "                       once for each plane.\n"
	       "                     premul-alpha sets V4L2_PIX_FMT_FLAG_PREMUL_ALPHA.\n"
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
	       );
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
	char *value, *subs;
	bool be_pixfmt;

	switch (ch) {
	case OptSetVideoFormat:
	case OptTryVideoFormat:
		set_fmts = parse_fmt(optarg, width, height, pixfmt, field, colorspace,
				xfer_func, ycbcr, quantization, flags, bytesperline,
				sizeimage);
		if (!set_fmts) {
			vidcap_usage();
			std::exit(EXIT_FAILURE);
		}
		break;
	case OptListFormats:
	case OptListFormatsExt:
		if (optarg)
			mbus_code = strtoul(optarg, nullptr, 0);
		break;
	case OptListFrameSizes:
		be_pixfmt = strlen(optarg) == 7 && !memcmp(optarg + 4, "-BE", 3);
		if (be_pixfmt || strlen(optarg) == 4) {
			frmsize.pixel_format = v4l2_fourcc(optarg[0], optarg[1],
							   optarg[2], optarg[3]);
			if (be_pixfmt)
				frmsize.pixel_format |= 1U << 31;
		} else if (isdigit(optarg[0])) {
			frmsize.pixel_format = strtol(optarg, nullptr, 0);
		} else {
			fprintf(stderr, "The pixelformat '%s' is invalid\n", optarg);
			std::exit(EXIT_FAILURE);
		}
		break;
	case OptListFrameIntervals:
		subs = optarg;
		while (*subs != '\0') {
			static constexpr const char *subopts[] = {
				"width",
				"height",
				"pixelformat",
				nullptr
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				frmival.width = strtol(value, nullptr, 0);
				break;
			case 1:
				frmival.height = strtol(value, nullptr, 0);
				break;
			case 2:
				be_pixfmt = strlen(value) == 7 && !memcmp(value + 4, "-BE", 3);
				if (be_pixfmt || strlen(value) == 4) {
					frmival.pixel_format =
						v4l2_fourcc(value[0], value[1],
							    value[2], value[3]);
					if (be_pixfmt)
						frmival.pixel_format |= 1U << 31;
				} else if (isdigit(optarg[0])) {
					frmival.pixel_format = strtol(value, nullptr, 0);
				} else {
					fprintf(stderr, "The pixelformat '%s' is invalid\n", optarg);
					std::exit(EXIT_FAILURE);
				}
				break;
			default:
				vidcap_usage();
				std::exit(EXIT_FAILURE);
			}
		}
		break;
	}
}

int vidcap_get_and_update_fmt(cv4l_fd &_fd, struct v4l2_format &vfmt)
{
	int fd = _fd.g_fd();
	int ret;

	memset(&vfmt, 0, sizeof(vfmt));
	vfmt.fmt.pix.priv = priv_magic;
	vfmt.type = vidcap_buftype;

	ret = doioctl(fd, VIDIOC_G_FMT, &vfmt);
	if (ret)
		return ret;

	if (is_multiplanar) {
		if (set_fmts & FmtWidth)
			vfmt.fmt.pix_mp.width = width;
		if (set_fmts & FmtHeight)
			vfmt.fmt.pix_mp.height = height;
		if (set_fmts & FmtPixelFormat) {
			vfmt.fmt.pix_mp.pixelformat = pixfmt;
			if (vfmt.fmt.pix_mp.pixelformat < 256) {
				vfmt.fmt.pix_mp.pixelformat = pixfmt =
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
			/*
			 * G_FMT might return bytesperline values > width,
			 * reset them to 0 to force the driver to update them
			 * to the closest value for the new width.
			 */
			for (unsigned i = 0; i < vfmt.fmt.pix_mp.num_planes; i++)
				vfmt.fmt.pix_mp.plane_fmt[i].bytesperline = 0;
		}
		if (set_fmts & FmtSizeImage) {
			for (unsigned i = 0; i < VIDEO_MAX_PLANES; i++)
				vfmt.fmt.pix_mp.plane_fmt[i].sizeimage =
					sizeimage[i];
		}

		if (set_fmts & FmtColorspace) {
			vfmt.fmt.pix_mp.flags |= V4L2_PIX_FMT_FLAG_SET_CSC;
			vfmt.fmt.pix_mp.colorspace = colorspace;
		}
		if (set_fmts & FmtYCbCr) {
			vfmt.fmt.pix_mp.flags |= V4L2_PIX_FMT_FLAG_SET_CSC;
			vfmt.fmt.pix_mp.ycbcr_enc = ycbcr;
		}
		if (set_fmts & FmtQuantization) {
			vfmt.fmt.pix_mp.flags |= V4L2_PIX_FMT_FLAG_SET_CSC;
			vfmt.fmt.pix_mp.quantization = quantization;
		}
		if (set_fmts & FmtXferFunc) {
			vfmt.fmt.pix_mp.flags |= V4L2_PIX_FMT_FLAG_SET_CSC;
			vfmt.fmt.pix_mp.xfer_func = xfer_func;
		}
	} else {
		if (set_fmts & FmtWidth)
			vfmt.fmt.pix.width = width;
		if (set_fmts & FmtHeight)
			vfmt.fmt.pix.height = height;
		if (set_fmts & FmtPixelFormat) {
			vfmt.fmt.pix.pixelformat = pixfmt;
			if (vfmt.fmt.pix.pixelformat < 256) {
				vfmt.fmt.pix.pixelformat = pixfmt =
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
			/*
			 * G_FMT might return a bytesperline value > width,
			 * reset this to 0 to force the driver to update it
			 * to the closest value for the new width.
			 */
			vfmt.fmt.pix.bytesperline = 0;
		}
		if (set_fmts & FmtSizeImage)
			vfmt.fmt.pix.sizeimage = sizeimage[0];
		if (set_fmts & FmtColorspace) {
			vfmt.fmt.pix.flags |= V4L2_PIX_FMT_FLAG_SET_CSC;
			vfmt.fmt.pix.colorspace = colorspace;
		}
		if (set_fmts & FmtYCbCr) {
			vfmt.fmt.pix.flags |= V4L2_PIX_FMT_FLAG_SET_CSC;
			vfmt.fmt.pix.ycbcr_enc = ycbcr;
		}
		if (set_fmts & FmtQuantization) {
			vfmt.fmt.pix.flags |= V4L2_PIX_FMT_FLAG_SET_CSC;
			vfmt.fmt.pix.quantization = quantization;
		}
		if (set_fmts & FmtXferFunc) {
			vfmt.fmt.pix.flags |= V4L2_PIX_FMT_FLAG_SET_CSC;
			vfmt.fmt.pix.xfer_func = xfer_func;
		}

	}

	if ((set_fmts & FmtPixelFormat) &&
	    !valid_pixel_format(fd, pixfmt, false, is_multiplanar)) {
		if (pixfmt)
			fprintf(stderr, "The pixelformat '%s' is invalid\n",
				fcc2s(pixfmt).c_str());
		else
			fprintf(stderr, "The pixelformat index was invalid\n");
		return -EINVAL;
	}
	return 0;
}

void vidcap_set(cv4l_fd &_fd)
{
	if (options[OptSetVideoFormat] || options[OptTryVideoFormat]) {
		int fd = _fd.g_fd();
		int ret;
		struct v4l2_format vfmt;

		if (vidcap_get_and_update_fmt(_fd, vfmt) == 0) {
			if (options[OptSetVideoFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &vfmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &vfmt);
			if (ret == 0 && (verbose || options[OptTryVideoFormat]))
				printfmt(fd, vfmt);
		}
	}
}

void vidcap_get(cv4l_fd &fd)
{
	if (options[OptGetVideoFormat]) {
		struct v4l2_format vfmt;

		memset(&vfmt, 0, sizeof(vfmt));
		vfmt.fmt.pix.priv = priv_magic;
		vfmt.type = vidcap_buftype;
		if (doioctl(fd.g_fd(), VIDIOC_G_FMT, &vfmt) == 0)
			printfmt(fd.g_fd(), vfmt);
	}
}

void vidcap_list(cv4l_fd &fd)
{
	if (options[OptListFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, vidcap_buftype, mbus_code);
	}

	if (options[OptListFormatsExt]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats_ext(fd, vidcap_buftype, mbus_code);
	}

	if (options[OptListFields]) {
		print_video_fields(fd.g_fd());
	}

	if (options[OptListFrameSizes]) {
		frmsize.index = 0;
		if (frmsize.pixel_format < 256) {
			frmsize.pixel_format =
				find_pixel_format(fd.g_fd(), frmsize.pixel_format,
						  false, is_multiplanar);
			if (!frmsize.pixel_format) {
				fprintf(stderr, "The pixelformat index was invalid\n");
				std::exit(EXIT_FAILURE);
			}
		}
		if (!valid_pixel_format(fd.g_fd(), frmsize.pixel_format, false, is_multiplanar) &&
		    !valid_pixel_format(fd.g_fd(), frmsize.pixel_format, true, is_multiplanar)) {
			fprintf(stderr, "The pixelformat '%s' is invalid\n",
				fcc2s(frmsize.pixel_format).c_str());
			std::exit(EXIT_FAILURE);
		}

		printf("ioctl: VIDIOC_ENUM_FRAMESIZES\n");
		while (test_ioctl(fd.g_fd(), VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
			print_frmsize(frmsize, "");
			frmsize.index++;
		}
	}

	if (options[OptListFrameIntervals]) {
		frmival.index = 0;
		if (frmival.pixel_format < 256) {
			frmival.pixel_format =
				find_pixel_format(fd.g_fd(), frmival.pixel_format,
						  false, is_multiplanar);
			if (!frmival.pixel_format) {
				fprintf(stderr, "The pixelformat index was invalid\n");
				std::exit(EXIT_FAILURE);
			}
		}
		if (!valid_pixel_format(fd.g_fd(), frmival.pixel_format, false, is_multiplanar)) {
			fprintf(stderr, "The pixelformat '%s' is invalid\n",
				fcc2s(frmival.pixel_format).c_str());
			std::exit(EXIT_FAILURE);
		}

		printf("ioctl: VIDIOC_ENUM_FRAMEINTERVALS\n");
		while (test_ioctl(fd.g_fd(), VIDIOC_ENUM_FRAMEINTERVALS, &frmival) >= 0) {
			print_frmival(frmival, "");
			frmival.index++;
		}
	}
}

void print_touch_buffer(FILE *f, cv4l_buffer &buf, cv4l_fmt &fmt, cv4l_queue &q)
{
	static constexpr char img[16] = {
		'.', ',', ':', ';', '!', '|', 'i', 'c',
		'n', 'o', 'm', 'I', 'C', 'N', 'O', 'M',
	};
	auto vbuf = static_cast<__s16 *>(q.g_dataptr(buf.g_index(), 0));
	__u32 x, y;

	switch (fmt.g_pixelformat()) {
	case V4L2_TCH_FMT_DELTA_TD16:
		for (y = 0; y < fmt.g_height(); y++) {
			fprintf(f, "TD16: ");

			for (x = 0; x < fmt.g_width(); x++, vbuf++) {
				auto v = static_cast<__s16>(le16toh(*vbuf));

				if (!options[OptConcise])
					fprintf(f, "% 4d", v);
				else if (v > 255)
					fprintf(f, "*");
				else if (v < -32)
					fprintf(f, "-");
				else if (v < 0)
					fprintf(f, "%c", img[0]);
				else
					fprintf(f, "%c", img[v / 16]);
			}
			fprintf(f, "\n");
		}
		break;
	}
}
