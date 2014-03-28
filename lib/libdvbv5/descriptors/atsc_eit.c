/*
 * Copyright (c) 2013 - Andre Roth <neolynx@gmail.com>
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

#include <libdvbv5/atsc_eit.h>
#include <libdvbv5/dvb-fe.h>

void atsc_table_eit_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, uint8_t *table, ssize_t *table_length)
{
	const uint8_t *p = buf, *endbuf = buf + buflen - 4; /* minus CRC */;
	struct atsc_table_eit *eit = (struct atsc_table_eit *) table;
	struct atsc_table_eit_event **head;
	int i = 0;
	struct atsc_table_eit_event *last = NULL;
	size_t size = offsetof(struct atsc_table_eit, event);

	if (p + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   size, endbuf - p);
		return;
	}

	if (*table_length > 0) {
		memcpy(eit, p, size);

		/* find end of curent list */
		head = &eit->event;
		while (*head != NULL)
			head = &(*head)->next;
	} else {
		memcpy(eit, p, size);
		*table_length = sizeof(struct atsc_table_eit);

		eit->event = NULL;
		head = &eit->event;
	}
	p += size;

	while (i++ < eit->events && p < endbuf) {
		struct atsc_table_eit_event *event;
                union atsc_table_eit_desc_length dl;

		size = offsetof(struct atsc_table_eit_event, descriptor);
		if (p + size > endbuf) {
			dvb_logerr("%s: short read %zd/%zd bytes", __func__,
				   size, endbuf - p);
			return;
		}
		event = (struct atsc_table_eit_event *) malloc(sizeof(struct atsc_table_eit_event));
		memcpy(event, p, size);
		p += size;

		bswap16(event->bitfield);
		bswap32(event->start_time);
		bswap32(event->bitfield2);
		event->descriptor = NULL;
		event->next = NULL;
                atsc_time(event->start_time, &event->start);
		event->source_id = eit->header.id;

		size = event->title_length - 1;
		if (p + size > endbuf) {
			dvb_logerr("%s: short read %zd/%zd bytes", __func__,
				   size, endbuf - p);
			return;
		}
                /* TODO: parse title */
                p += size;

		if(!*head)
			*head = event;
		if(last)
			last->next = event;

		/* get the descriptors for each program */
		size = sizeof(union atsc_table_eit_desc_length);
		if (p + size > endbuf) {
			dvb_logerr("%s: short read %zd/%zd bytes", __func__,
				   size, endbuf - p);
			return;
		}
		memcpy(&dl, p, size);
                p += size;
                bswap16(dl.bitfield);

		size = dl.desc_length;
		if (p + size > endbuf) {
			dvb_logerr("%s: short read %zd/%zd bytes", __func__,
				   size, endbuf - p);
			return;
		}
		dvb_parse_descriptors(parms, p, size, &event->descriptor);

		p += size;
		last = event;
	}
}

void atsc_table_eit_free(struct atsc_table_eit *eit)
{
	struct atsc_table_eit_event *event = eit->event;

	while (event) {
		struct atsc_table_eit_event *tmp = event;

		dvb_free_descriptors((struct dvb_desc **) &event->descriptor);
		event = event->next;
		free(tmp);
	}
	free(eit);
}

void atsc_table_eit_print(struct dvb_v5_fe_parms *parms, struct atsc_table_eit *eit)
{
	dvb_log("EIT");
	atsc_table_header_print(parms, &eit->header);
	const struct atsc_table_eit_event *event = eit->event;
	uint16_t events = 0;

	while (event) {
		char start[255];

		strftime(start, sizeof(start), "%F %T", &event->start);
		dvb_log("|-  event %7d", event->event_id);
		dvb_log("|   Source                %d", event->source_id);
		dvb_log("|   Starttime             %d", event->start_time);
		dvb_log("|   Start                 %s UTC", start);
		dvb_log("|   Duration              %dh %dm %ds", event->duration / 3600, (event->duration % 3600) / 60, event->duration % 60);
		dvb_log("|   ETM                   %d", event->etm);
		dvb_log("|   title length          %d", event->title_length);
		dvb_print_descriptors(parms, event->descriptor);
		event = event->next;
		events++;
	}
	dvb_log("|_  %d events", events);
}

void atsc_time(const uint32_t start_time, struct tm *tm)
{
  tm->tm_sec   = 0;
  tm->tm_min   = 0;
  tm->tm_hour  = 0;
  tm->tm_mday  = 6;
  tm->tm_mon   = 0;
  tm->tm_year  = 80;
  tm->tm_isdst = -1;
  tm->tm_wday  = 0;
  tm->tm_yday  = 0;
  mktime(tm);
  tm->tm_sec += start_time;
  mktime(tm);
}


