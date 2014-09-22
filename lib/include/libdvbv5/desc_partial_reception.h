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

/**
 * @file desc_partial_reception.h
 * @ingroup descriptors
 * @brief Provides the descriptors for the ISDB partial reception descriptor
 * @copyright GNU General Public License version 2 (GPLv2)
 * @author Mauro Carvalho Chehab
 * @author Andre Roth
 *
 * @par Relevant specs
 * The descriptor described herein is defined at:
 * - IEC/CENELEC DS/EN 62216-1:2011
 *
 * @see http://tdt.telecom.pt/recursos/apresentacoes/Signalling Specifications for DTT deployment in Portugal.pdf
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */


#ifndef _PARTIAL_RECEPTION_H
#define _PARTIAL_RECEPTION_H

#include <libdvbv5/descriptors.h>

/**
 * @struct isdb_partial_reception_service_id
 * @ingroup descriptors
 * @brief Service ID that uses partial reception
 *
 * @param service_id	service id
 */
struct isdb_partial_reception_service_id {
	uint16_t service_id;
} __attribute__((packed));

/**
 * @struct isdb_desc_partial_reception
 * @ingroup descriptors
 * @brief Structure containing the partial reception descriptor
 *
 * @param type			descriptor tag
 * @param length		descriptor length
 * @param next			pointer to struct dvb_desc
 * @param partial_reception	vector of struct isdb_partial_reception_service_id.
 *				The length of the vector is given by:
 *				length / sizeof(struct isdb_partial_reception_service_id).
 */
struct isdb_desc_partial_reception {
	uint8_t type;
	uint8_t length;
	struct dvb_desc *next;

	struct isdb_partial_reception_service_id *partial_reception;
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses the ISDB-T partial reception descriptor
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
int isdb_desc_partial_reception_init(struct dvb_v5_fe_parms *parms,
				     const uint8_t *buf, struct dvb_desc *desc);

/**
 * @brief Prints the content of the ISDB-T partial reception descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param desc	pointer to struct dvb_desc
 */
void isdb_desc_partial_reception_print(struct dvb_v5_fe_parms *parms,
				       const struct dvb_desc *desc);

/**
 * @brief Frees all data allocated by the ISDB-T partial reception descriptor
 * @ingroup descriptors
 *
 * @param desc pointer to struct dvb_desc to be freed
 */
void isdb_desc_partial_reception_free(struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
