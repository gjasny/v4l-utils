/*
 * Copyright (c) 2013 - Andre Roth <neolynx@gmail.com>
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
 * Described at ETSI EN 300 468 V1.11.1 (2010-04)
 */

#include <libdvbv5/desc_ca_identifier.h>
#include <libdvbv5/dvb-fe.h>

int dvb_desc_ca_identifier_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_ca_identifier *d = (struct dvb_desc_ca_identifier *) desc;
	int i;

	d->caid_count = d->length >> 1; /* FIXME: warn if odd */
	d->caids = malloc(d->length);
	if (!d->caids) {
		dvb_logerr("dvb_desc_ca_identifier_init: out of memory");
		return -1;
	}
	for (i = 0; i < d->caid_count; i++) {
		d->caids[i] = ((uint16_t *) buf)[i];
		bswap16(d->caids[i]);
	}
	return 0;
}

void dvb_desc_ca_identifier_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_ca_identifier *d = (const struct dvb_desc_ca_identifier *) desc;
	int i;

	for (i = 0; i < d->caid_count; i++)
		dvb_loginfo("|           caid %d            0x%04x", i, d->caids[i]);
}

void dvb_desc_ca_identifier_free(struct dvb_desc *desc)
{
	struct dvb_desc_ca_identifier *d = (struct dvb_desc_ca_identifier *) desc;
	if (d->caids)
		free(d->caids);
}

