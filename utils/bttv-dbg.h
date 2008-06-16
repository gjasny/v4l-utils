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

#include "v4l-board-dbg.h"

#define BTTV_IDENT "bttv"

static struct board_regs bt8xx_regs_other[] = {
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

static struct board_regs bt8xx_regs[] = {
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
