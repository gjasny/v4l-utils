/*
 * Copyright (c) 2013 - Mauro Carvalho Chehab <m.chehab@samsung.com>
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
 */

#include <libdvbv5/desc_extension.h>
#include <libdvbv5/desc_t2_delivery.h>
#include <libdvbv5/dvb-fe.h>

int dvb_desc_t2_delivery_init(struct dvb_v5_fe_parms *parms,
			       const uint8_t *buf,
			       struct dvb_extension_descriptor *ext,
			       void *desc)
{
	struct dvb_desc_t2_delivery *d = desc;
	unsigned char *p = (unsigned char *) buf;
	size_t desc_len = ext->length - 1, len, len2;
	int i, n, pos = 0, subcel_length;

	len = offsetof(struct dvb_desc_t2_delivery, bitfield);
	len2 = offsetof(struct dvb_desc_t2_delivery, centre_frequency);

	if (desc_len < len) {
		dvb_logwarn("T2 delivery descriptor is too small");
		return -1;
	}
	if (desc_len < len2) {
		memcpy(d, buf, len);
		bswap16(d->system_id);

		/* It is valid to have length == 4 */
		if (desc_len == len)
			return 0;

		dvb_logwarn("T2 delivery descriptor is truncated");
		return -2;
	}
	memcpy(d, buf, len2);
	bswap16(d->system_id);
	bswap16(d->bitfield);
	p += len2;

	while (desc_len - (p - buf)) {
		if (desc_len - (p - buf) < sizeof(uint16_t)) {
			dvb_logwarn("T2 delivery descriptor is truncated");
			return -2;
		}


		/* Discard cell ID */
		p += sizeof(uint16_t);

		if (d->tfs_flag) {
			n = *p;
			p++;
		}
		else
			n = 1;

		d->frequency_loop_length += n;
		d->centre_frequency = realloc(d->centre_frequency,
					      d->frequency_loop_length * sizeof(*d->centre_frequency));
		if (!d->centre_frequency) {
			dvb_logerr("%s: out of memory", __func__);
			return -3;
		}

		memcpy(&d->centre_frequency[pos], p, sizeof(*d->centre_frequency) * n);
		p += sizeof(*d->centre_frequency) * n;

		for (i = 0; i < n; i++) {
			bswap32(d->centre_frequency[pos]);
			pos++;
		}

		/* Handle subcel frequency table */
		subcel_length = *p;
		p++;
		for (i = 0; i < subcel_length; i++) {
			if (desc_len - (p - buf) < sizeof(uint8_t) + sizeof(uint32_t)) {
				dvb_logwarn("T2 delivery descriptor is truncated");
				return -2;
			}
			p++;	// Ignore subcell ID

			// Add transposer_frequency at centre_frequency table
			d->frequency_loop_length++;
			d->centre_frequency = realloc(d->centre_frequency,
						      d->frequency_loop_length * sizeof(*d->centre_frequency));
			memcpy(&d->centre_frequency[pos], p, sizeof(*d->centre_frequency));
			bswap32(d->centre_frequency[pos]);
			pos++;

			p += sizeof(*d->centre_frequency);
		}
	}

	return 0;
}

void dvb_desc_t2_delivery_print(struct dvb_v5_fe_parms *parms,
				const struct dvb_extension_descriptor *ext,
				const void *desc)
{
	const struct dvb_desc_t2_delivery *d = desc;
	int i;

	dvb_loginfo("|           plp_id                    %d", d->plp_id);
	dvb_loginfo("|           system_id                 %d", d->system_id);

	if (ext->length - 1 <= 4)
		return;

	dvb_loginfo("|           tfs_flag                  %d", d->tfs_flag);
	dvb_loginfo("|           other_frequency_flag      %d", d->other_frequency_flag);
	dvb_loginfo("|           transmission_mode         %s (%d)",
		    fe_transmission_mode_name[dvbt2_transmission_mode[d->transmission_mode]], d->transmission_mode);
	dvb_loginfo("|           guard_interval            %s (%d)",
		    fe_guard_interval_name[dvbt2_interval[d->guard_interval]], d->guard_interval );
	dvb_loginfo("|           reserved                  %d", d->reserved);
	dvb_loginfo("|           bandwidth                 %d", dvbt2_bw[d->bandwidth]);
	dvb_loginfo("|           SISO MISO                 %s", siso_miso[d->SISO_MISO]);

	for (i = 0; i < d->frequency_loop_length; i++)
		dvb_loginfo("|           centre frequency[%d]   %d", i, d->centre_frequency[i]);
}

void dvb_desc_t2_delivery_free(const void *desc)
{
	const struct dvb_desc_t2_delivery *d = desc;

	if (d->centre_frequency)
		free(d->centre_frequency);

	if (d->subcell)
		free(d->subcell);
}

const unsigned dvbt2_bw[] = {
	[0] =  8000000,
	[1] =  7000000,
	[2] =  6000000,
	[3] =  5000000,
	[4] = 10000000,
	[5] =  1712000,
	[6 ...15] = 0,		/* Reserved */
};
const uint32_t dvbt2_interval[] = {
	[0] = GUARD_INTERVAL_1_32,
	[1] = GUARD_INTERVAL_1_16,
	[2] = GUARD_INTERVAL_1_8,
	[3] = GUARD_INTERVAL_1_4,
	[4] = GUARD_INTERVAL_1_128,
	[5] = GUARD_INTERVAL_19_128,
	[6] = GUARD_INTERVAL_19_256,
	[7 ...15] = GUARD_INTERVAL_AUTO /* Reserved */
};
const unsigned dvbt2_transmission_mode[] = {
	[0] = TRANSMISSION_MODE_2K,
	[1] = TRANSMISSION_MODE_8K,
	[2] = TRANSMISSION_MODE_4K,
	[3] = TRANSMISSION_MODE_1K,
	[4] = TRANSMISSION_MODE_16K,
	[5] = TRANSMISSION_MODE_32K,
	[6 ...7] = TRANSMISSION_MODE_AUTO,	/* Reserved */
};
const char *siso_miso[4] = {
	[0] = "SISO",
	[1] = "MISO",
	[2 ...3] = "reserved",
};
