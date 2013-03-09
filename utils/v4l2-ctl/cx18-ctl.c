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

#include <config.h>
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

/* copied from cx18-driver.h */
#define CX18_DBGFLG_WARN  (1 << 0)
#define CX18_DBGFLG_INFO  (1 << 1)
#define CX18_DBGFLG_API   (1 << 2)
#define CX18_DBGFLG_DMA   (1 << 3)
#define CX18_DBGFLG_IOCTL (1 << 4)
#define CX18_DBGFLG_FILE  (1 << 5)
#define CX18_DBGFLG_I2C   (1 << 6)
#define CX18_DBGFLG_IRQ   (1 << 7)
/* Flag to turn on high volume debugging */
#define CX18_DBGFLG_HIGHVOL (1 << 8)

/* Internals copied from media/v4l2-common.h */
#define VIDIOC_INT_RESET            	_IOW('d', 102, __u32)

#define __stringify_1(x)	#x
#define __stringify(x)		__stringify_1(x)

/* GPIO */
#define CX18_REG_GPIO_IN     0x02c72010
#define CX18_REG_GPIO_OUT1   0x02c78100
#define CX18_REG_GPIO_DIR1   0x02c78108
#define CX18_REG_GPIO_OUT2   0x02c78104
#define CX18_REG_GPIO_DIR2   0x02c7810c

/* Options */
enum Option {
	OptSetDebugLevel = 'D',
	OptSetDevice = 'd',
	OptGetDebugLevel = 'e',
	OptHelp = 'h',
	OptSetGPIO = 'i',
	OptListGPIO = 'I',
	OptReset = 128,
	OptVersion,
	OptLast = 256
};

static char options[OptLast];

static struct option long_options[] = {
	/* Please keep in alphabetical order of the short option.
	   That makes it easier to see which options are still free. */
	{"set-debug", required_argument, 0, OptSetDebugLevel},
	{"device", required_argument, 0, OptSetDevice},
	{"get-debug", no_argument, 0, OptGetDebugLevel},
	{"help", no_argument, 0, OptHelp},
	{"set-gpio", required_argument, 0, OptSetGPIO},
	{"list-gpio", no_argument, 0, OptListGPIO},
	{"reset", required_argument, 0, OptReset},
	{"version", no_argument, 0, OptVersion},
	{0, 0, 0, 0}
};

static void usage(void)
{
	printf("Usage:\n");
	printf("  -d, --device <dev> use device <dev> instead of /dev/video0\n");
	printf("  -h, --help         display this help message\n");
	printf("  --reset <mask>     reset the infrared receiver (1) or digitizer (2) [VIDIOC_INT_RESET]\n");
	printf("  --version          shows the version number of this utility.\n");
	printf("                     It should match the driver version.\n");
	printf("\n");
	printf("Expert options:\n");
	printf("  -D, --set-debug <level>\n");
	printf("                     set the module debug variable\n");
	printf("                        1/0x0001: warning\n");
	printf("                        2/0x0002: info\n");
	printf("                        4/0x0004: mailbox\n");
	printf("                        8/0x0008: dma\n");
	printf("                       16/0x0010: ioctl\n");
	printf("                       32/0x0020: file\n");
	printf("                       64/0x0040: i2c\n");
	printf("                      128/0x0080: irq\n");
	printf("                      256/0x0100: high volume\n");
	printf("  -e, --get-debug    query the module debug variable\n");
	printf("  -I, --list-gpio\n");
	printf("                     show GPIO input/direction/output bits\n");
	printf("  -i, --set-gpio [dir=<dir>,]val=<val>\n");
	printf("                     set GPIO direction bits to <dir> and set output to <val>\n");
	exit(0);
}

static void print_debug_mask(int mask)
{
#define MASK_OR_NOTHING (mask ? " | " : "")
	if (mask & CX18_DBGFLG_WARN) {
		mask &= ~CX18_DBGFLG_WARN;
		printf("CX18_DBGFLG_WARN%s", MASK_OR_NOTHING);
	}
	if (mask & CX18_DBGFLG_INFO) {
		mask &= ~CX18_DBGFLG_INFO;
		printf("CX18_DBGFLG_INFO%s", MASK_OR_NOTHING);
	}
	if (mask & CX18_DBGFLG_API) {
		mask &= ~CX18_DBGFLG_API;
		printf("CX18_DBGFLG_API%s", MASK_OR_NOTHING);
	}
	if (mask & CX18_DBGFLG_DMA) {
		mask &= ~CX18_DBGFLG_DMA;
		printf("CX18_DBGFLG_DMA%s", MASK_OR_NOTHING);
	}
	if (mask & CX18_DBGFLG_IOCTL) {
		mask &= ~CX18_DBGFLG_IOCTL;
		printf("CX18_DBGFLG_IOCTL%s", MASK_OR_NOTHING);
	}
	if (mask & CX18_DBGFLG_FILE) {
		mask &= ~CX18_DBGFLG_FILE;
		printf("CX18_DBGFLG_FILE%s", MASK_OR_NOTHING);
	}
	if (mask & CX18_DBGFLG_I2C) {
		mask &= ~CX18_DBGFLG_I2C;
		printf("CX18_DBGFLG_I2C%s", MASK_OR_NOTHING);
	}
	if (mask & CX18_DBGFLG_IRQ) {
		mask &= ~CX18_DBGFLG_IRQ;
		printf("CX18_DBGFLG_IRQ%s", MASK_OR_NOTHING);
	}
	if (mask & CX18_DBGFLG_HIGHVOL) {
		mask &= ~CX18_DBGFLG_HIGHVOL;
		printf("CX18_DBGFLG_HIGHVOL%s", MASK_OR_NOTHING);
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

static int doioctl(int fd, unsigned long int request, void *parm, const char *name)
{
	int retVal;

	printf("ioctl %s ", name);
	retVal = ioctl(fd, request, parm);
	if (retVal < 0)
		printf("failed: %s\n", strerror(errno));
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
#define SUB_DIR				1
		"dir",

		NULL
	};

	int fd = -1;

	/* bitfield for OptSetCodec */

	/* command args */
	const char *device = "/dev/video0";	/* -d device */
	int ch;
	unsigned int gpio_out = 0x0;	/* GPIO output data */
	unsigned int gpio_dir = 0x0;	/* GPIO direction bits */
	int gpio_set_dir = 0;
	int debug_level = 0;
	__u32 reset = 0;
	int new_debug_level, gdebug_level;
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
					gpio_dir = strtoul(value, 0L, 0);
					gpio_set_dir = 1;
					break;
				case SUB_VAL:
					if (value == NULL) {
						fprintf(stderr,
							"No value given to suboption <val>\n");
						usage();
						exit(1);
					}
					gpio_out =
					    (unsigned short)strtoul(value, 0L, 0);
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
	if (options[OptSetGPIO]) {
		struct v4l2_dbg_register reg;

		reg.match.type = V4L2_CHIP_MATCH_HOST;
		reg.match.addr = 0;
		reg.reg = CX18_REG_GPIO_DIR1;
		reg.val = (unsigned)((gpio_dir & 0xffff) << 16);
		if (gpio_set_dir && doioctl(fd, VIDIOC_DBG_S_REGISTER, &reg,
			"VIDIOC_DBG_S_REGISTER") == 0)
			printf("GPIO dir 1 set to 0x%08llx\n", reg.val);
		reg.reg = CX18_REG_GPIO_DIR2;
		reg.val = (unsigned)(gpio_dir & 0xffff0000);
		if (gpio_set_dir && doioctl(fd, VIDIOC_DBG_S_REGISTER, &reg,
			"VIDIOC_DBG_S_REGISTER") == 0)
			printf("GPIO dir 2 set to 0x%08llx\n", reg.val);
		reg.reg = CX18_REG_GPIO_OUT1;
		reg.val = (unsigned)((gpio_dir & 0xffff) << 16) | (gpio_out & 0xffff);
		if (doioctl(fd, VIDIOC_DBG_S_REGISTER, &reg,
			"VIDIOC_DBG_S_REGISTER") == 0)
			printf("GPIO out 1 set to 0x%08llx\n", reg.val);
		reg.reg = CX18_REG_GPIO_OUT2;
		reg.val = (unsigned)(gpio_dir & 0xffff0000) | (gpio_out >> 16);
		if (doioctl(fd, VIDIOC_DBG_S_REGISTER, &reg,
			"VIDIOC_DBG_S_REGISTER") == 0)
			printf("GPIO out 2 set to 0x%08llx\n", reg.val);
	}

	if (options[OptListGPIO]) {
		struct v4l2_dbg_register reg;

		reg.match.type = V4L2_CHIP_MATCH_HOST;
		reg.match.addr = 0;
		reg.reg = CX18_REG_GPIO_IN;
		if (ioctl(fd, VIDIOC_DBG_G_REGISTER, &reg) == 0)
			printf("GPIO in:  0x%04llx\n", reg.val);
		reg.reg = CX18_REG_GPIO_DIR1;
		if (ioctl(fd, VIDIOC_DBG_G_REGISTER, &reg) == 0)
			printf("GPIO dir: 0x%04llx\n", reg.val);
		reg.reg = CX18_REG_GPIO_OUT1;
		if (ioctl(fd, VIDIOC_DBG_G_REGISTER, &reg) == 0)
			printf("GPIO out: 0x%04llx\n", reg.val);
	}

	if (options[OptSetDebugLevel]) {
		char buf[20];
		new_debug_level = debug_level;

		sprintf(buf, "%d", debug_level);
		if (dowrite(buf, "/sys/module/cx18/parameters/debug") == 0) {
			printf(" set debug level: ");
			print_debug_mask(new_debug_level);
			printf("\n");
		}
	}

	if (options[OptGetDebugLevel]) {
		char *buf;

		gdebug_level = 0;
		buf = doread("/sys/module/cx18/parameters/debug");
		if (buf) {
			gdebug_level = atol(buf);
			printf(" debug level: ");
			print_debug_mask(gdebug_level);
			printf("\n");
		}
	}

	if (options[OptReset])
		doioctl(fd, VIDIOC_INT_RESET, &reset, "VIDIOC_INT_RESET");

	if (options[OptVersion])
		printf("cx18ctl version " V4L_UTILS_VERSION "\n");

	close(fd);
	exit(0);
}
