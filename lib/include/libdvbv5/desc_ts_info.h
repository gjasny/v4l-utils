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
 * Described on ARIB STD-B10 as TS information descriptor
 */

#ifndef _TS_INFO_H
#define _TS_INFO_H

#include <libdvbv5/descriptors.h>

struct dvb_desc_ts_info_transmission_type {
	uint8_t transmission_type_info;
	uint8_t num_of_service;
} __attribute__((packed));

struct dvb_desc_ts_info {
	DVB_DESC_HEADER();

	char *ts_name, *ts_name_emph;
	struct dvb_desc_ts_info_transmission_type transmission_type;
	uint16_t *service_id;

	uint8_t remote_control_key_id;
	union {
		uint8_t bitfield;
		struct {
			uint8_t transmission_type_count:2;
			uint8_t length_of_ts_name:6;
		} __attribute__((packed));
	};
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

int dvb_desc_ts_info_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc);
void dvb_desc_ts_info_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc);
void dvb_desc_ts_info_free(struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
