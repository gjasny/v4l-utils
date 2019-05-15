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

#include <libdvbv5/header.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>

#if __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif

void dvb_table_header_init(struct dvb_table_header *t)
{
	bswap16(t->bitfield);
	bswap16(t->id);
}

void dvb_table_header_print(struct dvb_v5_fe_parms *parms, const struct dvb_table_header *t)
{
	dvb_loginfo("| table_id         0x%02x", t->table_id);
	dvb_loginfo("| section_length      %d", t->section_length);
	dvb_loginfo("| one                 %d", t->one);
	dvb_loginfo("| zero                %d", t->zero);
	dvb_loginfo("| syntax              %d", t->syntax);
	dvb_loginfo("| transport_stream_id %d", t->id);
	dvb_loginfo("| current_next        %d", t->current_next);
	dvb_loginfo("| version             %d", t->version);
	dvb_loginfo("| one2                %d", t->one2);
	dvb_loginfo("| section_number      %d", t->section_id);
	dvb_loginfo("| last_section_number %d", t->last_section);
}

