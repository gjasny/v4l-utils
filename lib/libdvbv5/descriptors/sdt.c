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

void dvb_table_sdt_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, uint8_t *table, ssize_t *table_length)
{
	const uint8_t *p = buf;
	struct dvb_table_sdt *sdt = (struct dvb_table_sdt *) table;
	struct dvb_table_sdt_service **head;

	if (*table_length > 0) {
		/* find end of curent list */
		head = &sdt->service;
		while (*head != NULL)
			head = &(*head)->next;
	} else {
		memcpy(sdt, p, sizeof(struct dvb_table_sdt) - sizeof(sdt->service));
		*table_length = sizeof(struct dvb_table_sdt);

		bswap16(sdt->network_id);

		sdt->service = NULL;
		head = &sdt->service;
	}
	p += sizeof(struct dvb_table_sdt) - sizeof(sdt->service);

	struct dvb_table_sdt_service *last = NULL;
	while ((uint8_t *) p < buf + buflen - 4) {
		struct dvb_table_sdt_service *service = (struct dvb_table_sdt_service *) malloc(sizeof(struct dvb_table_sdt_service));
		memcpy(service, p, sizeof(struct dvb_table_sdt_service) - sizeof(service->descriptor) - sizeof(service->next));
		p += sizeof(struct dvb_table_sdt_service) - sizeof(service->descriptor) - sizeof(service->next);

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
		dvb_parse_descriptors(parms, p, service->section_length, head_desc);

		p += service->section_length;
		last = service;
	}
}

void dvb_table_sdt_free(struct dvb_table_sdt *sdt)
{
	struct dvb_table_sdt_service *service = sdt->service;
	while(service) {
		dvb_free_descriptors((struct dvb_desc **) &service->descriptor);
		struct dvb_table_sdt_service *tmp = service;
		service = service->next;
		free(tmp);
	}
	free(sdt);
}

void dvb_table_sdt_print(struct dvb_v5_fe_parms *parms, struct dvb_table_sdt *sdt)
{
	dvb_log("SDT");
	dvb_table_header_print(parms, &sdt->header);
	dvb_log("|- network_id         %d", sdt->network_id);
	dvb_log("|\\  service_id");
	const struct dvb_table_sdt_service *service = sdt->service;
	uint16_t services = 0;
	while(service) {
		dvb_log("|- %7d", service->service_id);
		dvb_log("|   EIT schedule          %d", service->EIT_schedule);
		dvb_log("|   EIT present following %d", service->EIT_present_following);
		dvb_log("|   free CA mode          %d", service->free_CA_mode);
		dvb_log("|   running status        %d", service->running_status);
		dvb_print_descriptors(parms, service->descriptor);
		service = service->next;
		services++;
	}
	dvb_log("|_  %d services", services);
}

