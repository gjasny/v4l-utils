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

#include <libdvbv5/desc_event_short.h>
#include <libdvbv5/dvb-fe.h>
#include <parse_string.h>

int dvb_desc_event_short_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_event_short *event = (struct dvb_desc_event_short *) desc;
	uint8_t len;        /* the length of the string in the input data */
	uint8_t len1, len2; /* the lenght of the output strings */

	/*dvb_hexdump(parms, "event short desc: ", buf - 2, desc->length + 2);*/

	event->language[0] = buf[0];
	event->language[1] = buf[1];
	event->language[2] = buf[2];
	event->language[3] = '\0';
	buf += 3;

	event->name = NULL;
	event->name_emph = NULL;
	len = buf[0];
	buf++;
	len1 = len;
	dvb_parse_string(parms, &event->name, &event->name_emph, buf, len1);
	buf += len;

	event->text = NULL;
	event->text_emph = NULL;
	len = buf[0];
	len2 = len;
	buf++;
	dvb_parse_string(parms, &event->text, &event->text_emph, buf, len2);
	buf += len;
	return 0;
}

void dvb_desc_event_short_free(struct dvb_desc *desc)
{
	struct dvb_desc_event_short *event = (struct dvb_desc_event_short *) desc;
	free(event->name);
	free(event->name_emph);
	free(event->text);
	free(event->text_emph);
}

void dvb_desc_event_short_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_event_short *event = (const struct dvb_desc_event_short *) desc;
	dvb_loginfo("|           name          '%s'", event->name);
	dvb_loginfo("|           language      '%s'", event->language);
	dvb_loginfo("|           description   '%s'", event->text);
}

