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
    Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#include "v4l2-dbg.h"

#define EM28XX_IDENT "em28xx"

/* Register name prefix */
#define EM2800_PREFIX "EM2800_"
#define EM2874_PREFIX "EM2874_"
#define EM2880_PREFIX "EM2880_"
#define EM28XX_PREFIX "EM28XX_"

static struct board_regs em28xx_regs[] = {
	{0x00, EM28XX_PREFIX "CHIPCFG", 1},
	{0x04, EM2880_PREFIX "GPO", 1},
	{0x08, EM28XX_PREFIX "GPIO", 1},

	{0x06, EM28XX_PREFIX "I2C_CLK", 1},
	{0x0a, EM28XX_PREFIX "CHIPID", 1},
	{0x0c, EM28XX_PREFIX "USBSUSP", 1},

	{0x0e, EM28XX_PREFIX "AUDIOSRC", 1},
	{0x0f, EM28XX_PREFIX "XCLK", 1},

	{0x10, EM28XX_PREFIX "VINMODE", 1},
	{0x11, EM28XX_PREFIX "VINCTRL", 1},
	{0x12, EM28XX_PREFIX "VINENABLE", 1},

	{0x14, EM28XX_PREFIX "GAMMA", 1},
	{0x15, EM28XX_PREFIX "RGAIN", 1},
	{0x16, EM28XX_PREFIX "GGAIN", 1},
	{0x17, EM28XX_PREFIX "BGAIN", 1},
	{0x18, EM28XX_PREFIX "ROFFSET", 1},
	{0x19, EM28XX_PREFIX "GOFFSET", 1},
	{0x1a, EM28XX_PREFIX "BOFFSET", 1},

	{0x1b, EM28XX_PREFIX "OFLOW", 1},
	{0x1c, EM28XX_PREFIX "HSTART", 1},
	{0x1d, EM28XX_PREFIX "VSTART", 1},
	{0x1e, EM28XX_PREFIX "CWIDTH", 1},
	{0x1f, EM28XX_PREFIX "CHEIGHT", 1},

	{0x20, EM28XX_PREFIX "YGAIN", 1},
	{0x21, EM28XX_PREFIX "YOFFSET", 1},
	{0x22, EM28XX_PREFIX "UVGAIN", 1},
	{0x23, EM28XX_PREFIX "UOFFSET", 1},
	{0x24, EM28XX_PREFIX "VOFFSET", 1},
	{0x25, EM28XX_PREFIX "SHARPNESS", 1},

	{0x26, EM28XX_PREFIX "COMPR", 1},
	{0x27, EM28XX_PREFIX "OUTFMT", 1},

	{0x28, EM28XX_PREFIX "XMIN", 1},
	{0x29, EM28XX_PREFIX "XMAX", 1},
	{0x2a, EM28XX_PREFIX "YMIN", 1},
	{0x2b, EM28XX_PREFIX "YMAX", 1},

	{0x30, EM28XX_PREFIX "HSCALELOW", 1},
	{0x31, EM28XX_PREFIX "HSCALEHIGH", 1},
	{0x32, EM28XX_PREFIX "VSCALELOW", 1},
	{0x33, EM28XX_PREFIX "VSCALEHIGH", 1},

	{0x40, EM28XX_PREFIX "AC97LSB", 1},
	{0x41, EM28XX_PREFIX "AC97MSB", 1},
	{0x42, EM28XX_PREFIX "AC97ADDR", 1},
	{0x43, EM28XX_PREFIX "AC97BUSY", 1},

	{0x45, EM28XX_PREFIX "IR", 1},

	{0x50, EM2874_PREFIX "IR_CONFIG", 1},
	{0x51, EM2874_PREFIX "IR", 1},
	{0x5f, EM2874_PREFIX "TS_ENABLE", 1},
	{0x80, EM2874_PREFIX "GPIO", 1},
};

static struct board_regs em28xx_alt_regs[] = {
	{0x08, EM2800_PREFIX "AUDIOSRC", 1},
};
