/*
 * Copyright (c) 2013 - Mauro Carvalho Chehab <m.chehab@samsung.com>
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

#include "descriptors.h"
#include "descriptors/desc_atsc_service_location.h"
#include "dvb-fe.h"

void atsc_desc_service_location_init(struct dvb_v5_fe_parms *parms,
				     const uint8_t *buf, struct dvb_desc *desc)
{
	struct atsc_desc_service_location *s_loc = (struct atsc_desc_service_location *)desc;
	struct atsc_desc_service_location_elementary *el;
	unsigned char *p = (unsigned char *)buf;
	int i;
	size_t len;

	len = sizeof(*s_loc) - offsetof(struct atsc_desc_service_location, bitfield);
	memcpy(&s_loc->bitfield, p, len);
	p += len;

	bswap16(s_loc->bitfield);

	if (s_loc->number_elements) {
		s_loc->elementary = malloc(s_loc->number_elements * sizeof(*s_loc->elementary));
		if (!s_loc->elementary)
			dvb_perror("Can't allocate space for ATSC service location elementary data");

		el = s_loc->elementary;

		for (i = 0; i < s_loc->number_elements; i++) {
			memcpy(el, p, sizeof(*el));
			bswap16(el->bitfield);

			el++;
			p += sizeof(*el);
		}
	} else {
		s_loc->elementary = NULL;
	}
}

void atsc_desc_service_location_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct atsc_desc_service_location *s_loc = (const struct atsc_desc_service_location *) desc;
	struct atsc_desc_service_location_elementary *el = s_loc->elementary;
	int i;

	dvb_log("|       service location");
	dvb_log("|           pcr PID               %d", s_loc->pcr_pid);
	dvb_log("|\\ elementary service - %d elementaries", s_loc->number_elements);
	for (i = 0; i < s_loc->number_elements; i++) {
		dvb_log("|-  elementary %d", i);
		dvb_log("|-      | stream type 0x%02x", el[i].stream_type);
		dvb_log("|-      | PID         %d", el[i].elementary_pid);
		dvb_log("|-      | Language    %c%c%c",
			el[i].ISO_639_language_code[0],
			el[i].ISO_639_language_code[1],
			el[i].ISO_639_language_code[2]);
	}
}

void atsc_desc_service_location_free(struct dvb_desc *desc)
{
	const struct atsc_desc_service_location *s_loc = (const struct atsc_desc_service_location *) desc;

	if (s_loc->elementary)
		free(s_loc->elementary);
}