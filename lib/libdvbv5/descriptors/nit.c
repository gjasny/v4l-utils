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

void dvb_table_nit_init(struct dvb_v5_fe_parms *parms, const uint8_t *ptr, ssize_t size, uint8_t **buf, ssize_t *buflen)
{
	uint8_t *d;
	const uint8_t *p = ptr;
	struct dvb_table_nit *nit = (struct dvb_table_nit *) ptr;
	struct dvb_desc **head_desc;
	struct dvb_table_nit_transport **head;

	bswap16(nit->bitfield);

	if (!*buf) {
		d = malloc(DVB_MAX_PAYLOAD_PACKET_SIZE * 4);
		*buf = d;
		*buflen = 0;
		nit = (struct dvb_table_nit *) d;

		memcpy(d + *buflen, p, sizeof(struct dvb_table_nit) - sizeof(nit->descriptor) - sizeof(nit->transport));
		*buflen += sizeof(struct dvb_table_nit);

		nit->descriptor = NULL;
		nit->transport = NULL;
		head_desc = &nit->descriptor;
		head = &nit->transport;
	} else {
		// should realloc d
		d = *buf;

		// find end of curent list
		nit = (struct dvb_table_nit *) d;
		head_desc = &nit->descriptor;
		while (*head_desc != NULL)
			head_desc = &(*head_desc)->next;
		head = &nit->transport;
		while (*head != NULL)
			head = &(*head)->next;
		// read new table
		nit = (struct dvb_table_nit *) p; // FIXME: should be copied to tmp, cause bswap in const
	}
	p += sizeof(struct dvb_table_nit) - sizeof(nit->descriptor) - sizeof(nit->transport);

	*buflen += dvb_parse_descriptors(parms, p, d + *buflen, nit->desc_length, head_desc);
	p += nit->desc_length;

	p += sizeof(union dvb_table_nit_transport_header);

	struct dvb_table_nit_transport *last = NULL;
	while ((uint8_t *) p < ptr + size - 4) {
		struct dvb_table_nit_transport *transport = (struct dvb_table_nit_transport *) (d + *buflen);
		memcpy(d + *buflen, p, sizeof(struct dvb_table_nit_transport) - sizeof(transport->descriptor) - sizeof(transport->next));
		p += sizeof(struct dvb_table_nit_transport) - sizeof(transport->descriptor) - sizeof(transport->next);
		*buflen += sizeof(struct dvb_table_nit_transport);

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
		*buflen += dvb_parse_descriptors(parms, p, d + *buflen, transport->section_length, head_desc);

		p += transport->section_length;
		last = transport;
	}
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
		dvb_print_descriptors(parms, transport->descriptor);
		transport = transport->next;
		transports++;
	}
	dvb_log("|_  %d transports", transports);
}

