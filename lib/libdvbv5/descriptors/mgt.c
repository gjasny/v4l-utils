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

#include <libdvbv5/mgt.h>
#include <libdvbv5/dvb-fe.h>

void atsc_table_mgt_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, uint8_t *table, ssize_t *table_length)
{
	const uint8_t *p = buf, *endbuf = buf + buflen - 4; /* minus CRC */
	struct atsc_table_mgt *mgt = (struct atsc_table_mgt *) table;
	struct dvb_desc **head_desc;
	struct atsc_table_mgt_table **head;
	int i = 0;
	struct atsc_table_mgt_table *last = NULL;
	size_t size = offsetof(struct atsc_table_mgt, table);

	if (p + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   size, endbuf - p);
		return;
	}

	if (*table_length > 0) {
		/* find end of curent lists */
		head_desc = &mgt->descriptor;
		while (*head_desc != NULL)
			head_desc = &(*head_desc)->next;
		head = &mgt->table;
		while (*head != NULL)
			head = &(*head)->next;

		/* FIXME: read current mgt->tables for loop below */
	} else {
		memcpy(table, p, size);
		*table_length = sizeof(struct atsc_table_mgt);

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
				   size, endbuf - p);
			return;
		}
		table = (struct atsc_table_mgt_table *) malloc(sizeof(struct atsc_table_mgt_table));
		memcpy(table, p, size);
		p += size;

		bswap16(table->type);
		bswap16(table->bitfield);
		bswap16(table->bitfield2);
		bswap32(table->size);
		table->descriptor = NULL;
		table->next = NULL;

		if(!*head)
			*head = table;
		if(last)
			last->next = table;

		/* get the descriptors for each table */
		size = table->desc_length;
		if (p + size > endbuf) {
			dvb_logerr("%s: short read %zd/%zd bytes", __func__,
				   size, endbuf - p);
			return;
		}
		dvb_parse_descriptors(parms, p, size, &table->descriptor);

		p += size;
		last = table;
	}
	/* TODO: parse MGT descriptors here into head_desc */
}

void atsc_table_mgt_free(struct atsc_table_mgt *mgt)
{
	struct atsc_table_mgt_table *table = mgt->table;

	dvb_free_descriptors((struct dvb_desc **) &mgt->descriptor);
	while(table) {
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
	atsc_table_header_print(parms, &mgt->header);
	dvb_log("| tables           %d", mgt->tables);
	while(table) {
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

