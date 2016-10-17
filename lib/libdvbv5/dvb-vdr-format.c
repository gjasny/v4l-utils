/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libdvbv5/dvb-file.h>
#include <libdvbv5/dvb-v5-std.h>

#include <config.h>

#ifdef ENABLE_NLS
# include "gettext.h"
# include <libintl.h>
# define _(string) dgettext(LIBDVBV5_DOMAIN, string)

#else
# define _(string) string
#endif

# define N_(string) string


#define PTABLE(a) .table = a, .size=ARRAY_SIZE(a)

/*
 * Standard channel.conf format for DVB-T, DVB-C, DVB-S, DVB-S2 and ATSC
 */
static const char *vdr_parse_bandwidth[] = {
	[BANDWIDTH_1_712_MHZ] = "B1712",
	[BANDWIDTH_5_MHZ] =     "B5",
	[BANDWIDTH_6_MHZ] =     "B6",
	[BANDWIDTH_7_MHZ] =     "B7",
	[BANDWIDTH_8_MHZ] =     "B8",
	[BANDWIDTH_10_MHZ] =    "B10",
	[BANDWIDTH_AUTO] =      "B999",
};

static const char *vdr_parse_code_rate_hp[12] = {
	[FEC_NONE] = "C0",
	[FEC_1_2] =  "C12",
	[FEC_2_3] =  "C23",
	[FEC_3_4] =  "C34",
	[FEC_3_5] =  "C35",
	[FEC_4_5] =  "C45",
	[FEC_5_6] =  "C56",
	[FEC_6_7] =  "C67",
	[FEC_7_8] =  "C78",
	[FEC_8_9] =  "C89",
	[FEC_9_10] = "C910",
	[FEC_AUTO] = "C999",
};

static const char *vdr_parse_code_rate_lp[12] = {
	[FEC_NONE] = "D0",
	[FEC_1_2] =  "D12",
	[FEC_2_3] =  "D23",
	[FEC_3_4] =  "D34",
	[FEC_3_5] =  "D35",
	[FEC_4_5] =  "D45",
	[FEC_5_6] =  "D56",
	[FEC_6_7] =  "D67",
	[FEC_7_8] =  "D78",
	[FEC_8_9] =  "D89",
	[FEC_9_10] = "D910",
	[FEC_AUTO] = "D999",
};
static const char *vdr_parse_guard_interval[] = {
	[GUARD_INTERVAL_1_4] =    "G4",
	[GUARD_INTERVAL_1_8] =    "G8",
	[GUARD_INTERVAL_1_16] =   "G16",
	[GUARD_INTERVAL_1_32] =   "G32",
	[GUARD_INTERVAL_1_128] =  "G128",
	[GUARD_INTERVAL_19_128] = "G19128",
	[GUARD_INTERVAL_19_256] = "G19256",
	[GUARD_INTERVAL_AUTO] =   "G999",
};

static const char *vdr_parse_polarization[] = {
	[POLARIZATION_OFF] = "",
	[POLARIZATION_H] =   "H",
	[POLARIZATION_V] =   "V",
	[POLARIZATION_L] =   "L",
	[POLARIZATION_R] =   "R",
};

static const char *vdr_parse_inversion[] = {
	[INVERSION_OFF]  = "I0",
	[INVERSION_ON]   = "I1",
	[INVERSION_AUTO] = "I999",
};

static const char *vdr_parse_modulation[] = {
	[QAM_16] =   "M16",
	[QAM_32] =   "M32",
	[QAM_64] =   "M64",
	[QAM_128] =  "M128",
	[QAM_256] =  "M256",
	[QPSK] =     "M2",
	[PSK_8] =    "M5",
	[APSK_16] =  "M6",
	[APSK_32] =  "M7",
	[VSB_8] =    "M10",
	[VSB_16] =   "M11",
	[DQPSK] =    "M12",
	[QAM_AUTO] = "M999",
};

static const char *vdr_parse_pilot[] = {
	[PILOT_OFF] =  "N0",
	[PILOT_ON] =   "N1",
	[PILOT_AUTO] = "N999",
};

static const char *vdr_parse_rolloff[] = {
	[ROLLOFF_AUTO] = "O0",
	[ROLLOFF_20]   = "O20",
	[ROLLOFF_25]   = "O25",
	[ROLLOFF_35]   = "O35",
};

static const char *vdr_parse_trans_mode[] = {
	[TRANSMISSION_MODE_1K] =   "T1",
	[TRANSMISSION_MODE_2K] =   "T2",
	[TRANSMISSION_MODE_4K] =   "T4",
	[TRANSMISSION_MODE_8K] =   "T8",
	[TRANSMISSION_MODE_16K] =  "T16",
	[TRANSMISSION_MODE_32K] =  "T32",
	[TRANSMISSION_MODE_AUTO] = "T999",
};

static const char *vdr_parse_hierarchy[] = {
	[HIERARCHY_1] =    "Y1",
	[HIERARCHY_2] =    "Y2",
	[HIERARCHY_4] =    "Y4",
	[HIERARCHY_AUTO] = "Y999",
	[HIERARCHY_NONE] = "Y0",
};

static const char *vdr_parse_delivery_system[] = {
	[SYS_DVBS]  = "S0",
	[SYS_DVBS2] = "S1",
	[SYS_DVBT]  = "S0",
	[SYS_DVBT2] = "S1",
};

/*
 * The tables below deal only with the "Parameters" part of the VDR
 * format.
 */
static const struct dvb_parse_table sys_atsc_table[] = {
	{ DTV_MODULATION, PTABLE(vdr_parse_modulation) },
	{ DTV_INVERSION, PTABLE(vdr_parse_inversion) },
};

static const struct dvb_parse_table sys_dvbc_table[] = {
	{ DTV_SYMBOL_RATE, NULL, 0 },
	{ DTV_INNER_FEC, PTABLE(vdr_parse_code_rate_hp) },
	{ DTV_MODULATION, PTABLE(vdr_parse_modulation) },
	{ DTV_INVERSION, PTABLE(vdr_parse_inversion) },
};

static const struct dvb_parse_table sys_dvbs_table[] = {
	{ DTV_DELIVERY_SYSTEM, PTABLE(vdr_parse_delivery_system) },
	{ DTV_POLARIZATION, PTABLE(vdr_parse_polarization) },
	{ DTV_SYMBOL_RATE, NULL, 0 },
	{ DTV_INNER_FEC, PTABLE(vdr_parse_code_rate_hp) },
	{ DTV_INVERSION, PTABLE(vdr_parse_inversion) },
};

static const struct dvb_parse_table sys_dvbs2_table[] = {
	{ DTV_DELIVERY_SYSTEM, PTABLE(vdr_parse_delivery_system) },
	{ DTV_POLARIZATION, PTABLE(vdr_parse_polarization) },
	{ DTV_SYMBOL_RATE, NULL, 0 },
	{ DTV_INNER_FEC, PTABLE(vdr_parse_code_rate_hp) },
	{ DTV_INVERSION, PTABLE(vdr_parse_inversion) },
	/* DVB-S2 specifics */
	{ DTV_MODULATION, PTABLE(vdr_parse_modulation) },
	{ DTV_PILOT, PTABLE(vdr_parse_pilot) },
	{ DTV_ROLLOFF, PTABLE(vdr_parse_rolloff) },
	{ DTV_STREAM_ID, NULL, },
};

static const struct dvb_parse_table sys_dvbt_table[] = {
	{ DTV_BANDWIDTH_HZ, PTABLE(vdr_parse_bandwidth) },
	{ DTV_CODE_RATE_HP, PTABLE(vdr_parse_code_rate_hp) },
	{ DTV_CODE_RATE_LP, PTABLE(vdr_parse_code_rate_lp) },
	{ DTV_GUARD_INTERVAL, PTABLE(vdr_parse_guard_interval) },
	{ DTV_INVERSION, PTABLE(vdr_parse_inversion) },
	{ DTV_MODULATION, PTABLE(vdr_parse_modulation) },
	{ DTV_DELIVERY_SYSTEM, PTABLE(vdr_parse_delivery_system) },
	{ DTV_TRANSMISSION_MODE, PTABLE(vdr_parse_trans_mode) },
	{ DTV_HIERARCHY, PTABLE(vdr_parse_hierarchy) },
};

static const struct dvb_parse_table sys_dvbt2_table[] = {
	{ DTV_BANDWIDTH_HZ, PTABLE(vdr_parse_bandwidth) },
	{ DTV_CODE_RATE_HP, PTABLE(vdr_parse_code_rate_hp) },
	{ DTV_CODE_RATE_LP, PTABLE(vdr_parse_code_rate_lp) },
	{ DTV_GUARD_INTERVAL, PTABLE(vdr_parse_guard_interval) },
	{ DTV_INVERSION, PTABLE(vdr_parse_inversion) },
	{ DTV_MODULATION, PTABLE(vdr_parse_modulation) },
	{ DTV_DELIVERY_SYSTEM, PTABLE(vdr_parse_delivery_system) },
	{ DTV_TRANSMISSION_MODE, PTABLE(vdr_parse_trans_mode) },
	{ DTV_HIERARCHY, PTABLE(vdr_parse_hierarchy) },
	/* DVB-T2 specifics */
	{ DTV_STREAM_ID, NULL, },
};

static const struct dvb_parse_file vdr_file_format = {
	.formats = {
		{
			.id		= "A",
			.delsys		= SYS_ATSC,
			PTABLE(sys_atsc_table),
		}, {
			.id		= "C",
			.delsys		= SYS_DVBC_ANNEX_A,
			PTABLE(sys_dvbc_table),
		}, {
			.id		= "S",
			.delsys		= SYS_DVBS,
			PTABLE(sys_dvbs_table),
		}, {
			.id		= "S",
			.delsys		= SYS_DVBS2,
			PTABLE(sys_dvbs2_table),
		}, {
			.id		= "T",
			.delsys		= SYS_DVBT,
			PTABLE(sys_dvbt_table),
		}, {
			.id		= "T",
			.delsys		= SYS_DVBT2,
			PTABLE(sys_dvbt2_table),
		}, {
			NULL, 0, NULL, 0,
		}
	}
};

int dvb_write_format_vdr(const char *fname,
			 struct dvb_file *dvb_file)
{
	const struct dvb_parse_file *parse_file = &vdr_file_format;
	const struct dvb_parse_struct *formats = parse_file->formats;
	int i, j, line = 0;
	FILE *fp;
	const struct dvb_parse_struct *fmt;
	struct dvb_entry *entry;
	const struct dvb_parse_table *table;
	const char *id;
	uint32_t delsys, freq, data, srate;
	char err_msg[80];

	fp = fopen(fname, "w");
	if (!fp) {
		perror(fname);
		return -errno;
	}

	for (entry = dvb_file->first_entry; entry != NULL; entry = entry->next) {
		if (dvb_retrieve_entry_prop(entry, DTV_DELIVERY_SYSTEM, &delsys) < 0)
			continue;

		for (i = 0; formats[i].delsys != 0; i++) {
			if (formats[i].delsys == delsys)
				break;
		}
		if (formats[i].delsys == 0) {
			fprintf(stderr,
				_("WARNING: entry %d: delivery system %d not supported on this format. skipping entry\n"),
				 line, delsys);
			continue;
		}
		id = formats[i].id;

		if (!entry->channel) {
			fprintf(stderr,
				_("WARNING: entry %d: channel name not found. skipping entry\n"),
				 line);
			continue;
		}

		if (dvb_retrieve_entry_prop(entry, DTV_FREQUENCY, &freq) < 0) {
			fprintf(stderr,
				_("WARNING: entry %d: frequency not found. skipping entry\n"),
				 line);
			continue;
		}

		/* Output channel name */
		fprintf(fp, "%s", entry->channel);
		if (entry->vchannel)
			fprintf(fp, ",%s", entry->vchannel);
		fprintf(fp, ":");

		/*
		 * Output frequency:
		 *	in kHz for terrestrial/cable
		 *	in MHz for satellite
		 */
		fprintf(fp, "%i:", freq / 1000);

		/* Output modulation parameters */
		fmt = &formats[i];
		for (i = 0; i < fmt->size; i++) {
			table = &fmt->table[i];

			for (j = 0; j < entry->n_props; j++)
				if (entry->props[j].cmd == table->prop)
					break;

			if (!table->size || j >= entry->n_props)
				continue;

			data = entry->props[j].u.data;

			if (table->prop == DTV_BANDWIDTH_HZ) {
				for (j = 0; j < ARRAY_SIZE(fe_bandwidth_name); j++) {
					if (fe_bandwidth_name[j] == data) {
						data = j;
						break;
					}
				}
				if (j == ARRAY_SIZE(fe_bandwidth_name))
					data = BANDWIDTH_AUTO;
			}
			if (data >= table->size) {
				sprintf(err_msg,
						_("value not supported"));
				goto error;
			}

			fprintf(fp, "%s", table->table[data]);
		}
		fprintf(fp, ":");

		/*
		 * Output sources configuration for VDR
		 *
		 *   S (satellite) xy.z (orbital position in degrees) E or W (east or west)
		 *
		 *   FIXME: in case of ATSC we use "A", this is what w_scan does
		 */

		if (entry->location) {
			switch(delsys) {
			case SYS_DVBS:
			case SYS_DVBS2:
				fprintf(fp, "%s", entry->location);
				break;
			default:
				fprintf(fp, "%s", id);
				break;
			}
		} else {
			fprintf(fp, "%s", id);
		}
		fprintf(fp, ":");

		/* Output symbol rate */
		srate = 27500000;
		switch(delsys) {
		case SYS_DVBT:
			srate = 0;
			break;
		case SYS_DVBS:
		case SYS_DVBS2:
		case SYS_DVBC_ANNEX_A:
			if (dvb_retrieve_entry_prop(entry, DTV_SYMBOL_RATE, &srate) < 0) {
				sprintf(err_msg,
						_("symbol rate not found"));
				goto error;
			}
		}
		fprintf(fp, "%d:", srate / 1000);

		/* Output video PID(s) */
		for (i = 0; i < entry->video_pid_len; i++) {
			if (i)
				fprintf(fp,",");
			fprintf(fp, "%d", entry->video_pid[i]);
		}
		if (!i)
			fprintf(fp, "0");
		fprintf(fp, ":");

		/* Output audio PID(s) */
		for (i = 0; i < entry->audio_pid_len; i++) {
			if (i)
				fprintf(fp,",");
			fprintf(fp, "%d", entry->audio_pid[i]);
		}
		if (!i)
			fprintf(fp, "0");
		fprintf(fp, ":");

		/* FIXME: Output teletex PID(s) */
		fprintf(fp, "0:");

		/* Output Conditional Access - let VDR discover it */
		fprintf(fp, "0:");

		/* Output Service ID */
		fprintf(fp, "%d:", entry->service_id);

		/* Output Network ID */
		fprintf(fp, "0:");

		/* Output Transport Stream ID */
		fprintf(fp, "0:");

		/* Output Radio ID
		 * this is the last entry, tagged bei a new line (not a colon!)
		 */
		fprintf(fp, "0\n");
		line++;
	};
	fclose (fp);
	return 0;

error:
	fprintf(stderr, _("ERROR: %s while parsing entry %d of %s\n"),
		 err_msg, line, fname);
	fclose(fp);
	return -1;
}
