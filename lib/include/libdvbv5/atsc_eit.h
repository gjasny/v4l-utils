/*
 * Copyright (c) 2013 - Andre Roth <neolynx@gmail.com>
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

/**
 * @file atsc_eit.h
 * @ingroup dvb_table
 * @brief Provides the table parser for the ATSC EIT (Event Information Table)
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Mauro Carvalho Chehab
 * @author Andre Roth
 *
 * @par Relevant specs
 * The table described herein is defined at:
 * - ATSC A/65:2009
 *
 * @see
 * http://www.etherguidesystems.com/help/sdos/atsc/syntax/tablesections/eitks.aspx
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#ifndef _ATSC_EIT_H
#define _ATSC_EIT_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */
#include <time.h>

#include <libdvbv5/atsc_header.h>

/**
 * @def ATSC_TABLE_EIT
 *	@brief ATSC EIT table ID
 *	@ingroup dvb_table
 */
#define ATSC_TABLE_EIT        0xCB

/**
 * @struct atsc_table_eit_event
 * @brief ATSC EIT event table
 * @ingroup dvb_table
 *
 * @param event_id	an uniquelly (inside a service ID) event ID
 * @param title_length	title length. Zero means no title
 * @param duration	duration in seconds
 * @param etm		Extended Text Message location
 * @param descriptor	pointer to struct dvb_desc
 * @param next		pointer to struct atsc_table_eit_event
 * @param start		event start (in struct tm format)
 * @param source_id	source id (obtained from ATSC header)
 *
 * This structure is used to store the original ATSC EIT event table,
 * converting the integer fields to the CPU endianness, and converting the
 * timestamps to a way that it is better handled on Linux.
 *
 * The undocumented parameters are used only internally by the API and/or
 * are fields that are reserved. They shouldn't be used, as they may change
 * on future API releases.
 *
 * Everything after atsc_table_eit_event::descriptor (including it) won't
 * be bit-mapped to the data parsed from the MPEG TS. So, metadata are added
 * there.
 */
struct atsc_table_eit_event {
	union {
		uint16_t bitfield;
		struct {
	          uint16_t event_id:14;
	          uint16_t one:2;
		} __attribute__((packed));
	} __attribute__((packed));
	uint32_t start_time;
	union {
		uint32_t bitfield2;
		struct {
			uint32_t title_length:8;
			uint32_t duration:20;
			uint32_t etm:2;
			uint32_t one2:2;
			uint32_t :2;
		} __attribute__((packed));
	} __attribute__((packed));
	struct dvb_desc *descriptor;
	struct atsc_table_eit_event *next;
	struct tm start;
	uint16_t source_id;
} __attribute__((packed));

/**
 * @union atsc_table_eit_desc_length
 * @brief ATSC EIT descriptor length
 * @ingroup dvb_table
 *
 * @param desc_length	descriptor length
 *
 * This structure is used to store the original ATSC EIT event table,
 * converting the integer fields to the CPU endianness, and converting the
 * timestamps to a way that it is better handled on Linux.
 *
 * The undocumented parameters are used only internally by the API and/or
 * are fields that are reserved. They shouldn't be used, as they may change
 * on future API releases.
 */
union atsc_table_eit_desc_length {
	uint16_t bitfield;
	struct {
		uint16_t desc_length:12;
		uint16_t reserved:4;
	} __attribute__((packed));
} __attribute__((packed));

/**
 * @struct atsc_table_eit
 * @brief ATSC EIT table
 * @ingroup dvb_table
 *
 * @param header			struct dvb_table_header content
 * @param protocol_version		protocol version
 * @param events			events
 * @param event				pointer to struct atsc_table_eit_event
 *
 * This structure is used to store the original ATSC EIT table,
 * converting the integer fields to the CPU endianness.
 *
 * Everything after atsc_table_eit::event (including it) won't
 * be bit-mapped to the data parsed from the MPEG TS. So, metadata are added
 * there.
 */
struct atsc_table_eit {
	struct dvb_table_header header;
	uint8_t  protocol_version;
	uint8_t events;
	struct atsc_table_eit_event *event;
} __attribute__((packed));

/**
 * @brief Macro used to find event on an ATSC EIT table
 * @ingroup dvb_table
 *
 * @param _event			event to seek
 * @param _eit				pointer to struct atsc_table_eit_event
 */
#define atsc_eit_event_foreach(_event, _eit) \
	if (_eit && _eit->event) \
		for( struct atsc_table_eit_event *_event = _eit->event; _event; _event = _event->next ) \

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses ATSC EIT table
 * @ingroup dvb_table
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param buf buffer containing the EIT raw data
 * @param buflen length of the buffer
 * @param table pointer to struct atsc_table_eit to be allocated and filled
 *
 * This function allocates an ATSC EIT table and fills the fields inside
 * the struct. It also makes sure that all fields will follow the CPU
 * endianness. Due to that, the content of the buffer may change.
 *
 * @return On success, it returns the size of the allocated struct.
 *	   A negative value indicates an error.
 */
ssize_t atsc_table_eit_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			    ssize_t buflen, struct atsc_table_eit **table);

/**
 * @brief Frees all data allocated by the ATSC EIT table parser
 * @ingroup dvb_table
 *
 * @param table pointer to struct atsc_table_eit to be freed
 */
void atsc_table_eit_free(struct atsc_table_eit *table);

/**
 * @brief Prints the content of the ATSC EIT table
 * @ingroup dvb_table
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param table pointer to struct atsc_table_eit
 */
void atsc_table_eit_print(struct dvb_v5_fe_parms *parms,
			  struct atsc_table_eit *table);

/**
 * @brief Converts an ATSC EIT formatted timestamp into struct tm
 * @ingroup ancillary
 *
 * @param start_time	event on ATSC EIT time format
 * @param tm		pointer to struct tm where the converted timestamp will
 *			be stored.
 */
void atsc_time(const uint32_t start_time, struct tm *tm);

#ifdef __cplusplus
}
#endif

#endif
