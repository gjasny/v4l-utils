/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
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
 * Described at ISO/IEC 13818-1
 */

#ifndef _HEADER_H
#define _HEADER_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

/**
 * @file header.h
 * @ingroup dvb_table
 * @brief Provides the MPEG TS table headers
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Mauro Carvalho Chehab
 * @author Andre Roth
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

/**
 * @struct dvb_ts_packet_header
 * @brief Header of a MPEG-TS transport packet
 * @ingroup dvb_table
 *
 * @param sync_byte			sync byte
 * @param pid				Program ID
 * @param transport_priority		transport priority
 * @param payload_unit_start_indicator	payload unit start indicator
 * @param transport_error_indicator	transport error indicator
 * @param continuity_counter		continuity counter
 * @param adaptation_field_control	adaptation field control
 * @param transport_scrambling_control	transport scrambling control
 * @param adaptation_field_length	adaptation field length
 *
 * @see http://www.etherguidesystems.com/Help/SDOs/MPEG/Semantics/MPEG-2/transport_packet.aspx
 */
struct dvb_ts_packet_header {
	uint8_t  sync_byte;
	union {
		uint16_t bitfield;
		struct {
			uint16_t pid:13;
			uint16_t transport_priority:1;
			uint16_t payload_unit_start_indicator:1;
			uint16_t transport_error_indicator:1;
		} __attribute__((packed));
	} __attribute__((packed));
	uint8_t continuity_counter:4;
	uint8_t adaptation_field_control:2;
	uint8_t transport_scrambling_control:2;

	/* Only if adaptation_field_control > 1 */
	uint8_t adaptation_field_length;
	/* Only if adaptation_field_length >= 1 */
	struct {
		uint8_t extension:1;
		uint8_t private_data:1;
		uint8_t splicing_point:1;
		uint8_t OPCR:1;
		uint8_t PCR:1;
		uint8_t priority:1;
		uint8_t random_access:1;
		uint8_t discontinued:1;
	} __attribute__((packed));
} __attribute__((packed));

/**
 * @struct dvb_table_header
 * @brief Header of a MPEG-TS table
 * @ingroup dvb_table
 *
 * @param table_id		table id
 * @param section_length	section length
 * @param syntax		syntax
 * @param id			Table ID extension
 * @param current_next		current next
 * @param version		version
 * @param section_id		section number
 * @param last_section		last section number
 *
 * All MPEG-TS tables start with this header.
 */
struct dvb_table_header {
	uint8_t  table_id;
	union {
		uint16_t bitfield;
		struct {
			uint16_t section_length:12;
			uint8_t  one:2;
			uint8_t  zero:1;
			uint8_t  syntax:1;
		} __attribute__((packed));
	} __attribute__((packed));
	uint16_t id;			/* TS ID */
	uint8_t  current_next:1;
	uint8_t  version:5;
	uint8_t  one2:2;

	uint8_t  section_id;		/* section_number */
	uint8_t  last_section;		/* last_section_number */
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses MPEG-TS table header
 * @ingroup dvb_table
 *
 * @param header pointer to struct dvb_table_header to be parsed
 */
void dvb_table_header_init (struct dvb_table_header *header);
/**
 * @brief Prints the content of the MPEG-TS table header
 * @ingroup dvb_table
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param header pointer to struct dvb_table_header to be printed
 */
void dvb_table_header_print(struct dvb_v5_fe_parms *parms,
			    const struct dvb_table_header *header);

#ifdef __cplusplus
}
#endif

#endif
