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

#ifndef _ATSC_HEADER_H
#define _ATSC_HEADER_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#include <libdvbv5/header.h>

#define ATSC_BASE_PID  0x1FFB

#define ATSC_HEADER() \
	struct dvb_table_header header; \
	uint8_t  protocol_version; \

#define ATSC_TABLE_HEADER_PRINT(_parms, _table) \
	dvb_table_header_print(_parms, &_table->header); \
	dvb_loginfo("| protocol_version %d", _table->protocol_version); \


#endif
