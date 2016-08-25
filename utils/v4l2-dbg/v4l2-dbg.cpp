/*
    Copyright (C) 2007  Hans Verkuil <hverkuil@xs4all.nl>

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

#ifdef ANDROID
#include <android-config.h>
#else
#include <config.h>
#endif

#ifdef HAVE_SYS_KLOG_H
#include <sys/klog.h>
#endif

#include <linux/videodev2.h>

#include <list>
#include <vector>
#include <map>
#include <string>

#include "v4l2-dbg-bttv.h"
#include "v4l2-dbg-saa7134.h"
#include "v4l2-dbg-em28xx.h"
#include "v4l2-dbg-ac97.h"
#include "v4l2-dbg-tvp5150.h"
#include "v4l2-dbg-micron.h"

#define ARRAY_SIZE(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))

struct board_list {
	const char *name;
	int prefix; 		/* Register prefix size */
	const struct board_regs *regs;
	int regs_size;
	const struct board_regs *alt_regs;
	int alt_regs_size;
};

static const struct board_list boards[] = {
#define AC97_BOARD 0
	{				/* From v4l2-dbg-ac97.h */
		AC97_IDENT,
		sizeof(AC97_PREFIX) - 1,
		ac97_regs,
		ARRAY_SIZE(ac97_regs),
		NULL,
		0,
	},
	{				/* From v4l2-dbg-bttv.h */
		BTTV_IDENT,
		sizeof(BTTV_PREFIX) - 1,
		bt8xx_regs,
		ARRAY_SIZE(bt8xx_regs),
		bt8xx_regs_other,
		ARRAY_SIZE(bt8xx_regs_other),
	},
	{				/* From v4l2-dbg-saa7134.h */
		SAA7134_IDENT,
		sizeof(SAA7134_PREFIX) - 1,
		saa7134_regs,
		ARRAY_SIZE(saa7134_regs),
		NULL,
		0,
	},
	{				/* From v4l2-dbg-em28xx.h */
		EM28XX_IDENT,
		sizeof(EM28XX_PREFIX) - 1,
		em28xx_regs,
		ARRAY_SIZE(em28xx_regs),
		em28xx_alt_regs,
		ARRAY_SIZE(em28xx_alt_regs),
	},
	{				/* From v4l2-dbg-tvp5150.h */
		TVP5150_IDENT,
		sizeof(TVP5150_PREFIX) - 1,
		tvp5150_regs,
		ARRAY_SIZE(tvp5150_regs),
		NULL,
		0,
	},
	{				/* From v4l2-dbg-micron.h */
		MT9V011_IDENT,
		sizeof(MT9V011_PREFIX) - 1,
		mt9v011_regs,
		ARRAY_SIZE(mt9v011_regs),
		NULL,
		0,
	},
};

/* Short option list

   Please keep in alphabetical order.
   That makes it easier to see which short options are still free.

   In general the lower case is used to set something and the upper
   case is used to retrieve a setting. */
enum Option {
	OptListRegisters = 'l',
	OptGetRegister = 'g',
	OptSetRegister = 's',
	OptSetDevice = 'd',
	OptGetDriverInfo = 'D',
	OptChip = 'c',
	OptScanChips = 'n',
	OptSetStride = 'w',
	OptHelp = 'h',

	OptLogStatus = 128,
	OptVerbose,
	OptListSymbols,
	OptLast = 256
};

static char options[OptLast];

static unsigned capabilities;

static struct option long_options[] = {
	{"device", required_argument, 0, OptSetDevice},
	{"help", no_argument, 0, OptHelp},
	{"list-registers", optional_argument, 0, OptListRegisters},
	{"get-register", required_argument, 0, OptGetRegister},
	{"set-register", required_argument, 0, OptSetRegister},
	{"chip", required_argument, 0, OptChip},
	{"scan-chips", no_argument, 0, OptScanChips},
	{"info", no_argument, 0, OptGetDriverInfo},
	{"verbose", no_argument, 0, OptVerbose},
	{"log-status", no_argument, 0, OptLogStatus},
	{"list-symbols", no_argument, 0, OptListSymbols},
	{"wide", required_argument, 0, OptSetStride},
	{0, 0, 0, 0}
};

static void usage(void)
{
	printf("Usage: v4l2-dbg [options] [values]\n"
	       "  -D, --info         Show driver info [VIDIOC_QUERYCAP]\n"
	       "  -d, --device=<dev> Use device <dev> instead of /dev/video0\n"
	       "                     If <dev> starts with a digit, then /dev/video<dev> is used\n"
	       "  -h, --help         Display this help message\n"
	       "  --verbose          Turn on verbose ioctl error reporting\n"
	       "  -c, --chip=<chip>  The chip identifier to use with other commands\n"
	       "                     It can be one of:\n"
	       "                         bridge<num>: bridge chip number <num>\n"
	       "                         bridge (default): same as bridge0\n"
	       "                         subdev<num>: sub-device number <num>\n"
	       "  -l, --list-registers[=min=<addr>[,max=<addr>]]\n"
	       "		     Dump registers from <min> to <max> [VIDIOC_DBG_G_REGISTER]\n"
	       "  -g, --get-register=<addr>\n"
	       "		     Get the specified register [VIDIOC_DBG_G_REGISTER]\n"
	       "  -s, --set-register=<addr>\n"
	       "		     Set the register with the commandline arguments\n"
	       "                     The register will autoincrement [VIDIOC_DBG_S_REGISTER]\n"
	       "  -n, --scan-chips   Scan the available bridge and subdev chips [VIDIOC_DBG_G_CHIP_INFO]\n"
	       "  -w, --wide=<reg length>\n"
	       "		     Sets step between two registers\n"
	       "  --list-symbols     List the symbolic register names you can use, if any\n"
	       "  --log-status       Log the board status in the kernel log [VIDIOC_LOG_STATUS]\n");
	exit(0);
}

static std::string cap2s(unsigned cap)
{
	std::string s;

	if (cap & V4L2_CAP_VIDEO_CAPTURE)
		s += "\t\tVideo Capture\n";
	if (cap & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		s += "\t\tVideo Capture Multiplanar\n";
	if (cap & V4L2_CAP_VIDEO_OUTPUT)
		s += "\t\tVideo Output\n";
	if (cap & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
		s += "\t\tVideo Output Multiplanar\n";
	if (cap & V4L2_CAP_VIDEO_M2M)
		s += "\t\tVideo Memory-to-Memory\n";
	if (cap & V4L2_CAP_VIDEO_M2M_MPLANE)
		s += "\t\tVideo Memory-to-Memory Multiplanar\n";
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
	if (cap & V4L2_CAP_RDS_OUTPUT)
		s += "\t\tRDS Output\n";
	if (cap & V4L2_CAP_SDR_CAPTURE)
		s += "\t\tSDR Capture\n";
	if (cap & V4L2_CAP_TOUCH)
		s += "\t\tTouch Device\n";
	if (cap & V4L2_CAP_TUNER)
		s += "\t\tTuner\n";
	if (cap & V4L2_CAP_HW_FREQ_SEEK)
		s += "\t\tHW Frequency Seek\n";
	if (cap & V4L2_CAP_MODULATOR)
		s += "\t\tModulator\n";
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
	if (cap & V4L2_CAP_EXT_PIX_FORMAT)
		s += "\t\tExtended Pix Format\n";
	if (cap & V4L2_CAP_DEVICE_CAPS)
		s += "\t\tDevice Capabilities\n";
	return s;
}

static void print_regs(int fd, struct v4l2_dbg_register *reg, unsigned long min, unsigned long max, int stride)
{
	unsigned long mask;
	unsigned long i;
	int line = 0;

	/* Query size of the first register */
	reg->reg = min;
	if (ioctl(fd, VIDIOC_DBG_G_REGISTER, reg) == 0) {
		/* If size is set, then use this as the stride */
		if (reg->size)
			stride = reg->size;
	}

	mask = stride > 2 ? 0x1f : 0x0f;

	for (i = min & ~mask; i <= max; i += stride) {
		if ((i & mask) == 0 && line % 32 == 0) {
			if (stride == 4)
				printf("\n                00       04       08       0C       10       14       18       1C");
			else if (stride == 2)
				printf("\n            00   02   04   06   08   0A   0C   0E");
			else
				printf("\n          00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
		}

		if ((i & mask) == 0) {
			printf("\n%08lx: ", i);
			line++;
		}
		if (i < min) {
			printf("%*s ", 2 * stride, "");
			continue;
		}
		reg->reg = i;
		if (ioctl(fd, VIDIOC_DBG_G_REGISTER, reg) < 0) {
			perror("ioctl: VIDIOC_DBG_G_REGISTER failed\n");
			break;
		} else {
			printf("%0*llx ", 2 * stride, reg->val);
		}
		usleep(1);
	}
	printf("\n");
}

static void print_name(struct v4l2_dbg_chip_info *chip)
{
	printf("%-10s (%c%c)\n", chip->name,
		(chip->flags & V4L2_CHIP_FL_READABLE) ? 'r' : '-',
		(chip->flags & V4L2_CHIP_FL_WRITABLE) ? 'w' : '-');
}

static unsigned long long parse_reg(const struct board_list *curr_bd, const std::string &reg)
{
	if (curr_bd) {
		for (int i = 0; i < curr_bd->regs_size; i++) {
			if (!strcasecmp(reg.c_str(), curr_bd->regs[i].name) ||
			    !strcasecmp(reg.c_str(), curr_bd->regs[i].name + curr_bd->prefix)) {
				return curr_bd->regs[i].reg;
			}
		}
		for (int i = 0; i < curr_bd->alt_regs_size; i++) {
			if (!strcasecmp(reg.c_str(), curr_bd->alt_regs[i].name) ||
			    !strcasecmp(reg.c_str(), curr_bd->alt_regs[i].name + curr_bd->prefix)) {
				return curr_bd->alt_regs[i].reg;
			}
		}
	}
	return strtoull(reg.c_str(), NULL, 0);
}

static const char *reg_name(const struct board_list *curr_bd, unsigned long long reg)
{
	if (curr_bd) {
		for (int i = 0; i < curr_bd->regs_size; i++) {
			if (reg == curr_bd->regs[i].reg)
				return curr_bd->regs[i].name;
		}
		for (int i = 0; i < curr_bd->alt_regs_size; i++) {
			if (reg == curr_bd->regs[i].reg)
				return curr_bd->regs[i].name;
		}
	}
	return NULL;
}

static const char *binary(unsigned long long val)
{
	static char bin[80];
	char *p = bin;
	int i, j;
	int bits = 64;

	if ((val & 0xffffffff00000000LL) == 0) {
		if ((val & 0xffff0000) == 0) {
			if ((val & 0xff00) == 0)
				bits = 8;
			else
				bits= 16;
		}
		else
			bits = 32;
	}

	for (i = bits - 1; i >= 0; i -= 8) {
		for (j = i; j >= i - 7; j--) {
			if (val & (1LL << j))
				*p++ = '1';
			else
				*p++ = '0';
		}
		*p++ = ' ';
	}
	p[-1] = 0;
	return bin;
}

static int doioctl(int fd, unsigned long int request, void *parm, const char *name)
{
	int retVal = ioctl(fd, request, parm);

	if (options[OptVerbose]) {
		if (retVal < 0)
			printf("%s: failed: %s\n", name, strerror(errno));
		else
			printf("%s: ok\n", name);
	}

	return retVal;
}

static int parse_subopt(char **subs, const char * const *subopts, char **value)
{
	int opt = getsubopt(subs, (char * const *)subopts, value);

	if (opt == -1) {
		fprintf(stderr, "Invalid suboptions specified\n");
		usage();
		exit(1);
	}
	if (*value == NULL) {
		fprintf(stderr, "No value given to suboption <%s>\n",
				subopts[opt]);
		usage();
		exit(1);
	}
	return opt;
}

int main(int argc, char **argv)
{
	char *value, *subs;
	int i, forcedstride = 0;

	int fd = -1;

	/* command args */
	int ch;
	const char *device = "/dev/video0";	/* -d device */
	struct v4l2_capability vcap;	/* list_cap */
	struct v4l2_dbg_register set_reg;
	struct v4l2_dbg_register get_reg;
	struct v4l2_dbg_chip_info chip_info;
	const struct board_list *curr_bd = NULL;
	char short_options[26 * 2 * 3 + 1];
	int idx = 0;
	std::string reg_min_arg, reg_max_arg;
	std::string reg_set_arg;
	unsigned long long reg_min = 0, reg_max = 0;
	std::vector<std::string> get_regs;
	struct v4l2_dbg_match match;
	char *p;

	match.type = V4L2_CHIP_MATCH_BRIDGE;
	match.addr = 0;
	memset(&set_reg, 0, sizeof(set_reg));
	memset(&get_reg, 0, sizeof(get_reg));

	if (argc == 1) {
		usage();
		return 0;
	}
	for (i = 0; long_options[i].name; i++) {
		if (!isalpha(long_options[i].val))
			continue;
		short_options[idx++] = long_options[i].val;
		if (long_options[i].has_arg == required_argument) {
			short_options[idx++] = ':';
		} else if (long_options[i].has_arg == optional_argument) {
			short_options[idx++] = ':';
			short_options[idx++] = ':';
		}
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
			device = optarg;
			if (device[0] >= '0' && device[0] <= '9' && strlen(device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/video%s", device);
				device = newdev;
			}
			break;

		case OptChip:
			if (!memcmp(optarg, "subdev", 6) && isdigit(optarg[6])) {
				match.type = V4L2_CHIP_MATCH_SUBDEV;
				match.addr = strtoul(optarg + 6, NULL, 0);
				break;
			}
			if (!memcmp(optarg, "bridge", 6)) {
				match.type = V4L2_CHIP_MATCH_BRIDGE;
				match.addr = strtoul(optarg + 6, NULL, 0);
				break;
			}
			match.type = V4L2_CHIP_MATCH_BRIDGE;
			match.addr = 0;
			break;

		case OptSetRegister:
			reg_set_arg = optarg;
			break;

		case OptGetRegister:
			get_regs.push_back(optarg);
			break;

		case OptSetStride:
			forcedstride = strtoull(optarg, 0L, 0);
			break;

		case OptListRegisters:
			subs = optarg;
			if (subs == NULL)
				break;

			while (*subs != '\0') {
				static const char * const subopts[] = {
					"min",
					"max",
					NULL
				};

				switch (parse_subopt(&subs, subopts, &value)) {
				case 0:
					reg_min_arg = value;
					//if (reg_max == 0)
					//	reg_max = reg_min + 0xff;
					break;
				case 1:
					reg_max_arg = value;
					break;
				}
			}
			break;

		case OptListSymbols:
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

	if ((fd = open(device, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", device,
			strerror(errno));
		exit(1);
	}

	doioctl(fd, VIDIOC_QUERYCAP, &vcap, "VIDIOC_QUERYCAP");
	capabilities = vcap.capabilities;

	/* Information Opts */

	if (options[OptGetDriverInfo]) {
		printf("Driver info:\n");
		printf("\tDriver name   : %s\n", vcap.driver);
		printf("\tCard type     : %s\n", vcap.card);
		printf("\tBus info      : %s\n", vcap.bus_info);
		printf("\tDriver version: %d.%d.%d\n",
				vcap.version >> 16,
				(vcap.version >> 8) & 0xff,
				vcap.version & 0xff);
		printf("\tCapabilities  : 0x%08X\n", vcap.capabilities);
		printf("%s", cap2s(vcap.capabilities).c_str());
	}

	chip_info.name[0] = '\0';
	if (options[OptChip]) {
		/* try to figure out which chip it is */
		chip_info.match = match;
		if (doioctl(fd, VIDIOC_DBG_G_CHIP_INFO, &chip_info, "VIDIOC_DBG_G_CHIP_INFO") != 0)
			chip_info.name[0] = '\0';

		if (!strncasecmp(match.name, "ac97", 4)) {
			curr_bd = &boards[AC97_BOARD];
		} else {
			for (int board = ARRAY_SIZE(boards) - 1; board >= 0; board--) {
				if (!strcasecmp(chip_info.name, boards[board].name)) {
					curr_bd = &boards[board];
					break;
				}
			}
		}
	}

	/* Set options */

	if (options[OptSetRegister]) {
		set_reg.match = match;
		if (optind >= argc)
			usage();
		set_reg.reg = parse_reg(curr_bd, reg_set_arg);
		while (optind < argc) {
			unsigned size = 0;

			if (doioctl(fd, VIDIOC_DBG_G_REGISTER, &set_reg,
				    "VIDIOC_DBG_G_REGISTER") >= 0)
				size = set_reg.size;

			set_reg.val = strtoull(argv[optind++], NULL, 0);
			if (doioctl(fd, VIDIOC_DBG_S_REGISTER, &set_reg,
						"VIDIOC_DBG_S_REGISTER") >= 0) {
				const char *name = reg_name(curr_bd, set_reg.reg);

				printf("Register ");

				if (name)
					printf("%s (0x%08llx)", name, set_reg.reg);
				else
					printf("0x%08llx", set_reg.reg);

				printf(" set to 0x%llx\n", set_reg.val);
			} else {
				printf("Failed to set register 0x%08llx value 0x%llx: %s\n",
					set_reg.reg, set_reg.val, strerror(errno));
			}
			set_reg.reg += size ? : (forcedstride ? : 1);
		}
	}

	if (options[OptScanChips]) {
		chip_info.match.type = V4L2_CHIP_MATCH_BRIDGE;
		chip_info.match.addr = 0;

		while (doioctl(fd, VIDIOC_DBG_G_CHIP_INFO, &chip_info, "VIDIOC_DBG_G_CHIP_INFO") == 0 && chip_info.name[0]) {
			printf("bridge%d: ", chip_info.match.addr);
			print_name(&chip_info);
			chip_info.match.addr++;
		}

		chip_info.match.type = V4L2_CHIP_MATCH_SUBDEV;
		chip_info.match.addr = 0;
		while (doioctl(fd, VIDIOC_DBG_G_CHIP_INFO, &chip_info, "VIDIOC_DBG_G_CHIP_INFO") == 0 && chip_info.name[0]) {
			printf("subdev%d: ", chip_info.match.addr++);
			print_name(&chip_info);
		}
	}

	if (options[OptGetRegister]) {
		get_reg.match = match;
		printf("ioctl: VIDIOC_DBG_G_REGISTER\n");

		for (std::vector<std::string>::iterator iter = get_regs.begin();
				iter != get_regs.end(); ++iter) {
			get_reg.reg = parse_reg(curr_bd, *iter);
			if (ioctl(fd, VIDIOC_DBG_G_REGISTER, &get_reg) < 0)
				fprintf(stderr, "ioctl: VIDIOC_DBG_G_REGISTER "
						"failed for 0x%llx\n", get_reg.reg);
			else {
				const char *name = reg_name(curr_bd, get_reg.reg);

				printf("Register ");

				if (name)
					printf("%s (0x%08llx)", name, get_reg.reg);
				else
					printf("0x%08llx", get_reg.reg);

				printf(" = %llxh (%lldd  %sb)\n",
					get_reg.val, get_reg.val, binary(get_reg.val));
			}
		}
	}

	if (options[OptListRegisters]) {
		std::string name;
		int stride = 1;

		get_reg.match = match;
		if (forcedstride) {
			stride = forcedstride;
		} else if (get_reg.match.type == V4L2_CHIP_MATCH_BRIDGE) {
			stride = 4;
		}
		printf("ioctl: VIDIOC_DBG_G_REGISTER\n");

		if (curr_bd) {
			if (reg_min_arg.empty())
				reg_min = 0;
			else
				reg_min = parse_reg(curr_bd, reg_min_arg);


			if (reg_max_arg.empty())
				reg_max = (1ll << 32) - 1;
			else
				reg_max = parse_reg(curr_bd, reg_max_arg);

			for (int i = 0; i < curr_bd->regs_size; i++) {
				if (reg_min_arg.empty() || ((curr_bd->regs[i].reg >= reg_min) && curr_bd->regs[i].reg <= reg_max)) {
					get_reg.reg = curr_bd->regs[i].reg;

					if (ioctl(fd, VIDIOC_DBG_G_REGISTER, &get_reg) < 0)
						fprintf(stderr, "ioctl: VIDIOC_DBG_G_REGISTER "
								"failed for 0x%llx\n", get_reg.reg);
					else {
						const char *name = reg_name(curr_bd, get_reg.reg);

						printf("Register ");

						if (name)
							printf("%s (0x%08llx)", name, get_reg.reg);
						else
							printf("0x%08llx", get_reg.reg);

						printf(" = %llxh (%lldd  %sb)\n",
							get_reg.val, get_reg.val, binary(get_reg.val));
					}
				}
			}
			goto list_done;
		}

		if (!reg_min_arg.empty()) {
			reg_min = parse_reg(curr_bd, reg_min_arg);
			if (reg_max_arg.empty())
				reg_max = reg_min + 0xff;
			else
				reg_max = parse_reg(curr_bd, reg_max_arg);
			/* Explicit memory range: just do it */
			print_regs(fd, &get_reg, reg_min, reg_max, stride);
			goto list_done;
		}

		p = strchr(chip_info.name, ' ');
		if (p)
			*p = '\0';
		name = chip_info.name;

		if (name == "saa7115") {
			print_regs(fd, &get_reg, 0, 0xff, stride);
		} else if (name == "saa717x") {
			// FIXME: use correct reg regions
			print_regs(fd, &get_reg, 0, 0xff, stride);
		} else if (name == "saa7127") {
			print_regs(fd, &get_reg, 0, 0x7f, stride);
		} else if (name == "ov7670") {
			print_regs(fd, &get_reg, 0, 0x89, stride);
		} else if (name == "cx25840") {
			print_regs(fd, &get_reg, 0, 2, stride);
			print_regs(fd, &get_reg, 0x100, 0x15f, stride);
			print_regs(fd, &get_reg, 0x200, 0x23f, stride);
			print_regs(fd, &get_reg, 0x400, 0x4bf, stride);
			print_regs(fd, &get_reg, 0x800, 0x9af, stride);
		} else if (name == "cs5345") {
			print_regs(fd, &get_reg, 1, 0x10, stride);
		} else if (name == "cx23416") {
			print_regs(fd, &get_reg, 0x02000000, 0x020000ff, stride);
		} else if (name == "cx23418") {
			print_regs(fd, &get_reg, 0x02c40000, 0x02c409c7, stride);
		} else if (name == "cafe") {
			print_regs(fd, &get_reg, 0, 0x43, stride);
			print_regs(fd, &get_reg, 0x88, 0x8f, stride);
			print_regs(fd, &get_reg, 0xb4, 0xbb, stride);
			print_regs(fd, &get_reg, 0x3000, 0x300c, stride);
		} else {
			/* unknown chip, dump 0-0xff by default */
			print_regs(fd, &get_reg, 0, 0xff, stride);
		}
	}
list_done:

	if (options[OptLogStatus]) {
		static char buf[40960];
		int len = -1;

		if (doioctl(fd, VIDIOC_LOG_STATUS, NULL, "VIDIOC_LOG_STATUS") == 0) {
			printf("\nStatus Log:\n\n");
#ifdef HAVE_KLOGCTL
			len = klogctl(3, buf, sizeof(buf) - 1);
#endif
			if (len >= 0) {
				bool found_status = false;
				char *p = buf;
				char *q;

				buf[len] = 0;
				while ((q = strstr(p, "START STATUS"))) {
					found_status = true;
					p = q + 1;
				}
				if (found_status) {
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

	if (options[OptListSymbols]) {
		if (curr_bd == NULL) {
			printf("No symbols found for driver %s\n", vcap.driver);
		}
		else {
			printf("Symbols for driver %s:\n", curr_bd->name);
			for (int i = 0; i < curr_bd->regs_size; i++)
				printf("0x%08x: %s\n", curr_bd->regs[i].reg, curr_bd->regs[i].name);
			for (int i = 0; i < curr_bd->alt_regs_size; i++)
				printf("0x%08x: %s\n", curr_bd->alt_regs[i].reg, curr_bd->alt_regs[i].name);
		}
	}

	close(fd);
	exit(0);
}
