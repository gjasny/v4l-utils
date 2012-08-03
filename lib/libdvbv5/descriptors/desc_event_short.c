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

#include "descriptors/desc_event_short.h"
#include "descriptors.h"
#include "dvb-fe.h"
#include "parse_string.h"

ssize_t dvb_desc_event_short_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_event_short *event = (struct dvb_desc_event_short *) desc;
	char *string, *emph;
	uint8_t len;        /* the length of the string in the input data */
	uint8_t len1, len2; /* the lenght of the output strings */

	/*hexdump(parms, "event short desc: ", buf - 2, desc->length + 2);*/

	event->language[0] = buf[0];
	event->language[1] = buf[1];
	event->language[2] = buf[2];
	event->language[3] = '\0';
	buf += 3;

	event->name = ((char *) desc) + sizeof(struct dvb_desc_event_short);
	len = buf[0];
	buf++;
	len1 = len;
	string = NULL;
	emph   = NULL;
	parse_string(parms, &string, &emph, buf, len1, default_charset, output_charset);
	buf += len;
	if (emph)
		free(emph);
	if (string) {
		len1 = strlen(string);
		memcpy(event->name, string, len1);
		free(string);
	} else {
		memcpy(event->name, buf, len1);
	}
	event->name[len1] = '\0';

	event->text = event->name + len1 + 1;
	len = buf[0];
	len2 = len;
	buf++;
	string = NULL;
	emph   = NULL;
	parse_string(parms, &string, &emph, buf, len2, default_charset, output_charset);
	buf += len;
	if (emph)
		free(emph);
	if (string) {
		len2 = strlen(string);
		memcpy(event->text, string, len2);
		free(string);
	} else {
		memcpy(event->text, buf, len2);
	}
	event->text[len2] = '\0';

	return sizeof(struct dvb_desc_event_short) + len1 + 1 + len2 + 1;
}

void dvb_desc_event_short_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_event_short *event = (const struct dvb_desc_event_short *) desc;
	dvb_log("|   Event         '%s'", event->name);
	dvb_log("|   Description   '%s'", event->text);
}

