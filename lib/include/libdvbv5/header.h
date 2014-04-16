/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
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
 * Described at ISO/IEC 13818-1
 */

#ifndef _HEADER_H
#define _HEADER_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

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
} __attribute__((packed));

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

void dvb_table_header_init (struct dvb_table_header *t);
void dvb_table_header_print(struct dvb_v5_fe_parms *parms, const struct dvb_table_header *t);

#ifdef __cplusplus
}
#endif

#endif
