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

#ifndef _MPEG_PES_H
#define _MPEG_PES_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#define DVB_MPEG_PES  0x00001
#define DVB_MPEG_PES_VIDEO  0xe0 ... 0xef

#define DVB_MPEG_STREAM_MAP       0xBC
#define DVB_MPEG_STREAM_PADDING   0xBE
#define DVB_MPEG_STREAM_PRIVATE_2 0x5F
#define DVB_MPEG_STREAM_ECM       0x70
#define DVB_MPEG_STREAM_EMM       0x71
#define DVB_MPEG_STREAM_DIRECTORY 0xFF
#define DVB_MPEG_STREAM_DSMCC     0x7A
#define DVB_MPEG_STREAM_H222E     0xF8

struct ts_t {
	uint8_t  one:1;
	uint8_t  bits30:3;
	uint8_t  tag:4;

	union {
		uint16_t bitfield;
		struct {
			uint16_t  one1:1;
			uint16_t  bits15:15;
		} __attribute__((packed));
	} __attribute__((packed));

	union {
		uint16_t bitfield2;
		struct {
			uint16_t  one2:1;
			uint16_t  bits00:15;
		} __attribute__((packed));
	} __attribute__((packed));
} __attribute__((packed));

struct dvb_mpeg_pes_optional {
	union {
		uint16_t bitfield;
		struct {
			uint16_t PES_extension:1;
			uint16_t PES_CRC:1;
			uint16_t additional_copy_info:1;
			uint16_t DSM_trick_mode:1;
			uint16_t ES_rate:1;
			uint16_t ESCR:1;
			uint16_t PTS_DTS:2;
			uint16_t original_or_copy:1;
			uint16_t copyright:1;
			uint16_t data_alignment_indicator:1;
			uint16_t PES_priority:1;
			uint16_t PES_scrambling_control:2;
			uint16_t two:2;
		} __attribute__((packed));
	} __attribute__((packed));
	uint8_t length;
	uint64_t pts;
	uint64_t dts;
} __attribute__((packed));

struct dvb_mpeg_pes {
	union {
		uint32_t bitfield;
		struct {
			uint32_t  stream_id:8;
			uint32_t  sync:24;
		} __attribute__((packed));
	} __attribute__((packed));
	uint16_t length;
	struct dvb_mpeg_pes_optional optional[];
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

ssize_t dvb_mpeg_pes_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, uint8_t *table);
void dvb_mpeg_pes_free(struct dvb_mpeg_pes *ts);
void dvb_mpeg_pes_print(struct dvb_v5_fe_parms *parms, struct dvb_mpeg_pes *ts);

#ifdef __cplusplus
}
#endif

#endif
