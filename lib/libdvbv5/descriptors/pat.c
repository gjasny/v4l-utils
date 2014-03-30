/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
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

#include <libdvbv5/pat.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>

ssize_t dvb_table_pat_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			ssize_t buflen, struct dvb_table_pat *pat, ssize_t *table_length)
{
	struct dvb_table_pat_program **head = &pat->program;
	const uint8_t *p = buf, *endbuf = buf + buflen - 4;
	size_t size;

	if (*table_length > 0) {
		/* find end of current list */
		while (*head != NULL)
			head = &(*head)->next;
	} else {
		size = offsetof(struct dvb_table_pat, programs);
		if (p + size > endbuf) {
			dvb_logerr("PAT table was truncated. Need %zu bytes, but has only %zu.",
					size, buflen);
			return -1;
		}
		memcpy(pat, buf, size);
		p += size;
		pat->programs = 0;
		pat->program = NULL;
	}
	*table_length = sizeof(struct dvb_table_pat_program);

	size = offsetof(struct dvb_table_pat_program, next);
	while (p + size <= endbuf) {
		struct dvb_table_pat_program *pgm;

		pgm = malloc(sizeof(struct dvb_table_pat_program));
		if (!pgm) {
			dvb_perror("Out of memory");
			return -2;
		}

		memcpy(pgm, p, size);
		p += size;

		bswap16(pgm->service_id);
		bswap16(pgm->bitfield);
		pat->programs++;

		pgm->next = NULL;

		*head = pgm;
		head = &(*head)->next;
	}
	if (endbuf - p)
		dvb_logerr("PAT table has %zu spurious bytes at the end.",
			   endbuf - p);
	*table_length = p - buf;
	return p - buf;
}

void dvb_table_pat_free(struct dvb_table_pat *pat)
{
	struct dvb_table_pat_program *pgm = pat->program;

	while (pgm) {
		struct dvb_table_pat_program *tmp = pgm;
		pgm = pgm->next;
		free(tmp);
	}
	free(pat);
}

void dvb_table_pat_print(struct dvb_v5_fe_parms *parms, struct dvb_table_pat *pat)
{
	struct dvb_table_pat_program *pgm = pat->program;

	dvb_log("PAT");
	dvb_table_header_print(parms, &pat->header);
	dvb_log("|\\ %d program%s", pat->programs, pat->programs != 1 ? "s" : "");

	while (pgm) {
		dvb_log("|- program 0x%04x  ->  service 0x%04x", pgm->pid, pgm->service_id);
		pgm = pgm->next;
	}
}

