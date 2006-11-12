/*
    Copyright (C) 2003-2004  Kevin Thayer <nufan_wfk at yahoo dot com>

    Cleanup and VBI and audio in/out options:
    Copyright (C) 2004  Hans Verkuil <hverkuil@xs4all.nl>

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
	OptGetSlicedVbiFormat = 'B',
	OptSetSlicedVbiFormat = 'b',
	OptGetCtrl = 'C',
	OptSetCtrl = 'c',
	OptSetDevice = 'd',
	OptGetDriverInfo = 'D',
	OptGetFreq = 'F',
	OptSetFreq = 'f',
	OptHelp = 'h',
	OptGetInput = 'I',
	OptSetInput = 'i',
	OptListCtrls = 'l',
	OptListCtrlsMenus = 'L',
	OptListOutputs = 'N',
	OptListInputs = 'n',
	OptGetOutput = 'O',
	OptSetOutput = 'o',
	OptGetStandard = 'S',
	OptSetStandard = 's',
	OptGetTuner = 'T',
	OptSetTuner = 't',
	OptGetVideoFormat = 'V',
	OptSetVideoFormat = 'v',

	OptGetSlicedVbiOutFormat = 128,
	OptSetSlicedVbiOutFormat,
	OptGetOverlayFormat,
	//OptSetOverlayFormat, TODO
	OptGetVbiFormat,
	//OptSetVbiFormat, TODO
	OptGetVbiOutFormat,
	//OptSetVbiOutFormat, TODO
	OptAll,
	OptStreamOff,
	OptStreamOn,
	OptListStandards,
	OptListFormats,
	OptLogStatus,
	OptVerbose,
	OptGetVideoOutFormat,
	OptSetVideoOutFormat,
	OptGetSlicedVbiCap,
	OptGetSlicedVbiOutCap,
	OptGetVideoCrop,
	OptSetVideoCrop,
	OptGetAudioInput,
	OptSetAudioInput,
	OptGetAudioOutput,
	OptSetAudioOutput,
	OptListAudioOutputs,
	OptListAudioInputs,
	OptLast = 256
};

static char options[OptLast];

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
#define FMTWidth		(1L<<0)
#define FMTHeight		(1L<<1)

/* crop specified */
#define CropWidth		(1L<<0)
#define CropHeight		(1L<<1)
#define CropLeft		(1L<<2)
#define CropTop 		(1L<<3)

static struct option long_options[] = {
	{"list-audio-inputs", no_argument, 0, OptListAudioInputs},
	{"list-audio-outputs", no_argument, 0, OptListAudioOutputs},
	{"all", no_argument, 0, OptAll},
	{"device", required_argument, 0, OptSetDevice},
	{"get-fmt-video", no_argument, 0, OptGetVideoFormat},
	{"set-fmt-video", required_argument, 0, OptSetVideoFormat},
	{"get-fmt-video-out", no_argument, 0, OptGetVideoOutFormat},
	{"set-fmt-video-out", required_argument, 0, OptSetVideoOutFormat},
	{"help", no_argument, 0, OptHelp},
	{"get-output", no_argument, 0, OptGetOutput},
	{"set-output", required_argument, 0, OptSetOutput},
	{"list-outputs", no_argument, 0, OptListOutputs},
	{"list-inputs", no_argument, 0, OptListInputs},
	{"get-input", no_argument, 0, OptGetInput},
	{"set-input", required_argument, 0, OptSetInput},
	{"get-audio-input", no_argument, 0, OptGetAudioInput},
	{"set-audio-input", required_argument, 0, OptSetAudioInput},
	{"get-audio-output", no_argument, 0, OptGetAudioOutput},
	{"set-audio-output", required_argument, 0, OptSetAudioOutput},
	{"get-freq", no_argument, 0, OptGetFreq},
	{"set-freq", required_argument, 0, OptSetFreq},
	{"streamoff", no_argument, 0, OptStreamOff},
	{"streamon", no_argument, 0, OptStreamOn},
	{"list-standards", no_argument, 0, OptListStandards},
	{"list-formats", no_argument, 0, OptListFormats},
	{"get-standard", no_argument, 0, OptGetStandard},
	{"set-standard", required_argument, 0, OptSetStandard},
	{"info", no_argument, 0, OptGetDriverInfo},
	{"list-ctrls", no_argument, 0, OptListCtrls},
	{"list-ctrls-menus", no_argument, 0, OptListCtrlsMenus},
	{"set-ctrl", required_argument, 0, OptSetCtrl},
	{"get-ctrl", required_argument, 0, OptGetCtrl},
	{"get-tuner", no_argument, 0, OptGetTuner},
	{"set-tuner", required_argument, 0, OptSetTuner},
	{"verbose", no_argument, 0, OptVerbose},
	{"log-status", no_argument, 0, OptLogStatus},
	{"get-fmt-overlay", no_argument, 0, OptGetOverlayFormat},
	{"get-fmt-sliced-vbi", no_argument, 0, OptGetSlicedVbiFormat},
	{"set-fmt-sliced-vbi", required_argument, 0, OptSetSlicedVbiFormat},
	{"get-fmt-sliced-vbi-out", no_argument, 0, OptGetSlicedVbiOutFormat},
	{"set-fmt-sliced-vbi-out", required_argument, 0, OptSetSlicedVbiOutFormat},
	{"get-fmt-vbi", no_argument, 0, OptGetVbiFormat},
	{"get-fmt-vbi-out", no_argument, 0, OptGetVbiOutFormat},
	{"get-sliced-vbi-cap", no_argument, 0, OptGetSlicedVbiCap},
	{"get-sliced-vbi-out-cap", no_argument, 0, OptGetSlicedVbiOutCap},
	{"get-crop-video", no_argument, 0, OptGetVideoCrop},
	{"set-crop-video", required_argument, 0, OptSetVideoCrop},
	{0, 0, 0, 0}
};

static void usage(void)
{
	printf("Usage:\n");
	printf("Common options:\n");
	printf("  --all              display all information available\n");
	printf("  -B, --get-fmt-sliced-vbi\n");
	printf("		     query the sliced VBI capture format [VIDIOC_G_FMT]\n");
	printf("  -b, --set-fmt-sliced-vbi=<mode>\n");
	printf("                     set the sliced VBI capture format to <mode> [VIDIOC_S_FMT]\n");
	printf("                     <mode> is a comma separated list of:\n");
	printf("                     off:      turn off sliced VBI (cannot be combined with other modes)\n");
	printf("                     teletext: teletext (PAL/SECAM)\n");
	printf("                     cc:       closed caption (NTSC)\n");
	printf("                     wss:      widescreen signal (PAL/SECAM)\n");
	printf("                     vps:      VPS (PAL/SECAM)\n");
	printf("  -C, --get-ctrl=<ctrl>[,<ctrl>...]\n");
	printf("                     get the value of the controls [VIDIOC_G_EXT_CTRLS]\n");
	printf("  -c, --set-ctrl=<ctrl>=<val>[,<ctrl>=<val>...]\n");
	printf("                     set the controls to the values specified [VIDIOC_S_EXT_CTRLS]\n");
	printf("  -D, --info         show driver info [VIDIOC_QUERYCAP]\n");
	printf("  -d, --device=<dev> use device <dev> instead of /dev/video0\n");
	printf("                     if <dev> is a single digit, then /dev/video<dev> is used\n");
	printf("  -F, --get-freq     query the frequency [VIDIOC_G_FREQUENCY]\n");
	printf("  -f, --set-freq=<freq>\n");
	printf("                     set the frequency to <freq> MHz [VIDIOC_S_FREQUENCY]\n");
	printf("  -h, --help         display this help message\n");
	printf("  -I, --get-input    query the video input [VIDIOC_G_INPUT]\n");
	printf("  -i, --set-input=<num>\n");
	printf("                     set the video input to <num> [VIDIOC_S_INPUT]\n");
	printf("  -l, --list-ctrls   display all controls and their values [VIDIOC_QUERYCTRL]\n");
	printf("  -L, --list-ctrls-menus\n");
	printf("		     display all controls, their values and the menus [VIDIOC_QUERYMENU]\n");
	printf("  -N, --list-outputs display video outputs [VIDIOC_ENUMOUTPUT]\n");
	printf("  -n, --list-inputs  display video inputs [VIDIOC_ENUMINPUT]\n");
	printf("  -O, --get-output   query the video output [VIDIOC_G_OUTPUT]\n");
	printf("  -o, --set-output=<num>\n");
	printf("                     set the video output to <num> [VIDIOC_S_OUTPUT]\n");
	printf("  -Q, --list-audio-outputs\n");
	printf("                     display audio outputs [VIDIOC_ENUMAUDOUT]\n");
	printf("  -q, --list-audio-inputs\n");
	printf("                     display audio inputs [VIDIOC_ENUMAUDIO]\n");
	printf("  -S, --get-standard\n");
	printf("                     query the video standard [VIDIOC_G_STD]\n");
	printf("  -s, --set-standard=<num>\n");
	printf("                     set the video standard to <num> [VIDIOC_S_STD]\n");
	printf("                     <num> can be a numerical v4l2_std value, or it can be one of:\n");
	printf("                     pal-X (X = B/G/H/N/Nc/I/D/K/M) or just 'pal' (V4L2_STD_PAL)\n");
	printf("                     ntsc-X (X = M/J/K) or just 'ntsc' (V4L2_STD_NTSC)\n");
	printf("                     secam-X (X = B/G/H/D/K/L/Lc) or just 'secam' (V4L2_STD_SECAM)\n");
	printf("  --list-standards   display supported video standards [VIDIOC_ENUMSTD]\n");
	printf("  -T, --get-tuner    query the tuner settings [VIDIOC_G_TUNER]\n");
	printf("  -t, --set-tuner=<mode>\n");
	printf("                     set the audio mode of the tuner [VIDIOC_S_TUNER]\n");
	printf("                     Possible values: mono, stereo, lang2, lang1, bilingual\n");
	printf("  --list-formats     display supported video formats [VIDIOC_ENUM_FMT]\n");
	printf("  -V, --get-fmt-video\n");
	printf("     		     query the video capture format [VIDIOC_G_FMT]\n");
	printf("  -v, --set-fmt-video=width=<x>,height=<y>\n");
	printf("                     set the video capture format [VIDIOC_S_FMT]\n");
	printf("  --verbose          turn on verbose ioctl error reporting.\n");
	printf("\n");
	printf("Uncommon options:\n");
	printf("  --get-fmt-video-out\n");
	printf("     		     query the video output format [VIDIOC_G_FMT]\n");
	printf("  --set-fmt-video-out=width=<x>,height=<y>\n");
	printf("                     set the video output format [VIDIOC_S_FMT]\n");
	printf("  --get-fmt-overlay\n");
	printf("     		     query the video overlay format [VIDIOC_G_FMT]\n");
	printf("  --get-sliced-vbi-cap\n");
	printf("		     query the sliced VBI capture capabilities [VIDIOC_G_SLICED_VBI_CAP]\n");
	printf("  --get-sliced-vbi-out-cap\n");
	printf("		     query the sliced VBI output capabilities [VIDIOC_G_SLICED_VBI_CAP]\n");
	printf("  --get-fmt-sliced-vbi-out\n");
	printf("		     query the sliced VBI output format [VIDIOC_G_FMT]\n");
	printf("  --set-fmt-sliced-vbi-out=<mode>\n");
	printf("                     set the sliced VBI output format to <mode> [VIDIOC_S_FMT]\n");
	printf("                     <mode> is a comma separated list of:\n");
	printf("                     off:      turn off sliced VBI (cannot be combined with other modes)\n");
	printf("                     teletext: teletext (PAL/SECAM)\n");
	printf("                     cc:       closed caption (NTSC)\n");
	printf("                     wss:      widescreen signal (PAL/SECAM)\n");
	printf("                     vps:      VPS (PAL/SECAM)\n");
	printf("  --get-fmt-vbi      query the VBI capture format [VIDIOC_G_FMT]\n");
	printf("  --get-fmt-vbi-out  query the VBI output format [VIDIOC_G_FMT]\n");
	printf("  --get-crop-video\n");
	printf("     		     query the video capture crop window [VIDIOC_G_CROP]\n");
	printf("  --set-crop-video=top=<x>,left=<y>,width=<w>,height=<h>\n");
	printf("                     set the video capture crop window [VIDIOC_S_CROP]\n");
	printf("  --get-audio-input  query the audio input [VIDIOC_G_AUDIO]\n");
	printf("  --set-audio-input=<num>\n");
	printf("                     set the audio input to <num> [VIDIOC_S_AUDIO]\n");
	printf("  --get-audio-output query the audio output [VIDIOC_G_AUDOUT]\n");
	printf("  --set-audio-output=<num>\n");
	printf("                     set the audio output to <num> [VIDIOC_S_AUDOUT]\n");
	printf("  --list-audio-outputs\n");
	printf("                     display audio outputs [VIDIOC_ENUMAUDOUT]\n");
	printf("  --list-audio-inputs\n");
	printf("                     display audio inputs [VIDIOC_ENUMAUDIO]\n");
	printf("\n");
	printf("Expert options:\n");
	printf("  --streamoff        turn the stream off [VIDIOC_STREAMOFF]\n");
	printf("  --streamon         turn the stream on [VIDIOC_STREAMOFF]\n");
	printf("  --log-status       log the board status in the kernel log [VIDIOC_LOG_STATUS]\n");
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
		printf("%31s (int64): value=%lld", queryctrl->name, ctrl->value64);
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
		if (qctrl.type != V4L2_CTRL_TYPE_CTRL_CLASS) {
			ctrl_str2id[name2var(qctrl.name)] = qctrl.id;
			ctrl_id2str[qctrl.id] = name2var(qctrl.name);
		}
		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}
	if (qctrl.id != V4L2_CTRL_FLAG_NEXT_CTRL)
		return;
	for (id = V4L2_CID_USER_BASE; id < V4L2_CID_LASTP1; id++) {
		qctrl.id = id;
		if (ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0)
			ctrl_str2id[name2var(qctrl.name)] = qctrl.id;
	}
	for (qctrl.id = V4L2_CID_PRIVATE_BASE;
			ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0; qctrl.id++) {
		ctrl_str2id[name2var(qctrl.name)] = qctrl.id;
	}
}

static void printcrop(const struct v4l2_crop &crop)
{
	printf("Crop: Left %d, Top %d, Width %d, Height %d\n",
			crop.c.left, crop.c.top, crop.c.width, crop.c.height);
}

static int printfmt(struct v4l2_format vfmt)
{
	const flag_def vbi_def[] = {
		{ V4L2_VBI_UNSYNC,     "unsynchronized" },
		{ V4L2_VBI_INTERLACED, "interlaced" },
		{ 0, NULL }
	};
	printf("Format:\n");

	switch (vfmt.type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		printf("\tType          : %s\n", buftype2s(vfmt.type).c_str());
		printf("\tWidth/Height  : %u/%u\n", vfmt.fmt.pix.width, vfmt.fmt.pix.height);
		printf("\tPixel Format  : %s\n", fcc2s(vfmt.fmt.pix.pixelformat).c_str());
		printf("\tField         : %s\n", field2s(vfmt.fmt.pix.field).c_str());
		printf("\tBytes per Line: %u\n", vfmt.fmt.pix.bytesperline);
		printf("\tSize Image    : %u\n", vfmt.fmt.pix.sizeimage);
		printf("\tColorspace    : %s\n", colorspace2s(vfmt.fmt.pix.colorspace).c_str());
		if (vfmt.fmt.pix.priv)
			printf("\tCustom Info   : %08x\n", vfmt.fmt.pix.priv);
		break;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		printf("\tType        : %s\n", buftype2s(vfmt.type).c_str());
		printf("\tLeft/Top    : %d/%d\n",
				vfmt.fmt.win.w.left, vfmt.fmt.win.w.top);
		printf("\tWidth/Height: %d/%d\n",
				vfmt.fmt.win.w.width, vfmt.fmt.win.w.height);
		printf("\tField       : %s\n", field2s(vfmt.fmt.win.field).c_str());
		// TODO: check G_FBUF capabilities
		printf("\tChroma Key  : %08x\n", vfmt.fmt.win.chromakey);
		printf("\tClip Count  : %u\n", vfmt.fmt.win.clipcount);
		break;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		printf("\tType            : %s\n", buftype2s(vfmt.type).c_str());
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
		printf("\tType           : %s\n", buftype2s(vfmt.type).c_str());
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
		printf("\tType: %s\n", buftype2s(vfmt.type).c_str());
		break;
	default:
		printf("\tType: %s\n", buftype2s(vfmt.type).c_str());
		return -1;
	}
	return 0;
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

static int doioctl(int fd, int request, void *parm, const char *name)
{
	int retVal;

	if (!options[OptVerbose]) return ioctl(fd, request, parm);
	retVal = ioctl(fd, request, parm);
	printf("%s: ", name);
	if (retVal < 0)
		printf("failed: %s\n", strerror(errno));
	else
		printf("ok\n");

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

int main(int argc, char **argv)
{
	char *value, *subs;
	int i;

	int fd = -1;

	/* bitfield for fmts */
	unsigned int set_fmts = 0;
	unsigned int set_fmts_out = 0;
	unsigned int set_crop = 0;

	int mode = V4L2_TUNER_MODE_STEREO;	/* set audio mode */

	/* command args */
	int ch;
	char *device = strdup("/dev/video0");	/* -d device */
	struct v4l2_format vfmt;	/* set_format/get_format for video */
	struct v4l2_format vfmt_out;	/* set_format/get_format video output */
	struct v4l2_format vbi_fmt;	/* set_format/get_format for sliced VBI */
	struct v4l2_format vbi_fmt_out;	/* set_format/get_format for sliced VBI output */
	struct v4l2_format raw_fmt;	/* set_format/get_format for VBI */
	struct v4l2_format raw_fmt_out;	/* set_format/get_format for VBI output */
	struct v4l2_tuner tuner;        /* set_tuner/get_tuner */
	struct v4l2_capability vcap;	/* list_cap */
	struct v4l2_input vin;		/* list_inputs */
	struct v4l2_output vout;	/* list_outputs */
	struct v4l2_audio vaudio;	/* list audio inputs */
	struct v4l2_audioout vaudout;   /* audio outputs */
	struct v4l2_rect vcrop; 	/* crop rect */
	int input;			/* set_input/get_input */
	int output;			/* set_output/get_output */
	v4l2_std_id std;		/* get_std/set_std */
	double freq = 0;		/* get/set frequency */
	struct v4l2_frequency vf;	/* get_freq/set_freq */
	struct v4l2_standard vs;	/* list_std */
	char short_options[26 * 2 * 2 + 1];
	int idx = 0;

	memset(&vfmt, 0, sizeof(vfmt));
	memset(&vbi_fmt, 0, sizeof(vbi_fmt));
	memset(&raw_fmt, 0, sizeof(raw_fmt));
	memset(&vfmt_out, 0, sizeof(vfmt_out));
	memset(&vbi_fmt_out, 0, sizeof(vbi_fmt_out));
	memset(&raw_fmt_out, 0, sizeof(raw_fmt_out));
	memset(&tuner, 0, sizeof(tuner));
	memset(&vcap, 0, sizeof(vcap));
	memset(&vin, 0, sizeof(vin));
	memset(&vout, 0, sizeof(vout));
	memset(&vaudio, 0, sizeof(vaudio));
	memset(&vaudout, 0, sizeof(vaudout));
	memset(&vcrop, 0, sizeof(vcrop));
	memset(&vf, 0, sizeof(vf));
	memset(&vs, 0, sizeof(vs));

	if (argc == 1) {
		usage();
		return 0;
	}
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
		case OptSetDevice:
			device = strdup(optarg);
			if (device[0] >= '0' && device[0] <= '9' && device[1] == 0) {
				char dev = device[0];

				sprintf(device, "/dev/video%c", dev);
			}
			break;
		case OptSetVideoFormat:
			subs = optarg;
			while (*subs != '\0') {
				static char *const subopts[] = {
					"width",
					"height",
					NULL
				};

				switch (parse_subopt(&subs, subopts, &value)) {
				case 0:
					vfmt.fmt.pix.width = strtol(value, 0L, 0);
					set_fmts |= FMTWidth;
					break;
				case 1:
					vfmt.fmt.pix.height = strtol(value, 0L, 0);
					set_fmts |= FMTHeight;
					break;
				}
			}
			break;
		case OptSetVideoOutFormat:
			subs = optarg;
			while (*subs != '\0') {
				static char *const subopts[] = {
					"width",
					"height",
					NULL
				};

				switch (parse_subopt(&subs, subopts, &value)) {
				case 0:
					vfmt_out.fmt.pix.width = strtol(value, 0L, 0);
					set_fmts_out |= FMTWidth;
					break;
				case 1:
					vfmt_out.fmt.pix.height = strtol(value, 0L, 0);
					set_fmts_out |= FMTHeight;
					break;
				}
			}
			break;
		case OptSetVideoCrop:
			subs = optarg;
			while (*subs != '\0') {
				static char *const subopts[] = {
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
				}
			}
			break;
		case OptSetInput:
			input = strtol(optarg, 0L, 0);
			break;
		case OptSetOutput:
			output = strtol(optarg, 0L, 0);
			break;
		case OptSetAudioInput:
			vaudio.index = strtol(optarg, 0L, 0);
			break;
		case OptSetAudioOutput:
			vaudout.index = strtol(optarg, 0L, 0);
			break;
		case OptSetFreq:
			freq = strtod(optarg, NULL);
			break;
		case OptSetStandard:
			if (!strncmp(optarg, "pal", 3)) {
				if (optarg[3])
					std = parse_pal(optarg + 3);
				else
					std = V4L2_STD_PAL;
			}
			else if (!strncmp(optarg, "ntsc", 4)) {
				if (optarg[4])
					std = parse_ntsc(optarg + 4);
				else
					std = V4L2_STD_NTSC;
			}
			else if (!strncmp(optarg, "secam", 5)) {
				if (optarg[5])
					std = parse_secam(optarg + 5);
				else
					std = V4L2_STD_SECAM;
			}
			else {
				std = strtol(optarg, 0L, 0);
			}
			break;
		case OptGetCtrl:
			subs = optarg;
			while (*subs != '\0') {
				parse_next_subopt(&subs, &value);
				if (strchr(value, '=')) {
				    usage();
				    exit(1);
				}
				else {
				    get_ctrls.push_back(value);
				}
			}
			break;
		case OptSetCtrl:
			subs = optarg;
			while (*subs != '\0') {
				parse_next_subopt(&subs, &value);
				if (const char *equal = strchr(value, '=')) {
				    set_ctrls[std::string(value, (equal - value))] = equal + 1;
				}
				else {
				    fprintf(stderr, "control '%s' without '='\n", value);
				    exit(1);
				}
			}
			break;
		case OptSetTuner:
			if (!strcmp(optarg, "stereo"))
				mode = V4L2_TUNER_MODE_STEREO;
			else if (!strcmp(optarg, "lang1"))
				mode = V4L2_TUNER_MODE_LANG1;
			else if (!strcmp(optarg, "lang2"))
				mode = V4L2_TUNER_MODE_LANG2;
			else if (!strcmp(optarg, "bilingual"))
				mode = V4L2_TUNER_MODE_LANG1_LANG2;
			else if (!strcmp(optarg, "mono"))
				mode = V4L2_TUNER_MODE_MONO;
			else {
				fprintf(stderr, "Unknown audio mode\n");
				usage();
				return 1;
			}
			break;
		case OptSetSlicedVbiFormat:
		case OptSetSlicedVbiOutFormat:
		{
			bool foundOff = false;
			v4l2_format *fmt = &vbi_fmt;

			if (ch == OptSetSlicedVbiOutFormat)
				fmt = &vbi_fmt_out;
			fmt->fmt.sliced.service_set = 0;
			subs = optarg;
			while (*subs != '\0') {
				static char *const subopts[] = {
					"off",
					"teletext",
					"cc",
					"wss",
					"vps",
					NULL
				};

				switch (parse_subopt(&subs, subopts, &value)) {
				case 0:
					foundOff = true;
					break;
				case 1:
					fmt->fmt.sliced.service_set |=
					    V4L2_SLICED_TELETEXT_B;
					break;
				case 2:
					fmt->fmt.sliced.service_set |=
					    V4L2_SLICED_CAPTION_525;
					break;
				case 3:
					fmt->fmt.sliced.service_set |=
					    V4L2_SLICED_WSS_625;
					break;
				case 4:
					fmt->fmt.sliced.service_set |=
					    V4L2_SLICED_VPS;
					break;
				}
			}
			if (foundOff && fmt->fmt.sliced.service_set) {
				fprintf(stderr, "Sliced VBI mode 'off' cannot be combined with other modes\n");
				usage();
				return 1;
			}
			break;
		}
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

	if ((fd = open(device, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", device,
			strerror(errno));
		exit(1);
	}
	free(device);

	doioctl(fd, VIDIOC_QUERYCAP, &vcap, "VIDIOC_QUERYCAP");
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

	if (options[OptAll]) {
		options[OptGetVideoFormat] = 1;
		options[OptGetVideoCrop] = 1;
		options[OptGetVideoOutFormat] = 1;
		options[OptGetDriverInfo] = 1;
		options[OptGetInput] = 1;
		options[OptGetOutput] = 1;
		options[OptGetAudioInput] = 1;
		options[OptGetAudioOutput] = 1;
		options[OptGetStandard] = 1;
		options[OptGetFreq] = 1;
		options[OptGetTuner] = 1;
		options[OptGetOverlayFormat] = 1;
		options[OptGetVbiFormat] = 1;
		options[OptGetVbiOutFormat] = 1;
		options[OptGetSlicedVbiFormat] = 1;
		options[OptGetSlicedVbiOutFormat] = 1;
	}

	/* Information Opts */

	if (options[OptGetDriverInfo]) {
		printf("Driver info:\n");
		printf("\tDriver name   : %s\n", vcap.driver);
		printf("\tCard type     : %s\n", vcap.card);
		printf("\tBus info      : %s\n", vcap.bus_info);
		printf("\tDriver version: %d\n", vcap.version);
		printf("\tCapabilities  : 0x%08X\n", vcap.capabilities);
		printf("%s", cap2s(vcap.capabilities).c_str());
	}

	/* Set options */

	if (options[OptStreamOff]) {
		int dummy = 0;
		doioctl(fd, VIDIOC_STREAMOFF, &dummy, "VIDIOC_STREAMOFF");
	}

	if (options[OptSetFreq]) {
		double fac = 16;

		if (doioctl(fd, VIDIOC_G_TUNER, &tuner, "VIDIOC_G_TUNER") == 0) {
			fac = (tuner.capability & V4L2_TUNER_CAP_LOW) ? 16000 : 16;
		}
		vf.tuner = 0;
		vf.type = tuner.type;
		vf.frequency = __u32(freq * fac);
		if (doioctl(fd, VIDIOC_S_FREQUENCY, &vf,
			"VIDIOC_S_FREQUENCY") == 0)
			printf("Frequency set to %d (%f MHz)\n", vf.frequency,
					vf.frequency / fac);
	}

	if (options[OptSetStandard]) {
		if (std < 16) {
			vs.index = std;
			if (ioctl(fd, VIDIOC_ENUMSTD, &vs) >= 0) {
				std = vs.id;
			}
		}
		if (doioctl(fd, VIDIOC_S_STD, &std, "VIDIOC_S_STD") == 0)
			printf("Standard set to %08llx\n", (unsigned long long)std);
	}

	if (options[OptSetInput]) {
		if (doioctl(fd, VIDIOC_S_INPUT, &input, "VIDIOC_S_INPUT") == 0) {
			printf("Video input set to %d", input);
			vin.index = input;
			if (ioctl(fd, VIDIOC_ENUMINPUT, &vin) >= 0)
				printf(" (%s)", vin.name);
			printf("\n");
		}
	}

	if (options[OptSetOutput]) {
		if (doioctl(fd, VIDIOC_S_OUTPUT, &output, "VIDIOC_S_OUTPUT") == 0)
			printf("Output set to %d\n", output);
	}

	if (options[OptSetAudioInput]) {
		if (doioctl(fd, VIDIOC_S_AUDIO, &vaudio, "VIDIOC_S_AUDIO") == 0)
			printf("Audio input set to %d\n", vaudio.index);
	}

	if (options[OptSetAudioOutput]) {
		if (doioctl(fd, VIDIOC_S_AUDOUT, &vaudout, "VIDIOC_S_AUDOUT") == 0)
			printf("Audio output set to %d\n", vaudout.index);
	}

	if (options[OptSetTuner]) {
		struct v4l2_tuner vt;

		memset(&vt, 0, sizeof(struct v4l2_tuner));
		if (ioctl(fd, VIDIOC_G_TUNER, &vt) < 0) {
			fprintf(stderr, "ioctl: VIDIOC_G_TUNER failed\n");
			exit(1);
		}
		vt.audmode = mode;
		doioctl(fd, VIDIOC_S_TUNER, &vt, "VIDIOC_S_TUNER");
	}

	if (options[OptSetVideoFormat]) {
		struct v4l2_format in_vfmt;
		printf("ioctl: VIDIOC_S_FMT\n");
		in_vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(fd, VIDIOC_G_FMT, &in_vfmt) < 0)
			fprintf(stderr, "ioctl: VIDIOC_G_FMT failed\n");
		else {
			if (set_fmts & FMTWidth)
				in_vfmt.fmt.pix.width = vfmt.fmt.pix.width;
			if (set_fmts & FMTHeight)
				in_vfmt.fmt.pix.height = vfmt.fmt.pix.height;
			if (ioctl(fd, VIDIOC_S_FMT, &in_vfmt) < 0)
				fprintf(stderr, "ioctl: VIDIOC_S_FMT failed\n");
		}
	}

	if (options[OptSetVideoOutFormat]) {
		struct v4l2_format in_vfmt;
		printf("ioctl: VIDIOC_S_FMT\n");
		in_vfmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (ioctl(fd, VIDIOC_G_FMT, &in_vfmt) < 0)
			fprintf(stderr, "ioctl: VIDIOC_G_FMT failed\n");
		else {
			if (set_fmts & FMTWidth)
				in_vfmt.fmt.pix.width = vfmt.fmt.pix.width;
			if (set_fmts & FMTHeight)
				in_vfmt.fmt.pix.height = vfmt.fmt.pix.height;
			if (ioctl(fd, VIDIOC_S_FMT, &in_vfmt) < 0)
				fprintf(stderr, "ioctl: VIDIOC_S_FMT failed\n");
		}
	}

	if (options[OptSetSlicedVbiFormat]) {
		if (vbi_fmt.fmt.sliced.service_set == 0) {
			// switch to raw mode
			vbi_fmt.type = V4L2_BUF_TYPE_VBI_CAPTURE;
			if (doioctl(fd, VIDIOC_G_FMT, &vbi_fmt, "VIDIOC_G_FMT") == 0)
				doioctl(fd, VIDIOC_S_FMT, &vbi_fmt, "VIDIOC_S_FMT");
		} else {
			vbi_fmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
			doioctl(fd, VIDIOC_S_FMT, &vbi_fmt, "VIDIOC_S_FMT");
		}
	}

	if (options[OptSetSlicedVbiOutFormat]) {
		if (vbi_fmt_out.fmt.sliced.service_set == 0) {
			// switch to raw mode
			vbi_fmt_out.type = V4L2_BUF_TYPE_VBI_OUTPUT;
			if (doioctl(fd, VIDIOC_G_FMT, &vbi_fmt_out, "VIDIOC_G_FMT") == 0)
				doioctl(fd, VIDIOC_S_FMT, &vbi_fmt_out, "VIDIOC_S_FMT");
		} else {
			vbi_fmt_out.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
			doioctl(fd, VIDIOC_S_FMT, &vbi_fmt_out, "VIDIOC_S_FMT");
		}
	}

	if (options[OptSetVideoCrop]) {
		struct v4l2_crop in_crop;
		printf("ioctl: VIDIOC_S_CROP\n");
		in_crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(fd, VIDIOC_G_CROP, &in_crop) < 0)
			fprintf(stderr, "ioctl: VIDIOC_G_CROP failed\n");
		else {
			if (set_crop & CropWidth)
				in_crop.c.width = vcrop.width;
			if (set_crop & CropHeight)
				in_crop.c.height = vcrop.height;
			if (set_crop & CropLeft)
				in_crop.c.left = vcrop.left;
			if (set_crop & CropTop)
				in_crop.c.top = vcrop.top;
			if (ioctl(fd, VIDIOC_S_CROP, &in_crop) < 0)
				fprintf(stderr, "ioctl: VIDIOC_S_CROP failed\n");
		}
	}

	if (options[OptSetCtrl] && !set_ctrls.empty()) {
		struct v4l2_ext_controls ctrls = { 0 };

		for (ctrl_set_map::iterator iter = set_ctrls.begin();
				iter != set_ctrls.end(); ++iter) {
			struct v4l2_ext_control ctrl = { 0 };

			ctrl.id = ctrl_str2id[iter->first];
			ctrl.value = atol(iter->second.c_str());
			if (V4L2_CTRL_ID2CLASS(ctrl.id) == V4L2_CTRL_CLASS_MPEG)
				mpeg_ctrls.push_back(ctrl);
			else
				user_ctrls.push_back(ctrl);
		}
		for (unsigned i = 0; i < user_ctrls.size(); i++) {
			struct v4l2_control ctrl;

			ctrl.id = user_ctrls[i].id;
			ctrl.value = user_ctrls[i].value;
			if (doioctl(fd, VIDIOC_S_CTRL, &ctrl, "VIDIOC_S_CTRL")) {
				fprintf(stderr, "%s: %s\n",
					ctrl_id2str[ctrl.id].c_str(),
					strerror(errno));
			}
		}
		if (mpeg_ctrls.size()) {
			ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
			ctrls.count = mpeg_ctrls.size();
			ctrls.controls = &mpeg_ctrls[0];
			if (doioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls, "VIDIOC_S_EXT_CTRLS")) {
				if (ctrls.error_idx >= ctrls.count) {
					fprintf(stderr, "Error setting MPEG controls: %s\n",
						strerror(errno));
				}
				else {
					fprintf(stderr, "%s: %s\n",
						ctrl_id2str[mpeg_ctrls[ctrls.error_idx].id].c_str(),
						strerror(errno));
				}
			}
		}
	}

	/* Get options */

	if (options[OptGetVideoFormat]) {
		vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (doioctl(fd, VIDIOC_G_FMT, &vfmt, "VIDIOC_G_FMT") == 0)
			if (printfmt(vfmt) != 0)
				fprintf(stderr, "error printing result\n");
	}

	if (options[OptGetVideoOutFormat]) {
		vfmt_out.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (doioctl(fd, VIDIOC_G_FMT, &vfmt_out, "VIDIOC_G_FMT") == 0)
			if (printfmt(vfmt_out) != 0)
				fprintf(stderr, "error printing result\n");
	}

	if (options[OptGetOverlayFormat]) {
		struct v4l2_format fmt;
		memset(&fmt, 0, sizeof(fmt));
		fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
		if (doioctl(fd, VIDIOC_G_FMT, &fmt, "VIDIOC_G_FMT") == 0)
			if (printfmt(fmt) != 0)
				fprintf(stderr, "error printing result\n");
	}

	if (options[OptGetSlicedVbiFormat]) {
		vbi_fmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
		if (doioctl(fd, VIDIOC_G_FMT, &vbi_fmt,
			"VIDIOC_G_FMT") == 0)
			if (printfmt(vbi_fmt) != 0)
				fprintf(stderr,
					"error printing result\n");
	}

	if (options[OptGetSlicedVbiOutFormat]) {
		vbi_fmt_out.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
		if (doioctl(fd, VIDIOC_G_FMT, &vbi_fmt_out,
			"VIDIOC_G_FMT") == 0)
			if (printfmt(vbi_fmt_out) != 0)
				fprintf(stderr,
					"error printing result\n");
	}

	if (options[OptGetVbiFormat]) {
		raw_fmt.type = V4L2_BUF_TYPE_VBI_CAPTURE;
		if (doioctl(fd, VIDIOC_G_FMT, &raw_fmt,
			"VIDIOC_G_FMT") == 0)
			if (printfmt(raw_fmt) != 0)
				fprintf(stderr,
					"error printing result\n");
	}

	if (options[OptGetVbiOutFormat]) {
		raw_fmt_out.type = V4L2_BUF_TYPE_VBI_OUTPUT;
		if (doioctl(fd, VIDIOC_G_FMT, &raw_fmt_out,
			"VIDIOC_G_FMT") == 0)
			if (printfmt(raw_fmt_out) != 0)
				fprintf(stderr,
					"error printing result\n");
	}

	if (options[OptGetVideoCrop]) {
		struct v4l2_crop crop;

		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (doioctl(fd, VIDIOC_G_CROP, &crop, "VIDIOC_G_CROP") == 0)
			printcrop(crop);
	}

	if (options[OptGetInput]) {
		if (doioctl(fd, VIDIOC_G_INPUT, &input, "VIDIOC_G_INPUT") == 0) {
			printf("Video input : %d", input);
			vin.index = input;
			if (ioctl(fd, VIDIOC_ENUMINPUT, &vin) >= 0)
				printf(" (%s)", vin.name);
			printf("\n");
		}
	}

	if (options[OptGetOutput]) {
		if (doioctl(fd, VIDIOC_G_OUTPUT, &output, "VIDIOC_G_OUTPUT") == 0) {
			printf("Video output: %d", output);
			vout.index = output;
			if (ioctl(fd, VIDIOC_ENUMOUTPUT, &vout) >= 0) {
				printf(" (%s)", vout.name);
			}
			printf("\n");
		}
	}

	if (options[OptGetAudioInput]) {
		if (doioctl(fd, VIDIOC_G_AUDIO, &vaudio, "VIDIOC_G_AUDIO") == 0)
			printf("Audio input : %d (%s)\n", vaudio.index, vaudio.name);
	}

	if (options[OptGetAudioOutput]) {
		if (doioctl(fd, VIDIOC_G_AUDOUT, &vaudout, "VIDIOC_G_AUDOUT") == 0)
			printf("Audio output: %d (%s)\n", vaudout.index, vaudout.name);
	}

	if (options[OptGetFreq]) {
		double fac = 16;

		if (doioctl(fd, VIDIOC_G_TUNER, &tuner, "VIDIOC_G_TUNER") == 0) {
			fac = (tuner.capability & V4L2_TUNER_CAP_LOW) ? 16000 : 16;
		}
		vf.tuner = 0;
		if (doioctl(fd, VIDIOC_G_FREQUENCY, &vf, "VIDIOC_G_FREQUENCY") == 0)
			printf("Frequency: %d (%f MHz)\n", vf.frequency,
					vf.frequency / fac);
	}

	if (options[OptGetStandard]) {
		if (doioctl(fd, VIDIOC_G_STD, &std, "VIDIOC_G_STD") == 0) {
			static const char *pal[] = {
				"B", "B1", "G", "H", "I", "D", "D1", "K",
				"M", "N", "Nc", "60",
				NULL
			};
			static const char *ntsc[] = {
				"M", "M-JP", "443", "M-KR",
				NULL
			};
			static const char *secam[] = {
				"B", "D", "G", "H", "K", "K1", "L", "Lc",
				NULL
			};
			static const char *atsc[] = {
				"ATSC-8-VSB", "ATSC-16-VSB",
				NULL
			};

			printf("Video standard = 0x%08llx\n", (unsigned long long)std);
			if (std & 0xfff) {
				print_std("PAL", pal, std);
			}
			if (std & 0xf000) {
				print_std("NTSC", ntsc, std >> 12);
			}
			if (std & 0xff0000) {
				print_std("SECAM", secam, std >> 16);
			}
			if (std & 0xf000000) {
				print_std("ATSC/HDTV", atsc, std >> 24);
			}
		}
	}

	if (options[OptGetCtrl] && !get_ctrls.empty()) {
		struct v4l2_ext_controls ctrls = { 0 };

		mpeg_ctrls.clear();
		user_ctrls.clear();
		for (ctrl_get_list::iterator iter = get_ctrls.begin();
				iter != get_ctrls.end(); ++iter) {
			struct v4l2_ext_control ctrl = { 0 };

			ctrl.id = ctrl_str2id[*iter];
			if (V4L2_CTRL_ID2CLASS(ctrl.id) == V4L2_CTRL_CLASS_MPEG)
				mpeg_ctrls.push_back(ctrl);
			else
				user_ctrls.push_back(ctrl);
		}
		for (unsigned i = 0; i < user_ctrls.size(); i++) {
			struct v4l2_control ctrl;

			ctrl.id = user_ctrls[i].id;
			doioctl(fd, VIDIOC_G_CTRL, &ctrl, "VIDIOC_G_CTRL");
			printf("%s: %d\n", ctrl_id2str[ctrl.id].c_str(), ctrl.value);
		}
		if (mpeg_ctrls.size()) {
			ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
			ctrls.count = mpeg_ctrls.size();
			ctrls.controls = &mpeg_ctrls[0];
			doioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls, "VIDIOC_G_EXT_CTRLS");
			for (unsigned i = 0; i < mpeg_ctrls.size(); i++) {
				struct v4l2_ext_control ctrl = mpeg_ctrls[i];

				printf("%s: %d\n", ctrl_id2str[ctrl.id].c_str(), ctrl.value);
			}
		}
	}

	if (options[OptGetTuner]) {
		struct v4l2_tuner vt;
		memset(&vt, 0, sizeof(struct v4l2_tuner));
		if (doioctl(fd, VIDIOC_G_TUNER, &vt, "VIDIOC_G_TUNER") == 0) {
			printf("Tuner:\n");
			printf("\tCapabilities         : %s\n", tcap2s(vt.capability).c_str());
			if (vt.capability & V4L2_TUNER_CAP_LOW)
				printf("\tFrequency range      : %.1f MHz - %.1f MHz\n",
				     vt.rangelow / 16000.0, vt.rangehigh / 16000.0);
			else
				printf("\tFrequency range      : %.1f MHz - %.1f MHz\n",
				     vt.rangelow / 16.0, vt.rangehigh / 16.0);
			printf("\tSignal strength      : %d%%\n", (int)(vt.signal / 655.35));
			printf("\tCurrent audio mode   : %s\n", audmode2s(vt.audmode));
			printf("\tAvailable subchannels: %s\n",
					rxsubchans2s(vt.rxsubchans).c_str());
		}
	}

	if (options[OptLogStatus]) {
		static char buf[40960];
		int len;

		if (doioctl(fd, VIDIOC_LOG_STATUS, NULL, "VIDIOC_LOG_STATUS") == 0) {
			printf("\nStatus Log:\n\n");
			len = klogctl(3, buf, sizeof(buf) - 1);
			if (len >= 0) {
				char *p = buf;
				char *q;

				buf[len] = 0;
				while ((q = strstr(p, "START STATUS CARD #"))) {
					p = q + 1;
				}
				if (p) {
					while (p > buf && *p != '<') p--;
					q = p;
					while ((q = strstr(q, "<6>"))) {
						memcpy(q, "   ", 3);
					}
					printf("%s", p);
				}
			}
		}
	}

	/* List options */

	if (options[OptListInputs]) {
		vin.index = 0;
		printf("ioctl: VIDIOC_ENUMINPUT\n");
		while (ioctl(fd, VIDIOC_ENUMINPUT, &vin) >= 0) {
			if (vin.index)
				printf("\n");
			printf("\tInput   : %d\n", vin.index);
			printf("\tName    : %s\n", vin.name);
			printf("\tType    : 0x%08X\n", vin.type);
			printf("\tAudioset: 0x%08X\n", vin.audioset);
			printf("\tTuner   : 0x%08X\n", vin.tuner);
			printf("\tStandard: 0x%016llX ( ", (unsigned long long)vin.std);
			if (vin.std & 0x000FFF)
				printf("PAL ");	// hack
			if (vin.std & 0x00F000)
				printf("NTSC ");	// hack
			if (vin.std & 0x7F0000)
				printf("SECAM ");	// hack
			printf(")\n");
			printf("\tStatus  : %d\n", vin.status);
			vin.index++;
		}
	}

	if (options[OptListOutputs]) {
		vout.index = 0;
		printf("ioctl: VIDIOC_ENUMOUTPUT\n");
		while (ioctl(fd, VIDIOC_ENUMOUTPUT, &vout) >= 0) {
			if (vout.index)
				printf("\n");
			printf("\tOutput  : %d\n", vout.index);
			printf("\tName    : %s\n", vout.name);
			printf("\tType    : 0x%08X\n", vout.type);
			printf("\tAudioset: 0x%08X\n", vout.audioset);
			printf("\tStandard: 0x%016llX ( ", (unsigned long long)vout.std);
			if (vout.std & 0x000FFF)
				printf("PAL ");	// hack
			if (vout.std & 0x00F000)
				printf("NTSC ");	// hack
			if (vout.std & 0x7F0000)
				printf("SECAM ");	// hack
			printf(")\n");
			vout.index++;
		}
	}

	if (options[OptListAudioInputs]) {
		struct v4l2_audio vaudio;	/* list audio inputs */
		vaudio.index = 0;
		printf("ioctl: VIDIOC_ENUMAUDIO\n");
		while (ioctl(fd, VIDIOC_ENUMAUDIO, &vaudio) >= 0) {
			if (vaudio.index)
				printf("\n");
			printf("\tInput   : %d\n", vaudio.index);
			printf("\tName    : %s\n", vaudio.name);
			vaudio.index++;
		}
	}

	if (options[OptListAudioOutputs]) {
		struct v4l2_audioout vaudio;	/* list audio outputs */
		vaudio.index = 0;
		printf("ioctl: VIDIOC_ENUMAUDOUT\n");
		while (ioctl(fd, VIDIOC_ENUMAUDOUT, &vaudio) >= 0) {
			if (vaudio.index)
				printf("\n");
			printf("\tOutput  : %d\n", vaudio.index);
			printf("\tName    : %s\n", vaudio.name);
			vaudio.index++;
		}
	}

	if (options[OptListStandards]) {
		printf("ioctl: VIDIOC_ENUMSTD\n");
		vs.index = 0;
		while (ioctl(fd, VIDIOC_ENUMSTD, &vs) >= 0) {
			if (vs.index)
				printf("\n");
			printf("\tindex       : %d\n", vs.index);
			printf("\tID          : 0x%016llX\n", (unsigned long long)vs.id);
			printf("\tName        : %s\n", vs.name);
			printf("\tFrame period: %d/%d\n",
			       vs.frameperiod.numerator,
			       vs.frameperiod.denominator);
			printf("\tFrame lines : %d\n", vs.framelines);
			vs.index++;
		}
	}

	if (options[OptListFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);
		print_video_formats(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT);
		print_video_formats(fd, V4L2_BUF_TYPE_VIDEO_OVERLAY);
	}

	if (options[OptGetSlicedVbiCap]) {
		struct v4l2_sliced_vbi_cap cap;

		cap.type = V4L2_BUF_TYPE_VBI_CAPTURE;
		if (doioctl(fd, VIDIOC_G_SLICED_VBI_CAP, &cap, "VIDIOC_G_SLICED_VBI_CAP") == 0) {
			print_sliced_vbi_cap(cap);
		}
	}

	if (options[OptGetSlicedVbiOutCap]) {
		struct v4l2_sliced_vbi_cap cap;

		cap.type = V4L2_BUF_TYPE_VBI_OUTPUT;
		if (doioctl(fd, VIDIOC_G_SLICED_VBI_CAP, &cap, "VIDIOC_G_SLICED_VBI_CAP") == 0) {
			print_sliced_vbi_cap(cap);
		}
	}

	if (options[OptListCtrlsMenus]) {
		list_controls(fd, 1);
	}

	if (options[OptListCtrls]) {
		list_controls(fd, 0);
	}

	if (options[OptStreamOn]) {
		int dummy = 0;
		doioctl(fd, VIDIOC_STREAMON, &dummy, "VIDIOC_STREAMON");
	}

	close(fd);
	exit(0);
}
