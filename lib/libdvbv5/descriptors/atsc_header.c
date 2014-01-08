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

#include "descriptors/atsc_header.h"
#include "descriptors.h"
#include "dvb-fe.h"

int atsc_table_header_init(struct atsc_table_header *t)
{
	bswap16(t->bitfield);
	bswap16(t->id);
	return 0;
}

void atsc_table_header_print(struct dvb_v5_fe_parms *parms, const struct atsc_table_header *t)
{
	dvb_log("| table_id         %02x", t->table_id);
	dvb_log("| section_length   %d", t->section_length);
	dvb_log("| syntax           %d", t->syntax);
	dvb_log("| priv             %d", t->priv);
	dvb_log("| one              %d", t->one);
	dvb_log("| id               %d", t->id);
	dvb_log("| one2             %d", t->one2);
	dvb_log("| version          %d", t->version);
	dvb_log("| current_next     %d", t->current_next);
	dvb_log("| section_id       %d", t->section_id);
	dvb_log("| last_section     %d", t->last_section);
	dvb_log("| protocol_version %d", t->protocol_version);
}

