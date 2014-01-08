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

#ifndef _ATSC_HEADER_H
#define _ATSC_HEADER_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#define ATSC_BASE_PID  0x1FFB

struct atsc_table_header {
	uint8_t  table_id;
	union {
		uint16_t bitfield;
		struct {
			uint16_t section_length:12;
			uint16_t one:2;
			uint16_t priv:1;
			uint16_t syntax:1;
		} __attribute__((packed));
	};
	uint16_t id;
	uint8_t  current_next:1;
	uint8_t  version:5;
	uint8_t  one2:2;

	uint8_t  section_id;
	uint8_t  last_section;
	uint8_t  protocol_version;
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

int  atsc_table_header_init (struct atsc_table_header *t);
void atsc_table_header_print(struct dvb_v5_fe_parms *parms, const struct atsc_table_header *t);

#ifdef __cplusplus
}
#endif

#endif
