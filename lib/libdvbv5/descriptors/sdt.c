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

#include "descriptors/sdt.h"
#include "dvb-fe.h"

void *dvb_table_sdt_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t size)
{
	uint8_t *d = malloc(DVB_MAX_PAYLOAD_PACKET_SIZE * 2);
	const uint8_t *p = buf;
	struct dvb_table_sdt *sdt = (struct dvb_table_sdt *) d;

	memcpy(sdt, p, sizeof(struct dvb_table_sdt) - sizeof(sdt->service));
	p += sizeof(struct dvb_table_sdt) - sizeof(sdt->service);
	d += sizeof(struct dvb_table_sdt);

	dvb_table_header_init(&sdt->header);
	sdt->service = NULL;

	struct dvb_table_sdt_service *last = NULL;
	struct dvb_table_sdt_service **head = &sdt->service;
	while ((uint8_t *) p < buf + size - 4) {
		struct dvb_table_sdt_service *service = (struct dvb_table_sdt_service *) d;
		memcpy(d, p, sizeof(struct dvb_table_sdt_service) - sizeof(service->descriptor) - sizeof(service->next));
		p += sizeof(struct dvb_table_sdt_service) - sizeof(service->descriptor) - sizeof(service->next);
		d += sizeof(struct dvb_table_sdt_service);

		bswap16(service->service_id);
		bswap16(service->bitfield);
		service->descriptor = NULL;
		service->next = NULL;

		if(!*head)
			*head = service;
		if(last)
			last->next = service;

		/* get the descriptors for each program */
		struct dvb_desc **head_desc = &service->descriptor;
		d += dvb_parse_descriptors(parms, p, d, service->section_length, head_desc);

		p += service->section_length;
		last = service;
	}
	return sdt;
}

void dvb_table_sdt_print(struct dvb_v5_fe_parms *parms, struct dvb_table_sdt *sdt)
{
	dvb_log("SDT");
	dvb_table_header_print(parms, &sdt->header);
	dvb_log("|\\  service_id");
	const struct dvb_table_sdt_service *service = sdt->service;
	uint16_t services = 0;
	while(service) {
		dvb_log("|- %7d", service->service_id);
		dvb_log("|   EIT_schedule: %d", service->EIT_schedule);
		dvb_log("|   EIT_present_following: %d", service->EIT_present_following);
		struct dvb_desc *desc = service->descriptor;
		while (desc) {
			if (dvb_descriptors[desc->type].print)
				dvb_descriptors[desc->type].print(parms, desc);
			desc = desc->next;
		}
		service = service->next;
		services++;
	}
	dvb_log("|_  %d services", services);
}

