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
    Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

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
#include <math.h>

#include <linux/videodev2.h>
#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>

/* copied from ivtv-driver.h */
#define IVTV_DBGFLG_WARN    (1 << 0)
#define IVTV_DBGFLG_INFO    (1 << 1)
#define IVTV_DBGFLG_MB      (1 << 2)
#define IVTV_DBGFLG_IOCTL   (1 << 3)
#define IVTV_DBGFLG_FILE    (1 << 4)
#define IVTV_DBGFLG_DMA     (1 << 5)
#define IVTV_DBGFLG_IRQ     (1 << 6)
#define IVTV_DBGFLG_DEC     (1 << 7)
#define IVTV_DBGFLG_YUV     (1 << 8)
#define IVTV_DBGFLG_I2C     (1 << 9)
/* Flag to turn on high volume debugging */
#define IVTV_DBGFLG_HIGHVOL (1 << 10)

/* Internals copied from media/v4l2-common.h */
#define VIDIOC_INT_RESET            	_IOW('d', 102, __u32)

#include <linux/ivtv.h>

/* GPIO */
#define IVTV_REG_GPIO_IN_OFFSET    (0x9008 + 0x02000000)
#define IVTV_REG_GPIO_OUT_OFFSET   (0x900c + 0x02000000)
#define IVTV_REG_GPIO_DIR_OFFSET   (0x9020 + 0x02000000)

/* Options */
enum Option {
	OptSetDebugLevel = 'D',
	OptSetDevice = 'd',
	OptGetDebugLevel = 'e',
	OptHelp = 'h',
	OptSetGPIO = 'i',
	OptListGPIO = 'I',
	OptPassThrough = 'K',
	OptFrameSync = 'k',
	OptReset = 128,
	OptSetYuvMode,
	OptGetYuvMode,
	OptSetAudioMute,
	OptSetStereoMode,
	OptSetBilingualMode,
	OptLast = 256
};

static char options[OptLast];

static int app_result;

static struct option long_options[] = {
	/* Please keep in alphabetical order of the short option.
	   That makes it easier to see which options are still free. */
	{"set-debug", required_argument, 0, OptSetDebugLevel},
	{"device", required_argument, 0, OptSetDevice},
	{"get-debug", no_argument, 0, OptGetDebugLevel},
	{"help", no_argument, 0, OptHelp},
	{"set-gpio", required_argument, 0, OptSetGPIO},
	{"list-gpio", no_argument, 0, OptListGPIO},
	{"passthrough", required_argument, 0, OptPassThrough},
	{"sync", no_argument, 0, OptFrameSync},
	{"reset", required_argument, 0, OptReset},
	{"get-yuv-mode", no_argument, 0, OptGetYuvMode},
	{"set-yuv-mode", required_argument, 0, OptSetYuvMode},
	{"set-audio-mute", required_argument, 0, OptSetAudioMute},
	{"set-stereo-mode", required_argument, 0, OptSetStereoMode},
	{"set-bilingual-mode", required_argument, 0, OptSetBilingualMode},
	{0, 0, 0, 0}
};

static void usage(void)
{
	printf("Usage:\n");
	printf("  -d, --device <dev> use device <dev> instead of /dev/video0\n");
	printf("  -h, --help         display this help message\n");
	printf("  -K, --passthrough <mode>\n");
	printf("                     set passthrough mode: 1 = on, 0 = off [IVTV_IOC_PASSTHROUGH]\n");
	printf("  --get-yuv-mode     display the current yuv mode\n");
	printf("  --set-yuv-mode mode=<mode>\n");
	printf("                     set the current yuv mode:\n");
	printf("       		     mode 0: interlaced (top transmitted first)\n");
	printf("       		     mode 1: interlaced (bottom transmitted first)\n");
	printf("       		     mode 2: progressive\n");
	printf("       		     mode 3: auto\n");
	printf("  --set-audio-mute <mute>\n");
	printf("       		     0=enable audio during 1.5x and 0.5x playback\n");
	printf("       		     1=mute audio during 1.5x and 0.5x playback\n");
	printf("  --set-stereo-mode <mode>\n");
	printf("       		     mode 0: playback stereo as stereo\n");
	printf("       		     mode 1: playback left stereo channel as mono\n");
	printf("       		     mode 2: playback right stereo channel as mono\n");
	printf("       		     mode 3: playback stereo as mono\n");
	printf("       		     mode 4: playback stereo as swapped stereo\n");
	printf("  --set-bilingual-mode <mode>\n");
	printf("       		     mode 0: playback bilingual as stereo\n");
	printf("       		     mode 1: playback left bilingual channel as mono\n");
	printf("       		     mode 2: playback right bilingual channel as mono\n");
	printf("       		     mode 3: playback bilingual as mono\n");
	printf("       		     mode 4: playback bilingual as swapped stereo\n");
	printf("  --reset <mask>     reset the infrared receiver (1) or digitizer (2) [VIDIOC_INT_RESET]\n");
	printf("\n");
	printf("Expert options:\n");
	printf("  -D, --set-debug <level>\n");
	printf("                     set the module debug variable\n");
	printf("                        1/0x0001: warning\n");
	printf("                        2/0x0002: info\n");
	printf("                        4/0x0004: mailbox\n");
	printf("                        8/0x0008: ioctl\n");
	printf("                       16/0x0010: file\n");
	printf("                       32/0x0020: dma\n");
	printf("                       64/0x0040: irq\n");
	printf("                      128/0x0080: decoder\n");
	printf("                      256/0x0100: yuv\n");
	printf("                      512/0x0200: i2c\n");
	printf("                     1024/0x0400: high volume\n");
	printf("  -e, --get-debug    query the module debug variable\n");
	printf("  -I, --list-gpio\n");
	printf("                     show GPIO input/direction/output bits\n");
	printf("  -i, --set-gpio [dir=<dir>,]val=<val>\n");
	printf("                     set GPIO direction bits to <dir> and set output to <val>\n");
	printf("  -k, --sync         test vsync's capabilities [VIDEO_GET_EVENT]\n");
	exit(0);
}

static char *pts_to_string(unsigned long pts, float fps)
{
	static char buf[256];
	int hours, minutes, seconds, fracsec;
	int frame;

	static const int MPEG_CLOCK_FREQ = 90000;
	seconds = pts / MPEG_CLOCK_FREQ;
	fracsec = pts % MPEG_CLOCK_FREQ;

	minutes = seconds / 60;
	seconds = seconds % 60;

	hours = minutes / 60;
	minutes = minutes % 60;

	frame = (int)ceilf(((float)fracsec / (float)MPEG_CLOCK_FREQ) * fps);

	snprintf(buf, sizeof(buf), "%d:%02d:%02d:%d", hours, minutes, seconds, frame);
	return buf;
}

static void print_debug_mask(int mask)
{
#define MASK_OR_NOTHING (mask ? " | " : "")
	if (mask & IVTV_DBGFLG_WARN) {
		mask &= ~IVTV_DBGFLG_WARN;
		printf("IVTV_DBGFLG_WARN%s", MASK_OR_NOTHING);
	}
	if (mask & IVTV_DBGFLG_INFO) {
		mask &= ~IVTV_DBGFLG_INFO;
		printf("IVTV_DBGFLG_INFO%s", MASK_OR_NOTHING);
	}
	if (mask & IVTV_DBGFLG_MB) {
		mask &= ~IVTV_DBGFLG_MB;
		printf("IVTV_DBGFLG_MB%s", MASK_OR_NOTHING);
	}
	if (mask & IVTV_DBGFLG_DMA) {
		mask &= ~IVTV_DBGFLG_DMA;
		printf("IVTV_DBGFLG_DMA%s", MASK_OR_NOTHING);
	}
	if (mask & IVTV_DBGFLG_IOCTL) {
		mask &= ~IVTV_DBGFLG_IOCTL;
		printf("IVTV_DBGFLG_IOCTL%s", MASK_OR_NOTHING);
	}
	if (mask & IVTV_DBGFLG_FILE) {
		mask &= ~IVTV_DBGFLG_FILE;
		printf("IVTV_DBGFLG_FILE%s", MASK_OR_NOTHING);
	}
	if (mask & IVTV_DBGFLG_I2C) {
		mask &= ~IVTV_DBGFLG_I2C;
		printf("IVTV_DBGFLG_I2C%s", MASK_OR_NOTHING);
	}
	if (mask & IVTV_DBGFLG_IRQ) {
		mask &= ~IVTV_DBGFLG_IRQ;
		printf("IVTV_DBGFLG_IRQ%s", MASK_OR_NOTHING);
	}
	if (mask & IVTV_DBGFLG_DEC) {
		mask &= ~IVTV_DBGFLG_DEC;
		printf("IVTV_DBGFLG_DEC%s", MASK_OR_NOTHING);
	}
	if (mask & IVTV_DBGFLG_YUV) {
		mask &= ~IVTV_DBGFLG_YUV;
		printf("IVTV_DBGFLG_YUV%s", MASK_OR_NOTHING);
	}
	if (mask & IVTV_DBGFLG_HIGHVOL) {
		mask &= ~IVTV_DBGFLG_HIGHVOL;
		printf("IVTV_DBGFLG_HIGHVOL%s", MASK_OR_NOTHING);
	}
	if (mask)
		printf("0x%08x", mask);
	printf("\n");
}

static int dowrite(const char *buf, const char *fn)
{
	FILE *f = fopen(fn, "w");
	if (f == NULL) {
		printf("failed: %s\n", strerror(errno));
		return errno;
	}
	fprintf(f, "%s", buf);
	fclose(f);
	return 0;
}

static char *doread(const char *fn)
{
	static char buf[1000];
	FILE *f = fopen(fn, "r");
	int s;

	if (f == NULL) {
		printf("failed: %s\n", strerror(errno));
		return NULL;
	}
	s = fread(buf, 1, sizeof(buf) - 1, f);
	buf[s] = 0;
	fclose(f);
	return buf;
}

static const char *field2s(int val)
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
		return "Unknown";
	}
}

static int doioctl(int fd, unsigned long int request, void *parm, const char *name)
{
	int retVal;

	printf("ioctl %s ", name);
	retVal = ioctl(fd, request, parm);
	if (retVal < 0) {
		app_result = -1;
		printf("failed: %s\n", strerror(errno));
	}
	else
		printf("ok\n");

	return retVal;
}

int main(int argc, char **argv)
{
	char *value, *subs;
	int i;
	char *subopts[] = {
#define SUB_VAL				0
		"val",
#define SUB_YUV_MODE			1
		"mode",
#define SUB_DIR				2
		"dir",

		NULL
	};

	int fd = -1;

	/* bitfield for OptSetCodec */

	/* command args */
	const char *device = "/dev/video0";	/* -d device */
	int ch;
	int yuv_mode = 0;
	unsigned short gpio_out = 0x0;	/* GPIO output data */
	unsigned short gpio_dir = 0x0;	/* GPIO direction bits */
	int gpio_set_dir = 0;
	int passthrough = 0;
	long audio_mute = 0;
	long stereo_mode = 0;
	long bilingual_mode = 0;
	int debug_level = 0;
	__u32 reset = 0;
	int new_debug_level, gdebug_level;
	double timestamp;
	char *ptsstr;
	char short_options[26 * 2 * 2 + 1];

	if (argc == 1) {
		usage();
		return 0;
	}
	while (1) {
		int option_index = 0;
		int idx = 0;

		for (i = 0; long_options[i].name; i++) {
			if (!isalpha(long_options[i].val))
				continue;
			short_options[idx++] = long_options[i].val;
			if (long_options[i].has_arg == required_argument)
				short_options[idx++] = ':';
		}
		short_options[idx] = 0;
		ch = getopt_long(argc, argv, short_options,
				 long_options, &option_index);
		if (ch == -1)
			break;

		options[(int)ch] = 1;
		switch (ch) {
		case OptSetYuvMode:
		    {
			subs = optarg;
			while (*subs != '\0') {
				switch (getsubopt(&subs, subopts, &value)) {
				case SUB_YUV_MODE:
					if (value == NULL) {
						fprintf(stderr,
							"No value given to suboption <mode>\n");
						usage();
						return 1;

					}
					yuv_mode = strtol(value, 0L, 0);
					if (yuv_mode < 0 || yuv_mode > 3) {
						fprintf(stderr, "invalid yuv mode\n");
						return 1;
					}
					break;
				}
			}
		    }
		    break;
		case OptHelp:
			usage();
			return 0;
		case OptSetDebugLevel:{
			debug_level = strtol(optarg, 0L, 0);
			break;
		}
		case OptSetDevice:
			device = optarg;
			if (device[0] >= '0' && device[0] <= '9' && strlen(device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/video%s", device);
				device = newdev;
			}
			break;
		case OptReset:
			reset = strtol(optarg, 0L, 0);
			break;
		case OptPassThrough:
			passthrough = strtol(optarg, 0L, 0);
			break;
		case OptSetAudioMute:
			audio_mute = strtol(optarg, 0L, 0);
			break;
		case OptSetStereoMode:
			stereo_mode = strtol(optarg, 0L, 0);
			break;
		case OptSetBilingualMode:
			bilingual_mode = strtol(optarg, 0L, 0);
			break;
		case OptSetGPIO:
			subs = optarg;
			while (*subs != '\0') {
				switch (getsubopt(&subs, subopts, &value)) {
				case SUB_DIR:
					if (value == NULL) {
						fprintf(stderr,
							"No value given to suboption <dir>\n");
						usage();
						exit(1);
					}
					gpio_dir = strtol(value, 0L, 0);
					gpio_set_dir = 1;
					break;
				case SUB_VAL:
					if (value == NULL) {
						fprintf(stderr,
							"No value given to suboption <val>\n");
						usage();
						exit(1);
					}
					gpio_out = (unsigned short)strtol(value, 0L, 0);
					break;
				default:
					fprintf(stderr,
						"Invalid suboptions specified\n");
					usage();
					exit(1);
					break;
				}
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

	fd = open(device, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", device,
			strerror(errno));
		exit(1);
	}

	/* Setting Opts */

	if (options[OptFrameSync]) {
		printf("ioctl: VIDEO_GET_EVENT\n");

		for (;;) {
			struct video_event ev;
			int fps = 30;
			v4l2_std_id std;

			if (ioctl(fd, VIDIOC_G_STD, &std) == 0)
				fps = (std & V4L2_STD_525_60) ? 30 : 25;
			if (ioctl(fd, VIDEO_GET_EVENT, &ev) < 0) {
				fprintf(stderr, "ioctl: VIDEO_GET_EVENT failed\n");
				break;
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
			} else if (ev.timestamp.tv_sec == 0 && ev.timestamp.tv_nsec == 0) {
#else
			} else if (ev.timestamp == 0) {
#endif
				unsigned long long pts = 0, frame = 0;
				struct timeval tv;
				gettimeofday(&tv, NULL);
				timestamp =
				    (double)tv.tv_sec +
				    ((double)tv.tv_usec / 1000000.0);

				ioctl(fd, VIDEO_GET_PTS, &pts);
				ioctl(fd, VIDEO_GET_FRAME_COUNT, &frame);
				ptsstr = pts_to_string(pts, fps);
				printf("%10.6f: pts %-20s, %lld frames\n",
				     timestamp, ptsstr, frame);
			}
		}
	}

	if (options[OptSetGPIO]) {
		struct v4l2_dbg_register reg;

		reg.match.type = V4L2_CHIP_MATCH_HOST;
		reg.match.addr = 0;
		reg.reg = IVTV_REG_GPIO_DIR_OFFSET;
		reg.val = gpio_dir;
		if (gpio_set_dir && doioctl(fd, VIDIOC_DBG_S_REGISTER, &reg,
			"VIDIOC_DBG_S_REGISTER") == 0)
			printf("GPIO dir set to 0x%04llx\n", reg.val);
		reg.reg = IVTV_REG_GPIO_OUT_OFFSET;
		reg.val = gpio_out;
		if (doioctl(fd, VIDIOC_DBG_S_REGISTER, &reg,
			"VIDIOC_DBG_S_REGISTER") == 0)
			printf("GPIO out set to 0x%04llx\n", reg.val);
	}

	if (options[OptListGPIO]) {
		struct v4l2_dbg_register reg;

		reg.match.type = V4L2_CHIP_MATCH_HOST;
		reg.match.addr = 0;
		reg.reg = IVTV_REG_GPIO_IN_OFFSET;
		if (ioctl(fd, VIDIOC_DBG_G_REGISTER, &reg) == 0)
			printf("GPIO in:  0x%04llx\n", reg.val);
		reg.reg = IVTV_REG_GPIO_DIR_OFFSET;
		if (ioctl(fd, VIDIOC_DBG_G_REGISTER, &reg) == 0)
			printf("GPIO dir: 0x%04llx\n", reg.val);
		reg.reg = IVTV_REG_GPIO_OUT_OFFSET;
		if (ioctl(fd, VIDIOC_DBG_G_REGISTER, &reg) == 0)
			printf("GPIO out: 0x%04llx\n", reg.val);
	}

	if (options[OptSetDebugLevel]) {
		char buf[20];
		new_debug_level = debug_level;

		sprintf(buf, "%d", debug_level);
		if (dowrite(buf, "/sys/module/ivtv/parameters/debug") == 0) {
			printf(" set debug level: ");
			print_debug_mask(new_debug_level);
			printf("\n");
		}
	}

	if (options[OptGetDebugLevel]) {
		char *buf = doread("/sys/module/ivtv/parameters/debug");

		gdebug_level = 0;
		if (buf) {
			gdebug_level = atol(buf);
			printf(" debug level: ");
			print_debug_mask(gdebug_level);
			printf("\n");
		}
	}

	if (options[OptPassThrough]) {
		long source = passthrough ? VIDEO_SOURCE_DEMUX : VIDEO_SOURCE_MEMORY;

		doioctl(fd, VIDEO_SELECT_SOURCE, (void *)source,
				"IVTV_IOC_PASSTHROUGH");
	}

	if (options[OptSetAudioMute]) {
		doioctl(fd, AUDIO_SET_MUTE, (void *)audio_mute, "AUDIO_SET_MUTE");
	}

	if (options[OptSetStereoMode]) {
		doioctl(fd, AUDIO_CHANNEL_SELECT,
			(void *)stereo_mode, "AUDIO_CHANNEL_SELECT");
	}

	if (options[OptSetBilingualMode]) {
		doioctl(fd, AUDIO_BILINGUAL_CHANNEL_SELECT,
			(void *)bilingual_mode, "AUDIO_BILINGUAL_CHANNEL_SELECT");
	}

	if (options[OptReset])
		doioctl(fd, VIDIOC_INT_RESET, &reset, "VIDIOC_INT_RESET");

	if (options[OptSetYuvMode]) {
		struct ivtv_dma_frame frame;
		struct v4l2_format fmt;
		const enum v4l2_field map[4] = {
			V4L2_FIELD_INTERLACED_TB,
			V4L2_FIELD_INTERLACED_BT,
			V4L2_FIELD_NONE,
			V4L2_FIELD_ANY,
		};

		printf("set yuv mode\n");
		memset(&frame, 0, sizeof(frame));
		frame.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (ioctl(fd, IVTV_IOC_DMA_FRAME, &frame) < 0) {
			fprintf(stderr, "Unable to switch to user DMA YUV mode\n");
			exit(1);
		}
		fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		ioctl(fd, VIDIOC_G_FMT, &fmt);
		fmt.fmt.pix.field = map[yuv_mode];
		doioctl(fd, VIDIOC_S_FMT, &fmt, "VIDIOC_S_FMT");
	}

	if (options[OptGetYuvMode]) {
		struct ivtv_dma_frame frame;
		struct v4l2_format fmt;

		memset(&frame, 0, sizeof(frame));
		frame.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (ioctl(fd, IVTV_IOC_DMA_FRAME, &frame) < 0) {
			fprintf(stderr, "Unable to switch to user DMA YUV mode\n");
			exit(1);
		}
		fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		doioctl(fd, VIDIOC_G_FMT, &fmt, "VIDIOC_G_FMT");
		printf("Current yuv_mode %d %s\n", fmt.fmt.pix.field,
				field2s(fmt.fmt.pix.field));
	}

	close(fd);
	exit(app_result);
}
