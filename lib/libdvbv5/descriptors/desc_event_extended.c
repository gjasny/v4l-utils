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

#include <libdvbv5/desc_event_extended.h>
#include <libdvbv5/dvb-fe.h>
#include <parse_string.h>

#ifdef ENABLE_NLS
# include "gettext.h"
# include <libintl.h>
# define _(string) dgettext(LIBDVBV5_DOMAIN, string)
#else
# define _(string) string
#endif

int dvb_desc_event_extended_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_event_extended *event = (struct dvb_desc_event_extended *) desc;
	uint8_t len, size_items;
	const uint8_t *buf_start;
	int first = 1;
	struct dvb_desc_event_extended_item *item;

	event->ids = buf[0];

	event->language[0] = buf[1];
	event->language[1] = buf[2];
	event->language[2] = buf[3];
	event->language[3] = '\0';

	size_items = buf[4];
	buf += 5;

	event->items = NULL;
	event->num_items = 0;
	buf_start = buf;
	while (buf - buf_start < size_items) {
		if (first) {
			first = 0;
			event->num_items = 1;
			event->items = calloc(sizeof(struct dvb_desc_event_extended_item), event->num_items);
			if (!event->items) {
				dvb_logerr(_("%s: out of memory"), __func__);
				return -1;
			}
			item = event->items;
		} else {
			event->num_items++;
			event->items = realloc(event->items, sizeof(struct dvb_desc_event_extended_item) * (event->num_items));
			item = event->items + (event->num_items - 1);
		}
		len = *buf;
		buf++;
		item->description = NULL;
		item->description_emph = NULL;
		dvb_parse_string(parms, &item->description, &item->description_emph, buf, len);
		buf += len;

		len = *buf;
		buf++;
		item->item = NULL;
		item->item_emph = NULL;
		dvb_parse_string(parms, &item->item, &item->item_emph, buf, len);
		buf += len;
	}


	len = *buf;
	buf++;

	if (len) {
		event->text = NULL;
		event->text_emph = NULL;
		dvb_parse_string(parms, &event->text, &event->text_emph, buf, len);
		buf += len;
	}

	return 0;
}

void dvb_desc_event_extended_free(struct dvb_desc *desc)
{
	struct dvb_desc_event_extended *event = (struct dvb_desc_event_extended *) desc;
	int i;
	free(event->text);
	free(event->text_emph);
	for (i = 0; i < event->num_items; i++) {
		free(event->items[i].description);
		free(event->items[i].description_emph);
		free(event->items[i].item);
		free(event->items[i].item_emph);
	}
	free(event->items);
}

void dvb_desc_event_extended_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_event_extended *event = (const struct dvb_desc_event_extended *) desc;
	int i;
	dvb_loginfo("|           '%s'", event->text);
	for (i = 0; i < event->num_items; i++) {
		dvb_loginfo("|              description   '%s'", event->items[i].description);
		dvb_loginfo("|              item          '%s'", event->items[i].item);
	}
}

