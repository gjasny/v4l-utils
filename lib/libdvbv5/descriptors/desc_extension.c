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

#include "descriptors.h"
#include "descriptors/desc_extension.h"
#include "dvb-fe.h"

const struct dvb_descriptor dvb_ext_descriptors[] = {
	[image_icon_descriptor] = { "image_icon_descriptor", NULL, NULL, NULL, 0 },
	[cpcm_delivery_signalling_descriptor] = { "cpcm_delivery_signalling_descriptor", NULL, NULL, NULL, 0 },
	[CP_descriptor] = { "CP_descriptor", NULL, NULL, NULL, 0 },
	[CP_identifier_descriptor] = { "CP_identifier_descriptor", NULL, NULL, NULL, 0 },
	[T2_delivery_system_descriptor] = { "T2_delivery_system_descriptor", NULL, NULL, NULL, 0 },
	[SH_delivery_system_descriptor] = { "SH_delivery_system_descriptor", NULL, NULL, NULL, 0 },
	[supplementary_audio_descriptor] = { "supplementary_audio_descriptor", NULL, NULL, NULL, 0 },
	[network_change_notify_descriptor] = { "network_change_notify_descriptor", NULL, NULL, NULL, 0 },
	[message_descriptor] = { "message_descriptor", NULL, NULL, NULL, 0 },
	[target_region_descriptor] = { "target_region_descriptor", NULL, NULL, NULL, 0 },
	[target_region_name_descriptor] = { "target_region_name_descriptor", NULL, NULL, NULL, 0 },
	[service_relocated_descriptor] = { "service_relocated_descriptor", NULL, NULL, NULL, 0 },
};

void dvb_parse_ext_descriptors(struct dvb_v5_fe_parms *parms,
			       const uint8_t *buf, uint16_t section_length,
			       struct dvb_desc **head_desc)
{
	const uint8_t *ptr = buf;
	struct dvb_desc *current = NULL;
	struct dvb_desc *last = NULL;
	while (ptr < buf + section_length) {
		int desc_type = ptr[0];
		int desc_len  = ptr[1];
		size_t size;
		dvb_desc_init_func init = dvb_ext_descriptors[desc_type].init;
		if (!init) {
			init = dvb_desc_default_init;
			size = sizeof(struct dvb_desc) + desc_len;
		} else {
			size = dvb_descriptors[desc_type].size;
		}
		if (!size) {
			dvb_logerr("descriptor type %d has no size defined", current->type);
			size = 4096;
		}
		current = (struct dvb_desc *) malloc(size);
		ptr += dvb_desc_init(ptr, current); /* the standard header was read */
		init(parms, ptr, current);
		if (!*head_desc)
			*head_desc = current;
		if (last)
			last->next = current;
		last = current;
		ptr += current->length;     /* standard descriptor header plus descriptor length */
	}
}

void extension_descriptor_init(struct dvb_v5_fe_parms *parms,
				     const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_extension_descriptor *ext = (struct dvb_extension_descriptor *)desc;
	unsigned char *p = (unsigned char *)buf;
	size_t len;

	len = sizeof(*ext) - offsetof(struct dvb_extension_descriptor, descriptor);
	memcpy(&ext->descriptor, p, len);
	p += len;

	struct dvb_desc **head_desc = &ext->descriptor;
	dvb_parse_ext_descriptors(parms, p, ext->length,
				head_desc);
	p += ext->length;
}
