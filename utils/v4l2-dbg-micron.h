/*
    Copyright (C) 2009 Mauro Carvalho Chehab <mchehab@infradead.org>
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

#include "v4l2-dbg.h"

#define MT9V011_IDENT "mt9v011"

/* Register name prefix */
#define MT9V011_PREFIX "MT9V011_"

static struct board_regs mt9v011_regs[] = {
	{0x00, MT9V011_PREFIX "CHIP_VERSION"},
	{0x01, MT9V011_PREFIX "ROWSTART"},
	{0x02, MT9V011_PREFIX "COLSTART"},
	{0x03, MT9V011_PREFIX "HEIGHT"},
	{0x04, MT9V011_PREFIX "WIDTH"},
	{0x05, MT9V011_PREFIX "HBLANK"},
	{0x06, MT9V011_PREFIX "VBLANK"},
	{0x07, MT9V011_PREFIX "OUT_CTRL"},
	{0x09, MT9V011_PREFIX "SHUTTER_WIDTH"},
	{0x0a, MT9V011_PREFIX "CLK_SPEED"},
	{0x0b, MT9V011_PREFIX "RESTART"},
	{0x0c, MT9V011_PREFIX "SHUTTER_DELAY"},
	{0x0d, MT9V011_PREFIX "RESET"},
	{0x1e, MT9V011_PREFIX "DIGITAL_ZOOM"},
	{0x20, MT9V011_PREFIX "READ_MODE"},
	{0x2b, MT9V011_PREFIX "GREEN_1_GAIN"},
	{0x2c, MT9V011_PREFIX "BLUE_GAIN"},
	{0x2d, MT9V011_PREFIX "RED_GAIN"},
	{0x2e, MT9V011_PREFIX "GREEN_2_GAIN"},
	{0x35, MT9V011_PREFIX "GLOBAL_GAIN"},
	{0xf1, MT9V011_PREFIX "CHIP_ENABLE"},
};
