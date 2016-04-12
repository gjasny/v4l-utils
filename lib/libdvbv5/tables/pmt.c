/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 * Copyright (c) 2012-2014 - Andre Roth <neolynx@gmail.com>
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

#include <libdvbv5/pmt.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>

#include <string.h> /* memcpy */

ssize_t dvb_table_pmt_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			ssize_t buflen, struct dvb_table_pmt **table)
{
	const uint8_t *p = buf, *endbuf = buf + buflen;
	struct dvb_table_pmt *pmt;
	struct dvb_table_pmt_stream **head;
	struct dvb_desc **head_desc;
	size_t size;

	size = offsetof(struct dvb_table_pmt, dvb_pmt_field_last);
	if (p + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - p, size);
		return -1;
	}

	if (buf[0] != DVB_TABLE_PMT) {
		dvb_logerr("%s: invalid marker 0x%02x, sould be 0x%02x",
				__func__, buf[0], DVB_TABLE_PMT);
		return -2;
	}

	if (!*table) {
		*table = calloc(sizeof(struct dvb_table_pmt), 1);
		if (!*table) {
			dvb_logerr("%s: out of memory", __func__);
			return -3;
		}
	}
	pmt = *table;
	memcpy(pmt, p, size);
	p += size;
	dvb_table_header_init(&pmt->header);
	bswap16(pmt->bitfield);
	bswap16(pmt->bitfield2);

	/* find end of current list */
	head = &pmt->stream;
	while (*head != NULL)
		head = &(*head)->next;
	head_desc = &pmt->descriptor;
	while (*head_desc != NULL)
		head_desc = &(*head_desc)->next;

	size = pmt->header.section_length + 3 - DVB_CRC_SIZE; /* plus header, minus CRC */
	if (buf + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - buf, size);
		return -4;
	}
	endbuf = buf + size;

	/* parse the descriptors */
	if (pmt->desc_length > 0 ) {
		uint16_t desc_length = pmt->desc_length;
		if (p + desc_length > endbuf) {
			dvb_logwarn("%s: descriptors short read %d/%zd bytes", __func__,
				   desc_length, endbuf - p);
			desc_length = endbuf - p;
		}
		if (dvb_desc_parse(parms, p, desc_length,
				      head_desc) != 0) {
			return -4;
		}
		p += desc_length;
	}

	/* get the stream entries */
	size = offsetof(struct dvb_table_pmt_stream, descriptor);
	while (p + size <= endbuf) {
		struct dvb_table_pmt_stream *stream;

		stream = malloc(sizeof(struct dvb_table_pmt_stream));
		if (!stream) {
			dvb_logerr("%s: out of memory", __func__);
			return -5;
		}
		memcpy(stream, p, size);
		p += size;

		bswap16(stream->bitfield);
		bswap16(stream->bitfield2);
		stream->descriptor = NULL;
		stream->next = NULL;

		*head = stream;
		head = &(*head)->next;

		/* parse the descriptors */
		if (stream->desc_length > 0) {
			uint16_t desc_length = stream->desc_length;
			if (p + desc_length > endbuf) {
				dvb_logwarn("%s: descriptors short read %zd/%d bytes", __func__,
					   endbuf - p, desc_length);
				desc_length = endbuf - p;
			}
			if (dvb_desc_parse(parms, p, desc_length,
					      &stream->descriptor) != 0) {
				return -6;
			}
			p += desc_length;
		}
	}
	if (p < endbuf)
		dvb_logwarn("%s: %zu spurious bytes at the end",
			   __func__, endbuf - p);

	return p - buf;
}

void dvb_table_pmt_free(struct dvb_table_pmt *pmt)
{
	struct dvb_table_pmt_stream *stream = pmt->stream;
	while(stream) {
		dvb_desc_free((struct dvb_desc **) &stream->descriptor);
		struct dvb_table_pmt_stream *tmp = stream;
		stream = stream->next;
		free(tmp);
	}
	dvb_desc_free((struct dvb_desc **) &pmt->descriptor);
	free(pmt);
}

void dvb_table_pmt_print(struct dvb_v5_fe_parms *parms, const struct dvb_table_pmt *pmt)
{
	dvb_loginfo("PMT");
	dvb_table_header_print(parms, &pmt->header);
	dvb_loginfo("|- pcr_pid          %04x", pmt->pcr_pid);
	dvb_loginfo("|  reserved2           %d", pmt->reserved2);
	dvb_loginfo("|  descriptor length   %d", pmt->desc_length);
	dvb_loginfo("|  zero3               %d", pmt->zero3);
	dvb_loginfo("|  reserved3          %d", pmt->reserved3);
	dvb_desc_print(parms, pmt->descriptor);
	dvb_loginfo("|\\");
	const struct dvb_table_pmt_stream *stream = pmt->stream;
	uint16_t streams = 0;
	while(stream) {
		dvb_loginfo("|- stream 0x%04x: %s (%x)", stream->elementary_pid,
				pmt_stream_name[stream->type], stream->type);
		dvb_loginfo("|    descriptor length   %d", stream->desc_length);
		dvb_desc_print(parms, stream->descriptor);
		stream = stream->next;
		streams++;
	}
	dvb_loginfo("|_  %d streams", streams);
}

const char *pmt_stream_name[] = {
	[stream_reserved0]         = "Reserved",
	[stream_video]             = "Video ISO/IEC 11172",
	[stream_video_h262]        = "Video ISO/IEC 13818-2",
	[stream_audio]             = "Audio ISO/IEC 11172",
	[stream_audio_13818_3]     = "Audio ISO/IEC 13818-3",
	[stream_private_sections]  = "ISO/IEC 13818-1 Private Sections",
	[stream_private_data]      = "ISO/IEC 13818-1 Private Data",
	[stream_mheg]              = "ISO/IEC 13522 MHEG",
	[stream_h222]              = "ISO/IEC 13818-1 Annex A DSM-CC",
	[stream_h222_1]            = "ITU-T Rec. H.222.1",
	[stream_13818_6_A]         = "ISO/IEC 13818-6 type A",
	[stream_13818_6_B]         = "ISO/IEC 13818-6 type B",
	[stream_13818_6_C]         = "ISO/IEC 13818-6 type C",
	[stream_13818_6_D]         = "ISO/IEC 13818-6 type D",
	[stream_h222_aux]          = "ISO/IEC 13818-1 auxiliary",
	[stream_audio_adts]        = "Audio ISO/IEC 13818-7 ADTS",
	[stream_video_14496_2]     = "Video ISO/IEC 14496-2",
	[stream_audio_latm]        = "Audio ISO/IEC 14496-3 LATM",
	[stream_14496_1_pes]       = "ISO/IEC 14496-1 PES",
	[stream_14496_1_iso]       = "ISO/IEC 14496-1 ISO",
	[stream_download]          = "ISO/IEC 13818-6 Synchronized Download Protocol",
	[stream_reserved ... 0x7f] = "ISO/IEC 13818-1 Reserved",
	[stream_private  ... 0xff] = "User Private"
};

