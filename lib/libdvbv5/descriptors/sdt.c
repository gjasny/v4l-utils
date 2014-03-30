/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
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

#include <libdvbv5/sdt.h>
#include <libdvbv5/dvb-fe.h>

ssize_t dvb_table_sdt_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			ssize_t buflen, struct dvb_table_sdt *sdt, ssize_t *table_length)
{
	const uint8_t *p = buf, *endbuf = buf + buflen - 4;
	struct dvb_table_sdt_service **head = &sdt->service;
	size_t size = offsetof(struct dvb_table_sdt, service);

	if (*table_length > 0) {
		/* find end of curent list */
		while (*head != NULL)
			head = &(*head)->next;
	} else {
		if (p + size > endbuf) {
			dvb_logerr("SDT table was truncated. Need %zu bytes, but has only %zu.",
					size, buflen);
			return -1;
		}
		memcpy(sdt, p, size);
		*table_length = sizeof(struct dvb_table_sdt);

		bswap16(sdt->network_id);

		sdt->service = NULL;
	}
	p += size;

	size = offsetof(struct dvb_table_sdt_service, descriptor);
	while (p + size <= endbuf) {
		struct dvb_table_sdt_service *service;

		service = malloc(sizeof(struct dvb_table_sdt_service));

		memcpy(service, p, size);
		p += size;

		bswap16(service->service_id);
		bswap16(service->bitfield);
		service->descriptor = NULL;
		service->next = NULL;

		*head = service;
		head = &(*head)->next;

		/* get the descriptors for each program */
		dvb_parse_descriptors(parms, p, service->section_length,
				      &service->descriptor);

		p += service->section_length;
	}
	if (endbuf - p)
		dvb_logerr("SDT table has %zu spurious bytes at the end.",
			   endbuf - p);

	*table_length = p - buf;
	return p - buf;
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
	dvb_log("|\\");
	const struct dvb_table_sdt_service *service = sdt->service;
	uint16_t services = 0;
	while(service) {
		dvb_log("|- service 0x%04x", service->service_id);
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

