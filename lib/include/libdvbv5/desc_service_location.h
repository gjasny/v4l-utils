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
 */

/**
 * @file desc_service_location.h
 * @ingroup descriptors
 * @brief Provides the descriptors for the DVB service location
 * @copyright GNU General Public License version 2 (GPLv2)
 * @author Andre Roth
 *
 * @par Relevant specs
 * The descriptor described herein is defined at:
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 *
 * @todo DOES THIS DESCRIPTOR EXISTS? IF SO, WHERE?
 *
 * FIXME: this seems to be a duplicated copy of the ATSC service location.
 */

#ifndef _SERVICE_LOCATION_H
#define _SERVICE_LOCATION_H

#include <libdvbv5/descriptors.h>

/**
 * @struct dvb_desc_service_location_element
 * @ingroup descriptors
 * @brief
 *
 * @param stream_type	stream type
 * @param elementary_pid	elementary pid
 * @param language	language
 */
struct dvb_desc_service_location_element {
	uint8_t stream_type;
	union {
		uint16_t bitfield;
		struct {
			uint16_t elementary_pid:13;
			uint16_t reserved:3;
		} __attribute__((packed));
	} __attribute__((packed));
	uint8_t language[4];
} __attribute__((packed));

/**
 * @struct dvb_desc_service_location
 * @ingroup descriptors
 * @brief
 *
 * @param type	type
 * @param length	length
 * @param next	pointer to struct dvb_desc
 * @param pcr_pid	pcr pid
 * @param elements	elements
 * @param element	pointer to struct dvb_desc_service_location_element
 */
struct dvb_desc_service_location {
	uint8_t type;
	uint8_t length;
	struct dvb_desc *next;

	union {
		uint16_t bitfield;
		struct {
			uint16_t pcr_pid:13;
			uint16_t reserved:3;
		} __attribute__((packed));
	} __attribute__((packed));
	uint8_t elements;
	struct dvb_desc_service_location_element *element;
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

int dvb_desc_service_location_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc);
void dvb_desc_service_location_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc);
void dvb_desc_service_location_free (struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
