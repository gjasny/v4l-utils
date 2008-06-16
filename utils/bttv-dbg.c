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

struct bt8xx_regs {
	unsigned int reg;
	char *name;
	int size;
};

static struct bt8xx_regs bt8xx_regs_other[] = {
	{0x000, "BT848_DSTATUS", 1},
	{0x054, "BT848_TEST", 1},
	{0x060, "BT848_ADELAY", 1},
	{0x064, "BT848_BDELAY", 1},
	{0x07C, "BT848_SRESET", 1},
	{0x100, "BT848_INT_STAT", 1},
	{0x110, "BT848_I2C", 1},
	{0x11C, "BT848_GPIO_REG_INP", 1},
	{0x120, "BT848_RISC_COUNT", 1},

	/* This is also defined at bt8xx_regs with other name */
	{0x0fc, "BT848_VBI_PACK_DEL_VBI_HDELAY", 1},
};

static struct bt8xx_regs bt8xx_regs[] = {
	{0x004, "BT848_IFORM", 1},
	{0x008, "BT848_TDEC", 1},
	{0x00C, "BT848_E_CROP", 1},
	{0x08C, "BT848_O_CROP", 1},
	{0x010, "BT848_E_VDELAY_LO", 1},
	{0x090, "BT848_O_VDELAY_LO", 1},
	{0x014, "BT848_E_VACTIVE_LO", 1},
	{0x094, "BT848_O_VACTIVE_LO", 1},
	{0x018, "BT848_E_HDELAY_LO", 1},
	{0x098, "BT848_O_HDELAY_LO", 1},
	{0x01C, "BT848_E_HACTIVE_LO", 1},
	{0x09C, "BT848_O_HACTIVE_LO", 1},
	{0x020, "BT848_E_HSCALE_HI", 1},
	{0x0A0, "BT848_O_HSCALE_HI", 1},
	{0x024, "BT848_E_HSCALE_LO", 1},
	{0x0A4, "BT848_O_HSCALE_LO", 1},
	{0x028, "BT848_BRIGHT", 1},
	{0x02C, "BT848_E_CONTROL", 1},
	{0x0AC, "BT848_O_CONTROL", 1},
	{0x030, "BT848_CONTRAST_LO", 1},
	{0x034, "BT848_SAT_U_LO", 1},
	{0x038, "BT848_SAT_V_LO", 1},
	{0x03C, "BT848_HUE", 1},
	{0x040, "BT848_E_SCLOOP", 1},
	{0x0C0, "BT848_O_SCLOOP", 1},
	{0x048, "BT848_OFORM", 1},
	{0x04C, "BT848_E_VSCALE_HI", 1},
	{0x0CC, "BT848_O_VSCALE_HI", 1},
	{0x050, "BT848_E_VSCALE_LO", 1},
	{0x0D0, "BT848_O_VSCALE_LO", 1},
	{0x068, "BT848_ADC", 1},
	{0x044, "BT848_WC_UP", 1},
	{0x078, "BT848_WC_DOWN", 1},
	{0x06C, "BT848_E_VTC", 1},
	{0x080, "BT848_TGCTRL", 1},
	{0x0EC, "BT848_O_VTC", 1},
	{0x0D4, "BT848_COLOR_FMT", 1},
	{0x0B0, "BT848_VTOTAL_LO", 1},
	{0x0B4, "BT848_VTOTAL_HI", 1},
	{0x0D8, "BT848_COLOR_CTL", 1},
	{0x0DC, "BT848_CAP_CTL", 1},
	{0x0E0, "BT848_VBI_PACK_SIZE", 1},
	{0x0E4, "BT848_VBI_PACK_DEL", 1},
	{0x0E8,	"BT848_FCNTR", 1},

	{0x0F0, "BT848_PLL_F_LO", 1},
	{0x0F4, "BT848_PLL_F_HI", 1},
	{0x0F8, "BT848_PLL_XCI", 1},

	{0x0FC, "BT848_DVSIF", 1},

	{0x104, "BT848_INT_MASK", 4},
	{0x10C, "BT848_GPIO_DMA_CTL", 2},
	{0x114, "BT848_RISC_STRT_ADD", 4},
	{0x118, "BT848_GPIO_OUT_EN", 4},
	{0x11a, "BT848_GPIO_OUT_EN_HIBYTE", 4},
	{0x200, "BT848_GPIO_DATA", 4},
};

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
		struct bt8xx_regs *bt_reg;

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
