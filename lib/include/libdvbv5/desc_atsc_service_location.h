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
 */

#ifndef _ATSC_SERVICE_LOCATION_H
#define _ATSC_SERVICE_LOCATION_H

#include <libdvbv5/descriptors.h>

struct atsc_desc_service_location_elementary {
	uint8_t stream_type;
	union {
		uint16_t bitfield;
		struct {
			uint16_t elementary_pid:13;
			uint16_t reserved:3;
		} __attribute__((packed));
	} __attribute__((packed));
	char ISO_639_language_code[3];
} __attribute__((packed));

struct atsc_desc_service_location {
	DVB_DESC_HEADER();

	struct atsc_desc_service_location_elementary *elementary;

	union {
		uint16_t bitfield;
		struct {
			uint16_t pcr_pid:13;
			uint16_t reserved:3;
		} __attribute__((packed));
	} __attribute__((packed));

	uint8_t number_elements;
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

int atsc_desc_service_location_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc);
void atsc_desc_service_location_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc);
void atsc_desc_service_location_free(struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
