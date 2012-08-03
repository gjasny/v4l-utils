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

#include "descriptors/eit.h"
#include "dvb-fe.h"

void dvb_table_eit_init(struct dvb_v5_fe_parms *parms, const uint8_t *ptr, ssize_t size, uint8_t **buf, ssize_t *buflen)
{
	uint8_t *d;
	const uint8_t *p = ptr;
	struct dvb_table_eit *eit = (struct dvb_table_eit *) ptr;
	struct dvb_table_eit_event **head;

	bswap16(eit->transport_id);
	bswap16(eit->network_id);

	if (!*buf) {
		d = malloc(DVB_MAX_PAYLOAD_PACKET_SIZE * 2);
		*buf = d;
		*buflen = 0;
		eit = (struct dvb_table_eit *) d;
		memcpy(eit, p, sizeof(struct dvb_table_eit) - sizeof(eit->event));
		*buflen += sizeof(struct dvb_table_eit);

		eit->event = NULL;
		head = &eit->event;
	} else {
		// should realloc d
		d = *buf;

		/* find end of curent list */
		eit = (struct dvb_table_eit *) d;
		head = &eit->event;
		while (*head != NULL)
			head = &(*head)->next;

		/* read new table */
		eit = (struct dvb_table_eit *) p;
	}
	p += sizeof(struct dvb_table_eit) - sizeof(eit->event);

	struct dvb_table_eit_event *last = NULL;
	while ((uint8_t *) p < ptr + size - 4) {
		struct dvb_table_eit_event *event = (struct dvb_table_eit_event *) (d + *buflen);
		memcpy(d + *buflen, p, sizeof(struct dvb_table_eit_event) - sizeof(event->descriptor) - sizeof(event->next));
		p += sizeof(struct dvb_table_eit_event) - sizeof(event->descriptor) - sizeof(event->next);
		*buflen += sizeof(struct dvb_table_eit_event);

		bswap16(event->event_id);
		bswap16(event->bitfield);
		event->descriptor = NULL;
		event->next = NULL;

		if(!*head)
			*head = event;
		if(last)
			last->next = event;

		/* get the descriptors for each program */
		struct dvb_desc **head_desc = &event->descriptor;
		*buflen += dvb_parse_descriptors(parms, p, d + *buflen, event->section_length, head_desc);

		p += event->section_length;
		last = event;
	}
}

void dvb_table_eit_print(struct dvb_v5_fe_parms *parms, struct dvb_table_eit *eit)
{
	dvb_log("EIT");
	dvb_table_header_print(parms, &eit->header);
	dvb_log("|- transport_id       %d", eit->transport_id);
	dvb_log("|- network_id         %d", eit->network_id);
	dvb_log("|- last segment       %d", eit->last_segment);
	dvb_log("|- last table         %d", eit->last_table_id);
	dvb_log("|\\  event_id");
	const struct dvb_table_eit_event *event = eit->event;
	uint16_t events = 0;
	while(event) {
		dvb_log("|- %7d", event->event_id);
		dvb_log("|   free CA mode          %d", event->free_CA_mode);
		dvb_log("|   running status        %d", event->running_status);
		dvb_print_descriptors(parms, event->descriptor);
		event = event->next;
		events++;
	}
	dvb_log("|_  %d events", events);
}

