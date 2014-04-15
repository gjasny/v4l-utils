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

#ifndef _NIT_H
#define _NIT_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#include <libdvbv5/header.h>
#include <libdvbv5/descriptors.h>

#define DVB_TABLE_NIT      0x40
#define DVB_TABLE_NIT2     0x41
#define DVB_TABLE_NIT_PID  0x10

union dvb_table_nit_transport_header {
	uint16_t bitfield;
	struct {
		uint16_t transport_length:12;
		uint16_t reserved:4;
	} __attribute__((packed));
} __attribute__((packed));

struct dvb_table_nit_transport {
	uint16_t transport_id;
	uint16_t network_id;
	union {
		uint16_t bitfield;
		struct {
			uint16_t desc_length:12;
			uint16_t reserved:4;
		} __attribute__((packed));
	} __attribute__((packed));
	struct dvb_desc *descriptor;
	struct dvb_table_nit_transport *next;
} __attribute__((packed));

struct dvb_table_nit {
	struct dvb_table_header header;
	union {
		uint16_t bitfield;
		struct {
			uint16_t desc_length:12;
			uint16_t reserved:4;
		} __attribute__((packed));
	} __attribute__((packed));
	struct dvb_desc *descriptor;
	struct dvb_table_nit_transport *transport;
} __attribute__((packed));

typedef void nit_handler_callback_t(struct dvb_table_nit *nit,
				    struct dvb_desc *desc,
				    void *priv);

typedef void nit_tran_handler_callback_t(struct dvb_table_nit *nit,
					 struct dvb_table_nit_transport *tran,
					 struct dvb_desc *desc,
					 void *priv);

#define dvb_nit_transport_foreach( _tran, _nit ) \
  for (struct dvb_table_nit_transport *_tran = _nit->transport; _tran; _tran = _tran->next) \

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

ssize_t dvb_table_nit_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, struct dvb_table_nit **table);
void dvb_table_nit_free(struct dvb_table_nit *nit);
void dvb_table_nit_print(struct dvb_v5_fe_parms *parms, struct dvb_table_nit *nit);

void nit_descriptor_handler(struct dvb_v5_fe_parms *parms,
			    struct dvb_table_nit *nit,
			    enum descriptors descriptor,
			    nit_handler_callback_t *call_nit,
			    nit_tran_handler_callback_t *call_tran,
			    void *priv);

#ifdef __cplusplus
}
#endif

#endif
