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
#include "parse_string.h"
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

#include "dvb-scan-table-handler.h"

struct dvb_v5_descriptors *dvb_scan_alloc_handler_table(uint32_t delivery_system,
						       int verbose)
{
	struct dvb_v5_descriptors *dvb_scan_handler;

	dvb_scan_handler = calloc(sizeof(*dvb_scan_handler), 1);
	if (!dvb_scan_handler)
		return NULL;

	dvb_scan_handler->verbose = verbose;
	dvb_scan_handler->delivery_system = delivery_system;

	return dvb_scan_handler;
}

void dvb_scan_free_handler_table(struct dvb_v5_descriptors *dvb_scan_handler)
{
	struct pat_table *pat_table = &dvb_scan_handler->pat_table;
	struct pid_table *pid_table = dvb_scan_handler->pat_table.pid_table;
	struct nit_table *nit_table = &dvb_scan_handler->nit_table;
	struct sdt_table *sdt_table = &dvb_scan_handler->sdt_table;
	int i;

	if (pid_table) {
		for (i = 0; i < pat_table->pid_table_len; i++) {
			if (pid_table[i].video_pid)
				free(pid_table[i].video_pid);
			if (pid_table[i].audio_pid)
				free(pid_table[i].audio_pid);
			if (pid_table[i].other_el_pid)
				free(pid_table[i].other_el_pid);
		}
		free(pid_table);
	}

	if (nit_table->lcn)
		free(nit_table->lcn);
	if (nit_table->network_name)
		free(nit_table->network_name);
	if (nit_table->network_alias)
		free(nit_table->network_alias);
	if (nit_table->tr_table)
		free(nit_table->tr_table);
	if (nit_table->frequency)
		free(nit_table->frequency);
	if (nit_table->orbit)
		free(nit_table->orbit);

	if (sdt_table->service_table) {
		for (i = 0; i < sdt_table->service_table_len; i++) {
			if (sdt_table->service_table[i].provider_name)
				free(sdt_table->service_table[i].provider_name);
			if (sdt_table->service_table[i].provider_alias)
				free(sdt_table->service_table[i].provider_alias);
			if (sdt_table->service_table[i].service_name)
				free(sdt_table->service_table[i].service_name);
			if (sdt_table->service_table[i].service_alias)
				free(sdt_table->service_table[i].service_alias);
		}
		free(sdt_table->service_table);
	}

	if (dvb_scan_handler->pat)
		dvb_table_pat_free(dvb_scan_handler->pat);
	if (dvb_scan_handler->vct)
		dvb_table_vct_free(dvb_scan_handler->vct);
	if (dvb_scan_handler->nit)
		dvb_table_nit_free(dvb_scan_handler->nit);
	if (dvb_scan_handler->sdt)
		dvb_table_sdt_free(dvb_scan_handler->sdt);
	if (dvb_scan_handler->program) {
		for (i = 0; i < dvb_scan_handler->num_program; i++)
			if (dvb_scan_handler->program[i].pmt)
				dvb_table_pmt_free(dvb_scan_handler->program[i].pmt);
		free(dvb_scan_handler->program);
	}

	free(dvb_scan_handler);
}
