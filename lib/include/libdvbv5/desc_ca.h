/*
 * Copyright (c) 2013 - Andre Roth <neolynx@gmail.com>
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

#ifndef _CA_H
#define _CA_H

#include <libdvbv5/descriptors.h>

struct dvb_desc_ca {
	DVB_DESC_HEADER();

	uint16_t ca_id;
	union {
		uint16_t bitfield1;
		struct {
			uint16_t ca_pid:13;
			uint16_t reserved:3;
		} __attribute__((packed));
	} __attribute__((packed));

	uint8_t *privdata;
	uint8_t privdata_len;
} __attribute__((packed));

struct dvb_v5_fe_parms;

#define dvb_desc_ca_field_first ca_id
#define dvb_desc_ca_field_last  privdata

#ifdef __cplusplus
extern "C" {
#endif

int dvb_desc_ca_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc);
void dvb_desc_ca_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc);
void dvb_desc_ca_free (struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
