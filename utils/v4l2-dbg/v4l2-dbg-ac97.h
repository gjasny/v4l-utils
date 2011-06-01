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

#define AC97_IDENT "ac97"

/* Register name prefix */
#define AC97_PREFIX "AC97_"
#define EM202_PREFIX "EM202_"

static struct board_regs ac97_regs[] = {
	/* general ac97 registers */
	{0x00, AC97_PREFIX "RESET", 2},
	{0x02, AC97_PREFIX "MASTER_VOL", 2},
	{0x04, AC97_PREFIX "LINE_LEVEL_VOL", 2},
	{0x06, AC97_PREFIX "MASTER_MONO_VOL", 2},
	{0x0a, AC97_PREFIX "PC_BEEP_VOL", 2},
	{0x0c, AC97_PREFIX "PHONE_VOL", 2},
	{0x0e, AC97_PREFIX "MIC_VOL", 2},
	{0x10, AC97_PREFIX "LINEIN_VOL", 2},
	{0x12, AC97_PREFIX "CD_VOL", 2},
	{0x14, AC97_PREFIX "VIDEO_VOL", 2},
	{0x16, AC97_PREFIX "AUX_VOL", 2},
	{0x18, AC97_PREFIX "PCM_OUT_VOL", 2},
	{0x1a, AC97_PREFIX "RECORD_SELECT", 2},
	{0x1c, AC97_PREFIX "RECORD_GAIN", 2},
	{0x20, AC97_PREFIX "GENERAL_PURPOSE", 2},
	{0x22, AC97_PREFIX "3D_CTRL", 2},
	{0x24, AC97_PREFIX "AUD_INT_AND_PAG", 2},
	{0x26, AC97_PREFIX "POWER_DOWN_CTRL", 2},
	{0x28, AC97_PREFIX "EXT_AUD_ID", 2},
	{0x2a, AC97_PREFIX "EXT_AUD_CTRL", 2},
	{0x2c, AC97_PREFIX "PCM_OUT_FRONT_SRATE", 2},
	{0x2e, AC97_PREFIX "PCM_OUT_SURR_SRATE", 2},
	{0x30, AC97_PREFIX "PCM_OUT_LFE_SRATE", 2},
	{0x32, AC97_PREFIX "PCM_IN_SRATE", 2},
	{0x36, AC97_PREFIX "LFE_MASTER_VOL", 2},
	{0x38, AC97_PREFIX "SURR_MASTER_VOL", 2},
	{0x3a, AC97_PREFIX "SPDIF_OUT_CTRL", 2},
	{0x7c, AC97_PREFIX "VENDOR_ID1", 2},
	{0x7e, AC97_PREFIX "VENDOR_ID2", 2},

	/* em202 vendor specific registers */
	{0x3e, EM202_PREFIX "EXT_MODEM_CTRL", 2},
	{0x4c, EM202_PREFIX "GPIO_CONF", 2},
	{0x4e, EM202_PREFIX "GPIO_POLARITY", 2},
	{0x50, EM202_PREFIX "GPIO_STICKY", 2},
	{0x52, EM202_PREFIX "GPIO_MASK", 2},
	{0x54, EM202_PREFIX "GPIO_STATUS", 2},
	{0x6a, EM202_PREFIX "SPDIF_OUT_SEL", 2},
	{0x72, EM202_PREFIX "ANTIPOP", 2},
	{0x74, EM202_PREFIX "EAPD_GPIO_ACCESS", 2},
};
