/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
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

#include <libdvbv5/descriptors.h>

#include <libdvbv5/dvb-sat.h>

/* According with ISO/IEC 13818-1:2007 */

#define MAX_TABLE_SIZE 1024 * 1024

#ifdef __cplusplus
extern "C" {
#endif

struct dvb_entry;

struct dvb_v5_descriptors_program {
	struct dvb_table_pat_program *pat_pgm;
	struct dvb_table_pmt *pmt;
};

struct dvb_v5_descriptors {
	int verbose;
	uint32_t delivery_system;

	struct dvb_entry *entry;
	unsigned num_entry;

	struct dvb_table_pat *pat;
	struct atsc_table_vct *vct;
	struct dvb_v5_descriptors_program *program;
	struct dvb_table_nit *nit;
	struct dvb_table_sdt *sdt;

	unsigned num_program;
};

struct dvb_table_filter {
	/* Input data */
	unsigned char tid;
	uint16_t pid;
	int ts_id;
	void **table;

	int allow_section_gaps;

	/*
	 * Private temp data used by dvb_read_sections().
	 * Should not be filled outside dvb-scan.c, as they'll be
	 * overrided
	 */
	void *priv;
};

void dvb_table_filter_free(struct dvb_table_filter *sect);

/* Read DVB table sections
 *
 * The following functions can be used to read DVB table sections by
 * specifying a table ID and a program ID. Optionally a transport
 * stream ID can be specified as well. The function will read on the
 * specified demux and return when reading is done or an error has
 * occurred. If table is not NULL after the call, it has to be freed
 * with the apropriate free table function (even if an error has
 * occurred).
 *
 * Returns 0 on success or a negative error code.
 *
 * Example usage:
 *
 * struct dvb_table_pat *pat;
 * int r = dvb_read_section( parms, dmx_fd, DVB_TABLE_PAT, DVB_TABLE_PAT_PID, (void **) &pat, 5 );
 * if (r < 0)
 *	dvb_logerr("error reading PAT table");
 * else {
 *	// do something with pat
 * }
 * if (pat)
 *	dvb_table_pat_free( pat );
 *
 */

int dvb_read_section(struct dvb_v5_fe_parms *parms, int dmx_fd, unsigned char tid, uint16_t pid, void **table,
		unsigned timeout);

int dvb_read_section_with_id(struct dvb_v5_fe_parms *parms, int dmx_fd, unsigned char tid, uint16_t pid, int ts_id, void **table,
		unsigned timeout);

int dvb_read_sections(struct dvb_v5_fe_parms *parms, int dmx_fd,
			     struct dvb_table_filter *sect,
			     unsigned timeout);

struct dvb_v5_descriptors *dvb_scan_alloc_handler_table(uint32_t delivery_system,
						       int verbose);

void dvb_scan_free_handler_table(struct dvb_v5_descriptors *dvb_scan_handler);

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

void dvb_update_transponders(struct dvb_v5_fe_parms *parms,
			     struct dvb_v5_descriptors *dvb_scan_handler,
			     struct dvb_entry *first_entry,
			     struct dvb_entry *entry);


#ifdef __cplusplus
}
#endif

#endif
