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
 * Based on ETSI EN 300 468 V1.11.1 (2010-04)
 */

#include "descriptors.h"
#include "descriptors/desc_extension.h"
#include "descriptors/desc_t2_delivery.h"
#include "dvb-fe.h"

void dvb_desc_t2_delivery_init(struct dvb_v5_fe_parms *parms,
			       const uint8_t *buf,
			       struct dvb_extension_descriptor *ext,
			       void *desc)
{
	struct dvb_desc_t2_delivery *d = desc;
	unsigned char *p = (unsigned char *) buf;
	size_t len;
	int i;

	len = sizeof(*d);
	memcpy(&d->centre_frequency, p, len);
	p += len;

	bswap16(d->system_id);

	if (ext->length - 1 <= 4)
		return;

	bswap16(d->bitfield);

	if (d->tfs_flag)
		d->frequency_loop_length = 1;
	else {
		d->frequency_loop_length = *p;
		p++;
	}

	d->centre_frequency = malloc(sizeof(*d->centre_frequency) * d->frequency_loop_length);
	if (!d->centre_frequency)
		dvb_perror("Out of memory");

	memcpy(d->centre_frequency, p, sizeof(*d->centre_frequency) * d->frequency_loop_length);
	p += sizeof(*d->centre_frequency) * d->frequency_loop_length;

	for (i = 0; i < d->frequency_loop_length; i++)
		bswap32(d->centre_frequency[i]);

	d->subcel_info_loop_length = *p;
	p++;

	d->subcell = malloc(sizeof(*d->subcell) * d->subcel_info_loop_length);
	if (!d->subcell)
		dvb_perror("Out of memory");
	memcpy(d->subcell, p, sizeof(*d->subcell) * d->subcel_info_loop_length);

	for (i = 0; i < d->subcel_info_loop_length; i++)
		bswap16(d->subcell[i].transposer_frequency);
}

void dvb_desc_t2_delivery_print(struct dvb_v5_fe_parms *parms,
				const struct dvb_extension_descriptor *ext,
				const void *desc)
{
	const struct dvb_desc_t2_delivery *d = desc;
	int i;

	dvb_log("|       t2 delivery");
	dvb_log("|           descriptor_tag_extension  %d", d->descriptor_tag_extension);
	dvb_log("|           plp_id                    %d", d->plp_id);
	dvb_log("|           system_id                 %d", d->system_id);

	if (ext->length - 1 <= 4)
		return;

	dvb_log("|           tfs_flag                  %d", d->tfs_flag);
	dvb_log("|           other_frequency_flag      %d", d->other_frequency_flag);
	dvb_log("|           transmission_mode         %d", d->transmission_mode);
	dvb_log("|           guard_interval            %d", d->guard_interval);
	dvb_log("|           reserved                  %d", d->reserved);
	dvb_log("|           bandwidth                 %d", d->bandwidth);
	dvb_log("|           SISO MISO                 %d", d->SISO_MISO);

	for (i = 0; i < d->frequency_loop_length; i++)
		dvb_log("|           centre frequency[%d]   %d", i, d->centre_frequency[i]);

	for (i = 0; i < d->subcel_info_loop_length; i++) {
		dvb_log("|           cell_id_extension[%d]  %d", i, d->subcell[i].cell_id_extension);
		dvb_log("|           transposer frequency   %d", d->subcell[i].transposer_frequency);
	}
}

void dvb_desc_t2_delivery_free(const void *desc)
{
	const struct dvb_desc_t2_delivery *d = desc;

	if (d->centre_frequency)
		free(d->centre_frequency);

	if (d->subcell)
		free(d->subcell);
}
