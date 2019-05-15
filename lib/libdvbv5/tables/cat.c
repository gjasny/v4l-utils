/*
 * Copyright (c) 2013-2014 - Andre Roth <neolynx@gmail.com>
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

#include <libdvbv5/cat.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>

#if __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif

ssize_t dvb_table_cat_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
		ssize_t buflen, struct dvb_table_cat **table)
{
	const uint8_t *p = buf, *endbuf = buf + buflen;
	struct dvb_table_cat *cat;
	struct dvb_desc **head_desc;
	size_t size;

	size = offsetof(struct dvb_table_cat, descriptor);
	if (p + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - p, size);
		return -1;
	}

	if (buf[0] != DVB_TABLE_CAT) {
		dvb_logerr("%s: invalid marker 0x%02x, should be 0x%02x",
				__func__, buf[0], DVB_TABLE_CAT);
		return -2;
	}

	if (!*table) {
		*table = calloc(sizeof(struct dvb_table_cat), 1);
		if (!*table) {
			dvb_logerr("%s: out of memory", __func__);
			return -3;
		}
	}
	cat = *table;
	memcpy(cat, p, size);
	p += size;
	dvb_table_header_init(&cat->header);

	/* find end of current lists */
	head_desc = &cat->descriptor;
	while (*head_desc != NULL)
		head_desc = &(*head_desc)->next;

	size = cat->header.section_length + 3 - DVB_CRC_SIZE; /* plus header, minus CRC */
	if (buf + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - buf, size);
		return -4;
	}
	endbuf = buf + size;

	/* parse the descriptors */
	if (endbuf > p) {
		uint16_t desc_length = endbuf - p;
		if (dvb_desc_parse(parms, p, desc_length,
				      head_desc) != 0) {
			return -5;
		}
		p += desc_length;
	}

	if (endbuf - p)
		dvb_logwarn("%s: %zu spurious bytes at the end",
			   __func__, endbuf - p);

	return p - buf;
}

void dvb_table_cat_free(struct dvb_table_cat *cat)
{
	dvb_desc_free((struct dvb_desc **) &cat->descriptor);
	free(cat);
}

void dvb_table_cat_print(struct dvb_v5_fe_parms *parms, struct dvb_table_cat *cat)
{
	dvb_loginfo("CAT");
	dvb_table_header_print(parms, &cat->header);
	dvb_desc_print(parms, cat->descriptor);
}

