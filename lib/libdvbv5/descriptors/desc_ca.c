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

#include <libdvbv5/desc_ca.h>
#include <libdvbv5/dvb-fe.h>

int dvb_desc_ca_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	size_t size = offsetof(struct dvb_desc_ca, dvb_desc_ca_field_last) - offsetof(struct dvb_desc_ca, dvb_desc_ca_field_first);
	struct dvb_desc_ca *d = (struct dvb_desc_ca *) desc;

	memcpy(((uint8_t *) d ) + sizeof(struct dvb_desc), buf, size);
	bswap16(d->ca_id);
	bswap16(d->bitfield1);

	if (d->length > size) {
		size = d->length - size;
		d->privdata = malloc(size);
		if (!d->privdata)
			return -1;
		d->privdata_len = size;
		memcpy(d->privdata, buf + 4, size);
	} else {
		d->privdata = NULL;
		d->privdata_len = 0;
	}
	/*dvb_hexdump(parms, "desc ca ", buf, desc->length);*/
	/*dvb_desc_ca_print(parms, desc);*/
	return 0;
}

void dvb_desc_ca_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_ca *d = (const struct dvb_desc_ca *) desc;
	dvb_loginfo("|           ca_id             0x%04x", d->ca_id);
	dvb_loginfo("|           ca_pid            0x%04x", d->ca_pid);
	dvb_loginfo("|           privdata length   %d", d->privdata_len);
	if (d->privdata)
		dvb_hexdump(parms, "|           privdata          ", d->privdata, d->privdata_len);
}

void dvb_desc_ca_free(struct dvb_desc *desc)
{
	struct dvb_desc_ca *d = (struct dvb_desc_ca *) desc;
	if (d->privdata)
		free(d->privdata);
}

