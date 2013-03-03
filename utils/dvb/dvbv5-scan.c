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
 * Based on dvbv5-tzap utility.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <argp.h>

#include <config.h>

#include <linux/dvb/dmx.h>
#include "dvb-file.h"
#include "dvb-demux.h"
#include "dvb-v5-std.h"
#include "dvb-scan.h"

#define PROGRAM_NAME	"dvbv5-scan"
#define DEFAULT_OUTPUT  "dvb_channel.conf"

const char *argp_program_version = PROGRAM_NAME " version " V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <mchehab@redhat.com>";

struct arguments {
	char *confname, *lnb_name, *output, *demux_dev;
	unsigned adapter, frontend, demux, get_detected, get_nit;
	int force_dvbv3, lnb, sat_number, freq_bpf;
	unsigned diseqc_wait, dont_add_new_freqs, timeout_multiply;
	unsigned other_nit;
	enum file_formats input_format, output_format;

	/* Used by status print */
	unsigned n_status_lines;
};

static const struct argp_option options[] = {
	{"adapter",	'a',	"adapter#",		0, "use given adapter (default 0)", 0},
	{"frontend",	'f',	"frontend#",		0, "use given frontend (default 0)", 0},
	{"demux",	'd',	"demux#",		0, "use given demux (default 0)", 0},
	{"lnbf",	'l',	"LNBf_type",		0, "type of LNBf to use. 'help' lists the available ones", 0},
	{"sat_number",	'S',	"satellite_number",	0, "satellite number. If not specified, disable DISEqC", 0},
	{"freq_bpf",	'U',	"frequency",		0, "SCR/Unicable band-pass filter frequency to use, in kHz", 0},
	{"wait",	'W',	"time",			0, "adds additional wait time for DISEqC command completion", 0},
	{"nit",		'N',	NULL,			0, "use data from NIT table on the output file", 0},
	{"get_frontend",'G',	NULL,			0, "use data from get_frontend on the output file", 0},
	{"verbose",	'v',	NULL,			0, "be (very) verbose", 0},
	{"output",	'o',	"file",			0, "output filename (default: " DEFAULT_OUTPUT ")", 0},
	{"file-freqs-only", 'F', NULL,			0, "don't use the other frequencies discovered during scan", 0},
	{"timeout-multiply", 'T', "factor",		0, "Multiply scan timeouts by this factor", 0},
	{"parse-other-nit", 'p', NULL,			0, "Parse the other NIT/SDT tables", 0},
	{"input-format", 'I',	"format",		0, "Input format: CHANNEL, DVBV5 (default: DVBV5)", 0},
	{"output-format", 'O',	"format",		0, "Output format: CHANNEL, ZAP, DVBV5 (default: DVBV5)", 0},
	{"dvbv3",	'3',	0,			0, "Use DVBv3 only", 0},
	{ 0, 0, 0, 0, 0, 0 }
};

static int verbose = 0;
#define CHANNEL_FILE "channels.conf"

#define ERROR(x...)                                                     \
	do {                                                            \
		fprintf(stderr, "ERROR: ");                             \
		fprintf(stderr, x);                                     \
		fprintf(stderr, "\n");                                 \
	} while (0)

#define PERROR(x...)                                                    \
	do {                                                            \
		fprintf(stderr, "ERROR: ");                             \
		fprintf(stderr, x);                                     \
		fprintf(stderr, " (%s)\n", strerror(errno));		\
	} while (0)

static int print_frontend_stats(struct arguments *args,
				struct dvb_v5_fe_parms *parms)
{
	char buf[512], *p;
	int rc, i, len, show;
	uint32_t status = 0;

	/* Move cursor up and cleans down */
	if (isatty(STDERR_FILENO) && args->n_status_lines)
		fprintf(stderr, "\r\x1b[%dA\x1b[J", args->n_status_lines);

	args->n_status_lines = 0;

	if (isatty(STDERR_FILENO)) {
		rc = dvb_fe_retrieve_stats(parms, DTV_STATUS, &status);
		if (rc)
			status = 0;
		if (status & FE_HAS_LOCK)
			fprintf(stderr, "\x1b[1;32m");
		else
			fprintf(stderr, "\x1b[33m");
	}

	p = buf;
	len = sizeof(buf);
	dvb_fe_snprintf_stat(parms,  DTV_STATUS, NULL, 0, &p, &len, &show);

	for (i = 0; i < MAX_DTV_STATS; i++) {
		show = 1;

		dvb_fe_snprintf_stat(parms, DTV_QUALITY, "Quality",
				     i, &p, &len, &show);

		dvb_fe_snprintf_stat(parms, DTV_STAT_SIGNAL_STRENGTH, "Signal",
				     i, &p, &len, &show);

		dvb_fe_snprintf_stat(parms, DTV_STAT_CNR, "C/N",
				     i, &p, &len, &show);

		dvb_fe_snprintf_stat(parms, DTV_STAT_ERROR_BLOCK_COUNT, "UCB",
				     i,  &p, &len, &show);

		dvb_fe_snprintf_stat(parms, DTV_BER, "postBER",
				     i,  &p, &len, &show);

		dvb_fe_snprintf_stat(parms, DTV_PRE_BER, "preBER",
				     i,  &p, &len, &show);

		dvb_fe_snprintf_stat(parms, DTV_PER, "PER",
				     i,  &p, &len, &show);

		if (p != buf) {
			if (args->n_status_lines)
				fprintf(stderr, "\t%s\n", buf);
			else
				fprintf(stderr, "%s\n", buf);

			args->n_status_lines++;

			p = buf;
			len = sizeof(buf);
		}
	}

	fflush(stderr);

	return 0;
}

static int check_frontend(struct arguments *args, struct dvb_v5_fe_parms *parms)
{
	int rc, i;
	fe_status_t status;

	args->n_status_lines = 0;
	for (i = 0; i < args->timeout_multiply * 40; i++) {
		rc = dvb_fe_get_stats(parms);
		if (rc)
			PERROR("dvb_fe_get_stats failed");

		rc = dvb_fe_retrieve_stats(parms, DTV_STATUS, &status);
		if (rc)
			status = 0;
		print_frontend_stats(args, parms);
		if (status & FE_HAS_LOCK)
			break;
		usleep(100000);
	};

	if (isatty(STDERR_FILENO)) {
		fprintf(stderr, "\x1b[37m");
	}

	return (status & FE_HAS_LOCK) ? 0 : -1;
}

static int new_freq_is_needed(struct dvb_entry *entry,
			      struct dvb_entry *last_entry,
			      uint32_t freq,
			      enum dvb_sat_polarization pol,
			      int shift)
{
	int i;
	uint32_t data;

	for (; entry != last_entry; entry = entry->next) {
		for (i = 0; i < entry->n_props; i++) {
			data = entry->props[i].u.data;
			if (entry->props[i].cmd == DTV_POLARIZATION) {
				if (data != pol)
					continue;
			}
			if (entry->props[i].cmd == DTV_FREQUENCY) {
				if (( freq >= data - shift) && (freq <= data + shift))
					return 0;
			}
		}
	}

	return 1;
}

static void add_new_freq(struct dvb_entry *entry, uint32_t freq)
{
	struct dvb_entry *new_entry;
	int i, n = 2;

	/* Clone the current entry into a new entry */
	new_entry = calloc(sizeof(*new_entry), 1);
	if (!new_entry) {
		PERROR("not enough memory ofr a new scanning frequency");
		return;
	}

	memcpy(new_entry, entry, sizeof(*entry));

	/*
	 * The frequency should change to the new one. Seek for it and
	 * replace its value to the desired one.
	 */
	for (i = 0; i < new_entry->n_props; i++) {
		if (new_entry->props[i].cmd == DTV_FREQUENCY) {
			new_entry->props[i].u.data = freq;
			/* Navigate to the end of the entry list */
			while (entry->next) {
				entry = entry->next;
				n++;
			}
			printf("New transponder/channel found: #%d: %d\n",
			       n, freq);
			entry->next = new_entry;
			new_entry->next = NULL;
			return;
		}
	}

	/* This should never happen */
	fprintf(stderr, "BUG: Couldn't add %d to the scan frequency list.\n",
		freq);
	free(new_entry);
}

static int estimate_freq_shift(struct dvb_v5_fe_parms *parms)
{
	uint32_t shift = 0, bw = 0, symbol_rate, ro;
	int rolloff = 0;
	int divisor = 100;

	/* Need to handle only cable/satellite and ATSC standards */
	switch (parms->current_sys) {
	case SYS_DVBC_ANNEX_A:
		rolloff = 115;
		break;
	case SYS_DVBC_ANNEX_C:
		rolloff = 115;
		break;
	case SYS_DVBS:
	case SYS_ISDBS:	/* FIXME: not sure if this rollof is right for ISDB-S */
		divisor = 100000;
		rolloff = 135;
		break;
	case SYS_DVBS2:
	case SYS_DSS:
	case SYS_TURBO:
		divisor = 100000;
		dvb_fe_retrieve_parm(parms, DTV_ROLLOFF, &ro);
		switch (ro) {
		case ROLLOFF_20:
			rolloff = 120;
			break;
		case ROLLOFF_25:
			rolloff = 125;
			break;
		default:
		case ROLLOFF_AUTO:
		case ROLLOFF_35:
			rolloff = 135;
			break;
		}
		break;
	case SYS_ATSC:
	case SYS_DVBC_ANNEX_B:
		bw = 6000000;
		break;
	default:
		break;
	}
	if (rolloff) {
		/*
		 * This is not 100% correct for DVB-S2, as there is a bw
		 * guard interval there but it should be enough for the
		 * purposes of estimating a max frequency shift here.
		 */
		dvb_fe_retrieve_parm(parms, DTV_SYMBOL_RATE, &symbol_rate);
		bw = (symbol_rate * rolloff) / divisor;
	}
	if (!bw)
		dvb_fe_retrieve_parm(parms, DTV_BANDWIDTH_HZ, &bw);

	/*
	 * If the max frequency shift between two frequencies is below
	 * than the used bandwidth / 8, it should be the same channel.
	 */
	shift = bw / 8;

	return shift;
}

static void add_other_freq_entries(struct dvb_file *dvb_file,
				   struct dvb_v5_fe_parms *parms,
				   struct dvb_v5_descriptors *dvb_desc)
{
	int i;
	uint32_t freq, shift = 0;
	enum dvb_sat_polarization pol = POLARIZATION_OFF;

	if (!dvb_desc->nit_table.frequency)
		return;

	pol = dvb_desc->nit_table.pol;

	shift = estimate_freq_shift(parms);

	for (i = 0; i < dvb_desc->nit_table.frequency_len; i++) {
		freq = dvb_desc->nit_table.frequency[i];

		if (new_freq_is_needed(dvb_file->first_entry, NULL, freq, pol,
				       shift))
			add_new_freq(dvb_file->first_entry, freq);
	}
}

static int run_scan(struct arguments *args,
		    struct dvb_v5_fe_parms *parms)
{
	struct dvb_file *dvb_file = NULL, *dvb_file_new = NULL;
	struct dvb_entry *entry;
	int i, rc, count = 0, dmx_fd, shift;
	uint32_t freq, sys;

	/* This is used only when reading old formats */
	switch (parms->current_sys) {
	case SYS_DVBT:
	case SYS_DVBS:
	case SYS_DVBC_ANNEX_A:
	case SYS_ATSC:
		sys = parms->current_sys;
		break;
	case SYS_DVBC_ANNEX_C:
		sys = SYS_DVBC_ANNEX_A;
		break;
	case SYS_DVBC_ANNEX_B:
		sys = SYS_ATSC;
		break;
	case SYS_ISDBT:
		sys = SYS_DVBT;
		break;
	default:
		sys = SYS_UNDEFINED;
		break;
	}
	dvb_file = dvb_read_file_format(args->confname, sys,
				    args->input_format);
	if (!dvb_file)
		return -2;

	dmx_fd = open(args->demux_dev, O_RDWR);
	if (dmx_fd < 0) {
		perror("openening pat demux failed");
		return -3;
	}

	for (entry = dvb_file->first_entry; entry != NULL; entry = entry->next) {
		struct dvb_v5_descriptors *dvb_desc = NULL;

		/* First of all, set the delivery system */
		for (i = 0; i < entry->n_props; i++)
			if (entry->props[i].cmd == DTV_DELIVERY_SYSTEM)
				dvb_set_compat_delivery_system(parms,
							       entry->props[i].u.data);

		/* Copy data into parms */
		for (i = 0; i < entry->n_props; i++) {
			uint32_t data = entry->props[i].u.data;

			/* Don't change the delivery system */
			if (entry->props[i].cmd == DTV_DELIVERY_SYSTEM)
				continue;

			dvb_fe_store_parm(parms, entry->props[i].cmd, data);
			if (parms->current_sys == SYS_ISDBT) {
				dvb_fe_store_parm(parms, DTV_ISDBT_PARTIAL_RECEPTION, 0);
				dvb_fe_store_parm(parms, DTV_ISDBT_SOUND_BROADCASTING, 0);
				dvb_fe_store_parm(parms, DTV_ISDBT_LAYER_ENABLED, 0x07);
				if (entry->props[i].cmd == DTV_CODE_RATE_HP) {
					dvb_fe_store_parm(parms, DTV_ISDBT_LAYERA_FEC,
							data);
					dvb_fe_store_parm(parms, DTV_ISDBT_LAYERB_FEC,
							data);
					dvb_fe_store_parm(parms, DTV_ISDBT_LAYERC_FEC,
							data);
				} else if (entry->props[i].cmd == DTV_MODULATION) {
					dvb_fe_store_parm(parms,
							DTV_ISDBT_LAYERA_MODULATION,
							data);
					dvb_fe_store_parm(parms,
							DTV_ISDBT_LAYERB_MODULATION,
							data);
					dvb_fe_store_parm(parms,
							DTV_ISDBT_LAYERC_MODULATION,
							data);
				}
			}
			if (parms->current_sys == SYS_ATSC &&
			    entry->props[i].cmd == DTV_MODULATION) {
				if (data != VSB_8 && data != VSB_16)
					dvb_fe_store_parm(parms,
							DTV_DELIVERY_SYSTEM,
							SYS_DVBC_ANNEX_B);
			}
		}

		/*
		 * If the channel file has duplicated frequencies, or some
		 * entries without any frequency at all, discard.
		 */
		freq = 0;
		for (i = 0; i < entry->n_props; i++) {
			if (entry->props[i].cmd == DTV_FREQUENCY) {
				freq = entry->props[i].u.data;
				break;
			}
		}
		if (!freq)
			continue;
		shift = estimate_freq_shift(parms);
		if (dvb_desc && !new_freq_is_needed(dvb_file->first_entry, entry,
					freq, dvb_desc->nit_table.pol, shift))
			continue;

		rc = dvb_fe_set_parms(parms);
		if (rc < 0) {
			PERROR("dvb_fe_set_parms failed");
			return -1;
		}

		/* As the DVB core emulates it, better to always use auto */
		dvb_fe_store_parm(parms, DTV_INVERSION, INVERSION_AUTO);

		dvb_fe_retrieve_parm(parms, DTV_FREQUENCY, &freq);
		count++;
		dvb_log("Scanning frequency #%d %d", count, freq);
		if (verbose)
			dvb_fe_prt_parms(parms);

		rc = check_frontend(args, parms);
		if (rc < 0)
			continue;

		dvb_desc = dvb_get_ts_tables(parms, dmx_fd,
					     parms->current_sys,
					     args->other_nit,
					     args->timeout_multiply,
					     verbose);
		if (!dvb_desc)
			continue;

		for (i = 0; i < dvb_desc->sdt_table.service_table_len; i++) {
			struct service_table *service_table = &dvb_desc->sdt_table.service_table[i];

			entry->vchannel = dvb_vchannel(dvb_desc, i);
			printf("Service #%d (%d)", i,
				service_table->service_id);
			if (service_table->service_name)
				printf(" %s", service_table->service_name);
			if (entry->vchannel)
				printf(" channel %s", entry->vchannel);
			printf("\n");
		}

		store_dvb_channel(&dvb_file_new, parms, dvb_desc,
				  args->get_detected, args->get_nit);

		if (!args->dont_add_new_freqs)
			add_other_freq_entries(dvb_file, parms, dvb_desc);

		dvb_free_ts_tables(dvb_desc);
	}

	if (dvb_file_new)
		write_file_format(args->output, dvb_file_new,
				  parms->current_sys, args->output_format);

	dvb_file_free(dvb_file);
	if (dvb_file_new)
		dvb_file_free(dvb_file_new);

	close(dmx_fd);
	return 0;
}

static error_t parse_opt(int k, char *optarg, struct argp_state *state)
{
	struct arguments *args = state->input;
	switch (k) {
	case 'a':
		args->adapter = strtoul(optarg, NULL, 0);
		break;
	case 'f':
		args->frontend = strtoul(optarg, NULL, 0);
		break;
	case 'd':
		args->demux = strtoul(optarg, NULL, 0);
		break;
	case 'l':
		args->lnb_name = optarg;
		break;
	case 'S':
		args->sat_number = strtoul(optarg, NULL, 0);
		break;
	case 'U':
		args->freq_bpf = strtoul(optarg, NULL, 0);
		break;
	case 'W':
		args->diseqc_wait = strtoul(optarg, NULL, 0);
		break;
	case 'N':
		args->get_nit++;
		break;
	case 'G':
		args->get_detected++;
		break;
	case 'F':
		args->dont_add_new_freqs++;
		break;
	case 'p':
		args->other_nit++;
		break;
	case 'v':
		verbose++;
		break;
	case 'T':
		args->timeout_multiply = strtoul(optarg, NULL, 0);
		break;
	case 'I':
		args->input_format = parse_format(optarg);
		break;
	case 'O':
		args->output_format = parse_format(optarg);
		break;
	case 'o':
		args->output = optarg;
		break;
	case '3':
		args->force_dvbv3 = 1;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	};
	return 0;
}

int main(int argc, char **argv)
{
	struct arguments args;
	int lnb = -1,idx = -1;
	const struct argp argp = {
		.options = options,
		.parser = parse_opt,
		.doc = "scan DVB services using the channel file",
		.args_doc = "<initial file>",
	};

	memset(&args, 0, sizeof(args));
	args.sat_number = -1;
	args.output = DEFAULT_OUTPUT;
	args.input_format = FILE_DVBV5;
	args.output_format = FILE_DVBV5;
	args.timeout_multiply = 1;

	argp_parse(&argp, argc, argv, 0, &idx, &args);
	if (args.timeout_multiply == 0)
		args.timeout_multiply = 1;

	if (args.lnb_name) {
		lnb = dvb_sat_search_lnb(args.lnb_name);
		if (lnb < 0) {
			printf("Please select one of the LNBf's below:\n");
			print_all_lnb();
			exit(1);
		} else {
			printf("Using LNBf ");
			print_lnb(lnb);
		}
	}

	if (idx < argc)
		args.confname = argv[idx];

	if (!args.confname || idx < 0) {
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, PROGRAM_NAME);
		return -1;
	}
	if ((args.input_format == FILE_ZAP) ||
		   (args.input_format == FILE_UNKNOWN) ||
		   (args.output_format == FILE_UNKNOWN)) {
		fprintf(stderr, "ERROR: Please specify a valid format\n");
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, PROGRAM_NAME);
		return -1;
	}

	asprintf(&args.demux_dev,
		 "/dev/dvb/adapter%i/demux%i", args.adapter, args.demux);

	if (verbose)
		fprintf(stderr, "using demux '%s'\n", args.demux_dev);

	struct dvb_v5_fe_parms *parms = dvb_fe_open(args.adapter,
						    args.frontend,
						    verbose, args.force_dvbv3);
	if (!parms)
		return -1;
	if (lnb >= 0)
		parms->lnb = dvb_sat_get_lnb(lnb);
	if (args.sat_number >= 0)
		parms->sat_number = args.sat_number % 3;
	parms->diseqc_wait = args.diseqc_wait;
	parms->freq_bpf = args.freq_bpf;

	if (run_scan(&args, parms))
		return -1;

	dvb_fe_close(parms);

	free(args.demux_dev);

	return 0;
}
