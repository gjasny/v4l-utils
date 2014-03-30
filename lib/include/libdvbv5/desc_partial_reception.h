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
 * Described on IEC/CENELEC DS/EN 62216-1:2011
 *
 * I couldn't find the original version, so I used what's there at:
 *	http://tdt.telecom.pt/recursos/apresentacoes/Signalling Specifications for DTT deployment in Portugal.pdf
 */

#ifndef _PARTIAL_RECEPTION_H
#define _PARTIAL_RECEPTION_H

#include <libdvbv5/descriptors.h>

struct isdb_partial_reception_service_id {
	uint16_t service_id;
} __attribute__((packed));

struct isdb_desc_partial_reception {
	DVB_DESC_HEADER();

	struct isdb_partial_reception_service_id *partial_reception;
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

int isdb_desc_partial_reception_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc);
void isdb_desc_partial_reception_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc);
void isdb_desc_partial_reception_free(struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
