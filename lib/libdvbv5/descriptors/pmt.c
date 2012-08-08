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

#include "descriptors/pmt.h"
#include "descriptors.h"
#include "dvb-fe.h"

#include <string.h> /* memcpy */

void dvb_table_pmt_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, uint8_t *table, ssize_t *table_length)
{
	if (*table_length > 0) {
		dvb_logerr("multisecttion PMT table not implemented");
		return;
	}

	const uint8_t *p = buf;
	struct dvb_table_pmt *pmt = (struct dvb_table_pmt *) table;
	memcpy(table, p, sizeof(struct dvb_table_pmt) - sizeof(pmt->stream));
	p += sizeof(struct dvb_table_pmt) - sizeof(pmt->stream);
	*table_length = sizeof(struct dvb_table_pmt);

	bswap16(pmt->bitfield);
	bswap16(pmt->bitfield2);
	pmt->stream = NULL;

	/* skip prog section */
	p += pmt->prog_length;

	/* get the stream entries */
	struct dvb_table_pmt_stream *last = NULL;
	struct dvb_table_pmt_stream **head = &pmt->stream;
	while (p < buf + buflen - 4) {
		struct dvb_table_pmt_stream *stream = (struct dvb_table_pmt_stream *) malloc(sizeof(struct dvb_table_pmt_stream));
		memcpy(stream, p, sizeof(struct dvb_table_pmt_stream) - sizeof(stream->descriptor) - sizeof(stream->next));
		p += sizeof(struct dvb_table_pmt_stream) - sizeof(stream->descriptor) - sizeof(stream->next);

		bswap16(stream->bitfield);
		bswap16(stream->bitfield2);
		stream->descriptor = NULL;
		stream->next = NULL;

		if(!*head)
			*head = stream;
		if(last)
			last->next = stream;

		/* get the descriptors for each program */
		struct dvb_desc **head_desc = &stream->descriptor;
		dvb_parse_descriptors(parms, p, stream->section_length, head_desc);

		p += stream->section_length;
		last = stream;
	}
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
	free(pmt);
}

void dvb_table_pmt_print(struct dvb_v5_fe_parms *parms, const struct dvb_table_pmt *pmt)
{
	dvb_log("PMT");
	dvb_table_header_print(parms, &pmt->header);
	dvb_log("|- pcr_pid       %d", pmt->pcr_pid);
	dvb_log("|  reserved2     %d", pmt->reserved2);
	dvb_log("|  prog length   %d", pmt->prog_length);
	dvb_log("|  zero3         %d", pmt->zero3);
	dvb_log("|  reserved3     %d", pmt->reserved3);
	dvb_log("|\\  pid     type");
	const struct dvb_table_pmt_stream *stream = pmt->stream;
	uint16_t streams = 0;
	while(stream) {
		dvb_log("|- %5d   %s (%d)", stream->elementary_pid,
				pmt_stream_name[stream->type], stream->type);
		dvb_print_descriptors(parms, stream->descriptor);
		stream = stream->next;
		streams++;
	}
	dvb_log("|_  %d streams", streams);
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

