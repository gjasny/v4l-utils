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

#include "descriptors/desc_network_name.h"
#include "descriptors.h"
#include "dvb-fe.h"
#include "parse_string.h"

ssize_t dvb_desc_network_name_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_network_name *net = (struct dvb_desc_network_name *) desc;
	char *name, *emph;
	uint8_t len;  /* the length of the string in the input data */
	uint8_t len1; /* the lenght of the output strings */

	len = desc->length;
	len1 = len;
	name = NULL;
	emph = NULL;
	parse_string(parms, &name, &emph, buf, len1, default_charset, output_charset);
	buf += len;
	if (emph)
		free(emph);
	if (name) {
		len1 = strlen(name);
		memcpy(net->network_name, name, len1);
		free(name);
	} else {
		memcpy(net->network_name, buf, len1);
	}
	net->network_name[len1] = '\0';

	return sizeof(struct dvb_desc_network_name) + len1 + 1;
}

void dvb_desc_network_name_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_network_name *net = (const struct dvb_desc_network_name *) desc;
	dvb_log("|           network name: '%s'", net->network_name);
}

