#include <endian.h>

#include "v4l2-ctl.h"

static struct v4l2_format vfmt;	/* set_format/get_format */
static unsigned mbus_code;
static unsigned mbus_code_out;

void meta_usage()
{
	printf("\nMetadata Formats options:\n"
	       "  --list-formats-meta [<mbus_code>] display supported metadata capture formats.\n"
	       "		      <mbus_code> is an optional media bus code, if the device has\n"
	       "		      capability V4L2_CAP_IO_MC then only formats that support this\n"
	       "		      media bus code are listed [VIDIOC_ENUM_FMT]\n"
	       "  --get-fmt-meta      query the metadata capture format [VIDIOC_G_FMT]\n"
	       "  --set-fmt-meta <f>  set the metadata capture format [VIDIOC_S_FMT]\n"
	       "                     parameter is either the format index as reported by\n"
	       "                     --list-formats-meta, or the fourcc value as a string\n"
	       "  --try-fmt-meta <f>  try the metadata capture format [VIDIOC_TRY_FMT]\n"
	       "                     parameter is either the format index as reported by\n"
	       "                     --list-formats-meta, or the fourcc value as a string\n"
	       "  --list-formats-meta-out [<mbus_code>] display supported metadata output formats.\n"
	       "		      <mbus_code> is an optional media bus code, if the device has\n"
	       "		      capability V4L2_CAP_IO_MC then only formats that support this\n"
	       "		      media bus code are listed [VIDIOC_ENUM_FMT]\n"
	       "  --get-fmt-meta-out      query the metadata output format [VIDIOC_G_FMT]\n"
	       "  --set-fmt-meta-out <f>  set the metadata output format [VIDIOC_S_FMT]\n"
	       "                          parameter is either the format index as reported by\n"
	       "                          --list-formats-meta-out, or the fourcc value as a string\n"
	       "  --try-fmt-meta-out <f>  try the metadata output format [VIDIOC_TRY_FMT]\n"
	       "                          parameter is either the format index as reported by\n"
	       "                          --list-formats-meta-out, or the fourcc value as a string\n"
	       );
}

void meta_cmd(int ch, char *optarg)
{
	switch (ch) {
	case OptSetMetaFormat:
	case OptTryMetaFormat:
	case OptSetMetaOutFormat:
	case OptTryMetaOutFormat:
		if (strlen(optarg) == 0) {
			meta_usage();
			std::exit(EXIT_FAILURE);
		} else if (strlen(optarg) == 4) {
			vfmt.fmt.meta.dataformat = v4l2_fourcc(optarg[0],
					optarg[1], optarg[2], optarg[3]);
		} else {
			vfmt.fmt.meta.dataformat = strtol(optarg, nullptr, 0);
		}
		break;
	case OptListMetaFormats:
		if (optarg)
			mbus_code = strtoul(optarg, nullptr, 0);
		break;
	case OptListMetaOutFormats:
		if (optarg)
			mbus_code_out = strtoul(optarg, nullptr, 0);
		break;
	}
}

static void __meta_set(cv4l_fd &_fd, bool set, bool _try, __u32 type)
{
	struct v4l2_format in_vfmt;
	int fd = _fd.g_fd();
	int ret;

	if (!set && !_try)
		return;

	in_vfmt.type = type;
	in_vfmt.fmt.meta.dataformat = vfmt.fmt.meta.dataformat;

	if (in_vfmt.fmt.meta.dataformat < 256) {
		struct v4l2_fmtdesc fmt = {};

		fmt.index = in_vfmt.fmt.meta.dataformat;
		fmt.type = in_vfmt.type;

		if (doioctl(fd, VIDIOC_ENUM_FMT, &fmt))
			fmt.pixelformat = 0;

		in_vfmt.fmt.meta.dataformat = fmt.pixelformat;
	}

	if (set)
		ret = doioctl(fd, VIDIOC_S_FMT, &in_vfmt);
	else
		ret = doioctl(fd, VIDIOC_TRY_FMT, &in_vfmt);
	if (ret == 0 && (verbose || _try))
		printfmt(fd, in_vfmt);
}

void meta_set(cv4l_fd &_fd)
{
	__meta_set(_fd, options[OptSetMetaFormat], options[OptTryMetaFormat],
		   V4L2_BUF_TYPE_META_CAPTURE);
	__meta_set(_fd, options[OptSetMetaOutFormat],
		   options[OptTryMetaOutFormat], V4L2_BUF_TYPE_META_OUTPUT);
}

static void __meta_get(cv4l_fd &fd, __u32 type)
{
	vfmt.type = type;
	if (doioctl(fd.g_fd(), VIDIOC_G_FMT, &vfmt) == 0)
		printfmt(fd.g_fd(), vfmt);
}

void meta_get(cv4l_fd &fd)
{
	if (options[OptGetMetaFormat])
		__meta_get(fd, V4L2_BUF_TYPE_META_CAPTURE);
	if (options[OptGetMetaOutFormat])
		__meta_get(fd, V4L2_BUF_TYPE_META_OUTPUT);
}

void meta_list(cv4l_fd &fd)
{
	if (options[OptListMetaFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_META_CAPTURE, mbus_code);
	}

	if (options[OptListMetaOutFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_META_OUTPUT, mbus_code_out);
	}
}

struct uvc_meta_buf {
	__u64 ns;
	__u16 sof;
	__u8 length;
	__u8 flags;
	__u8 buf[10];
};

#define UVC_STREAM_SCR	(1 << 3)
#define UVC_STREAM_PTS	(1 << 2)

struct vivid_meta_out_buf {
        __u16	brightness;
        __u16	contrast;
        __u16	saturation;
        __s16	hue;
};

void print_meta_buffer(FILE *f, cv4l_buffer &buf, cv4l_fmt &fmt, cv4l_queue &q)
{
	struct uvc_meta_buf *vbuf;
	int buf_off = 0;
	struct vivid_meta_out_buf *vbuf_out;

	switch (fmt.g_pixelformat()) {
	case V4L2_META_FMT_UVC:
		fprintf(f, "UVC: ");
		vbuf = static_cast<uvc_meta_buf *>(q.g_dataptr(buf.g_index(), 0));

		fprintf(f, "%.6fs sof: %4d len: %u flags: 0x%02x",
			static_cast<double>(vbuf->ns) / 1000000000.0,
			vbuf->sof,
			vbuf->length,
			vbuf->flags);
		if (vbuf->flags & UVC_STREAM_PTS) {
			fprintf(f, " PTS: %u", le32toh(*(__u32*)(vbuf->buf)));
			buf_off = 4;
		}
		if (vbuf->flags & UVC_STREAM_SCR)
			fprintf(f, " STC: %u SOF counter: %u",
				le32toh(*(__u32*)(vbuf->buf + buf_off)),
				le16toh(*(__u16*)(vbuf->buf + buf_off + 4)));
		fprintf(f, "\n");
		break;
	case V4L2_META_FMT_VIVID:
		fprintf(f, "VIVID:");
		vbuf_out = static_cast<vivid_meta_out_buf *>(q.g_dataptr(buf.g_index(), 0));

		fprintf(f, " brightness: %u contrast: %u saturation: %u  hue: %d\n",
			vbuf_out->brightness, vbuf_out->contrast,
			vbuf_out->saturation, vbuf_out->hue);
		break;
	}
}

void meta_fillbuffer(cv4l_buffer &buf, cv4l_fmt &fmt, cv4l_queue &q)
{
	struct vivid_meta_out_buf *vbuf;

	switch (fmt.g_pixelformat()) {
		case V4L2_META_FMT_VIVID:
			vbuf = static_cast<vivid_meta_out_buf *>(q.g_dataptr(buf.g_index(), 0));
			vbuf->brightness = buf.g_sequence() % 192 + 64;
			vbuf->contrast = (buf.g_sequence() + 10) % 192 + 64;
			vbuf->saturation = (buf.g_sequence() + 20) % 256;
			vbuf->hue = buf.g_sequence() % 257 - 128;
			break;
	}
}
