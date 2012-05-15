/*
 * File auto-generated from the kernel sources. Please, don't edit it
 */
#ifndef _DVB_V5_CONSTS_H
#define _DVB_V5_CONSTS_H

#include <stddef.h>
#include "dvb_frontend.h"

struct fe_caps_name {
	unsigned  idx;
	char *name;
} fe_caps_name[30] = {
	{ FE_CAN_2G_MODULATION,          "CAN_2G_MODULATION" },
	{ FE_CAN_8VSB,                   "CAN_8VSB" },
	{ FE_CAN_16VSB,                  "CAN_16VSB" },
	{ FE_CAN_BANDWIDTH_AUTO,         "CAN_BANDWIDTH_AUTO" },
	{ FE_CAN_FEC_1_2,                "CAN_FEC_1_2" },
	{ FE_CAN_FEC_2_3,                "CAN_FEC_2_3" },
	{ FE_CAN_FEC_3_4,                "CAN_FEC_3_4" },
	{ FE_CAN_FEC_4_5,                "CAN_FEC_4_5" },
	{ FE_CAN_FEC_5_6,                "CAN_FEC_5_6" },
	{ FE_CAN_FEC_6_7,                "CAN_FEC_6_7" },
	{ FE_CAN_FEC_7_8,                "CAN_FEC_7_8" },
	{ FE_CAN_FEC_8_9,                "CAN_FEC_8_9" },
	{ FE_CAN_FEC_AUTO,               "CAN_FEC_AUTO" },
	{ FE_CAN_GUARD_INTERVAL_AUTO,    "CAN_GUARD_INTERVAL_AUTO" },
	{ FE_CAN_HIERARCHY_AUTO,         "CAN_HIERARCHY_AUTO" },
	{ FE_CAN_INVERSION_AUTO,         "CAN_INVERSION_AUTO" },
	{ FE_CAN_MUTE_TS,                "CAN_MUTE_TS" },
	{ FE_CAN_QAM_16,                 "CAN_QAM_16" },
	{ FE_CAN_QAM_32,                 "CAN_QAM_32" },
	{ FE_CAN_QAM_64,                 "CAN_QAM_64" },
	{ FE_CAN_QAM_128,                "CAN_QAM_128" },
	{ FE_CAN_QAM_256,                "CAN_QAM_256" },
	{ FE_CAN_QAM_AUTO,               "CAN_QAM_AUTO" },
	{ FE_CAN_QPSK,                   "CAN_QPSK" },
	{ FE_CAN_RECOVER,                "CAN_RECOVER" },
	{ FE_CAN_TRANSMISSION_MODE_AUTO, "CAN_TRANSMISSION_MODE_AUTO" },
	{ FE_CAN_TURBO_FEC,              "CAN_TURBO_FEC" },
	{ FE_HAS_EXTENDED_CAPS,          "HAS_EXTENDED_CAPS" },
	{ FE_IS_STUPID,                  "IS_STUPID" },
	{ FE_NEEDS_BENDING,              "NEEDS_BENDING" },
};

struct fe_status_name {
	unsigned  idx;
	char *name;
} fe_status_name[7] = {
	{ FE_HAS_CARRIER, "CARRIER" },
	{ FE_HAS_LOCK,    "LOCK" },
	{ FE_HAS_SIGNAL,  "SIGNAL" },
	{ FE_HAS_SYNC,    "SYNC" },
	{ FE_HAS_VITERBI, "VITERBI" },
	{ FE_REINIT,      "REINIT" },
	{ FE_TIMEDOUT,    "TIMEDOUT" },
};

const char *fe_code_rate_name[13] = {
	[FEC_1_2] =  "1/2",
	[FEC_2_3] =  "2/3",
	[FEC_3_4] =  "3/4",
	[FEC_3_5] =  "3/5",
	[FEC_4_5] =  "4/5",
	[FEC_5_6] =  "5/6",
	[FEC_6_7] =  "6/7",
	[FEC_7_8] =  "7/8",
	[FEC_8_9] =  "8/9",
	[FEC_9_10] = "9/10",
	[FEC_AUTO] = "AUTO",
	[FEC_NONE] = "NONE",
	[12] = NULL,
};

const char *fe_modulation_name[14] = {
	[APSK_16] =  "APSK/16",
	[APSK_32] =  "APSK/32",
	[DQPSK] =    "DQPSK",
	[PSK_8] =    "PSK/8",
	[QAM_16] =   "QAM/16",
	[QAM_32] =   "QAM/32",
	[QAM_64] =   "QAM/64",
	[QAM_128] =  "QAM/128",
	[QAM_256] =  "QAM/256",
	[QAM_AUTO] = "QAM/AUTO",
	[QPSK] =     "QPSK",
	[VSB_8] =    "VSB/8",
	[VSB_16] =   "VSB/16",
	[13] = NULL,
};

const char *fe_transmission_mode_name[8] = {
	[TRANSMISSION_MODE_1K] =   "1K",
	[TRANSMISSION_MODE_2K] =   "2K",
	[TRANSMISSION_MODE_4K] =   "4K",
	[TRANSMISSION_MODE_8K] =   "8K",
	[TRANSMISSION_MODE_16K] =  "16K",
	[TRANSMISSION_MODE_32K] =  "32K",
	[TRANSMISSION_MODE_AUTO] = "AUTO",
	[7] = NULL,
};

const unsigned fe_bandwidth_name[8] = {
	[BANDWIDTH_1_712_MHZ] = 1712000,
	[BANDWIDTH_5_MHZ] =     5000000,
	[BANDWIDTH_6_MHZ] =     6000000,
	[BANDWIDTH_7_MHZ] =     7000000,
	[BANDWIDTH_8_MHZ] =     8000000,
	[BANDWIDTH_10_MHZ] =    10000000,
	[BANDWIDTH_AUTO] =      0,
	[7] = 0,
};

const char *fe_guard_interval_name[9] = {
	[GUARD_INTERVAL_1_4] =    "1/4",
	[GUARD_INTERVAL_1_8] =    "1/8",
	[GUARD_INTERVAL_1_16] =   "1/16",
	[GUARD_INTERVAL_1_32] =   "1/32",
	[GUARD_INTERVAL_1_128] =  "1/128",
	[GUARD_INTERVAL_19_128] = "19/128",
	[GUARD_INTERVAL_19_256] = "19/256",
	[GUARD_INTERVAL_AUTO] =   "AUTO",
	[8] = NULL,
};

const char *fe_hierarchy_name[6] = {
	[HIERARCHY_1] =    "1",
	[HIERARCHY_2] =    "2",
	[HIERARCHY_4] =    "4",
	[HIERARCHY_AUTO] = "AUTO",
	[HIERARCHY_NONE] = "NONE",
	[5] = NULL,
};

const char *fe_voltage_name[4] = {
	[SEC_VOLTAGE_13] =  "13",
	[SEC_VOLTAGE_18] =  "18",
	[SEC_VOLTAGE_OFF] = "OFF",
	[3] = NULL,
};

const char *fe_tone_name[3] = {
	[SEC_TONE_OFF] = "OFF",
	[SEC_TONE_ON] =  "ON",
	[2] = NULL,
};

const char *fe_inversion_name[4] = {
	[INVERSION_AUTO] = "AUTO",
	[INVERSION_OFF] =  "OFF",
	[INVERSION_ON] =   "ON",
	[3] = NULL,
};

const char *fe_pilot_name[4] = {
	[PILOT_AUTO] = "AUTO",
	[PILOT_OFF] =  "OFF",
	[PILOT_ON] =   "ON",
	[3] = NULL,
};

const char *fe_rolloff_name[5] = {
	[ROLLOFF_20] =   "20",
	[ROLLOFF_25] =   "25",
	[ROLLOFF_35] =   "35",
	[ROLLOFF_AUTO] = "AUTO",
	[4] = NULL,
};

const char *dvb_v5_name[46] = {
	[DTV_API_VERSION] =                    "API_VERSION",
	[DTV_BANDWIDTH_HZ] =                   "BANDWIDTH_HZ",
	[DTV_CLEAR] =                          "CLEAR",
	[DTV_CODE_RATE_HP] =                   "CODE_RATE_HP",
	[DTV_CODE_RATE_LP] =                   "CODE_RATE_LP",
	[DTV_DELIVERY_SYSTEM] =                "DELIVERY_SYSTEM",
	[DTV_DISEQC_MASTER] =                  "DISEQC_MASTER",
	[DTV_DISEQC_SLAVE_REPLY] =             "DISEQC_SLAVE_REPLY",
	[DTV_DVBT2_PLP_ID] =                   "DVBT2_PLP_ID",
	[DTV_ENUM_DELSYS] =                    "ENUM_DELSYS",
	[DTV_FE_CAPABILITY] =                  "FE_CAPABILITY",
	[DTV_FE_CAPABILITY_COUNT] =            "FE_CAPABILITY_COUNT",
	[DTV_FREQUENCY] =                      "FREQUENCY",
	[DTV_GUARD_INTERVAL] =                 "GUARD_INTERVAL",
	[DTV_HIERARCHY] =                      "HIERARCHY",
	[DTV_INNER_FEC] =                      "INNER_FEC",
	[DTV_INVERSION] =                      "INVERSION",
	[DTV_ISDBS_TS_ID] =                    "ISDBS_TS_ID",
	[DTV_ISDBT_LAYERA_FEC] =               "ISDBT_LAYERA_FEC",
	[DTV_ISDBT_LAYERA_MODULATION] =        "ISDBT_LAYERA_MODULATION",
	[DTV_ISDBT_LAYERA_SEGMENT_COUNT] =     "ISDBT_LAYERA_SEGMENT_COUNT",
	[DTV_ISDBT_LAYERA_TIME_INTERLEAVING] = "ISDBT_LAYERA_TIME_INTERLEAVING",
	[DTV_ISDBT_LAYERB_FEC] =               "ISDBT_LAYERB_FEC",
	[DTV_ISDBT_LAYERB_MODULATION] =        "ISDBT_LAYERB_MODULATION",
	[DTV_ISDBT_LAYERB_SEGMENT_COUNT] =     "ISDBT_LAYERB_SEGMENT_COUNT",
	[DTV_ISDBT_LAYERB_TIME_INTERLEAVING] = "ISDBT_LAYERB_TIME_INTERLEAVING",
	[DTV_ISDBT_LAYERC_FEC] =               "ISDBT_LAYERC_FEC",
	[DTV_ISDBT_LAYERC_MODULATION] =        "ISDBT_LAYERC_MODULATION",
	[DTV_ISDBT_LAYERC_SEGMENT_COUNT] =     "ISDBT_LAYERC_SEGMENT_COUNT",
	[DTV_ISDBT_LAYERC_TIME_INTERLEAVING] = "ISDBT_LAYERC_TIME_INTERLEAVING",
	[DTV_ISDBT_LAYER_ENABLED] =            "ISDBT_LAYER_ENABLED",
	[DTV_ISDBT_PARTIAL_RECEPTION] =        "ISDBT_PARTIAL_RECEPTION",
	[DTV_ISDBT_SB_SEGMENT_COUNT] =         "ISDBT_SB_SEGMENT_COUNT",
	[DTV_ISDBT_SB_SEGMENT_IDX] =           "ISDBT_SB_SEGMENT_IDX",
	[DTV_ISDBT_SB_SUBCHANNEL_ID] =         "ISDBT_SB_SUBCHANNEL_ID",
	[DTV_ISDBT_SOUND_BROADCASTING] =       "ISDBT_SOUND_BROADCASTING",
	[DTV_MODULATION] =                     "MODULATION",
	[DTV_PILOT] =                          "PILOT",
	[DTV_ROLLOFF] =                        "ROLLOFF",
	[DTV_SYMBOL_RATE] =                    "SYMBOL_RATE",
	[DTV_TONE] =                           "TONE",
	[DTV_TRANSMISSION_MODE] =              "TRANSMISSION_MODE",
	[DTV_TUNE] =                           "TUNE",
	[DTV_UNDEFINED] =                      "UNDEFINED",
	[DTV_VOLTAGE] =                        "VOLTAGE",
	[45] = NULL,
};

const char *delivery_system_name[20] = {
	[SYS_ATSC] =         "ATSC",
	[SYS_ATSCMH] =       "ATSCMH",
	[SYS_CMMB] =         "CMMB",
	[SYS_DAB] =          "DAB",
	[SYS_DMBTH] =        "DMBTH",
	[SYS_DSS] =          "DSS",
	[SYS_DVBC_ANNEX_A] = "DVBC/ANNEX_A",
	[SYS_DVBC_ANNEX_B] = "DVBC/ANNEX_B",
	[SYS_DVBC_ANNEX_C] = "DVBC/ANNEX_C",
	[SYS_DVBH] =         "DVBH",
	[SYS_DVBS] =         "DVBS",
	[SYS_DVBS2] =        "DVBS2",
	[SYS_DVBT] =         "DVBT",
	[SYS_DVBT2] =        "DVBT2",
	[SYS_ISDBC] =        "ISDBC",
	[SYS_ISDBS] =        "ISDBS",
	[SYS_ISDBT] =        "ISDBT",
	[SYS_TURBO] =        "TURBO",
	[SYS_UNDEFINED] =    "UNDEFINED",
	[19] = NULL,
};

#endif
