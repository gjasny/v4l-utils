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
 */

/**
 * @file desc_extension.h
 * @ingroup descriptors
 * @brief Provides the descriptors for the extension descriptor.
 * 	  The extension descriptor is used to extend the 8-bit namespace
 *	  of the descriptor_tag field.
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Mauro Carvalho Chehab
 *
 * @par Relevant specs
 * The descriptor described herein is defined at:
 *
 * @see
 * - ETSI EN 300 468 V1.11.1
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#ifndef _EXTENSION_DESC_H
#define _EXTENSION_DESC_H

#include <libdvbv5/descriptors.h>

struct dvb_v5_fe_parms;

/**
 * @enum extension_descriptors
 * @brief List containing all extended descriptors used by Digital TV MPEG-TS
 *	  as defined at ETSI EN 300 468 V1.11.1
 * @ingroup descriptors
 *
 * @var image_icon_descriptor
 *	@brief image icon descriptor
 *
 * @var cpcm_delivery_signalling_descriptor
 *	@brief Content Protection/Copy Management (CPCM) delivery signalling
 *	       descriptor
 *
 * @var CP_descriptor
 *	@brief Content Protection descriptor
 *
 * @var CP_identifier_descriptor
 *	@brief Content Protection identifier descriptor
 *
 * @var T2_delivery_system_descriptor
 *	@brief DVB-T2 delivery system descriptor
 *
 * @var SH_delivery_system_descriptor
 *	@brief DVB-SH delivery system descriptor
 *
 * @var supplementary_audio_descriptor
 *	@brief supplementary audio descriptor
 *
 * @var network_change_notify_descriptor
 *	@brief network change notify descriptor
 *
 * @var message_descriptor
 *	@brief message descriptor
 *
 * @var target_region_descriptor
 *	@brief target region descriptor
 *
 * @var target_region_name_descriptor
 *	@brief target region name descriptor
 *
 * @var service_relocated_descriptor
 *	@brief service relocated descriptor
 */
enum extension_descriptors {
	image_icon_descriptor				= 0x00,
	cpcm_delivery_signalling_descriptor		= 0x01,
	CP_descriptor					= 0x02,
	CP_identifier_descriptor			= 0x03,
	T2_delivery_system_descriptor			= 0x04,
	SH_delivery_system_descriptor			= 0x05,
	supplementary_audio_descriptor			= 0x06,
	network_change_notify_descriptor		= 0x07,
	message_descriptor				= 0x08,
	target_region_descriptor			= 0x09,
	target_region_name_descriptor			= 0x0a,
	service_relocated_descriptor			= 0x0b,
};

/**
 * @struct dvb_extension_descriptor
 * @ingroup descriptors
 * @brief Structure containing the extended descriptors
 *
 * @param type			Descriptor type
 * @param length		Length of the descriptor
 * @param next			pointer to the dvb_desc descriptor
 * @param extension_code	extension code
 * @param descriptor		pointer to struct dvb_desc
 */
struct dvb_extension_descriptor {
	uint8_t type;
	uint8_t length;
	struct dvb_desc *next;

	uint8_t extension_code;

	struct dvb_desc *descriptor;
} __attribute__((packed));


/**
 * @brief Function prototype for the extended descriptors parsing init code
 * @ingroup dvb_table
 *
 * @param parms		Struct dvb_v5_fe_parms pointer
 * @param buf		buffer with the content of the descriptor
 * @param ext		struct dvb_extension_descriptor pointer
 * @param desc		struct dvb_desc pointer
 */
typedef int  (*dvb_desc_ext_init_func) (struct dvb_v5_fe_parms *parms,
					const uint8_t *buf,
					struct dvb_extension_descriptor *ext,
					void *desc);
/**
 * @brief Function prototype for the extended descriptors parsing print code
 * @ingroup dvb_table
 *
 * @param parms		Struct dvb_v5_fe_parms pointer
 * @param ext		struct dvb_extension_descriptor pointer
 * @param desc		struct dvb_desc pointer
 */
typedef void (*dvb_desc_ext_print_func)(struct dvb_v5_fe_parms *parms,
					const struct dvb_extension_descriptor *ext,
					const void *desc);
/**
 * @brief Function prototype for the extended descriptors parsing free code
 * @ingroup dvb_table
 *
 * @param desc		struct dvb_desc pointer
 */
typedef void (*dvb_desc_ext_free_func) (const void *desc);

/**
 * @struct dvb_ext_descriptor
 * @ingroup descriptors
 * @brief Structure that describes the parser functions for the extended
 *	  descriptors. Works on a similar way as struct dvb_descriptor.
 *
 * @param name	name of the descriptor
 * @param init	init dvb_desc_ext_init_func pointer
 * @param print	print dvb_desc_ext_print_func pointer
 * @param free	free dvb_desc_ext_free_func pointer
 * @param size	size of the descriptor
 */
struct dvb_ext_descriptor {
	const char *name;
	dvb_desc_ext_init_func init;
	dvb_desc_ext_print_func print;
	dvb_desc_ext_free_func free;
	ssize_t size;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and parses the extended descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param buf	buffer containing the descriptor's raw data
 * @param desc	pointer to struct dvb_desc to be allocated and filled
 *
 * This function allocates a the descriptor and fills the fields inside
 * the struct. It also makes sure that all fields will follow the CPU
 * endianness. Due to that, the content of the buffer may change.
 *
 * @return On success, it returns the size of the allocated struct.
 *	   A negative value indicates an error.
 */
int dvb_extension_descriptor_init(struct dvb_v5_fe_parms *parms,
				  const uint8_t *buf, struct dvb_desc *desc);

/**
 * @brief Prints the content of the extended descriptor
 * @ingroup descriptors
 *
 * @param parms	struct dvb_v5_fe_parms pointer to the opened device
 * @param desc	pointer to struct dvb_desc
 */
void dvb_extension_descriptor_print(struct dvb_v5_fe_parms *parms,
				    const struct dvb_desc *desc);

/**
 * @brief Frees all data allocated by the extended descriptor
 * @ingroup descriptors
 *
 * @param desc pointer to struct dvb_desc to be freed
 */
void dvb_extension_descriptor_free(struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
