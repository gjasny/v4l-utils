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

/**
 * @file desc_ca_identifier.h
 * @ingroup descriptors
 * @brief Provides the descriptors for the Conditional Access identifier
 * @copyright GNU General Public License version 2 (GPLv2)
 * @author Andre Roth
 *
 * @par Relevant specs
 * The descriptor described herein is defined at:
 * - ETSI EN 300 468 V1.11.1 (2010-04)
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#ifndef _CA_IDENTIFIER_H
#define _CA_IDENTIFIER_H

#include <libdvbv5/descriptors.h>

/**
 * @struct dvb_desc_ca_identifier
 * @ingroup descriptors
 * @brief Indicates if a particular bouquet, service or event is associated
 *	  with a CA system
 *
 * @param type			descriptor tag
 * @param length		descriptor length
 * @param next			pointer to struct dvb_desc
 * @param caid_count		Number of CA IDs
 * @param caids			CA Identifier IDs
 */
struct dvb_desc_ca_identifier {
	uint8_t type;
	uint8_t length;
	struct dvb_desc *next;

	uint8_t caid_count;
	uint16_t *caids;

} __attribute__((packed));

struct dvb_v5_fe_parms;

/** @brief initial descriptor field at dvb_desc_ca_identifier struct */
#define dvb_desc_ca_identifier_field_first ca_id
/** @brief last descriptor field at dvb_desc_ca_identifier struct */
#define dvb_desc_ca_identifier_field_last  privdata

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses the CA identifier descriptor
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
int dvb_desc_ca_identifier_init(struct dvb_v5_fe_parms *parms,
				 const uint8_t *buf, struct dvb_desc *desc);

/**
 * @brief Prints the content of the CA identifier descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param desc	pointer to struct dvb_desc
 */
void dvb_desc_ca_identifier_print(struct dvb_v5_fe_parms *parms,
				  const struct dvb_desc *desc);

/**
 * @brief Frees all data allocated by the CA identifier descriptor
 * @ingroup descriptors
 *
 * @param desc pointer to struct dvb_desc to be freed
 */
void dvb_desc_ca_identifier_free(struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
