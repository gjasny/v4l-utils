/*
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

#include <libdvbv5/eit.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>

ssize_t dvb_table_eit_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
		ssize_t buflen, struct dvb_table_eit **table)
{
	const uint8_t *p = buf, *endbuf = buf + buflen;
	struct dvb_table_eit *eit;
	struct dvb_table_eit_event **head;
	size_t size;

	size = offsetof(struct dvb_table_eit, event);
	if (p + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - p, size);
		return -1;
	}

	if ((buf[0] != DVB_TABLE_EIT && buf[0] != DVB_TABLE_EIT_OTHER) &&
		!(buf[0] >= DVB_TABLE_EIT_SCHEDULE && buf[0] <= DVB_TABLE_EIT_SCHEDULE + 0xF) &&
		!(buf[0] >= DVB_TABLE_EIT_SCHEDULE_OTHER && buf[0] <= DVB_TABLE_EIT_SCHEDULE_OTHER + 0xF)) {
		dvb_logerr("%s: invalid marker 0x%02x, sould be 0x%02x, 0x%02x or between 0x%02x and 0x%02x or 0x%02x and 0x%02x",
				__func__, buf[0], DVB_TABLE_EIT, DVB_TABLE_EIT_OTHER,
				DVB_TABLE_EIT_SCHEDULE, DVB_TABLE_EIT_SCHEDULE + 0xF,
				DVB_TABLE_EIT_SCHEDULE_OTHER, DVB_TABLE_EIT_SCHEDULE_OTHER + 0xF);
		return -2;
	}

	if (!*table) {
		*table = calloc(sizeof(struct dvb_table_eit), 1);
		if (!*table) {
			dvb_logerr("%s: out of memory", __func__);
			return -3;
		}
	}
	eit = *table;
	memcpy(eit, p, size);
	p += size;
	dvb_table_header_init(&eit->header);

	bswap16(eit->transport_id);
	bswap16(eit->network_id);

	/* find end of curent list */
	head = &eit->event;
	while (*head != NULL)
		head = &(*head)->next;

	/* get the event entries */
	size = offsetof(struct dvb_table_eit_event, descriptor);
	while (p + size <= endbuf) {
		struct dvb_table_eit_event *event;

		event = malloc(sizeof(struct dvb_table_eit_event));
		if (!event) {
			dvb_logerr("%s: out of memory", __func__);
			return -4;
		}
		memcpy(event, p, size);
		p += size;

		bswap16(event->event_id);
		bswap16(event->bitfield1);
		bswap16(event->bitfield2);
		event->descriptor = NULL;
		event->next = NULL;
		dvb_time(event->dvbstart, &event->start);
		event->duration = dvb_bcd((uint32_t) event->dvbduration[0]) * 3600 +
				  dvb_bcd((uint32_t) event->dvbduration[1]) * 60 +
				  dvb_bcd((uint32_t) event->dvbduration[2]);

		event->service_id = eit->header.id;

		*head = event;
		head = &(*head)->next;

		/* parse the descriptors */
		if (event->desc_length > 0) {
			uint16_t desc_length = event->desc_length;
			if (p + desc_length > endbuf) {
				dvb_logwarn("%s: descriptors short read %zd/%d bytes", __func__,
					   endbuf - p, desc_length);
				desc_length = endbuf - p;
			}
			if (dvb_desc_parse(parms, p, desc_length,
					      &event->descriptor) != 0) {
				return -5;
			}
			p += desc_length;
		}
	}
	if (p < endbuf)
		dvb_logwarn("%s: %zu spurious bytes at the end",
			   __func__, endbuf - p);
	return p - buf;
}

void dvb_table_eit_free(struct dvb_table_eit *eit)
{
	struct dvb_table_eit_event *event = eit->event;
	while (event) {
		dvb_desc_free((struct dvb_desc **) &event->descriptor);
		struct dvb_table_eit_event *tmp = event;
		event = event->next;
		free(tmp);
	}
	free(eit);
}

void dvb_table_eit_print(struct dvb_v5_fe_parms *parms, struct dvb_table_eit *eit)
{
	dvb_loginfo("EIT");
	dvb_table_header_print(parms, &eit->header);
	dvb_loginfo("|- transport_id       %d", eit->transport_id);
	dvb_loginfo("|- network_id         %d", eit->network_id);
	dvb_loginfo("|- last segment       %d", eit->last_segment);
	dvb_loginfo("|- last table         %d", eit->last_table_id);
	dvb_loginfo("|\\  event_id");
	const struct dvb_table_eit_event *event = eit->event;
	uint16_t events = 0;
	while (event) {
		char start[255];
		strftime(start, sizeof(start), "%F %T", &event->start);
		dvb_loginfo("|- %7d", event->event_id);
		dvb_loginfo("|   Service               %d", event->service_id);
		dvb_loginfo("|   Start                 %s UTC", start);
		dvb_loginfo("|   Duration              %dh %dm %ds", event->duration / 3600, (event->duration % 3600) / 60, event->duration % 60);
		dvb_loginfo("|   free CA mode          %d", event->free_CA_mode);
		dvb_loginfo("|   running status        %d: %s", event->running_status, dvb_eit_running_status_name[event->running_status] );
		dvb_desc_print(parms, event->descriptor);
		event = event->next;
		events++;
	}
	dvb_loginfo("|_  %d events", events);
}

void dvb_time(const uint8_t data[5], struct tm *tm)
{
	/* ETSI EN 300 468 V1.4.1 */
	int year, month, day, hour, min, sec;
	int k = 0;
	uint16_t mjd;

	mjd   = *(uint16_t *) data;
	hour  = dvb_bcd(data[2]);
	min   = dvb_bcd(data[3]);
	sec   = dvb_bcd(data[4]);
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
	tm->tm_isdst = -1; /* do not adjust */
	mktime( tm );
}

const char *dvb_eit_running_status_name[8] = {
	[0] = "Undefined",
	[1] = "Not running",
	[2] = "Starts in a few seconds",
	[3] = "Pausing",
	[4] = "Running",
        [5 ... 7] = "Reserved"
};
