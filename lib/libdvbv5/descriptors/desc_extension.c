/*
 * Copyright (c) 2013 - Mauro Carvalho Chehab <mchehab@kernel.org>
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

#include <libdvbv5/desc_extension.h>
#include <libdvbv5/desc_t2_delivery.h>
#include <libdvbv5/dvb-fe.h>

#if __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif

const struct dvb_ext_descriptor dvb_ext_descriptors[] = {
	[0 ...255 ] = {
		.name  = "Unknown descriptor",
		.init  = NULL,
		.print = NULL,
		.free  = NULL,
		.size  = 0,
	},
	[image_icon_descriptor] = {
		.name  = "image_icon_descriptor",
		.init  = NULL,
		.print = NULL,
		.free  = NULL,
		.size  = 0,
	},
	[cpcm_delivery_signalling_descriptor] = {
		.name  = "cpcm_delivery_signalling_descriptor",
		.init  = NULL,
		.print = NULL,
		.free  = NULL,
		.size  = 0,
	},
	[CP_descriptor] = {
		.name  = "CP_descriptor",
		.init  = NULL,
		.print = NULL,
		.free  = NULL,
		.size  = 0,
	},
	[CP_identifier_descriptor] = {
		.name  = "CP_identifier_descriptor",
		.init  = NULL,
		.print = NULL,
		.free  = NULL,
		.size  = 0,
	},
	[T2_delivery_system_descriptor] = {
		.name  = "T2_delivery_system_descriptor",
		.init  = dvb_desc_t2_delivery_init,
		.print = dvb_desc_t2_delivery_print,
		.free  = dvb_desc_t2_delivery_free,
		.size  = sizeof(struct dvb_desc_t2_delivery),
	},
	[SH_delivery_system_descriptor] = {
		.name  = "SH_delivery_system_descriptor",
		.init  = NULL,
		.print = NULL,
		.free  = NULL,
		.size  = 0,
	},
	[supplementary_audio_descriptor] = {
		.name  = "supplementary_audio_descriptor",
		.init  = NULL,
		.print = NULL,
		.free  = NULL,
		.size  = 0,
	},
	[network_change_notify_descriptor] = {
		.name  = "network_change_notify_descriptor",
		.init  = NULL,
		.print = NULL,
		.free  = NULL,
		.size  = 0,
	},
	[message_descriptor] = {
		.name  = "message_descriptor",
		.init  = NULL,
		.print = NULL,
		.free  = NULL,
		.size  = 0,
	},
	[target_region_descriptor] = {
		.name  = "target_region_descriptor",
		.init  = NULL,
		.print = NULL,
		.free  = NULL,
		.size  = 0,
	},
	[target_region_name_descriptor] = {
		.name  = "target_region_name_descriptor",
		.init  = NULL,
		.print = NULL,
		.free  = NULL,
		.size  = 0,
	},
	[service_relocated_descriptor] = {
		.name  = "service_relocated_descriptor",
		.init  = NULL,
		.print = NULL,
		.free  = NULL,
		.size  = 0,
	},
};

int dvb_extension_descriptor_init(struct dvb_v5_fe_parms *parms,
				  const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_extension_descriptor *ext = (void *)desc;
	unsigned char *p = (unsigned char *)buf;
	unsigned desc_type = *p;
	size_t size = 0;
	int desc_len  = ext->length - 1;
	dvb_desc_ext_init_func init;

	ext->extension_code = desc_type;
	p++;

	switch (parms->verbose) {
	case 0:
	case 1:
		break;
	case 2:
		if (dvb_ext_descriptors[desc_type].init)
			break;
		/* fall through */
	case 3:
		dvb_logwarn("%sextension descriptor %s type 0x%02x, size %d",
			dvb_ext_descriptors[desc_type].init ? "" : "Not handled ",
			dvb_ext_descriptors[desc_type].name, desc_type, desc_len);
		dvb_hexdump(parms, "content: ", p, desc_len);
	}

	init = dvb_ext_descriptors[desc_type].init;
	if (init)
		size = dvb_ext_descriptors[desc_type].size;
	if (!size)
		size = desc_len;

	ext->descriptor = calloc(1, size);

	if (init) {
		if (init(parms, p, ext, ext->descriptor) != 0)
			return -1;
	}
	else
		memcpy(ext->descriptor, p, size);
	return 0;
}

void dvb_extension_descriptor_free(struct dvb_desc *descriptor)
{
	struct dvb_extension_descriptor *ext = (void *)descriptor;
	uint8_t type = ext->extension_code;

	if (!ext->descriptor)
		return;

	if (dvb_ext_descriptors[type].free)
		dvb_ext_descriptors[type].free(ext->descriptor);

	free(ext->descriptor);
}

void dvb_extension_descriptor_print(struct dvb_v5_fe_parms *parms,
				const struct dvb_desc *desc)
{
	struct dvb_extension_descriptor *ext = (void *)desc;
	uint8_t type = ext->extension_code;
	dvb_loginfo("|           descriptor %s type 0x%02x",
		dvb_ext_descriptors[type].name, type);

	if (dvb_ext_descriptors[type].print)
		dvb_ext_descriptors[type].print(parms, ext, ext->descriptor);
}
