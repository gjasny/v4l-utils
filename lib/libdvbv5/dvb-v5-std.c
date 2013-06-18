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
#include <stddef.h>

#include "dvb-v5-std.h"
#include "dvb-v5.h"

const unsigned int sys_dvbt_props[] = {
	DTV_FREQUENCY,
	DTV_MODULATION,
	DTV_BANDWIDTH_HZ,
	DTV_INVERSION,
	DTV_CODE_RATE_HP,
	DTV_CODE_RATE_LP,
	DTV_GUARD_INTERVAL,
	DTV_TRANSMISSION_MODE,
	DTV_HIERARCHY,
	0
};

const unsigned int sys_dvbt2_props[] = {
	DTV_FREQUENCY,
	DTV_MODULATION,
	DTV_BANDWIDTH_HZ,
	DTV_INVERSION,
	DTV_CODE_RATE_HP,
	DTV_CODE_RATE_LP,
	DTV_GUARD_INTERVAL,
	DTV_TRANSMISSION_MODE,
	DTV_HIERARCHY,
	DTV_DVBT2_PLP_ID_LEGACY,
	0
};

const unsigned int sys_isdbt_props[] = {
	DTV_FREQUENCY,
	DTV_BANDWIDTH_HZ,
	DTV_INVERSION,
	DTV_GUARD_INTERVAL,
	DTV_TRANSMISSION_MODE,
	DTV_ISDBT_LAYER_ENABLED,
	DTV_ISDBT_PARTIAL_RECEPTION,
	DTV_ISDBT_SOUND_BROADCASTING,
	DTV_ISDBT_SB_SUBCHANNEL_ID,
	DTV_ISDBT_SB_SEGMENT_IDX,
	DTV_ISDBT_SB_SEGMENT_COUNT,
	DTV_ISDBT_LAYERA_FEC,
	DTV_ISDBT_LAYERA_MODULATION,
	DTV_ISDBT_LAYERA_SEGMENT_COUNT,
	DTV_ISDBT_LAYERA_TIME_INTERLEAVING,
	DTV_ISDBT_LAYERB_FEC,
	DTV_ISDBT_LAYERB_MODULATION,
	DTV_ISDBT_LAYERB_SEGMENT_COUNT,
	DTV_ISDBT_LAYERB_TIME_INTERLEAVING,
	DTV_ISDBT_LAYERC_FEC,
	DTV_ISDBT_LAYERC_MODULATION,
	DTV_ISDBT_LAYERC_SEGMENT_COUNT,
	DTV_ISDBT_LAYERC_TIME_INTERLEAVING,
	0
};

const unsigned int sys_atsc_props[] = {
	DTV_FREQUENCY,
	DTV_MODULATION,
	0
};

const unsigned int sys_atscmh_props[] = {
	DTV_FREQUENCY,
	DTV_MODULATION,
	DTV_ATSCMH_FIC_VER,
	DTV_ATSCMH_PARADE_ID,
	DTV_ATSCMH_NOG,
	DTV_ATSCMH_TNOG,
	DTV_ATSCMH_SGN,
	DTV_ATSCMH_PRC,
	DTV_ATSCMH_RS_FRAME_MODE,
	DTV_ATSCMH_RS_FRAME_ENSEMBLE,
	DTV_ATSCMH_RS_CODE_MODE_PRI,
	DTV_ATSCMH_RS_CODE_MODE_SEC,
	DTV_ATSCMH_SCCC_BLOCK_MODE,
	DTV_ATSCMH_SCCC_CODE_MODE_A,
	DTV_ATSCMH_SCCC_CODE_MODE_B,
	DTV_ATSCMH_SCCC_CODE_MODE_C,
	DTV_ATSCMH_SCCC_CODE_MODE_D,
	0
};

const unsigned int sys_dvbc_annex_ac_props[] = {
	DTV_FREQUENCY,
	DTV_MODULATION,
	DTV_INVERSION,
	DTV_SYMBOL_RATE,
	DTV_INNER_FEC,
	0
};

const unsigned int sys_dvbc_annex_b_props[] = {
	DTV_FREQUENCY,
	DTV_MODULATION,
	0
};

const unsigned int sys_dvbs_props[] = {
	DTV_FREQUENCY,
	DTV_INVERSION,
	DTV_SYMBOL_RATE,
	DTV_INNER_FEC,
	DTV_POLARIZATION,
	0
};

const unsigned int sys_dvbs2_props[] = {
	DTV_FREQUENCY,
	DTV_INVERSION,
	DTV_SYMBOL_RATE,
	DTV_INNER_FEC,
	DTV_MODULATION,
	DTV_PILOT,
	DTV_ROLLOFF,
	DTV_POLARIZATION,
	0
};

const unsigned int sys_turbo_props[] = {
	DTV_FREQUENCY,
	DTV_INVERSION,
	DTV_SYMBOL_RATE,
	DTV_INNER_FEC,
	DTV_MODULATION,
	DTV_POLARIZATION,
	0
};

const unsigned int sys_isdbs_props[] = {
	DTV_FREQUENCY,
	DTV_INVERSION,
	DTV_SYMBOL_RATE,
	DTV_INNER_FEC,
	DTV_ISDBS_TS_ID_LEGACY,
	DTV_POLARIZATION,
	0
};

const unsigned int *dvb_v5_delivery_system[] = {
	[SYS_ATSC] =          sys_atsc_props,
	[SYS_ATSCMH] =        sys_atscmh_props,
	[SYS_DVBC_ANNEX_A] =  sys_dvbc_annex_ac_props,
	[SYS_DVBC_ANNEX_C] =  sys_dvbc_annex_ac_props,
	[SYS_DVBC_ANNEX_B] =  sys_dvbc_annex_b_props,
	[SYS_DVBS] =          sys_dvbs_props,
	[SYS_DVBS2] =         sys_dvbs2_props,
	[SYS_DVBT] =          sys_dvbt_props,
	[SYS_DVBT2] =         sys_dvbt2_props,
	[SYS_ISDBS] =         sys_isdbs_props,
	[SYS_ISDBT] =         sys_isdbt_props,
	[SYS_TURBO] =         sys_turbo_props,
	[SYS_ATSCMH] =        NULL,
	[SYS_CMMB] =          NULL,
	[SYS_DAB] =           NULL,
	[SYS_DMBTH] =         NULL,
	[SYS_DSS] =           NULL,
	[SYS_DVBH] =          NULL,
	[SYS_ISDBC] =         NULL,
	[SYS_UNDEFINED] =     NULL,
};

const void *dvb_v5_attr_names[] = {
	[0 ...DTV_MAX_COMMAND ] = NULL,
	[DTV_CODE_RATE_HP]		= fe_code_rate_name,
	[DTV_CODE_RATE_LP]		= fe_code_rate_name,
	[DTV_INNER_FEC]			= fe_code_rate_name,
	[DTV_ISDBT_LAYERA_FEC]		= fe_code_rate_name,
	[DTV_ISDBT_LAYERB_FEC]		= fe_code_rate_name,
	[DTV_ISDBT_LAYERC_FEC]		= fe_code_rate_name,
	[DTV_MODULATION]		= fe_modulation_name,
	[DTV_ISDBT_LAYERA_MODULATION]	= fe_modulation_name,
	[DTV_ISDBT_LAYERB_MODULATION]	= fe_modulation_name,
	[DTV_ISDBT_LAYERC_MODULATION]	= fe_modulation_name,
	[DTV_TRANSMISSION_MODE]		= fe_transmission_mode_name,
	[DTV_GUARD_INTERVAL]		= fe_guard_interval_name,
	[DTV_HIERARCHY]			= fe_hierarchy_name,
	[DTV_VOLTAGE]			= fe_voltage_name,
	[DTV_TONE]			= fe_tone_name,
	[DTV_INVERSION]			= fe_inversion_name,
	[DTV_PILOT]			= fe_pilot_name,
	[DTV_ROLLOFF]			= fe_rolloff_name,
	[DTV_DELIVERY_SYSTEM]		= delivery_system_name,
};

const char *dvb_sat_pol_name[6] = {
	[POLARIZATION_OFF] = "OFF",
	[POLARIZATION_H] = "HORIZONTAL",
	[POLARIZATION_V] = "VERTICAL",
	[POLARIZATION_L] = "LEFT",
	[POLARIZATION_R] = "RIGHT",
	[5] = NULL,
};

const char *dvb_user_name[DTV_USER_NAME_SIZE] = {
	[DTV_POLARIZATION - DTV_USER_COMMAND_START] =   "POLARIZATION",
	[DTV_VIDEO_PID - DTV_USER_COMMAND_START] =	"VIDEO PID",
	[DTV_AUDIO_PID - DTV_USER_COMMAND_START] =	"AUDIO PID",
	[DTV_SERVICE_ID - DTV_USER_COMMAND_START] =	"SERVICE ID",
	[DTV_CH_NAME - DTV_USER_COMMAND_START] =	"CHANNEL",
	[DTV_VCHANNEL - DTV_USER_COMMAND_START] =	"VCHANNEL",
	[DTV_SAT_NUMBER - DTV_USER_COMMAND_START] =	"SAT NUMBER",
	[DTV_DISEQC_WAIT - DTV_USER_COMMAND_START] =	"DISEQC WAIT",
	[DTV_DISEQC_LNB - DTV_USER_COMMAND_START] =	"DISEQC LNB",
	[DTV_FREQ_BPF - DTV_USER_COMMAND_START] =	"FREQ BPF",
	[DTV_STATUS - DTV_USER_COMMAND_START] = 	"STATUS",
	[DTV_BER - DTV_USER_COMMAND_START] =		"POST BER",
	[DTV_PER - DTV_USER_COMMAND_START] =		"PER",
	[DTV_QUALITY - DTV_USER_COMMAND_START] =	"QUALITY",
	[DTV_PRE_BER - DTV_USER_COMMAND_START] =	"PRE BER",

	[DTV_USER_NAME_SIZE - 1] = NULL,
};

const void *dvb_user_attr_names[] = {
	[0 ... DTV_MAX_STAT_COMMAND - DTV_USER_COMMAND_START] = NULL,
	[DTV_POLARIZATION - DTV_USER_COMMAND_START]           = dvb_sat_pol_name,
};

