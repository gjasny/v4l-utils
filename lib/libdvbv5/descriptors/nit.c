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
	const uint8_t *p = buf, *endbuf = buf + buflen - 4;
	struct dvb_table_nit *nit = (struct dvb_table_nit *) table;
	struct dvb_desc **head_desc;
	struct dvb_table_nit_transport **head;
	struct dvb_table_nit_transport *last = NULL;
	size_t size;

	if (*table_length > 0) {
		struct dvb_table_nit *t;

		/* find end of current lists */
		head_desc = &nit->descriptor;
		while (*head_desc != NULL)
			head_desc = &(*head_desc)->next;
		head = &nit->transport;
		while (*head != NULL)
			head = &(*head)->next;

		size = offsetof(struct dvb_table_nit, descriptor);
		if (p + size > endbuf) {
			dvb_logerr("NIT table (cont) was truncated");
			return;
		}
		p += size;
		t = (struct dvb_table_nit *)buf;

		bswap16(t->bitfield);
		size = t->desc_length;
	} else {
		size = offsetof(struct dvb_table_nit, descriptor);
		if (p + size > endbuf) {
			dvb_logerr("NIT table was truncated while filling dvb_table_nit. Need %zu bytes, but has only %zu.",
				   size, buflen);
			return;
		}
		memcpy(table, p, size);
		p += size;

		*table_length = sizeof(struct dvb_table_nit);

		nit->descriptor = NULL;
		nit->transport = NULL;
		head_desc = &nit->descriptor;
		head = &nit->transport;

		bswap16(nit->bitfield);
		size = nit->desc_length;
	}
	if (p + size > endbuf) {
		dvb_logerr("NIT table was truncated while getting NIT descriptors. Need %zu bytes, but has only %zu.",
			   size, endbuf - p);
		return;
	}
	dvb_parse_descriptors(parms, p, size, head_desc);
	p += size;

	size = sizeof(union dvb_table_nit_transport_header);
	if (p + size > endbuf) {
		dvb_logerr("NIT table was truncated while getting NIT transports. Need %zu bytes, but has only %zu.",
			   size, endbuf - p);
		return;
	}
	p += size;

	size = offsetof(struct dvb_table_nit_transport, descriptor);
	while (p + size <= endbuf) {
		struct dvb_table_nit_transport *transport = malloc(sizeof(struct dvb_table_nit_transport));
		if (!transport)
			dvb_perror("Out of memory");
		memcpy(transport, p, size);
		p += size;

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
		head_desc = &transport->descriptor;

		if (p + transport->section_length > endbuf) {
			dvb_logerr("NIT table was truncated while getting NIT transport descriptors. Need %u bytes, but has only %zu.",
				transport->section_length, endbuf - p);
			return;
		}
		dvb_parse_descriptors(parms, p, transport->section_length, head_desc);
		p += transport->section_length;

		last = transport;
	}
	if (endbuf - p)
		dvb_logerr("NIT table has %zu spurious bytes at the end.",
			   endbuf - p);
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

void nit_descriptor_handler(struct dvb_v5_fe_parms *parms,
			    struct dvb_table_nit *nit,
			    enum descriptors descriptor,
			    nit_handler_callback_t *call_nit,
			    nit_tran_handler_callback_t *call_tran,
			    void *priv)
{
	if (call_nit || parms->verbose) {
		dvb_desc_find(struct dvb_desc, desc, nit,
			      descriptor) {
			if (call_nit)
				call_nit(nit, desc, priv);
			else
				dvb_logwarn("descriptor %s found on NIT but unhandled",
					   dvb_descriptors[descriptor].name);

		}
	}
	if (!call_tran && !parms->verbose)
		return;

	dvb_nit_transport_foreach(tran, nit) {
		dvb_desc_find(struct dvb_desc, desc, tran,
			      descriptor) {
			if (call_tran)
				call_tran(nit, tran, desc, priv);
			else
				dvb_logwarn("descriptor %s found on NIT transport but unhandled",
					   dvb_descriptors[descriptor].name);
		}
	}
}
