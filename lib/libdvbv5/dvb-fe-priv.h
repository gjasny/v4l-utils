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
 */

#ifndef __DVB_FE_PRIV_H
#define __DVB_FE_PRIV_H

#include <libdvbv5/dvb-fe.h>
#include <libdvbv5/countries.h>

enum dvbv3_emulation_type {
	DVBV3_UNKNOWN = -1,
	DVBV3_QPSK,
	DVBV3_QAM,
	DVBV3_OFDM,
	DVBV3_ATSC,
};

struct dvb_v5_counters {
	uint64_t			pre_bit_count;
	uint64_t			pre_bit_error;
	uint64_t			post_bit_count;
	uint64_t			post_bit_error;
	uint64_t			block_count;
	uint64_t			block_error;
};

struct dvb_v5_stats {
	struct dtv_property		prop[DTV_NUM_STATS_PROPS];

	struct dvb_v5_counters		prev[MAX_DTV_STATS];
	struct dvb_v5_counters		cur[MAX_DTV_STATS];

	int				has_post_ber[MAX_DTV_STATS];
	int				has_pre_ber[MAX_DTV_STATS];
	int				has_per[MAX_DTV_STATS];

	fe_status_t prev_status;

};

struct dvb_device_priv;

struct dvb_v5_fe_parms_priv {
	/* dvbv_v4_fe_parms should be the first element on this struct */
	struct dvb_v5_fe_parms		p;

	struct dvb_device_priv		*dvb;

	int				fd;
	int				fe_flags;	/* open() flags */
	char				*fname;
	int				n_props;
	struct dtv_property		dvb_prop[DTV_MAX_COMMAND];
	struct dvb_v5_stats		stats;

	/* country variant of the delivery system */
	enum dvb_country_t		country;

	/* Satellite specific stuff */
	int				high_band;
	unsigned			freq_offset;

	dvb_logfunc_priv		logfunc_priv;
	void				*logpriv;
};

/* Functions used internally by dvb-dev.c. Aren't part of the API */
int dvb_fe_open_fname(struct dvb_v5_fe_parms_priv *parms, char *fname,
		      int flags);
void dvb_v5_free(struct dvb_v5_fe_parms_priv *parms);
void __dvb_fe_close(struct dvb_v5_fe_parms_priv *parms);

/* Functions that can be overriden to be executed remotely */
int __dvb_set_sys(struct dvb_v5_fe_parms *p, fe_delivery_system_t sys);
int __dvb_fe_get_parms(struct dvb_v5_fe_parms *p);
int __dvb_fe_set_parms(struct dvb_v5_fe_parms *p);
int __dvb_fe_get_stats(struct dvb_v5_fe_parms *p);

#endif
