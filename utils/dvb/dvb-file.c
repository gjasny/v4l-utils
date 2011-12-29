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

static const char *parm_name(const struct parse_table *table)
{
	if (table->prop < DTV_MAX_COMMAND)
		return dvb_v5_name[table->prop];
	switch (table->prop) {
	case DTV_CH_NAME:
		return ("CHANNEL");
	case DTV_POLARIZATION:
		return ("POLARIZATION");
	case DTV_VIDEO_PID:
		return ("VIDEO PID");
	case DTV_AUDIO_PID:
		return ("AUDIO PID");
	case DTV_SERVICE_PID:
		return ("SERVICE ID");
	default:
		return ("unknown");
	}
}

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
		if (len <= 0)
			break;
		line++;

		p = buf;
		while (*p == ' ')
			p++;
		if (*p == '\n' || *p == '#' || *p == '\a' || *p == '\0')
			continue;

		if (!delsys) {
			p = strtok(p, delimiter);
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
			if (delsys && !i) {
				p = strtok(p, delimiter);
			} else
				p = strtok(NULL, delimiter);
			if (!p) {
				sprintf(err_msg, "parameter %i (%s) missing",
					i, parm_name(table));
				goto error;
			}
			if (table->size) {
				for (j = 0; j < table->size; j++)
					if (!strcasecmp(table->table[j], p))
						break;
				if (j == table->size) {
					sprintf(err_msg, "parameter %s invalid: %s",
						parm_name(table), p);
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
	fprintf (stderr, "ERROR %s while parsing line %d of %s\n",
		 err_msg, line, fname);
	dvb_file_free(dvb_file);
	fclose(fd);
	return NULL;
}

static const char *pol_name[] = {
	[POLARIZATION_H] = "HORIZONTAL",
	[POLARIZATION_V] = "VERTICAL",
	[POLARIZATION_L] = "LEFT",
	[POLARIZATION_R] = "RIGHT",
};

int write_dvb_file(const char *fname, struct dvb_file *dvb_file)
{
	FILE *fp;
	int i;
	struct dvb_entry *entry = dvb_file->first_entry;

	fp = fopen(fname, "w");
	if (!fp) {
		perror(fname);
		return -errno;
	}

	for (entry = dvb_file->first_entry; entry != NULL; entry = entry->next) {
		if (entry->channel) {
			fprintf(fp, "[%s]\n", entry->channel);
			fprintf(fp, "\tVIDEO_PID = %d\n", entry->video_pid);
			fprintf(fp, "\tAUDIO_PID = %d\n", entry->audio_pid);
			fprintf(fp, "\tSERVICE_ID = %d\n", entry->service_pid);
			if (entry->pol != POLARIZATION_OFF) {
				fprintf(fp, "\tPOLARIZATION = %s\n",
					pol_name[entry->pol]);
			}
		} else {
			fprintf(fp, "[CHANNEL]\n");
		}
		for (i = 0; i < entry->n_props; i++) {
			const char * const *attr_name = dvbv5_attr_names[entry->props[i].cmd];
			if (attr_name) {
				int j;

				for (j = 0; j < entry->props[i].u.data; j++) {
					if (!*attr_name)
						break;
					attr_name++;
				}
			}

			if (!attr_name || !*attr_name)
				fprintf(fp, "\t%s = %u\n",
					dvb_v5_name[entry->props[i].cmd],
					entry->props[i].u.data);
			else
				fprintf(fp, "\t%s = %s\n",
					dvb_v5_name[entry->props[i].cmd],
					*attr_name);
		}
		fprintf(fp, "\n");
	}
	fclose(fp);
	return 0;
};
