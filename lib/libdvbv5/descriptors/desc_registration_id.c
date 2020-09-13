/*
 * Copyright (c) 2020 - Mauro Carvalho Chehab
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

#include <libdvbv5/desc_registration_id.h>
#include <libdvbv5/dvb-fe.h>

#if __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif

int dvb_desc_registration_init(struct dvb_v5_fe_parms *parms,
			       const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_registration *d = (struct dvb_desc_registration *) desc;
	size_t size = sizeof(d->format_identifier);

	if (desc->length < size) {
		dvb_logerr("dvb_desc_registration_init short read %d/%zd bytes", desc->length, size);
		return -1;
	}

	memcpy(&d->format_identifier, buf, size);

	if (desc->length <= size)
		return 0;

	d->additional_identification_info = malloc(desc->length - size);
	memcpy(desc->data, buf + size, desc->length - size);

	return 0;
}

void dvb_desc_registration_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_registration *d = (const struct dvb_desc_registration *) desc;
	int i = 0;

	dvb_loginfo("|           format_identifier     : %c%c%c%c",
		    d->format_identifier[0], d->format_identifier[1],
		    d->format_identifier[2], d->format_identifier[3]);

	if (!d->additional_identification_info)
		return;

	for (i = 0; i < d->length - 4; i++) {
		dvb_loginfo("|           aditional_id_info[%d] : %02x",
			    i, d->additional_identification_info[i]);
	}
}

void dvb_desc_registration_free(struct dvb_desc *desc)
{
	const struct dvb_desc_registration *d = (const void *) desc;

	free(d->additional_identification_info);
}
