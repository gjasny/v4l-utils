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
#include <sys/time.h>

#include "dvb-scan.h"
#include "dvb-frontend.h"
#include "descriptors.h"
#include "parse_string.h"
#include "crc32.h"
#include "dvb-fe.h"
#include "dvb-file.h"
#include "dvb-scan.h"
#include "dvb-log.h"
#include "dvb-demux.h"
#include "descriptors.h"
#include "descriptors/header.h"
#include "descriptors/pat.h"
#include "descriptors/pmt.h"
#include "descriptors/nit.h"
#include "descriptors/sdt.h"
#include "descriptors/vct.h"
#include "descriptors/desc_extension.h"
#include "descriptors/desc_cable_delivery.h"
#include "descriptors/desc_isdbt_delivery.h"
#include "descriptors/desc_partial_reception.h"
#include "descriptors/desc_terrestrial_delivery.h"
#include "descriptors/desc_t2_delivery.h"
#include "descriptors/desc_sat.h"

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
	if (!buf) {
		dvb_perror("Out of memory");
		dvb_dmx_stop(dmx_fd);
		return -1;
	}
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
			else {
				dvb_logerr("dvb_read_section: no table size for table %d",
					   tid);
				free(buf);
				dvb_dmx_stop(dmx_fd);
				return -1;
			}
			if (!tbl) {
				dvb_logerr("Out of memory");
				free(buf);
				dvb_dmx_stop(dmx_fd);
				return -4;
			}
		}

		if (dvb_table_initializers[tid].init)
			dvb_table_initializers[tid].init(parms, buf, buf_length, tbl, &table_length);
		else
			dvb_logerr("dvb_read_section: no initializer for table %d", tid);

		if (++sections == last_section + 1)
			break;
	}
	free(buf);

	dvb_dmx_stop(dmx_fd);

	*table = tbl;
	return 0;
}

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
	int i;

	if (dvb_scan_handler->pat)
		dvb_table_pat_free(dvb_scan_handler->pat);
	if (dvb_scan_handler->vct)
		atsc_table_vct_free(dvb_scan_handler->vct);
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
			atsc_filter = ATSC_TABLE_TVCT;
			pat_pmt_time = 2;
			vct_time = 2;
			sdt_time = 5;
			nit_time = 5;
			break;
		case SYS_DVBC_ANNEX_B:
			atsc_filter = ATSC_TABLE_CVCT;
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
	if (parms->abort)
		return dvb_scan_handler;
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
				      atsc_filter, ATSC_TABLE_VCT_PID,
				      (uint8_t **)&dvb_scan_handler->vct,
				      vct_time * timeout_multiply);
		if (parms->abort)
			return dvb_scan_handler;
		if (rc < 0)
			dvb_logerr("error while waiting for VCT table");
		else if (parms->verbose)
			atsc_table_vct_print(parms, dvb_scan_handler->vct);
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
		if (parms->abort) {
			dvb_scan_handler->num_program = num_pmt + 1;
			return dvb_scan_handler;
		}
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
	if (parms->abort)
		return dvb_scan_handler;
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
		if (parms->abort)
			return dvb_scan_handler;
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
		if (parms->abort)
			return dvb_scan_handler;
		if (rc < 0)
			dvb_logerr("error while reading the NIT table");
		else if (parms->verbose)
			dvb_table_nit_print(parms, dvb_scan_handler->nit);

		rc = dvb_read_section(parms, dmx_fd,
				DVB_TABLE_SDT2, DVB_TABLE_SDT_PID,
				(uint8_t **)&dvb_scan_handler->sdt,
				sdt_time * timeout_multiply);
		if (parms->abort)
			return dvb_scan_handler;
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
	uint32_t freq, delsys = SYS_UNDEFINED;
	int i, rc;

	/* First of all, set the delivery system */
	retrieve_entry_prop(entry, DTV_DELIVERY_SYSTEM, &delsys);
	dvb_set_compat_delivery_system(parms, delsys);

	/* Copy data into parms */
	for (i = 0; i < entry->n_props; i++) {
		uint32_t data = entry->props[i].u.data;

		/* Don't change the delivery system */
		if (entry->props[i].cmd == DTV_DELIVERY_SYSTEM)
			continue;

		dvb_fe_store_parm(parms, entry->props[i].cmd, data);

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

struct update_transponders {
	struct dvb_v5_fe_parms *parms;
	struct dvb_v5_descriptors *dvb_scan_handler;
	struct dvb_entry *first_entry;
	struct dvb_entry *entry;
	uint32_t update;
	enum dvb_sat_polarization pol;
	uint32_t shift;
};

static void add_update_nit_dvbc(struct dvb_table_nit *nit,
				struct dvb_table_nit_transport *tran,
				struct dvb_desc *desc,
				void *priv)
{
	struct update_transponders *tr = priv;
	struct dvb_entry *new;
	struct dvb_desc_cable_delivery *d = (void *)desc;

	if (tr->update) {
		uint32_t freq;
		dvb_fe_retrieve_parm(tr->parms, DTV_FREQUENCY, &freq);

		if (freq != d->frequency)
			return;
		new = tr->entry;
	} else {
		new = dvb_scan_add_entry(tr->parms, tr->first_entry, tr->entry,
					 d->frequency, tr->shift, tr->pol);
		if (!new)
			return;
	}

	/* Set NIT props for the transponder */
	store_entry_prop(new, DTV_MODULATION,
			 dvbc_modulation_table[d->modulation]);
	store_entry_prop(new, DTV_SYMBOL_RATE, d->symbol_rate);
	store_entry_prop(new, DTV_INNER_FEC, dvbc_fec_table[d->fec_inner]);

}

static void add_update_nit_isdbt(struct dvb_table_nit *nit,
				 struct dvb_table_nit_transport *tran,
				 struct dvb_desc *desc,
				 void *priv)
{
	struct update_transponders *tr = priv;
	struct dvb_entry *new;
	struct isdbt_desc_terrestrial_delivery_system *d = (void *)desc;
	int i;

	if (tr->update) {
		uint32_t mode = isdbt_mode[d->transmission_mode];
		uint32_t guard = isdbt_interval[d->guard_interval];

		store_entry_prop(tr->entry, DTV_TRANSMISSION_MODE, mode);
		store_entry_prop(tr->entry, DTV_GUARD_INTERVAL, guard);
		return;
	}

	for (i = 0; i < d->num_freqs; i++) {
		uint32_t frq = d->frequency[i] * 1000000l / 7;

		new = dvb_scan_add_entry(tr->parms, tr->first_entry, tr->entry,
					 frq, tr->shift, tr->pol);
		if (!new)
			return;
	}
}

static void add_update_nit_1seg(struct dvb_table_nit *nit,
				struct dvb_table_nit_transport *tran,
				struct dvb_desc *desc,
				void *priv)
{
	struct update_transponders *tr = priv;
	struct isdb_desc_partial_reception *d = (void *)desc;
	size_t len;
	int i;

	if (!tr->update)
		return;

	len = d->length / sizeof(d->partial_reception);

	for (i = 0; i < len; i++) {
		if (tr->entry->service_id == d->partial_reception[i].service_id) {
			store_entry_prop(tr->entry,
					 DTV_ISDBT_PARTIAL_RECEPTION, 1);
			return;
		}
	}
	store_entry_prop(tr->entry, DTV_ISDBT_PARTIAL_RECEPTION, 0);
}

static void add_update_nit_dvbt2(struct dvb_table_nit *nit,
				 struct dvb_table_nit_transport *tran,
				 struct dvb_desc *desc,
				 void *priv)
{
	struct update_transponders *tr = priv;
	struct dvb_entry *new;
	struct dvb_extension_descriptor *d = (void *)desc;
	struct dvb_desc_t2_delivery *t2 = (void *)d->descriptor;
	int i;

	if (d->extension_code != T2_delivery_system_descriptor)
		return;

	if (tr->update) {
		uint32_t freq;
		dvb_fe_retrieve_parm(tr->parms, DTV_FREQUENCY, &freq);

		if (tr->entry->service_id != t2->system_id)
			return;

		store_entry_prop(tr->entry, DTV_DELIVERY_SYSTEM,
				SYS_DVBT2);
		store_entry_prop(tr->entry, DTV_STREAM_ID,
				t2->plp_id);

		if (d->length -1 <= 4)
			return;

		store_entry_prop(tr->entry, DTV_BANDWIDTH_HZ,
				dvbt2_bw[t2->bandwidth]);
		store_entry_prop(tr->entry, DTV_GUARD_INTERVAL,
				dvbt2_interval[t2->guard_interval]);
		store_entry_prop(tr->entry, DTV_TRANSMISSION_MODE,
				dvbt2_transmission_mode[t2->transmission_mode]);

		return;
	}

	if (d->length -1 <= 4)
		return;

	for (i = 0; i < t2->frequency_loop_length; i++) {
		new = dvb_scan_add_entry(tr->parms, tr->first_entry, tr->entry,
					 t2->centre_frequency[i] * 10,
					 tr->shift, tr->pol);
		if (!new)
			return;

		store_entry_prop(new, DTV_DELIVERY_SYSTEM,
				 SYS_DVBT2);
		store_entry_prop(new, DTV_STREAM_ID,
				t2->plp_id);
		store_entry_prop(new, DTV_BANDWIDTH_HZ,
				dvbt2_bw[t2->bandwidth]);
		store_entry_prop(new, DTV_GUARD_INTERVAL,
				dvbt2_interval[t2->guard_interval]);
		store_entry_prop(new, DTV_TRANSMISSION_MODE,
				dvbt2_transmission_mode[t2->transmission_mode]);
	}
}

static void add_update_nit_dvbt(struct dvb_table_nit *nit,
				struct dvb_table_nit_transport *tran,
				struct dvb_desc *desc,
				void *priv)
{
	struct update_transponders *tr = priv;
	struct dvb_entry *new;
	struct dvb_desc_terrestrial_delivery *d = (void *)desc;

	if (tr->update)
		return;

	new = dvb_scan_add_entry(tr->parms, tr->first_entry, tr->entry,
				d->centre_frequency * 10, tr->shift, tr->pol);
	if (!new)
		return;

	/* Set NIT DVB-T props for the transponder */
	store_entry_prop(new, DTV_MODULATION,
				dvbt_modulation[d->constellation]);
	store_entry_prop(new, DTV_BANDWIDTH_HZ,
				dvbt_bw[d->bandwidth]);
	store_entry_prop(new, DTV_CODE_RATE_HP,
				dvbt_code_rate[d->code_rate_hp_stream]);
	store_entry_prop(new, DTV_CODE_RATE_LP,
				dvbt_code_rate[d->code_rate_lp_stream]);
	store_entry_prop(new, DTV_GUARD_INTERVAL,
				dvbt_interval[d->guard_interval]);
	store_entry_prop(new, DTV_TRANSMISSION_MODE,
				dvbt_transmission_mode[d->transmission_mode]);
	store_entry_prop(new, DTV_HIERARCHY,
				dvbt_hierarchy[d->hierarchy_information]);
}

static void add_update_nit_dvbs(struct dvb_table_nit *nit,
				struct dvb_table_nit_transport *tran,
				struct dvb_desc *desc,
				void *priv)
{
	struct update_transponders *tr = priv;
	struct dvb_entry *new;
	struct dvb_desc_sat *d = (void *)desc;

	if (tr->update) {
		uint32_t freq;

		dvb_fe_retrieve_parm(tr->parms, DTV_FREQUENCY, &freq);
		if (freq != d->frequency)
			return;
		new = tr->entry;
	} else {
		new = dvb_scan_add_entry(tr->parms, tr->first_entry, tr->entry,
					 d->frequency, tr->shift, tr->pol);
		if (!new)
			return;
	}

	/* Set NIT DVB-S props for the transponder */

	store_entry_prop(new, DTV_MODULATION,
			dvbs_modulation[d->modulation_system]);
	store_entry_prop(new, DTV_POLARIZATION,
			dvbs_polarization[d->polarization]);
	store_entry_prop(new, DTV_SYMBOL_RATE,
			d->symbol_rate);
	store_entry_prop(new, DTV_INNER_FEC,
			dvbs_dvbc_dvbs_freq_inner[d->fec]);
	store_entry_prop(new, DTV_ROLLOFF,
				dvbs_rolloff[d->roll_off]);
	if (d->roll_off != 0)
		store_entry_prop(new, DTV_DELIVERY_SYSTEM,
					SYS_DVBS2);
}


void __dvb_add_update_transponders(struct dvb_v5_fe_parms *parms,
				   struct dvb_v5_descriptors *dvb_scan_handler,
				   struct dvb_entry *first_entry,
				   struct dvb_entry *entry,
				   uint32_t update)
{
	struct update_transponders tr = {
		.parms = parms,
		.dvb_scan_handler = dvb_scan_handler,
		.first_entry = first_entry,
		.entry = entry,
		.update = update,
		.pol = POLARIZATION_OFF,
	};

	if (!dvb_scan_handler->nit)
		return;

	tr.shift = estimate_freq_shift(parms);

	switch (parms->current_sys) {
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		nit_descriptor_handler(parms, dvb_scan_handler->nit,
				       cable_delivery_system_descriptor,
				       NULL, add_update_nit_dvbc, &tr);
		return;
	case SYS_ISDBT:
		nit_descriptor_handler(parms, dvb_scan_handler->nit,
				       partial_reception_descriptor,
				       NULL, add_update_nit_1seg, &tr);
		nit_descriptor_handler(parms, dvb_scan_handler->nit,
				       ISDBT_delivery_system_descriptor,
				       NULL, add_update_nit_isdbt, &tr);
		return;
	case SYS_DVBT:
	case SYS_DVBT2:
		nit_descriptor_handler(parms, dvb_scan_handler->nit,
				       extension_descriptor,
				       NULL, add_update_nit_dvbt2, &tr);

		nit_descriptor_handler(parms, dvb_scan_handler->nit,
				       terrestrial_delivery_system_descriptor,
				       NULL, add_update_nit_dvbt, &tr);
		return;
	case SYS_DVBS:
	case SYS_DVBS2:
		nit_descriptor_handler(parms, dvb_scan_handler->nit,
				       satellite_delivery_system_descriptor,
				       NULL, add_update_nit_dvbs, &tr);
		return;
	default:
		dvb_log("Transponders detection not implemented for this standard yet.");
		return;
	}
}

void dvb_add_scaned_transponders(struct dvb_v5_fe_parms *parms,
				 struct dvb_v5_descriptors *dvb_scan_handler,
				 struct dvb_entry *first_entry,
				 struct dvb_entry *entry)
{
	return __dvb_add_update_transponders(parms, dvb_scan_handler,
					     first_entry, entry, 0);
}

void dvb_update_transponders(struct dvb_v5_fe_parms *parms,
			     struct dvb_v5_descriptors *dvb_scan_handler,
			     struct dvb_entry *first_entry,
			     struct dvb_entry *entry)
{
	return __dvb_add_update_transponders(parms, dvb_scan_handler,
					     first_entry, entry, 1);
}
