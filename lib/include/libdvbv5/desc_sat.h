/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 * Copyright (c) 2012 - Andre Roth <neolynx@gmail.com>
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

#ifndef _SAT_H
#define _SAT_H

#include <libdvbv5/descriptors.h>

struct dvb_desc_sat {
	DVB_DESC_HEADER();

	uint32_t frequency;
	uint16_t orbit;
	uint8_t modulation_type:2;
	uint8_t modulation_system:1;
	uint8_t roll_off:2;
	uint8_t polarization:2;
	uint8_t west_east:1;
	union {
		uint32_t bitfield;
		struct {
			uint32_t fec:4;
			uint32_t symbol_rate:28;
		} __attribute__((packed));
	} __attribute__((packed));
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

int dvb_desc_sat_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc);
void dvb_desc_sat_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc);

extern const unsigned dvbs_dvbc_dvbs_freq_inner[];
extern const unsigned dvbs_polarization[];
extern const unsigned dvbs_rolloff[];
extern const unsigned dvbs_modulation[];

#ifdef __cplusplus
}
#endif

#endif
