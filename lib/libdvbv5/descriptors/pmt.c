/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 * Copyright (c) 2012-2013 - Andre Roth <neolynx@gmail.com>
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

#include <libdvbv5/pmt.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>

#include <string.h> /* memcpy */

void dvb_table_pmt_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			ssize_t buflen, uint8_t *table, ssize_t *table_length)
{
	const uint8_t *p = buf, *endbuf = buf + buflen - 4;
	struct dvb_table_pmt *pmt = (void *)table;
	struct dvb_table_pmt_stream **head = &pmt->stream;
	size_t size;

	if (buf[0] != DVB_TABLE_PMT) {
		dvb_logerr("%s: invalid marker 0x%02x, sould be 0x%02x", __func__, buf[0], DVB_TABLE_PMT);
		*table_length = 0;
		return;
	}

	if (*table_length > 0) {
		/* find end of current list */
		while (*head != NULL)
			head = &(*head)->next;
	} else {
		size = offsetof(struct dvb_table_pmt, dvb_pmt_field_last);
		if (p + size > endbuf) {
			dvb_logerr("%s: short read %zd/%zd bytes", __func__,
				   size, endbuf - p);
			return;
		}
		memcpy(pmt, p, size);
		p += size;

		bswap16(pmt->bitfield);
		bswap16(pmt->bitfield2);
		pmt->descriptor = NULL;
		pmt->stream = NULL;

		/* parse the descriptors */
		if (pmt->desc_length > 0 ) {
			size = pmt->desc_length;
			if (p + size > endbuf) {
				dvb_logwarn("%s: decsriptors short read %zd/%zd bytes", __func__,
					   size, endbuf - p);
				size = endbuf - p;
			}
			dvb_parse_descriptors(parms, p, size,
					      &pmt->descriptor);
			p += size;
		}
	}

	/* get the stream entries */
	size = offsetof(struct dvb_table_pmt_stream, descriptor);
	while (p + size <= endbuf) {
		struct dvb_table_pmt_stream *stream;

		stream = malloc(sizeof(struct dvb_table_pmt_stream));

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
			size = stream->desc_length;
			if (p + size > endbuf) {
				dvb_logwarn("%s: decsriptors short read %zd/%zd bytes", __func__,
					   size, endbuf - p);
				size = endbuf - p;
			}
			dvb_parse_descriptors(parms, p, size,
					      &stream->descriptor);

			p += size;
		}
	}
	if (p < endbuf)
		dvb_logwarn("%s: %zu spurious bytes at the end",
			   __func__, endbuf - p);

	*table_length = p - buf;
}

void dvb_table_pmt_free(struct dvb_table_pmt *pmt)
{
	struct dvb_table_pmt_stream *stream = pmt->stream;
	while(stream) {
		dvb_free_descriptors((struct dvb_desc **) &stream->descriptor);
		struct dvb_table_pmt_stream *tmp = stream;
		stream = stream->next;
		free(tmp);
	}
	dvb_free_descriptors((struct dvb_desc **) &pmt->descriptor);
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
	dvb_print_descriptors(parms, pmt->descriptor);
	dvb_loginfo("|\\");
	const struct dvb_table_pmt_stream *stream = pmt->stream;
	uint16_t streams = 0;
	while(stream) {
		dvb_loginfo("|- stream 0x%04x: %s (%x)", stream->elementary_pid,
				pmt_stream_name[stream->type], stream->type);
		dvb_loginfo("|    descriptor length   %d", stream->desc_length);
		dvb_print_descriptors(parms, stream->descriptor);
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

