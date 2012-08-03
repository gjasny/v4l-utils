/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab <mchehab@redhat.com>
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
 */
#ifndef _LIBSCAN_H
#define _LIBSCAN_H

#include <stdint.h>
#include <linux/dvb/dmx.h>

#include "descriptors.h"

/* According with ISO/IEC 13818-1:2007 */

#define MAX_TABLE_SIZE 1024 * 1024

#ifdef __cplusplus
extern "C" {
#endif

int dvb_read_section(struct dvb_v5_fe_parms *parms, int dmx_fd, unsigned char tid, uint16_t pid, unsigned char **table,
		unsigned timeout);

int dvb_read_section_with_id(struct dvb_v5_fe_parms *parms, int dmx_fd, unsigned char tid, uint16_t pid, int id, uint8_t **table,
		unsigned timeout);

struct dvb_v5_descriptors *dvb_get_ts_tables(struct dvb_v5_fe_parms *parms, int dmx_fd,
					  uint32_t delivery_system,
					  unsigned other_nit,
					  unsigned timeout_multiply,
					  int verbose);
void dvb_free_ts_tables(struct dvb_v5_descriptors *dvb_desc);

#ifdef __cplusplus
}
#endif

#endif
