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

#ifndef _MPEG_TS_H
#define _MPEG_TS_H

/**
 * @file mpeg_ts.h
 * @ingroup dvb_table
 * @brief Provides the table parser for the MPEG-PES Elementary Stream
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Andre Roth
 *
 * @par Relevant specs
 * The table described herein is defined in ISO 13818-1
 *
 * @see
 * http://en.wikipedia.org/wiki/MPEG_transport_stream
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */
#include <stdint.h>
#include <unistd.h> /* ssize_t */

/**
 * @def DVB_MPEG_TS
 *	@brief MPEG Transport Stream magic
 *	@ingroup dvb_table
 * @def DVB_MPEG_TS_PACKET_SIZE
 *	@brief Size of an MPEG packet
 *	@ingroup dvb_table
 */
#define DVB_MPEG_TS  0x47
#define DVB_MPEG_TS_PACKET_SIZE  188

/**
 * @struct dvb_mpeg_ts_adaption
 * @brief MPEG TS header adaption field
 * @ingroup dvb_table
 *
 * @param type			DVB_MPEG_ES_SEQ_START
 * @param length		1 bit	Adaptation Field Length
 * @param discontinued		1 bit	Discontinuity indicator
 * @param random_access		1 bit	Random Access indicator
 * @param priority		1 bit	Elementary stream priority indicator
 * @param PCR			1 bit	PCR flag
 * @param OPCR			1 bit	OPCR flag
 * @param splicing_point	1 bit	Splicing point flag
 * @param private_data		1 bit	Transport private data flag
 * @param extension		1 bit	Adaptation field extension flag
 * @param data			Pointer to data
 */
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

/**
 * @struct dvb_mpeg_ts
 * @brief MPEG TS header
 * @ingroup dvb_table
 *
 * @param sync_byte		DVB_MPEG_TS
 * @param tei			1 bit	Transport Error Indicator
 * @param payload_start		1 bit	Payload Unit Start Indicator
 * @param priority		1 bit	Transport Priority
 * @param pid			13 bits	Packet Identifier
 * @param scrambling		2 bits	Scrambling control
 * @param adaptation_field	1 bit	Adaptation field exist
 * @param payload		1 bit	Contains payload
 * @param continuity_counter	4 bits	Continuity counter
 * @param adaption		Pointer to optional adaption fiels (struct dvb_mpeg_ts_adaption)
 */
struct dvb_mpeg_ts {
	uint8_t sync_byte;
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

/**
 * @brief Initialize a struct dvb_mpeg_ts from buffer
 * @ingroup dvb_table
 *
 * @param parms		struct dvb_v5_fe_parms for log functions
 * @param buf		Buffer
 * @param buflen	Length of buffer
 * @param table		Pointer to allocated struct dvb_mpeg_ts
 * @param table_length	Pointer to size_t where length will be written to
 *
 * @return		Length of data in table
 *
 * This function copies the length of struct dvb_mpeg_ts
 * to table and fixes endianness. The pointer table has to be allocated
 * on stack or dynamically.
 */
ssize_t dvb_mpeg_ts_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen,
		uint8_t *table, ssize_t *table_length);

/**
 * @brief Deallocate memory associated with a struct dvb_mpeg_ts
 * @ingroup dvb_table
 *
 * @param ts	struct dvb_mpeg_ts to be deallocated
 *
 * If ts was allocated dynamically, this function
 * can be used to free the memory.
 */
void dvb_mpeg_ts_free(struct dvb_mpeg_ts *ts);

/**
 * @brief Print details of struct dvb_mpeg_ts
 * @ingroup dvb_table
 *
 * @param parms		struct dvb_v5_fe_parms for log functions
 * @param ts    	Pointer to struct dvb_mpeg_ts to print
 *
 * This function prints the fields of struct dvb_mpeg_ts
 */
void dvb_mpeg_ts_print(struct dvb_v5_fe_parms *parms, struct dvb_mpeg_ts *ts);

#ifdef __cplusplus
}
#endif

#endif
