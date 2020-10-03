#include "v4l2-ctl.h"

static unsigned set_fmts_out;
static __u32 width, height, pixfmt, field, colorspace, xfer_func, ycbcr, quantization, flags;
static __u32 bytesperline[VIDEO_MAX_PLANES];
static __u32 sizeimage[VIDEO_MAX_PLANES];
static unsigned mbus_code_out;

void vidout_usage()
{
	printf("\nVideo Output Formats options:\n"
	       "  --list-formats-out [<mbus_code>] display supported video output formats.\n"
	       "		     <mbus_code> is an optional media bus code, if the device has\n"
	       "		     capability V4L2_CAP_IO_MC then only formats that support this\n"
	       "		     media bus code are listed [VIDIOC_ENUM_FMT]\n"
	       "  --list-formats-out-ext [<mbus_code>] display supported video output formats including\n"
	       "		     frame sizes and intervals. <mbus_code> is an optional media bus code,\n"
	       "		     if the device has capability V4L2_CAP_IO_MC then only formats that\n"
	       "		     support this media bus code are listed [VIDIOC_ENUM_FMT]\n"
	       "  --list-fields-out  list supported fields for the current output format\n"
	       "  -X, --get-fmt-video-out\n"
	       "     		     query the video output format [VIDIOC_G_FMT]\n"
	       "  -x, --set-fmt-video-out\n"
	       "  --try-fmt-video-out width=<w>,height=<h>,pixelformat=<pf>,field=<f>,colorspace=<c>,\n"
	       "                      xfer=<xf>,ycbcr=<y>,hsv=<hsv>,quantization=<q>,\n"
	       "                      premul-alpha,bytesperline=<bpl>,sizeimage=<sz>\n"
	       "                     set/try the video output format [VIDIOC_S/TRY_FMT]\n"
	       "                     pixelformat is either the format index as reported by\n"
	       "                       --list-formats-out, or the fourcc value as a string.\n"
	       "                     premul-alpha sets V4L2_PIX_FMT_FLAG_PREMUL_ALPHA.\n"
	       "                     The bytesperline and sizeimage options can be used multiple times,\n"
	       "                       once for each plane.\n"
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

static void print_video_out_fields(int fd)
{
	struct v4l2_format fmt;
	struct v4l2_format tmp;

	memset(&fmt, 0, sizeof(fmt));
	fmt.fmt.pix.priv = priv_magic;
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
	switch (ch) {
	case OptSetVideoOutFormat:
	case OptTryVideoOutFormat:
		set_fmts_out = parse_fmt(optarg, width, height, pixfmt, field,
				colorspace, xfer_func, ycbcr, quantization, flags, bytesperline,
				sizeimage);
		if (!set_fmts_out) {
			vidcap_usage();
			std::exit(EXIT_FAILURE);
		}
		break;
	case OptListOutFormats:
	case OptListOutFormatsExt:
		if (optarg)
			mbus_code_out = strtoul(optarg, nullptr, 0);
		break;
	}
}

void vidout_set(cv4l_fd &_fd)
{
	int fd = _fd.g_fd();
	int ret;

	if (options[OptSetVideoOutFormat] || options[OptTryVideoOutFormat]) {
		struct v4l2_format vfmt;

		memset(&vfmt, 0, sizeof(vfmt));
		vfmt.fmt.pix.priv = priv_magic;
		vfmt.type = vidout_buftype;
		if (doioctl(fd, VIDIOC_G_FMT, &vfmt) == 0) {
			if (is_multiplanar) {
				if (set_fmts_out & FmtWidth)
					vfmt.fmt.pix_mp.width = width;
				if (set_fmts_out & FmtHeight)
					vfmt.fmt.pix_mp.height = height;
				if (set_fmts_out & FmtPixelFormat) {
					vfmt.fmt.pix_mp.pixelformat = pixfmt;
					if (vfmt.fmt.pix_mp.pixelformat < 256) {
						vfmt.fmt.pix_mp.pixelformat = pixfmt =
							find_pixel_format(fd, vfmt.fmt.pix_mp.pixelformat,
									true, true);
					}
				}
				if (set_fmts_out & FmtField)
					vfmt.fmt.pix_mp.field = field;
				if (set_fmts_out & FmtColorspace)
					vfmt.fmt.pix_mp.colorspace = colorspace;
				if (set_fmts_out & FmtXferFunc)
					vfmt.fmt.pix_mp.xfer_func = xfer_func;
				if (set_fmts_out & FmtYCbCr)
					vfmt.fmt.pix_mp.ycbcr_enc = ycbcr;
				if (set_fmts_out & FmtQuantization)
					vfmt.fmt.pix_mp.quantization = quantization;
				if (set_fmts_out & FmtFlags)
					vfmt.fmt.pix_mp.flags = flags;
				if (set_fmts_out & FmtBytesPerLine) {
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
				if (set_fmts_out & FmtSizeImage) {
					for (unsigned i = 0; i < VIDEO_MAX_PLANES; i++)
						vfmt.fmt.pix_mp.plane_fmt[i].sizeimage =
							sizeimage[i];
				}
			} else {
				if (set_fmts_out & FmtWidth)
					vfmt.fmt.pix.width = width;
				if (set_fmts_out & FmtHeight)
					vfmt.fmt.pix.height = height;
				if (set_fmts_out & FmtPixelFormat) {
					vfmt.fmt.pix.pixelformat = pixfmt;
					if (vfmt.fmt.pix.pixelformat < 256) {
						vfmt.fmt.pix.pixelformat = pixfmt =
							find_pixel_format(fd, vfmt.fmt.pix.pixelformat,
									true, false);
					}
				}
				if (set_fmts_out & FmtField)
					vfmt.fmt.pix.field = field;
				if (set_fmts_out & FmtColorspace)
					vfmt.fmt.pix.colorspace = colorspace;
				if (set_fmts_out & FmtXferFunc)
					vfmt.fmt.pix.xfer_func = xfer_func;
				if (set_fmts_out & FmtYCbCr)
					vfmt.fmt.pix.ycbcr_enc = ycbcr;
				if (set_fmts_out & FmtQuantization)
					vfmt.fmt.pix.quantization = quantization;
				if (set_fmts_out & FmtFlags)
					vfmt.fmt.pix.flags = flags;
				if (set_fmts_out & FmtBytesPerLine) {
					vfmt.fmt.pix.bytesperline = bytesperline[0];
				} else {
					/* G_FMT might return a bytesperline value > width,
					 * reset this to 0 to force the driver to update it
					 * to the closest value for the new width. */
					vfmt.fmt.pix.bytesperline = 0;
				}
				if (set_fmts_out & FmtSizeImage)
					vfmt.fmt.pix.sizeimage = sizeimage[0];
			}

			if ((set_fmts_out & FmtPixelFormat) &&
			    !valid_pixel_format(fd, pixfmt, true, is_multiplanar)) {
				if (pixfmt)
					fprintf(stderr, "The pixelformat '%s' is invalid\n",
						fcc2s(pixfmt).c_str());
				else
					fprintf(stderr, "The pixelformat index was invalid\n");
				std::exit(EXIT_FAILURE);
			}

			if (options[OptSetVideoOutFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &vfmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &vfmt);
			if (ret == 0 && (verbose || options[OptTryVideoOutFormat]))
				printfmt(fd, vfmt);
		}
	}
}

void vidout_get(cv4l_fd &fd)
{
	if (options[OptGetVideoOutFormat]) {
		struct v4l2_format vfmt;

		memset(&vfmt, 0, sizeof(vfmt));
		vfmt.fmt.pix.priv = priv_magic;
		vfmt.type = vidout_buftype;
		if (doioctl(fd.g_fd(), VIDIOC_G_FMT, &vfmt) == 0)
			printfmt(fd.g_fd(), vfmt);
	}
}

void vidout_list(cv4l_fd &fd)
{
	if (options[OptListOutFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, vidout_buftype, mbus_code_out);
	}

	if (options[OptListOutFormatsExt]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats_ext(fd, vidout_buftype, mbus_code_out);
	}

	if (options[OptListOutFields]) {
		print_video_out_fields(fd.g_fd());
	}
}
