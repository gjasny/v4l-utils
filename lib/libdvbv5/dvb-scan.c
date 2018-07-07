/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 * Copyright (c) 2013 - Andre Roth <neolynx@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
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

#include "dvb-fe-priv.h"
#include <libdvbv5/dvb-scan.h>
#include <libdvbv5/dvb-frontend.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/crc32.h>
#include <libdvbv5/dvb-file.h>
#include <libdvbv5/dvb-scan.h>
#include <libdvbv5/dvb-log.h>
#include <libdvbv5/dvb-demux.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/header.h>
#include <libdvbv5/pat.h>
#include <libdvbv5/pmt.h>
#include <libdvbv5/nit.h>
#include <libdvbv5/sdt.h>
#include <libdvbv5/vct.h>
#include <libdvbv5/desc_extension.h>
#include <libdvbv5/desc_cable_delivery.h>
#include <libdvbv5/desc_isdbt_delivery.h>
#include <libdvbv5/desc_partial_reception.h>
#include <libdvbv5/desc_terrestrial_delivery.h>
#include <libdvbv5/desc_t2_delivery.h>
#include <libdvbv5/desc_sat.h>

#include <config.h>

#ifdef ENABLE_NLS
# include "gettext.h"
# include <libintl.h>
# define _(string) dgettext(LIBDVBV5_DOMAIN, string)

#else
# define _(string) string
#endif

# define N_(string) string

static int dvb_poll(struct dvb_v5_fe_parms_priv *parms, int fd, unsigned int seconds)
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
	while (!parms->p.abort && ret == -1 && errno == EINTR);

	return ret;
}

int dvb_read_section(struct dvb_v5_fe_parms *parms, int dmx_fd,
		     unsigned char tid, uint16_t pid, void **table,
		     unsigned timeout)
{
	return dvb_read_section_with_id(parms, dmx_fd, tid, pid, -1, table, timeout);
}

/*
 * The code below was inspired on Linux Kernel's bitmask implementation
 */

#define BITS_PER_LONG		(8 * sizeof(long))
#define BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)
#define BITS_TO_LONGS(nr)	((nr +BITS_PER_LONG - 1) / BITS_PER_LONG)

static void set_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = addr + BIT_WORD(nr);

	*p  |= mask;
}

static int test_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = addr + BIT_WORD(nr);

	return (*p & mask) ? -1 : 0;
}

static int is_all_bits_set(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr + 1) - 1;

	return (*addr == mask);
}


struct dvb_table_filter_ext_priv {
	int last_section;
	unsigned long is_read_bits[BITS_TO_LONGS(256)];

	/* section gaps and multiple ts_id handling */
	int ext_id;
	int first_section;
	int done;
};

struct dvb_table_filter_priv {
	int num_extensions;
	struct dvb_table_filter_ext_priv *extensions;
};

static int dvb_parse_section_alloc(struct dvb_v5_fe_parms_priv *parms,
				   struct dvb_table_filter *sect)
{
	struct dvb_table_filter_priv *priv;

	if (!sect->table) {
		dvb_logerr(_("%s: table memory pointer not filled"),
				__func__);
		return -4;
	}
	*sect->table = NULL;
	priv = calloc(sizeof(struct dvb_table_filter_priv), 1);
	if (!priv) {
		dvb_logerr(_("%s: out of memory"), __func__);
		return -1;
	}
	sect->priv = priv;

	return 0;
}

void dvb_table_filter_free(struct dvb_table_filter *sect)
{
	struct dvb_table_filter_priv *priv = sect->priv;

	if (priv) {
		free (priv->extensions);
		free(priv);
		sect->priv = NULL;
	}
}

static int dvb_parse_section(struct dvb_v5_fe_parms_priv *parms,
			     struct dvb_table_filter *sect,
			     const uint8_t *buf, ssize_t buf_length)
{
	struct dvb_table_header h;
	struct dvb_table_filter_priv *priv;
	struct dvb_table_filter_ext_priv *ext;
	unsigned char tid;
	int i = 0, new = 0;

	memcpy(&h, buf, sizeof(struct dvb_table_header));
	dvb_table_header_init(&h);

	if (parms->p.verbose)
		dvb_log(_("%s: received table 0x%02x, extension ID 0x%04x, section %d/%d"),
			__func__, h.table_id, h.id, h.section_id, h.last_section);

	if (sect->tid != h.table_id) {
		dvb_logdbg(_("%s: couldn't match ID %d at the active section filters"),
			   __func__, h.table_id);
		return -1;
	}
	priv = sect->priv;
	ext = priv->extensions;
	tid = h.table_id;

	if (!ext) {
		ext = calloc(sizeof(struct dvb_table_filter_ext_priv), 1);
		if (!ext) {
			dvb_logerr(_("%s: out of memory"), __func__);
			return -1;
		}
		ext->ext_id = h.id;
		ext->first_section = h.section_id;
		ext->last_section = h.last_section;
		priv->extensions = ext;
		priv->num_extensions = 1;
		new = 1;
	} else {
		/* search for an specific TS ID */
		if (sect->ts_id != -1) {
			if (h.id != sect->ts_id)
				return 0;
		}

		for (i = 0; i < priv->num_extensions; i++, ext++) {
			if (ext->ext_id == h.id)
				break;
		}
		if (i == priv->num_extensions) {
			priv->num_extensions++;
			priv->extensions = realloc(priv->extensions, sizeof(struct dvb_table_filter_ext_priv) * (priv->num_extensions));
			ext = priv->extensions;
			if (!ext) {
				dvb_logerr(_("%s: out of memory"), __func__);
				return -1;
			}
			ext += i;
			memset(ext, 0, sizeof(struct dvb_table_filter_ext_priv));
			ext->ext_id = h.id;
			ext->first_section = h.section_id;
			ext->last_section = h.last_section;
			new = 1;
		}
	}

	if (!new) { /* Check if the table was already parsed, but not on first pass */
		if(ext->done) {
			if (parms->p.verbose)
				dvb_log(_("%s: extension already done, ignoring: 0x%04x"), __func__, ext->ext_id);
			return 0;
		}

		if (!sect->allow_section_gaps && sect->ts_id == -1) {
			if (test_bit(h.section_id, ext->is_read_bits))
				return 0;
		} else if (ext->ext_id == h.id && ext->first_section == h.section_id) {
			/* tables like EIT can increment sections by gaps > 1.
			 * in this case, reading is done when a already read
			 * table is reached.
			 */
			if (parms->p.verbose)
				dvb_log(_("%s: section repeated on table 0x%02x, extension ID 0x%04x, section %d/%d: done"),
					__func__, h.table_id, ext->ext_id, h.section_id, h.last_section);

			ext->done = 1;

			goto ret;
		}
	}

	/* handle the sections */
	if (!sect->allow_section_gaps && sect->ts_id == -1)
		set_bit(h.section_id, ext->is_read_bits);

	if (dvb_table_initializers[tid])
		dvb_table_initializers[tid](&parms->p, buf,
					    buf_length - DVB_CRC_SIZE,
					    sect->table);
	else
		dvb_logerr(_("%s: no initializer for table %d"),
			   __func__, tid);

	if (!sect->allow_section_gaps && sect->ts_id == -1 &&
			is_all_bits_set(ext->last_section, ext->is_read_bits)) {
		if (parms->p.verbose)
			dvb_log(_("%s: table 0x%02x, extension ID 0x%04x: done"),
				__func__, h.table_id, h.id);
		ext->done = 1;
	}

	if (!ext->done)
		return 0;

ret:
	/* Check if all extensions are done */
	for (ext = priv->extensions, i = 0; i < priv->num_extensions; i++, ext++) {
		if (!ext->done) {
			if (parms->p.verbose)
				dvb_log(_("%s: extension not completed yet: 0x%04x"),
						__func__, ext->ext_id);
			return 0;
		}
	}

	/* Section was fully parsed */
	return 1;
}

int dvb_read_sections(struct dvb_v5_fe_parms *__p, int dmx_fd,
			     struct dvb_table_filter *sect,
			     unsigned timeout)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)__p;
	int ret;
	uint8_t *buf = NULL;
	uint8_t mask = 0xff;

	ret = dvb_parse_section_alloc(parms, sect);
	if (ret < 0)
		return ret;

	if (dvb_set_section_filter(dmx_fd, sect->pid, 1,
				   &sect->tid, &mask, NULL,
				   DMX_IMMEDIATE_START | DMX_CHECK_CRC)) {
		dvb_dmx_stop(dmx_fd);
		return -1;
	}
	if (parms->p.verbose)
		dvb_log(_("%s: waiting for table ID 0x%02x, program ID 0x%02x"),
			__func__, sect->tid, sect->pid);

	buf = calloc(DVB_MAX_PAYLOAD_PACKET_SIZE, 1);
	if (!buf) {
		dvb_logerr(_("%s: out of memory"), __func__);
		dvb_dmx_stop(dmx_fd);
		dvb_table_filter_free(sect);
		return -1;
	}


	do {
		int available;
		uint32_t crc;
		ssize_t buf_length = 0;

		do {
			available = dvb_poll(parms, dmx_fd, timeout);
		} while (available < 0 && errno == EOVERFLOW);

		if (parms->p.abort) {
			ret = 0;
			break;
		}
		if (available <= 0) {
			dvb_logerr(_("%s: no data read on section filter"), __func__);
			ret = -1;
			break;
		}
		buf_length = read(dmx_fd, buf, DVB_MAX_PAYLOAD_PACKET_SIZE);

		if (!buf_length) {
			dvb_logerr(_("%s: buf returned an empty buffer"), __func__);
			ret = -1;
			break;
		}
		if (buf_length < 0) {
			dvb_perror(_("dvb_read_section: read error"));
			ret = -2;
			break;
		}

		crc = dvb_crc32(buf, buf_length, 0xFFFFFFFF);
		if (crc != 0) {
			dvb_logerr(_("%s: crc error"), __func__);
			ret = -3;
			break;
		}

		ret = dvb_parse_section(parms, sect, buf, buf_length);
	} while (!ret);
	free(buf);
	dvb_dmx_stop(dmx_fd);
	dvb_table_filter_free(sect);

	if (ret > 0)
		ret = 0;

	return ret;
}

int dvb_read_section_with_id(struct dvb_v5_fe_parms *parms, int dmx_fd,
			     unsigned char tid, uint16_t pid,
			     int ts_id,
			     void **table, unsigned timeout)
{
	struct dvb_table_filter tab;

	tab.tid = tid;
	tab.pid = pid;
	tab.ts_id = ts_id;
	tab.table = table;
	tab.allow_section_gaps = 0;

	return dvb_read_sections(parms, dmx_fd, &tab, timeout);
}

struct dvb_v5_descriptors *dvb_scan_alloc_handler_table(uint32_t delivery_system)
{
	struct dvb_v5_descriptors *dvb_scan_handler;

	dvb_scan_handler = calloc(sizeof(*dvb_scan_handler), 1);
	if (!dvb_scan_handler)
		return NULL;

	dvb_scan_handler->delivery_system = delivery_system;

	return dvb_scan_handler;
}

void dvb_scan_free_handler_table(struct dvb_v5_descriptors *dvb_scan_handler)
{
	int i;

	if (!dvb_scan_handler)
		return;

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

struct dvb_v5_descriptors *dvb_get_ts_tables(struct dvb_v5_fe_parms *__p,
					     int dmx_fd,
					     uint32_t delivery_system,
					     unsigned other_nit,
					     unsigned timeout_multiply)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)__p;
	int rc;
	unsigned pat_pmt_time, sdt_time, nit_time, vct_time;
	int atsc_filter = 0;
	unsigned num_pmt = 0;

	struct dvb_v5_descriptors *dvb_scan_handler;

	dvb_scan_handler = dvb_scan_alloc_handler_table(delivery_system);
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
	rc = dvb_read_section(&parms->p, dmx_fd,
			      DVB_TABLE_PAT, DVB_TABLE_PAT_PID,
			      (void **)&dvb_scan_handler->pat,
			      pat_pmt_time * timeout_multiply);
	if (parms->p.abort)
		return dvb_scan_handler;
	if (rc < 0) {
		dvb_logerr(_("error while waiting for PAT table"));
		dvb_scan_free_handler_table(dvb_scan_handler);
		return NULL;
	}
	if (parms->p.verbose)
		dvb_table_pat_print(&parms->p, dvb_scan_handler->pat);

	/* ATSC-specific VCT table */
	if (atsc_filter) {
		rc = dvb_read_section(&parms->p, dmx_fd,
				      atsc_filter, ATSC_TABLE_VCT_PID,
				      (void **)&dvb_scan_handler->vct,
				      vct_time * timeout_multiply);
		if (parms->p.abort)
			return dvb_scan_handler;
		if (rc < 0)
			dvb_logerr(_("error while waiting for VCT table"));
		else if (parms->p.verbose)
			atsc_table_vct_print(&parms->p, dvb_scan_handler->vct);
	}

	/* PMT tables */
	dvb_scan_handler->program = calloc(dvb_scan_handler->pat->programs,
					   sizeof(*dvb_scan_handler->program));

	dvb_pat_program_foreach(program, dvb_scan_handler->pat) {
		dvb_scan_handler->program[num_pmt].pat_pgm = program;

		if (!program->service_id) {
			if (parms->p.verbose)
				dvb_log(_("Program #%d is network PID: 0x%04x"),
					num_pmt, program->pid);
			num_pmt++;
			continue;
		}
		if (parms->p.verbose)
			dvb_log(_("Program #%d ID 0x%04x, service ID 0x%04x"),
				num_pmt, program->pid, program->service_id);
		rc = dvb_read_section(&parms->p, dmx_fd,
				      DVB_TABLE_PMT, program->pid,
				      (void **)&dvb_scan_handler->program[num_pmt].pmt,
				      pat_pmt_time * timeout_multiply);
		if (parms->p.abort) {
			dvb_scan_handler->num_program = num_pmt + 1;
			return dvb_scan_handler;
		}
		if (rc < 0) {
			dvb_logerr(_("error while reading the PMT table for service 0x%04x"),
				   program->service_id);
			dvb_scan_handler->program[num_pmt].pmt = NULL;
		} else {
			if (parms->p.verbose)
				dvb_table_pmt_print(&parms->p,
						    dvb_scan_handler->program[num_pmt].pmt);
		}
		num_pmt++;
	}
	dvb_scan_handler->num_program = num_pmt;

	/* NIT table */
	rc = dvb_read_section(&parms->p, dmx_fd,
			      DVB_TABLE_NIT, DVB_TABLE_NIT_PID,
			      (void **)&dvb_scan_handler->nit,
			      nit_time * timeout_multiply);
	if (parms->p.abort)
		return dvb_scan_handler;
	if (rc < 0)
		dvb_logerr(_("error while reading the NIT table"));
	else if (parms->p.verbose)
		dvb_table_nit_print(&parms->p, dvb_scan_handler->nit);

	/* SDT table */
	if (!dvb_scan_handler->vct || other_nit) {
		rc = dvb_read_section(&parms->p, dmx_fd,
				DVB_TABLE_SDT, DVB_TABLE_SDT_PID,
				(void **)&dvb_scan_handler->sdt,
				sdt_time * timeout_multiply);
		if (parms->p.abort)
			return dvb_scan_handler;
		if (rc < 0)
			dvb_logerr(_("error while reading the SDT table"));
		else if (parms->p.verbose)
			dvb_table_sdt_print(&parms->p, dvb_scan_handler->sdt);
	}

	/* NIT/SDT other tables */
	if (other_nit) {
		if (parms->p.verbose)
			dvb_log(_("Parsing other NIT/SDT"));
		rc = dvb_read_section(&parms->p, dmx_fd,
				      DVB_TABLE_NIT2, DVB_TABLE_NIT_PID,
				      (void **)&dvb_scan_handler->nit,
				      nit_time * timeout_multiply);
		if (parms->p.abort)
			return dvb_scan_handler;
		if (rc < 0)
			dvb_logerr(_("error while reading the NIT table"));
		else if (parms->p.verbose)
			dvb_table_nit_print(&parms->p, dvb_scan_handler->nit);

		rc = dvb_read_section(&parms->p, dmx_fd,
				DVB_TABLE_SDT2, DVB_TABLE_SDT_PID,
				(void **)&dvb_scan_handler->sdt,
				sdt_time * timeout_multiply);
		if (parms->p.abort)
			return dvb_scan_handler;
		if (rc < 0)
			dvb_logerr(_("error while reading the SDT table"));
		else if (parms->p.verbose)
			dvb_table_sdt_print(&parms->p, dvb_scan_handler->sdt);
	}

	return dvb_scan_handler;
}

struct dvb_v5_descriptors *dvb_scan_transponder(struct dvb_v5_fe_parms *__p,
					        struct dvb_entry *entry,
						int dmx_fd,
					        check_frontend_t *check_frontend,
					        void *args,
						unsigned other_nit,
						unsigned timeout_multiply)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)__p;
	struct dvb_v5_descriptors *dvb_scan_handler = NULL;
	uint32_t freq, delsys = SYS_UNDEFINED;
	int i, rc;

	/* First of all, set the delivery system */
	dvb_retrieve_entry_prop(entry, DTV_DELIVERY_SYSTEM, &delsys);
	dvb_set_compat_delivery_system(&parms->p, delsys);

	/* Copy data into parms */
	for (i = 0; i < entry->n_props; i++) {
		uint32_t data = entry->props[i].u.data;

		/* Don't change the delivery system */
		if (entry->props[i].cmd == DTV_DELIVERY_SYSTEM)
			continue;

		dvb_fe_store_parm(&parms->p, entry->props[i].cmd, data);

		if (parms->p.current_sys == SYS_ATSC &&
			entry->props[i].cmd == DTV_MODULATION) {
			if (data != VSB_8 && data != VSB_16)
				dvb_fe_store_parm(&parms->p,
						DTV_DELIVERY_SYSTEM,
						SYS_DVBC_ANNEX_B);
		}
	}

	rc = dvb_fe_set_parms(&parms->p);
	if (rc < 0) {
		dvb_perror(_("dvb_fe_set_parms failed"));
		return NULL;
	}

	/* As the DVB core emulates it, better to always use auto */
	dvb_fe_store_parm(&parms->p, DTV_INVERSION, INVERSION_AUTO);

	dvb_fe_retrieve_parm(&parms->p, DTV_FREQUENCY, &freq);
	if (parms->p.verbose)
		dvb_fe_prt_parms(&parms->p);

	rc = check_frontend(args, &parms->p);
	if (rc < 0)
		return NULL;

	dvb_scan_handler = dvb_get_ts_tables(&parms->p, dmx_fd,
					parms->p.current_sys,
					other_nit,
					timeout_multiply);

	return dvb_scan_handler;
}

int dvb_estimate_freq_shift(struct dvb_v5_fe_parms *__p)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)__p;
	uint32_t shift = 0, bw = 0, min_bw = 0, symbol_rate, ro;
	int rolloff = 0;
	int divisor = 100;

	/* Need to handle only cable/satellite and ATSC standards */
	switch (parms->p.current_sys) {
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		rolloff = 115;
		break;
	case SYS_DVBS:
		divisor = 100000;
		rolloff = 135;
		break;
	case SYS_DVBS2:
	case SYS_DSS:
	case SYS_TURBO:
		divisor = 100000;
		dvb_fe_retrieve_parm(&parms->p, DTV_ROLLOFF, &ro);
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
	case SYS_ISDBS:
		/* since ISDBS uses fixed symbol_rate & rolloff,
		 * those parameters are not mandatory in channel config file.
		 */
		bw = 28860 * 135 / 100;
		break;
	case SYS_DVBT2:
		min_bw = 1700000;
		break;
	case SYS_ISDBT:
	case SYS_DVBT:
	case SYS_DTMB:
		/* FIXME: does it also apply for DTMB? */
		min_bw = 6000000;
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
		dvb_fe_retrieve_parm(&parms->p, DTV_SYMBOL_RATE, &symbol_rate);
		bw = (symbol_rate * rolloff) / divisor;
	}
	if (!bw)
		dvb_fe_retrieve_parm(&parms->p, DTV_BANDWIDTH_HZ, &bw);
	if (!bw)
		bw = min_bw;
	if (!bw)
		dvb_log(_("Cannot calc frequency shift. " \
			  "Either bandwidth/symbol-rate is unavailable (yet)."));
	/*
	 * If the max frequency shift between two frequencies is below
	 * than the used bandwidth / 8, it should be the same channel.
	 */
	shift = bw / 8;

	return shift;
}

int dvb_new_freq_is_needed(struct dvb_entry *entry, struct dvb_entry *last_entry,
			   uint32_t freq, enum dvb_sat_polarization pol, int shift)
{
	return dvb_new_entry_is_needed(entry, last_entry, freq, shift,
					 pol, NO_STREAM_ID_FILTER);
}

struct dvb_entry *dvb_scan_add_entry(struct dvb_v5_fe_parms *__p,
				     struct dvb_entry *first_entry,
			             struct dvb_entry *entry,
			             uint32_t freq, uint32_t shift,
			             enum dvb_sat_polarization pol)
{
	return dvb_scan_add_entry_ex(__p, first_entry, entry, freq, shift,
					pol, NO_STREAM_ID_FILTER);
}

int dvb_new_entry_is_needed(struct dvb_entry *entry,
			    struct dvb_entry *last_entry,
			    uint32_t freq, int shift,
			    enum dvb_sat_polarization pol, uint32_t stream_id)
{
	for (; entry != last_entry; entry = entry->next) {
		int i;

		for (i = 0; i < entry->n_props; i++) {
			uint32_t data = entry->props[i].u.data;

			if (entry->props[i].cmd == DTV_FREQUENCY) {
				if (freq < data - shift || freq > data + shift)
					break;
			}
			if (pol != POLARIZATION_OFF
			    && entry->props[i].cmd == DTV_POLARIZATION) {
				if (data != pol)
					break;
			}
			/* NO_STREAM_ID_FILTER: stream_id is not used.
			 * 0: unspecified/auto. libdvbv5 default value.
			 */
			if (stream_id != NO_STREAM_ID_FILTER && stream_id != 0
			    && entry->props[i].cmd == DTV_STREAM_ID) {
				if (data != stream_id)
					break;
			}
		}
		if (i == entry->n_props && entry->n_props > 0)
			return 0;
	}

	return 1;
}

struct dvb_entry *dvb_scan_add_entry_ex(struct dvb_v5_fe_parms *__p,
					struct dvb_entry *first_entry,
					struct dvb_entry *entry,
					uint32_t freq, uint32_t shift,
					enum dvb_sat_polarization pol,
					uint32_t stream_id)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)__p;
	struct dvb_entry *new_entry;
	int i, n = 2;

	if (!dvb_new_entry_is_needed(first_entry, NULL, freq, shift, pol,
				     stream_id))
		return NULL;

	/* Clone the current entry into a new entry */
	new_entry = calloc(sizeof(*new_entry), 1);
	if (!new_entry) {
		dvb_perror(_("not enough memory for a new scanning frequency/TS"));
		return NULL;
	}

	/*
	 * We can't just copy the entire entry struct, as some strings
	 * like lnb, channel, vchannel will be freed multiple times.
	 * So, copy the props and the Satellite parameters only.
	 */
	memcpy(new_entry->props, entry->props, sizeof(entry->props));
	new_entry->n_props = entry->n_props;
	new_entry->sat_number = entry->sat_number;
	new_entry->freq_bpf = entry->freq_bpf;
	new_entry->diseqc_wait = entry->diseqc_wait;
	if (entry->lnb)
		new_entry->lnb = strdup(entry->lnb);

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
			dvb_log(_("New transponder/channel found: #%d: %d"),
			        n, freq);
			entry->next = new_entry;
			new_entry->next = NULL;
			return new_entry;
		}
	}

	/* This should never happen */
	dvb_logerr(_("BUG: Couldn't add %d to the scan frequency list."), freq);
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
	dvb_store_entry_prop(new, DTV_MODULATION,
			     dvbc_modulation_table[d->modulation]);
	dvb_store_entry_prop(new, DTV_SYMBOL_RATE, d->symbol_rate);
	dvb_store_entry_prop(new, DTV_INNER_FEC, dvbc_fec_table[d->fec_inner]);

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

		dvb_store_entry_prop(tr->entry, DTV_TRANSMISSION_MODE, mode);
		dvb_store_entry_prop(tr->entry, DTV_GUARD_INTERVAL, guard);
		return;
	}

	for (i = 0; i < d->num_freqs; i++) {
		new = dvb_scan_add_entry(tr->parms, tr->first_entry, tr->entry,
					 d->frequency[i], tr->shift, tr->pol);
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

       len = d->length / sizeof(*d->partial_reception);

	for (i = 0; i < len; i++) {
		if (tr->entry->service_id == d->partial_reception[i].service_id) {
			dvb_store_entry_prop(tr->entry,
					     DTV_ISDBT_PARTIAL_RECEPTION, 1);
			return;
		}
	}
	dvb_store_entry_prop(tr->entry, DTV_ISDBT_PARTIAL_RECEPTION, 0);
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

		dvb_store_entry_prop(tr->entry, DTV_DELIVERY_SYSTEM,
				     SYS_DVBT2);
		dvb_store_entry_prop(tr->entry, DTV_STREAM_ID,
				     t2->plp_id);

		if (d->length -1 <= 4)
			return;

		dvb_store_entry_prop(tr->entry, DTV_BANDWIDTH_HZ,
				     dvbt2_bw[t2->bandwidth]);
		dvb_store_entry_prop(tr->entry, DTV_GUARD_INTERVAL,
				     dvbt2_interval[t2->guard_interval]);
		dvb_store_entry_prop(tr->entry, DTV_TRANSMISSION_MODE,
				     dvbt2_transmission_mode[t2->transmission_mode]);

		return;
	}

	if (d->length -1 <= 4)
		return;

	for (i = 0; i < t2->frequency_loop_length; i++) {
		new = dvb_scan_add_entry_ex(tr->parms,
					    tr->first_entry, tr->entry,
					    t2->centre_frequency[i] * 10,
					    tr->shift, tr->pol, t2->plp_id);
		if (!new)
			continue;

		dvb_store_entry_prop(new, DTV_DELIVERY_SYSTEM,
				     SYS_DVBT2);
		dvb_store_entry_prop(new, DTV_STREAM_ID,
				     t2->plp_id);
		dvb_store_entry_prop(new, DTV_BANDWIDTH_HZ,
				     dvbt2_bw[t2->bandwidth]);
		dvb_store_entry_prop(new, DTV_GUARD_INTERVAL,
				     dvbt2_interval[t2->guard_interval]);
		dvb_store_entry_prop(new, DTV_TRANSMISSION_MODE,
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
	dvb_store_entry_prop(new, DTV_MODULATION,
				dvbt_modulation[d->constellation]);
	dvb_store_entry_prop(new, DTV_BANDWIDTH_HZ,
				dvbt_bw[d->bandwidth]);
	dvb_store_entry_prop(new, DTV_CODE_RATE_HP,
				dvbt_code_rate[d->code_rate_hp_stream]);
	dvb_store_entry_prop(new, DTV_CODE_RATE_LP,
				dvbt_code_rate[d->code_rate_lp_stream]);
	dvb_store_entry_prop(new, DTV_GUARD_INTERVAL,
				dvbt_interval[d->guard_interval]);
	dvb_store_entry_prop(new, DTV_TRANSMISSION_MODE,
				dvbt_transmission_mode[d->transmission_mode]);
	dvb_store_entry_prop(new, DTV_HIERARCHY,
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

	dvb_store_entry_prop(new, DTV_MODULATION,
			     dvbs_modulation[d->modulation_system]);
	dvb_store_entry_prop(new, DTV_POLARIZATION,
			     dvbs_polarization[d->polarization]);
	dvb_store_entry_prop(new, DTV_SYMBOL_RATE,
			     d->symbol_rate);
	dvb_store_entry_prop(new, DTV_INNER_FEC,
			     dvbs_dvbc_dvbs_freq_inner[d->fec]);
	dvb_store_entry_prop(new, DTV_ROLLOFF,
			     dvbs_rolloff[d->roll_off]);
	if (d->roll_off != 0)
		dvb_store_entry_prop(new, DTV_DELIVERY_SYSTEM,
				     SYS_DVBS2);
}

static void add_update_nit_isdbs(struct dvb_table_nit *nit,
				 struct dvb_table_nit_transport *tran,
				 struct dvb_desc *desc,
				 void *priv)
{
	struct update_transponders *tr = priv;
	struct dvb_entry *new;
	/* FIXME:
	 * use the ISDB-S specific satellite delsys descriptor structure,
	 * instead of overloading to the EN300-468's one, dvb_desc_sat.
	 * The following members are incompatible:
	 *  {.modulation_type, .modulation_system, .roll_off, .fec}
	 */
	struct dvb_desc_sat *d = (void *)desc;
	uint32_t ts_id;

	if (tr->update)
		return;

	ts_id = tran->transport_id;
	new = dvb_scan_add_entry_ex(tr->parms, tr->first_entry, tr->entry,
				    d->frequency, tr->shift, tr->pol, ts_id);
	if (!new)
		return;

	/* Set (optional) ISDB-S props for the transponder */
	/* FIXME: fill in other props like DTV_MODULATION, DTV_INNER_FEC.
	 *   This requires extending the enum definitions in DVBv5 API
	 *   to include the ISDB-S/T specific modulation/fec values,
	 *   such as "BPSK" and "look TMCC".
	 * Since even "AUTO" is not defined, skip setting them now.
	 */
	dvb_store_entry_prop(new, DTV_POLARIZATION,
			     dvbs_polarization[d->polarization]);
	dvb_store_entry_prop(new, DTV_SYMBOL_RATE,
			     d->symbol_rate);
}


static void __dvb_add_update_transponders(struct dvb_v5_fe_parms_priv *parms,
					  struct dvb_v5_descriptors *dvb_scan_handler,
					  struct dvb_entry *first_entry,
					  struct dvb_entry *entry,
					  uint32_t update)
{
	struct update_transponders tr = {
		.parms = &parms->p,
		.dvb_scan_handler = dvb_scan_handler,
		.first_entry = first_entry,
		.entry = entry,
		.update = update,
		.pol = POLARIZATION_OFF,
	};

	if (!dvb_scan_handler->nit)
		return;

	tr.shift = dvb_estimate_freq_shift(&parms->p);

	switch (parms->p.current_sys) {
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		dvb_table_nit_descriptor_handler(
				&parms->p, dvb_scan_handler->nit,
				cable_delivery_system_descriptor,
				NULL, add_update_nit_dvbc, &tr);
		return;
	case SYS_ISDBT:
		dvb_table_nit_descriptor_handler(
				&parms->p, dvb_scan_handler->nit,
				partial_reception_descriptor,
				NULL, add_update_nit_1seg, &tr);
		dvb_table_nit_descriptor_handler(&parms->p,
				dvb_scan_handler->nit,
				ISDBT_delivery_system_descriptor,
				NULL, add_update_nit_isdbt, &tr);
		return;
	case SYS_DVBT:
	case SYS_DVBT2:
	case SYS_DTMB:	/* FIXME: are DTMB nit tables equal to DVB-T? */
		dvb_table_nit_descriptor_handler(
				&parms->p, dvb_scan_handler->nit,
				extension_descriptor,
				NULL, add_update_nit_dvbt2, &tr);

		dvb_table_nit_descriptor_handler(
				&parms->p, dvb_scan_handler->nit,
				terrestrial_delivery_system_descriptor,
				NULL, add_update_nit_dvbt, &tr);
		return;
	case SYS_DVBS:
	case SYS_DVBS2:
		dvb_table_nit_descriptor_handler(
				&parms->p, dvb_scan_handler->nit,
				satellite_delivery_system_descriptor,
				NULL, add_update_nit_dvbs, &tr);
		return;
	case SYS_ISDBS:
		/* see the FIXME: in add_update_nit_isdbs() */
		dvb_table_nit_descriptor_handler(
				&parms->p, dvb_scan_handler->nit,
				satellite_delivery_system_descriptor,
				NULL, add_update_nit_isdbs, &tr);
		return;

	default:
		dvb_log(_("Transponders detection not implemented for this standard yet."));
		return;
	}
}

void dvb_add_scaned_transponders(struct dvb_v5_fe_parms *__p,
				 struct dvb_v5_descriptors *dvb_scan_handler,
				 struct dvb_entry *first_entry,
				 struct dvb_entry *entry)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)__p;

	return __dvb_add_update_transponders(parms, dvb_scan_handler,
					     first_entry, entry, 0);
}

void dvb_update_transponders(struct dvb_v5_fe_parms *__p,
			     struct dvb_v5_descriptors *dvb_scan_handler,
			     struct dvb_entry *first_entry,
			     struct dvb_entry *entry)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)__p;

	return __dvb_add_update_transponders(parms, dvb_scan_handler,
					     first_entry, entry, 1);
}
