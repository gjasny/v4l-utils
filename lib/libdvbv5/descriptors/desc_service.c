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
#include "parse_string.h"

void dvb_desc_service_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_service *service = (struct dvb_desc_service *) desc;
	uint8_t len;        /* the length of the string in the input data */
	uint8_t len1, len2; /* the lenght of the output strings */

        /*hexdump(parms, "service desc: ", buf - 2, desc->length + 2);*/
	service->service_type = buf[0];
	buf++;

	service->provider = NULL;
	service->provider_emph = NULL;
	len = buf[0];
	buf++;
	len1 = len;
	parse_string(parms, &service->provider, &service->provider_emph, buf, len1, default_charset, output_charset);
	buf += len;

	service->name = NULL;
	service->name_emph = NULL;
	len = buf[0];
	len2 = len;
	buf++;
	parse_string(parms, &service->name, &service->name_emph, buf, len2, default_charset, output_charset);
	buf += len;
}

void dvb_desc_service_free(struct dvb_desc *desc)
{
	struct dvb_desc_service *service = (struct dvb_desc_service *) desc;
	free(service->provider);
	free(service->provider_emph);
	free(service->name);
	free(service->name_emph);
}

void dvb_desc_service_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_service *service = (const struct dvb_desc_service *) desc;
	dvb_log("|   service type     %d", service->service_type);
	dvb_log("|           name     '%s'", service->name);
	dvb_log("|           provider '%s'", service->provider);
}

