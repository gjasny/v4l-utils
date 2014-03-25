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

#ifndef _MPEG_TS_H
#define _MPEG_TS_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#define DVB_MPEG_TS  0x47
#define DVB_MPEG_TS_PACKET_SIZE  188

struct dvb_mpeg_ts_adaption {
	uint8_t length;
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
	uint8_t data[];
} __attribute__((packed));

struct dvb_mpeg_ts {
	uint8_t sync_byte; // DVB_MPEG_TS
	union {
		uint16_t bitfield;
		struct {
			uint16_t pid:13;
			uint16_t priority:1;
			uint16_t payload_start:1;
			uint16_t tei:1;
		} __attribute__((packed));
	} __attribute__((packed));
	struct {
		uint8_t continuity_counter:4;
		uint8_t payload:1;
		uint8_t adaptation_field:1;
		uint8_t scrambling:2;
	} __attribute__((packed));
	struct dvb_mpeg_ts_adaption adaption[];
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

ssize_t dvb_mpeg_ts_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, uint8_t *table, ssize_t *table_length);
void dvb_mpeg_ts_free(struct dvb_mpeg_ts *ts);
void dvb_mpeg_ts_print(struct dvb_v5_fe_parms *parms, struct dvb_mpeg_ts *ts);

#ifdef __cplusplus
}
#endif

#endif
