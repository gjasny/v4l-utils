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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dvb-file.h"
#include "dvb-v5-std.h"

/*
 * Standard channel.conf format for DVB-T, DVB-C, DVB-S and ATSC
 */

static const char *zap_parse_inversion[] = {
	[INVERSION_OFF]  = "INVERSION_OFF",
	[INVERSION_ON]   = "INVERSION_ON",
	[INVERSION_AUTO] = "INVERSION_AUTO"
};

static const char *zap_parse_code_rate[] = {
	[FEC_1_2] =  "FEC_1_2",
	[FEC_2_3] =  "FEC_2_3",
	[FEC_3_4] =  "FEC_3_4",
	[FEC_3_5] =  "FEC_3_5",
	[FEC_4_5] =  "FEC_4_5",
	[FEC_5_6] =  "FEC_5_6",
	[FEC_6_7] =  "FEC_6_7",
	[FEC_7_8] =  "FEC_7_8",
	[FEC_8_9] =  "FEC_8_9",
	[FEC_9_10] = "FEC_9_10",
	[FEC_AUTO] = "FEC_AUTO",
	[FEC_NONE] = "FEC_NONE",
};

static const char *zap_parse_modulation[] = {
	[QPSK] =     "QPSK",
	[QAM_16] =   "QAM_16",
	[QAM_32] =   "QAM_32",
	[QAM_64] =   "QAM_64",
	[QAM_128] =  "QAM_128",
	[QAM_256] =  "QAM_256",
	[QAM_AUTO] = "QAM_AUTO",
	[PSK_8] =    "8PSK",
	[VSB_8] =    "8VSB",
	[VSB_16] =   "16VSB",
	[APSK_16] =  "APSK16",
	[APSK_32] =  "APSK32",
	[DQPSK] =    "DQPSK",
};


static const char *zap_parse_trans_mode[] = {
	[TRANSMISSION_MODE_1K] =   "TRANSMISSION_MODE_1K",
	[TRANSMISSION_MODE_2K] =   "TRANSMISSION_MODE_2K",
	[TRANSMISSION_MODE_4K] =   "TRANSMISSION_MODE_4K",
	[TRANSMISSION_MODE_8K] =   "TRANSMISSION_MODE_8K",
	[TRANSMISSION_MODE_16K] =  "TRANSMISSION_MODE_16K",
	[TRANSMISSION_MODE_32K] =  "TRANSMISSION_MODE_32K",
	[TRANSMISSION_MODE_AUTO] = "TRANSMISSION_MODE_AUTO",
};

static const char *zap_parse_guard_interval[8] = {
	[GUARD_INTERVAL_1_4] =    "GUARD_INTERVAL_1_4",
	[GUARD_INTERVAL_1_8] =    "GUARD_INTERVAL_1_8",
	[GUARD_INTERVAL_1_16] =   "GUARD_INTERVAL_1_16",
	[GUARD_INTERVAL_1_32] =   "GUARD_INTERVAL_1_32",
	[GUARD_INTERVAL_1_128] =  "GUARD_INTERVAL_1_128",
	[GUARD_INTERVAL_19_128] = "GUARD_INTERVAL_19_128",
	[GUARD_INTERVAL_19_256] = "GUARD_INTERVAL_19_256",
	[GUARD_INTERVAL_AUTO] =   "GUARD_INTERVAL_AUTO",
};

static const char *zap_parse_hierarchy[] = {
	[HIERARCHY_1] =    "HIERARCHY_1",
	[HIERARCHY_2] =    "HIERARCHY_2",
	[HIERARCHY_4] =    "HIERARCHY_4",
	[HIERARCHY_AUTO] = "HIERARCHY_AUTO",
	[HIERARCHY_NONE] = "HIERARCHY_NONE",
};

static const char *zap_parse_bandwidth[] = {
	[BANDWIDTH_1_712_MHZ] = "BANDWIDTH_1.712_MHZ",
	[BANDWIDTH_5_MHZ] =     "BANDWIDTH_5_MHZ",
	[BANDWIDTH_6_MHZ] =     "BANDWIDTH_6_MHZ",
	[BANDWIDTH_7_MHZ] =     "BANDWIDTH_7_MHZ",
	[BANDWIDTH_8_MHZ] =     "BANDWIDTH_8_MHZ",
	[BANDWIDTH_10_MHZ] =    "BANDWIDTH_10_MHZ",
	[BANDWIDTH_AUTO] =      "BANDWIDTH_AUTO",
};

static const char *zap_parse_polarization[] = {
	[POLARIZATION_H] = "H",
	[POLARIZATION_V] = "V",
	[POLARIZATION_L] = "L",
	[POLARIZATION_R] = "R",
};

static const struct parse_table sys_atsc_table[] = {
	{ DTV_CH_NAME, NULL, 0 },

	{ DTV_FREQUENCY, NULL, 0 },
	{ DTV_MODULATION, PTABLE(zap_parse_modulation) },

	{ DTV_VIDEO_PID, NULL, 0 },
	{ DTV_AUDIO_PID, NULL, 0 },
	{ DTV_SERVICE_ID, NULL, 0 },

};

static const struct parse_table sys_dvbc_table[] = {
	{ DTV_CH_NAME, NULL, 0 },

	{ DTV_FREQUENCY, NULL, 0 },
	{ DTV_INVERSION, PTABLE(zap_parse_inversion) },
	{ DTV_SYMBOL_RATE, NULL, 0 },
	{ DTV_INNER_FEC, PTABLE(zap_parse_code_rate) },
	{ DTV_MODULATION, PTABLE(zap_parse_modulation) },

	{ DTV_VIDEO_PID, NULL, 0 },
	{ DTV_AUDIO_PID, NULL, 0 },
	{ DTV_SERVICE_ID, NULL, 0 },
};

/* Note: On DVB-S, frequency and symbol rate are divided by 1000 */
static const struct parse_table sys_dvbs_table[] = {
	{ DTV_CH_NAME, NULL, 0 },

	{ DTV_FREQUENCY, NULL, 0 },
	{ DTV_POLARIZATION, PTABLE(zap_parse_polarization) },
	{ DTV_SYMBOL_RATE, NULL, 0, .mult_factor = 1000 },

	{ DTV_VIDEO_PID, NULL, 0 },
	{ DTV_AUDIO_PID, NULL, 0 },
	{ DTV_SERVICE_ID, NULL, 0 },

};

static const struct parse_table sys_dvbt_table[] = {
	{ DTV_CH_NAME, NULL, 0 },

	{ DTV_FREQUENCY, NULL, 0 },
	{ DTV_INVERSION, PTABLE(zap_parse_inversion) },
	{ DTV_BANDWIDTH_HZ, PTABLE(zap_parse_bandwidth) },
	{ DTV_CODE_RATE_HP, PTABLE(zap_parse_code_rate) },
	{ DTV_CODE_RATE_LP, PTABLE(zap_parse_code_rate) },
	{ DTV_MODULATION, PTABLE(zap_parse_modulation) },
	{ DTV_TRANSMISSION_MODE, PTABLE(zap_parse_trans_mode) },
	{ DTV_GUARD_INTERVAL, PTABLE(zap_parse_guard_interval) },
	{ DTV_HIERARCHY, PTABLE(zap_parse_hierarchy) },

	{ DTV_VIDEO_PID, NULL, 0 },
	{ DTV_AUDIO_PID, NULL, 0 },
	{ DTV_SERVICE_ID, NULL, 0 },
};

const struct parse_file channel_file_zap_format = {
	.has_delsys_id = 0,
	.delimiter = ":\n",
	.formats = {
		{
			.delsys		= SYS_ATSC,
			PTABLE(sys_atsc_table),
		}, {
			.delsys		= SYS_DVBC_ANNEX_A,
			PTABLE(sys_dvbc_table),
		}, {
			.delsys		= SYS_DVBS,
			PTABLE(sys_dvbs_table),
		}, {
			.delsys		= SYS_DVBT,
			PTABLE(sys_dvbt_table),
		}, {
			NULL, 0, NULL, 0,
		}
	}
};
