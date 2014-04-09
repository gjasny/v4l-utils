/*
 * Copyright (c) 2013-2014 - Andre Roth <neolynx@gmail.com>
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

#include <libdvbv5/mgt.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>

ssize_t atsc_table_mgt_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
		ssize_t buflen, struct atsc_table_mgt *mgt, ssize_t *table_length)
{
	const uint8_t *p = buf, *endbuf = buf + buflen - 4; /* minus CRC */
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
		dvb_logerr("%s: invalid marker 0x%02x, sould be 0x%02x",
				__func__, buf[0], ATSC_TABLE_MGT);
		return -2;
	}

	if (*table_length > 0) {
		memcpy(mgt, p, size);

		bswap16(mgt->tables);

		/* find end of curent lists */
		head_desc = &mgt->descriptor;
		while (*head_desc != NULL)
			head_desc = &(*head_desc)->next;
		head = &mgt->table;
		while (*head != NULL)
			head = &(*head)->next;
	} else {
		memcpy(mgt, p, size);

		bswap16(mgt->tables);

		mgt->descriptor = NULL;
		mgt->table = NULL;
		head_desc = &mgt->descriptor;
		head = &mgt->table;
	}
	p += size;

	while (i++ < mgt->tables && p < endbuf) {
		struct atsc_table_mgt_table *table;

		size = offsetof(struct atsc_table_mgt_table, descriptor);
		if (p + size > endbuf) {
			dvb_logerr("%s: short read %zd/%zd bytes", __func__,
				   endbuf - p, size);
			return -2;
		}
		table = (struct atsc_table_mgt_table *) malloc(sizeof(struct atsc_table_mgt_table));
		if (!table) {
			dvb_logerr("%s: out of memory", __func__);
			return -3;
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

		/* get the descriptors for each table */
		size = table->desc_length;
		if (p + size > endbuf) {
			dvb_logerr("%s: short read %zd/%zd bytes", __func__,
				   endbuf - p, size);
			return -3;
		}
		dvb_parse_descriptors(parms, p, size, &table->descriptor);

		p += size;
	}

	/* TODO: parse MGT descriptors here into head_desc */

	*table_length = p - buf;
	return p - buf;
}

void atsc_table_mgt_free(struct atsc_table_mgt *mgt)
{
	struct atsc_table_mgt_table *table = mgt->table;

	dvb_free_descriptors((struct dvb_desc **) &mgt->descriptor);
	while (table) {
		struct atsc_table_mgt_table *tmp = table;

		dvb_free_descriptors((struct dvb_desc **) &table->descriptor);
		table = table->next;
		free(tmp);
	}
	free(mgt);
}

void atsc_table_mgt_print(struct dvb_v5_fe_parms *parms, struct atsc_table_mgt *mgt)
{
	const struct atsc_table_mgt_table *table = mgt->table;
	uint16_t tables = 0;

	dvb_log("MGT");
	ATSC_TABLE_HEADER_PRINT(parms, mgt);
	dvb_log("| tables           %d", mgt->tables);
	while (table) {
                dvb_log("|- type %04x    %d", table->type, table->pid);
                dvb_log("|  one          %d", table->one);
                dvb_log("|  one2         %d", table->one2);
                dvb_log("|  type version %d", table->type_version);
                dvb_log("|  size         %d", table->size);
                dvb_log("|  one3         %d", table->one3);
                dvb_log("|  desc_length  %d", table->desc_length);
		dvb_print_descriptors(parms, table->descriptor);
		table = table->next;
		tables++;
	}
	dvb_log("|_  %d tables", tables);
}

