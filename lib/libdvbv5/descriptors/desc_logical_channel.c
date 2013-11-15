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
 * Described on IEC/CENELEC DS/EN 62216-1:2011
 *
 * I couldn't find the original version, so I used what's there at:
 *	http://tdt.telecom.pt/recursos/apresentacoes/Signalling Specifications for DTT deployment in Portugal.pdf
 */

#include "descriptors.h"
#include "descriptors/desc_logical_channel.h"
#include "dvb-fe.h"
#include "parse_string.h"

void dvb_desc_logical_channel_init(struct dvb_v5_fe_parms *parms,
			      const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_logical_channel *d = (void *)desc;
	unsigned char *p = (unsigned char *)buf;
	size_t len;
	int i;

	d->lcn = malloc(d->length);
	if (!d->lcn)
		dvb_perror("Out of memory!");

	memcpy(d->lcn, p, d->length);

	len = d->length / sizeof(d->lcn);

	for (i = 0; i < len; i++) {
		bswap16(d->lcn[i].service_id);
		bswap16(d->lcn[i].bitfield);
	}
}

void dvb_desc_logical_channel_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	struct dvb_desc_logical_channel *d = (void *)desc;
	int i;
	size_t len;

	len = d->length / sizeof(d->lcn);

	for (i = 0; i < len; i++) {
		dvb_log("|           service ID[%d]     %d", i, d->lcn[i].service_id);
		dvb_log("|           LCN             %d", d->lcn[i].logical_channel_number);
		dvb_log("|           visible service %d", d->lcn[i].visible_service_flag);
	}
}
