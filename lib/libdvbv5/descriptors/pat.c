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

void *dvb_table_pat_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t size)
{
	struct dvb_table_pat *pat = malloc(size + sizeof(uint16_t));
	memcpy(pat, buf, sizeof(struct dvb_table_pat) - sizeof(uint16_t));

	dvb_table_header_init(&pat->header);

	struct dvb_table_pat_program *p = (struct dvb_table_pat_program *)
		                          (buf + sizeof(struct dvb_table_pat) - sizeof(uint16_t));
	int i = 0;
	while ((uint8_t *) p < buf + size - 4) {
		memcpy(pat->program + i, p, sizeof(struct dvb_table_pat_program));
		bswap16(pat->program[i].program_id);
		bswap16(pat->program[i].bitfield);
		p++;
		i++;
	}
	pat->programs = i;
	return pat;
}

void dvb_table_pat_print(struct dvb_v5_fe_parms *parms, struct dvb_table_pat *t)
{
	dvb_log("PAT");
	dvb_table_header_print(parms, &t->header);
	dvb_log("|\\   pid     program_id (%d programs)", t->programs);
	int i;
	for (i = 0; i < t->programs; i++) {
		dvb_log("|- %7d %7d", t->program[i].pid, t->program[i].program_id);
	}
}

