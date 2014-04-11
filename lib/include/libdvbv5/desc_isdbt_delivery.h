/*
 * Copyright (c) 2013 - Mauro Carvalho Chehab <m.chehab@samsung.com>
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
 * Described on ARIB STD-B10 as Terrestrial delivery system descriptor
 */

#ifndef _ISDBT_DELIVERY_H
#define _ISDBT_DELIVERY_H

#include <libdvbv5/descriptors.h>

struct isdbt_desc_terrestrial_delivery_system {
	DVB_DESC_HEADER();

	uint32_t *frequency;
	unsigned num_freqs;

	union {
		uint16_t bitfield;
		struct {
			uint16_t transmission_mode:2;
			uint16_t guard_interval:2;
			uint16_t area_code:12;
		} __attribute__((packed));
	} __attribute__((packed));
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

int isdbt_desc_delivery_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc);
void isdbt_desc_delivery_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc);
void isdbt_desc_delivery_free(struct dvb_desc *desc);

extern const uint32_t isdbt_interval[];
extern const uint32_t isdbt_mode[];

#ifdef __cplusplus
}
#endif

#endif
