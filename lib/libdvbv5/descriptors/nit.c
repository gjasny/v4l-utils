/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab <mchehab@redhat.com>
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

#include "descriptors/nit.h"
#include "dvb-fe.h"

void *dvb_table_nit_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t size)
{
	uint8_t *d = malloc(DVB_MAX_PAYLOAD_PACKET_SIZE * 2);
	const uint8_t *p = buf;
	struct dvb_table_nit *nit = (struct dvb_table_nit *) d;

	memcpy(nit, p, sizeof(struct dvb_table_nit) - sizeof(nit->descriptor) - sizeof(nit->transport));
	p += sizeof(struct dvb_table_nit) - sizeof(nit->descriptor) - sizeof(nit->transport);
	d += sizeof(struct dvb_table_nit);

	dvb_table_header_init(&nit->header);
	bswap16(nit->bitfield);
	nit->descriptor = NULL;
	nit->transport = NULL;

	d += dvb_parse_descriptors(parms, p, d, nit->desc_length, &nit->descriptor);
	p += nit->desc_length;

	p += sizeof(union dvb_table_nit_transport_header);

	struct dvb_table_nit_transport *last = NULL;
	struct dvb_table_nit_transport **head = &nit->transport;
	while ((uint8_t *) p < buf + size - 4) {
		struct dvb_table_nit_transport *transport = (struct dvb_table_nit_transport *) d;
		memcpy(d, p, sizeof(struct dvb_table_nit_transport) - sizeof(transport->descriptor) - sizeof(transport->next));
		p += sizeof(struct dvb_table_nit_transport) - sizeof(transport->descriptor) - sizeof(transport->next);
		d += sizeof(struct dvb_table_nit_transport);

		bswap16(transport->transport_id);
		bswap16(transport->network_id);
		bswap16(transport->bitfield);
		transport->descriptor = NULL;
		transport->next = NULL;

		if(!*head)
			*head = transport;
		if(last)
			last->next = transport;

		/* get the descriptors for each transport */
		struct dvb_desc **head_desc = &transport->descriptor;
		d += dvb_parse_descriptors(parms, p, d, transport->section_length, head_desc);

		p += transport->section_length;
		last = transport;
	}
	return nit;
}

void dvb_table_nit_print(struct dvb_v5_fe_parms *parms, struct dvb_table_nit *nit)
{
	dvb_log("NIT");
	dvb_table_header_print(parms, &nit->header);
	dvb_log("| desc_length   %d", nit->desc_length);
	struct dvb_desc *desc = nit->descriptor;
	while (desc) {
		if (dvb_descriptors[desc->type].print)
			dvb_descriptors[desc->type].print(parms, desc);
		desc = desc->next;
	}
	const struct dvb_table_nit_transport *transport = nit->transport;
	uint16_t transports = 0;
	while(transport) {
		dvb_log("|- Transport: %-7d Network: %-7d", transport->transport_id, transport->network_id);
		desc = transport->descriptor;
		while (desc) {
			if (dvb_descriptors[desc->type].print)
				dvb_descriptors[desc->type].print(parms, desc);
			desc = desc->next;
		}
		transport = transport->next;
		transports++;
	}
	dvb_log("|_  %d transports", transports);
}

