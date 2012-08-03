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
#ifndef _DVB_FE_H
#define _DVB_FE_H

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include "dvb-frontend.h"
#include "dvb-sat.h"
#include "dvb-log.h"

#define ARRAY_SIZE(x)	(sizeof(x)/sizeof((x)[0]))

#define MAX_DELIVERY_SYSTEMS	20

/*
 * Implement the stats properties as if they were defined via S2API style.
 * This simplifies the addition of newer stats, and helps to port the
 * code to v5 style, if such change gets merged upstream.
 */

#define DTV_MAX_STATS 5
#define DTV_STATUS			(DTV_MAX_COMMAND + 100)
#define DTV_BER				(DTV_MAX_COMMAND + 101)
#define DTV_SIGNAL_STRENGTH		(DTV_MAX_COMMAND + 102)
#define DTV_SNR				(DTV_MAX_COMMAND + 103)
#define DTV_UNCORRECTED_BLOCKS		(DTV_MAX_COMMAND + 104)

enum dvbv3_emulation_type {
	DVBV3_UNKNOWN = -1,
	DVBV3_QPSK,
	DVBV3_QAM,
	DVBV3_OFDM,
	DVBV3_ATSC,
};

struct dvb_v5_stats {
	struct dtv_property		prop[DTV_MAX_STATS];
};


struct dvb_v5_fe_parms {
	int				fd;
	char				*fname;
	unsigned			verbose;
	struct dvb_frontend_info	info;
	uint32_t			version;
	fe_delivery_system_t		current_sys;
	int				num_systems;
	fe_delivery_system_t		systems[MAX_DELIVERY_SYSTEMS];
	int				n_props;
	struct dtv_property		dvb_prop[DTV_MAX_COMMAND];
	int				legacy_fe;
	struct dvb_v5_stats		stats;

	/* Satellite specific stuff, specified by the library client */
	const struct dvb_sat_lnb       	*lnb;
	int				sat_number;
	unsigned			freq_bpf;

	/* Satellite specific stuff, used internally */
	//enum dvb_sat_polarization       pol;
	int				high_band;
	unsigned			diseqc_wait;
	unsigned			freq_offset;

	int				abort;
        dvb_logfunc                     logfunc;
};


/* Open/close methods */

#ifdef __cplusplus
extern "C" {
#endif

struct dvb_v5_fe_parms *dvb_fe_open(int adapter, int frontend,
				    unsigned verbose, unsigned use_legacy_call);
struct dvb_v5_fe_parms *dvb_fe_open2(int adapter, int frontend,
				    unsigned verbose, unsigned use_legacy_call,
                                    dvb_logfunc logfunc);
void dvb_fe_close(struct dvb_v5_fe_parms *parms);

/* Get/set delivery system parameters */

const char *dvb_cmd_name(int cmd);
const char *const *dvb_attr_names(int cmd);

int dvb_fe_retrieve_parm(const struct dvb_v5_fe_parms *parms,
			unsigned cmd, uint32_t *value);
int dvb_fe_store_parm(struct dvb_v5_fe_parms *parms,
		      unsigned cmd, uint32_t value);
int dvb_set_sys(struct dvb_v5_fe_parms *parms,
		   fe_delivery_system_t sys);
int dvb_add_parms_for_sys(struct dtv_property *dvb_prop,
			  unsigned max_size,
			  fe_delivery_system_t sys);
int dvb_set_compat_delivery_system(struct dvb_v5_fe_parms *parms,
				   uint32_t desired_system);

void dvb_fe_prt_parms(const struct dvb_v5_fe_parms *parms);
int dvb_fe_set_parms(struct dvb_v5_fe_parms *parms);
int dvb_fe_get_parms(struct dvb_v5_fe_parms *parms);

/* Get statistics */

int dvb_fe_retrieve_stats(struct dvb_v5_fe_parms *parms,
			   unsigned cmd, uint32_t *value);
int dvb_fe_store_stats(struct dvb_v5_fe_parms *parms,
			unsigned cmd, uint32_t value);
int dvb_fe_get_stats(struct dvb_v5_fe_parms *parms);

/* Get both status statistics and dvb parameters */

int dvb_fe_get_event(struct dvb_v5_fe_parms *parms);

/* Ancillary functions */

const char * const *dvb_attr_names(int cmd);

/* Other functions, associated to SEC/LNB/DISEqC */

/*
 * NOTE: It currently lacks support for two ioctl's:
 * FE_DISEQC_RESET_OVERLOAD	used only on av7110.
 * Spec says:
 *   If the bus has been automatically powered off due to power overload,
 *   this ioctl call restores the power to the bus. The call requires read/write
 *   access to the device. This call has no effect if the device is manually
 *   powered off. Not all DVB adapters support this ioctl.
 *
 * FE_DISHNETWORK_SEND_LEGACY_CMD is used on av7110, budget, gp8psk and stv0299
 * Spec says:
 *   WARNING: This is a very obscure legacy command, used only at stv0299
 *   driver. Should not be used on newer drivers.
 *   It provides a non-standard method for selecting Diseqc voltage on the
 *   frontend, for Dish Network legacy switches.
 *   As support for this ioctl were added in 2004, this means that such dishes
 *   were already legacy in 2004.
 *
 * So, it doesn't make much sense on implementing support for them.
 */

int dvb_fe_sec_voltage(struct dvb_v5_fe_parms *parms, int on, int v18);
int dvb_fe_sec_tone(struct dvb_v5_fe_parms *parms, fe_sec_tone_mode_t tone);
int dvb_fe_lnb_high_voltage(struct dvb_v5_fe_parms *parms, int on);
int dvb_fe_diseqc_burst(struct dvb_v5_fe_parms *parms, int mini_b);
int dvb_fe_diseqc_cmd(struct dvb_v5_fe_parms *parms, const unsigned len,
		      const unsigned char *buf);
int dvb_fe_diseqc_reply(struct dvb_v5_fe_parms *parms, unsigned *len, char *buf,
		       int timeout);

#ifdef __cplusplus
}
#endif

/* Arrays from dvb-v5.h */

extern const unsigned fe_bandwidth_name[8];
extern const char *dvb_v5_name[61];
extern const void *dvb_v5_attr_names[];
extern const char *delivery_system_name[20];
extern const char *fe_code_rate_name[13];
extern const char *fe_modulation_name[14];
extern const char *fe_transmission_mode_name[8];
extern const unsigned fe_bandwidth_name[8];
extern const char *fe_guard_interval_name[9];
extern const char *fe_hierarchy_name[6];
extern const char *fe_voltage_name[4];
extern const char *fe_tone_name[3];
extern const char *fe_inversion_name[4];
extern const char *fe_pilot_name[4];
extern const char *fe_rolloff_name[5];

#endif
