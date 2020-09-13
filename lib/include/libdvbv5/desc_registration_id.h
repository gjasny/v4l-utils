/*
 * Copyright (c) 2020 - Mauro Carvalho Chehab
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

#ifndef _DESC_REGISTRATION_ID_H
#define _DESC_REGISTRATION_ID_H

#include <libdvbv5/descriptors.h>
/**
 * @file desc_registration_id.h
 * @ingroup descriptors
 * @brief Provides the descriptors for the registration descriptor.
 *	  This descriptor provides the format information for an
 *	  Elementary Stream.
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Mauro Carvalho Chehab
 *
 * @par Relevant specs
 * The descriptor described herein is defined at:
 * - ISO/IEC 13818-1
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

/**
 * @struct dvb_desc_registration
 * @ingroup descriptors
 * @brief Struct containing the frequency list descriptor
 *
 * @param type			descriptor tag
 * @param length		descriptor length
 * @param format_identifier	32-bit value obtained from ISO/IEC JTC 1/SC 29
 *				which describes the format of the ES
 *				The length of the vector is given by:
 *				length - 4.
 */
struct dvb_desc_registration {
	uint8_t type;
	uint8_t length;
	struct dvb_desc *next;

	uint8_t format_identifier[4];
	uint8_t *additional_identification_info;
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses the registration descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param buf	buffer containing the descriptor's raw data
 * @param desc	pointer to struct dvb_desc to be allocated and filled
 *
 * This function initializes and makes sure that all fields will follow the CPU
 * endianness. Due to that, the content of the buffer may change.
 *
 * Currently, no memory is allocated internally.
 *
 * @return On success, it returns the size of the allocated struct.
 *	   A negative value indicates an error.
 */
int dvb_desc_registration_init(struct dvb_v5_fe_parms *parms,
			       const uint8_t *buf, struct dvb_desc *desc);

/**
 * @brief Prints the content of the registration descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param desc	pointer to struct dvb_desc
 */
void dvb_desc_registration_print(struct dvb_v5_fe_parms *parms,
				 const struct dvb_desc *desc);

/**
 * @brief Frees all data allocated by the registration descriptor
 * @ingroup descriptors
 *
 * @param desc pointer to struct dvb_desc to be freed
 */
void dvb_desc_registration_free(struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
