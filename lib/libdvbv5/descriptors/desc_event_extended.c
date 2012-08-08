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

#include "descriptors/desc_event_extended.h"
#include "descriptors.h"
#include "dvb-fe.h"
#include "parse_string.h"

void dvb_desc_event_extended_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_event_extended *event = (struct dvb_desc_event_extended *) desc;
	uint8_t len;  /* the length of the string in the input data */
	uint8_t len1; /* the lenght of the output strings */

	/*hexdump(parms, "event extended desc: ", buf - 2, desc->length + 2);*/

	event->ids = buf[0];
	event->language[0] = buf[1];
	event->language[1] = buf[2];
	event->language[2] = buf[3];
	event->language[3] = '\0';

	uint8_t items = buf[4];
	buf += 5;

	int i;
	for (i = 0; i < items; i++) {
		dvb_logwarn("dvb_desc_event_extended: items not implemented");
		uint8_t desc_len = *buf;
		buf++;

		buf += desc_len;

		uint8_t item_len = *buf;
		buf++;

		buf += item_len;
	}

	event->text = NULL;
	event->text_emph = NULL;
	len = *buf;
	len1 = len;
	buf++;
	parse_string(parms, &event->text, &event->text_emph, buf, len1, default_charset, output_charset);
	buf += len;
}

void dvb_desc_event_extended_free(struct dvb_desc *desc)
{
	struct dvb_desc_event_extended *event = (struct dvb_desc_event_extended *) desc;
	free(event->text);
	free(event->text_emph);
}

void dvb_desc_event_extended_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_event_extended *event = (const struct dvb_desc_event_extended *) desc;
	dvb_log("|   Description   '%s'", event->text);
}

