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

/**
 * @file mgt.h
 * @ingroup dvb_table
 * @brief Provides the table parser for the ATSC MGT (Master Guide Table)
 * @copyright GNU General Public License version 2 (GPLv2)
 * @author Andre Roth
 *
 * @par Relevant specs
 * The table described herein is defined at:
 * - ATSC A/65:2009
 *
 * @see
 * http://www.etherguidesystems.com/help/sdos/atsc/syntax/tablesections/MGT.aspx
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#ifndef _MGT_H
#define _MGT_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#include <libdvbv5/atsc_header.h>

/**
 * @def ATSC_TABLE_MGT
 *	@brief ATSC MGT table ID
 *	@ingroup dvb_table
 */
#define ATSC_TABLE_MGT 0xC7

/**
 * @struct atsc_table_mgt_table
 * @brief ATSC tables descrition at MGT table
 * @ingroup dvb_table
 *
 * @param type		table type
 * @param pid		table type pid
 * @param type_version	type type version number
 * @param size		number of bytes for the table entry
 * @param desc_length	table type descriptors length
 * @param descriptor	pointer to struct dvb_desc
 * @param next		pointer to struct atsc_table_mgt_table
 *
 * This structure is used to store the original VCT channel table,
 * converting the integer fields to the CPU endianness.
 *
 * The undocumented parameters are used only internally by the API and/or
 * are fields that are reserved. They shouldn't be used, as they may change
 * on future API releases.
 *
 * Everything after atsc_table_mgt_table::descriptor (including it) won't
 * be bit-mapped * to the data parsed from the MPEG TS. So, metadata are
 * added there.
 */
struct atsc_table_mgt_table {
	uint16_t type;
	union {
		uint16_t bitfield;
		struct {
			uint16_t pid:13;
			uint16_t one:3;
		} __attribute__((packed));
	} __attribute__((packed));
        uint8_t type_version:5;
        uint8_t one2:3;
        uint32_t size;
	union {
		uint16_t bitfield2;
		struct {
			uint16_t desc_length:12;
			uint16_t one3:4;
		} __attribute__((packed));
	} __attribute__((packed));
	struct dvb_desc *descriptor;
	struct atsc_table_mgt_table *next;
} __attribute__((packed));

/**
 * @struct atsc_table_mgt
 * @brief ATSC MGT table
 * @ingroup dvb_table
 *
 * @param header		struct dvb_table_header content
 * @param protocol_version	protocol version
 * @param tables		tables_defined Number of tables defined
 * @param table			pointer to struct atsc_table_mgt_table
 * @param descriptor		pointer to struct dvb_desc
 *
 * This structure is used to store the original MGT channel table,
 * converting the integer fields to the CPU endianness.
 *
 * The undocumented parameters are used only internally by the API and/or
 * are fields that are reserved. They shouldn't be used, as they may change
 * on future API releases.
 *
 * Everything after atsc_table_mgt::table (including it) won't
 * be bit-mapped * to the data parsed from the MPEG TS. So, metadata are
 * added there.
 */
struct atsc_table_mgt {
	struct dvb_table_header header;
	uint8_t  protocol_version;
        uint16_t tables;
        struct atsc_table_mgt_table *table;
	struct dvb_desc *descriptor;
} __attribute__((packed));

/**
 * @brief Macro used to find a table inside a MGT table
 *
 * @param _table	channel to seek
 * @param _mgt		pointer to struct atsc_table_mgt_table
 */
#define atsc_mgt_table_foreach( _table, _mgt ) \
  for( struct atsc_table_mgt_table *_table = _mgt->table; _table; _table = _table->next ) \

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses MGT table
 * @ingroup dvb_table
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param buf buffer containing the MGT raw data
 * @param buflen length of the buffer
 * @param table pointer to struct atsc_table_mgt to be allocated and filled
 *
 * This function allocates an ATSC MGT table and fills the fields inside
 * the struct. It also makes sure that all fields will follow the CPU
 * endianness. Due to that, the content of the buffer may change.
 *
 * @return On success, it returns the size of the allocated struct.
 *	   A negative value indicates an error.
 */
ssize_t atsc_table_mgt_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			    ssize_t buflen, struct atsc_table_mgt **table);

/**
 * @brief Frees all data allocated by the MGT table parser
 * @ingroup dvb_table
 *
 * @param table pointer to struct atsc_table_mgt to be freed
 */
void atsc_table_mgt_free(struct atsc_table_mgt *table);

/**
 * @brief Prints the content of the MGT table
 * @ingroup dvb_table
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param table pointer to struct atsc_table_mgt
 */
void atsc_table_mgt_print(struct dvb_v5_fe_parms *parms,
			  struct atsc_table_mgt *table);

#ifdef __cplusplus
}
#endif

#endif
