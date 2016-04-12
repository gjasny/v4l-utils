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

#ifndef _MPEG_ES_H
#define _MPEG_ES_H

/**
 * @file mpeg_es.h
 * @ingroup dvb_table
 * @brief Provides the table parser for the MPEG-TS Elementary Stream
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Andre Roth
 *
 * @par Relevant specs
 * The table described herein is defined in ISO 13818-2
 *
 * @see
 * http://dvd.sourceforge.net/dvdinfo/mpeghdrs.html
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#include <stdint.h>
#include <unistd.h> /* ssize_t */

/**
 * @def DVB_MPEG_ES_PIC_START
 *	@brief Picture Start
 *	@ingroup dvb_table
 * @def DVB_MPEG_ES_USER_DATA
 *	@brief User Data
 *	@ingroup dvb_table
 * @def DVB_MPEG_ES_SEQ_START
 *	@brief Sequence Start
 *	@ingroup dvb_table
 * @def DVB_MPEG_ES_SEQ_EXT
 *	@brief Extension
 *	@ingroup dvb_table
 * @def DVB_MPEG_ES_GOP
 *	@brief Group Of Pictures
 *	@ingroup dvb_table
 * @def DVB_MPEG_ES_SLICES
 *	@brief Slices
 *	@ingroup dvb_table
 */
#define DVB_MPEG_ES_PIC_START  0x00
#define DVB_MPEG_ES_USER_DATA  0xb2
#define DVB_MPEG_ES_SEQ_START  0xb3
#define DVB_MPEG_ES_SEQ_EXT    0xb5
#define DVB_MPEG_ES_GOP        0xb8
#define DVB_MPEG_ES_SLICES     0x01 ... 0xaf

/**
 * @struct dvb_mpeg_es_seq_start
 * @brief MPEG ES Sequence header
 * @ingroup dvb_table
 *
 * @param type		DVB_MPEG_ES_SEQ_START
 * @param sync		Sync bytes
 * @param framerate	Framerate
 * @param aspect	Aspect ratio
 * @param height	Height
 * @param width		Width
 * @param qm_nonintra	Load non-intra quantizer matrix
 * @param qm_intra	Load intra quantizer matrix
 * @param constrained	Constrained parameters flag
 * @param vbv		VBV buffer size
 * @param one		Should be 1
 * @param bitrate	Bitrate
 */
struct dvb_mpeg_es_seq_start {
	union {
		uint32_t bitfield;
		struct {
			uint32_t  type:8;
			uint32_t  sync:24;
		} __attribute__((packed));
	} __attribute__((packed));
	union {
		uint32_t bitfield2;
		struct {
			uint32_t framerate:4;
			uint32_t aspect:4;
			uint32_t height:12;
			uint32_t width:12;
		} __attribute__((packed));
	} __attribute__((packed));
	union {
		uint32_t bitfield3;
		struct {
			uint32_t qm_nonintra:1;
			uint32_t qm_intra:1;
			uint32_t constrained:1;
			uint32_t vbv:10; // Size of video buffer verifier = 16*1024*vbv buf size
			uint32_t one:1;
			uint32_t bitrate:18;
		} __attribute__((packed));
	} __attribute__((packed));
} __attribute__((packed));

/**
 * @struct dvb_mpeg_es_pic_start
 * @brief MPEG ES Picture start header
 * @ingroup dvb_table
 *
 * @param type		DVB_MPEG_ES_PIC_START
 * @param sync		Sync bytes
 * @param dummy		Unused
 * @param vbv_delay	VBV delay
 * @param coding_type	Frame type (enum dvb_mpeg_es_frame_t)
 * @param temporal_ref	Temporal sequence number
 */
struct dvb_mpeg_es_pic_start {
	union {
		uint32_t bitfield;
		struct {
			uint32_t  type:8;
			uint32_t  sync:24;
		} __attribute__((packed));
	} __attribute__((packed));
	union {
		uint32_t bitfield2;
		struct {
			uint32_t dummy:3;
			uint32_t vbv_delay:16;
			uint32_t coding_type:3;
			uint32_t temporal_ref:10;
		} __attribute__((packed));
	} __attribute__((packed));
} __attribute__((packed));

/**
 * @enum dvb_mpeg_es_frame_t
 * @brief MPEG frame types
 * @ingroup dvb_table
 *
 * @var DVB_MPEG_ES_FRAME_UNKNOWN
 *	@brief	Unknown frame
 * @var DVB_MPEG_ES_FRAME_I
 *	@brief	I frame
 * @var DVB_MPEG_ES_FRAME_P
 *	@brief	P frame
 * @var DVB_MPEG_ES_FRAME_B
 *	@brief	B frame
 * @var DVB_MPEG_ES_FRAME_D
 *	@brief	D frame
 */
enum dvb_mpeg_es_frame_t
{
	DVB_MPEG_ES_FRAME_UNKNOWN,
	DVB_MPEG_ES_FRAME_I,
	DVB_MPEG_ES_FRAME_P,
	DVB_MPEG_ES_FRAME_B,
	DVB_MPEG_ES_FRAME_D
};

/**
 * @brief Vector that translates from enum dvb_mpeg_es_frame_t to string.
 * @ingroup dvb_table
 */
extern const char *dvb_mpeg_es_frame_names[5];

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize a struct dvb_mpeg_es_seq_start from buffer
 * @ingroup dvb_table
 *
 * @param buf		Buffer
 * @param buflen	Length of buffer
 * @param seq_start	Pointer to allocated struct dvb_mpeg_es_seq_start
 *
 * @return		If buflen too small, return -1, 0 otherwise.
 *
 * This function copies the length of struct dvb_mpeg_es_seq_start
 * to seq_start and fixes endianness. seq_start has to be allocated
 * with malloc.
 */
int  dvb_mpeg_es_seq_start_init (const uint8_t *buf, ssize_t buflen,
		struct dvb_mpeg_es_seq_start *seq_start);

/**
 * @brief Print details of struct dvb_mpeg_es_seq_start
 * @ingroup dvb_table
 *
 * @param parms		struct dvb_v5_fe_parms for log functions
 * @param seq_start	Pointer to struct dvb_mpeg_es_seq_start to print
 *
 * This function prints the fields of struct dvb_mpeg_es_seq_start
 */
void dvb_mpeg_es_seq_start_print(struct dvb_v5_fe_parms *parms,
		struct dvb_mpeg_es_seq_start *seq_start);

/**
 * @brief Initialize a struct dvb_mpeg_es_pic_start from buffer
 * @ingroup dvb_table
 *
 * @param buf		Buffer
 * @param buflen	Length of buffer
 * @param pic_start	Pointer to allocated structdvb_mpeg_es_pic_start
 *
 * @return		If buflen too small, return -1, 0 otherwise.
 *
 * This function copies the length of struct dvb_mpeg_es_pic_start
 * to pic_start	and fixes endianness. seq_start has to be allocated
 * with malloc.
 */
int  dvb_mpeg_es_pic_start_init (const uint8_t *buf, ssize_t buflen,
		struct dvb_mpeg_es_pic_start *pic_start);

/**
 * @brief Print details of struct dvb_mpeg_es_pic_start
 * @ingroup dvb_table
 *
 * @param parms		struct dvb_v5_fe_parms for log functions
 * @param pic_start	Pointer to struct dvb_mpeg_es_pic_start to print
 *
 * This function prints the fields of struct dvb_mpeg_es_pic_start
 */
void dvb_mpeg_es_pic_start_print(struct dvb_v5_fe_parms *parms,
		struct dvb_mpeg_es_pic_start *pic_start);

#ifdef __cplusplus
}
#endif

#endif
