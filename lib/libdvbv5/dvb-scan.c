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
#include "dvb-log.h"
#include "dvb-demux.h"
#include "descriptors/header.h"
#include "descriptors/pat.h"
#include "descriptors/pmt.h"
#include "descriptors/nit.h"
#include "descriptors/sdt.h"
#include "descriptors/vct.h"
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

	if (!table)
		return -4;
	*table = NULL;

	// FIXME: verify known table


	struct dmx_sct_filter_params f;
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
			if (tbl)
				free(tbl);
			return 0;
		}
		if (available <= 0) {
			dvb_logerr("dvb_read_section: no data read on pid %x table %x",
				   pid, tid);
			if (tbl)
				free(tbl);
			return -1;
		}
		buf_length = read(dmx_fd, buf, DVB_MAX_PAYLOAD_PACKET_SIZE);

		if (!buf_length) {
			dvb_logerr("dvb_read_section: not enough data to read on pid %x table %x",
				   pid, tid);
			free(buf);
			if (tbl)
				free(tbl);
			return -1;
		}
		if (buf_length < 0) {
			dvb_perror("dvb_read_section: read error");
			free(buf);
			if (tbl)
				free(tbl);
			return -2;
		}

		uint32_t crc = crc32(buf, buf_length, 0xFFFFFFFF);
		if (crc != 0) {
			dvb_logerr("dvb_read_section: crc error");
			free(buf);
			if (tbl)
				free(tbl);
			return -3;
		}

		struct dvb_table_header *h = (struct dvb_table_header *) buf;
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

		//ARRAY_SIZE(vb_table_initializers) >= table
		if (!tbl) {
			tbl = malloc(MAX_TABLE_SIZE);
			if (!tbl)
				dvb_perror("Out of memory");
		}

		if (dvb_table_initializers[tid].init) {
			dvb_table_initializers[tid].init(parms, buf, buf_length, tbl, &table_length);
			tbl = realloc(tbl, table_length);
			if (!tbl)
				dvb_perror("Out of memory");
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
					     unsigned timeout_multiply,
					     int verbose)
{
	int rc;
	unsigned pat_pmt_time, sdt_time, nit_time, vct_time;
	int atsc_filter = 0;
	unsigned num_pmt = 0;

	struct dvb_v5_descriptors *dvb_scan_handler;

	dvb_scan_handler = dvb_scan_alloc_handler_table(delivery_system, verbose);
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
		fprintf(stderr, "error while waiting for PAT table\n");
		if (!atsc_filter) {
			dvb_scan_free_handler_table(dvb_scan_handler);
			return NULL;
		}
	}
	if (verbose)
		dvb_table_pat_print(parms, dvb_scan_handler->pat);

	/* ATSC-specific VCT table */
	if (atsc_filter) {
		rc = dvb_read_section(parms, dmx_fd,
				      atsc_filter, DVB_TABLE_VCT_PID,
				      (uint8_t **)&dvb_scan_handler->vct,
				      vct_time * timeout_multiply);
		if (rc < 0)
			fprintf(stderr, "error while waiting for VCT table\n");
		else if (verbose)
			dvb_table_vct_print(parms, dvb_scan_handler->vct);
	}

	/* PMT tables */

	dvb_scan_handler->pmt = NULL;

	dvb_pat_program_foreach(program, dvb_scan_handler->pat) {
		uint16_t pn = program->service_id;
		/* Skip PAT, CAT, reserved and NULL packets */
		if (!pn)
			continue;

		dvb_scan_handler->pmt = realloc(dvb_scan_handler->pmt,
					        sizeof(*dvb_scan_handler->pmt) * (num_pmt + 1));

		dvb_log("Program ID %d", program->pid);
		rc = dvb_read_section(parms, dmx_fd,
				      DVB_TABLE_PMT, program->pid,
				      (uint8_t **)&dvb_scan_handler->pmt[num_pmt],
				      pat_pmt_time * timeout_multiply);
		if (rc < 0)
			fprintf(stderr, "error while reading the PMT table for service 0x%04x\n",
					pn);
		else {
			if (verbose)
				dvb_table_pmt_print(parms, dvb_scan_handler->pmt[num_pmt]);

			num_pmt++;
		}
	}

	/* NIT table */
	rc = dvb_read_section(parms, dmx_fd,
			      DVB_TABLE_NIT, DVB_TABLE_NIT_PID,
			      (uint8_t **)&dvb_scan_handler->nit,
			      nit_time * timeout_multiply);
	if (rc < 0)
		fprintf(stderr, "error while reading the NIT table\n");
	else if (verbose)
		dvb_table_nit_print(parms, dvb_scan_handler->nit);

	/* SDT table */
	rc = dvb_read_section(parms, dmx_fd,
			      DVB_TABLE_SDT, DVB_TABLE_SDT_PID,
			      (uint8_t **)&dvb_scan_handler->sdt,
			      sdt_time * timeout_multiply);
	if (rc < 0)
		fprintf(stderr, "error while reading the SDT table\n");
	else if (verbose)
		dvb_table_sdt_print(parms, dvb_scan_handler->sdt);

	/* NIT/SDT other tables */
	if (other_nit) {
		if (verbose)
			printf("Parsing other NIT/SDT\n");
		rc = dvb_read_section(parms, dmx_fd,
				      DVB_TABLE_NIT2, DVB_TABLE_NIT_PID,
				      (uint8_t **)&dvb_scan_handler->nit,
				      nit_time * timeout_multiply);
		if (rc < 0)
			fprintf(stderr, "error while reading the NIT table\n");
		else if (verbose)
			dvb_table_nit_print(parms, dvb_scan_handler->nit);

		rc = dvb_read_section(parms, dmx_fd,
				DVB_TABLE_SDT2, DVB_TABLE_SDT_PID,
				(uint8_t **)&dvb_scan_handler->sdt,
				sdt_time * timeout_multiply);
		if (rc < 0)
			fprintf(stderr, "error while reading the SDT table\n");
		else if (verbose)
			dvb_table_sdt_print(parms, dvb_scan_handler->sdt);
	}

	return dvb_scan_handler;
}
