/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 * Copyright (c) 2012 - Andre Roth <neolynx@gmail.com>
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
 *
 */

#ifndef _NIT_H
#define _NIT_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#include <libdvbv5/header.h>
#include <libdvbv5/descriptors.h>

/**
 * @file nit.h
 * @ingroup dvb_table
 * @brief Provides the descriptors for NIT MPEG-TS table
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Mauro Carvalho Chehab
 * @author Andre Roth
 *
 * @par Bug Report
 * Please submit bug report and patches to linux-media@vger.kernel.org
 *
 * @par Relevant specs
 * The table described herein is defined at:
 * - ISO/IEC 13818-1
 * - ETSI EN 300 468
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

/**
 * @def DVB_TABLE_NIT
 *	@brief NIT table ID
 *	@ingroup dvb_table
 * @def DVB_TABLE_NIT2
 *	@brief NIT table ID (alternative table ID)
 *	@ingroup dvb_table
 * @def DVB_TABLE_NIT_PID
 *	@brief NIT Program ID
 *	@ingroup dvb_table
 */
#define DVB_TABLE_NIT      0x40
#define DVB_TABLE_NIT2     0x41
#define DVB_TABLE_NIT_PID  0x10

/**
 * @union dvb_table_nit_transport_header
 * @brief MPEG-TS NIT transport header
 * @ingroup dvb_table
 *
 * @param transport_length	transport length
 *
 * This structure is used to store the original NIT transport header,
 * converting the integer fields to the CPU endianness.
 *
 * The undocumented parameters are used only internally by the API and/or
 * are fields that are reserved. They shouldn't be used, as they may change
 * on future API releases.
 */
union dvb_table_nit_transport_header {
	uint16_t bitfield;
	struct {
		uint16_t transport_length:12;
		uint16_t reserved:4;
	} __attribute__((packed));
} __attribute__((packed));

/**
 * @struct dvb_table_nit_transport
 * @brief MPEG-TS NIT transport table
 * @ingroup dvb_table
 *
 * @param transport_id	transport id
 * @param network_id	network id
 * @param desc_length	desc length
 * @param descriptor	pointer to struct dvb_desc
 * @param next		pointer to struct dvb_table_nit_transport
 *
 * This structure is used to store the original NIT transport table,
 * converting the integer fields to the CPU endianness.
 *
 * The undocumented parameters are used only internally by the API and/or
 * are fields that are reserved. They shouldn't be used, as they may change
 * on future API releases.
 *
 * Everything after dvb_table_nit_transport::descriptor (including it) won't
 * be bit-mapped to the data parsed from the MPEG TS. So, metadata are added
 * there.
 */
struct dvb_table_nit_transport {
	uint16_t transport_id;
	uint16_t network_id;
	union {
		uint16_t bitfield;
		struct {
			uint16_t desc_length:12;
			uint16_t reserved:4;
		} __attribute__((packed));
	} __attribute__((packed));
	struct dvb_desc *descriptor;
	struct dvb_table_nit_transport *next;
} __attribute__((packed));

/**
 * @struct dvb_table_nit
 * @brief MPEG-TS NIT table
 * @ingroup dvb_table
 *
 * @param header	struct dvb_table_header content
 * @param desc_length	descriptor length
 * @param descriptor	pointer to struct dvb_desc
 * @param transport	pointer to struct dvb_table_nit_transport
 *
 * This structure is used to store the original NIT table,
 * converting the integer fields to the CPU endianness.
 *
 * The undocumented parameters are used only internally by the API and/or
 * are fields that are reserved. They shouldn't be used, as they may change
 * on future API releases.
 *
 * Everything after dvb_table_nit::descriptor (including it) won't be bit-mapped
 * to the data parsed from the MPEG TS. So, metadata are added there.
 */
struct dvb_table_nit {
	struct dvb_table_header header;
	union {
		uint16_t bitfield;
		struct {
			uint16_t desc_length:12;
			uint16_t reserved:4;
		} __attribute__((packed));
	} __attribute__((packed));
	struct dvb_desc *descriptor;
	struct dvb_table_nit_transport *transport;
} __attribute__((packed));

/**
 * @brief typedef for a callback used when a NIT table entry is found
 * @ingroup dvb_table
 *
 * @param nit	a struct dvb_table_nit pointer
 * @param desc	a struct dvb_desc pointer
 * @param priv	an opaque optional pointer
 */
typedef void nit_handler_callback_t(struct dvb_table_nit *nit,
				    struct dvb_desc *desc,
				    void *priv);

/**
 * @brief typedef for a callback used when a NIT transport table entry is found
 * @ingroup dvb_table
 *
 * @param nit	a struct dvb_table_nit pointer
 * @param tran	a struct dvb_table_nit_transport pointer
 * @param desc	a struct dvb_desc pointer
 * @param priv	an opaque optional pointer
 */
typedef void nit_tran_handler_callback_t(struct dvb_table_nit *nit,
					 struct dvb_table_nit_transport *tran,
					 struct dvb_desc *desc,
					 void *priv);

/**
 * @brief Macro used to find a transport inside a NIT table
 * @ingroup dvb_table
 *
 * @param _tran		transport to seek
 * @param _nit		pointer to struct dvb_table_nit_transport
 */
#define dvb_nit_transport_foreach( _tran, _nit ) \
	if (_nit && _nit->transport) \
		for (struct dvb_table_nit_transport *_tran = _nit->transport; _tran; _tran = _tran->next) \

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses NIT table
 * @ingroup dvb_table
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param buf buffer containing the NIT raw data
 * @param buflen length of the buffer
 * @param table pointer to struct dvb_table_nit to be allocated and filled
 *
 * This function allocates a NIT table and fills the fields inside
 * the struct. It also makes sure that all fields will follow the CPU
 * endianness. Due to that, the content of the buffer may change.
 *
 * @return On success, it returns the size of the allocated struct.
 *	   A negative value indicates an error.
 */
ssize_t dvb_table_nit_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			    ssize_t buflen, struct dvb_table_nit **table);

/**
 * @brief Frees all data allocated by the NIT table parser
 * @ingroup dvb_table
 *
 * @param table pointer to struct dvb_table_nit to be freed
 */
void dvb_table_nit_free(struct dvb_table_nit *table);

/**
 * @brief Prints the content of the NIT table
 * @ingroup dvb_table
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param table	pointer to struct dvb_table_nit
 */
void dvb_table_nit_print(struct dvb_v5_fe_parms *parms, struct dvb_table_nit *table);

/**
 * @brief For each entry at NIT and NIT transport tables, call a callback
 * @ingroup dvb_table
 *
 * @param parms		struct dvb_v5_fe_parms pointer to the opened device
 * @param table		pointer to struct dvb_table_nit
 * @param descriptor	indicates the NIT table descriptor to seek
 * @param call_nit	a nit_handler_callback_t function to be called when a
 *			new entry at the NIT table is found (or NULL).
 * @param call_tran	a nit_tran_handler_callback_t function to be called
 *			when a new entry at the NIT transport table is found
 *			(or NULL).
 * @param priv		an opaque pointer to be optionally used by the
 *			callbacks. The function won't touch on it, just use
 *			as an argument for the callback functions.
 *
 * When parsing a NIT entry, we need to call some code to properly handle
 * when a given descriptor in the table is found. This is used, for example,
 * to create newer transponders to seek during scan.
 *
 * For example, to seek for the CATV delivery system descriptor and call a
 * function that would add a new transponder to a scan procedure:
 * @code
 * 	dvb_table_nit_descriptor_handler(
 *				&parms->p, dvb_scan_handler->nit,
 *				cable_delivery_system_descriptor,
 *				NULL, add_update_nit_dvbc, &tr);
 * @endcode
 */
void dvb_table_nit_descriptor_handler(
			    struct dvb_v5_fe_parms *parms,
			    struct dvb_table_nit *table,
			    enum descriptors descriptor,
			    nit_handler_callback_t *call_nit,
			    nit_tran_handler_callback_t *call_tran,
			    void *priv);

#ifdef __cplusplus
}
#endif

#endif
