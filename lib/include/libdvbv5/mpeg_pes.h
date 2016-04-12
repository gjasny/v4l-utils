/*
 * Copyright (c) 2013-2014 - Andre Roth <neolynx@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 */

#ifndef _MPEG_PES_H
#define _MPEG_PES_H

/**
 * @file mpeg_pes.h
 * @ingroup dvb_table
 * @brief Provides the table parser for the MPEG-PES Elementary Stream
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Andre Roth
 *
 * @par Relevant specs
 * The table described herein is defined in ISO 13818-1
 *
 * @see
 * http://dvd.sourceforge.net/dvdinfo/pes-hdr.html
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#include <stdint.h>
#include <unistd.h> /* ssize_t */


/**
 * @def DVB_MPEG_PES
 *	@brief MPEG Packetized Elementary Stream magic
 *	@ingroup dvb_table
 * @def DVB_MPEG_PES_AUDIO
 *	@brief PES Audio
 *	@ingroup dvb_table
 * @def DVB_MPEG_PES_VIDEO
 *	@brief PES Video
 *	@ingroup dvb_table
 * @def DVB_MPEG_STREAM_MAP
 *	@brief PES Stream map
 *	@ingroup dvb_table
 * @def DVB_MPEG_STREAM_PADDING
 *	@brief PES padding
 *	@ingroup dvb_table
 * @def DVB_MPEG_STREAM_PRIVATE_2
 *	@brief PES private
 *	@ingroup dvb_table
 * @def DVB_MPEG_STREAM_ECM
 *	@brief PES ECM Stream
 *	@ingroup dvb_table
 * @def DVB_MPEG_STREAM_EMM
 *	@brief PES EMM Stream
 *	@ingroup dvb_table
 * @def DVB_MPEG_STREAM_DIRECTORY
 *	@brief PES Stream directory
 *	@ingroup dvb_table
 * @def DVB_MPEG_STREAM_DSMCC
 *	@brief PES DSMCC
 *	@ingroup dvb_table
 * @def DVB_MPEG_STREAM_H222E
 *	@brief PES H.222.1 type E
 *	@ingroup dvb_table
 */

#define DVB_MPEG_PES  0x00001

#define DVB_MPEG_PES_AUDIO  0xc0 ... 0xcf
#define DVB_MPEG_PES_VIDEO  0xe0 ... 0xef

#define DVB_MPEG_STREAM_MAP       0xBC
#define DVB_MPEG_STREAM_PADDING   0xBE
#define DVB_MPEG_STREAM_PRIVATE_2 0x5F
#define DVB_MPEG_STREAM_ECM       0x70
#define DVB_MPEG_STREAM_EMM       0x71
#define DVB_MPEG_STREAM_DIRECTORY 0xFF
#define DVB_MPEG_STREAM_DSMCC     0x7A
#define DVB_MPEG_STREAM_H222E     0xF8

/**
 * @struct ts_t
 * @brief MPEG PES timestamp structure, used for dts and pts
 * @ingroup dvb_table
 *
 * @param tag		4 bits  Should be 0010 for PTS and 0011 for DTS
 * @param bits30	3 bits	Timestamp bits 30-32
 * @param one		1 bit	Sould be 1
 * @param bits15	15 bits	Timestamp bits 15-29
 * @param one1		1 bit	Should be 1
 * @param bits00	15 Bits	Timestamp bits 0-14
 * @param one2		1 bit	Should be 1
 */

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

/**
 * @struct dvb_mpeg_pes_optional
 * @brief MPEG PES optional header
 * @ingroup dvb_table
 *
 * @param two				2 bits	Should be 10
 * @param PES_scrambling_control	2 bits	PES Scrambling Control (Not Scrambled=00, otherwise scrambled)
 * @param PES_priority			1 bit	PES Priority
 * @param data_alignment_indicator	1 bit	PES data alignment
 * @param copyright			1 bit	PES content protected by copyright
 * @param original_or_copy		1 bit	PES content is original (=1) or copied (=0)
 * @param PTS_DTS			2 bit	PES header contains PTS (=10, =11) and/or DTS (=01, =11)
 * @param ESCR				1 bit	PES header contains ESCR fields
 * @param ES_rate			1 bit	PES header contains ES_rate field
 * @param DSM_trick_mode		1 bit	PES header contains DSM_trick_mode field
 * @param additional_copy_info		1 bit	PES header contains additional_copy_info field
 * @param PES_CRC			1 bit	PES header contains CRC field
 * @param PES_extension			1 bit	PES header contains extension field
 * @param length			8 bit	PES header data length
 * @param pts				64 bit	PES PTS timestamp
 * @param dts				64 bit	PES DTS timestamp
 */
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

/**
 * @struct dvb_mpeg_pes
 * @brief MPEG PES data structure
 * @ingroup dvb_table
 *
 * @param sync		24 bits	DVB_MPEG_PES
 * @param stream_id	8 bits	PES Stream ID
 * @param length	16 bits	PES packet length
 * @param optional	Pointer to optional PES header
 */
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

/**
 * @brief Initialize a struct dvb_mpeg_pes from buffer
 * @ingroup dvb_table
 *
 * @param parms		struct dvb_v5_fe_parms for log functions
 * @param buf		Buffer
 * @param buflen	Length of buffer
 * @param table		Pointer to allocated struct dvb_mpeg_pes
 *
 * @return		Length of data in table
 *
 * This function copies the length of struct dvb_mpeg_pes
 * to table and fixes endianness. The pointer table has to be
 * allocated on stack or dynamically.
 */
ssize_t dvb_mpeg_pes_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen,
		uint8_t *table);

/**
 * @brief Deallocate memory associated with a struct dvb_mpeg_pes
 * @ingroup dvb_table
 *
 * @param pes	struct dvb_mpeg_pes to be deallocated
 *
 * If the pointer pes was allocated dynamically, this function
 * can be used to free the memory.
 */
void dvb_mpeg_pes_free(struct dvb_mpeg_pes *pes);

/**
 * @brief Print details of struct dvb_mpeg_pes
 * @ingroup dvb_table
 *
 * @param parms		struct dvb_v5_fe_parms for log functions
 * @param pes    	Pointer to struct dvb_mpeg_pes to print
 *
 * This function prints the fields of struct dvb_mpeg_pes
 */
void dvb_mpeg_pes_print (struct dvb_v5_fe_parms *parms, struct dvb_mpeg_pes *pes);

#ifdef __cplusplus
}
#endif

#endif
