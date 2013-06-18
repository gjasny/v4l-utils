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
 */
#ifndef _LIBSAT_H
#define _LIBSAT_H

#include "dvb-v5-std.h"

struct dvbsat_freqrange {
	unsigned low, high;
};

struct dvb_sat_lnb {
	const char *name;
	const char *alias;
	unsigned lowfreq, highfreq;

	unsigned rangeswitch;

	struct dvbsat_freqrange freqrange[2];
};

struct dvb_v5_fe_parms;

extern const char *dvbsat_polarization_name[5];

#ifdef __cplusplus
extern "C" {
#endif

/* From libsat.c */
int dvb_sat_search_lnb(const char *name);
int print_lnb(int i);
void print_all_lnb(void);
const struct dvb_sat_lnb *dvb_sat_get_lnb(int i);
int dvb_sat_set_parms(struct dvb_v5_fe_parms *parms);

#ifdef __cplusplus
}
#endif

#endif // _LIBSAT_H
