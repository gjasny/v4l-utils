/*
 * Copyright (c) 2011 - Mauro Carvalho Chehab <mchehab@redhat.com>
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dvb-file.h"

/*
 * Generic parse function for all formats each channel is contained into
 * just one line.
 */
struct dvb_file *parse_format_oneline(const char *fname, const char *delimiter,
				      uint32_t delsys,
				      const struct parse_struct *formats)
{
	char *buf = NULL, *p;
	size_t size = 0;
	int len = 0;
	int i, j, line = 0;
	struct dvb_file *dvb_file;
	FILE *fd;
	const struct parse_struct *fmt;
	struct dvb_entry *entry = NULL;
	const struct parse_table *table;
	char err_msg[80];
	int has_inversion;

	dvb_file = calloc(sizeof(*dvb_file), 1);
	if (!dvb_file) {
		perror("Allocating memory for dvb_file");
		return NULL;
	}

	fd = fopen(fname, "r");
	if (!fd) {
		perror(fname);
		return NULL;
	}

	do {
		len = getline(&buf, &size, fd);
		if (!len)
			break;
		line++;

		if (!delsys) {
			p = strtok(buf, delimiter);
			if (!p) {
				sprintf(err_msg, "unknown delivery system type for %s",
					p);
				goto error;
			}

			/* Parse the type of the delivery system */
			for (i = 0; formats[i].id != NULL; i++) {
				if (!strcmp(p, formats[i].id))
					break;
			}
		} else {
			/* Seek for the delivery system */
			for (i = 0; formats[i].id != NULL; i++) {
				if (formats[i].delsys == delsys)
					break;
			}
		}
		if (i == ARRAY_SIZE(formats))
			goto error;

		fmt = &formats[i];
		if (!entry) {
			dvb_file->first_entry = calloc(sizeof(*entry), 1);
			entry = dvb_file->first_entry;
		} else {
			entry->next = calloc(sizeof(*entry), 1);
			entry = entry->next;
		}
		entry->props[entry->n_props].cmd = DTV_DELIVERY_SYSTEM;
		entry->props[entry->n_props++].u.data = fmt->delsys;
		has_inversion = 0;
		for (i = 0; i < fmt->size; i++) {
			table = &fmt->table[i];
			p = strtok(NULL, delimiter);
			if (!p) {
				sprintf(err_msg, "parameter %s missing",
					dvb_v5_name[table->prop]);
				goto error;
			}
			if (table->size) {
				for (j = 0; j < table->size; j++)
					if (!strcasecmp(table->table[j], p))
						break;
				if (j == table->size) {
					sprintf(err_msg, "parameter %s invalid: %s",
						dvb_v5_name[table->prop], p);
					goto error;
				}
				if (table->prop == DTV_BANDWIDTH_HZ)
					j = fe_bandwidth_name[j];
				if (table->prop == DTV_POLARIZATION) {
					entry->pol = j;
				} else {
					entry->props[entry->n_props].cmd = table->prop;
					entry->props[entry->n_props++].u.data = j;
				}
			} else {
				long v = atol(p);
				if (table->mult_factor)
					v *= table->mult_factor;

				switch (table->prop) {
				case DTV_VIDEO_PID:
					entry->video_pid = v;
					break;
				case DTV_AUDIO_PID:
					entry->audio_pid = v;
					break;
				case DTV_SERVICE_PID:
					entry->service_pid = v;
					break;
				case DTV_CH_NAME:
					entry->channel = calloc(strlen(p) + 1, 1);
					strcpy(entry->channel, p);
					break;
				default:
					entry->props[entry->n_props].cmd = table->prop;
					entry->props[entry->n_props++].u.data = v;
				}
			}
			if (table->prop == DTV_INVERSION)
				has_inversion = 1;
		}
		if (!has_inversion) {
			entry->props[entry->n_props].cmd = DTV_INVERSION;
			entry->props[entry->n_props++].u.data = INVERSION_AUTO;
		}

	} while (1);
	fclose (fd);
	return dvb_file;

error:
	fprintf (stderr, "Error parsing line %d of %s\n", line, fname);
	dvb_file_free(dvb_file);
	fclose(fd);
	return NULL;
}
