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

#define BTTV_IDENT "bttv"

/* Register name prefix */
#define BTTV_PREFIX "BT848_"

static struct board_regs bt8xx_regs_other[] = {
	{0x000, BTTV_PREFIX "DSTATUS", 1},
	{0x054, BTTV_PREFIX "TEST", 1},
	{0x060, BTTV_PREFIX "ADELAY", 1},
	{0x064, BTTV_PREFIX "BDELAY", 1},
	{0x07C, BTTV_PREFIX "SRESET", 1},
	{0x100, BTTV_PREFIX "INT_STAT", 1},
	{0x110, BTTV_PREFIX "I2C", 1},
	{0x11C, BTTV_PREFIX "GPIO_REG_INP", 1},
	{0x120, BTTV_PREFIX "RISC_COUNT", 1},

	/* This is also defined at bt8xx_regs with other name */
	{0x0fc, BTTV_PREFIX "VBI_PACK_DEL_VBI_HDELAY", 1},
};

static struct board_regs bt8xx_regs[] = {
	{0x004, BTTV_PREFIX "IFORM", 1},
	{0x008, BTTV_PREFIX "TDEC", 1},
	{0x00C, BTTV_PREFIX "E_CROP", 1},
	{0x08C, BTTV_PREFIX "O_CROP", 1},
	{0x010, BTTV_PREFIX "E_VDELAY_LO", 1},
	{0x090, BTTV_PREFIX "O_VDELAY_LO", 1},
	{0x014, BTTV_PREFIX "E_VACTIVE_LO", 1},
	{0x094, BTTV_PREFIX "O_VACTIVE_LO", 1},
	{0x018, BTTV_PREFIX "E_HDELAY_LO", 1},
	{0x098, BTTV_PREFIX "O_HDELAY_LO", 1},
	{0x01C, BTTV_PREFIX "E_HACTIVE_LO", 1},
	{0x09C, BTTV_PREFIX "O_HACTIVE_LO", 1},
	{0x020, BTTV_PREFIX "E_HSCALE_HI", 1},
	{0x0A0, BTTV_PREFIX "O_HSCALE_HI", 1},
	{0x024, BTTV_PREFIX "E_HSCALE_LO", 1},
	{0x0A4, BTTV_PREFIX "O_HSCALE_LO", 1},
	{0x028, BTTV_PREFIX "BRIGHT", 1},
	{0x02C, BTTV_PREFIX "E_CONTROL", 1},
	{0x0AC, BTTV_PREFIX "O_CONTROL", 1},
	{0x030, BTTV_PREFIX "CONTRAST_LO", 1},
	{0x034, BTTV_PREFIX "SAT_U_LO", 1},
	{0x038, BTTV_PREFIX "SAT_V_LO", 1},
	{0x03C, BTTV_PREFIX "HUE", 1},
	{0x040, BTTV_PREFIX "E_SCLOOP", 1},
	{0x0C0, BTTV_PREFIX "O_SCLOOP", 1},
	{0x048, BTTV_PREFIX "OFORM", 1},
	{0x04C, BTTV_PREFIX "E_VSCALE_HI", 1},
	{0x0CC, BTTV_PREFIX "O_VSCALE_HI", 1},
	{0x050, BTTV_PREFIX "E_VSCALE_LO", 1},
	{0x0D0, BTTV_PREFIX "O_VSCALE_LO", 1},
	{0x068, BTTV_PREFIX "ADC", 1},
	{0x044, BTTV_PREFIX "WC_UP", 1},
	{0x078, BTTV_PREFIX "WC_DOWN", 1},
	{0x06C, BTTV_PREFIX "E_VTC", 1},
	{0x080, BTTV_PREFIX "TGCTRL", 1},
	{0x0EC, BTTV_PREFIX "O_VTC", 1},
	{0x0D4, BTTV_PREFIX "COLOR_FMT", 1},
	{0x0B0, BTTV_PREFIX "VTOTAL_LO", 1},
	{0x0B4, BTTV_PREFIX "VTOTAL_HI", 1},
	{0x0D8, BTTV_PREFIX "COLOR_CTL", 1},
	{0x0DC, BTTV_PREFIX "CAP_CTL", 1},
	{0x0E0, BTTV_PREFIX "VBI_PACK_SIZE", 1},
	{0x0E4, BTTV_PREFIX "VBI_PACK_DEL", 1},
	{0x0E8,	BTTV_PREFIX "FCNTR", 1},

	{0x0F0, BTTV_PREFIX "PLL_F_LO", 1},
	{0x0F4, BTTV_PREFIX "PLL_F_HI", 1},
	{0x0F8, BTTV_PREFIX "PLL_XCI", 1},

	{0x0FC, BTTV_PREFIX "DVSIF", 1},

	{0x104, BTTV_PREFIX "INT_MASK", 4},
	{0x10C, BTTV_PREFIX "GPIO_DMA_CTL", 2},
	{0x114, BTTV_PREFIX "RISC_STRT_ADD", 4},
	{0x118, BTTV_PREFIX "GPIO_OUT_EN", 4},
	{0x11a, BTTV_PREFIX "GPIO_OUT_EN_HIBYTE", 4},
	{0x200, BTTV_PREFIX "GPIO_DATA", 4},
};
