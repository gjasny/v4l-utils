/*
 * Copyright (c) 2011-2014 - Mauro Carvalho Chehab
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
#ifndef _LIBSCAN_H
#define _LIBSCAN_H

#include <stdint.h>
#include <linux/dvb/dmx.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-sat.h>

/**
 * @file dvb-scan.h
 * @ingroup frontend_scan
 * @brief Provides interfaces to scan programs inside MPEG-TS digital TV streams.
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Mauro Carvalho Chehab
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

/* According with ISO/IEC 13818-1:2007 */

#define MAX_TABLE_SIZE 1024 * 1024

#ifdef __cplusplus
extern "C" {
#endif

struct dvb_entry;

/**
 * @struct dvb_v5_descriptors_program
 * @brief Associates PMT with PAT tables
 * @ingroup frontend_scan
 *
 * @param pat_pgm	pointer for PAT descriptor
 * @param pmt	pointer for PMT descriptor
 */
struct dvb_v5_descriptors_program {
	struct dvb_table_pat_program *pat_pgm;
	struct dvb_table_pmt *pmt;
};

/**
 * @struct dvb_v5_descriptors
 * @brief Contains the descriptors needed to scan the Service ID and other relevant info at a MPEG-TS Digital TV stream
 * @ingroup frontend_scan
 *
 * @param delivery_system Delivery system of the parsed MPEG-TS
 * @param entry		struct dvb_entry pointer (see dvb-file.h)
 * @param pat		PAT table descriptor pointer (table ID 0x00).
 * @param vct		VCT table descriptor pointer (either table ID 0xc8,
 * 			for TVCT or table ID 0xc9, for CVCT)
 * @param program	PAT/PMT array associated programs found at MPEG-TS
 * @param num_program	Number of program entries at @ref program array.
 * @param nit		NIT table descriptor pointer for table ID 0x40.
 * @param sdt		SDT table descriptor pointer for table ID 0x42.
 * @param other_nits	Contains an array of pointers to the other NIT
 *			extension tables identified by table ID 0x41.
 * @param num_other_nits Number of NIT tables at @ref other_nits array.
 * @param other_sdts	Contains an array of pointers to the other NIT
 *			extension tables identified by table ID 0x46.
 * @param num_other_sdts Number of NIT tables at @ref other_sdts array.
 *
 * Those descriptors are filled by the scan routines when the tables are
 * found. Otherwise, they're NULL.
 *
 * @note: Never alloc this struct yourself. This is meant to always be
 * allocated via dvb_scan_alloc_handler_table() or via dvb_get_ts_tables().
 */
struct dvb_v5_descriptors {
	uint32_t delivery_system;

	struct dvb_entry *entry;
	unsigned num_entry;

	struct dvb_table_pat *pat;
	struct atsc_table_vct *vct;
	struct dvb_v5_descriptors_program *program;
	struct dvb_table_nit *nit;
	struct dvb_table_sdt *sdt;

	unsigned num_program;

	struct dvb_table_nit **other_nits;
	unsigned num_other_nits;

	struct dvb_table_sdt **other_sdts;
	unsigned num_other_sdts;
};

/**
 * @struct dvb_table_filter
 * @brief Describes the PES filters used by DVB scan
 * @ingroup frontend_scan
 *
 * @param tid		Table ID
 * @param pid		Program ID
 * @param ts_id		Table section ID (for multisession filtering). If no
 *			specific table section is needed, -1 should be used
 * @param table		pointer to a pointer for the table struct to be filled
 * @param allow_section_gaps	Allow non-continuous section numbering
 * @param priv		Internal structure used inside the DVB core. shouldn't
 *			be touched externally.
 */
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
/**
 * @brief deallocates all data associated with a table filter
 * @ingroup frontend_scan
 *
 * @param sect	table filter pointer
 */
void dvb_table_filter_free(struct dvb_table_filter *sect);

/**
 * @brief read MPEG-TS tables that comes from a DTV card
 * @ingroup frontend_scan
 *
 * @param parms		pointer to struct dvb_v5_fe_parms created when the
 *			frontend is opened
 * @param dmx_fd	an opened demux file descriptor
 * @param tid		Table ID
 * @param pid		Program ID
 * @param table		pointer to a pointer for the table struct to be filled
 * @param timeout	Limit, in seconds, to read a MPEG-TS table
 *
 * This function is used to read the DVB tables by specifying a table ID and
 * a program ID. The libdvbv5 should have a parser for the descriptors of the
 * table type that should be parsed.
 * The table will be automatically allocated on success.
 * The function will read on the specified demux and return when reading is
 * done or an error has occurred. If table is not NULL after the call, it has
 * to be freed with the apropriate free table function (even if an error has
 * occurred).
 *
 * If the application wants to abort the read operation, it can change the
 * value of parms->p.abort to 1.
 *
 * Returns 0 on success or a negative error code.
 *
 * Example usage:
 * @code
 * struct dvb_table_pat *pat;
 * int r = dvb_read_section( parms, dmx_fd, DVB_TABLE_PAT, DVB_TABLE_PAT_PID,
 *			    (void **) &pat, 5 );
 * if (r < 0)
 *	dvb_logerr("error reading PAT table");
 * else {
 *	// do something with pat
 * }
 * if (pat)
 *	dvb_table_pat_free( pat );
 * @endcode
 */
int dvb_read_section(struct dvb_v5_fe_parms *parms, int dmx_fd,
		     unsigned char tid, uint16_t pid, void **table,
		     unsigned timeout);

/**
 * @brief read MPEG-TS tables that comes from a DTV card
 *				with an specific table section ID
 * @ingroup frontend_scan
 *
 * @param parms		pointer to struct dvb_v5_fe_parms created when the
 * 			frontend is opened
 * @param dmx_fd	an opened demux file descriptor
 * @param tid		Table ID
 * @param pid		Program ID
 * @param ts_id		Table section ID (for multisession filtering). If no
 *			specific table section is needed, -1 should be used
 * @param table		pointer to a pointer for the table struct to be filled
 * @param timeout	limit, in seconds, to read a MPEG-TS table
 *
 * This is a variant of dvb_read_section() that also seeks for an specific
 * table section ID given by ts_id.
 */
 int dvb_read_section_with_id(struct dvb_v5_fe_parms *parms, int dmx_fd,
			      unsigned char tid, uint16_t pid, int ts_id,
			      void **table, unsigned timeout);

/**
 * @brief read MPEG-TS tables that comes from a DTV card
 * @ingroup frontend_scan
 *
 * @param parms		pointer to struct dvb_v5_fe_parms created when the
 *			frontend is opened
 * @param dmx_fd	an opened demux file descriptor
 * @param sect		section filter pointer
 * @param timeout	limit, in seconds, to read a MPEG-TS table
 *
 * This is a variant of dvb_read_section() that uses a struct dvb_table_filter
 * to specify the filter to use.
 */
int dvb_read_sections(struct dvb_v5_fe_parms *parms, int dmx_fd,
			     struct dvb_table_filter *sect,
			     unsigned timeout);

/**
 * @brief allocates a struct dvb_v5_descriptors
 * @ingroup frontend_scan
 *
 * @param delivery_system	Delivery system to be used on the table
 *
 * At success, returns a pointer. NULL otherwise.
 */
struct dvb_v5_descriptors *dvb_scan_alloc_handler_table(uint32_t delivery_system);

/**
 * @brief frees a struct dvb_v5_descriptors
 * @ingroup frontend_scan
 *
 * @param dvb_scan_handler	pointer to the struct to be freed.
 */
void dvb_scan_free_handler_table(struct dvb_v5_descriptors *dvb_scan_handler);

/**
 * @brief Scans a DVB stream, looking for the tables needed to
 *			 identify the programs inside a MPEG-TS
 * @ingroup frontend_scan
 *
 * @param parms			pointer to struct dvb_v5_fe_parms created when
 *				the frontend is opened
 * @param dmx_fd		an opened demux file descriptor
 * @param delivery_system	delivery system to be scanned
 * @param other_nit		use alternate table IDs for NIT and other tables
 * @param timeout_multiply	improves the timeout for each table reception
 * 				by using a value that will multiply the wait
 *				time.
 *
 * Given an opened frontend and demux, this function seeks for all programs
 * available at the transport stream, and parses the following tables:
 * PAT, PMT, NIT, SDT (and VCT, if the delivery system is ATSC).
 *
 * On sucess, it returns a pointer to a struct dvb_v5_descriptors, that can
 * either be used to tune into a service or to be stored inside a file.
 */
struct dvb_v5_descriptors *dvb_get_ts_tables(struct dvb_v5_fe_parms *parms, int dmx_fd,
					  uint32_t delivery_system,
					  unsigned other_nit,
					  unsigned timeout_multiply);

/**
 * @brief frees a struct dvb_v5_descriptors
 * @ingroup frontend_scan
 *
 * @param dvb_desc	pointed to the structure to be freed.
 *
 * This function recursively frees everything that is allocated by
 * dvb_get_ts_tables() and stored at dvb_desc, including dvb_desc itself.
 */
void dvb_free_ts_tables(struct dvb_v5_descriptors *dvb_desc);

/**
 * @brief Callback for the application to show the frontend status
 * @ingroup frontend_scan
 *
 * @param args		a pointer, opaque to libdvbv5, to be used by the
 *			application if needed.
 * @param parms		pointer to struct dvb_v5_fe_parms created when the
 *			frontend is opened
 */
typedef int (check_frontend_t)(void *args, struct dvb_v5_fe_parms *parms);

/**
 * @brief Scans a DVB dvb_add_scaned_transponder
 * @ingroup frontend_scan
 *
 * @param parms		pointer to struct dvb_v5_fe_parms created when the
 *			frontend is opened
 * @param entry		DVB file entry that corresponds to a transponder to be
 * 			tuned
 * @param dmx_fd		an opened demux file descriptor
 * @param check_frontend	a pointer to a function that will show the frontend
 *			status while tuning into a transponder
 * @param args		a pointer, opaque to libdvbv5, that will be used when
 *			calling check_frontend. It should contain any parameters
 *			that could be needed by check_frontend.
 * @param other_nit		Use alternate table IDs for NIT and other tables
 * @param timeout_multiply	Improves the timeout for each table reception, by
 *
 * This is the function that applications should use when doing a transponders
 * scan. It does everything needed to fill the entries with DVB programs
 * (virtual channels) and detect the PIDs associated with them.
 *
 * A typical usage is to after open a channel file, open a dmx_fd and open
 * a frontend. Then, seek for the MPEG tables on all the transponder
 * frequencies with:
 *
 * @code
 * for (entry = dvb_file->first_entry; entry != NULL; entry = entry->next) {
 *	struct dvb_v5_descriptors *dvb_scan_handler = NULL;
 *
 *	dvb_scan_handler = dvb_scan_transponder(parms, entry, dmx_fd,
 *						&check_frontend, args,
 *						args->other_nit,
 *						args->timeout_multiply);
 *	if (parms->abort) {
 *		dvb_scan_free_handler_table(dvb_scan_handler);
 *		break;
 *	}
 *	if (dvb_scan_handler) {
 *		dvb_store_channel(&dvb_file_new, parms, dvb_scan_handler,
 *				  args->get_detected, args->get_nit);
 *		dvb_scan_free_handler_table(dvb_scan_handler);
 * 	}
 * }
 * @endcode
 */
struct dvb_v5_descriptors *dvb_scan_transponder(struct dvb_v5_fe_parms *parms,
						struct dvb_entry *entry,
						int dmx_fd,
						check_frontend_t *check_frontend,
						void *args,
						unsigned other_nit,
						unsigned timeout_multiply);


/**
 * @brief Add new transponders to a dvb_file
 * @ingroup frontend_scan
 *
 * @param parms		pointer to struct dvb_v5_fe_parms created when the
 *			frontend is opened
 * @param dvb_scan_handler	pointer to a struct dvb_v5_descriptors containing
 *			scaned MPEG-TS
 * @param first_entry	first entry of a DVB file struct
 * @param entry		current entry on a DVB file struct
 *
 * When the NIT table is parsed, some new transponders could be described
 * inside. This function adds new entries to a dvb_file struct, pointing
 * to those new transponders. It is used inside the scan loop, as shown at
 * the dvb_scan_transponder(), to add new channels.
 *
 * Example:
 * @code
 * for (entry = dvb_file->first_entry; entry != NULL; entry = entry->next) {
 *	struct dvb_v5_descriptors *dvb_scan_handler = NULL;
 *
 *	dvb_scan_handler = dvb_scan_transponder(parms, entry, dmx_fd,
 *						&check_frontend, args,
 *						args->other_nit,
 *						args->timeout_multiply);
 *	if (parms->abort) {
 *		dvb_scan_free_handler_table(dvb_scan_handler);
 *		break;
 *	}
 *	if (dvb_scan_handler) {
 *		dvb_store_channel(&dvb_file_new, parms, dvb_scan_handler,
 *				  args->get_detected, args->get_nit);
 *		dvb_scan_free_handler_table(dvb_scan_handler);
 *
 *		dvb_add_scaned_transponders(parms, dvb_scan_handler,
 *					    dvb_file->first_entry, entry);
 *
 *		dvb_scan_free_handler_table(dvb_scan_handler);
 * 	}
 * }
 * @endcode
 */
void dvb_add_scaned_transponders(struct dvb_v5_fe_parms *parms,
				 struct dvb_v5_descriptors *dvb_scan_handler,
				 struct dvb_entry *first_entry,
				 struct dvb_entry *entry);

#ifndef _DOXYGEN
/*
 * Some ancillary functions used internally inside the library, used to
 * identify duplicated transport streams and add new found transponder entries
 */
int dvb_estimate_freq_shift(struct dvb_v5_fe_parms *parms);

int dvb_new_freq_is_needed(struct dvb_entry *entry, struct dvb_entry *last_entry,
			   uint32_t freq, enum dvb_sat_polarization pol, int shift);

struct dvb_entry *dvb_scan_add_entry(struct dvb_v5_fe_parms *parms,
				     struct dvb_entry *first_entry,
			             struct dvb_entry *entry,
			             uint32_t freq, uint32_t shift,
			             enum dvb_sat_polarization pol);

int dvb_new_entry_is_needed(struct dvb_entry *entry,
			    struct dvb_entry *last_entry,
			    uint32_t freq, int shift,
			    enum dvb_sat_polarization pol, uint32_t stream_id);

struct dvb_entry *dvb_scan_add_entry_ex(struct dvb_v5_fe_parms *parms,
					struct dvb_entry *first_entry,
					struct dvb_entry *entry,
					uint32_t freq, uint32_t shift,
					enum dvb_sat_polarization pol,
					uint32_t stream_id);

void dvb_update_transponders(struct dvb_v5_fe_parms *parms,
			     struct dvb_v5_descriptors *dvb_scan_handler,
			     struct dvb_entry *first_entry,
			     struct dvb_entry *entry);
#endif

#ifdef __cplusplus
}
#endif

#endif
