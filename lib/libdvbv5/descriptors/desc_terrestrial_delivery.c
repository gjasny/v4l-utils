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

#include "descriptors/desc_terrestrial_delivery.h"
#include "descriptors.h"
#include "dvb-fe.h"

void dvb_desc_terrestrial_delivery_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_terrestrial_delivery *tdel = (struct dvb_desc_terrestrial_delivery *) desc;
	/* copy from .length */
	memcpy(((uint8_t *) tdel ) + sizeof(tdel->type) + sizeof(tdel->next) + sizeof(tdel->length),
			buf,
			tdel->length);
	bswap32(tdel->centre_frequency);
	bswap32(tdel->reserved_future_use2);
}

void dvb_desc_terrestrial_delivery_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_terrestrial_delivery *tdel = (const struct dvb_desc_terrestrial_delivery *) desc;
	dvb_log("|       terrestrial delivery");
	dvb_log("|           length                %d", tdel->length);
	dvb_log("|           centre frequency      %d", tdel->centre_frequency);
	dvb_log("|           mpe_fec_indicator     %d", tdel->mpe_fec_indicator);
	dvb_log("|           time_slice_indicator  %d", tdel->time_slice_indicator);
	dvb_log("|           priority              %d", tdel->priority);
	dvb_log("|           bandwidth             %d", tdel->bandwidth);
	dvb_log("|           code_rate_hp_stream   %d", tdel->code_rate_hp_stream);
	dvb_log("|           hierarchy_information %d", tdel->hierarchy_information);
	dvb_log("|           constellation         %d", tdel->constellation);
	dvb_log("|           other_frequency_flag  %d", tdel->other_frequency_flag);
	dvb_log("|           transmission_mode     %d", tdel->transmission_mode);
	dvb_log("|           guard_interval        %d", tdel->guard_interval);
	dvb_log("|           code_rate_lp_stream   %d", tdel->code_rate_lp_stream);
}

