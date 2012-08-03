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

void dvb_table_nit_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, uint8_t *table, ssize_t *table_length)
{
	const uint8_t *p = buf;
	struct dvb_table_nit *nit = (struct dvb_table_nit *) table;
	struct dvb_desc **head_desc;
	struct dvb_table_nit_transport **head;
	int desc_length;

	if (*table_length > 0) {
		/* find end of curent lists */
		head_desc = &nit->descriptor;
		while (*head_desc != NULL)
			head_desc = &(*head_desc)->next;
		head = &nit->transport;
		while (*head != NULL)
			head = &(*head)->next;

		struct dvb_table_nit *t = (struct dvb_table_nit *) buf;
		bswap16(t->bitfield);
		desc_length = t->desc_length;

	} else {
		memcpy(table, p, sizeof(struct dvb_table_nit) - sizeof(nit->descriptor) - sizeof(nit->transport));
		*table_length = sizeof(struct dvb_table_nit);

		bswap16(nit->bitfield);
		nit->descriptor = NULL;
		nit->transport = NULL;
		head_desc = &nit->descriptor;
		head = &nit->transport;
		desc_length = nit->desc_length;
	}
	p += sizeof(struct dvb_table_nit) - sizeof(nit->descriptor) - sizeof(nit->transport);

	dvb_parse_descriptors(parms, p, desc_length, head_desc);
	p += desc_length;

	p += sizeof(union dvb_table_nit_transport_header);

	struct dvb_table_nit_transport *last = NULL;
	while ((uint8_t *) p < buf + buflen - 4) {
		struct dvb_table_nit_transport *transport = (struct dvb_table_nit_transport *) malloc(sizeof(struct dvb_table_nit_transport));
		memcpy(transport, p, sizeof(struct dvb_table_nit_transport) - sizeof(transport->descriptor) - sizeof(transport->next));
		p += sizeof(struct dvb_table_nit_transport) - sizeof(transport->descriptor) - sizeof(transport->next);

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
		dvb_parse_descriptors(parms, p, transport->section_length, head_desc);

		p += transport->section_length;
		last = transport;
	}
}

void dvb_table_nit_free(struct dvb_table_nit *nit)
{
	struct dvb_table_nit_transport *transport = nit->transport;
	dvb_free_descriptors((struct dvb_desc **) &nit->descriptor);
	while(transport) {
		dvb_free_descriptors((struct dvb_desc **) &transport->descriptor);
		struct dvb_table_nit_transport *tmp = transport;
		transport = transport->next;
		free(tmp);
	}
	free(nit);
}

void dvb_table_nit_print(struct dvb_v5_fe_parms *parms, struct dvb_table_nit *nit)
{
	dvb_log("NIT");
	dvb_table_header_print(parms, &nit->header);
	dvb_log("| desc_length   %d", nit->desc_length);
	dvb_print_descriptors(parms, nit->descriptor);
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

