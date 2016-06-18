/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 * Copyright (c) 2012-2014 - Andre Roth <neolynx@gmail.com>
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

#include <libdvbv5/nit.h>
#include <libdvbv5/dvb-fe.h>

ssize_t dvb_table_nit_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			ssize_t buflen, struct dvb_table_nit **table)
{
	const uint8_t *p = buf, *endbuf = buf + buflen;
	struct dvb_table_nit *nit;
	struct dvb_table_nit_transport **head;
	struct dvb_desc **head_desc;
	size_t size;

	size = offsetof(struct dvb_table_nit, descriptor);
	if (p + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - p, size);
		return -1;
	}

	if (buf[0] != DVB_TABLE_NIT && (buf[0] != DVB_TABLE_NIT2)) {
		dvb_logerr("%s: invalid marker 0x%02x, should be 0x%02x or 0x%02x",
				__func__, buf[0], DVB_TABLE_NIT, DVB_TABLE_NIT2);
		return -2;
	}

	if (!*table) {
		*table = calloc(sizeof(struct dvb_table_nit), 1);
		if (!*table) {
			dvb_logerr("%s: out of memory", __func__);
			return -3;
		}
	}
	nit = *table;
	memcpy(nit, p, size);
	p += size;
	dvb_table_header_init(&nit->header);

	bswap16(nit->bitfield);

	/* find end of current lists */
	head_desc = &nit->descriptor;
	while (*head_desc != NULL)
		head_desc = &(*head_desc)->next;
	head = &nit->transport;
	while (*head != NULL)
		head = &(*head)->next;

	size = nit->desc_length;
	if (p + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - p, size);
		return -4;
	}
	if (dvb_desc_parse(parms, p, size, head_desc) != 0) {
		return -5;
	}
	p += size;

	size = sizeof(union dvb_table_nit_transport_header);
	if (p + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - p, size);
		return -6;
	}
	p += size;

	size = offsetof(struct dvb_table_nit_transport, descriptor);
	while (p + size <= endbuf) {
		struct dvb_table_nit_transport *transport;

		transport = malloc(sizeof(struct dvb_table_nit_transport));
		if (!transport) {
			dvb_logerr("%s: out of memory", __func__);
			return -7;
		}
		memcpy(transport, p, size);
		p += size;

		bswap16(transport->transport_id);
		bswap16(transport->network_id);
		bswap16(transport->bitfield);
		transport->descriptor = NULL;
		transport->next = NULL;

		*head = transport;
		head = &(*head)->next;

		/* parse the descriptors */
		if (transport->desc_length > 0) {
			uint16_t desc_length = transport->desc_length;
			if (p + desc_length > endbuf) {
				dvb_logwarn("%s: descriptors short read %zd/%d bytes", __func__,
					   endbuf - p, desc_length);
				desc_length = endbuf - p;
			}
			if (dvb_desc_parse(parms, p, desc_length,
					      &transport->descriptor) != 0) {
				return -8;
			}
			p += desc_length;
		}
	}
	if (endbuf - p)
		dvb_logwarn("%s: %zu spurious bytes at the end",
			   __func__, endbuf - p);
	return p - buf;
}

void dvb_table_nit_free(struct dvb_table_nit *nit)
{
	struct dvb_table_nit_transport *transport = nit->transport;
	dvb_desc_free((struct dvb_desc **) &nit->descriptor);
	while (transport) {
		dvb_desc_free((struct dvb_desc **) &transport->descriptor);
		struct dvb_table_nit_transport *tmp = transport;
		transport = transport->next;
		free(tmp);
	}
	free(nit);
}

void dvb_table_nit_print(struct dvb_v5_fe_parms *parms, struct dvb_table_nit *nit)
{
	const struct dvb_table_nit_transport *transport = nit->transport;
	uint16_t transports = 0;

	dvb_loginfo("NIT");
	dvb_table_header_print(parms, &nit->header);
	dvb_loginfo("| desc_length   %d", nit->desc_length);
	dvb_desc_print(parms, nit->descriptor);
	while (transport) {
		dvb_loginfo("|- transport %04x network %04x", transport->transport_id, transport->network_id);
		dvb_desc_print(parms, transport->descriptor);
		transport = transport->next;
		transports++;
	}
	dvb_loginfo("|_  %d transports", transports);
}

void dvb_table_nit_descriptor_handler(
			    struct dvb_v5_fe_parms *parms,
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
