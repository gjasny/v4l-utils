/*
 * Copyright (c) 2011-2014 - Mauro Carvalho Chehab
 * Copyright (c) 2012 - Andre Roth <neolynx@gmail.com>
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

/**
 * @file desc_event_short.h
 * @ingroup descriptors
 * @brief Provides the descriptors for the short event descriptor
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Mauro Carvalho Chehab
 * @author Andre Roth
 *
 * @par Relevant specs
 * The descriptor described herein is defined at:
 * - ETSI EN 300 468 V1.11.1
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#ifndef _DESC_EVENT_SHORT_H
#define _DESC_EVENT_SHORT_H

#include <libdvbv5/descriptors.h>

/**
 * @struct dvb_desc_event_short
 * @ingroup descriptors
 * @brief Structure containing the short event descriptor
 *
 * @param type			descriptor tag
 * @param length		descriptor length
 * @param next			pointer to struct dvb_desc
 * @param language		ISO 639 language code
 * @param name			event name string
 * @param name_emph		event name emphasis string
 * @param text			event text string
 * @param text_emph		event text emphasis string
 *
 * @details
 * The emphasis text is the one that uses asterisks. For example, in the text:
 *	"the quick *fox* jumps over the lazy table" the emphasis would be "fox".
 */
struct dvb_desc_event_short {
	uint8_t type;
	uint8_t length;
	struct dvb_desc *next;

	unsigned char language[4];
	char *name;
	char *name_emph;
	char *text;
	char *text_emph;
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses the short event descriptor
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
int dvb_desc_event_short_init(struct dvb_v5_fe_parms *parms,
			      const uint8_t *buf, struct dvb_desc *desc);

/**
 * @brief Prints the content of the short event descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param desc	pointer to struct dvb_desc
 */
void dvb_desc_event_short_print(struct dvb_v5_fe_parms *parms,
				const struct dvb_desc *desc);

/**
 * @brief Frees all data allocated by the short event descriptor
 * @ingroup descriptors
 *
 * @param desc pointer to struct dvb_desc to be freed
 */
void dvb_desc_event_short_free(struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
