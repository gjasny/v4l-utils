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

#include "descriptors/desc_cable_delivery.h"
#include "descriptors.h"
#include "dvb-fe.h"

void dvb_desc_cable_delivery_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_cable_delivery *cable = (struct dvb_desc_cable_delivery *) desc;
	/* copy only the data - length already initialize */
	memcpy(((uint8_t *) cable ) + sizeof(cable->type) + sizeof(cable->next) + sizeof(cable->length),
			buf,
			cable->length);
	bswap32(cable->frequency);
	bswap16(cable->bitfield1);
	bswap32(cable->bitfield2);
	cable->frequency   = bcd(cable->frequency) * 100;
	cable->symbol_rate = bcd(cable->symbol_rate) * 100;
}

void dvb_desc_cable_delivery_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_cable_delivery *cable = (const struct dvb_desc_cable_delivery *) desc;
	dvb_log("|        cable delivery");
	dvb_log("|           length            %d", cable->length);
	dvb_log("|           frequency         %d", cable->frequency);
	dvb_log("|           fec outer         %d", cable->fec_outer);
	dvb_log("|           modulation        %d", cable->modulation);
	dvb_log("|           symbol_rate       %d", cable->symbol_rate);
	dvb_log("|           fec inner         %d", cable->fec_inner);
}

