/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab <mchehab@redhat.com>
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

#ifndef _EIT_H
#define _EIT_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */
#include <time.h>

#include "descriptors/header.h"
#include "descriptors.h"

#define DVB_TABLE_EIT        0x4E
#define DVB_TABLE_EIT_OTHER  0x4F

#define DVB_TABLE_EIT_SCHEDULE 0x50       /* - 0x5F */
#define DVB_TABLE_EIT_SCHEDULE_OTHER 0x60 /* - 0x6F */

#define DVB_TABLE_EIT_PID  0x12

struct dvb_table_eit_event {
	uint16_t event_id;
	union {
		uint16_t bitfield;
		uint8_t dvbstart[5];
	} __attribute__((packed));
	uint8_t dvbduration[3];
	union {
		uint16_t bitfield2;
		struct {
			uint16_t section_length:12;
			uint16_t free_CA_mode:1;
			uint16_t running_status:3;
		} __attribute__((packed));
	} __attribute__((packed));
	struct dvb_desc *descriptor;
	struct dvb_table_eit_event *next;
	struct tm start;
	uint32_t duration;
} __attribute__((packed));

struct dvb_table_eit {
	struct dvb_table_header header;
	uint16_t transport_id;
	uint16_t network_id;
	uint8_t  last_segment;
	uint8_t  last_table_id;
	struct dvb_table_eit_event *event;
} __attribute__((packed));

#define dvb_eit_event_foreach(_event, _eit) \
	for( struct dvb_table_eit_event *_event = _eit->event; _event; _event = _event->next ) \

struct dvb_v5_fe_parms;

extern const char *dvb_eit_running_status_name[8];

#ifdef __cplusplus
extern "C" {
#endif

void dvb_table_eit_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, uint8_t *table, ssize_t *table_length);
void dvb_table_eit_free(struct dvb_table_eit *eit);
void dvb_table_eit_print(struct dvb_v5_fe_parms *parms, struct dvb_table_eit *eit);
void dvb_time(const uint8_t data[5], struct tm *tm);

#ifdef __cplusplus
}
#endif

#endif
