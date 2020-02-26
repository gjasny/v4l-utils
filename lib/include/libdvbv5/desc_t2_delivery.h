/*
 * Copyright (c) 2013-2014 - Mauro Carvalho Chehab <mchehab@kernel.org>
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
 * Based on ETSI EN 300 468 V1.11.1 (2010-04)
 */

/**
 * @file desc_t2_delivery.h
 * @ingroup descriptors
 * @brief Provides the descriptors for the DVB-T2 delivery system descriptor
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Mauro Carvalho Chehab
 *
 * @par Relevant specs
 * The descriptor described herein is defined at:
 * - ETSI EN 300 468 V1.11.1
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#ifndef _T2_DELIVERY_H
#define _T2_DELIVERY_H

#include <libdvbv5/descriptors.h>

/**
 * @struct dvb_desc_t2_delivery_subcell_old
 * @ingroup descriptors
 * @brief Structure to describe transponder subcell extension and frequencies
 *
 * @param cell_id_extension	cell id extension
 * @param transposer_frequency	transposer frequency
 *
 * NOTE: This struct is deprecated and will never be filled.
 *       It is kept here just to avoid breaking ABI.
 *
 * All subcell transposer frequencies will be added to
 * dvb_desc_t2_delivery::centre_frequency array.
 */
struct dvb_desc_t2_delivery_subcell_old {
	uint8_t cell_id_extension;
	uint16_t transposer_frequency;		// Should be 32 bits, instead
} __attribute__((packed));


/**
 * @struct dvb_desc_t2_delivery_subcell
 * @ingroup descriptors
 * @brief Structure to describe transponder subcell extension and frequencies
 *
 * @param cell_id_extension	cell id extension
 * @param transposer_frequency	pointer to transposer frequency
 */
struct dvb_desc_t2_delivery_subcell {
	uint8_t cell_id_extension;
	uint32_t transposer_frequency;
} __attribute__((packed));


/**
 * @struct dvb_desc_t2_delivery_cell
 * @ingroup descriptors
 * @brief Structure to describe transponder cells
 *
 * @param cell_id		cell id extension
 * @param num_freqs		number of cell frequencies
 * @param centre_frequency	pointer to centre frequencies
 * @param subcel_length		number of subcells. May be zero
 * @param subcell		pointer to subcell array. May be NULL
 */
struct dvb_desc_t2_delivery_cell {
	uint16_t cell_id;
	int num_freqs;
	uint32_t *centre_frequency;
	uint8_t subcel_length;
	struct dvb_desc_t2_delivery_subcell *subcel;
} __attribute__((packed));

/**
 * @struct dvb_desc_t2_delivery
 * @ingroup descriptors
 * @brief Structure containing the T2 delivery system descriptor
 *
 * @param plp_id		data PLP id
 * @param system_id		T2 system id
 * @param SISO_MISO		SISO MISO
 * @param bandwidth		bandwidth
 * @param guard_interval	guard interval
 * @param transmission_mode	transmission mode
 * @param other_frequency_flag	other frequency flag
 * @param tfs_flag		tfs flag
 *
 * @param centre_frequency	centre frequency vector. It contains the full
 *				frequencies for all cells and subcells.
 * @param frequency_loop_length	size of the dvb_desc_t2_delivery::centre_frequency
 *				vector
 *
 * @param subcel_info_loop_length unused. Always 0
 * @param subcell		unused. Always NULL
 * @param num_cell		number of cells
 * @param cell			cell array. Contains per-cell and per-subcell
 *				pointers to the frequencies parsed.
 */
struct dvb_desc_t2_delivery {
	/* extended descriptor */

	uint8_t plp_id;
	uint16_t system_id;
	union {
		uint16_t bitfield;
		struct {
			uint16_t tfs_flag:1;
			uint16_t other_frequency_flag:1;
			uint16_t transmission_mode:3;
			uint16_t guard_interval:3;
			uint16_t reserved:2;
			uint16_t bandwidth:4;
			uint16_t SISO_MISO:2;
		} __attribute__((packed));
	} __attribute__((packed));

	uint32_t *centre_frequency;
	uint8_t frequency_loop_length;

	/* Unused, as the definitions here are incomplete. */
	uint8_t subcel_info_loop_length;
	struct dvb_desc_t2_delivery_subcell_old *subcell;

	/* Since version 1.13 */
	unsigned int num_cell;
	struct dvb_desc_t2_delivery_cell *cell;

} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses the T2 delivery system descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param buf	buffer containing the descriptor's raw data
 * @param ext	struct dvb_extension_descriptor pointer
 * @param desc	pointer to struct dvb_desc to be allocated and filled
 *
 * This function allocates a the descriptor and fills the fields inside
 * the struct. It also makes sure that all fields will follow the CPU
 * endianness. Due to that, the content of the buffer may change.
 *
 * @return On success, it returns the size of the allocated struct.
 *	   A negative value indicates an error.
 */
int dvb_desc_t2_delivery_init(struct dvb_v5_fe_parms *parms,
			       const uint8_t *buf,
			       struct dvb_extension_descriptor *ext,
			       void *desc);

/**
 * @brief Prints the content of the T2 delivery system descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param ext	struct dvb_extension_descriptor pointer
 * @param desc	pointer to struct dvb_desc
 */
void dvb_desc_t2_delivery_print(struct dvb_v5_fe_parms *parms,
				const struct dvb_extension_descriptor *ext,
				const void *desc);

/**
 * @brief Frees all data allocated by the T2 delivery system descriptor
 * @ingroup descriptors
 *
 * @param desc pointer to struct dvb_desc to be freed
 */
void dvb_desc_t2_delivery_free(const void *desc);

/**
 * @brief converts from internal representation into bandwidth in Hz
 */
extern const unsigned dvbt2_bw[];

/**
 * @brief converts from internal representation into enum fe_guard_interval,
 * as defined at DVBv5 API.
 */
extern const uint32_t dvbt2_interval[];

/**
 * @brief converts from the descriptor's transmission mode into
 *	  enum fe_transmit_mode, as defined by DVBv5 API.
 */
extern const unsigned dvbt2_transmission_mode[];

/**
 * @brief converts from internal representation to string the SISO_MISO
 *	  field of dvb_desc_t2_delivery:SISO_MISO field.
 */
extern const char *siso_miso[4];

#ifdef __cplusplus
}
#endif

#endif
