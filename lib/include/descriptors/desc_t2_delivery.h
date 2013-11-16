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
 * Based on ETSI EN 300 468 V1.11.1 (2010-04)
 */

#ifndef _T2_DELIVERY_H
#define _T2_DELIVERY_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

struct dvb_desc_t2_delivery_subcell {
	uint8_t cell_id_extension;
	uint16_t transposer_frequency;
} __attribute__((packed));

struct dvb_desc_t2_delivery {
	uint8_t type;
	uint8_t length;
	struct dvb_desc *next;

	uint32_t *centre_frequency;
	uint8_t frequency_loop_length;
	uint8_t subcel_info_loop_length;
	struct dvb_desc_t2_delivery_subcell *subcell;

	uint8_t descriptor_tag_extension;
	uint8_t plp_id;
	uint16_t system_id;
	union {
		uint16_t bitfield;
		struct {
			uint16_t tfs_flag:1;
			uint16_t other_frequency_flag:1;
			uint16_t transmission_mode:3;
			uint16_t guard_interval:3;
			uint16_t reserved:2;
			uint16_t bandwidth:3;
			uint16_t SISO_MISO:2;
		};
	};
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

void dvb_desc_t2_delivery_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc);
void dvb_desc_t2_delivery_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc);
void dvb_desc_t2_delivery_free(struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
