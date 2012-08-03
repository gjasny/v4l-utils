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
				dvb_descriptors[stream->type].name, stream->type);
		dvb_print_descriptors(parms, stream->descriptor);
		stream = stream->next;
		streams++;
	}
	dvb_log("|_  %d streams", streams);
}

