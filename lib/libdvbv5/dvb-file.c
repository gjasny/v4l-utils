/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab <mchehab@redhat.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dvb-file.h"
#include "dvb-v5-std.h"
#include "dvb-scan.h"

/*
 * Generic parse function for all formats each channel is contained into
 * just one line.
 */
struct dvb_file *parse_format_oneline(const char *fname,
				      uint32_t delsys,
				      const struct parse_file *parse_file)
{
	const char *delimiter = parse_file->delimiter;
	const struct parse_struct *formats = parse_file->formats;
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
		free(dvb_file);
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

		if (parse_file->has_delsys_id) {
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
			if (!formats[i].id) {
				sprintf(err_msg, "Doesn't know how to handle delimiter '%s'",
					p);
				goto error;
			}
		} else {
			/* Seek for the delivery system */
			for (i = 0; formats[i].delsys != 0; i++) {
				if (formats[i].delsys == delsys)
					break;
			}
			if (!formats[i].delsys) {
				sprintf(err_msg, "Doesn't know how to parse delivery system %d",
					delsys);
				goto error;
			}
		}


		fmt = &formats[i];
		if (!entry) {
			dvb_file->first_entry = calloc(sizeof(*entry), 1);
			entry = dvb_file->first_entry;
		} else {
			entry->next = calloc(sizeof(*entry), 1);
			entry = entry->next;
		}
		entry->sat_number = -1;
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
					i, dvb_cmd_name(table->prop));
				goto error;
			}
			if (table->size) {
				for (j = 0; j < table->size; j++)
					if (!table->table[j] || !strcasecmp(table->table[j], p))
						break;
				if (j == table->size) {
					sprintf(err_msg, "parameter %s invalid: %s",
						dvb_cmd_name(table->prop), p);
					goto error;
				}
				if (table->prop == DTV_BANDWIDTH_HZ)
					j = fe_bandwidth_name[j];
				/*if (table->prop == DTV_POLARIZATION) {*/
					/*entry->pol = j;*/
				/*} else {*/
					entry->props[entry->n_props].cmd = table->prop;
					entry->props[entry->n_props++].u.data = j;
				/*}*/
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

static uint32_t get_compat_format(uint32_t delivery_system)
{
	switch (delivery_system) {
	case SYS_DVBS:
	case SYS_DVBS2:
	case SYS_TURBO:
	case SYS_ISDBS:
	case SYS_DSS:
		return SYS_DVBS;
	case SYS_ATSC:
	case SYS_DVBC_ANNEX_B:
		return SYS_ATSC;
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		return  SYS_DVBC_ANNEX_A;
	case SYS_CMMB:
	case SYS_ISDBT:
	case SYS_DVBT:
	case SYS_DVBT2:
		return SYS_DVBT;
	default:
		return 0;

	}
}

int write_format_oneline(const char *fname,
			 struct dvb_file *dvb_file,
			 uint32_t delsys,
			 const struct parse_file *parse_file)
{
	const char delimiter = parse_file->delimiter[0];
	const struct parse_struct *formats = parse_file->formats;
	int i, j, line = 0, first;
	FILE *fp;
	const struct parse_struct *fmt;
	struct dvb_entry *entry;
	const struct parse_table *table;
	uint32_t data;
	char err_msg[80];
	uint32_t delsys_compat = 0;

	fp = fopen(fname, "w");
	if (!fp) {
		perror(fname);
		return -errno;
	}

	for (entry = dvb_file->first_entry; entry != NULL; entry = entry->next) {
		for (i = 0; i < entry->n_props; i++) {
			if (entry->props[i].cmd == DTV_DELIVERY_SYSTEM) {
				delsys = entry->props[i].u.data;
				break;
			}
		}

		for (i = 0; formats[i].delsys != 0; i++) {
			if (formats[i].delsys == delsys)
				break;
		}
		delsys_compat = get_compat_format(delsys);
		for (i = 0; formats[i].delsys != 0; i++) {
			if (formats[i].delsys == delsys_compat) {
				delsys = delsys_compat;
				break;
			}
		}
		if (formats[i].delsys == 0) {
			sprintf(err_msg,
				 "delivery system %d not supported on this format",
				 delsys);
			goto error;
		}
		if (parse_file->has_delsys_id) {
			fprintf(fp, "%s", formats[i].id);
			first = 0;
		} else
			first = 1;

		fmt = &formats[i];
		for (i = 0; i < fmt->size; i++) {
			table = &fmt->table[i];

			if (first)
				first = 0;
			else
				fprintf(fp, "%c", delimiter);

			for (j = 0; j < entry->n_props; j++)
				if (entry->props[j].cmd == table->prop)
					break;

			if (table->size && j < entry->n_props) {
				data = entry->props[j].u.data;

				if (table->prop == DTV_BANDWIDTH_HZ) {
					if (data < ARRAY_SIZE(fe_bandwidth_name))
						data = fe_bandwidth_name[data];
					else
						data = BANDWIDTH_AUTO;
				}

				if (data >= table->size) {
					sprintf(err_msg,
						 "value not supported");
					goto error;
				}

				fprintf(fp, "%s", table->table[data]);
			} else {
				switch (table->prop) {
				case DTV_VIDEO_PID:
					if (!entry->video_pid) {
						fprintf(stderr,
							"WARNING: missing video PID while parsing entry %d of %s\n",
							line, fname);
						fprintf(fp, "%d",0);
					} else
						fprintf(fp, "%d",
							entry->video_pid[0]);
					break;
				case DTV_AUDIO_PID:
					if (!entry->audio_pid) {
						fprintf(stderr,
							"WARNING: missing audio PID while parsing entry %d of %s\n",
							line, fname);
						fprintf(fp, "%d",0);
					} else
						fprintf(fp, "%d",
							entry->audio_pid[0]);
					break;
				case DTV_SERVICE_ID:
					fprintf(fp, "%d", entry->service_id);
					break;
				case DTV_CH_NAME:
					fprintf(fp, "%s", entry->channel);
					break;
				default:
					if (j >= entry->n_props) {
						fprintf(stderr,
							"property %s not supported while parsing entry %d of %s\n",
							dvb_v5_name[entry->props[i].cmd],
							line, fname);
					}

					data = entry->props[j].u.data;
					fprintf(fp, "%d", data);
					break;
				}
			}
		}
		fprintf(fp, "\n");
		line++;
	};
	fclose (fp);
	return 0;

error:
	fprintf(stderr, "ERROR: %s while parsing entry %d of %s\n",
		 err_msg, line, fname);
	fclose(fp);
	return -1;
}

static int store_entry_prop(struct dvb_entry *entry,
			    uint32_t cmd, uint32_t value)
{
	int i;

	for (i = 0; i < entry->n_props; i++) {
		if (cmd == entry->props[i].cmd)
			break;
	}
	if (i == entry->n_props) {
		if (i == DTV_MAX_COMMAND) {
			fprintf(stderr, "Can't add property %s\n",
			       dvb_v5_name[cmd]);
			return -1;
		}
		entry->n_props++;
		entry->props[i].cmd = cmd;
	}

	entry->props[i].u.data = value;

	return 0;
}

#define CHANNEL "CHANNEL"

static int fill_entry(struct dvb_entry *entry, char *key, char *value)
{
	int i, j, len, type = 0;
	int is_video = 0, is_audio = 0, n_prop;
	uint16_t *pid = NULL;
	char *p;

	/* Handle the DVBv5 DTV_foo properties */
	for (i = 0; i < ARRAY_SIZE(dvb_v5_name); i++) {
		if (!dvb_v5_name[i])
			continue;
		if (!strcasecmp(key, dvb_v5_name[i]))
			break;
	}
	if (i < ARRAY_SIZE(dvb_v5_name)) {
		const char * const *attr_name = dvb_attr_names(i);
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

	if (!strcasecmp(key, "FREQ_BPF")) {
		entry->freq_bpf = atol(value);
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
	else if (!strcasecmp(key, "AUDIO_PID"))
		is_audio = 1;
	else if (!strcasecmp(key, "POLARIZATION")) {
		for (j = 0; ARRAY_SIZE(dvb_sat_pol_name); j++)
			if (!strcasecmp(value, dvb_sat_pol_name[j]))
				break;
		if (j == ARRAY_SIZE(dvb_sat_pol_name))
			return -2;
		store_entry_prop(entry, DTV_POLARIZATION, j);
		return 0;
	} else if (!strncasecmp(key,"PID_", 4)){
		type = strtol(&key[4], NULL, 16);
		if (!type)
			return 0;

		len = 0;

		p = strtok(value," \t");
		if (!p)
			return 0;
		while (p) {
			entry->other_el_pid = realloc(entry->other_el_pid,
						      (len + 1) *
						      sizeof (*entry->other_el_pid));
			entry->other_el_pid[len].type = type;
			entry->other_el_pid[len].pid = atol(p);
			p = strtok(NULL, " \t\n");
			len++;
		}
		entry->other_el_pid_len = len;
	}

	/*
	 * If the key is not known, just discard.
	 * This way, it provides forward compatibility with new keys
	 * that may be added in the future.
	 */
	if (!is_video && !is_audio)
		return 0;

	/* Video and audio may have multiple values */

	len = 0;

	p = strtok(value," \t");
	if (!p)
		return 0;
	while (p) {
		pid = realloc(pid, (len + 1) * sizeof (*pid));
		pid[len] = atol(p);
		p = strtok(NULL, " \t\n");
		len++;
	}

	if (is_video) {
		entry->video_pid = pid;
		entry->video_pid_len = len;
	} else {
		entry->audio_pid = pid;
		entry->audio_pid_len = len;
	}

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
		free(dvb_file);
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
			entry->sat_number = -1;
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
		} else {
			fprintf(fp, "[CHANNEL]\n");
		}

		if (entry->service_id)
			fprintf(fp, "\tSERVICE_ID = %d\n", entry->service_id);

		if (entry->video_pid_len){
			fprintf(fp, "\tVIDEO_PID =");
			for (i = 0; i < entry->video_pid_len; i++)
				fprintf(fp, " %d", entry->video_pid[i]);
			fprintf(fp, "\n");
		}

		if (entry->audio_pid_len) {
			fprintf(fp, "\tAUDIO_PID =");
			for (i = 0; i < entry->audio_pid_len; i++)
				fprintf(fp, " %d", entry->audio_pid[i]);
			fprintf(fp, "\n");
		}

		if (entry->other_el_pid_len) {
			int type = -1;
			for (i = 0; i < entry->other_el_pid_len; i++) {
				if (type != entry->other_el_pid[i].type) {
					type = entry->other_el_pid[i].type;
					if (i)
						fprintf(fp, "\n");
					fprintf(fp, "\tPID_%02x =", type);
				}
				fprintf(fp, " %d", entry->other_el_pid[i].pid);
			}
			fprintf(fp, "\n");
		}

		if (entry->sat_number >= 0) {
			fprintf(fp, "\tSAT_NUMBER = %d\n",
				entry->sat_number);
		}

		if (entry->freq_bpf > 0) {
			fprintf(fp, "\tFREQ_BPF = %d\n",
				entry->freq_bpf);
		}

		if (entry->diseqc_wait > 0) {
			fprintf(fp, "\tDISEQC_WAIT = %d\n",
				entry->diseqc_wait);
		}
		if (entry->lnb)
				fprintf(fp, "\tLNB = %s\n", entry->lnb);

		for (i = 0; i < entry->n_props; i++) {
			const char * const *attr_name = dvb_attr_names(entry->props[i].cmd);
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
					dvb_cmd_name(entry->props[i].cmd),
					entry->props[i].u.data);
			else
				fprintf(fp, "\t%s = %s\n",
					dvb_cmd_name(entry->props[i].cmd),
					*attr_name);
		}
		fprintf(fp, "\n");

		for (i = 0; i < entry->n_props; i++) {
		  if (entry->props[i].cmd < DTV_USER_COMMAND_START)
		    continue;
			const char * const *attr_name = dvb_user_attr_names[entry->props[i].cmd - DTV_USER_COMMAND_START];
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
					dvb_user_name[entry->props[i].cmd - DTV_USER_COMMAND_START],
					entry->props[i].u.data);
			else
				fprintf(fp, "\t%s = %s\n",
					dvb_user_name[entry->props[i].cmd - DTV_USER_COMMAND_START],
					*attr_name);
		}
		fprintf(fp, "\n");
	}
	fclose(fp);
	return 0;
};

char *dvb_vchannel(struct dvb_v5_descriptors *dvb_desc,
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

static void handle_std_specific_parms(struct dvb_entry *entry,
				      struct dvb_v5_descriptors *dvb_desc)
{
	struct nit_table *nit_table = &dvb_desc->nit_table;
	int i;

	/*
	 * If found, parses the NIT tables for the delivery systems
	 * with the information provided on it. For some delivery systems,
	 * there are some missing stuff.
	 */
	switch (nit_table->delivery_system) {
	case SYS_ISDBT:
		store_entry_prop(entry, DTV_GUARD_INTERVAL,
				 nit_table->guard_interval);
		store_entry_prop(entry, DTV_TRANSMISSION_MODE,
				 nit_table->transmission_mode);
		asprintf(&entry->location, "area %d", nit_table->area_code);
		for (i = 0; i < nit_table->partial_reception_len; i++) {
			int par, sid = entry->service_id;
			par = (sid == nit_table->partial_reception[i]) ?
			      1 : 0;
			store_entry_prop(entry, DTV_ISDBT_PARTIAL_RECEPTION,
					par);
			break;
		}
		break;
	case SYS_DVBS2:
		store_entry_prop(entry, DTV_ROLLOFF,
				 nit_table->rolloff);
		/* fall through */
	case SYS_DVBS:
		if (nit_table->orbit)
			entry->location = strdup(nit_table->orbit);
		store_entry_prop(entry, DTV_FREQUENCY,
				 nit_table->frequency[0]);
		store_entry_prop(entry, DTV_MODULATION,
				 nit_table->modulation);
		store_entry_prop(entry, DTV_POLARIZATION,
				 nit_table->pol);
		store_entry_prop(entry, DTV_DELIVERY_SYSTEM,
				 nit_table->delivery_system);
		store_entry_prop(entry, DTV_SYMBOL_RATE,
				 nit_table->symbol_rate);
		store_entry_prop(entry, DTV_INNER_FEC,
				 nit_table->fec_inner);
		break;
	case SYS_DVBC_ANNEX_A:
		if (nit_table->network_name)
			entry->location = strdup(nit_table->network_name);
		store_entry_prop(entry, DTV_FREQUENCY,
				 nit_table->frequency[0]);
		store_entry_prop(entry, DTV_MODULATION,
				 nit_table->modulation);
		store_entry_prop(entry, DTV_SYMBOL_RATE,
				 nit_table->symbol_rate);
		store_entry_prop(entry, DTV_INNER_FEC,
				 nit_table->fec_inner);
		break;
	case SYS_DVBT:
		if (nit_table->network_name)
			entry->location = strdup(nit_table->network_name);
		store_entry_prop(entry, DTV_FREQUENCY,
				 nit_table->frequency[0]);
		store_entry_prop(entry, DTV_MODULATION,
				 nit_table->modulation);
		store_entry_prop(entry, DTV_BANDWIDTH_HZ,
				 nit_table->bandwidth);
		store_entry_prop(entry, DTV_CODE_RATE_HP,
				 nit_table->code_rate_hp);
		store_entry_prop(entry, DTV_CODE_RATE_LP,
				 nit_table->code_rate_lp);
		store_entry_prop(entry, DTV_GUARD_INTERVAL,
				 nit_table->guard_interval);
		store_entry_prop(entry, DTV_TRANSMISSION_MODE,
				 nit_table->transmission_mode);
		store_entry_prop(entry, DTV_HIERARCHY,
				 nit_table->hierarchy);
		break;
	case SYS_DVBT2:
		if (nit_table->network_name)
			entry->location = strdup(nit_table->network_name);
		store_entry_prop(entry, DTV_DVBT2_PLP_ID_LEGACY,
				 nit_table->plp_id);
		store_entry_prop(entry, DTV_BANDWIDTH_HZ,
				 nit_table->bandwidth);
		store_entry_prop(entry, DTV_GUARD_INTERVAL,
				 nit_table->guard_interval);
		store_entry_prop(entry, DTV_TRANSMISSION_MODE,
				 nit_table->transmission_mode);
		if (!nit_table->has_dvbt)
			break;

		/* Fill data from terrestrial descriptor */
		store_entry_prop(entry, DTV_FREQUENCY,
				 nit_table->frequency[0]);
		store_entry_prop(entry, DTV_MODULATION,
				 nit_table->modulation);
		store_entry_prop(entry, DTV_CODE_RATE_HP,
				 nit_table->code_rate_hp);
		store_entry_prop(entry, DTV_CODE_RATE_LP,
				 nit_table->code_rate_lp);
		store_entry_prop(entry, DTV_HIERARCHY,
				 nit_table->hierarchy);
	}
}

static int sort_other_el_pid(const void *a_arg, const void *b_arg)
{
	const struct el_pid *a = a_arg, *b = b_arg;
	int r;

	r = b->type - a->type;
	if (r)
		return r;

	return b->pid - a->pid;
}

int store_dvb_channel(struct dvb_file **dvb_file,
		      struct dvb_v5_fe_parms *parms,
		      struct dvb_v5_descriptors *dvb_desc,
		      int get_detected, int get_nit)
{
	struct dvb_entry *entry;
	int i, j, has_detected = 1;

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
		entry->sat_number = -1;

		if (service_table->service_name) {
			entry->channel = calloc(strlen(service_table->service_name) + 1, 1);
			strcpy(entry->channel, service_table->service_name);
		} else {
			asprintf(&entry->channel, "#%d",
				 service_table->service_id);
		}
		entry->service_id = service_table->service_id;

		entry->vchannel = dvb_vchannel(dvb_desc, i);

		/*entry->pol = parms->pol;*/
		entry->sat_number = parms->sat_number;
		entry->freq_bpf = parms->freq_bpf;
		entry->diseqc_wait = parms->diseqc_wait;
		if (parms->lnb)
			entry->lnb = strdup(parms->lnb->alias);

		for (j = 0; j < pat_table->pid_table_len; j++) {
			pid_table = &pat_table->pid_table[j];
			if (service_table->service_id == pid_table->service_id)
				break;
		}
		if (j == pat_table->pid_table_len) {
			fprintf(stderr, "Service ID %d not found on PMT!\n",
			      service_table->service_id);
			has_detected = 0;
		} else has_detected = 1;

		/*
		 * Video/audio/other pid's can only be filled if the service
		 * was found.
		 */
		if (has_detected) {
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

			entry->other_el_pid = calloc(sizeof(*entry->other_el_pid),
						pid_table->other_el_pid_len);
			memcpy(entry->other_el_pid, pid_table->other_el_pid,
			pid_table->other_el_pid_len * sizeof(*entry->other_el_pid));
			entry->other_el_pid_len = pid_table->other_el_pid_len;
			qsort(entry->other_el_pid, entry->other_el_pid_len,
			sizeof(*entry->other_el_pid), sort_other_el_pid);
		}

		/* Copy data from parms */
		if (get_detected) {
			int rc;
			do {
				rc = dvb_fe_get_parms(parms);
				if (rc == EAGAIN)
					usleep(100000);
			} while (rc == EAGAIN);
		}
		for (j = 0; j < parms->n_props; j++) {
			entry->props[j].cmd = parms->dvb_prop[j].cmd;
			entry->props[j].u.data = parms->dvb_prop[j].u.data;
		}
		entry->n_props = parms->n_props;

		if (get_nit)
			handle_std_specific_parms(entry, dvb_desc);
	}

	return 0;
}

enum file_formats parse_format(const char *name)
{
	if (!strcasecmp(name, "ZAP"))
		return FILE_ZAP;
	if (!strcasecmp(name, "CHANNEL"))
		return FILE_CHANNEL;
	if (!strcasecmp(name, "DVBV5"))
		return FILE_DVBV5;

	fprintf(stderr, "File format %s is unknown\n", name);
	return FILE_UNKNOWN;
}

static struct {
	uint32_t delsys;
	char *name;
} alt_names[] = {
	{ SYS_DVBC_ANNEX_A,	"DVB-C" },
	{ SYS_DVBH,		"DVB-H" },
	{ SYS_DVBS,		"DVB-S" },
	{ SYS_DVBS2,		"DVB-S2" },
	{ SYS_DVBT,		"DVB-T" },
	{ SYS_DVBT2,		"DVB-T2" },
	{ SYS_ISDBC,		"ISDB-C" },
	{ SYS_ISDBS,		"ISDB-S" },
	{ SYS_ISDBT,		"ISDB-T" },
	{ SYS_ATSCMH,		"ATSC-MH" },
	{ SYS_DMBTH,		"DMB-TH" },
};

int parse_delsys(const char *name)
{
	int i, cnt = 0;

	/* Check for DVBv5 names */
	for (i = 0; i < ARRAY_SIZE(delivery_system_name); i++)
		if (delivery_system_name[i] &&
			!strcasecmp(name, delivery_system_name[i]))
			break;
	if (i < ARRAY_SIZE(delivery_system_name))
		return i;

	/* Also accept the alternative names */
	for (i = 0; i < ARRAY_SIZE(alt_names); i++)
		if (!strcasecmp(name, alt_names[i].name))
			break;
	if (i < ARRAY_SIZE(alt_names))
		return alt_names[i].delsys;

	/*
	 * Not found. Print all possible values, except for
	 * SYS_UNDEFINED.
	 */
	fprintf(stderr, "ERROR: Delivery system %s is not known. Valid values are:\n",
		name);
	for (i = 0; i < ARRAY_SIZE(alt_names) - 1; i++) {
		if (!(cnt % 5))
			fprintf(stderr, "\n");
		fprintf(stderr, "%-15s", alt_names[i].name);
		cnt++;
	}

	for (i = 1; i < ARRAY_SIZE(delivery_system_name) - 1; i++) {
		if (!(cnt % 5))
			fprintf(stderr, "\n");
		fprintf(stderr, "%-15s", delivery_system_name[i]);
		cnt++;
	}
	if (cnt % 5)
		fprintf(stderr, "\n");

	fprintf(stderr, "\n");
	return -1;
}

struct dvb_file *dvb_read_file_format(const char *fname,
				  uint32_t delsys,
				  enum file_formats format)
{
	struct dvb_file *dvb_file;

	switch (format) {
	case FILE_CHANNEL:		/* DVB channel/transponder old format */
		dvb_file = parse_format_oneline(fname,
						SYS_UNDEFINED,
						&channel_file_format);
		break;
	case FILE_ZAP:
		dvb_file = parse_format_oneline(fname,
						delsys,
						&channel_file_zap_format);
		break;
	case FILE_DVBV5:
		dvb_file = read_dvb_file(fname);
		break;
	default:
		fprintf(stderr, "Format is not supported\n");
		return NULL;
	}

	return dvb_file;
}

int write_file_format(const char *fname,
		      struct dvb_file *dvb_file,
		      uint32_t delsys,
		      enum file_formats format)
{
	int ret;

	switch (format) {
	case FILE_CHANNEL:		/* DVB channel/transponder old format */
		ret = write_format_oneline(fname,
					   dvb_file,
					   SYS_UNDEFINED,
					   &channel_file_format);
		break;
	case FILE_ZAP:
		ret = write_format_oneline(fname,
					   dvb_file,
					   delsys,
					   &channel_file_zap_format);
		break;
	case FILE_DVBV5:
		ret = write_dvb_file(fname, dvb_file);
		break;
	default:
		return -1;
	}

	return ret;
}

