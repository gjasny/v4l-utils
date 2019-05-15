/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
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

#include <libdvbv5/sdt.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>

#if __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif

ssize_t dvb_table_sdt_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			ssize_t buflen, struct dvb_table_sdt **table)
{
	const uint8_t *p = buf, *endbuf = buf + buflen;
	struct dvb_table_sdt *sdt;
	struct dvb_table_sdt_service **head;
	size_t size;

	size = offsetof(struct dvb_table_sdt, service);
	if (p + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - p, size);
		return -1;
	}

	if (buf[0] != DVB_TABLE_SDT && buf[0] != DVB_TABLE_SDT2) {
		dvb_logerr("%s: invalid marker 0x%02x, should be 0x%02x or 0x%02x",
				__func__, buf[0], DVB_TABLE_SDT, DVB_TABLE_SDT2);
		return -2;
	}

	if (!*table) {
		*table = calloc(sizeof(struct dvb_table_sdt), 1);
		if (!*table) {
			dvb_logerr("%s: out of memory", __func__);
			return -3;
		}
	}
	sdt = *table;
	memcpy(sdt, p, size);
	p += size;
	dvb_table_header_init(&sdt->header);
	bswap16(sdt->network_id);

	/* find end of curent list */
	head = &sdt->service;
	while (*head != NULL)
		head = &(*head)->next;

	size = sdt->header.section_length + 3 - DVB_CRC_SIZE; /* plus header, minus CRC */
	if (buf + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - buf, size);
		return -4;
	}
	endbuf = buf + size;

	/* get the event entries */
	size = offsetof(struct dvb_table_sdt_service, descriptor);
	while (p + size <= endbuf) {
		struct dvb_table_sdt_service *service;

		service = malloc(sizeof(struct dvb_table_sdt_service));
		if (!service) {
			dvb_logerr("%s: out of memory", __func__);
			return -5;
		}
		memcpy(service, p, size);
		p += size;

		bswap16(service->service_id);
		bswap16(service->bitfield);
		service->descriptor = NULL;
		service->next = NULL;

		*head = service;
		head = &(*head)->next;

		/* parse the descriptors */
		if (service->desc_length > 0) {
			uint16_t desc_length = service->desc_length;
			if (p + desc_length > endbuf) {
				dvb_logwarn("%s: descriptors short read %zd/%d bytes", __func__,
					   endbuf - p, desc_length);
				desc_length = endbuf - p;
			}
			if (dvb_desc_parse(parms, p, desc_length,
					      &service->descriptor) != 0) {
				return -6;
			}
			p += desc_length;
		}

	}
	if (endbuf - p)
		dvb_logwarn("%s: %zu spurious bytes at the end",
			   __func__, endbuf - p);

	return p - buf;
}

void dvb_table_sdt_free(struct dvb_table_sdt *sdt)
{
	struct dvb_table_sdt_service *service = sdt->service;
	while(service) {
		dvb_desc_free((struct dvb_desc **) &service->descriptor);
		struct dvb_table_sdt_service *tmp = service;
		service = service->next;
		free(tmp);
	}
	free(sdt);
}

void dvb_table_sdt_print(struct dvb_v5_fe_parms *parms, struct dvb_table_sdt *sdt)
{
	dvb_loginfo("SDT");
	dvb_table_header_print(parms, &sdt->header);
	dvb_loginfo("| network_id          %d", sdt->network_id);
	dvb_loginfo("| reserved            %d", sdt->reserved);
	dvb_loginfo("|\\");
	const struct dvb_table_sdt_service *service = sdt->service;
	uint16_t services = 0;
	while (service) {
		dvb_loginfo("|- service 0x%04x", service->service_id);
		dvb_loginfo("|   EIT schedule          %d", service->EIT_schedule);
		dvb_loginfo("|   EIT present following %d", service->EIT_present_following);
		dvb_loginfo("|   free CA mode          %d", service->free_CA_mode);
		dvb_loginfo("|   running status        %d", service->running_status);
		dvb_loginfo("|   descriptor length     %d", service->desc_length);
		dvb_desc_print(parms, service->descriptor);
		service = service->next;
		services++;
	}
	dvb_loginfo("|_  %d services", services);
}

