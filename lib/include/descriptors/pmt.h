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

#ifndef _PMT_H
#define _PMT_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#include "descriptors/header.h"

#define DVB_TABLE_PMT      2

struct dvb_table_pmt_stream {
	uint8_t type;
	union {
		uint16_t bitfield;
		struct {
			uint16_t elementary_pid:13;
			uint16_t  reserved:3;
		};
	};
	union {
		uint16_t bitfield2;
		struct {
			uint16_t section_length:10;
			uint16_t  zero:2;
			uint16_t  reserved2:4;
		};
	};
	struct dvb_desc *descriptor;
	struct dvb_table_pmt_stream *next;
} __attribute__((packed));

struct dvb_table_pmt {
	struct dvb_table_header header;
	union {
		uint16_t bitfield;
		struct {
			uint16_t pcr_pid:13;
			uint16_t reserved2:3;
		};
	};

	union {
		uint16_t bitfield2;
		struct {
			uint16_t prog_length:10;
			uint16_t  zero3:2;
			uint16_t  reserved3:4;
		};
	};
	struct dvb_table_pmt_stream *stream;
} __attribute__((packed));

#define dvb_pmt_stream_foreach(_stream, _pmt) \
  for( struct dvb_table_pmt_stream *_stream = _pmt->stream; _stream; _stream = _stream->next ) \

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

void *dvb_table_pmt_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t size);
void dvb_table_pmt_print(struct dvb_v5_fe_parms *parms, const struct dvb_table_pmt *pmt);

#ifdef __cplusplus
}
#endif

#endif
