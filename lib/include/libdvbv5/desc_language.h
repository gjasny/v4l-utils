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
 * @file desc_language.h
 * @ingroup descriptors
 * @brief Provides the descriptors for the ISO639 language descriptor
 * @copyright GNU General Public License version 2 (GPLv2)
 * @author Mauro Carvalho Chehab
 * @author Andre Roth
 *
 * @par Relevant specs
 * The descriptor described herein is defined at:
 * - ISO/IEC 13818-1
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#ifndef _DESC_LANGUAGE_H
#define _DESC_LANGUAGE_H

#include <libdvbv5/descriptors.h>

/**
 * @struct dvb_desc_language
 * @ingroup descriptors
 * @brief Structure containing the ISO639 language descriptor
 *
 * @param type			descriptor tag
 * @param length		descriptor length
 * @param next			pointer to struct dvb_desc
 * @param language		ISO639 language string
 * @param audio_type		audio type: 0 = undefined, 1 = clean effects,
 *				2 = hearing impaired, 3 = visual impired
 *				comentary, other values are reserved.
 */
struct dvb_desc_language {
	uint8_t type;
	uint8_t length;
	struct dvb_desc *next;

	unsigned char language[4];
	uint8_t audio_type;
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses the language descriptor
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
int dvb_desc_language_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			   struct dvb_desc *desc);

/**
 * @brief Prints the content of the language descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param desc	pointer to struct dvb_desc
 */
void dvb_desc_language_print(struct dvb_v5_fe_parms *parms,
			     const struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
