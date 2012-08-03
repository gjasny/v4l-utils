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

void dvb_table_eit_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, uint8_t *table, ssize_t *table_length)
{
	const uint8_t *p = buf;
	struct dvb_table_eit *eit = (struct dvb_table_eit *) table;
	struct dvb_table_eit_event **head;

	if (*table_length > 0) {
		/* find end of curent list */
		head = &eit->event;
		while (*head != NULL)
			head = &(*head)->next;
	} else {
		memcpy(eit, p, sizeof(struct dvb_table_eit) - sizeof(eit->event));
		*table_length = sizeof(struct dvb_table_eit);

		bswap16(eit->transport_id);
		bswap16(eit->network_id);

		eit->event = NULL;
		head = &eit->event;
	}
	p += sizeof(struct dvb_table_eit) - sizeof(eit->event);

	struct dvb_table_eit_event *last = NULL;
	while ((uint8_t *) p < buf + buflen - 4) {
		struct dvb_table_eit_event *event = (struct dvb_table_eit_event *) malloc(sizeof(struct dvb_table_eit_event));
		memcpy(event, p, sizeof(struct dvb_table_eit_event) - sizeof(event->descriptor) - sizeof(event->next) - sizeof(event->start) - sizeof(event->duration));
		p += sizeof(struct dvb_table_eit_event) - sizeof(event->descriptor) - sizeof(event->next) - sizeof(event->start) - sizeof(event->duration);

		bswap16(event->event_id);
		bswap16(event->bitfield);
		bswap16(event->bitfield2);
		event->descriptor = NULL;
		event->next = NULL;
		dvb_time(event->dvbstart, &event->start);
		event->duration = bcd(event->dvbduration[0]) * 3600 +
				  bcd(event->dvbduration[1]) * 60 +
				  bcd(event->dvbduration[2]);

		if(!*head)
			*head = event;
		if(last)
			last->next = event;

		/* get the descriptors for each program */
		struct dvb_desc **head_desc = &event->descriptor;
		dvb_parse_descriptors(parms, p, event->section_length, head_desc);

		p += event->section_length;
		last = event;
	}
}

void dvb_table_eit_free(struct dvb_table_eit *eit)
{
	struct dvb_table_eit_event *event = eit->event;
	while (event) {
		dvb_free_descriptors((struct dvb_desc **) &event->descriptor);
		struct dvb_table_eit_event *tmp = event;
		event = event->next;
		free(tmp);
	}
	free(eit);
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
	while (event) {
		char start[255];
		strftime(start, sizeof(start), "%F %T", &event->start);
		dvb_log("|- %7d", event->event_id);
		dvb_log("|   Start                 %s UTC", start);
		dvb_log("|   Duration              %dh %dm %ds", event->duration / 3600, (event->duration % 3600) / 60, event->duration % 60);
		dvb_log("|   free CA mode          %d", event->free_CA_mode);
		dvb_log("|   running status        %d: %s", event->running_status, dvb_eit_running_status_name[event->running_status] );
		dvb_print_descriptors(parms, event->descriptor);
		event = event->next;
		events++;
	}
	dvb_log("|_  %d events", events);
}

void dvb_time(const uint8_t data[5], struct tm *tm)
{
  /* ETSI EN 300 468 V1.4.1 */
  int year, month, day, hour, min, sec;
  int k = 0;
  uint16_t mjd;

  mjd   = *(uint16_t *) data;
  hour  = bcd(data[2]);
  min   = bcd(data[3]);
  sec   = bcd(data[4]);
  year  = ((mjd - 15078.2) / 365.25);
  month = ((mjd - 14956.1 - (int) (year * 365.25)) / 30.6001);
  day   = mjd - 14956 - (int) (year * 365.25) - (int) (month * 30.6001);
  if (month == 14 || month == 15) k = 1;
  year += k;
  month = month - 1 - k * 12;

  tm->tm_sec   = sec;
  tm->tm_min   = min;
  tm->tm_hour  = hour;
  tm->tm_mday  = day;
  tm->tm_mon   = month - 1;
  tm->tm_year  = year;
  tm->tm_isdst = -1;
  tm->tm_wday  = 0;
  tm->tm_yday  = 0;
}


const char *dvb_eit_running_status_name[8] = {
	[0] = "Undefined",
	[1] = "Not running",
	[2] = "Starts in a few seconds",
	[3] = "Pausing",
	[4] = "Running",
        [5 ... 7] = "Reserved"
};
