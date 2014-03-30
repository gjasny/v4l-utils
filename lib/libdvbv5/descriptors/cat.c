/*
 * Copyright (c) 2013 - Andre Roth <neolynx@gmail.com>
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

#include <libdvbv5/cat.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>

void dvb_table_cat_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			ssize_t buflen, uint8_t *table, ssize_t *table_length)
{
	struct dvb_table_cat *cat = (void *)table;
	struct dvb_desc **head_desc = &cat->descriptor;
	const uint8_t *p = buf, *endbuf = buf + buflen - 4;
	size_t size;

	if (buf[0] != DVB_TABLE_CAT) {
		dvb_logerr("%s: invalid marker 0x%02x, sould be 0x%02x", __func__, buf[0], DVB_TABLE_CAT);
		*table_length = 0;
		return;
	}

	if (*table_length > 0) {
		/* find end of current lists */
		while (*head_desc != NULL)
			head_desc = &(*head_desc)->next;
	}

	size = offsetof(struct dvb_table_cat, descriptor);
	if (p + size > endbuf) {
		dvb_logerr("CAT table was truncated while filling dvb_table_cat. Need %zu bytes, but has only %zu.",
			   size, buflen);
		return;
	}

	memcpy(table, p, size);
	p += size;
	*table_length = sizeof(struct dvb_table_cat);

	size = endbuf - p;
	dvb_parse_descriptors(parms, p, size, head_desc);
}

void dvb_table_cat_free(struct dvb_table_cat *cat)
{
	dvb_free_descriptors((struct dvb_desc **) &cat->descriptor);
	free(cat);
}

void dvb_table_cat_print(struct dvb_v5_fe_parms *parms, struct dvb_table_cat *cat)
{
	dvb_log("CAT");
	dvb_table_header_print(parms, &cat->header);
	dvb_print_descriptors(parms, cat->descriptor);
}

