/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 * Copyright (c) 2012 - Andre Roth <neolynx@gmail.com>
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

#include <libdvbv5/desc_cable_delivery.h>
#include <libdvbv5/dvb-fe.h>

int dvb_desc_cable_delivery_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_cable_delivery *cable = (struct dvb_desc_cable_delivery *) desc;
	/* copy only the data - length already initialize */
	memcpy(((uint8_t *) cable ) + sizeof(cable->type) + sizeof(cable->next) + sizeof(cable->length),
			buf,
			cable->length);
	bswap32(cable->frequency);
	bswap16(cable->bitfield1);
	bswap32(cable->bitfield2);
	cable->frequency   = dvb_bcd(cable->frequency) * 100;
	cable->symbol_rate = dvb_bcd(cable->symbol_rate) * 100;
	return 0;
}

void dvb_desc_cable_delivery_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_cable_delivery *cable = (const struct dvb_desc_cable_delivery *) desc;
	dvb_loginfo("|           length            %d", cable->length);
	dvb_loginfo("|           frequency         %d", cable->frequency);
	dvb_loginfo("|           fec outer         %d", cable->fec_outer);
	dvb_loginfo("|           modulation        %d", cable->modulation);
	dvb_loginfo("|           symbol_rate       %d", cable->symbol_rate);
	dvb_loginfo("|           fec inner         %d", cable->fec_inner);
}

const unsigned dvbc_fec_table[] = {
	[0]  = FEC_AUTO,	/* not defined */
	[1]  = FEC_1_2,
	[2]  = FEC_2_3,
	[3]  = FEC_3_4,
	[4]  = FEC_5_6,
	[5]  = FEC_7_8,
	[6]  = FEC_8_9,
	[7]  = FEC_3_5,
	[8]  = FEC_4_5,
	[9]  = FEC_9_10,
	[10 ...14] = FEC_AUTO,	/* Reserved for future usage */
	[15] = FEC_NONE,
};

const unsigned dvbc_modulation_table[] = {
	[0] = QAM_AUTO,		/* Not defined */
	[1] = QAM_16,
	[2] = QAM_32,
	[3] = QAM_64,
	[4] = QAM_128,
	[5] = QAM_256,
	[6 ...255] = QAM_AUTO	/* Reserved for future usage */
};

