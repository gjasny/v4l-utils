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

/**
 * @file pmt.h
 * @ingroup dvb_table
 * @brief Provides the descriptors for PMT MPEG-TS table
 * @copyright GNU General Public License version 2 (GPLv2)
 * @author Mauro Carvalho Chehab
 * @author Andre Roth
 *
 * @par Relevant specs
 * The table described herein is defined at:
 * - ISO/IEC 13818-1
 *
 * @see http://www.etherguidesystems.com/help/sdos/mpeg/syntax/tablesections/pmts.aspx
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#ifndef _PMT_H
#define _PMT_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#include <libdvbv5/header.h>

/**
 * @def DVB_TABLE_PMT
 *	@brief PMT table ID
 *	@ingroup dvb_table
 */
#define DVB_TABLE_PMT      0x02

/**
 * @enum dvb_streams
 * @brief Add support for MPEG-TS Stream types
 * @ingroup dvb_table
 *
 * @var stream_reserved0
 *	@brief	ITU-T | ISO/IEC Reserved
 * @var stream_video
 *	@brief	ISO/IEC 11172 Video
 * @var stream_video_h262
 *	@brief	ITU-T Rec. H.262 | ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained parameter video stream
 * @var stream_audio
 *	@brief	ISO/IEC 11172 Audio
 * @var stream_audio_13818_3
 *	@brief	ISO/IEC 13818-3 Audio
 * @var stream_private_sections
 *	@brief	ITU-T Rec. H.222.0 | ISO/IEC 13818-1 private_sections
 * @var stream_private_data
 *	@brief	ITU-T Rec. H.222.0 | ISO/IEC 13818-1 PES packets containing private data
 * @var stream_mheg
 *	@brief	ISO/IEC 13522 MHEG
 * @var stream_h222
 *	@brief	ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Annex A DSM-CC
 * @var stream_h222_1
 *	@brief	ITU-T Rec. H.222.1
 * @var stream_13818_6_A
 *	@brief	ISO/IEC 13818-6 type A
 * @var stream_13818_6_B
 *	@brief	ISO/IEC 13818-6 type B
 * @var stream_13818_6_C
 *	@brief	ISO/IEC 13818-6 type C
 * @var stream_13818_6_D
 *	@brief	ISO/IEC 13818-6 type D
 * @var stream_h222_aux
 *	@brief	ITU-T Rec. H.222.0 | ISO/IEC 13818-1 auxiliary
 * @var stream_audio_adts
 *	@brief	ISO/IEC 13818-7 Audio with ADTS transport syntax
 * @var stream_video_14496_2
 *	@brief	ISO/IEC 14496-2 Visual
 * @var stream_audio_latm
 *	@brief	ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3 / AMD 1
 * @var stream_14496_1_pes
 *	@brief	ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets
 * @var stream_14496_1_iso
 *	@brief	ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC14496_sections.
 * @var stream_download
 *	@brief	ISO/IEC 13818-6 Synchronized Download Protocol
 * @var stream_reserved
 *	@brief	ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved (from 0x15 to 0x7f)
 * @var stream_private
 *	@brief	User Private (from 0x80 to 0xff)
 */
enum dvb_streams {
	stream_reserved0	= 0x00,
	stream_video		= 0x01,
	stream_video_h262	= 0x02,
	stream_audio		= 0x03,
	stream_audio_13818_3	= 0x04,
	stream_private_sections	= 0x05,
	stream_private_data	= 0x06,
	stream_mheg		= 0x07,
	stream_h222		= 0x08,
	stream_h222_1		= 0x09,
	stream_13818_6_A	= 0x0A,
	stream_13818_6_B	= 0x0B,
	stream_13818_6_C	= 0x0C,
	stream_13818_6_D	= 0x0D,
	stream_h222_aux		= 0x0E,
	stream_audio_adts	= 0x0F,
	stream_video_14496_2	= 0x10,
	stream_audio_latm	= 0x11,
	stream_14496_1_pes	= 0x12,
	stream_14496_1_iso	= 0x13,
	stream_download		= 0x14,
	stream_reserved		= 0x15,
	stream_private		= 0x80
};

/**
 * @brief Converts from enum dvb_streams into a string
 * @ingroup dvb_table
 */
extern const char *pmt_stream_name[];

/**
 * @struct dvb_table_pmt_stream
 * @brief MPEG-TS PMT stream table
 * @ingroup dvb_table
 *
 * @param type			stream type
 * @param elementary_pid	elementary pid
 * @param desc_length		descriptor length
 * @param zero			zero
 * @param descriptor		pointer to struct dvb_desc
 * @param next			pointer to struct dvb_table_pmt_stream

 *
 * This structure is used to store the original PMT stream table,
 * converting the integer fields to the CPU endianness.
 *
 * The undocumented parameters are used only internally by the API and/or
 * are fields that are reserved. They shouldn't be used, as they may change
 * on future API releases.
 *
 * Everything after dvb_table_pmt_stream::descriptor (including it) won't be
 * bit-mapped to the data parsed from the MPEG TS. So, metadata are added there.
 */
struct dvb_table_pmt_stream {
	uint8_t type;
	union {
		uint16_t bitfield;
		struct {
			uint16_t elementary_pid:13;
			uint16_t reserved:3;
		} __attribute__((packed));
	} __attribute__((packed));
	union {
		uint16_t bitfield2;
		struct {
			uint16_t desc_length:10;
			uint16_t zero:2;
			uint16_t reserved2:4;
		} __attribute__((packed));
	} __attribute__((packed));
	struct dvb_desc *descriptor;
	struct dvb_table_pmt_stream *next;
} __attribute__((packed));

/**
 * @struct dvb_table_pmt
 * @brief MPEG-TS PMT table
 * @ingroup dvb_table
 *
 * @param header	struct dvb_table_header content
 * @param pcr_pid	PCR PID
 * @param desc_length	descriptor length
 * @param descriptor	pointer to struct dvb_desc
 * @param stream	pointer to struct dvb_table_pmt_stream
 *
 * This structure is used to store the original PMT stream table,
 * converting the integer fields to the CPU endianness.
 *
 * The undocumented parameters are used only internally by the API and/or
 * are fields that are reserved. They shouldn't be used, as they may change
 * on future API releases.
 *
 * Everything after dvb_table_pmt::descriptor (including it) won't be
 * bit-mapped to the data parsed from the MPEG TS. So, metadata are added there.
 */
struct dvb_table_pmt {
	struct dvb_table_header header;
	union {
		uint16_t bitfield;
		struct {
			uint16_t pcr_pid:13;
			uint16_t reserved2:3;
		} __attribute__((packed));
	} __attribute__((packed));

	union {
		uint16_t bitfield2;
		struct {
			uint16_t desc_length:10;
			uint16_t zero3:2;
			uint16_t reserved3:4;
		} __attribute__((packed));
	} __attribute__((packed));
	struct dvb_desc *descriptor;
	struct dvb_table_pmt_stream *stream;
} __attribute__((packed));

/** @brief First field at the struct */
#define dvb_pmt_field_first header

/** @brief First field that are not part of the received data */
#define dvb_pmt_field_last descriptor

/**
 * @brief Macro used to find streams on a PMT table
 * @ingroup dvb_table
 *
 * @param _stream	stream to seek
 * @param _pmt		pointer to struct dvb_table_pmt_stream
 */
#define dvb_pmt_stream_foreach(_stream, _pmt) \
  for (struct dvb_table_pmt_stream *_stream = _pmt->stream; _stream; _stream = _stream->next) \

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses PMT table
 * @ingroup dvb_table
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param buf buffer containing the PMT raw data
 * @param buflen length of the buffer
 * @param table pointer to struct dvb_table_pmt to be allocated and filled
 *
 * This function allocates a PMT table and fills the fields inside
 * the struct. It also makes sure that all fields will follow the CPU
 * endianness. Due to that, the content of the buffer may change.
 *
 * @return On success, it returns the size of the allocated struct.
 *	   A negative value indicates an error.
 */
ssize_t dvb_table_pmt_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			    ssize_t buflen, struct dvb_table_pmt **table);

/**
 * @brief Frees all data allocated by the PMT table parser
 * @ingroup dvb_table
 *
 * @param table pointer to struct dvb_table_pmt to be freed
 */
void dvb_table_pmt_free(struct dvb_table_pmt *table);

/**
 * @brief Prints the content of the PAT table
 * @ingroup dvb_table
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param table pointer to struct dvb_table_pmt
 */
void dvb_table_pmt_print(struct dvb_v5_fe_parms *parms,
			 const struct dvb_table_pmt *table);

#ifdef __cplusplus
}
#endif

#endif
