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

#include "descriptors/desc_service_list.h"
#include "descriptors.h"
#include "dvb-fe.h"

void dvb_desc_service_list_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	/*struct dvb_desc_service_list *slist = (struct dvb_desc_service_list *) desc;*/

	/*memcpy( slist->services, buf, slist->length);*/
	/*[> close the list <]*/
	/*slist->services[slist->length / sizeof(struct dvb_desc_service_list_table)].service_id = 0;*/
	/*slist->services[slist->length / sizeof(struct dvb_desc_service_list_table)].service_type = 0;*/
	/*[> swap the ids <]*/
	/*int i;*/
	/*for( i = 0; slist->services[i].service_id != 0; ++i) {*/
		/*bswap16(slist->services[i].service_id);*/
	/*}*/

	/*return sizeof(struct dvb_desc_service_list) + slist->length + sizeof(struct dvb_desc_service_list_table);*/
	//FIXME: make linked list
}

void dvb_desc_service_list_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	/*const struct dvb_desc_service_list *slist = (const struct dvb_desc_service_list *) desc;*/
	/*int i = 0;*/
	/*while(slist->services[i].service_id != 0) {*/
		/*dvb_log("|           service id   : '%d'", slist->services[i].service_id);*/
		/*dvb_log("|           service type : '%d'", slist->services[i].service_type);*/
		/*++i;*/
	/*}*/
}

