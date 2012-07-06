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

#include "descriptors/desc_service.h"
#include "descriptors.h"
#include "dvb-fe.h"

ssize_t dvb_desc_service_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_service *service = (struct dvb_desc_service *) desc;

	service->service_type = buf[0];
	buf++;

	service->provider = ((char *) desc) + sizeof(struct dvb_desc_service);
	uint8_t len1 = buf[0];
	buf++;
	memcpy(service->provider, buf, len1);
	service->provider[len1] = '\0';
	buf += len1;

	service->name = service->provider + len1 + 1;
	uint8_t len2 = buf[0];
	buf++;
	memcpy(service->name, buf, len2);
	service->name[len2] = '\0';

	return sizeof(struct dvb_desc_service) + len1 + 1 + len2 + 1;
}

void dvb_desc_service_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_service *srv = (const struct dvb_desc_service *) desc;
	dvb_log("|           type    : '%d'", srv->service_type);
	dvb_log("|           name    : '%s'", srv->name);
	dvb_log("|           provider: '%s'", srv->provider);
}

