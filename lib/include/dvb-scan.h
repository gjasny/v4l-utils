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

#include "dvb-sat.h"

/* According with ISO/IEC 13818-1:2007 */

#define MAX_TABLE_SIZE 1024 * 1024

#ifdef __cplusplus
extern "C" {
#endif

struct dvb_entry;

int dvb_read_section(struct dvb_v5_fe_parms *parms, int dmx_fd, unsigned char tid, uint16_t pid, unsigned char **table,
		unsigned timeout);

int dvb_read_section_with_id(struct dvb_v5_fe_parms *parms, int dmx_fd, unsigned char tid, uint16_t pid, int id, uint8_t **table,
		unsigned timeout);

struct dvb_v5_descriptors *dvb_get_ts_tables(struct dvb_v5_fe_parms *parms, int dmx_fd,
					  uint32_t delivery_system,
					  unsigned other_nit,
					  unsigned timeout_multiply);
void dvb_free_ts_tables(struct dvb_v5_descriptors *dvb_desc);

typedef int (check_frontend_t)(void *args, struct dvb_v5_fe_parms *parms);

struct dvb_v5_descriptors *dvb_scan_transponder(struct dvb_v5_fe_parms *parms,
						struct dvb_entry *entry,
						int dmx_fd,
						check_frontend_t *check_frontend,
						void *args,
						unsigned other_nit,
						unsigned timeout_multiply);

int estimate_freq_shift(struct dvb_v5_fe_parms *parms);

int new_freq_is_needed(struct dvb_entry *entry, struct dvb_entry *last_entry,
		       uint32_t freq, enum dvb_sat_polarization pol, int shift);

struct dvb_entry *dvb_scan_add_entry(struct dvb_v5_fe_parms *parms,
				     struct dvb_entry *first_entry,
			             struct dvb_entry *entry,
			             uint32_t freq, uint32_t shift,
			             enum dvb_sat_polarization pol);

void dvb_add_scaned_transponders(struct dvb_v5_fe_parms *parms,
				 struct dvb_v5_descriptors *dvb_scan_handler,
				 struct dvb_entry *first_entry,
				 struct dvb_entry *entry);


#ifdef __cplusplus
}
#endif

#endif
