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

#include "descriptors/desc_frequency_list.h"
#include "descriptors.h"
#include "dvb-fe.h"

void dvb_desc_frequency_list_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_frequency_list *flist = (struct dvb_desc_frequency_list *) desc;

	flist->bitfield = buf[0];

	/*flist->frequencies = (flist->length - sizeof(flist->bitfield)) / sizeof(flist->frequency[0]);*/
	/*int i;*/
	/*for (i = 1; i <= flist->frequencies; i++) {*/
		/*flist->frequency[i] = ((uint32_t *) buf)[i];*/
		/*bswap32(flist->frequency[i]);*/
		/*switch (flist->freq_type) {*/
			/*case 1: [> satellite - to get kHz<]*/
			/*case 3: [> terrestrial - to get Hz<]*/
				/*flist->frequency[i] *= 10;*/
				/*break;*/
			/*case 2: [> cable - to get Hz <]*/
				/*flist->frequency[i] *= 100;*/
				/*break;*/
			/*case 0: [> not defined <]*/
			/*default:*/
				/*break;*/
		/*}*/
	/*}*/
// FIXME: malloc list entires
//	return sizeof(struct dvb_desc_frequency_list) + (flist->frequencies * sizeof(flist->frequency[0]));
}

void dvb_desc_frequency_list_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_frequency_list *flist = (const struct dvb_desc_frequency_list *) desc;
	dvb_log("|       frequency list type: %d", flist->freq_type);
	/*int i = 0;*/
	/*for (i = 0; i < flist->frequencies; i++) {*/
		/*dvb_log("|       frequency : %d", flist->frequency[i]);*/
	/*}*/
}

