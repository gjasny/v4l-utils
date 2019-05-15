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
 * Based on ETSI EN 300 468 V1.11.1 (2010-04)
 *
 */

#include <libdvbv5/desc_terrestrial_delivery.h>
#include <libdvbv5/dvb-fe.h>

#if __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif

int dvb_desc_terrestrial_delivery_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_terrestrial_delivery *tdel = (struct dvb_desc_terrestrial_delivery *) desc;
	/* copy from .length */
	memcpy(((uint8_t *) tdel ) + sizeof(tdel->type) + sizeof(tdel->next) + sizeof(tdel->length),
			buf,
			tdel->length);
	bswap32(tdel->centre_frequency);
	bswap32(tdel->reserved_future_use2);
	return 0;
}

void dvb_desc_terrestrial_delivery_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_terrestrial_delivery *tdel = (const struct dvb_desc_terrestrial_delivery *) desc;
	dvb_loginfo("|           length                %d", tdel->length);
	dvb_loginfo("|           centre frequency      %d", tdel->centre_frequency * 10);
	dvb_loginfo("|           mpe_fec_indicator     %d", tdel->mpe_fec_indicator);
	dvb_loginfo("|           time_slice_indicator  %d", tdel->time_slice_indicator);
	dvb_loginfo("|           priority              %d", tdel->priority);
	dvb_loginfo("|           bandwidth             %d", tdel->bandwidth);
	dvb_loginfo("|           code_rate_hp_stream   %d", tdel->code_rate_hp_stream);
	dvb_loginfo("|           hierarchy_information %d", tdel->hierarchy_information);
	dvb_loginfo("|           constellation         %d", tdel->constellation);
	dvb_loginfo("|           other_frequency_flag  %d", tdel->other_frequency_flag);
	dvb_loginfo("|           transmission_mode     %d", tdel->transmission_mode);
	dvb_loginfo("|           guard_interval        %d", tdel->guard_interval);
	dvb_loginfo("|           code_rate_lp_stream   %d", tdel->code_rate_lp_stream);
}

const unsigned dvbt_bw[] = {
	[0] = 8000000,
	[1] = 7000000,
	[2] = 6000000,
	[3] = 5000000,
	[4 ...7] = 0,		/* Reserved */
};
const unsigned dvbt_modulation[] = {
	[0] = QPSK,
	[1] = QAM_16,
	[2] = QAM_64,
	[3] = QAM_AUTO	/* Reserved */
};
const unsigned dvbt_hierarchy[] = {
	[0] = HIERARCHY_NONE,
	[1] = HIERARCHY_1,
	[2] = HIERARCHY_2,
	[3] = HIERARCHY_4,
};
const unsigned dvbt_code_rate[] = {
	[0] = FEC_1_2,
	[1] = FEC_2_3,
	[2] = FEC_3_4,
	[3] = FEC_5_6,
	[4] = FEC_7_8,
	[5 ...7] = FEC_AUTO,	/* Reserved */
};
const uint32_t dvbt_interval[] = {
	[0] = GUARD_INTERVAL_1_32,
	[1] = GUARD_INTERVAL_1_16,
	[2] = GUARD_INTERVAL_1_8,
	[3] = GUARD_INTERVAL_1_4,
};
const unsigned dvbt_transmission_mode[] = {
	[0] = TRANSMISSION_MODE_2K,
	[1] = TRANSMISSION_MODE_8K,
	[2] = TRANSMISSION_MODE_4K,
	[3] = TRANSMISSION_MODE_AUTO,	/* Reserved */
};
