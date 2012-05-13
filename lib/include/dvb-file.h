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


#include "dvb-fe.h"

struct dvb_entry {
	struct dtv_property props[DTV_MAX_COMMAND];
	unsigned int n_props;
	struct dvb_entry *next;
	uint16_t service_id;
	uint16_t *video_pid, *audio_pid;
	struct el_pid *other_el_pid;
	unsigned video_pid_len, audio_pid_len, other_el_pid_len;
	char *channel;
	char *vchannel;

	char *location;

	enum polarization pol;
	int sat_number;
	unsigned freq_bpf;
	unsigned diseqc_wait;
	char *lnb;
};

struct dvb_file {
	char *fname;
	int n_entries;
	struct dvb_entry *first_entry;
};

struct parse_table {
	unsigned int prop;
	const char **table;
	unsigned int size;
	int	mult_factor;	/* Factor to muliply from file parsing POV */
};

struct parse_struct {
	char				*id;
	uint32_t			delsys;
	const struct parse_table	*table;
	unsigned int			size;
};

struct parse_file {
	int has_delsys_id;
	char *delimiter;
	struct parse_struct formats[];
};

/* Known file formats */
enum file_formats {
	FILE_UNKNOWN,
	FILE_ZAP,
	FILE_CHANNEL,
	FILE_DVBV5,
};

#define PTABLE(a) .table = a, .size=ARRAY_SIZE(a)

/* FAKE DTV codes, for internal usage */
#define DTV_POLARIZATION        (DTV_MAX_COMMAND + 200)
#define DTV_VIDEO_PID           (DTV_MAX_COMMAND + 201)
#define DTV_AUDIO_PID           (DTV_MAX_COMMAND + 202)
#define DTV_SERVICE_ID          (DTV_MAX_COMMAND + 203)
#define DTV_CH_NAME             (DTV_MAX_COMMAND + 204)
#define DTV_VCHANNEL            (DTV_MAX_COMMAND + 205)
#define DTV_SAT_NUMBER          (DTV_MAX_COMMAND + 206)
#define DTV_DISEQC_WAIT         (DTV_MAX_COMMAND + 207)
#define DTV_DISEQC_LNB          (DTV_MAX_COMMAND + 208)
#define DTV_FREQ_BPF            (DTV_MAX_COMMAND + 209)

struct dvb_descriptors;

static inline void dvb_file_free(struct dvb_file *dvb_file)
{
	struct dvb_entry *entry = dvb_file->first_entry, *next;
	while (entry) {
		next = entry->next;
		if (entry->channel)
			free (entry->channel);
		if (entry->vchannel)
			free (entry->vchannel);
		if (entry->location)
			free (entry->location);
		if (entry->video_pid)
			free (entry->video_pid);
		if (entry->audio_pid)
			free (entry->audio_pid);
		if (entry->other_el_pid)
			free (entry->other_el_pid);
		if (entry->lnb)
			free (entry->lnb);
		entry = next;
	}
	free (dvb_file);
}

/* From dvb-legacy-channel-format.c */
extern const struct parse_file channel_file_format;

/* From dvb-zap-format.c */
extern const struct parse_file channel_file_zap_format;

/* From dvb-file.c */
struct dvb_file *parse_format_oneline(const char *fname,
				      uint32_t delsys,
				      const struct parse_file *parse_file);
int write_format_oneline(const char *fname,
			 struct dvb_file *dvb_file,
			 uint32_t delsys,
			 const struct parse_file *parse_file);



struct dvb_file *read_dvb_file(const char *fname);

int write_dvb_file(const char *fname, struct dvb_file *dvb_file);

char *dvb_vchannel(struct dvb_descriptors *dvb_desc,
	           int service);

int store_dvb_channel(struct dvb_file **dvb_file,
		      struct dvb_v5_fe_parms *parms,
		      struct dvb_descriptors *dvb_desc,
		      int get_detected, int get_nit);
int parse_delsys(const char *name);
enum file_formats parse_format(const char *name);
struct dvb_file *read_file_format(const char *fname,
					   uint32_t delsys,
					   enum file_formats format);
int write_file_format(const char *fname,
		      struct dvb_file *dvb_file,
		      uint32_t delsys,
		      enum file_formats format);
