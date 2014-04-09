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

#include <libdvbv5/desc_service_location.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>

int dvb_desc_service_location_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_service_location *service_location = (struct dvb_desc_service_location *) desc;
	const uint8_t *endbuf = buf + desc->length;
	ssize_t size = sizeof(struct dvb_desc_service_location) - sizeof(struct dvb_desc);
	struct dvb_desc_service_location_element *element;
	int i;

	if (buf + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __FUNCTION__, endbuf - buf, size);
		return -1;
	}

	memcpy(desc->data, buf, size);
	bswap16(service_location->bitfield);
	buf += size;

	if (service_location->elements == 0)
		return 0;

	size = service_location->elements * sizeof(struct dvb_desc_service_location_element);
	if (buf + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __FUNCTION__, endbuf - buf, size);
		return -2;
	}
	service_location->element = malloc(size);
	element = service_location->element;
	for (i = 0; i < service_location->elements; i++) {
		memcpy(element, buf, sizeof(struct dvb_desc_service_location_element) - 1); /* no \0 in lang */
		buf += sizeof(struct dvb_desc_service_location_element) - 1;
		element->language[3] = '\0';
		bswap16(element->bitfield);
		element++;
	}
	return 0;
}

void dvb_desc_service_location_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_service_location *service_location = (const struct dvb_desc_service_location *) desc;
	struct dvb_desc_service_location_element *element = service_location->element;
	int i;

	dvb_loginfo("|    pcr pid      %d", service_location->pcr_pid);
	dvb_loginfo("|    streams:");
	for (i = 0; i < service_location->elements; i++)
		dvb_loginfo("|      pid %d, type %d: %s", element[i].elementary_pid, element[i].stream_type, element[i].language);
	dvb_loginfo("| 	%d elements", service_location->elements);
}

void dvb_desc_service_location_free(struct dvb_desc *desc)
{
	const struct dvb_desc_service_location *service_location = (const struct dvb_desc_service_location *) desc;

	free(service_location->element);
}
