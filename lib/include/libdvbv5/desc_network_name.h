/*
 * Copyright (c) 2011-2014 - Mauro Carvalho Chehab
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
 */

/**
 * @file desc_network_name.h
 * @ingroup descriptors
 * @brief Provides the descriptors for the network name descriptor
 * @copyright GNU General Public License version 2 (GPLv2)
 * @author Mauro Carvalho Chehab
 * @author Andre Roth
 *
 * @par Relevant specs
 * The descriptor described herein is defined at:
 * - ETSI EN 300 468 V1.11.1 (2010-04)
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#ifndef _DESC_NETWORK_NAME_H
#define _DESC_NETWORK_NAME_H

#include <libdvbv5/descriptors.h>

/**
 * @struct dvb_desc_network_name
 * @ingroup descriptors
 * @brief Struct containing the network name descriptor
 *
 * @param type			descriptor tag
 * @param length		descriptor length
 * @param next			pointer to struct dvb_desc
 * @param network_name		network name string
 * @param network_name_emph	network name emphasis string
 *
 * @details
 * The emphasis text is the one that uses asterisks. For example, in the text:
 *	"the quick *fox* jumps over the lazy table" the emphasis would be "fox".
 */
struct dvb_desc_network_name {
	uint8_t type;
	uint8_t length;
	struct dvb_desc *next;

	char *network_name;
	char *network_name_emph;
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses the network name descriptor
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
int dvb_desc_network_name_init(struct dvb_v5_fe_parms *parms,
			       const uint8_t *buf, struct dvb_desc *desc);

/**
 * @brief Prints the content of the network name descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param desc	pointer to struct dvb_desc
 */
void dvb_desc_network_name_print(struct dvb_v5_fe_parms *parms,
				 const struct dvb_desc *desc);

/**
 * @brief Frees all data allocated by the network name descriptor
 * @ingroup descriptors
 *
 * @param desc pointer to struct dvb_desc to be freed
 */
void dvb_desc_network_name_free(struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
