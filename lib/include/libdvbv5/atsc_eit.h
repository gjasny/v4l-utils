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

#ifndef _ATSC_EIT_H
#define _ATSC_EIT_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */
#include <time.h>

#include <libdvbv5/atsc_header.h>

#define ATSC_TABLE_EIT        0xCB

struct atsc_table_eit_event {
	union {
		uint16_t bitfield;
		struct {
	          uint16_t event_id:14;
	          uint16_t one:2;
		} __attribute__((packed));
	} __attribute__((packed));
	uint32_t start_time;
	union {
		uint32_t bitfield2;
		struct {
			uint32_t title_length:8;
			uint32_t duration:20;
			uint32_t etm:2;
			uint32_t one2:2;
			uint32_t :2;
		} __attribute__((packed));
	} __attribute__((packed));
	struct dvb_desc *descriptor;
	struct atsc_table_eit_event *next;
	struct tm start;
	uint16_t source_id;
} __attribute__((packed));

union atsc_table_eit_desc_length {
	uint16_t bitfield;
	struct {
		uint16_t desc_length:12;
		uint16_t reserved:4;
	} __attribute__((packed));
} __attribute__((packed));

struct atsc_table_eit {
	ATSC_HEADER();
	uint8_t events;
	struct atsc_table_eit_event *event;
} __attribute__((packed));

#define atsc_eit_event_foreach(_event, _eit) \
	for( struct atsc_table_eit_event *_event = _eit->event; _event; _event = _event->next ) \

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

ssize_t atsc_table_eit_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, struct atsc_table_eit **table);
void atsc_table_eit_free(struct atsc_table_eit *eit);
void atsc_table_eit_print(struct dvb_v5_fe_parms *parms, struct atsc_table_eit *eit);
void atsc_time(const uint32_t start_time, struct tm *tm);

#ifdef __cplusplus
}
#endif

#endif
