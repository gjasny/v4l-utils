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

enum dvb_streams {
	stream_reserved0	= 0x00, // ITU-T | ISO/IEC Reserved
	stream_video		= 0x01, // ISO/IEC 11172 Video
	stream_video_h262	= 0x02, // ITU-T Rec. H.262 | ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained parameter video stream
	stream_audio		= 0x03, // ISO/IEC 11172 Audio
	stream_audio_13818_3	= 0x04, // ISO/IEC 13818-3 Audio
	stream_private_sections	= 0x05, // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 private_sections
	stream_private_data	= 0x06, // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 PES packets containing private data
	stream_mheg		= 0x07, // ISO/IEC 13522 MHEG
	stream_h222		= 0x08, // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Annex A DSM-CC
	stream_h222_1		= 0x09, // ITU-T Rec. H.222.1
	stream_13818_6_A	= 0x0A, // ISO/IEC 13818-6 type A
	stream_13818_6_B	= 0x0B, // ISO/IEC 13818-6 type B
	stream_13818_6_C	= 0x0C, // ISO/IEC 13818-6 type C
	stream_13818_6_D	= 0x0D, // ISO/IEC 13818-6 type D
	stream_h222_aux		= 0x0E, // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 auxiliary
	stream_audio_adts	= 0x0F, // ISO/IEC 13818-7 Audio with ADTS transport syntax
	stream_video_14496_2	= 0x10, // ISO/IEC 14496-2 Visual
	stream_audio_latm	= 0x11, // ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3 / AMD 1
	stream_14496_1_pes	= 0x12, // ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets
	stream_14496_1_iso	= 0x13, // ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC14496_sections.
	stream_download		= 0x14, // ISO/IEC 13818-6 Synchronized Download Protocol
	stream_reserved		= 0x15, // - 0x7F, ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved
	stream_private		= 0x80  // - 0xFF, User Private
};

extern const char *pmt_stream_name[];

struct dvb_table_pmt_stream {
	uint8_t type;
	union {
		uint16_t bitfield;
		struct {
			uint16_t elementary_pid:13;
			uint16_t reserved:3;
		};
	};
	union {
		uint16_t bitfield2;
		struct {
			uint16_t section_length:10;
			uint16_t zero:2;
			uint16_t reserved2:4;
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
			uint16_t zero3:2;
			uint16_t reserved3:4;
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

void dvb_table_pmt_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, uint8_t *table, ssize_t *table_length);
void dvb_table_pmt_free(struct dvb_table_pmt *pmt);
void dvb_table_pmt_print(struct dvb_v5_fe_parms *parms, const struct dvb_table_pmt *pmt);

#ifdef __cplusplus
}
#endif

#endif
