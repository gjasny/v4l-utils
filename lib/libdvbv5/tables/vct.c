/*
 * Copyright (c) 2013 - Mauro Carvalho Chehab <mchehab@kernel.org>
 * Copyright (c) 2013-2014 - Andre Roth <neolynx@gmail.com>
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
 */

#include <libdvbv5/vct.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>
#include <parse_string.h>

#if __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif

ssize_t atsc_table_vct_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			ssize_t buflen, struct atsc_table_vct **table)
{
	const uint8_t *p = buf, *endbuf = buf + buflen;
	struct atsc_table_vct *vct;
	struct atsc_table_vct_channel **head;
	size_t size;
	int i, n;

	size = offsetof(struct atsc_table_vct, channel);
	if (p + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - p, size);
		return -1;
	}

	if (buf[0] != ATSC_TABLE_TVCT && buf[0] != ATSC_TABLE_CVCT) {
		dvb_logerr("%s: invalid marker 0x%02x, should be 0x%02x or 0x%02x",
				__func__, buf[0], ATSC_TABLE_TVCT, ATSC_TABLE_CVCT);
		return -2;
	}

	if (!*table) {
		*table = calloc(sizeof(struct atsc_table_vct), 1);
		if (!*table) {
			dvb_logerr("%s: out of memory", __func__);
			return -3;
		}
	}
	vct = *table;
	memcpy(vct, p, size);
	p += size;
	dvb_table_header_init(&vct->header);

	/* find end of curent list */
	head = &vct->channel;
	while (*head != NULL)
		head = &(*head)->next;

	size = offsetof(struct atsc_table_vct_channel, descriptor);
	for (n = 0; n < vct->num_channels_in_section; n++) {
		struct atsc_table_vct_channel *channel;

		if (p + size > endbuf) {
			dvb_logerr("%s: channel table is missing %d elements",
				   __func__, vct->num_channels_in_section - n + 1);
			vct->num_channels_in_section = n;
			break;
		}

		channel = malloc(sizeof(struct atsc_table_vct_channel));
		if (!channel) {
			dvb_logerr("%s: out of memory", __func__);
			return -4;
		}

		memcpy(channel, p, size);
		p += size;

		/* Fix endiannes of the copied fields */
		for (i = 0; i < ARRAY_SIZE(channel->__short_name); i++)
			bswap16(channel->__short_name[i]);

		bswap32(channel->carrier_frequency);
		bswap16(channel->channel_tsid);
		bswap16(channel->program_number);
		bswap32(channel->bitfield1);
		bswap16(channel->bitfield2);
		bswap16(channel->source_id);
		bswap16(channel->bitfield3);

		/* Short name is always UTF-16 */
		dvb_iconv_to_charset(parms, channel->short_name,
				     sizeof(channel->short_name),
				     (const unsigned char *)channel->__short_name,
				     sizeof(channel->__short_name),
				     "UTF-16",
				     parms->output_charset);

		/* Fill descriptors */

		channel->descriptor = NULL;
		channel->next = NULL;

		*head = channel;
		head = &(*head)->next;

		if (endbuf - p < channel->descriptors_length) {
			dvb_logerr("%s: short read %d/%zd bytes", __func__,
				   channel->descriptors_length, endbuf - p);
			return -5;
		}

		/* get the descriptors for each program */
		if (dvb_desc_parse(parms, p, channel->descriptors_length,
				      &channel->descriptor) != 0) {
			return -6;
		}

		p += channel->descriptors_length;
	}

	/* Get extra descriptors */
	size = sizeof(union atsc_table_vct_descriptor_length);
	while (p + size <= endbuf) {
		union atsc_table_vct_descriptor_length *d = (void *)p;
		bswap16(d->descriptor_length);
		p += size;
		if (endbuf - p < d->descriptor_length) {
			dvb_logerr("%s: short read %d/%zd bytes", __func__,
				   d->descriptor_length, endbuf - p);
			return -7;
		}
		if (dvb_desc_parse(parms, p, d->descriptor_length,
				      &vct->descriptor) != 0) {
			return -8;
		}
	}
	if (endbuf - p)
		dvb_logwarn("%s: %zu spurious bytes at the end",
			   __func__, endbuf - p);
	return p - buf;
}

void atsc_table_vct_free(struct atsc_table_vct *vct)
{
	struct atsc_table_vct_channel *channel = vct->channel;
	while (channel) {
		dvb_desc_free((struct dvb_desc **) &channel->descriptor);
		struct atsc_table_vct_channel *tmp = channel;
		channel = channel->next;
		free(tmp);
	}
	dvb_desc_free(&vct->descriptor);

	free(vct);
}

void atsc_table_vct_print(struct dvb_v5_fe_parms *parms, struct atsc_table_vct *vct)
{
	const struct atsc_table_vct_channel *channel = vct->channel;
	uint16_t channels = 0;

	if (vct->header.table_id == ATSC_TABLE_CVCT)
		dvb_loginfo("CVCT");
	else
		dvb_loginfo("TVCT");

	ATSC_TABLE_HEADER_PRINT(parms, vct);

	dvb_loginfo("|- #channels        %d", vct->num_channels_in_section);
	dvb_loginfo("|\\  channel_id");
	while (channel) {
		dvb_loginfo("|- Channel                %d.%d: %s",
			channel->major_channel_number,
			channel->minor_channel_number,
			channel->short_name);
		dvb_loginfo("|   modulation mode       %d", channel->modulation_mode);
		dvb_loginfo("|   carrier frequency     %d", channel->carrier_frequency);
		dvb_loginfo("|   TS ID                 %d", channel->channel_tsid);
		dvb_loginfo("|   program number        %d", channel->program_number);

		dvb_loginfo("|   ETM location          %d", channel->ETM_location);
		dvb_loginfo("|   access controlled     %d", channel->access_controlled);
		dvb_loginfo("|   hidden                %d", channel->hidden);

		if (vct->header.table_id == ATSC_TABLE_CVCT) {
			dvb_loginfo("|   path select           %d", channel->path_select);
			dvb_loginfo("|   out of band           %d", channel->out_of_band);
		}
		dvb_loginfo("|   hide guide            %d", channel->hide_guide);
		dvb_loginfo("|   service type          %d", channel->service_type);
		dvb_loginfo("|   source id            %d", channel->source_id);

		dvb_desc_print(parms, channel->descriptor);
		channel = channel->next;
		channels++;
	}
	dvb_loginfo("|_  %d channels", channels);
}
