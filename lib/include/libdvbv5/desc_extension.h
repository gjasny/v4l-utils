/*
 * Copyright (c) 2013 - Mauro Carvalho Chehab <m.chehab@samsung.com>
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

#ifndef _EXTENSION_DESC_H
#define _EXTENSION_DESC_H

#include <libdvbv5/descriptors.h>

struct dvb_v5_fe_parms;

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

struct dvb_extension_descriptor {
	DVB_DESC_HEADER();

	uint8_t extension_code;

	struct dvb_desc *descriptor;
} __attribute__((packed));


typedef int  (*dvb_desc_ext_init_func) (struct dvb_v5_fe_parms *parms,
					const uint8_t *buf,
					struct dvb_extension_descriptor *ext,
					void *desc);
typedef void (*dvb_desc_ext_print_func)(struct dvb_v5_fe_parms *parms,
					const struct dvb_extension_descriptor *ext,
					const void *desc);
typedef void (*dvb_desc_ext_free_func) (const void *desc);

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

int dvb_extension_descriptor_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc);
void dvb_extension_descriptor_free(struct dvb_desc *descriptor);
void dvb_extension_descriptor_print(struct dvb_v5_fe_parms *parms,
				    const struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

#endif
