/*
 * Copyright (c) 2013-2014 - Mauro Carvalho Chehab <m.chehab@samsung.com>
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

/**
 * @file desc_atsc_service_location.h
 * @ingroup descriptors
 * @brief Provides the descriptors for ATSC service location
 * @copyright GNU General Public License version 2 (GPLv2)
 * @author Mauro Carvalho Chehab
 *
 * @par Relevant specs
 * The descriptor described herein is defined at:
 * -  ATSC A/53
 *
 * @see http://www.etherguidesystems.com/help/sdos/atsc/semantics/descriptors/ServiceLocation.aspx
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

/**
 * @struct atsc_desc_service_location_elementary
 * @ingroup descriptors
 * @brief service location elementary descriptors
 *
 * @param stream_type		stream type
 * @param elementary_pid	elementary pid
 * @param ISO_639_language_code	ISO 639 language code
 */
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

/**
 * @struct atsc_desc_service_location
 * @ingroup descriptors
 * @brief Describes the elementary streams inside a PAT table for ATSC
 *
 * @param type			descriptor tag
 * @param length		descriptor length
 * @param next			pointer to struct dvb_desc
 * @param elementary		pointer to struct atsc_desc_service_location_elementary
 * @param pcr_pid		PCR pid
 * @param number_elements	number elements
 */
struct atsc_desc_service_location {
	uint8_t type;
	uint8_t length;
	struct dvb_desc *next;

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

/**
 * @brief Initializes and parses the service location descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param buf	buffer containing the descriptor's raw data
 * @param desc	pointer to struct dvb_desc to be allocated and filled
 *
 * This function allocates a the descriptor and fills the fields inside
 * the struct. It also makes sure that all fields will follow the CPU
 * endianness. Due to that, the content of the buffer may change.
 *
 * @return On success, it returns the size of the allocated struct.
 *	   A negative value indicates an error.
 */
int atsc_desc_service_location_init(struct dvb_v5_fe_parms *parms,
				    const uint8_t *buf,
				    struct dvb_desc *desc);

/**
 * @brief Prints the content of the service location descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param desc	pointer to struct dvb_desc
 */
void atsc_desc_service_location_print(struct dvb_v5_fe_parms *parms,
				      const struct dvb_desc *desc);

/**
 * @brief Frees all data allocated by the service location descriptor
 * @ingroup descriptors
 *
 * @param desc pointer to struct dvb_desc to be freed
 */
void atsc_desc_service_location_free(struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
