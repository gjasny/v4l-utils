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

#ifndef _PAT_H
#define _PAT_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#include <libdvbv5/header.h>

#define DVB_TABLE_PAT      0x00
#define DVB_TABLE_PAT_PID  0x0000

struct dvb_table_pat_program {
	uint16_t service_id;
	union {
		uint16_t bitfield;
		struct {
			uint16_t pid:13;
			uint8_t  reserved:3;
		} __attribute__((packed));
	} __attribute__((packed));
	struct dvb_table_pat_program *next;
} __attribute__((packed));

struct dvb_table_pat {
	struct dvb_table_header header;
	uint16_t programs;
	struct dvb_table_pat_program *program;
} __attribute__((packed));

#define dvb_pat_program_foreach(_pgm, _pat) \
	for (struct dvb_table_pat_program *_pgm = _pat->program; _pgm; _pgm = _pgm->next) \

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

ssize_t dvb_table_pat_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, struct dvb_table_pat **table);
void dvb_table_pat_free(struct dvb_table_pat *pat);
void dvb_table_pat_print(struct dvb_v5_fe_parms *parms, struct dvb_table_pat *t);

#ifdef __cplusplus
}
#endif

#endif
