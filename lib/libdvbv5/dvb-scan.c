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

/******************************************************************************
 * Parse DVB tables
 * According with:
 *	ETSI EN 301 192 V1.5.1 (2009-11)
 *	ISO/IEC 13818-1:2007
 *	ETSI EN 300 468 V1.11.1 (2010-04)
 *****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>

#include "dvb-scan.h"
#include "dvb-frontend.h"
#include "descriptors.h"
#include "parse_string.h"
#include "crc32.h"
#include "dvb-fe.h"
#include "dvb-file.h"
#include "dvb-log.h"
#include "dvb-demux.h"
#include "descriptors/header.h"
#include "descriptors/pat.h"
#include "descriptors/pmt.h"
#include "descriptors/nit.h"
#include "descriptors/sdt.h"
#include "descriptors/vct.h"
#include "descriptors/desc_extension.h"
#include "descriptors/desc_cable_delivery.h"
#include "descriptors/desc_isdbt_delivery.h"
#include "descriptors/desc_terrestrial_delivery.h"
#include "descriptors/desc_t2_delivery.h"
#include "dvb-scan-table-handler.h"

static int poll(struct dvb_v5_fe_parms *parms, int fd, unsigned int seconds)
{
	fd_set set;
	struct timeval timeout;
	int ret;

	/* Initialize the file descriptor set. */
	FD_ZERO (&set);
	FD_SET (fd, &set);

	/* Initialize the timeout data structure. */
	timeout.tv_sec = seconds;
	timeout.tv_usec = 0;

	/* `select' logfuncreturns 0 if timeout, 1 if input available, -1 if error. */
	do ret = select (FD_SETSIZE, &set, NULL, NULL, &timeout);
	while (!parms->abort && ret == -1 && errno == EINTR);

	return ret;
}

int dvb_read_section(struct dvb_v5_fe_parms *parms, int dmx_fd,
		     unsigned char tid, uint16_t pid, uint8_t **table,
		     unsigned timeout)
{
	return dvb_read_section_with_id(parms, dmx_fd, tid, pid, -1, table, timeout);
}

int dvb_read_section_with_id(struct dvb_v5_fe_parms *parms, int dmx_fd,
			     unsigned char tid, uint16_t pid, int id,
			     uint8_t **table, unsigned timeout)
{
	uint8_t *buf = NULL;
	uint8_t *tbl = NULL;
	ssize_t table_length = 0;
	int first_section = -1;
	int last_section = -1;
	int table_id = -1;
	int sections = 0;
	struct dmx_sct_filter_params f;
	struct dvb_table_header *h;

	if (!table)
		return -4;
	*table = NULL;

	// FIXME: verify known table

	memset(&f, 0, sizeof(f));
	f.pid = pid;
	f.filter.filter[0] = tid;
	f.filter.mask[0] = 0xff;
	f.timeout = 0;
	f.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;
	if (ioctl(dmx_fd, DMX_SET_FILTER, &f) == -1) {
		dvb_perror("dvb_read_section: ioctl DMX_SET_FILTER failed");
		return -1;
	}

	if (parms->verbose)
		dvb_log("Parsing table ID %d, program ID %d", tid, pid);

	buf = malloc(DVB_MAX_PAYLOAD_PACKET_SIZE);
	if (!buf)
		dvb_perror("Out of memory");
	while (1) {
		int available;
		ssize_t buf_length = 0;

		do {
			available = poll(parms, dmx_fd, timeout);
		} while (available < 0 && errno == EOVERFLOW);

		if (parms->abort) {
			free(buf);
			if (tbl)
				free(tbl);
			dvb_dmx_stop(dmx_fd);
			return 0;
		}
		if (available <= 0) {
			dvb_logerr("dvb_read_section: no data read on pid %x table %x",
				   pid, tid);
			free(buf);
			if (tbl)
				free(tbl);
			dvb_dmx_stop(dmx_fd);
			return -1;
		}
		buf_length = read(dmx_fd, buf, DVB_MAX_PAYLOAD_PACKET_SIZE);

		if (!buf_length) {
			dvb_logerr("dvb_read_section: not enough data to read on pid %x table %x",
				   pid, tid);
			free(buf);
			if (tbl)
				free(tbl);
			dvb_dmx_stop(dmx_fd);
			return -1;
		}
		if (buf_length < 0) {
			dvb_perror("dvb_read_section: read error");
			free(buf);
			if (tbl)
				free(tbl);
			dvb_dmx_stop(dmx_fd);
			return -2;
		}

		uint32_t crc = crc32(buf, buf_length, 0xFFFFFFFF);
		if (crc != 0) {
			dvb_logerr("dvb_read_section: crc error");
			free(buf);
			if (tbl)
				free(tbl);
			dvb_dmx_stop(dmx_fd);
			return -3;
		}

		h = (struct dvb_table_header *)buf;
		dvb_table_header_init(h);
		if (id != -1 && h->id != id) { /* search for a specific table id */
			continue;
		} else {
			if (table_id == -1)
				table_id = h->id;
			else if (h->id != table_id) {
				dvb_logwarn("dvb_read_section: table ID mismatch reading multi section table: %d != %d", h->id, table_id);
				continue;
			}
		}

		/* handle the sections */
		if (first_section == -1)
			first_section = h->section_id;
		else if (h->section_id == first_section)
			break;

		if (last_section == -1)
			last_section = h->last_section;

		if (!tbl) {
			if (dvb_table_initializers[tid].size)
				tbl = malloc(dvb_table_initializers[tid].size);
			else
				tbl = malloc(MAX_TABLE_SIZE);
			if (!tbl) {
				dvb_logerr("Out of memory");
				free(buf);
				dvb_dmx_stop(dmx_fd);
				return -4;
			}
		}

		if (dvb_table_initializers[tid].init) {
			dvb_table_initializers[tid].init(parms, buf, buf_length, tbl, &table_length);
			if (!tbl) {
				dvb_perror("Out of memory");
				free(buf);
				dvb_dmx_stop(dmx_fd);
				return -4;
			}
			if (!dvb_table_initializers[tid].size)
				tbl = realloc(tbl, table_length);
		} else
			dvb_logerr("dvb_read_section: no initializer for table %d", tid);

		if (++sections == last_section + 1)
			break;
	}
	free(buf);

	dvb_dmx_stop(dmx_fd);

	*table = tbl;
	return 0;
}

struct dvb_v5_descriptors *dvb_get_ts_tables(struct dvb_v5_fe_parms *parms,
					     int dmx_fd,
					     uint32_t delivery_system,
					     unsigned other_nit,
					     unsigned timeout_multiply)
{
	int rc;
	unsigned pat_pmt_time, sdt_time, nit_time, vct_time;
	int atsc_filter = 0;
	unsigned num_pmt = 0;

	struct dvb_v5_descriptors *dvb_scan_handler;

	dvb_scan_handler = dvb_scan_alloc_handler_table(delivery_system, parms->verbose);
	if (!dvb_scan_handler)
		return NULL;

	if (!timeout_multiply)
		timeout_multiply = 1;

	/* Get standard timeouts for each table */
	switch(delivery_system) {
		case SYS_DVBC_ANNEX_A:
		case SYS_DVBC_ANNEX_C:
		case SYS_DVBS:
		case SYS_DVBS2:
		case SYS_TURBO:
			pat_pmt_time = 1;
			sdt_time = 2;
			nit_time = 10;
			break;
		case SYS_DVBT:
		case SYS_DVBT2:
			pat_pmt_time = 1;
			sdt_time = 2;
			nit_time = 12;
			break;
		case SYS_ISDBT:
			pat_pmt_time = 1;
			sdt_time = 2;
			nit_time = 12;
			break;
		case SYS_ATSC:
			atsc_filter = DVB_TABLE_TVCT;
			pat_pmt_time = 2;
			vct_time = 2;
			sdt_time = 5;
			nit_time = 5;
			break;
		case SYS_DVBC_ANNEX_B:
			atsc_filter = DVB_TABLE_CVCT;
			pat_pmt_time = 2;
			vct_time = 2;
			sdt_time = 5;
			nit_time = 5;
			break;
		default:
			pat_pmt_time = 1;
			sdt_time = 2;
			nit_time = 10;
			break;
	};

	/* PAT table */
	rc = dvb_read_section(parms, dmx_fd,
			      DVB_TABLE_PAT, DVB_TABLE_PAT_PID,
			      (uint8_t **) &dvb_scan_handler->pat,
			      pat_pmt_time * timeout_multiply);
	if (rc < 0) {
		dvb_logerr("error while waiting for PAT table");
		dvb_scan_free_handler_table(dvb_scan_handler);
		return NULL;
	}
	if (parms->verbose)
		dvb_table_pat_print(parms, dvb_scan_handler->pat);

	/* ATSC-specific VCT table */
	if (atsc_filter) {
		rc = dvb_read_section(parms, dmx_fd,
				      atsc_filter, DVB_TABLE_VCT_PID,
				      (uint8_t **)&dvb_scan_handler->vct,
				      vct_time * timeout_multiply);
		if (rc < 0)
			dvb_logerr("error while waiting for VCT table");
		else if (parms->verbose)
			dvb_table_vct_print(parms, dvb_scan_handler->vct);
	}

	/* PMT tables */
	dvb_scan_handler->program = calloc(dvb_scan_handler->pat->programs,
					   sizeof(*dvb_scan_handler->program));

	dvb_pat_program_foreach(program, dvb_scan_handler->pat) {
		dvb_scan_handler->program[num_pmt].pat_pgm = program;

		if (!program->service_id) {
			if (parms->verbose)
				dvb_log("Network PID: 0x%02x", program->pid);
			num_pmt++;
			continue;
		}
		if (parms->verbose)
			dvb_log("Program ID %d", program->pid);
		rc = dvb_read_section(parms, dmx_fd,
				      DVB_TABLE_PMT, program->pid,
				      (uint8_t **)&dvb_scan_handler->program[num_pmt].pmt,
				      pat_pmt_time * timeout_multiply);
		if (rc < 0) {
			dvb_logerr("error while reading the PMT table for service 0x%04x",
				   program->service_id);
			dvb_scan_handler->program[num_pmt].pmt = NULL;
		} else {
			if (parms->verbose)
				dvb_table_pmt_print(parms, dvb_scan_handler->program[num_pmt].pmt);
		}
		num_pmt++;
	}
	dvb_scan_handler->num_program = num_pmt;

	/* NIT table */
	rc = dvb_read_section(parms, dmx_fd,
			      DVB_TABLE_NIT, DVB_TABLE_NIT_PID,
			      (uint8_t **)&dvb_scan_handler->nit,
			      nit_time * timeout_multiply);
	if (rc < 0)
		dvb_logerr("error while reading the NIT table");
	else if (parms->verbose)
		dvb_table_nit_print(parms, dvb_scan_handler->nit);

	/* SDT table */
	if (!dvb_scan_handler->vct || other_nit) {
		rc = dvb_read_section(parms, dmx_fd,
				DVB_TABLE_SDT, DVB_TABLE_SDT_PID,
				(uint8_t **)&dvb_scan_handler->sdt,
				sdt_time * timeout_multiply);
		if (rc < 0)
			dvb_logerr("error while reading the SDT table");
		else if (parms->verbose)
			dvb_table_sdt_print(parms, dvb_scan_handler->sdt);
	}

	/* NIT/SDT other tables */
	if (other_nit) {
		if (parms->verbose)
			dvb_log("Parsing other NIT/SDT");
		rc = dvb_read_section(parms, dmx_fd,
				      DVB_TABLE_NIT2, DVB_TABLE_NIT_PID,
				      (uint8_t **)&dvb_scan_handler->nit,
				      nit_time * timeout_multiply);
		if (rc < 0)
			dvb_logerr("error while reading the NIT table");
		else if (parms->verbose)
			dvb_table_nit_print(parms, dvb_scan_handler->nit);

		rc = dvb_read_section(parms, dmx_fd,
				DVB_TABLE_SDT2, DVB_TABLE_SDT_PID,
				(uint8_t **)&dvb_scan_handler->sdt,
				sdt_time * timeout_multiply);
		if (rc < 0)
			dvb_logerr("error while reading the SDT table");
		else if (parms->verbose)
			dvb_table_sdt_print(parms, dvb_scan_handler->sdt);
	}

	return dvb_scan_handler;
}

struct dvb_v5_descriptors *dvb_scan_transponder(struct dvb_v5_fe_parms *parms,
					        struct dvb_entry *entry,
						int dmx_fd,
					        check_frontend_t *check_frontend,
					        void *args,
						unsigned other_nit,
						unsigned timeout_multiply)
{
	struct dvb_v5_descriptors *dvb_scan_handler = NULL;
	uint32_t freq;
	int i, rc;

	/* First of all, set the delivery system */
	for (i = 0; i < entry->n_props; i++)
		if (entry->props[i].cmd == DTV_DELIVERY_SYSTEM)
			dvb_set_compat_delivery_system(parms,
							entry->props[i].u.data);

	/* Copy data into parms */
	for (i = 0; i < entry->n_props; i++) {
		uint32_t data = entry->props[i].u.data;

		/* Don't change the delivery system */
		if (entry->props[i].cmd == DTV_DELIVERY_SYSTEM)
			continue;

		dvb_fe_store_parm(parms, entry->props[i].cmd, data);
		if (parms->current_sys == SYS_ISDBT) {
			dvb_fe_store_parm(parms, DTV_ISDBT_PARTIAL_RECEPTION, 0);
			dvb_fe_store_parm(parms, DTV_ISDBT_SOUND_BROADCASTING, 0);
			dvb_fe_store_parm(parms, DTV_ISDBT_LAYER_ENABLED, 0x07);
			if (entry->props[i].cmd == DTV_CODE_RATE_HP) {
				dvb_fe_store_parm(parms, DTV_ISDBT_LAYERA_FEC,
						data);
				dvb_fe_store_parm(parms, DTV_ISDBT_LAYERB_FEC,
						data);
				dvb_fe_store_parm(parms, DTV_ISDBT_LAYERC_FEC,
						data);
			} else if (entry->props[i].cmd == DTV_MODULATION) {
				dvb_fe_store_parm(parms,
						DTV_ISDBT_LAYERA_MODULATION,
						data);
				dvb_fe_store_parm(parms,
						DTV_ISDBT_LAYERB_MODULATION,
						data);
				dvb_fe_store_parm(parms,
						DTV_ISDBT_LAYERC_MODULATION,
						data);
			}
		}
		if (parms->current_sys == SYS_ATSC &&
			entry->props[i].cmd == DTV_MODULATION) {
			if (data != VSB_8 && data != VSB_16)
				dvb_fe_store_parm(parms,
						DTV_DELIVERY_SYSTEM,
						SYS_DVBC_ANNEX_B);
		}
	}

	rc = dvb_fe_set_parms(parms);
	if (rc < 0) {
		dvb_perror("dvb_fe_set_parms failed");
		return NULL;
	}

	/* As the DVB core emulates it, better to always use auto */
	dvb_fe_store_parm(parms, DTV_INVERSION, INVERSION_AUTO);

	dvb_fe_retrieve_parm(parms, DTV_FREQUENCY, &freq);
	if (parms->verbose)
		dvb_fe_prt_parms(parms);

	rc = check_frontend(args, parms);
	if (rc < 0)
		return NULL;

	dvb_scan_handler = dvb_get_ts_tables(parms, dmx_fd,
					parms->current_sys,
					other_nit,
					timeout_multiply);

	return dvb_scan_handler;
}

int estimate_freq_shift(struct dvb_v5_fe_parms *parms)
{
	uint32_t shift = 0, bw = 0, symbol_rate, ro;
	int rolloff = 0;
	int divisor = 100;

	/* Need to handle only cable/satellite and ATSC standards */
	switch (parms->current_sys) {
	case SYS_DVBC_ANNEX_A:
		rolloff = 115;
		break;
	case SYS_DVBC_ANNEX_C:
		rolloff = 115;
		break;
	case SYS_DVBS:
	case SYS_ISDBS:	/* FIXME: not sure if this rollof is right for ISDB-S */
		divisor = 100000;
		rolloff = 135;
		break;
	case SYS_DVBS2:
	case SYS_DSS:
	case SYS_TURBO:
		divisor = 100000;
		dvb_fe_retrieve_parm(parms, DTV_ROLLOFF, &ro);
		switch (ro) {
		case ROLLOFF_20:
			rolloff = 120;
			break;
		case ROLLOFF_25:
			rolloff = 125;
			break;
		default:
		case ROLLOFF_AUTO:
		case ROLLOFF_35:
			rolloff = 135;
			break;
		}
		break;
	case SYS_ATSC:
	case SYS_DVBC_ANNEX_B:
		bw = 6000000;
		break;
	default:
		break;
	}
	if (rolloff) {
		/*
		 * This is not 100% correct for DVB-S2, as there is a bw
		 * guard interval there but it should be enough for the
		 * purposes of estimating a max frequency shift here.
		 */
		dvb_fe_retrieve_parm(parms, DTV_SYMBOL_RATE, &symbol_rate);
		bw = (symbol_rate * rolloff) / divisor;
	}
	if (!bw)
		dvb_fe_retrieve_parm(parms, DTV_BANDWIDTH_HZ, &bw);

	/*
	 * If the max frequency shift between two frequencies is below
	 * than the used bandwidth / 8, it should be the same channel.
	 */
	shift = bw / 8;

	return shift;
}

int new_freq_is_needed(struct dvb_entry *entry, struct dvb_entry *last_entry,
		       uint32_t freq, enum dvb_sat_polarization pol, int shift)
{
	int i;
	uint32_t data;

	for (; entry != last_entry; entry = entry->next) {
		for (i = 0; i < entry->n_props; i++) {
			data = entry->props[i].u.data;
			if (entry->props[i].cmd == DTV_POLARIZATION) {
				if (data != pol)
					continue;
			}
			if (entry->props[i].cmd == DTV_FREQUENCY) {
				if (( freq >= data - shift) && (freq <= data + shift))
					return 0;
			}
		}
	}

	return 1;
}

struct dvb_entry *dvb_scan_add_entry(struct dvb_v5_fe_parms *parms,
				     struct dvb_entry *first_entry,
			             struct dvb_entry *entry,
			             uint32_t freq, uint32_t shift,
			             enum dvb_sat_polarization pol)
{
	struct dvb_entry *new_entry;
	int i, n = 2;

	if (!new_freq_is_needed(first_entry, NULL, freq, pol, shift))
		return NULL;

	/* Clone the current entry into a new entry */
	new_entry = calloc(sizeof(*new_entry), 1);
	if (!new_entry) {
		dvb_perror("not enough memory for a new scanning frequency");
		return NULL;
	}

	memcpy(new_entry, entry, sizeof(*entry));

	/*
	 * The frequency should change to the new one. Seek for it and
	 * replace its value to the desired one.
	 */
	for (i = 0; i < new_entry->n_props; i++) {
		if (new_entry->props[i].cmd == DTV_FREQUENCY) {
			new_entry->props[i].u.data = freq;
			/* Navigate to the end of the entry list */
			while (entry->next) {
				entry = entry->next;
				n++;
			}
			dvb_log("New transponder/channel found: #%d: %d",
			        n, freq);
			entry->next = new_entry;
			new_entry->next = NULL;
			return entry;
		}
	}

	/* This should never happen */
	dvb_logerr("BUG: Couldn't add %d to the scan frequency list.", freq);
	free(new_entry);

	return NULL;
}

void dvb_add_scaned_transponders(struct dvb_v5_fe_parms *parms,
				 struct dvb_v5_descriptors *dvb_scan_handler,
				 struct dvb_entry *first_entry,
				 struct dvb_entry *entry)
{
	struct dvb_entry *new;
	enum dvb_sat_polarization pol = POLARIZATION_OFF;
	uint32_t shift = 0;
	int i;

	if (!dvb_scan_handler->nit)
		return;

	shift = estimate_freq_shift(parms);

	switch (parms->current_sys) {
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		dvb_nit_transport_foreach(tran, dvb_scan_handler->nit) {
			dvb_desc_find(struct dvb_desc_cable_delivery, cable,
				      tran, cable_delivery_system_descriptor) {
				new = dvb_scan_add_entry(parms,
							 first_entry, entry,
							 cable->frequency,
							 shift, pol);
				if (!new)
					return;

				/* Set NIT cable props for the transponder */
				store_entry_prop(entry, DTV_MODULATION,
						 dvbc_modulation_table[cable->modulation]);
				store_entry_prop(entry, DTV_SYMBOL_RATE,
						 cable->symbol_rate);
				store_entry_prop(entry, DTV_INNER_FEC,
						 dvbc_fec_table[cable->fec_inner]);

			}
		}
		return;
	case SYS_ISDBT:
		/* FIXME: add some logic here to detect partial reception */
		dvb_nit_transport_foreach(tran, dvb_scan_handler->nit) {
			dvb_desc_find(struct isdbt_desc_terrestrial_delivery_system, d,
				      tran, ISDBT_delivery_system_descriptor) {
				uint32_t mode = isdbt_mode[d->transmission_mode];
				uint32_t guard = isdbt_interval[d->guard_interval];

				for (i = 0; i < d->num_freqs; i++) {
					uint32_t frq = d->frequency[i] * 1000000l / 7;
					new = dvb_scan_add_entry(parms,
								first_entry, entry,
								frq, shift, pol);
					store_entry_prop(entry,
							 DTV_TRANSMISSION_MODE,
							 mode);
					store_entry_prop(entry,
							 DTV_GUARD_INTERVAL,
							 guard);
				}
				if (!new)
					return;
			}
		}
		return;
	case SYS_DVBT:
		dvb_nit_transport_foreach(tran, dvb_scan_handler->nit) {
			dvb_desc_find(struct dvb_extension_descriptor, d,
				      tran, extension_descriptor) {
				struct dvb_desc_t2_delivery *t2;
				if (d->extension_code != T2_delivery_system_descriptor)
					continue;

				t2 = (struct dvb_desc_t2_delivery *)d->descriptor;

				for (i = 0; i < t2->frequency_loop_length; i++) {

					new = dvb_scan_add_entry(parms,
								 first_entry, entry,
								 t2->centre_frequency[i],
								 shift, pol);
					if (!new)
						return;
					store_entry_prop(entry, DTV_DELIVERY_SYSTEM,
							SYS_DVBT2);
#if 0
					store_entry_prop(entry, DTV_DVBT2_PLP_ID_LEGACY,
							nit_table->plp_id);
					store_entry_prop(entry, DTV_BANDWIDTH_HZ,
							nit_table->bandwidth);
					store_entry_prop(entry, DTV_GUARD_INTERVAL,
							nit_table->guard_interval);
					store_entry_prop(entry, DTV_TRANSMISSION_MODE,
							nit_table->transmission_mode);

					/* Fill data from terrestrial descriptor */
					store_entry_prop(entry, DTV_FREQUENCY,
							nit_table->frequency[0]);
					store_entry_prop(entry, DTV_MODULATION,
							nit_table->modulation);
					store_entry_prop(entry, DTV_CODE_RATE_HP,
							nit_table->code_rate_hp);
					store_entry_prop(entry, DTV_CODE_RATE_LP,
							nit_table->code_rate_lp);
					store_entry_prop(entry, DTV_HIERARCHY,
							nit_table->hierarchy);
#endif
				}

			}


			dvb_desc_find(struct dvb_desc_terrestrial_delivery, d,
				      tran, terrestrial_delivery_system_descriptor) {
				new = dvb_scan_add_entry(parms,
							 first_entry, entry,
							 d->centre_frequency,
							 shift, pol);
				if (!new)
					return;

				/* Set NIT DVB-T props for the transponder */
				store_entry_prop(entry, DTV_MODULATION,
						 dvbt_modulation[d->constellation]);
				store_entry_prop(entry, DTV_BANDWIDTH_HZ,
						 dvbt_bw[d->bandwidth]);
				store_entry_prop(entry, DTV_CODE_RATE_HP,
						 dvbt_code_rate[d->code_rate_hp_stream]);
				store_entry_prop(entry, DTV_CODE_RATE_LP,
						 dvbt_code_rate[d->code_rate_lp_stream]);
				store_entry_prop(entry, DTV_GUARD_INTERVAL,
						 dvbt_interval[d->guard_interval]);
				store_entry_prop(entry, DTV_TRANSMISSION_MODE,
						 dvbt_transmission_mode[d->transmission_mode]);
				store_entry_prop(entry, DTV_HIERARCHY,
						 dvbt_hierarchy[d->hierarchy_information]);
			}
		}
		return;
	case SYS_DVBS:
	case SYS_DVBS2:
		dvb_nit_transport_foreach(tran, dvb_scan_handler->nit) {
			dvb_desc_find(struct dvb_desc_sat, d,
				      tran, satellite_delivery_system_descriptor) {
				new = dvb_scan_add_entry(parms,
							 first_entry, entry,
							 d->frequency,
							 shift, pol);
				if (!new)
					return;

				/* Set NIT DVB-S props for the transponder */

				store_entry_prop(entry, DTV_MODULATION,
						dvbs_modulation[d->modulation_system]);
				store_entry_prop(entry, DTV_POLARIZATION,
						dvbs_polarization[d->polarization]);
				store_entry_prop(entry, DTV_SYMBOL_RATE,
						d->symbol_rate);
				store_entry_prop(entry, DTV_INNER_FEC,
						dvbs_dvbc_dvbs_freq_inner[d->fec]);
				store_entry_prop(entry, DTV_ROLLOFF,
						 dvbs_rolloff[d->roll_off]);
				if (d->roll_off != 0)
					store_entry_prop(entry, DTV_DELIVERY_SYSTEM,
							 SYS_DVBS2);

			}
		}
		return;
	default:
		dvb_log("Transponders detection not implemented for this standard yet.");
		return;
	}
}
