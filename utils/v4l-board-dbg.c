/*
    Copyright (C) 2008 Mauro Carvalho Chehab <mchehab@infradead.org>
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/types.h>
#include <linux/videodev2.h>

#include "bttv-dbg.h"

#define ARRAY_SIZE(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))

static int is_get=0, is_set=0;

static int doioctl(int fd, int request, void *parm, const char *name)
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

static void usage(void)
{
	printf("bttv-dbg <args>\n");
}

enum Option {
	OptGetReg = 'g',
	OptSetReg = 's',
	OptHelp   = 'h',
};

static void print_bin (int val, int size)
{
	int i, j, v;

	printf("(");
	for (i = size-1; i >= 0; i--) {
		v = (val >> (i * 8)) & 0xff;
		for (j = 7; j >= 0; j--) {
			int bit = (v >> j) & 0x1;
			if (bit)
				printf("1");
			else
				printf("0");
		}
		if (i)
			printf(" ");
		else
			printf(")");
	}
}

int main(int argc, char **argv)
{
	char *device = strdup("/dev/video0");
	char *reg_set = NULL;
	int ch;
	int i;
	int fd = -1;
	struct v4l2_register reg;
	struct option long_options[] = {
		/* Please keep in alphabetical order of the short option.
		That makes it easier to see which options are still free. */
		{"get-reg", no_argument, 0, OptGetReg},
		{"set-reg", required_argument, 0, OptSetReg},
		{"help",    no_argument, 0, OptHelp},
		{0, 0, 0, 0}
	};

	/* command args */
	if (argc == 1) {
		usage();
		return 0;
	}
	while (1) {
		int option_index = 0;

		ch = getopt_long(argc, argv, "gs:", long_options, &option_index);
		if (ch == -1)
			break;

		switch (ch) {
		case OptHelp:
			usage();
			return 0;
		case OptGetReg:
			is_get++;
			break;
		case OptSetReg:
			is_set++;
			reg_set = optarg;

			break;
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
	free(device);

	reg.match_type = V4L2_CHIP_MATCH_HOST;
	reg.match_chip = 0;

	if (is_get) {
		for (i = 0; i < ARRAY_SIZE(bt8xx_regs); i++) {
			char name[256];
			reg.reg = bt8xx_regs[i].reg;
			if (ioctl(fd, VIDIOC_DBG_G_REGISTER, &reg) < 0) {
				printf("Error while reading\n");
				continue;
			}
			sprintf(name, "%s:", bt8xx_regs[i].name);

			switch (bt8xx_regs[i].size) {
			case 1:
				printf("%-32s %02llx         ", name, reg.val & 0xff);
				break;
			case 2:
				printf("%-32s %04llx       ", name, reg.val & 0xffff);
				break;
			case 4:
				printf("%-32s %08llx   ", name, reg.val & 0xffffffff);
				break;
			}
			print_bin (reg.val, bt8xx_regs[i].size);
			printf("\n");
		}
		return 0;
	}

	if (is_set) {
		char *reg_name;
		int  val;
		int r, size;
		unsigned prev;
		struct board_regs *bt_reg;

		reg_name = strtok(reg_set, "=:");
		val = strtol(strtok(NULL, "=:"), 0L, 0);

		if (!reg_name) {
			printf("set argument is invalid\n");
			return -1;
		}


		for (i = ARRAY_SIZE(bt8xx_regs) - 1; i >=0 ; i--) {
			if (!strcasecmp(reg_name, bt8xx_regs[i].name)) {
				bt_reg = &bt8xx_regs[i];
				r    = bt8xx_regs[i].reg;
				size = bt8xx_regs[i].size;
				break;
			}
		}

		if (i < 0) {
			for (i = ARRAY_SIZE(bt8xx_regs_other) - 1; i >=0 ; i--) {
				if (!strcasecmp(reg_name, bt8xx_regs_other[i].name)) {
					bt_reg = &bt8xx_regs_other[i];
					r    = bt8xx_regs_other[i].reg;
					size = bt8xx_regs_other[i].size;
					break;
				}
			}
		}

		if (i < 0) {
			for (i = ARRAY_SIZE(bt8xx_regs) - 1; i >=0 ; i--) {
				if (!strcasecmp(reg_name, bt8xx_regs[i].name+6)) {
					bt_reg = &bt8xx_regs[i];
					r    = bt8xx_regs[i].reg;
					size = bt8xx_regs[i].size;
					break;
				}
			}
		}

		if (i < 0) {
			for (i = ARRAY_SIZE(bt8xx_regs_other) - 1; i >=0 ; i--) {
				if (!strcasecmp(reg_name, bt8xx_regs_other[i].name+6)) {
					bt_reg = &bt8xx_regs_other[i];
					r    = bt8xx_regs_other[i].reg;
					size = bt8xx_regs_other[i].size;
					break;
				}
			}
		}


		if (i < 0) {
			printf("Register not found\n");
			return -1;
		}

		reg.reg = r;
		if (ioctl(fd, VIDIOC_DBG_G_REGISTER, &reg) < 0) {
			printf("Error while reading register 0x%02x\n", r);
			return -1;
		}
		prev = reg.val;

		switch (size) {
		case 1:
			reg.val = (reg.val & (~0xff)) | val;
			break;
		case 2:
			reg.val = (reg.val & (~0xffff)) | val;
			break;
		case 4:
			reg.val = val;
			break;
		}

		printf("Changing value of register %s(0x%x) from 0x%02x to 0x%02x\n",
			bt_reg->name, r, prev, (unsigned int)reg.val);

		prev = reg.val;

		if (ioctl(fd, VIDIOC_DBG_S_REGISTER, &reg) < 0) {
			printf("Error while writing\n");
			return -1;
		}
		if (ioctl(fd, VIDIOC_DBG_G_REGISTER, &reg) < 0) {
			printf("Error while reading register 0x%02x\n", r);
			return -1;
		}
		if (reg.val != prev) {
			printf("Value of register %s(0x%x) is now 0x%02x\n",
			bt_reg->name, r, (unsigned int)reg.val);
		}

	}

	close(fd);
	exit(0);
}
