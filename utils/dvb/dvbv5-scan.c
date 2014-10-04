/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
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
#include "libdvbv5/dvb-file.h"
#include "libdvbv5/dvb-demux.h"
#include "libdvbv5/dvb-v5-std.h"
#include "libdvbv5/dvb-scan.h"

#define PROGRAM_NAME	"dvbv5-scan"
#define DEFAULT_OUTPUT  "dvb_channel.conf"

const char *argp_program_version = PROGRAM_NAME " version " V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <m.chehab@samsung.com>";

struct arguments {
	char *confname, *lnb_name, *output, *demux_dev;
	unsigned adapter, n_adapter, adapter_fe, adapter_dmx, frontend, demux, get_detected, get_nit;
	int force_dvbv3, lna, lnb, sat_number, freq_bpf;
	unsigned diseqc_wait, dont_add_new_freqs, timeout_multiply;
	unsigned other_nit;
	enum dvb_file_formats input_format, output_format;

	/* Used by status print */
	unsigned n_status_lines;
};

static const struct argp_option options[] = {
	{"adapter",	'a',	"adapter#",		0, "use given adapter (default 0)", 0},
	{"frontend",	'f',	"frontend#",		0, "use given frontend (default 0)", 0},
	{"demux",	'd',	"demux#",		0, "use given demux (default 0)", 0},
	{"lnbf",	'l',	"LNBf_type",		0, "type of LNBf to use. 'help' lists the available ones", 0},
	{"lna",		'w',	"LNA (0, 1, -1)",	0, "enable/disable/auto LNA power", 0},
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
	{"output-format", 'O',	"format",		0, "Output format: VDR, CHANNEL, ZAP, DVBV5 (default: DVBV5)", 0},
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

static int check_frontend(void *__args,
			  struct dvb_v5_fe_parms *parms)
{
	struct arguments *args = __args;
	int rc, i;
	fe_status_t status;

	args->n_status_lines = 0;
	for (i = 0; i < args->timeout_multiply * 40; i++) {
		if (parms->abort)
			return 0;
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

static int run_scan(struct arguments *args,
		    struct dvb_v5_fe_parms *parms)
{
	struct dvb_file *dvb_file = NULL, *dvb_file_new = NULL;
	struct dvb_entry *entry;
	int count = 0, dmx_fd, shift;
	uint32_t freq, sys;
	enum dvb_sat_polarization pol;

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
		struct dvb_v5_descriptors *dvb_scan_handler = NULL;

		/*
		 * If the channel file has duplicated frequencies, or some
		 * entries without any frequency at all, discard.
		 */
		if (dvb_retrieve_entry_prop(entry, DTV_FREQUENCY, &freq))
			continue;

		shift = dvb_estimate_freq_shift(parms);

		if (dvb_retrieve_entry_prop(entry, DTV_POLARIZATION, &pol))
			pol = POLARIZATION_OFF;

		if (!dvb_new_freq_is_needed(dvb_file->first_entry, entry,
					    freq, pol, shift))
			continue;

		count++;
		dvb_log("Scanning frequency #%d %d", count, freq);

		/*
		 * Run the scanning logic
		 */

		dvb_scan_handler = dvb_scan_transponder(parms, entry, dmx_fd,
							&check_frontend, args,
							args->other_nit,
							args->timeout_multiply);

		if (parms->abort) {
			dvb_scan_free_handler_table(dvb_scan_handler);
			break;
		}
		if (!dvb_scan_handler)
			continue;

		/*
		 * Store the service entry
		 */
		dvb_store_channel(&dvb_file_new, parms, dvb_scan_handler,
				  args->get_detected, args->get_nit);

		/*
		 * Add new transponders based on NIT table information
		 */
		if (!args->dont_add_new_freqs)
			dvb_add_scaned_transponders(parms, dvb_scan_handler,
						    dvb_file->first_entry, entry);

		/*
		 * Free the scan handler associated with the transponder
		 */

		dvb_scan_free_handler_table(dvb_scan_handler);
	}

	if (dvb_file_new)
		dvb_write_file_format(args->output, dvb_file_new,
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
		args->n_adapter++;
		break;
	case 'f':
		args->frontend = strtoul(optarg, NULL, 0);
		args->adapter_fe = args->adapter;
		break;
	case 'd':
		args->demux = strtoul(optarg, NULL, 0);
		args->adapter_dmx = args->adapter;
		break;
	case 'w':
		if (!strcasecmp(optarg,"on")) {
			args->lna = 1;
		} else if (!strcasecmp(optarg,"off")) {
			args->lna = 0;
		} else if (!strcasecmp(optarg,"auto")) {
			args->lna = LNA_AUTO;
		} else {
			int val = strtoul(optarg, NULL, 0);
			if (!val)
				args->lna = 0;
			else if (val > 0)
				args->lna = 1;
			else
				args->lna = LNA_AUTO;
		}
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
		args->input_format = dvb_parse_format(optarg);
		break;
	case 'O':
		args->output_format = dvb_parse_format(optarg);
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

static int *timeout_flag;

static void do_timeout(int x)
{
	(void)x;
	if (*timeout_flag == 0) {
		*timeout_flag = 1;
		alarm(5);
		signal(SIGALRM, do_timeout);
	} else {
		/* something has gone wrong ... exit */
		exit(1);
	}
}


int main(int argc, char **argv)
{
	struct arguments args;
	int err, lnb = -1,idx = -1;
	int r;
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
	args.adapter = (unsigned)-1;
	args.lna = LNA_AUTO;

	argp_parse(&argp, argc, argv, 0, &idx, &args);
	if (args.timeout_multiply == 0)
		args.timeout_multiply = 1;

	if (args.n_adapter == 1) {
		args.adapter_fe = args.adapter;
		args.adapter_dmx = args.adapter;
	}

	if (args.lnb_name) {
		lnb = dvb_sat_search_lnb(args.lnb_name);
		if (lnb < 0) {
			printf("Please select one of the LNBf's below:\n");
			dvb_print_all_lnb();
			exit(1);
		} else {
			printf("Using LNBf ");
			dvb_print_lnb(lnb);
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

	r = asprintf(&args.demux_dev,
		 "/dev/dvb/adapter%i/demux%i", args.adapter_dmx, args.demux);
	if (r < 0) {
		fprintf(stderr, "asprintf error\n" );
		return -1;
	}

	if (verbose)
		fprintf(stderr, "using demux '%s'\n", args.demux_dev);

	struct dvb_v5_fe_parms *parms = dvb_fe_open(args.adapter_fe,
						    args.frontend,
						    verbose, args.force_dvbv3);
	if (!parms) {
		free(args.demux_dev);
		return -1;
	}
	if (lnb >= 0)
		parms->lnb = dvb_sat_get_lnb(lnb);
	if (args.sat_number >= 0)
		parms->sat_number = args.sat_number % 3;
	parms->diseqc_wait = args.diseqc_wait;
	parms->freq_bpf = args.freq_bpf;
	parms->lna = args.lna;

	timeout_flag = &parms->abort;
	signal(SIGTERM, do_timeout);
	signal(SIGINT, do_timeout);

	err = run_scan(&args, parms);

	dvb_fe_close(parms);
	free(args.demux_dev);

	return err;
}
