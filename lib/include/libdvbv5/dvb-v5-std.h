/*
 * Copyright (c) 2011-2014 - Mauro Carvalho Chehab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * Per-delivery system properties defined at libdvbv5 scope, following
 * the same model as defined at the Linux DVB media specs:
 * 	http://linuxtv.org/downloads/v4l-dvb-apis/FE_GET_SET_PROPERTY.html
 */
#ifndef _DVB_V5_STD_H
#define _DVB_V5_STD_H

#include <stddef.h>
#include "dvb-frontend.h"

/**
 * @file dvb-v5-std.h
 * @ingroup frontend
 * @brief Provides libdvbv5 defined properties for the frontend.
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Mauro Carvalho Chehab
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

/*
 * User DTV codes, for internal usage. There are two sets of
 * properties. One for DTV properties and another one for statistics
 */

/*
 * First set: DTV properties that don't belong to Kernelspace
 *
 * Those properties contain data that comes from the MPEG-TS
 * tables, like audio/video/other PIDs, and satellite config
 */

/**
 * @def DTV_USER_COMMAND_START
 *	@brief Start number for libdvbv5 user commands
 *	@ingroup frontend
 * @def DTV_POLARIZATION
 *	@brief Satellite polarization (for Satellite delivery systems)
 *	@ingroup frontend
 * @def DTV_AUDIO_PID
 * 	@brief Audio PID
 *	@ingroup frontend
 * @def DTV_VIDEO_PID
 *	@brief Video PID
 *	@ingroup frontend
 * @def DTV_SERVICE_ID
 *	@brief MPEG TS service ID
 *	@ingroup frontend
 * @def DTV_CH_NAME
 *	@brief Digital TV service name
 *	@ingroup frontend
 * @def DTV_VCHANNEL
 *	@brief Digital TV channel number. May contain symbols
 *	@ingroup frontend
 * @def DTV_SAT_NUMBER
 *	@brief Number of the satellite (used on multi-dish Satellite systems)
 *	@ingroup frontend
 * @def DTV_DISEQC_WAIT
 *	@brief Extra time needed to wait for DiSeqC to complete, in ms.
 *		The minimal wait time is 15 ms. The time here will be
 *		added to the minimal time.
 *	@ingroup frontend
 * @def DTV_DISEQC_LNB
 *	@brief LNBf name
 *	@ingroup frontend
 * @def DTV_FREQ_BPF
 *	@brief SCR/Unicable band-pass filter frequency in kHz
 *	@ingroup frontend
 * @def DTV_PLS_CODE
 *	@brief DVB-T2 PLS code. Not used internally. It is needed
 *			only for file conversion.
 *	@ingroup frontend
 * @def DTV_PLS_MODE
 *	@brief DVB-T2 PLS mode. Not used internally. It is needed
 *			only for file conversion.
 *	@ingroup frontend
 * @def DTV_COUNTRY_CODE
 *	@brief Country variant of international delivery system standard.
		in ISO 3166-1 two letter code.
 *	@ingroup frontend
 * @def DTV_MAX_USER_COMMAND
 *	 @brief Last user command
 *	@ingroup frontend
 * @def DTV_USER_NAME_SIZE
 *	 @brief Number of user commands
 *	@ingroup frontend
 */

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
#define DTV_PLS_CODE		(DTV_USER_COMMAND_START + 10)
#define DTV_PLS_MODE		(DTV_USER_COMMAND_START + 11)
#define DTV_COUNTRY_CODE	(DTV_USER_COMMAND_START + 12)

#define DTV_MAX_USER_COMMAND    DTV_COUNTRY_CODE

#define DTV_USER_NAME_SIZE	(1 + DTV_MAX_USER_COMMAND - DTV_USER_COMMAND_START)

/**
 * @enum dvb_sat_polarization
 * @brief Polarization types for Satellite systems
 * @ingroup satellite
 *
 * @param POLARIZATION_OFF		Polarization disabled/unused.
 * @param POLARIZATION_H		Horizontal polarization
 * @param POLARIZATION_V		Vertical polarization
 * @param POLARIZATION_L		Left circular polarization (C-band)
 * @param POLARIZATION_R		Right circular polarization (C-band)
 */
enum dvb_sat_polarization {
	POLARIZATION_OFF	= 0,
	POLARIZATION_H		= 1,
	POLARIZATION_V		= 2,
	POLARIZATION_L		= 3,
	POLARIZATION_R		= 4,
};

/*
 * Second set: DTV statistics
 *
 * Those properties contain statistics measurements that aren't
 * either provided by the Kernel via property cmd/value pair,
 * like status (with has its own ioctl), or that are derivated
 * measures from two or more Kernel reported stats.
 */

/**
 * @def DTV_STAT_COMMAND_START
 *	@brief Start number for libdvbv5 statistics commands
 *	@ingroup frontend
 * @def DTV_STATUS
 *	@brief Lock status of a DTV frontend. This actually comes from
 *			the Kernel, but it uses a separate ioctl.
 *	@ingroup frontend
 * @def DTV_BER
 * 	@brief Bit Error Rate. This is a parameter that it is
 *			derivated from two counters at the Kernel side
 *	@ingroup frontend
 * @def DTV_PER
 *	@brief Packet Error Rate. This is a parameter that it is
 *			derivated from two counters at the Kernel side
 *	@ingroup frontend
 * @def DTV_QUALITY
 * 	@brief A quality indicator that represents if a locked
 *			channel provides a good, OK or poor signal. This is
 *			estimated considering the error rates, signal strengh
 *			and/or S/N ratio of the carrier.
 *	@ingroup frontend
 * @def DTV_PRE_BER
 *	@brief Bit Error Rate before Viterbi. This is the error rate
 *			before applying the Forward Error Correction. This is
 *			a parameter that it is derivated from two counters
 *			at the Kernel side.
 *	@ingroup frontend
 * @def DTV_MAX_STAT_COMMAND
 *	@brief Last statistics command
 *	@ingroup frontend
 * @def DTV_STAT_NAME_SIZE
 *	@brief Number of statistics commands
 *	@ingroup frontend
 * @def DTV_NUM_KERNEL_STATS
 *	@brief Number of statistics commands provided by the Kernel
 *	@ingroup frontend
 * @def DTV_NUM_STATS_PROPS
 *	@brief Total number of statistics commands
 *	@ingroup frontend
 */

#define DTV_STAT_COMMAND_START 512

#define DTV_STATUS              (DTV_STAT_COMMAND_START + 0)
#define DTV_BER                 (DTV_STAT_COMMAND_START + 1)
#define DTV_PER                 (DTV_STAT_COMMAND_START + 2)
#define DTV_QUALITY             (DTV_STAT_COMMAND_START + 3)
#define DTV_PRE_BER		(DTV_STAT_COMMAND_START + 4)

#define DTV_MAX_STAT_COMMAND	DTV_PRE_BER

#define DTV_STAT_NAME_SIZE	(1 + DTV_MAX_STAT_COMMAND - DTV_STAT_COMMAND_START)

/* There are currently 8 stats provided on Kernelspace */
#define DTV_NUM_KERNEL_STATS	8

#define DTV_NUM_STATS_PROPS	(DTV_NUM_KERNEL_STATS + DTV_STAT_NAME_SIZE)

/**
 * @enum dvb_quality
 * @brief Provides an estimation about the user's experience
 *	  while watching to a given MPEG stream
 * @ingroup frontend
 *
 * @param DVB_QUAL_UNKNOWN	Quality could not be estimated, as the Kernel driver
 *			doesn't provide enough statistics
 *
 * @param DVB_QUAL_POOR	The signal reception is poor. Signal loss or packets
 *			can be lost too frequently.
 * @param DVB_QUAL_OK	The signal reception is ok. Eventual artifacts could
 *			be expected, but it should work.
 * @param DVB_QUAL_GOOD	The signal is good, and not many errors are happening.
 *			The user should have a good experience watching the
 *			stream.
 */
enum dvb_quality {
	DVB_QUAL_UNKNOWN = 0,
	DVB_QUAL_POOR,
	DVB_QUAL_OK,
	DVB_QUAL_GOOD,
};

#ifndef _DOXYGEN

/*
 * Some tables to translate from value to string
 *
 * These tables are raw ways to translate from some DTV  values into strings.
 * Please use the API-provided function dvb_cmd_name() and dvb_dvb_attr_names(),
 * instead of using the tables directly.
 */

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
extern const char *dvb_sat_pol_name[6];
extern const char *dvb_user_name[DTV_USER_NAME_SIZE + 1];
extern const char *dvb_stat_name[DTV_STAT_NAME_SIZE + 1];
extern const void *dvb_user_attr_names[];

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

#endif
