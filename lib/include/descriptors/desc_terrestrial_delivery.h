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

#ifndef _TERRESTRIAL_DELIVERY_H
#define _TERRESTRIAL_DELIVERY_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

struct dvb_desc_terrestrial_delivery {
	uint8_t type;
	uint8_t length;
	struct dvb_desc *next;

	uint32_t centre_frequency;
	uint8_t reserved_future_use1:2;
	uint8_t mpe_fec_indicator:1;
	uint8_t time_slice_indicator:1;
	uint8_t priority:1;
	uint8_t bandwidth:3;
	uint8_t code_rate_hp_stream:3;
	uint8_t hierarchy_information:3;
	uint8_t constellation:2;
	uint8_t other_frequency_flag:1;
	uint8_t transmission_mode:2;
	uint8_t guard_interval:2;
	uint8_t code_rate_lp_stream:3;
	uint32_t reserved_future_use2;
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

void dvb_desc_terrestrial_delivery_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc);
void dvb_desc_terrestrial_delivery_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
