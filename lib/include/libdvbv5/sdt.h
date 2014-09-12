/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 * Copyright (c) 2012 - Andre Roth <neolynx@gmail.com>
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
 *
 */

#ifndef _SDT_H
#define _SDT_H

/**
 * @file sdt.h
 * @ingroup dvb_table
 * @brief Provides the descriptors for SDT MPEG-TS table
 * @copyright GNU General Public License version 2 (GPLv2)
 * @author Mauro Carvalho Chehab
 * @author Andre Roth
 *
 * @par Relevant specs
 * The table described herein is defined at:
 * - ISO/IEC 13818-1
 *
 * @see http://www.etherguidesystems.com/Help/SDOs/dvb/syntax/tablesections/SDT.aspx
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#include <libdvbv5/header.h>

/**
 * @def DVB_TABLE_SDT
 *	@brief SDT table ID
 *	@ingroup dvb_table
 * @def DVB_TABLE_SDT2
 *	@brief SDT table ID (alternative table ID)
 *	@ingroup dvb_table
 * @def DVB_TABLE_SDT_PID
 *	@brief SDT Program ID
 *	@ingroup dvb_table
 */
#define DVB_TABLE_SDT      0x42
#define DVB_TABLE_SDT2     0x46
#define DVB_TABLE_SDT_PID  0x0011

/**
 * @struct dvb_table_sdt_service
 * @brief MPEG-TS SDT service table
 * @ingroup dvb_table
 *
 * @param service_id		service id
 * @param EIT_present_following	EIT present following
 * @param EIT_schedule		EIT schedule
 * @param desc_length		desc length
 * @param free_CA_mode		free CA mode
 * @param running_status	running status
 * @param descriptor		pointer to struct dvb_desc
 * @param next			pointer to struct dvb_table_sdt_service
 *
 * This structure is used to store the original SDT service table,
 * converting the integer fields to the CPU endianness.
 *
 * The undocumented parameters are used only internally by the API and/or
 * are fields that are reserved. They shouldn't be used, as they may change
 * on future API releases.
 *
 * Everything after dvb_table_sdt_service::descriptor (including it) won't
 * be bit-mapped to the data parsed from the MPEG TS. So, metadata are added
 * there.
 */
struct dvb_table_sdt_service {
	uint16_t service_id;
	uint8_t EIT_present_following:1;
	uint8_t EIT_schedule:1;
	uint8_t reserved:6;
	union {
		uint16_t bitfield;
		struct {
			uint16_t desc_length:12;
			uint16_t free_CA_mode:1;
			uint16_t running_status:3;
		} __attribute__((packed));
	} __attribute__((packed));
	struct dvb_desc *descriptor;
	struct dvb_table_sdt_service *next;
} __attribute__((packed));

/**
 * @struct dvb_table_sdt
 * @brief MPEG-TS SDT table
 * @ingroup dvb_table
 *
 * @param header	struct dvb_table_header content
 * @param network_id	network id
 * @param service	pointer to struct dvb_table_sdt_service
 *
 * This structure is used to store the original SDT table,
 * converting the integer fields to the CPU endianness.
 *
 * The undocumented parameters are used only internally by the API and/or
 * are fields that are reserved. They shouldn't be used, as they may change
 * on future API releases.
 *
 * Everything after dvb_table_sdt::service (including it) won't be bit-mapped
 * to the data parsed from the MPEG TS. So, metadata are added there.
 */
struct dvb_table_sdt {
	struct dvb_table_header header;
	uint16_t network_id;
	uint8_t  reserved;
	struct dvb_table_sdt_service *service;
} __attribute__((packed));

/**
 * @brief Macro used to find services on a SDT table
 * @ingroup dvb_table
 *
 * @param _service	service to seek
 * @param _sdt		pointer to struct dvb_table_sdt_service
 */
#define dvb_sdt_service_foreach(_service, _sdt) \
	for (struct dvb_table_sdt_service *_service = _sdt->service; _service; _service = _service->next ) \

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses SDT table
 * @ingroup dvb_table
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param buf buffer containing the SDT raw data
 * @param buflen length of the buffer
 * @param table pointer to struct dvb_table_sdt to be allocated and filled
 *
 * This function allocates a SDT table and fills the fields inside
 * the struct. It also makes sure that all fields will follow the CPU
 * endianness. Due to that, the content of the buffer may change.
 *
 * @return On success, it returns the size of the allocated struct.
 *	   A negative value indicates an error.
 */
ssize_t dvb_table_sdt_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			    ssize_t buflen, struct dvb_table_sdt **table);

/**
 * @brief Frees all data allocated by the SDT table parser
 * @ingroup dvb_table
 *
 * @param table pointer to struct dvb_table_sdt to be freed
 */
void dvb_table_sdt_free(struct dvb_table_sdt *table);

/**
 * @brief Prints the content of the SDT table
 * @ingroup dvb_table
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param table pointer to struct dvb_table_sdt
 */
void dvb_table_sdt_print(struct dvb_v5_fe_parms *parms, struct dvb_table_sdt *table);

#ifdef __cplusplus
}
#endif

#endif
