/*
 * Copyright (c) 2011-2014 - Mauro Carvalho Chehab
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

/**
 * @file desc_sat.h
 * @ingroup descriptors
 * @brief Provides the descriptors for the satellite delivery system descriptor
 * @copyright GNU General Public License version 2 (GPLv2)
 * @author Mauro Carvalho Chehab
 * @author Andre Roth
 *
 * @par Relevant specs
 * The descriptor described herein is defined at:
 * - ETSI EN 300 468 V1.11.1
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#ifndef _SAT_H
#define _SAT_H

#include <libdvbv5/descriptors.h>

/**
 * @struct dvb_desc_sat
 * @ingroup descriptors
 * @brief Structure containing the satellite delivery system descriptor
 *
 * @param type			descriptor tag
 * @param length		descriptor length
 * @param next			pointer to struct dvb_desc
 * @param frequency		frequency in kHz
 * @param orbit			orbital position in degrees (multiplied by 10)
 * @param west_east		west east flag. 0 = west, 1 = east
 * @param polarization		polarization. 0 = horizontal, 1 = vertical,
 *				2 = left, 3 = right.
 * @param roll_off		roll off alpha factor. 0 = 0.35, 1 = 0.25,
 * 				2 = 0.20, 3 = reserved.
 * @param modulation_system	modulation system. 0 = DVB-S, 1 = DVB-S2.
 * @param modulation_type	modulation type. 0 = auto, 1 = QPSK, 2 = 8PSK,
 *				3 = 16-QAM (only for DVB-S2).
 * @param symbol_rate		symbol rate in Kbauds.
 * @param fec			inner FEC (convolutional code)
 */
struct dvb_desc_sat {
	uint8_t type;
	uint8_t length;
	struct dvb_desc *next;

	uint32_t frequency;
	uint16_t orbit;
	uint8_t modulation_type:2;
	uint8_t modulation_system:1;
	uint8_t roll_off:2;
	uint8_t polarization:2;
	uint8_t west_east:1;
	union {
		uint32_t bitfield;
		struct {
			uint32_t fec:4;
			uint32_t symbol_rate:28;
		} __attribute__((packed));
	} __attribute__((packed));
} __attribute__((packed));

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses the satellite delivery system descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param buf	buffer containing the descriptor's raw data
 * @param desc	pointer to struct dvb_desc to be allocated and filled
 *
 * This function initializes and makes sure that all fields will follow the CPU
 * endianness. Due to that, the content of the buffer may change.
 *
 * Currently, no memory is allocated internally.
 *
 * @return On success, it returns the size of the allocated struct.
 *	   A negative value indicates an error.
 */
int dvb_desc_sat_init(struct dvb_v5_fe_parms *parms,
		      const uint8_t *buf, struct dvb_desc *desc);

/**
 * @brief Prints the content of the satellite delivery system descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param desc	pointer to struct dvb_desc
 */
void dvb_desc_sat_print(struct dvb_v5_fe_parms *parms,
			const struct dvb_desc *desc);

/**
 * @brief converts from the descriptor's FEC into enum fe_code_rate,
 *	  as defined by DVBv5 API.
 */
extern const unsigned dvbs_dvbc_dvbs_freq_inner[];

/**
 * @brief converts from the descriptor's polarization into
 *	  enum dvb_sat_polarization, as defined at dvb-v5-std.h.
 */
extern const unsigned dvbs_polarization[];

/**
 * @brief converts from the descriptor's rolloff into  enum fe_rolloff,
 *	  as defined by DVBv5 API.
 */
extern const unsigned dvbs_rolloff[];

/**
 * @brief converts from the descriptor's modulation into enum fe_modulation,
 *	  as defined by DVBv5 API.
 */
extern const unsigned dvbs_modulation[];

#ifdef __cplusplus
}
#endif

#endif
