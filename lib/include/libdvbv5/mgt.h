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

#ifndef _MGT_H
#define _MGT_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#include <libdvbv5/atsc_header.h>

#define ATSC_TABLE_MGT 0xC7

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

struct atsc_table_mgt {
	ATSC_HEADER();
        uint16_t tables;
        struct atsc_table_mgt_table *table;
	struct dvb_desc *descriptor;
} __attribute__((packed));

#define atsc_mgt_table_foreach( _tran, _mgt ) \
  for( struct atsc_table_mgt_table *_tran = _mgt->table; _tran; _tran = _tran->next ) \

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

ssize_t atsc_table_mgt_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, struct atsc_table_mgt **table);
void atsc_table_mgt_free(struct atsc_table_mgt *mgt);
void atsc_table_mgt_print(struct dvb_v5_fe_parms *parms, struct atsc_table_mgt *mgt);

#ifdef __cplusplus
}
#endif

#endif
