/*
 * Copyright (c) 2013 - Mauro Carvalho Chehab <m.chehab@samsung.com>
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

#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "descriptors.h"
#include "dvb-fe.h"
#include "dvb-scan.h"
#include "dvb-frontend.h"
#include "dvb-v5-std.h"
#include "dvb-log.h"

#include "descriptors/pat.h"
#include "descriptors/pmt.h"
#include "descriptors/nit.h"
#include "descriptors/sdt.h"
#include "descriptors/eit.h"
#include "descriptors/vct.h"

#include "descriptors/desc_language.h"
#include "descriptors/desc_network_name.h"
#include "descriptors/desc_cable_delivery.h"
#include "descriptors/desc_sat.h"
#include "descriptors/desc_terrestrial_delivery.h"
#include "descriptors/desc_service.h"
#include "descriptors/desc_service_list.h"
#include "descriptors/desc_frequency_list.h"
#include "descriptors/desc_event_short.h"
#include "descriptors/desc_event_extended.h"
#include "descriptors/desc_atsc_service_location.h"
#include "descriptors/desc_hierarchy.h"

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
	struct dvb_table_vct *vct;
	struct dvb_v5_descriptors_program *program;
	struct dvb_table_nit *nit;
	struct dvb_table_sdt *sdt;

	unsigned num_program;
};

struct dvb_v5_descriptors *dvb_scan_alloc_handler_table(uint32_t delivery_system,
						       int verbose);

void dvb_scan_free_handler_table(struct dvb_v5_descriptors *dvb_scan_handler);
