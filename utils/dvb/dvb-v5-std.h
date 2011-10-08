/*
 * Per-delivery system properties, according with the specs:
 * 	http://linuxtv.org/downloads/v4l-dvb-apis/FE_GET_SET_PROPERTY.html
 */

#include "dvb_frontend.h"

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
	DTV_DVBT2_PLP_ID,
	0
};

const unsigned int sys_isdbt_props[] = {
	DTV_FREQUENCY,
	DTV_MODULATION,
	DTV_BANDWIDTH_HZ,
	DTV_INVERSION,
	DTV_CODE_RATE_HP,
	DTV_CODE_RATE_LP,
	DTV_GUARD_INTERVAL,
	DTV_TRANSMISSION_MODE,
	DTV_HIERARCHY,
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
	DTV_BANDWIDTH_HZ,
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
	DTV_INVERSION,
	0
};

const unsigned int sys_dvbs_props[] = {
	DTV_FREQUENCY,
	DTV_INVERSION,
	DTV_SYMBOL_RATE,
	DTV_INNER_FEC,
	DTV_VOLTAGE,
	DTV_TONE,
	0
};

const unsigned int sys_dvbs2_props[] = {
	DTV_FREQUENCY,
	DTV_INVERSION,
	DTV_SYMBOL_RATE,
	DTV_INNER_FEC,
	DTV_VOLTAGE,
	DTV_TONE,
	DTV_MODULATION,
	DTV_PILOT,
	DTV_ROLLOFF,
	0
};

const unsigned int sys_turbo_props[] = {
	DTV_FREQUENCY,
	DTV_INVERSION,
	DTV_SYMBOL_RATE,
	DTV_INNER_FEC,
	DTV_VOLTAGE,
	DTV_TONE,
	DTV_MODULATION,
	0
};

const unsigned int sys_isdbs_props[] = {
	DTV_FREQUENCY,
	DTV_INVERSION,
	DTV_SYMBOL_RATE,
	DTV_INNER_FEC,
	DTV_VOLTAGE,
	DTV_ISDBS_TS_ID,
	0
};

const unsigned int *delivery_system_name[] = {
	[SYS_ATSC] =          sys_atsc_props,
	[SYS_DVBC_ANNEX_AC] = sys_dvbc_annex_ac_props,
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
