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

#include "dvb-fe.h"

enum polarization {
	POLARIZATION_OFF	= 0,
	POLARIZATION_H		= 1,
	POLARIZATION_V		= 2,
	POLARIZATION_L		= 3,
	POLARIZATION_R		= 4,
};

struct dvb_entry {
	struct dtv_property props[DTV_MAX_COMMAND];
	unsigned int n_props;
	struct dvb_entry *next;
	enum polarization pol;
	uint32_t video_pid, audio_pid, service_pid;
	char *channel;
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

#define PTABLE(a) .table = a, .size=ARRAY_SIZE(a)

/* FAKE DTV codes, for internal usage */
#define DTV_POLARIZATION        (DTV_MAX_COMMAND + 200)
#define DTV_VIDEO_PID           (DTV_MAX_COMMAND + 201)
#define DTV_AUDIO_PID           (DTV_MAX_COMMAND + 202)
#define DTV_SERVICE_PID         (DTV_MAX_COMMAND + 203)
#define DTV_CH_NAME             (DTV_MAX_COMMAND + 204)

static inline void dvb_file_free(struct dvb_file *dvb_file)
{
	struct dvb_entry *entry = dvb_file->first_entry, *next;
	while (entry) {
		next = entry->next;
		if (entry->channel)
			free (entry->channel);
		free (entry);
		entry = next;
	}
	free (dvb_file);
}

/* From dvb-legacy-channel-format.c */
extern const const struct parse_struct channel_formats[];

/* From dvb-zap-format.c */
extern const const struct parse_struct zap_formats[];

/* From dvb-file.c */
struct dvb_file *parse_format_oneline(const char *fname, const char *delimiter,
				      uint32_t delsys,
				      const struct parse_struct *formats);
