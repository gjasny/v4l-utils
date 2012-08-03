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

void dvb_table_pat_init(struct dvb_v5_fe_parms *parms, const uint8_t *ptr, ssize_t size, uint8_t **buf, ssize_t *buflen)
{
	uint8_t *d;
	struct dvb_table_pat *pat;
	if (!*buf) {
		d = malloc(size + sizeof(uint16_t));
		*buf = d;
		*buflen = size + sizeof(uint16_t);
		pat = (struct dvb_table_pat *) d;
		memcpy(pat, ptr, sizeof(struct dvb_table_pat) - sizeof(uint16_t));

		struct dvb_table_pat_program *p = (struct dvb_table_pat_program *)
			                          (ptr + sizeof(struct dvb_table_pat) - sizeof(uint16_t));
		pat->programs = 0;
		while ((uint8_t *) p < ptr + size - 4) {
			memcpy(pat->program + pat->programs, p, sizeof(struct dvb_table_pat_program));
			bswap16(pat->program[pat->programs].service_id);
			bswap16(pat->program[pat->programs].bitfield);
			p++;
			pat->programs++;
		}
	} else {
		dvb_logerr("multisection PAT table not implemented");
	}
}

void dvb_table_pat_print(struct dvb_v5_fe_parms *parms, struct dvb_table_pat *t)
{
	dvb_log("PAT");
	dvb_table_header_print(parms, &t->header);
	dvb_log("|\\  program  service (%d programs)", t->programs);
	int i;
	for (i = 0; i < t->programs; i++) {
		dvb_log("|- %7d %7d", t->program[i].pid, t->program[i].service_id);
	}
}

