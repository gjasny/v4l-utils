/*
    V4L2 API compliance test tool.

    Copyright (C) 2008  Hans Verkuil <hverkuil@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <unistd.h>
#include <features.h>		/* Uses _GNU_SOURCE to define getsubopt in stdlib.h */
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
#include <math.h>
#include <sys/klog.h>

#include <linux/videodev2.h>

#include <list>
#include <vector>
#include <map>
#include <string>

/* Short option list

   Please keep in alphabetical order.
   That makes it easier to see which short options are still free.

   In general the lower case is used to set something and the upper
   case is used to retrieve a setting. */
enum Option {
	OptSetDevice = 'd',
	OptGetDriverInfo = 'D',
	OptHelp = 'h',
	OptTest = 't',
	OptVerbose = 'v',
	OptLast = 256
};

enum Test {
	TestCap = 0,
	TestChipIdent,
	TestRegister,
	TestLogStatus,
	TestMax
};

static int test[TestMax];

static char options[OptLast];

static int app_result;
static int verbose;

static unsigned caps;

typedef std::vector<struct v4l2_ext_control> ctrl_list;
static ctrl_list user_ctrls;
static ctrl_list mpeg_ctrls;

typedef std::map<std::string, unsigned> ctrl_strmap;
static ctrl_strmap ctrl_str2id;
typedef std::map<unsigned, std::string> ctrl_idmap;
static ctrl_idmap ctrl_id2str;

typedef std::list<std::string> ctrl_get_list;
static ctrl_get_list get_ctrls;

typedef std::map<std::string,std::string> ctrl_set_map;
static ctrl_set_map set_ctrls;

typedef struct {
	unsigned flag;
	const char *str;
} flag_def;

static const flag_def service_def[] = {
	{ V4L2_SLICED_TELETEXT_B,  "teletext" },
	{ V4L2_SLICED_VPS,         "vps" },
	{ V4L2_SLICED_CAPTION_525, "cc" },
	{ V4L2_SLICED_WSS_625,     "wss" },
	{ 0, NULL }
};

/* fmts specified */
#define FmtWidth		(1L<<0)
#define FmtHeight		(1L<<1)
#define FmtChromaKey		(1L<<2)
#define FmtGlobalAlpha		(1L<<3)

/* crop specified */
#define CropWidth		(1L<<0)
#define CropHeight		(1L<<1)
#define CropLeft		(1L<<2)
#define CropTop 		(1L<<3)

static struct option long_options[] = {
	{"device", required_argument, 0, OptSetDevice},
	{"help", no_argument, 0, OptHelp},
	{"info", no_argument, 0, OptGetDriverInfo},
	{"verbose", no_argument, 0, OptVerbose},
	{"test", required_argument, 0, OptTest},
	{0, 0, 0, 0}
};

static void usage(void)
{
	printf("Usage:\n");
	printf("Common options:\n");
	printf("  -D, --info         show driver info [VIDIOC_QUERYCAP]\n");
	printf("  -d, --device=<dev> use device <dev> instead of /dev/video0\n");
	printf("                     if <dev> is a single digit, then /dev/video<dev> is used\n");
	printf("  -h, --help         display this help message\n");
	printf("  -t, --test=<num>   run specified test.\n");
	printf("                     By default all tests are run.\n");
	printf("                     0 = test VIDIOC_QUERYCAP\n");
	printf("  -v, --verbose      turn on verbose ioctl error reporting.\n");
	exit(0);
}

static std::string num2s(unsigned num)
{
	char buf[10];

	sprintf(buf, "%08x", num);
	return buf;
}

static std::string buftype2s(int type)
{
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		return "Video Capture";
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return "Video Output";
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		return "Video Overlay";
	case V4L2_BUF_TYPE_VBI_CAPTURE:
		return "VBI Capture";
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		return "VBI Output";
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
		return "Sliced VBI Capture";
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		return "Sliced VBI Output";
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		return "Video Output Overlay";
	case V4L2_BUF_TYPE_PRIVATE:
		return "Private";
	default:
		return "Unknown (" + num2s(type) + ")";
	}
}

static std::string fcc2s(unsigned int val)
{
	std::string s;

	s += val & 0xff;
	s += (val >> 8) & 0xff;
	s += (val >> 16) & 0xff;
	s += (val >> 24) & 0xff;
	return s;
}

static std::string field2s(int val)
{
	switch (val) {
	case V4L2_FIELD_ANY:
		return "Any";
	case V4L2_FIELD_NONE:
		return "None";
	case V4L2_FIELD_TOP:
		return "Top";
	case V4L2_FIELD_BOTTOM:
		return "Bottom";
	case V4L2_FIELD_INTERLACED:
		return "Interlaced";
	case V4L2_FIELD_SEQ_TB:
		return "Sequential Top-Bottom";
	case V4L2_FIELD_SEQ_BT:
		return "Sequential Bottom-Top";
	case V4L2_FIELD_ALTERNATE:
		return "Alternating";
	case V4L2_FIELD_INTERLACED_TB:
		return "Interlaced Top-Bottom";
	case V4L2_FIELD_INTERLACED_BT:
		return "Interlaced Bottom-Top";
	default:
		return "Unknown (" + num2s(val) + ")";
	}
}

static std::string colorspace2s(int val)
{
	switch (val) {
	case V4L2_COLORSPACE_SMPTE170M:
		return "Broadcast NTSC/PAL (SMPTE170M/ITU601)";
	case V4L2_COLORSPACE_SMPTE240M:
		return "1125-Line (US) HDTV (SMPTE240M)";
	case V4L2_COLORSPACE_REC709:
		return "HDTV and modern devices (ITU709)";
	case V4L2_COLORSPACE_BT878:
		return "Broken Bt878";
	case V4L2_COLORSPACE_470_SYSTEM_M:
		return "NTSC/M (ITU470/ITU601)";
	case V4L2_COLORSPACE_470_SYSTEM_BG:
		return "PAL/SECAM BG (ITU470/ITU601)";
	case V4L2_COLORSPACE_JPEG:
		return "JPEG (JFIF/ITU601)";
	case V4L2_COLORSPACE_SRGB:
		return "SRGB";
	default:
		return "Unknown (" + num2s(val) + ")";
	}
}

static std::string flags2s(unsigned val, const flag_def *def)
{
	std::string s;

	while (def->flag) {
		if (val & def->flag) {
			if (s.length()) s += " ";
			s += def->str;
		}
		def++;
	}
	return s;
}

static void print_sliced_vbi_cap(struct v4l2_sliced_vbi_cap &cap)
{
	printf("\tType           : %s\n", buftype2s(cap.type).c_str());
	printf("\tService Set    : %s\n",
			flags2s(cap.service_set, service_def).c_str());
	for (int i = 0; i < 24; i++) {
		printf("\tService Line %2d: %8s / %-8s\n", i,
				flags2s(cap.service_lines[0][i], service_def).c_str(),
				flags2s(cap.service_lines[1][i], service_def).c_str());
	}
}

static std::string name2var(unsigned char *name)
{
	std::string s;

	while (*name) {
		if (*name == ' ') s += "_";
		else s += std::string(1, tolower(*name));
		name++;
	}
	return s;
}

static void print_qctrl(int fd, struct v4l2_queryctrl *queryctrl,
		struct v4l2_ext_control *ctrl, int show_menus)
{
	struct v4l2_querymenu qmenu = { 0 };
	std::string s = name2var(queryctrl->name);
	int i;

	qmenu.id = queryctrl->id;
	switch (queryctrl->type) {
	case V4L2_CTRL_TYPE_INTEGER:
		printf("%31s (int)  : min=%d max=%d step=%d default=%d value=%d",
				s.c_str(),
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->step, queryctrl->default_value,
				ctrl->value);
		break;
	case V4L2_CTRL_TYPE_INTEGER64:
		printf("%31s (int64): value=%lld", s.c_str(), ctrl->value64);
		break;
	case V4L2_CTRL_TYPE_BOOLEAN:
		printf("%31s (bool) : default=%d value=%d",
				s.c_str(),
				queryctrl->default_value, ctrl->value);
		break;
	case V4L2_CTRL_TYPE_MENU:
		printf("%31s (menu) : min=%d max=%d default=%d value=%d",
				s.c_str(),
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->default_value, ctrl->value);
		break;
	case V4L2_CTRL_TYPE_BUTTON:
		printf("%31s (button)\n", s.c_str());
		break;
	default: break;
	}
	if (queryctrl->flags) {
		const flag_def def[] = {
			{ V4L2_CTRL_FLAG_GRABBED,   "grabbed" },
			{ V4L2_CTRL_FLAG_READ_ONLY, "readonly" },
			{ V4L2_CTRL_FLAG_UPDATE,    "update" },
			{ V4L2_CTRL_FLAG_INACTIVE,  "inactive" },
			{ V4L2_CTRL_FLAG_SLIDER,    "slider" },
			{ 0, NULL }
		};
		printf(" flags=%s", flags2s(queryctrl->flags, def).c_str());
	}
	printf("\n");
	if (queryctrl->type == V4L2_CTRL_TYPE_MENU && show_menus) {
		for (i = 0; i <= queryctrl->maximum; i++) {
			qmenu.index = i;
			if (ioctl(fd, VIDIOC_QUERYMENU, &qmenu))
				continue;
			printf("\t\t\t\t%d: %s\n", i, qmenu.name);
		}
	}
}

static int print_control(int fd, struct v4l2_queryctrl &qctrl, int show_menus)
{
	struct v4l2_control ctrl = { 0 };
	struct v4l2_ext_control ext_ctrl = { 0 };
	struct v4l2_ext_controls ctrls = { 0 };

	if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED)
		return 1;
	if (qctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS) {
		printf("\n%s\n\n", qctrl.name);
		return 1;
	}
	ext_ctrl.id = qctrl.id;
	ctrls.ctrl_class = V4L2_CTRL_ID2CLASS(qctrl.id);
	ctrls.count = 1;
	ctrls.controls = &ext_ctrl;
	if (V4L2_CTRL_ID2CLASS(qctrl.id) != V4L2_CTRL_CLASS_USER &&
	    qctrl.id < V4L2_CID_PRIVATE_BASE) {
		if (ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls)) {
			printf("error %d getting ext_ctrl %s\n",
					errno, qctrl.name);
			return 0;
		}
	}
	else {
		ctrl.id = qctrl.id;
		if (ioctl(fd, VIDIOC_G_CTRL, &ctrl)) {
			printf("error %d getting ctrl %s\n",
					errno, qctrl.name);
			return 0;
		}
		ext_ctrl.value = ctrl.value;
	}
	print_qctrl(fd, &qctrl, &ext_ctrl, show_menus);
	return 1;
}

static void list_controls(int fd, int show_menus)
{
	struct v4l2_queryctrl qctrl = { V4L2_CTRL_FLAG_NEXT_CTRL };
	int id;

	while (ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
			print_control(fd, qctrl, show_menus);
		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}
	if (qctrl.id != V4L2_CTRL_FLAG_NEXT_CTRL)
		return;
	for (id = V4L2_CID_USER_BASE; id < V4L2_CID_LASTP1; id++) {
		qctrl.id = id;
		if (ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0)
			print_control(fd, qctrl, show_menus);
	}
	for (qctrl.id = V4L2_CID_PRIVATE_BASE;
			ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0; qctrl.id++) {
		print_control(fd, qctrl, show_menus);
	}
}

static void find_controls(int fd)
{
	struct v4l2_queryctrl qctrl = { V4L2_CTRL_FLAG_NEXT_CTRL };
	int id;

	while (ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
		if (qctrl.type != V4L2_CTRL_TYPE_CTRL_CLASS &&
		    !(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
			ctrl_str2id[name2var(qctrl.name)] = qctrl.id;
			ctrl_id2str[qctrl.id] = name2var(qctrl.name);
		}
		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}
	if (qctrl.id != V4L2_CTRL_FLAG_NEXT_CTRL)
		return;
	for (id = V4L2_CID_USER_BASE; id < V4L2_CID_LASTP1; id++) {
		qctrl.id = id;
		if (ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0 &&
		    !(qctrl.flags & V4L2_CTRL_FLAG_DISABLED))
			ctrl_str2id[name2var(qctrl.name)] = qctrl.id;
	}
	for (qctrl.id = V4L2_CID_PRIVATE_BASE;
			ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0; qctrl.id++) {
		if (!(qctrl.flags & V4L2_CTRL_FLAG_DISABLED))
			ctrl_str2id[name2var(qctrl.name)] = qctrl.id;
	}
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
	printf("\tPixel Format  : %s\n", fcc2s(fb.fmt.pixelformat).c_str());
	if (!is_ext) {
		printf("\tBytes per Line: %d\n", fb.fmt.bytesperline);
		printf("\tSize image    : %d\n", fb.fmt.sizeimage);
		printf("\tColorspace    : %s\n", colorspace2s(fb.fmt.colorspace).c_str());
		if (fb.fmt.priv)
			printf("\tCustom Info   : %08x\n", fb.fmt.priv);
	}
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

static void printfmt(struct v4l2_format vfmt)
{
	const flag_def vbi_def[] = {
		{ V4L2_VBI_UNSYNC,     "unsynchronized" },
		{ V4L2_VBI_INTERLACED, "interlaced" },
		{ 0, NULL }
	};
	printf("Format %s:\n", buftype2s(vfmt.type).c_str());

	switch (vfmt.type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		printf("\tWidth/Height  : %u/%u\n", vfmt.fmt.pix.width, vfmt.fmt.pix.height);
		printf("\tPixel Format  : %s\n", fcc2s(vfmt.fmt.pix.pixelformat).c_str());
		printf("\tField         : %s\n", field2s(vfmt.fmt.pix.field).c_str());
		printf("\tBytes per Line: %u\n", vfmt.fmt.pix.bytesperline);
		printf("\tSize Image    : %u\n", vfmt.fmt.pix.sizeimage);
		printf("\tColorspace    : %s\n", colorspace2s(vfmt.fmt.pix.colorspace).c_str());
		if (vfmt.fmt.pix.priv)
			printf("\tCustom Info   : %08x\n", vfmt.fmt.pix.priv);
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		printf("\tLeft/Top    : %d/%d\n",
				vfmt.fmt.win.w.left, vfmt.fmt.win.w.top);
		printf("\tWidth/Height: %d/%d\n",
				vfmt.fmt.win.w.width, vfmt.fmt.win.w.height);
		printf("\tField       : %s\n", field2s(vfmt.fmt.win.field).c_str());
		printf("\tChroma Key  : 0x%08x\n", vfmt.fmt.win.chromakey);
		printf("\tGlobal Alpha: 0x%02x\n", vfmt.fmt.win.global_alpha);
		printf("\tClip Count  : %u\n", vfmt.fmt.win.clipcount);
		printf("\tClip Bitmap : %s\n", vfmt.fmt.win.bitmap ? "Yes" : "No");
		break;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		printf("\tSampling Rate   : %u Hz\n", vfmt.fmt.vbi.sampling_rate);
		printf("\tOffset          : %u samples (%g secs after leading edge)\n",
				vfmt.fmt.vbi.offset,
				(double)vfmt.fmt.vbi.offset / (double)vfmt.fmt.vbi.sampling_rate);
		printf("\tSamples per Line: %u\n", vfmt.fmt.vbi.samples_per_line);
		printf("\tSample Format   : %s\n", fcc2s(vfmt.fmt.vbi.sample_format).c_str());
		printf("\tStart 1st Field : %u\n", vfmt.fmt.vbi.start[0]);
		printf("\tCount 1st Field : %u\n", vfmt.fmt.vbi.count[0]);
		printf("\tStart 2nd Field : %u\n", vfmt.fmt.vbi.start[1]);
		printf("\tCount 2nd Field : %u\n", vfmt.fmt.vbi.count[1]);
		if (vfmt.fmt.vbi.flags)
			printf("\tFlags           : %s\n", flags2s(vfmt.fmt.vbi.flags, vbi_def).c_str());
		break;
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		printf("\tService Set    : %s\n",
				flags2s(vfmt.fmt.sliced.service_set, service_def).c_str());
		for (int i = 0; i < 24; i++) {
			printf("\tService Line %2d: %8s / %-8s\n", i,
			       flags2s(vfmt.fmt.sliced.service_lines[0][i], service_def).c_str(),
			       flags2s(vfmt.fmt.sliced.service_lines[1][i], service_def).c_str());
		}
		printf("\tI/O Size       : %u\n", vfmt.fmt.sliced.io_size);
		break;
	case V4L2_BUF_TYPE_PRIVATE:
		break;
	}
}

static void print_video_formats(int fd, enum v4l2_buf_type type)
{
	struct v4l2_fmtdesc fmt;

	fmt.index = 0;
	fmt.type = type;
	while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) >= 0) {
		printf("\tType        : %s\n", buftype2s(type).c_str());
		printf("\tPixelformat : %s", fcc2s(fmt.pixelformat).c_str());
		if (fmt.flags)
			printf(" (compressed)");
		printf("\n");
		printf("\tName        : %s\n", fmt.description);
		printf("\n");
		fmt.index++;
	}
}

static char *pts_to_string(char *str, unsigned long pts)
{
	static char buf[256];
	int hours, minutes, seconds, fracsec;
	float fps;
	int frame;
	char *p = (str) ? str : buf;

	static const int MPEG_CLOCK_FREQ = 90000;
	seconds = pts / MPEG_CLOCK_FREQ;
	fracsec = pts % MPEG_CLOCK_FREQ;

	minutes = seconds / 60;
	seconds = seconds % 60;

	hours = minutes / 60;
	minutes = minutes % 60;

	fps = 30;
	frame = (int)ceilf(((float)fracsec / (float)MPEG_CLOCK_FREQ) * fps);

	snprintf(p, sizeof(buf), "%d:%02d:%02d:%d", hours, minutes, seconds,
		 frame);
	return p;
}

static const char *audmode2s(int audmode)
{
	switch (audmode) {
		case V4L2_TUNER_MODE_STEREO: return "stereo";
		case V4L2_TUNER_MODE_LANG1: return "lang1";
		case V4L2_TUNER_MODE_LANG2: return "lang2";
		case V4L2_TUNER_MODE_LANG1_LANG2: return "bilingual";
		case V4L2_TUNER_MODE_MONO: return "mono";
		default: return "unknown";
	}
}

static std::string rxsubchans2s(int rxsubchans)
{
	std::string s;

	if (rxsubchans & V4L2_TUNER_SUB_MONO)
		s += "mono ";
	if (rxsubchans & V4L2_TUNER_SUB_STEREO)
		s += "stereo ";
	if (rxsubchans & V4L2_TUNER_SUB_LANG1)
		s += "lang1 ";
	if (rxsubchans & V4L2_TUNER_SUB_LANG2)
		s += "lang2 ";
	return s;
}

static std::string tcap2s(unsigned cap)
{
	std::string s;

	if (cap & V4L2_TUNER_CAP_LOW)
		s += "62.5 Hz ";
	else
		s += "62.5 kHz ";
	if (cap & V4L2_TUNER_CAP_NORM)
		s += "multi-standard ";
	if (cap & V4L2_TUNER_CAP_STEREO)
		s += "stereo ";
	if (cap & V4L2_TUNER_CAP_LANG1)
		s += "lang1 ";
	if (cap & V4L2_TUNER_CAP_LANG2)
		s += "lang2 ";
	return s;
}

static std::string cap2s(unsigned cap)
{
	std::string s;

	if (cap & V4L2_CAP_VIDEO_CAPTURE)
		s += "\t\tVideo Capture\n";
	if (cap & V4L2_CAP_VIDEO_OUTPUT)
		s += "\t\tVideo Output\n";
	if (cap & V4L2_CAP_VIDEO_OVERLAY)
		s += "\t\tVideo Overlay\n";
	if (cap & V4L2_CAP_VIDEO_OUTPUT_OVERLAY)
		s += "\t\tVideo Output Overlay\n";
	if (cap & V4L2_CAP_VBI_CAPTURE)
		s += "\t\tVBI Capture\n";
	if (cap & V4L2_CAP_VBI_OUTPUT)
		s += "\t\tVBI Output\n";
	if (cap & V4L2_CAP_SLICED_VBI_CAPTURE)
		s += "\t\tSliced VBI Capture\n";
	if (cap & V4L2_CAP_SLICED_VBI_OUTPUT)
		s += "\t\tSliced VBI Output\n";
	if (cap & V4L2_CAP_RDS_CAPTURE)
		s += "\t\tRDS Capture\n";
	if (cap & V4L2_CAP_TUNER)
		s += "\t\tTuner\n";
	if (cap & V4L2_CAP_AUDIO)
		s += "\t\tAudio\n";
	if (cap & V4L2_CAP_RADIO)
		s += "\t\tRadio\n";
	if (cap & V4L2_CAP_READWRITE)
		s += "\t\tRead/Write\n";
	if (cap & V4L2_CAP_ASYNCIO)
		s += "\t\tAsync I/O\n";
	if (cap & V4L2_CAP_STREAMING)
		s += "\t\tStreaming\n";
	return s;
}

static v4l2_std_id parse_pal(const char *pal)
{
	if (pal[0] == '-') {
		switch (pal[1]) {
			case '6':
				return V4L2_STD_PAL_60;
			case 'b':
			case 'B':
			case 'g':
			case 'G':
				return V4L2_STD_PAL_BG;
			case 'h':
			case 'H':
				return V4L2_STD_PAL_H;
			case 'n':
			case 'N':
				if (pal[2] == 'c' || pal[2] == 'C')
					return V4L2_STD_PAL_Nc;
				return V4L2_STD_PAL_N;
			case 'i':
			case 'I':
				return V4L2_STD_PAL_I;
			case 'd':
			case 'D':
			case 'k':
			case 'K':
				return V4L2_STD_PAL_DK;
			case 'M':
			case 'm':
				return V4L2_STD_PAL_M;
			case '-':
				break;
		}
	}
	fprintf(stderr, "pal specifier not recognised\n");
	return 0;
}

static v4l2_std_id parse_secam(const char *secam)
{
	if (secam[0] == '-') {
		switch (secam[1]) {
			case 'b':
			case 'B':
			case 'g':
			case 'G':
			case 'h':
			case 'H':
				return V4L2_STD_SECAM_B | V4L2_STD_SECAM_G | V4L2_STD_SECAM_H;
			case 'd':
			case 'D':
			case 'k':
			case 'K':
				return V4L2_STD_SECAM_DK;
			case 'l':
			case 'L':
				if (secam[2] == 'C' || secam[2] == 'c')
					return V4L2_STD_SECAM_LC;
				return V4L2_STD_SECAM_L;
			case '-':
				break;
		}
	}
	fprintf(stderr, "secam specifier not recognised\n");
	return 0;
}

static v4l2_std_id parse_ntsc(const char *ntsc)
{
	if (ntsc[0] == '-') {
		switch (ntsc[1]) {
			case 'm':
			case 'M':
				return V4L2_STD_NTSC_M;
			case 'j':
			case 'J':
				return V4L2_STD_NTSC_M_JP;
			case 'k':
			case 'K':
				return V4L2_STD_NTSC_M_KR;
			case '-':
				break;
		}
	}
	fprintf(stderr, "ntsc specifier not recognised\n");
	return 0;
}

static int doioctl(int fd, unsigned long int request, void *parm, const char *name)
{
	int retVal;
	int e;

	errno = 0;
	retVal = ioctl(fd, request, parm);
	e = errno;
	if (verbose)
		printf("\t\t%s returned %d (%s)\n", name, retVal, strerror(e));
	if (retVal == 0) return retVal;
	if (retVal != -1) {
		return -1;
	}
	retVal = e;
	return retVal;
}

static int parse_subopt(char **subs, char * const *subopts, char **value)
{
	int opt = getsubopt(subs, subopts, value);

	if (opt == -1) {
		fprintf(stderr, "Invalid suboptions specified\n");
		usage();
		exit(1);
	}
	if (value == NULL) {
		fprintf(stderr, "No value given to suboption <%s>\n",
				subopts[opt]);
		usage();
		exit(1);
	}
	return opt;
}

static void parse_next_subopt(char **subs, char **value)
{
	static char *const subopts[] = {
	    NULL
	};
	int opt = getsubopt(subs, subopts, value);

	if (value == NULL) {
		fprintf(stderr, "No value given to suboption <%s>\n",
				subopts[opt]);
		usage();
		exit(1);
	}
}

static void print_std(const char *prefix, const char *stds[], unsigned long long std)
{
	int first = 1;

	printf("\t%s-", prefix);
	while (*stds) {
		if (std & 1) {
			if (!first)
				printf("/");
			first = 0;
			printf("%s", *stds);
		}
		stds++;
		std >>= 1;
	}
	printf("\n");
}

static const char *ok(int res)
{
	if (res)
		app_result = res;
	return res ? "FAIL" : "OK";
}

static int check_string(const char *s, size_t len, const char *fld)
{
	if (strlen(s) == 0) {
		if (verbose)
			printf("%s field empty\n", fld);
		return -1;
	}
	if (strlen(s) >= len) {
		if (verbose)
			printf("%s field not 0-terminated\n", fld);
		return -1;
	}
	return 0;
}

static int check_ustring(const __u8 *s, int len, const char *fld)
{
	return check_string((const char *)s, len, fld);
}

static int check_0(void *p, int len)
{
	__u8 *q = (__u8 *)p;

	while (len--)
		if (*q++) {
			if (verbose)
				printf("array not zeroed by driver\n");
			return -1;
		}
	return 0;
}

static int testCap(int fd)
{
	struct v4l2_capability vcap;
	__u32 caps;

	if (doioctl(fd, VIDIOC_QUERYCAP, &vcap, "VIDIOC_QUERYCAP"))
		return -1;
	if (check_ustring(vcap.driver, sizeof(vcap.driver), "driver"))
		return -1;
	if (check_ustring(vcap.card, sizeof(vcap.card), "card"))
		return -1;
	if (check_ustring(vcap.bus_info, sizeof(vcap.bus_info), "bus_info"))
		return -1;
	if (check_0(vcap.reserved, sizeof(vcap.reserved)))
		return -1;
	caps = vcap.capabilities;
	if (caps == 0) {
		if (verbose) printf("no capabilities set\n");
		return -1;
	}
	return 0;
}

static int testChipIdent(int fd)
{
	struct v4l2_dbg_chip_ident chip;
	int ret;

	memset(&chip, 0, sizeof(chip));
	chip.match.type = V4L2_CHIP_MATCH_HOST;
	chip.match.addr = 0;
	ret = doioctl(fd, VIDIOC_DBG_G_CHIP_IDENT, &chip, "VIDIOC_DBG_G_CHIP_IDENT");
	// Must return either 0 (OK) or EINVAL (not supported)
	if (ret == 0) {
		struct v4l2_dbg_chip_ident orig;

		memset(&orig, 0, sizeof(orig));
		// set invalid match_type
		chip.match.type = V4L2_CHIP_MATCH_I2C_ADDR + 1;
		chip.match.addr = 0xdeadbeef;
		chip.ident = 0xdeadbeef;
		chip.revision = 0xdeadbeef;
		orig = chip;
		ret = doioctl(fd, VIDIOC_DBG_G_CHIP_IDENT, &chip, "VIDIOC_DBG_G_CHIP_IDENT");
		if (ret != EINVAL) {
			if (verbose)
				printf("Invalid match_type accepted\n");
			return -1;
		}
		if (memcmp(&orig, &chip, sizeof(chip))) {
			if (verbose)
				printf("Error, but struct modified\n");
			return -1;
		}
		return 0;
	}
	return ret != EINVAL;
}

static int testRegister(int fd)
{
	struct v4l2_dbg_register reg;
	struct v4l2_dbg_chip_ident chip;
	int ret;
	int uid = getuid();

	reg.match.type = V4L2_CHIP_MATCH_HOST;
	reg.match.addr = 0;
	reg.reg = 0;
	ret = doioctl(fd, VIDIOC_DBG_G_REGISTER, &reg, "VIDIOC_DBG_G_REGISTER");
	if (ret == EINVAL)
		return 0;
	if (uid && ret != EPERM) {
		printf("Not allowed to call VIDIOC_DBG_G_REGISTER unless root\n");
		return -1;
	}
	if (uid == 0 && ret) {
		printf("Not allowed to call VIDIOC_DBG_G_REGISTER even though we are root\n");
		return -1;
	}
	chip.match.type = V4L2_CHIP_MATCH_HOST;
	chip.match.addr = 0;
	if (doioctl(fd, VIDIOC_DBG_G_CHIP_IDENT, &chip, "VIDIOC_DBG_G_CHIP_IDENT")) {
		printf("Must support VIDIOC_DBG_G_CHIP_IDENT\n");
		return -1;
	}
	if (uid) {
		// Don't test S_REGISTER as root, don't want to risk
		// messing with registers in the compliance test.
		reg.reg = reg.val = 0;
		ret = doioctl(fd, VIDIOC_DBG_S_REGISTER, &reg, "VIDIOC_DBG_S_REGISTER");
		if (ret != EINVAL && ret != EPERM) {
			printf("Invalid error calling VIDIOC_DBG_S_REGISTER as non-root\n");
			return -1;
		}
	}
	return 0;
}

static int testLogStatus(int fd)
{
	int ret = doioctl(fd, VIDIOC_LOG_STATUS, NULL, "VIDIOC_LOG_STATUS");

	return (ret == 0 || ret == EINVAL) ? 0 : -1;
}

int main(int argc, char **argv)
{
	char *value, *subs;
	int i;
	unsigned t;
	int fd = -1;

	/* command args */
	int ch;
	const char *device = "/dev/video0";	/* -d device */
	struct v4l2_capability vcap;	/* list_cap */
	char short_options[26 * 2 * 2 + 1];
	int idx = 0;
	int tests = 0;

	for (i = 0; long_options[i].name; i++) {
		if (!isalpha(long_options[i].val))
			continue;
		short_options[idx++] = long_options[i].val;
		if (long_options[i].has_arg == required_argument)
			short_options[idx++] = ':';
	}
	while (1) {
		int option_index = 0;

		short_options[idx] = 0;
		ch = getopt_long(argc, argv, short_options,
				 long_options, &option_index);
		if (ch == -1)
			break;

		options[(int)ch] = 1;
		switch (ch) {
		case OptHelp:
			usage();
			return 0;
		case OptTest:
			t = strtoul(optarg, NULL, 0);

			if (t >= TestMax)
				usage();
			test[t] = 1;
			tests++;
			break;
		case OptSetDevice:
			device = optarg;
			if (device[0] >= '0' && device[0] <= '9' && device[1] == 0) {
				static char newdev[20];
				char dev = device[0];

				sprintf(newdev, "/dev/video%c", dev);
				device = newdev;
			}
			break;
		case ':':
			fprintf(stderr, "Option `%s' requires a value\n",
				argv[optind]);
			usage();
			return 1;
		case '?':
			fprintf(stderr, "Unknown argument `%s'\n",
				argv[optind]);
			usage();
			return 1;
		}
	}
	if (optind < argc) {
		printf("unknown arguments: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
		usage();
		return 1;
	}
	verbose = options[OptVerbose];
	if (!tests) {
		for (t = 0; t < TestMax; t++)
			test[t] = 1;
	}

	if ((fd = open(device, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", device,
			strerror(errno));
		exit(1);
	}

	ioctl(fd, VIDIOC_QUERYCAP, &vcap, "VIDIOC_QUERYCAP");
	caps = vcap.capabilities;
	find_controls(fd);
	for (ctrl_get_list::iterator iter = get_ctrls.begin(); iter != get_ctrls.end(); ++iter) {
	    if (ctrl_str2id.find(*iter) == ctrl_str2id.end()) {
		fprintf(stderr, "unknown control '%s'\n", (*iter).c_str());
		exit(1);
	    }
	}
	for (ctrl_set_map::iterator iter = set_ctrls.begin(); iter != set_ctrls.end(); ++iter) {
	    if (ctrl_str2id.find(iter->first) == ctrl_str2id.end()) {
		fprintf(stderr, "unknown control '%s'\n", iter->first.c_str());
		exit(1);
	    }
	}

	/* Information Opts */

	if (options[OptGetDriverInfo]) {
		printf("Driver Info:\n");
		printf("\tDriver name   : %s\n", vcap.driver);
		printf("\tCard type     : %s\n", vcap.card);
		printf("\tBus info      : %s\n", vcap.bus_info);
		printf("\tDriver version: %d\n", vcap.version);
		printf("\tCapabilities  : 0x%08X\n", vcap.capabilities);
		printf("%s", cap2s(vcap.capabilities).c_str());
	}

	printf("Compliance test for device %s:\n\n", device);

	printf("Required ioctls:\n");
	if (test[TestCap])
		printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(fd)));

	printf("Debug ioctls:\n");
	if (test[TestChipIdent])
		printf("\ttest VIDIOC_DBG_G_CHIP_IDENT: %s\n", ok(testChipIdent(fd)));
	if (test[TestRegister])
		printf("\ttest VIDIOC_DBG_G/S_REGISTER: %s\n", ok(testRegister(fd)));
	if (test[TestLogStatus])
		printf("\ttest VIDIOC_LOG_STATUS: %s\n", ok(testLogStatus(fd)));

	close(fd);
	exit(app_result);
}
