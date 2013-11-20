/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab <mchehab@redhat.com>
 * Copyright (c) 2012 - Andre Roth <neolynx@gmail.com>
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

#include "descriptors/pat.h"
#include "descriptors.h"
#include "dvb-fe.h"

void dvb_table_pat_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, uint8_t *table, ssize_t *table_length)
{
	struct dvb_table_pat *pat = (struct dvb_table_pat *) table;
	const uint8_t *p = buf, *endbuf = buf + buflen - 4;
	size_t size;

	if (*table_length > 0) {
		dvb_logerr("multisection PAT table not implemented");
		return;
	}

	size = offsetof(struct dvb_table_pat, programs);
	if (p + size > endbuf) {
		dvb_logerr("PAT table was truncated. Need %zu bytes, but has only %zu.",
				size, buflen);
		return;
	}
	memcpy(table, buf, size);
	p += size;
	pat->programs = 0;

	*table_length = buflen + sizeof(uint16_t);

	size = sizeof(struct dvb_table_pat_program);
	while (p + size <= endbuf) {
		memcpy(pat->program + pat->programs, p, size);
		bswap16(pat->program[pat->programs].service_id);
		bswap16(pat->program[pat->programs].bitfield);
		p += size;
		pat->programs++;
	}
	if (endbuf - p)
		dvb_logerr("PAT table has %zu spurious bytes at the end.",
			   endbuf - p);
}

void dvb_table_pat_free(struct dvb_table_pat *pat)
{
	free(pat);
}

void dvb_table_pat_print(struct dvb_v5_fe_parms *parms, struct dvb_table_pat *pat)
{
	dvb_log("PAT");
	dvb_table_header_print(parms, &pat->header);
	dvb_log("|\\  program  service (%d programs)", pat->programs);
	int i;
	for (i = 0; i < pat->programs; i++) {
		dvb_log("|- %7d %7d", pat->program[i].pid, pat->program[i].service_id);
	}
}

