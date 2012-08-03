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

ssize_t dvb_desc_service_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_service *service = (struct dvb_desc_service *) desc;
	char *name, *emph;
	uint8_t len;        /* the length of the string in the input data */
	uint8_t len1, len2; /* the lenght of the output strings */

	/*hexdump(parms, "service desc: ", buf - 2, desc->length + 2);*/
	service->service_type = buf[0];
	buf++;

	service->provider = ((char *) desc) + sizeof(struct dvb_desc_service);
	len = buf[0];
	buf++;
	len1 = len;
	name = NULL;
	emph = NULL;
	parse_string(parms, &name, &emph, buf, len1, default_charset, output_charset);
	buf += len;
	if (emph)
		free(emph);
	if (name) {
		len1 = strlen(name);
		memcpy(service->provider, name, len1);
		free(name);
	} else {
		memcpy(service->provider, buf, len1);
	}
	service->provider[len1] = '\0';

	service->name = service->provider + len1 + 1;
	len = buf[0];
	len2 = len;
	buf++;
	name = NULL;
	emph = NULL;
	parse_string(parms, &name, &emph, buf, len2, default_charset, output_charset);
	buf += len;
	if (emph)
		free(emph);
	if (name) {
		len2 = strlen(name);
		memcpy(service->name, name, len2);
		free(name);
	} else {
		memcpy(service->name, buf, len2);
	}
	service->name[len2] = '\0';

	return sizeof(struct dvb_desc_service) + len1 + 1 + len2 + 1;
}

void dvb_desc_service_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_service *service = (const struct dvb_desc_service *) desc;
	dvb_log("|   service type     %d", service->service_type);
	dvb_log("|           name     '%s'", service->name);
	dvb_log("|           provider '%s'", service->provider);
}

