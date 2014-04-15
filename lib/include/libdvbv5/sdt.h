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
 */

#ifndef _SDT_H
#define _SDT_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#include <libdvbv5/header.h>

#define DVB_TABLE_SDT      0x42
#define DVB_TABLE_SDT2     0x46
#define DVB_TABLE_SDT_PID  0x0011

struct dvb_table_sdt_service {
	uint16_t service_id;
	uint8_t EIT_present_following:1;
	uint8_t EIT_schedule:1;
	uint8_t reserved:6;
	union {
		uint16_t bitfield;
		struct {
			uint16_t desc_length:12;
			uint16_t free_CA_mode:1;
			uint16_t running_status:3;
		} __attribute__((packed));
	} __attribute__((packed));
	struct dvb_desc *descriptor;
	struct dvb_table_sdt_service *next;
} __attribute__((packed));

struct dvb_table_sdt {
	struct dvb_table_header header;
	uint16_t network_id;
	uint8_t  reserved;
	struct dvb_table_sdt_service *service;
} __attribute__((packed));

#define dvb_sdt_service_foreach(_service, _sdt) \
	for (struct dvb_table_sdt_service *_service = _sdt->service; _service; _service = _service->next ) \

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

ssize_t dvb_table_sdt_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, struct dvb_table_sdt **table);
void dvb_table_sdt_free(struct dvb_table_sdt *sdt);
void dvb_table_sdt_print(struct dvb_v5_fe_parms *parms, struct dvb_table_sdt *sdt);

#ifdef __cplusplus
}
#endif

#endif
