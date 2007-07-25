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
#include <linux/i2c-id.h>

#include <list>
#include <vector>
#include <map>
#include <string>

struct driverid {
	const char *name;
	unsigned id;
};

extern struct driverid driverids[];

struct chipid {
	const char *name;
	unsigned id;
};

extern struct chipid chipids[];

/* Short option list

   Please keep in alphabetical order.
   That makes it easier to see which short options are still free.

   In general the lower case is used to set something and the upper
   case is used to retrieve a setting. */
enum Option {
	OptListRegisters = 'R',
	OptSetRegister = 'r',
	OptSetSlicedVbiFormat = 'b',
	OptSetDevice = 'd',
	OptGetDriverInfo = 'D',
	OptScanChipIdents = 'C',
	OptGetChipIdent = 'c',
	OptHelp = 'h',

	OptLogStatus = 128,
	OptVerbose,
	OptListDriverIDs,
	OptLast = 256
};

static char options[OptLast];

static unsigned capabilities;

static struct option long_options[] = {
	{"device", required_argument, 0, OptSetDevice},
	{"help", no_argument, 0, OptHelp},
	{"list-registers", required_argument, 0, OptListRegisters},
	{"set-register", required_argument, 0, OptSetRegister},
	{"scan-chip-idents", no_argument, 0, OptScanChipIdents},
	{"get-chip-ident", required_argument, 0, OptGetChipIdent},
	{"info", no_argument, 0, OptGetDriverInfo},
	{"verbose", no_argument, 0, OptVerbose},
	{"log-status", no_argument, 0, OptLogStatus},
	{"list-driverids", no_argument, 0, OptListDriverIDs},
	{0, 0, 0, 0}
};

static void usage(void)
{
	printf("Usage:\n");
	printf("  -D, --info         show driver info [VIDIOC_QUERYCAP]\n");
	printf("  -d, --device=<dev> use device <dev> instead of /dev/video0\n");
	printf("                     if <dev> is a single digit, then /dev/video<dev> is used\n");
	printf("  -h, --help         display this help message\n");
	printf("  --verbose          turn on verbose ioctl error reporting.\n");
	printf("  -R, --list-registers=type=<host/i2cdrv/i2caddr>,chip=<chip>[,min=<addr>,max=<addr>] \n");
	printf("		     dump registers from <min> to <max> [VIDIOC_DBG_G_REGISTER]\n");
	printf("  -r, --set-register=type=<host/i2cdrv/i2caddr>,chip=<chip>,reg=<addr>,val=<val>\n");
	printf("		     set the register [VIDIOC_DBG_S_REGISTER]\n");
	printf("  -C, --scan-chip-idents\n");
	printf("		     Scan the available host and i2c chips [VIDIOC_G_CHIP_IDENT]\n");
	printf("  -c, --get-chip-ident=type=<host/i2cdrv/i2caddr>,chip=<chip>\n");
	printf("		     Get the chip identifier [VIDIOC_G_CHIP_IDENT]\n");
	printf("  --log-status       log the board status in the kernel log [VIDIOC_LOG_STATUS]\n");
	printf("  --list-driverids   list the known I2C driver IDs for use with the i2cdrv type\n");
	printf("\n");
	printf("  if type == host, then <chip> is the host's chip ID (default 0)\n");
	printf("  if type == i2cdrv (default), then <chip> is the I2C driver name or ID\n");
	printf("  if type == i2caddr, then <chip> is the 7-bit I2C address\n");
	exit(0);
}

static unsigned parse_type(const std::string &s)
{
	if (s == "host") return V4L2_CHIP_MATCH_HOST;
	if (s == "i2caddr") return V4L2_CHIP_MATCH_I2C_ADDR;
	return V4L2_CHIP_MATCH_I2C_DRIVER;
}

static unsigned parse_chip(int type, const std::string &s)
{
	if (type == V4L2_CHIP_MATCH_HOST || type == V4L2_CHIP_MATCH_I2C_ADDR || isdigit(s[0]))
		return strtoul(s.c_str(), 0, 0);
	for (int i = 0; driverids[i].name; i++)
		if (!strcasecmp(s.c_str(), driverids[i].name))
			return driverids[i].id;
	return 0;
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

static void print_regs(int fd, struct v4l2_register *reg, unsigned long min, unsigned long max, int stride)
{
	unsigned long mask = stride > 1 ? 0x1f : 0x0f;
	unsigned long i;
	int line = 0;

	for (i = min & ~mask; i <= max; i += stride) {
		if ((i & mask) == 0 && line % 32 == 0) {
			if (stride == 4)
				printf("\n                00       04       08       0C       10       14       18       1C");
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
			fprintf(stderr, "ioctl: VIDIOC_DBG_G_REGISTER "
					"failed for 0x%llx\n", reg->reg);
		} else {
			printf("%0*llx ", 2 * stride, reg->val);
		}
		usleep(1);
	}
	printf("\n");
}

static void print_chip(struct v4l2_chip_ident *chip)
{
	const char *name = NULL;

	for (int i = 0; chipids[i].name; i++) {
		if (chipids[i].id == chip->ident) {
			name = chipids[i].name;
			break;
		}
	}
	if (name)
		printf("%-10s revision 0x%08x\n", name, chip->revision);
	else
		printf("%-10d revision 0x%08x\n", chip->ident, chip->revision);
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

int main(int argc, char **argv)
{
	char *value, *subs;
	int i;

	int fd = -1;

	/* command args */
	int ch;
	char *device = strdup("/dev/video0");	/* -d device */
	struct v4l2_capability vcap;	/* list_cap */
	struct v4l2_register set_reg;
	struct v4l2_register get_reg;
	struct v4l2_chip_ident chip_id;
	char short_options[26 * 2 * 2 + 1];
	int idx = 0;
	unsigned long long reg_min = 0, reg_max = 0;

	memset(&set_reg, 0, sizeof(set_reg));
	memset(&get_reg, 0, sizeof(get_reg));
	memset(&chip_id, 0, sizeof(chip_id));

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
		case OptSetRegister:
			subs = optarg;
			set_reg.match_type = V4L2_CHIP_MATCH_I2C_DRIVER;
			while (*subs != '\0') {
				static char *const subopts[] = {
					"type",
					"chip",
					"reg",
					"val",
					NULL
				};

				switch (parse_subopt(&subs, subopts, &value)) {
				case 0:
					set_reg.match_type = parse_type(value);
					break;
				case 1:
					set_reg.match_chip = parse_chip(set_reg.match_type, value);
					break;
				case 2:
					set_reg.reg = strtoull(value, 0L, 0);
					break;
				case 3:
					set_reg.val = strtoull(value, 0L, 0);
					break;
				}
			}
			break;
		case OptListRegisters:
			subs = optarg;
			get_reg.match_type = V4L2_CHIP_MATCH_I2C_DRIVER;
			while (*subs != '\0') {
				static char *const subopts[] = {
					"type",
					"chip",
					"min",
					"max",
					NULL
				};

				switch (parse_subopt(&subs, subopts, &value)) {
				case 0:
					get_reg.match_type = parse_type(value);
					break;
				case 1:
					get_reg.match_chip = parse_chip(get_reg.match_type, value);
					break;
				case 2:
					reg_min = strtoull(value, 0L, 0);
					break;
				case 3:
					reg_max = strtoull(value, 0L, 0);
					break;
				}
			}
			break;
		case OptGetChipIdent:
			subs = optarg;
			set_reg.match_type = V4L2_CHIP_MATCH_I2C_DRIVER;
			while (*subs != '\0') {
				static char *const subopts[] = {
					"type",
					"chip",
					NULL
				};

				switch (parse_subopt(&subs, subopts, &value)) {
				case 0:
					chip_id.match_type = parse_type(value);
					break;
				case 1:
					chip_id.match_chip = parse_chip(chip_id.match_type, value);
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

	if ((fd = open(device, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", device,
			strerror(errno));
		exit(1);
	}
	free(device);

	doioctl(fd, VIDIOC_QUERYCAP, &vcap, "VIDIOC_QUERYCAP");
	capabilities = vcap.capabilities;

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

	if (options[OptSetRegister]) {
		if (doioctl(fd, VIDIOC_DBG_S_REGISTER, &set_reg,
			"VIDIOC_DBG_S_REGISTER") == 0)
			printf("register 0x%llx set to 0x%llx\n", set_reg.reg, set_reg.val);
	}

	if (options[OptGetChipIdent]) {
		if (doioctl(fd, VIDIOC_G_CHIP_IDENT, &chip_id, "VIDIOC_G_CHIP_IDENT") == 0)
			print_chip(&chip_id);
	}

	if (options[OptScanChipIdents]) {
		int i;

		chip_id.match_type = V4L2_CHIP_MATCH_HOST;
		chip_id.match_chip = 0;

		while (doioctl(fd, VIDIOC_G_CHIP_IDENT, &chip_id, "VIDIOC_G_CHIP_IDENT") == 0 && chip_id.ident) {
			printf("host 0x%x: ", chip_id.match_chip);
			print_chip(&chip_id);
			chip_id.match_chip++;
		}

		chip_id.match_type = V4L2_CHIP_MATCH_I2C_ADDR;
		for (i = 0; i < 128; i++) {
			chip_id.match_chip = i;
			if (doioctl(fd, VIDIOC_G_CHIP_IDENT, &chip_id, "VIDIOC_G_CHIP_IDENT") == 0 && chip_id.ident) {
				printf("i2c 0x%02x: ", i);
				print_chip(&chip_id);
			}
		}
	}

	if (options[OptListRegisters]) {
		int stride = 1;

		if (get_reg.match_type == V4L2_CHIP_MATCH_HOST) stride = 4;
		printf("ioctl: VIDIOC_DBG_G_REGISTER\n");
		if (reg_max == 0) {
			switch (get_reg.match_chip) {
			case I2C_DRIVERID_SAA711X:
				print_regs(fd, &get_reg, 0, 0xff, stride);
				break;
			case I2C_DRIVERID_SAA717X:
				// FIXME: use correct reg regions
				print_regs(fd, &get_reg, 0, 0xff, stride);
				break;
			case I2C_DRIVERID_SAA7127:
				print_regs(fd, &get_reg, 0, 0x7f, stride);
				break;
			case I2C_DRIVERID_CX25840:
				print_regs(fd, &get_reg, 0, 2, stride);
				print_regs(fd, &get_reg, 0x100, 0x15f, stride);
				print_regs(fd, &get_reg, 0x200, 0x23f, stride);
				print_regs(fd, &get_reg, 0x400, 0x4bf, stride);
				print_regs(fd, &get_reg, 0x800, 0x9af, stride);
				break;
			case 0:
				print_regs(fd, &get_reg, 0x02000000, 0x020000ff, stride);
				break;
			}
		}
		else {
			print_regs(fd, &get_reg, reg_min, reg_max, stride);
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

	if (options[OptListDriverIDs]) {
		printf("Known I2C driver IDs:\n");
		for (int i = 0; driverids[i].name; i++)
			printf("%s\n", driverids[i].name);
	}

	close(fd);
	exit(0);
}
