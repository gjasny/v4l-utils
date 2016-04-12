/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 * Copyright (c) 2012 - Andre Roth <neolynx@gmail.com>
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

#include <libdvbv5/desc_hierarchy.h>
#include <libdvbv5/dvb-fe.h>

int dvb_desc_hierarchy_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_hierarchy *hierarchy = (struct dvb_desc_hierarchy *) desc;
	/* copy from .length */
	memcpy(((uint8_t *) hierarchy ) + sizeof(hierarchy->type) + sizeof(hierarchy->length) + sizeof(hierarchy->next),
		buf,
		hierarchy->length);
	return 0;
}

void dvb_desc_hierarchy_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_hierarchy *hierarchy = (const struct dvb_desc_hierarchy *) desc;
	dvb_loginfo("|           type           %d", hierarchy->hierarchy_type);
	dvb_loginfo("|           layer          %d", hierarchy->layer);
	dvb_loginfo("|           embedded_layer %d", hierarchy->embedded_layer);
	dvb_loginfo("|           channel        %d", hierarchy->channel);
}

