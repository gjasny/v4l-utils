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
 * Described at ETSI EN 300 468 V1.11.1 (2010-04)
 */

#ifndef _CABLE_DELIVERY_H
#define _CABLE_DELIVERY_H

#include <libdvbv5/descriptors.h>

struct dvb_desc_cable_delivery {
	DVB_DESC_HEADER();

	uint32_t frequency;
	union {
		uint16_t bitfield1;
		struct {
			uint16_t fec_outer:4;
			uint16_t reserved_future_use:12;
		} __attribute__((packed));
	} __attribute__((packed));
	uint8_t modulation;
	union {
		uint32_t bitfield2;
		struct {
			uint32_t fec_inner:4;
			uint32_t symbol_rate:28;
		} __attribute__((packed));
	} __attribute__((packed));
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

int dvb_desc_cable_delivery_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc);
void dvb_desc_cable_delivery_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc);

extern const unsigned dvbc_modulation_table[];
extern const unsigned dvbc_fec_table[];


#ifdef __cplusplus
}
#endif

#endif
