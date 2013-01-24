/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab <mchehab@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * Per-delivery system properties, according with the specs:
 * 	http://linuxtv.org/downloads/v4l-dvb-apis/FE_GET_SET_PROPERTY.html
 */
#ifndef _DVB_V5_STD_H
#define _DVB_V5_STD_H

#include <stddef.h>
#include "dvb-frontend.h"

extern const unsigned int sys_dvbt_props[];
extern const unsigned int sys_dvbt2_props[];
extern const unsigned int sys_isdbt_props[];
extern const unsigned int sys_atsc_props[];
extern const unsigned int sys_atscmh_props[];
extern const unsigned int sys_dvbc_annex_ac_props[];
extern const unsigned int sys_dvbc_annex_b_props[];
extern const unsigned int sys_dvbs_props[];
extern const unsigned int sys_dvbs2_props[];
extern const unsigned int sys_turbo_props[];
extern const unsigned int sys_isdbs_props[];
extern const unsigned int *dvb_v5_delivery_system[];
extern const void *dvb_v5_attr_names[];

/* User DTV codes, for internal usage */

#define DTV_USER_COMMAND_START 256

#define DTV_POLARIZATION        (DTV_USER_COMMAND_START + 0)
#define DTV_VIDEO_PID           (DTV_USER_COMMAND_START + 1)
#define DTV_AUDIO_PID           (DTV_USER_COMMAND_START + 2)
#define DTV_SERVICE_ID          (DTV_USER_COMMAND_START + 3)
#define DTV_CH_NAME             (DTV_USER_COMMAND_START + 4)
#define DTV_VCHANNEL            (DTV_USER_COMMAND_START + 5)
#define DTV_SAT_NUMBER          (DTV_USER_COMMAND_START + 6)
#define DTV_DISEQC_WAIT         (DTV_USER_COMMAND_START + 7)
#define DTV_DISEQC_LNB          (DTV_USER_COMMAND_START + 8)
#define DTV_FREQ_BPF            (DTV_USER_COMMAND_START + 9)

#define DTV_MAX_USER_COMMAND    DTV_FREQ_BPF

/* For status and statistics */

#define DTV_STATUS              (DTV_MAX_USER_COMMAND + 1)
#define DTV_BER                 (DTV_MAX_USER_COMMAND + 2)
#define DTV_PER                 (DTV_MAX_USER_COMMAND + 3)
#define DTV_QUALITY             (DTV_MAX_USER_COMMAND + 4)
#define DTV_PRE_BER		(DTV_MAX_USER_COMMAND + 5)

#define DTV_MAX_STAT_COMMAND	DTV_PRE_BER

#define DTV_USER_NAME_SIZE	(1 + DTV_MAX_STAT_COMMAND - DTV_USER_COMMAND_START)

/* There are currently 8 stats provided on Kernelspace */
#define DTV_NUM_KERNEL_STATS	8

#define DTV_NUM_STATS_PROPS	(DTV_NUM_KERNEL_STATS + DTV_MAX_STAT_COMMAND - DTV_MAX_USER_COMMAND)

enum dvb_sat_polarization {
	POLARIZATION_OFF	= 0,
	POLARIZATION_H		= 1,
	POLARIZATION_V		= 2,
	POLARIZATION_L		= 3,
	POLARIZATION_R		= 4,
};

enum dvb_quality {
	DVB_QUAL_UNKNOWN = 0,
	DVB_QUAL_POOR,
	DVB_QUAL_OK,
	DVB_QUAL_GOOD,
};

extern const char *dvb_sat_pol_name[6];
extern const char *dvb_user_name[DTV_USER_NAME_SIZE];
extern const void *dvb_user_attr_names[];

#endif

