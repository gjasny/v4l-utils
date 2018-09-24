// SPDX-License-Identifier: GPL-2.0-only
/*
 * qvidcap: a control panel controlling v4l2 devices.
 *
 * Copyright 2018 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <QApplication>
#include <QScrollArea>
#include <QtMath>

#include "qvidcap.h"
#include "capture-win-gl.h"

#include <libv4lconvert.h>
#include "v4l-stream.h"
#include "v4l2-info.h"

static void usage()
{
	printf("Usage: qvidcap <options>\n\n"
	       "Options:\n\n"
	       "  -d, --device=<dev>       use device <dev> as the video device\n"
	       "                           if <dev> is a number, then /dev/video<dev> is used\n"
	       "  -f, --file=<file>        read from the file <file> for the raw frame data\n"
	       "  -p, --port[=<port>]      listen for a network connection on the given port\n"
	       "                           The default port is %d\n"
	       "  -T, --tpg                use the test pattern generator\n"
	       "\n"
	       "  If neither -d, -f, -p nor -T is specified then use /dev/video0.\n"
	       "\n"
	       "  -c, --count=<cnt>        stop after <cnt> captured frames\n"
	       "  -b, --buffers=<bufs>     request <bufs> buffers (default 4) when streaming\n"
	       "                           from a video device\n"
	       "  -s, --single-step[=<frm>] starting with frame <frm> (default 1), pause after\n"
	       "                           displaying each frame until Space is pressed.\n"
	       "  -C, --colorspace=<c>     override colorspace\n"
	       "                           <c> can be one of the following colorspaces:\n"
	       "                               smpte170m, smpte240m, rec709, 470m, 470bg, jpeg, srgb,\n"
	       "                               oprgb, bt2020, dcip3\n"
	       "  -X, --xfer-func=<x>      override transfer function\n"
	       "                           <x> can be one of the following transfer functions:\n"
	       "                               default, 709, srgb, oprgb, smpte240m, smpte2084, dcip3, none\n"
	       "  -Y, --ycbcr-enc=<y>      override Y'CbCr encoding\n"
	       "                           <y> can be one of the following Y'CbCr encodings:\n"
	       "                               default, 601, 709, xv601, xv709, bt2020, bt2020c, smpte240m\n"
	       "  -H, --hsv-enc=<hsv>      override HSV encoding\n"
	       "                           <hsv> can be one of the following HSV encodings:\n"
	       "                               default, 180, 256\n"
	       "  -Q, --quant=<q>          override quantization\n"
	       "                           <q> can be one of the following quantization methods:\n"
	       "                               default, full-range, lim-range\n"
	       "  -P, --pixelformat=<p>    For video devices: set the format to this pixel format.\n"
	       "                           For reading from a file: interpret the data using this\n"
	       "                           pixel format setting.\n"
	       "                           Use -l to see the list of supported pixel formats.\n"
	       "\n"
	       "  -l, --list-formats       display all supported formats\n"
	       "  -h, --help               display this help message\n"
	       "  -t, --timings            report frame render timings\n"
	       "  -v, --verbose            be more verbose\n"
	       "  -R, --raw                open device in raw mode\n"
	       "\n"
	       "  --opengl                 force openGL to display the video\n"
	       "  --opengles               force openGL ES to display the video\n"
	       "\n"
	       "  The following options are ignored when capturing from a video device:\n"
	       "\n"
	       "  -W, --width=<width>      set width\n"
	       "  -H, --height=<height>    set frame (not field!) height\n"
	       "  --fps=<fps>              set frames-per-second (default is 30)\n"
	       "\n"
	       "  The following option is only valid when reading from a file:\n"
	       "\n"
	       "  -F, --field=<f>          override field setting\n"
	       "                           <f> can be one of the following field layouts:\n"
	       "                               any, none, top, bottom, interlaced, seq_tb, seq_bt,\n"
	       "                               alternate, interlaced_tb, interlaced_bt\n"
	       "  The following options are specific to the test pattern generator:\n"
	       "\n"
	       "  --list-patterns          list available patterns for use with --pattern\n"
	       "  --pattern=<pat>          choose output test pattern, the default is 0\n"
	       "  --square                 show a square in the middle of the output test pattern\n"
	       "  --border                 show a border around the pillar/letterboxed video\n"
	       "  --sav                    insert an SAV code in every line\n"
	       "  --eav                    insert an EAV code in every line\n"
	       "  --pixel-aspect=<aspect>  select a pixel aspect ratio, the default is to autodetect\n"
	       "                           <aspect> can be one of: square, ntsc, pal\n"
	       "  --video-aspect=<aspect>  select a video aspect ratio, the default is to use the frame ratio\n"
	       "                           <aspect> can be one of: 4x3, 14x9, 16x9, anamorphic\n"
	       "  --alpha=<alpha-value>    value to use for the alpha component, range 0-255, the default is 0\n"
	       "  --alpha-red-only         only use the --alpha value for the red colors, for all others use 0\n"
	       "  --rgb-lim-range          encode RGB values as limited [16-235] instead of full range\n"
	       "  --hor-speed=<speed>      choose speed for horizontal movement, the default is 0\n"
	       "                           and the range is [-3...3]\n"
	       "  --vert-speed=<speed>     choose speed for vertical movement, the default is 0\n"
	       "                           and the range is [-3...3]\n"
	       "  --perc-fill=<percentage> percentage of the frame to actually fill. the default is 100%%\n"
	       "\n"
	       "  These options use the test pattern generator to test the OpenGL backend:\n"
	       "\n"
	       "  --test=<count>           test all formats, each test generates <count> frames.\n"
	       "  --test-mask=<mask>       mask which tests are performed.\n"
	       "                           <mask> is a bit mask with these values:\n"
	       "                           0x01: mask iterating over pixel formats\n"
	       "                           0x02: mask iterating over fields\n"
	       "                           0x04: mask iterating over colorspaces\n"
	       "                           0x08: mask iterating over transfer functions\n"
	       "                           0x10: mask iterating over Y'CbCr/HSV encodings\n"
	       "                           0x20: mask iterating over quantization ranges\n",
		V4L_STREAM_PORT);
}

static void usageError(const char *msg)
{
	printf("Missing parameter for %s\n", msg);
	usage();
}

static void usageInvParm(const char *msg)
{
	printf("Invalid parameter for %s\n", msg);
	usage();
}

static QString getDeviceName(QString dev, QString &name)
{
	bool ok;
	name.toInt(&ok);
	return ok ? QString("%1%2").arg(dev).arg(name) : name;
}

static __u32 parse_field(const QString &s)
{
	if (s == "any") return V4L2_FIELD_ANY;
	if (s == "none") return V4L2_FIELD_NONE;
	if (s == "top") return V4L2_FIELD_TOP;
	if (s == "bottom") return V4L2_FIELD_BOTTOM;
	if (s == "interlaced") return V4L2_FIELD_INTERLACED;
	if (s == "seq_tb") return V4L2_FIELD_SEQ_TB;
	if (s == "seq_bt") return V4L2_FIELD_SEQ_BT;
	if (s == "alternate") return V4L2_FIELD_ALTERNATE;
	if (s == "interlaced_tb") return V4L2_FIELD_INTERLACED_TB;
	if (s == "interlaced_bt") return V4L2_FIELD_INTERLACED_BT;
	return V4L2_FIELD_ANY;
}

static __u32 parse_colorspace(const QString &s)
{
	if (s == "smpte170m") return V4L2_COLORSPACE_SMPTE170M;
	if (s == "smpte240m") return V4L2_COLORSPACE_SMPTE240M;
	if (s == "rec709") return V4L2_COLORSPACE_REC709;
	if (s == "470m") return V4L2_COLORSPACE_470_SYSTEM_M;
	if (s == "470bg") return V4L2_COLORSPACE_470_SYSTEM_BG;
	if (s == "jpeg") return V4L2_COLORSPACE_JPEG;
	if (s == "srgb") return V4L2_COLORSPACE_SRGB;
	if (s == "oprgb") return V4L2_COLORSPACE_OPRGB;
	if (s == "bt2020") return V4L2_COLORSPACE_BT2020;
	if (s == "dcip3") return V4L2_COLORSPACE_DCI_P3;
	return 0;
}

static __u32 parse_xfer_func(const QString &s)
{
	if (s == "default") return V4L2_XFER_FUNC_DEFAULT;
	if (s == "smpte240m") return V4L2_XFER_FUNC_SMPTE240M;
	if (s == "rec709") return V4L2_XFER_FUNC_709;
	if (s == "srgb") return V4L2_XFER_FUNC_SRGB;
	if (s == "oprgb") return V4L2_XFER_FUNC_OPRGB;
	if (s == "dcip3") return V4L2_XFER_FUNC_DCI_P3;
	if (s == "smpte2084") return V4L2_XFER_FUNC_SMPTE2084;
	if (s == "none") return V4L2_XFER_FUNC_NONE;
	return 0;
}

static __u32 parse_ycbcr(const QString &s)
{
	if (s == "default") return V4L2_YCBCR_ENC_DEFAULT;
	if (s == "601") return V4L2_YCBCR_ENC_601;
	if (s == "709") return V4L2_YCBCR_ENC_709;
	if (s == "xv601") return V4L2_YCBCR_ENC_XV601;
	if (s == "xv709") return V4L2_YCBCR_ENC_XV709;
	if (s == "bt2020") return V4L2_YCBCR_ENC_BT2020;
	if (s == "bt2020c") return V4L2_YCBCR_ENC_BT2020_CONST_LUM;
	if (s == "smpte240m") return V4L2_YCBCR_ENC_SMPTE240M;
	return V4L2_YCBCR_ENC_DEFAULT;
}

static __u32 parse_hsv(const QString &s)
{
	if (s == "default") return V4L2_YCBCR_ENC_DEFAULT;
	if (s == "180") return V4L2_HSV_ENC_180;
	if (s == "256") return V4L2_HSV_ENC_256;
	return V4L2_YCBCR_ENC_DEFAULT;
}

static __u32 parse_quantization(const QString &s)
{
	if (s == "default") return V4L2_QUANTIZATION_DEFAULT;
	if (s == "full-range") return V4L2_QUANTIZATION_FULL_RANGE;
	if (s == "lim-range") return V4L2_QUANTIZATION_LIM_RANGE;
	return V4L2_QUANTIZATION_DEFAULT;
}

static __u32 parse_pixel_format(const QString &s)
{
	for (unsigned i = 0; formats[i]; i++)
		if (s == fcc2s(formats[i]).c_str())
			return formats[i];
	return 0;
}

static unsigned findVal(const __u32 *values, __u32 &val)
{
	unsigned idx = 0;
	__u32 first = *values;

	while (*values) {
		if (*values == val)
			return idx;
		values++;
		idx++;
	}
	val = first;
	return 0;
}

static void list_formats()
{
	for (unsigned i = 0; formats[i]; i++)
		printf("'%s':\r\t\t%s\n", fcc2s(formats[i]).c_str(),
		       pixfmt2s(formats[i]).c_str());
}

static bool processOption(const QStringList &args, int &i, QString &s)
{
	int index = -1;

	if (args[i][1] == '-')
		index = args[i].indexOf('=');
	else if (args[i].length() > 2)
		index = 1;

	if (index >= 0) {
		s = args[i].mid(index + 1);
		if (s.length() == 0) {
			usageError(args[i].toLatin1());
			return false;
		}
		return true;
	}
	if (i + 1 >= args.size()) {
		usageError(args[i].toLatin1());
		return false;
	}
	s = args[++i];
	return true;
}

static bool processOption(const QStringList &args, int &i, unsigned &u)
{
	QString s;
	bool ok = processOption(args, i, s);

	if (!ok)
		return ok;
	u = s.toUInt(&ok, 0);
	if (!ok)
		usageInvParm(s.toLatin1());
	return ok;
}

static bool processOption(const QStringList &args, int &i, int &v)
{
	QString s;
	bool ok = processOption(args, i, s);

	if (!ok)
		return ok;
	v = s.toInt(&ok, 0);
	if (!ok)
		usageInvParm(s.toLatin1());
	return ok;
}

static bool processOption(const QStringList &args, int &i, double &v)
{
	QString s;
	bool ok = processOption(args, i, s);

	if (!ok)
		return ok;
	v = s.toDouble(&ok);
	if (!ok)
		usageInvParm(s.toLatin1());
	return ok;
}

static bool isOptArg(const QString &opt, const char *longOpt, const char *shortOpt = NULL)
{
	return opt.startsWith(longOpt) || (shortOpt && opt.startsWith(shortOpt));
}

static bool isOption(const QString &opt, const char *longOpt, const char *shortOpt = NULL)
{
	return opt == longOpt || opt == shortOpt;
}

__u32 read_u32(int fd)
{
	__u32 v;
	int n;

	n = read(fd, &v, sizeof(v));
	if (n != sizeof(v)) {
		fprintf(stderr, "could not read __u32\n");
		exit(1);
	}
	return ntohl(v);
}

int initSocket(int port, cv4l_fmt &fmt, v4l2_fract &pixelaspect)
{
	static int listen_fd = -1;
	int sock_fd;
	socklen_t clilen;
	struct sockaddr_in serv_addr = {}, cli_addr;
	int val = 1;

	if (listen_fd < 0) {
		listen_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (listen_fd < 0) {
			fprintf(stderr, "could not opening socket\n");
			exit(1);
		}
		setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));

		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(port);
		if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
			fprintf(stderr, "could not bind: %s\n", strerror(errno));
			exit(1);
		}
	}
	listen(listen_fd, 1);
	clilen = sizeof(cli_addr);
	sock_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &clilen);
	if (sock_fd < 0) {
		fprintf(stderr, "could not accept\n");
		exit(1);
	}
	if (read_u32(sock_fd) != V4L_STREAM_ID) {
		fprintf(stderr, "unknown protocol ID\n");
		exit(1);
	}
	__u32 version = read_u32(sock_fd);

	if (!version || version > V4L_STREAM_VERSION) {
		fprintf(stderr, "unknown protocol version %u\n", version);
		exit(1);
	}
	for (;;) {
		__u32 packet = read_u32(sock_fd);
		char buf[1024];

		if (packet == V4L_STREAM_PACKET_END) {
			fprintf(stderr, "END packet read\n");
			exit(1);
		}

		if (packet == V4L_STREAM_PACKET_FMT_VIDEO)
			break;

		unsigned sz = read_u32(sock_fd);
		while (sz) {
			unsigned rdsize = sz > sizeof(buf) ? sizeof(buf) : sz;
			int n;

			n = read(sock_fd, buf, rdsize);
			if (n < 0) {
				fprintf(stderr, "error reading %d bytes\n", sz);
				exit(1);
			}
			sz -= n;
		}
	}
	read_u32(sock_fd);
	fmt.s_type(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	unsigned sz = read_u32(sock_fd);

	if (sz != V4L_STREAM_PACKET_FMT_VIDEO_SIZE_FMT) {
		fprintf(stderr, "unsupported FMT_VIDEO size\n");
		exit(1);
	}
	fmt.s_num_planes(read_u32(sock_fd));
	fmt.s_pixelformat(read_u32(sock_fd));
	fmt.s_width(read_u32(sock_fd));
	fmt.s_height(read_u32(sock_fd));
	fmt.s_field(read_u32(sock_fd));
	fmt.s_colorspace(read_u32(sock_fd));
	fmt.s_ycbcr_enc(read_u32(sock_fd));
	fmt.s_quantization(read_u32(sock_fd));
	fmt.s_xfer_func(read_u32(sock_fd));
	fmt.s_flags(read_u32(sock_fd));
	pixelaspect.numerator = read_u32(sock_fd);
	pixelaspect.denominator = read_u32(sock_fd);

	for (unsigned i = 0; i < fmt.g_num_planes(); i++) {
		unsigned sz = read_u32(sock_fd);

		if (sz != V4L_STREAM_PACKET_FMT_VIDEO_SIZE_FMT_PLANE) {
			fprintf(stderr, "unsupported FMT_VIDEO plane size\n");
			exit(1);
		}
		fmt.s_sizeimage(read_u32(sock_fd), i);
		fmt.s_bytesperline(read_u32(sock_fd), i);
	}
	return sock_fd;
}

int main(int argc, char **argv)
{
	QApplication disp(argc, argv);
	QScrollArea *sa = new QScrollArea; // Automatically freed on window close
	QSurfaceFormat format;
	QString video_device = "0";
	QString filename;
	cv4l_fd fd;
	enum AppMode mode = AppModeV4L2;
	int sock_fd = -1;
	cv4l_fmt fmt;
	v4l2_fract pixelaspect = { 1, 1 };
	unsigned cnt = 0;
	unsigned v4l2_bufs = 4;
	bool single_step = false;
	unsigned single_step_start = 1;
	int port = 0;
	bool info_option = false;
	bool report_timings = false;
	bool verbose = false;
	__u32 overridePixelFormat = 0;
	__u32 overrideWidth = 0;
	__u32 overrideHeight = 0;
	__u32 overrideField = 0xffffffff;
	__u32 overrideColorspace = 0xffffffff;
	__u32 overrideYCbCrEnc = 0xffffffff;
	__u32 overrideHSVEnc = 0xffffffff;
	__u32 overrideXferFunc = 0xffffffff;
	__u32 overrideQuantization = 0xffffffff;
	double fps = 30;
	unsigned pattern = 0;
	unsigned test = 0;
	unsigned test_mask = 0;
	bool square = false;
	bool border = false;
	bool sav = false;
	bool eav = false;
	int tpg_pixelaspect = -1;
	tpg_video_aspect video_aspect = TPG_VIDEO_ASPECT_IMAGE;
	unsigned alpha = 0;
	bool alpha_red_only = false;
	bool rgb_lim_range = false;
	unsigned perc_fill = 100;
	tpg_move_mode hor_mode = TPG_MOVE_NONE;
	tpg_move_mode vert_mode = TPG_MOVE_NONE;
	bool force_opengl = false;
	bool force_opengles = false;

	disp.setWindowIcon(QIcon(":/qvidcap.png"));
	disp.setApplicationDisplayName("V4L2 Viewer");
	QStringList args = disp.arguments();
	for (int i = 1; i < args.size(); i++) {
		QString s;

		if (isOptArg(args[i], "--device", "-d")) {
			if (!processOption(args, i, video_device))
				return 0;
			mode = AppModeV4L2;
		} else if (isOptArg(args[i], "--file", "-f")) {
			if (!processOption(args, i, filename))
				return 0;
			mode = AppModeFile;
		} else if (isOption(args[i], "--port", "-p")) {
			mode = AppModeSocket;
			port = V4L_STREAM_PORT;
		} else if (isOptArg(args[i], "--port", "-p")) {
			if (!processOption(args, i, port))
				return 0;
			mode = AppModeSocket;
		} else if (isOption(args[i], "--tpg", "-T")) {
			mode = AppModeTPG;
		} else if (isOptArg(args[i], "--test-mask")) {
			if (!processOption(args, i, test_mask))
				return 0;
		} else if (isOptArg(args[i], "--test")) {
			if (!processOption(args, i, test))
				return 0;
			mode = AppModeTest;
			test &= ~1;
			if (test == 0)
				test = 2;
			if (!pattern)
				pattern = TPG_PAT_CSC_COLORBAR;
		} else if (isOptArg(args[i], "--pixel-format", "-P")) {
			if (!processOption(args, i, s))
				return 0;
			overridePixelFormat = parse_pixel_format(s);
		} else if (isOptArg(args[i], "--width", "-W")) {
			if (!processOption(args, i, overrideWidth))
				return 0;
		} else if (isOptArg(args[i], "--height", "-H")) {
			if (!processOption(args, i, overrideHeight))
				return 0;
		} else if (isOptArg(args[i], "--field", "-F")) {
			if (!processOption(args, i, s))
				return 0;
			overrideField = parse_field(s);
		} else if (isOptArg(args[i], "--colorspace", "-C")) {
			if (!processOption(args, i, s))
				return 0;
			overrideColorspace = parse_colorspace(s);
		} else if (isOptArg(args[i], "--ycbcr-enc", "-Y")) {
			if (!processOption(args, i, s))
				return 0;
			overrideYCbCrEnc = parse_ycbcr(s);
		} else if (isOptArg(args[i], "--hsv-enc", "-H")) {
			if (!processOption(args, i, s))
				return 0;
			overrideHSVEnc = parse_hsv(s);
		} else if (isOptArg(args[i], "--xfer-func", "-X")) {
			if (!processOption(args, i, s))
				return 0;
			overrideXferFunc = parse_xfer_func(s);
		} else if (isOptArg(args[i], "--quant", "-Q")) {
			if (!processOption(args, i, s))
				return 0;
			overrideQuantization = parse_quantization(s);
		} else if (isOption(args[i], "--list-patterns")) {
			printf("List of available patterns:\n");
			for (unsigned i = 0; tpg_pattern_strings[i]; i++)
				printf("\t%2d: %s\n", i, tpg_pattern_strings[i]);
			info_option = true;
		} else if (isOptArg(args[i], "--fps")) {
			if (!processOption(args, i, fps))
				return 0;
			if (fps <= 0)
				fps = 30;
		} else if (isOptArg(args[i], "--pattern")) {
			if (!processOption(args, i, pattern))
				return 0;
		} else if (isOption(args[i], "--square")) {
		} else if (args[i] == "--square") {
			square = true;
		} else if (isOption(args[i], "--border")) {
			border = true;
		} else if (isOption(args[i], "--sav")) {
			sav = true;
		} else if (isOption(args[i], "--eav")) {
			eav = true;
		} else if (isOptArg(args[i], "--pixel-aspect")) {
			if (!processOption(args, i, s))
				return 0;
			if (s == "square") {
				tpg_pixelaspect = TPG_PIXEL_ASPECT_SQUARE;
			} else if (s == "ntsc") {
				static const v4l2_fract hz50 = { 11, 12 };
				pixelaspect = hz50;
				tpg_pixelaspect = TPG_PIXEL_ASPECT_NTSC;
			} else if (s == "pal") {
				static const v4l2_fract hz60 = { 11, 10 };
				pixelaspect = hz60;
				tpg_pixelaspect = TPG_PIXEL_ASPECT_PAL;
			} else {
				usage();
				return 0;
			}
		} else if (isOptArg(args[i], "--video-aspect")) {
			if (!processOption(args, i, s))
				return 0;
			if (s == "4x3")
				video_aspect = TPG_VIDEO_ASPECT_4X3;
			else if (s == "14x9")
				video_aspect = TPG_VIDEO_ASPECT_14X9_CENTRE;
			else if (s == "16x9")
				video_aspect = TPG_VIDEO_ASPECT_16X9_CENTRE;
			else if (s == "anamorphic")
				video_aspect = TPG_VIDEO_ASPECT_16X9_ANAMORPHIC;
			else {
				usage();
				return 0;
			}
		} else if (isOption(args[i], "--alpha-red-only")) {
			alpha_red_only = true;
		} else if (isOptArg(args[i], "--alpha")) {
			if (!processOption(args, i, alpha))
				return 0;
			if (alpha > 255)
				alpha = 255;
		} else if (isOption(args[i], "--rgb-lim-range")) {
			rgb_lim_range = true;
		} else if (isOptArg(args[i], "--hor-speed")) {
			int speed;

			if (!processOption(args, i, speed))
				return 0;
			if (speed < -3)
				speed = -3;
			if (speed > 3)
				speed = 3;
			hor_mode = (tpg_move_mode)(speed + 3);
		} else if (isOptArg(args[i], "--vert-speed")) {
			int speed;

			if (!processOption(args, i, speed))
				return 0;
			if (speed < -3)
				speed = -3;
			if (speed > 3)
				speed = 3;
			vert_mode = (tpg_move_mode)(speed + 3);
		} else if (isOptArg(args[i], "--perc-fill")) {
			if (!processOption(args, i, perc_fill))
				return 0;
			if (perc_fill > 100)
				perc_fill = 100;
		} else if (isOption(args[i], "--help", "-h")) {
			usage();
			info_option = true;
		} else if (isOption(args[i], "--list-formats", "-l")) {
			list_formats();
			info_option = true;
		} else if (isOption(args[i], "--timings", "-t")) {
			report_timings = true;
		} else if (isOptArg(args[i], "--opengles")) {
			force_opengles = true;
		} else if (isOptArg(args[i], "--opengl")) {
			force_opengl = true;
		} else if (isOption(args[i], "--verbose", "-v")) {
			verbose = true;
		} else if (isOption(args[i], "--raw", "-R")) {
			fd.s_direct(true);
		} else if (isOptArg(args[i], "--count", "-c")) {
			if (!processOption(args, i, cnt))
				return 0;
		} else if (isOptArg(args[i], "--buffers", "-b")) {
			if (!processOption(args, i, v4l2_bufs))
				return 0;
		} else if (isOption(args[i], "--single-step", "-s")) {
			single_step = true;
			single_step_start = 1;
		} else if (isOptArg(args[i], "--single-step", "-s")) {
			if (!processOption(args, i, single_step_start))
				return 0;
			single_step = true;
		} else {
			printf("Invalid argument %s\n", args[i].toLatin1().data());
			return 0;
		}
	}
	if (info_option)
		return 0;

	if (mode == AppModeV4L2) {
		fps = 0;
		video_device = getDeviceName("/dev/video", video_device);
		if (fd.open(video_device.toLatin1().data(), true) < 0) {
			perror((QString("could not open ") + video_device).toLatin1().data());
			exit(1);
		}
		if (!fd.has_vid_cap()) {
			fprintf(stderr, "%s is not a video capture device\n", video_device.toLatin1().data());
			exit(1);
		}
		fd.g_fmt(fmt);

		if (overridePixelFormat) {
			fmt.s_pixelformat(overridePixelFormat);
			fd.s_fmt(fmt);
			fd.g_fmt(fmt);
			if (fmt.g_pixelformat() != overridePixelFormat) {
				fprintf(stderr, "Could not set format: '%s' %s\n",
					fcc2s(overridePixelFormat).c_str(),
					pixfmt2s(overridePixelFormat).c_str());
				fprintf(stderr, "Fall back to format: '%s' %s\n",
					fcc2s(fmt.g_pixelformat()).c_str(),
					pixfmt2s(fmt.g_pixelformat()).c_str());
			}
		}

		unsigned tmp_w, tmp_h;

		pixelaspect = fd.g_pixel_aspect(tmp_w, tmp_h);
	} else if (mode == AppModeSocket) {
		fps = 0;
		sock_fd = initSocket(port, fmt, pixelaspect);
	} else {
		fmt.s_type(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
		fmt.s_num_planes(1);
		fmt.s_pixelformat(V4L2_PIX_FMT_RGB24);
		fmt.s_width(640);
		fmt.s_height(360);
		fmt.s_field(V4L2_FIELD_NONE);
		fmt.s_colorspace(V4L2_COLORSPACE_SRGB);
		fmt.s_xfer_func(V4L2_XFER_FUNC_DEFAULT);
		fmt.s_ycbcr_enc(V4L2_YCBCR_ENC_DEFAULT);
		fmt.s_quantization(V4L2_QUANTIZATION_DEFAULT);
		fmt.s_bytesperline(3 * fmt.g_width());
		fmt.s_sizeimage(fmt.g_bytesperline() * fmt.g_height());
	}

	format.setDepthBufferSize(24);

	if (force_opengles)
		format.setRenderableType(QSurfaceFormat::OpenGLES);
	else if (force_opengl)
		format.setRenderableType(QSurfaceFormat::OpenGL);
	format.setProfile(QSurfaceFormat::CoreProfile);
	format.setVersion(3, 3);

	QSurfaceFormat::setDefaultFormat(format);
	CaptureGLWin win(sa);
	win.setVerbose(verbose);
	if (mode == AppModeFile) {
		win.setModeFile(filename);
		if (single_step_start)
			single_step_start--;
	} else if (mode == AppModeV4L2) {
		win.setModeV4L2(&fd);
	} else if (mode == AppModeTPG) {
		win.setModeTPG();
	}
	win.setOverrideWidth(overrideWidth);
	win.setOverrideHeight(overrideHeight);
	win.setFps(fps);
	win.setFormat(format);
	win.setReportTimings(report_timings);
	win.setCount(test ? test : cnt);
	if (mode == AppModeTest) {
		win.setModeTest(test);

		TestState state = { };

		state.fmt_idx = findVal(formats, overridePixelFormat);
		state.field_idx = findVal(fields, overrideField);
		state.colorspace_idx = findVal(colorspaces, overrideColorspace);
		state.xfer_func_idx = findVal(xfer_funcs, overrideXferFunc);
		state.ycbcr_enc_idx = findVal(ycbcr_encs, overrideYCbCrEnc);
		state.hsv_enc_idx = findVal(hsv_encs, overrideHSVEnc);
		state.quant_idx = findVal(quantizations, overrideQuantization);
		state.mask = test_mask;
		win.setTestState(state);
	}

	win.setOverridePixelFormat(overridePixelFormat);
	win.setOverrideField(overrideField);
	win.setOverrideColorspace(overrideColorspace);
	win.setOverrideYCbCrEnc(overrideYCbCrEnc);
	win.setOverrideHSVEnc(overrideHSVEnc);
	win.setOverrideXferFunc(overrideXferFunc);
	win.setOverrideQuantization(overrideQuantization);
	while (!win.setV4LFormat(fmt)) {
		fprintf(stderr, "Unsupported format: '%s' %s\n",
			fcc2s(fmt.g_pixelformat()).c_str(),
			pixfmt2s(fmt.g_pixelformat()).c_str());
		if (mode != AppModeSocket)
			exit(1);
		sock_fd = initSocket(port, fmt, pixelaspect);
	}
	win.setPixelAspect(pixelaspect);
	win.setMinimumSize(16, 16);
	win.setSizeIncrement(2, 2);
	win.resize(fmt.g_width(), fmt.g_frame_height());
	win.setFocusPolicy(Qt::StrongFocus);
	if (single_step && mode != AppModeTest)
		win.setSingleStepStart(single_step_start);

	sa->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
	sa->setWidget(win.window());
	sa->setFrameShape(QFrame::NoFrame);
	sa->resize(win.correctAspect(QSize(fmt.g_width(), fmt.g_frame_height())));
	sa->setWidgetResizable(true);

	if (mode == AppModeSocket)
		win.setModeSocket(sock_fd, port);
	else if (mode == AppModeV4L2) {
		cv4l_queue q(fd.g_type(), V4L2_MEMORY_MMAP);
		q.reqbufs(&fd, v4l2_bufs);
		q.obtain_bufs(&fd);
		q.queue_all(&fd);
		win.setQueue(&q);
		if (fd.streamon())
			exit(1);
	} else {
		struct tpg_data *tpg = win.getTPG();

		tpg_init(tpg, fmt.g_width(), fmt.g_height());
		tpg_alloc(tpg, fmt.g_width());
		tpg_s_pattern(tpg, (tpg_pattern)pattern);
		tpg_s_mv_hor_mode(tpg, hor_mode);
		tpg_s_mv_vert_mode(tpg, vert_mode);
		tpg_s_show_square(tpg, square);
		tpg_s_show_border(tpg, border);
		tpg_s_insert_sav(tpg, sav);
		tpg_s_insert_eav(tpg, eav);
		tpg_s_perc_fill(tpg, perc_fill);
		if (rgb_lim_range)
			tpg_s_real_rgb_range(tpg, V4L2_DV_RGB_RANGE_LIMITED);
		tpg_s_alpha_component(tpg, alpha);
		tpg_s_alpha_mode(tpg, alpha_red_only);
		tpg_s_video_aspect(tpg, video_aspect);
		switch (tpg_pixelaspect) {
		case -1:
			break;
		default:
			tpg_s_pixel_aspect(tpg, (tpg_pixel_aspect)tpg_pixelaspect);
			break;
		}
		win.startTimer();
	}
	sa->show();
	return disp.exec();
}
