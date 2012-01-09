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
#include "libscan.h"

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
	case DTV_SERVICE_ID:
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
					entry->video_pid = calloc(sizeof(*entry->video_pid), 1);
					entry->video_pid_len = 1;
					entry->video_pid[0] = v;
					break;
				case DTV_AUDIO_PID:
					entry->audio_pid = calloc(sizeof(*entry->audio_pid), 1);
					entry->audio_pid_len = 1;
					entry->audio_pid[0] = v;
					break;
				case DTV_SERVICE_ID:
					entry->service_id = v;
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

#define CHANNEL "CHANNEL"

static int fill_entry(struct dvb_entry *entry, char *key, char *value)
{
	int i, j, len;
	int is_video = 0, is_audio = 0, n_prop;
	uint16_t *pid = NULL;
	char *p;

	for (i = 0; i < ARRAY_SIZE(dvb_v5_name); i++) {
		if (!dvb_v5_name[i])
			continue;
		if (!strcasecmp(key, dvb_v5_name[i]))
			break;
	}

	/* Handle the DVBv5 DTV_foo properties */
	if (i < ARRAY_SIZE(dvb_v5_name)) {
		const char * const *attr_name = dvbv5_attr_names[i];
		n_prop = entry->n_props;
		entry->props[n_prop].cmd = i;
		if (!attr_name || !*attr_name)
			entry->props[n_prop].u.data = atol(value);
		else {
			for (j = 0; attr_name[j]; j++)
				if (!strcasecmp(value, attr_name[j]))
					break;
			if (!attr_name[j])
				return -2;
			entry->props[n_prop].u.data = j;
		}
		entry->n_props++;
		return 0;
	}

	/* Handle the other properties */

	if (!strcasecmp(key, "SERVICE_ID")) {
		entry->service_id = atol(value);
		return 0;
	}

	if (!strcasecmp(key, "VCHANNEL")) {
		entry->vchannel = strdup(value);
		return 0;
	}

	if (!strcasecmp(key, "SAT_NUMBER")) {
		entry->sat_number = atol(value);
		return 0;
	}

	if (!strcasecmp(key, "DISEQC_WAIT")) {
		entry->diseqc_wait = atol(value);
		return 0;
	}

	if (!strcasecmp(key, "LNB")) {
		entry->lnb = strdup(value);
		return 0;
	}

	if (!strcasecmp(key, "VIDEO_PID"))
		is_video = 1;
	else 	if (!strcasecmp(key, "AUDIO_PID"))
		is_audio = 1;
	else if (!strcasecmp(key, "POLARIZATION")) {
		entry->service_id = atol(value);
		for (j = 0; ARRAY_SIZE(pol_name); j++)
			if (!strcasecmp(value, pol_name[j]))
				break;
		if (j == ARRAY_SIZE(pol_name))
			return -2;
		entry->pol = j;
		return 0;
	}

	if (!is_video && !is_audio)
		return -1;

	/* Video and audio may have multiple values */

	len = 0;

	p = strtok(value," \t");
	if (!p)
		return -2;
	while (p) {
		pid = realloc(pid, (len + 1) * sizeof (*pid));
		pid[len] = atol(p);
		p = strtok(NULL, " \t\n");
		len++;
	}

	if (is_video)
		entry->video_pid = pid;
	else
		entry->audio_pid = pid;

	return 0;
}


struct dvb_file *read_dvb_file(const char *fname)
{
	char *buf = NULL, *p, *key, *value;
	size_t size = 0;
	int len = 0;
	int line = 0, rc;
	struct dvb_file *dvb_file;
	FILE *fd;
	struct dvb_entry *entry = NULL;
	char err_msg[80];

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
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '\n' || *p == '#' || *p == '\a' || *p == '\0')
			continue;

		if (*p == '[') {
			/* NEW Entry */
			if (!entry) {
				dvb_file->first_entry = calloc(sizeof(*entry), 1);
				entry = dvb_file->first_entry;
			} else {
				entry->next = calloc(sizeof(*entry), 1);
				entry = entry->next;
			}
			p++;
			p = strtok(p, "]");
			if (!p) {
				sprintf(err_msg, "Missing channel group");
				goto error;
			}
			if (!strcasecmp(p, CHANNEL))
				p += strlen(CHANNEL);
			while (*p == ' ' || *p == '\t')
				p++;
			if (*p) {
				entry->channel = calloc(strlen(p) + 1, 1);
				strcpy(entry->channel, p);
			}
		} else {
			if (!entry) {
				sprintf(err_msg, "key/value without a channel group");
				goto error;
			}
			key = strtok(p, "=");
			if (!key) {
				sprintf(err_msg, "missing key");
				goto error;
			}
			p = &key[strlen(key) - 1];
			while ((p > key) && (*(p - 1) == ' ' || *(p - 1) == '\t'))
				p--;
			*p = 0;
			value = strtok(NULL, "\n");
			if (!value) {
				sprintf(err_msg, "missing value");
				goto error;
			}
			while (*value == ' ' || *value == '\t')
				value++;

			rc = fill_entry(entry, key, value);
			if (rc == -2) {
				sprintf(err_msg, "value %s is invalid for %s",
					value, key);
				goto error;
			} else if (rc == -2) {
				sprintf(err_msg, "key %s is unknown", key);
				goto error;
			}
		}
	} while (1);
	fclose(fd);
	return dvb_file;

error:
	fprintf (stderr, "ERROR %s while parsing line %d of %s\n",
		 err_msg, line, fname);
	dvb_file_free(dvb_file);
	fclose(fd);
	return NULL;
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
			if (entry->vchannel)
				fprintf(fp, "\tVCHANNEL = %s\n", entry->vchannel);
			fprintf(fp, "\tSERVICE_ID = %d\n", entry->service_id);

			fprintf(fp, "\tVIDEO_PID =");
			for (i = 0; i < entry->video_pid_len; i++)
				fprintf(fp, " %d", entry->video_pid[i]);
			fprintf(fp, "\n");

			fprintf(fp, "\tAUDIO_PID =");
			for (i = 0; i < entry->audio_pid_len; i++)
				fprintf(fp, " %d", entry->audio_pid[i]);
			fprintf(fp, "\n");

			if (entry->pol != POLARIZATION_OFF) {
				fprintf(fp, "\tPOLARIZATION = %s\n",
					pol_name[entry->pol]);
			}

			if (entry->sat_number >= 0) {
				fprintf(fp, "\tSAT_NUMBER = %d\n",
					entry->sat_number);
			}

			if (entry->diseqc_wait > 0) {
				fprintf(fp, "\tDISEQC_WAIT = %d\n",
					entry->diseqc_wait);
			}
			if (entry->lnb)
				fprintf(fp, "\tLNB = %s\n", entry->lnb);
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

char *dvb_vchannel(struct dvb_descriptors *dvb_desc,
	           int service)
{
	struct service_table *service_table = &dvb_desc->sdt_table.service_table[service];
	struct lcn_table *lcn = dvb_desc->nit_table.lcn;
	int i;
	char *buf;

	if (!lcn) {
		if (!dvb_desc->nit_table.virtual_channel)
			return NULL;

		asprintf(&buf, "%d.%d", dvb_desc->nit_table.virtual_channel,
			 service);
		return buf;
	}

	for (i = 0; i < dvb_desc->nit_table.lcn_len; i++) {
		if (lcn[i].service_id == service_table->service_id) {
			asprintf(&buf, "%d.%d.%d",
					dvb_desc->nit_table.virtual_channel,
					lcn[i].lcn, service);
			return buf;
		}
	}
	asprintf(&buf, "%d.%d", dvb_desc->nit_table.virtual_channel,
			service);
	return buf;
}

int store_dvb_channel(struct dvb_file **dvb_file,
		      struct dvb_v5_fe_parms *parms,
		      struct dvb_descriptors *dvb_desc,
		      int get_detected)
{
	struct dvb_entry *entry;
	int i, j;

	if (!*dvb_file) {
		*dvb_file = calloc(sizeof(*dvb_file), 1);
		if (!*dvb_file) {
			perror("Allocating memory for dvb_file");
			return -1;
		}
	}

	/* Go to the last entry */
	entry = (*dvb_file)->first_entry;
	while (entry && entry->next)
		entry = entry->next;

	for (i = 0; i < dvb_desc->sdt_table.service_table_len; i++) {
		struct service_table *service_table = &dvb_desc->sdt_table.service_table[i];
		struct pat_table *pat_table = &dvb_desc->pat_table;
		struct pid_table *pid_table = NULL;

		if (!entry) {
			(*dvb_file)->first_entry = calloc(sizeof(*entry), 1);
			entry = (*dvb_file)->first_entry;
		} else {
			entry->next = calloc(sizeof(*entry), 1);
			entry = entry->next;
		}
		if (!entry) {
			fprintf(stderr, "Not enough memory\n");
			return -1;
		}

		entry->channel = calloc(strlen(service_table->service_name) + 1, 1);
		strcpy(entry->channel, service_table->service_name);
		entry->service_id = service_table->service_id;

		entry->vchannel = dvb_vchannel(dvb_desc, i);

		entry->pol = parms->pol;
		entry->sat_number = parms->sat_number;
		entry->diseqc_wait = parms->diseqc_wait;
		if (parms->lnb)
			entry->lnb = strdup(parms->lnb->alias);

		for (j = 0; j < pat_table->pid_table_len; j++) {
			pid_table = &pat_table->pid_table[j];
			if (service_table->service_id == pid_table->service_id)
				break;
		}
		if (j == pat_table->pid_table_len) {
			fprintf(stderr, "Service ID 0x%04x not found!\n",
			      service_table->service_id);
			return -1;
		}
		entry->video_pid = calloc(sizeof(*entry[i].video_pid),
					    pid_table->video_pid_len);
		for (j = 0; j < pid_table->video_pid_len; j++)
			entry->video_pid[j] = pid_table->video_pid[j];
		entry->video_pid_len = pid_table->video_pid_len;

		entry->audio_pid = calloc(sizeof(*entry[i].audio_pid),
					    pid_table->audio_pid_len);
		for (j = 0; j < pid_table->audio_pid_len; j++)
			entry->audio_pid[j] = pid_table->audio_pid[j];
		entry->audio_pid_len = pid_table->audio_pid_len;

		/* Copy data from parms */
		if (get_detected)
			dvb_fe_get_parms(parms);

		for (j = 0; j < parms->n_props; j++) {
			entry->props[j].cmd = parms->dvb_prop[j].cmd;
			entry->props[j].u.data = parms->dvb_prop[j].u.data;
		}
		entry->n_props = parms->n_props;
	}

	return 0;
}
