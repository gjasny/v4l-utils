/*
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

#include <libdvbv5/atsc_eit.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>

ssize_t atsc_table_eit_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
		ssize_t buflen, struct atsc_table_eit **table)
{
	const uint8_t *p = buf, *endbuf = buf + buflen;
	struct atsc_table_eit *eit;
	struct atsc_table_eit_event **head;
	size_t size;
	int i = 0;

	size = offsetof(struct atsc_table_eit, event);
	if (p + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - p, size);
		return -1;
	}

	if (buf[0] != ATSC_TABLE_EIT) {
		dvb_logerr("%s: invalid marker 0x%02x, sould be 0x%02x",
				__func__, buf[0], ATSC_TABLE_EIT);
		return -2;
	}

	if (!*table) {
		*table = calloc(sizeof(struct atsc_table_eit), 1);
		if (!*table) {
			dvb_logerr("%s: out of memory", __func__);
			return -3;
		}
	}
	eit = *table;
	memcpy(eit, p, size);
	p += size;
	dvb_table_header_init(&eit->header);

	/* find end of curent list */
	head = &eit->event;
	while (*head != NULL)
		head = &(*head)->next;

	while (i++ < eit->events && p < endbuf) {
		struct atsc_table_eit_event *event;
                union atsc_table_eit_desc_length dl;

		size = offsetof(struct atsc_table_eit_event, descriptor);
		if (p + size > endbuf) {
			dvb_logerr("%s: short read %zd/%zd bytes", __func__,
				   endbuf - p, size);
			return -4;
		}
		event = (struct atsc_table_eit_event *) malloc(sizeof(struct atsc_table_eit_event));
		if (!event) {
			dvb_logerr("%s: out of memory", __func__);
			return -5;
		}
		memcpy(event, p, size);
		p += size;

		bswap16(event->bitfield);
		bswap32(event->start_time);
		bswap32(event->bitfield2);
		event->descriptor = NULL;
		event->next = NULL;
                atsc_time(event->start_time, &event->start);
		event->source_id = eit->header.id;

		*head = event;
		head = &(*head)->next;

		size = event->title_length - 1;
		if (p + size > endbuf) {
			dvb_logerr("%s: short read %zd/%zd bytes", __func__,
				   endbuf - p, size);
			return -6;
		}
                /* TODO: parse title */
                p += size;

		/* get the descriptors for each program */
		size = sizeof(union atsc_table_eit_desc_length);
		if (p + size > endbuf) {
			dvb_logerr("%s: short read %zd/%zd bytes", __func__,
				   endbuf - p, size);
			return -7;
		}
		memcpy(&dl, p, size);
                p += size;
                bswap16(dl.bitfield);

		size = dl.desc_length;
		if (p + size > endbuf) {
			dvb_logerr("%s: short read %zd/%zd bytes", __func__,
				   endbuf - p, size);
			return -8;
		}
		if (dvb_desc_parse(parms, p, size,
					&event->descriptor) != 0 ) {
			return -9;
		}

		p += size;
	}

	return p - buf;
}

void atsc_table_eit_free(struct atsc_table_eit *eit)
{
	struct atsc_table_eit_event *event = eit->event;

	while (event) {
		struct atsc_table_eit_event *tmp = event;

		dvb_desc_free((struct dvb_desc **) &event->descriptor);
		event = event->next;
		free(tmp);
	}
	free(eit);
}

void atsc_table_eit_print(struct dvb_v5_fe_parms *parms, struct atsc_table_eit *eit)
{
	dvb_loginfo("EIT");
	ATSC_TABLE_HEADER_PRINT(parms, eit);
	const struct atsc_table_eit_event *event = eit->event;
	uint16_t events = 0;

	while (event) {
		char start[255];

		strftime(start, sizeof(start), "%F %T", &event->start);
		dvb_loginfo("|-  event %7d", event->event_id);
		dvb_loginfo("|   Source                %d", event->source_id);
		dvb_loginfo("|   Starttime             %d", event->start_time);
		dvb_loginfo("|   Start                 %s UTC", start);
		dvb_loginfo("|   Duration              %dh %dm %ds", event->duration / 3600, (event->duration % 3600) / 60, event->duration % 60);
		dvb_loginfo("|   ETM                   %d", event->etm);
		dvb_loginfo("|   title length          %d", event->title_length);
		dvb_desc_print(parms, event->descriptor);
		event = event->next;
		events++;
	}
	dvb_loginfo("|_  %d events", events);
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


