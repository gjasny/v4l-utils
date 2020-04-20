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

#include <libdvbv5/mgt.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>

#if __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif

ssize_t atsc_table_mgt_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
		ssize_t buflen, struct atsc_table_mgt **table)
{
	const uint8_t *p = buf, *endbuf = buf + buflen;
	struct atsc_table_mgt *mgt;
	struct atsc_table_mgt_table **head;
	struct dvb_desc **head_desc;
	size_t size;
	int i = 0;

	size = offsetof(struct atsc_table_mgt, table);
	if (p + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - p, size);
		return -1;
	}

	if (buf[0] != ATSC_TABLE_MGT) {
		dvb_logerr("%s: invalid marker 0x%02x, should be 0x%02x",
				__func__, buf[0], ATSC_TABLE_MGT);
		return -2;
	}

	if (!*table) {
		*table = calloc(sizeof(struct atsc_table_mgt), 1);
		if (!*table) {
			dvb_logerr("%s: out of memory", __func__);
			return -3;
		}
	}
	mgt = *table;
	memcpy(mgt, p, size);
	p += size;
	dvb_table_header_init(&mgt->header);

	bswap16(mgt->tables);

	/* find end of curent lists */
	head_desc = &mgt->descriptor;
	while (*head_desc != NULL)
		head_desc = &(*head_desc)->next;
	head = &mgt->table;
	while (*head != NULL)
		head = &(*head)->next;

	while (i++ < mgt->tables && p < endbuf) {
		struct atsc_table_mgt_table *table;

		size = offsetof(struct atsc_table_mgt_table, descriptor);
		if (p + size > endbuf) {
			dvb_logerr("%s: short read %zd/%zd bytes", __func__,
				   endbuf - p, size);
			return -4;
		}
		table = (struct atsc_table_mgt_table *) malloc(sizeof(struct atsc_table_mgt_table));
		if (!table) {
			dvb_logerr("%s: out of memory", __func__);
			return -5;
		}
		memcpy(table, p, size);
		p += size;

		bswap16(table->type);
		bswap16(table->bitfield);
		bswap16(table->bitfield2);
		bswap32(table->size);
		table->descriptor = NULL;
		table->next = NULL;

		*head = table;
		head = &(*head)->next;

		/* parse the descriptors */
		size = table->desc_length;
		if (p + size > endbuf) {
			dvb_logerr("%s: short read %zd/%zd bytes", __func__,
				   endbuf - p, size);
			return -6;
		}
		if (dvb_desc_parse(parms, p, size,
					&table->descriptor) != 0) {
			return -7;
		}

		p += size;
	}

	/* TODO: parse MGT descriptors here into head_desc */

	return p - buf;
}

void atsc_table_mgt_free(struct atsc_table_mgt *mgt)
{
	struct atsc_table_mgt_table *table = mgt->table;

	dvb_desc_free(&mgt->descriptor);
	while (table) {
		struct atsc_table_mgt_table *tmp = table;

		dvb_desc_free((struct dvb_desc **) &table->descriptor);
		table = table->next;
		free(tmp);
	}
	free(mgt);
}

void atsc_table_mgt_print(struct dvb_v5_fe_parms *parms, struct atsc_table_mgt *mgt)
{
	const struct atsc_table_mgt_table *table = mgt->table;
	uint16_t tables = 0;

	dvb_loginfo("MGT");
	ATSC_TABLE_HEADER_PRINT(parms, mgt);
	dvb_loginfo("| tables           %d", mgt->tables);
	while (table) {
                dvb_loginfo("|- type %04x    %d", table->type, table->pid);
                dvb_loginfo("|  one          %d", table->one);
                dvb_loginfo("|  one2         %d", table->one2);
                dvb_loginfo("|  type version %d", table->type_version);
                dvb_loginfo("|  size         %d", table->size);
                dvb_loginfo("|  one3         %d", table->one3);
                dvb_loginfo("|  desc_length  %d", table->desc_length);
		dvb_desc_print(parms, table->descriptor);
		table = table->next;
		tables++;
	}
	dvb_loginfo("|_  %d tables", tables);
}

