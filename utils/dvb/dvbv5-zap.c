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
 * Based on dvb-apps tzap utility, made by:
 *	Bernard Hatt 24/2/04
 */

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1
#define _LARGEFILE64_SOURCE 1

/*
 * Use a buffer big enough at least 1 second of data. It is interesting
 * To have it multiple of a page. So, define it as a multiply of
 * 4096.
 */
#define DVB_BUF_SIZE	(4096 * 8 * 188)

/*
 * Size of the buffer on read operations. The better is if it is
 * smaller than DVB_BUF_SIZE, as we want to give more time for
 * write() syscalls to be able to flush data.
 */
#define BUFLEN (188 * 512)

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <argp.h>
#include <sys/time.h>
#include <time.h>

#include <config.h>

#ifdef ENABLE_NLS
# define _(string) gettext(string)
# include "gettext.h"
# include <locale.h>
# include <langinfo.h>
# include <iconv.h>
#else
# define _(string) string
#endif

# define N_(string) string

#include <linux/dvb/dmx.h>
#include "libdvbv5/dvb-file.h"
#include "libdvbv5/dvb-demux.h"
#include "libdvbv5/dvb-dev.h"
#include "libdvbv5/dvb-scan.h"
#include "libdvbv5/header.h"
#include "libdvbv5/countries.h"

#define CHANNEL_FILE	"channels.conf"
#define PROGRAM_NAME	"dvbv5-zap"


#ifndef O_LARGEFILE
#  define O_LARGEFILE 0
#endif


const int NANO_SECONDS_IN_SEC = 1000000000;

const char *argp_program_version = PROGRAM_NAME " version " V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <m.chehab@samsung.com>";

struct arguments {
	char *confname, *lnb_name, *output, *demux_dev, *dvr_dev, *dvr_fname;
	char *filename, *dvr_pipe;
	unsigned adapter, frontend, demux, get_detected, get_nit;
	int lna, lnb, sat_number;
	unsigned diseqc_wait, silent, verbose, frontend_only, freq_bpf;
	unsigned timeout, dvr, rec_psi, exit_after_tuning;
	unsigned n_apid, n_vpid, all_pids;
	enum dvb_file_formats input_format, output_format;
	unsigned traffic_monitor, low_traffic, non_human, port;
	char *search, *server;
	const char *cc;

	/* Used by status print */
	unsigned n_status_lines;
};

static const struct argp_option options[] = {
	{"adapter",	'a', N_("adapter#"),		0, N_("use given adapter (default 0)"), 0},
	{"audio_pid",	'A', N_("audio_pid#"),		0, N_("audio pid program to use (default 0)"), 0},
	{"channels",	'c', N_("file"),		0, N_("read channels list from 'file'"), 0},
	{"demux",	'd', N_("demux#"),		0, N_("use given demux (default 0)"), 0},
	{"frontend",	'f', N_("frontend#"),		0, N_("use given frontend (default 0)"), 0},
	{"input-format", 'I',	N_("format"),		0, N_("Input format: ZAP, CHANNEL, DVBV5 (default: DVBV5)"), 0},
	{"lna",		'w', N_("LNA (0, 1, -1)"),	0, N_("enable/disable/auto LNA power"), 0},
	{"lnbf",	'l', N_("LNBf_type"),		0, N_("type of LNBf to use. 'help' lists the available ones"), 0},
	{"search",	'L', N_("string"),		0, N_("search/look for a string inside the traffic"), 0},
	{"monitor",	'm', NULL,			0, N_("monitors the DVB traffic"), 0},
	{"output",	'o', N_("file"),		0, N_("output filename (use -o - for stdout)"), 0},
	{"pat",		'p', NULL,			0, N_("add pat and pmt to TS recording (implies -r)"), 0},
	{"all-pids",	'P', NULL,			0, N_("don't filter any pids. Instead, outputs all of them"), 0 },
	{"record",	'r', NULL,			0, N_("set up /dev/dvb/adapterX/dvr0 for TS recording"), 0},
	{"silence",	's', NULL,			0, N_("increases silence (can be used more than once)"), 0},
	{"sat_number",	'S', N_("satellite_number"),	0, N_("satellite number. If not specified, disable DISEqC"), 0},
	{"timeout",	't', N_("seconds"),		0, N_("timeout for zapping and for recording"), 0},
	{"freq_bpf",	'U', N_("frequency"),		0, N_("SCR/Unicable band-pass filter frequency to use, in kHz"), 0},
	{"verbose",	'v', NULL,			0, N_("verbose debug messages (can be used more than once)"), 0},
	{"video_pid",	'V', N_("video_pid#"),		0, N_("video pid program to use (default 0)"), 0},
	{"wait",	'W', N_("time"),		0, N_("adds additional wait time for DISEqC command completion"), 0},
	{"exit",	'x', NULL,			0, N_("exit after tuning"), 0},
	{"low_traffic",	'X', N_("packets_per_sec"),	0, N_("sets DVB low traffic traffic threshold. PIDs with less than this amount of packets per second will be ignored. Default: 1 packet per second"), 0},
	{"cc",		'C', N_("country_code"),	0, N_("Set the default country to be used (in ISO 3166-1 two letter code)"), 0},
	{"non-numan",	'N', NULL,			0, N_("Non-human formatted stats (useful for scripts)"), 0},
	{"server",	'H', N_("SERVER"),		0, N_("dvbv5-daemon host IP address"), 0},
	{"tcp-port",	'T', N_("PORT"),		0, N_("dvbv5-daemon host tcp port"), 0},
	{"dvr-pipe",	'D', N_("PIPE"),		0, N_("Named pipe for DVR output, when using remote access (by default: /tmp/dvr-pipe)"), 0},
	{"help",        '?', 0,				0, N_("Give this help list"), -1},
	{"usage",	-3,  0,				0, N_("Give a short usage message")},
	{"version",	-4,  0,				0, N_("Print program version"), -1},
	{ 0, 0, 0, 0, 0, 0 }
};

static int timeout_flag = 0;

#define ERROR(x...)                                                     \
	do {                                                            \
		fprintf(stderr, _("ERROR: "));                             \
		fprintf(stderr, x);                                     \
		fprintf(stderr, "\n");                                 \
	} while (0)

#define PERROR(x...)                                                    \
	do {                                                            \
		fprintf(stderr, _("ERROR: "));                             \
		fprintf(stderr, x);                                     \
		fprintf(stderr, " (%s)\n", strerror(errno));		\
	} while (0)

#define monitor_log(msg, args...)						\
	do {									\
		struct timespec __now = { 0 };					\
		float __diff;							\
										\
		clock_gettime(CLOCK_MONOTONIC, &__now);				\
		__diff = __now.tv_sec * 1.					\
			 + __now.tv_nsec *1. / NANO_SECONDS_IN_SEC;		\
		fprintf(stderr, msg, __diff, ##args);				\
	} while (0)


static int parse(struct arguments *args,
		 struct dvb_v5_fe_parms *parms,
		 char *channel,
		 int *vpid, int *apid, int *sid)
{
	struct dvb_file *dvb_file;
	struct dvb_entry *entry;
	int i;
	uint32_t sys;

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
	case SYS_DTMB:
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

	for (entry = dvb_file->first_entry; entry != NULL; entry = entry->next) {
		if (entry->channel && !strcmp(entry->channel, channel))
			break;
		if (entry->vchannel && !strcmp(entry->vchannel, channel))
			break;
	}
	/*
	 * Give a second shot, using a case insensitive seek
	 */
	if (!entry) {
		for (entry = dvb_file->first_entry; entry != NULL;
		     entry = entry->next) {
			if (entry->channel && !strcasecmp(entry->channel, channel))
				break;
		}
	}

	/*
	 * When this tool is used to just tune to a channel, to monitor it or
	 * to capture all PIDs, all it needs is a frequency.
	 * So, let the tool to accept a frequency as the tuning channel on those
	 * cases.
	 * This way, a file in "channel" format can be used instead of a zap file.
	 * It is also easier to use it for testing purposes.
	 */
	if (!entry && (!args->dvr && !args->rec_psi)) {
		uint32_t f, freq = atoi(channel);
		if (freq) {
			for (entry = dvb_file->first_entry; entry != NULL;
			entry = entry->next) {
				dvb_retrieve_entry_prop(entry, DTV_FREQUENCY, &f);
				if (f == freq)
					break;
			}

		}
	}

	if (!entry) {
		ERROR("Can't find channel");
		dvb_file_free(dvb_file);
		return -3;
	}

	/*
	 * Both the DVBv5 format and the command line parameters may
	 * specify the LNBf. If both have the definition, use the one
	 * provided by the command line parameter, overriding the one
	 * stored in the channel file.
	 */
	if (entry->lnb && !parms->lnb) {
		int lnb = dvb_sat_search_lnb(entry->lnb);
		if (lnb == -1) {
			ERROR("unknown LNB %s\n", entry->lnb);
			dvb_file_free(dvb_file);
			return -1;
		}
		parms->lnb = dvb_sat_get_lnb(lnb);
	}

	if (parms->sat_number < 0 && entry->sat_number >= 0)
		parms->sat_number = entry->sat_number;

	if (entry->video_pid) {
		if (args->n_vpid < entry->video_pid_len)
			*vpid = entry->video_pid[args->n_vpid];
		else
			*vpid = entry->video_pid[0];
	}
	if (entry->audio_pid) {
		if (args->n_apid < entry->audio_pid_len)
			*apid = entry->audio_pid[args->n_apid];
		else
		*apid = entry->audio_pid[0];
	}
	if (entry->other_el_pid) {
		int i, type = -1;
		for (i = 0; i < entry->other_el_pid_len; i++) {
			if (type != entry->other_el_pid[i].type) {
				type = entry->other_el_pid[i].type;
				if (i)
					fprintf(stderr, "\n");
				fprintf(stderr, _("service has pid type %02x: "), type);
			}
			fprintf(stderr, " %d", entry->other_el_pid[i].pid);
		}
		fprintf(stderr, "\n");
	}
	*sid = entry->service_id;

        /* First of all, set the delivery system */
	dvb_retrieve_entry_prop(entry, DTV_DELIVERY_SYSTEM, &sys);
	dvb_set_compat_delivery_system(parms, sys);

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

	dvb_file_free(dvb_file);
	return 0;
}

static int setup_frontend(struct arguments *args,
			  struct dvb_v5_fe_parms *parms)
{
	int rc;
	uint32_t freq;

	if (args->silent < 2) {
		rc = dvb_fe_retrieve_parm(parms, DTV_FREQUENCY, &freq);
		if (rc < 0) {
			ERROR("can't get the frequency");
			return -1;
		}
		fprintf(stderr, _("tuning to %i Hz\n"), freq);
	}

	rc = dvb_fe_set_parms(parms);
	if (rc < 0) {
		ERROR("dvb_fe_set_parms failed");
		return -1;
	}

	return 0;
}

static void do_timeout(int x)
{
	(void)x;

	if (timeout_flag == 0) {
		timeout_flag = 1;
		alarm(2);
		signal(SIGALRM, do_timeout);
	} else {
		/* something has gone wrong ... exit */
		exit(1);
	}
}

static int print_non_human_stats(FILE *fd, struct dvb_v5_fe_parms *parms)
{
	int rc;
	fe_status_t status;
	uint32_t snr = 0, _signal = 0, quality = 0;
	uint32_t ber = 0, per = 0, pre_ber = 0, uncorrected_blocks = 0;

	rc = dvb_fe_get_stats(parms);
	if (rc < 0) {
		PERROR("dvb_fe_get_stats failed");
		return -1;
	}

	dvb_fe_retrieve_stats(parms, DTV_STATUS, &status);
	dvb_fe_retrieve_stats(parms, DTV_QUALITY, &quality);
	dvb_fe_retrieve_stats(parms, DTV_STAT_SIGNAL_STRENGTH, &_signal);
	dvb_fe_retrieve_stats(parms, DTV_STAT_CNR, &snr);
	dvb_fe_retrieve_stats(parms, DTV_BER, &ber);
	dvb_fe_retrieve_stats(parms, DTV_STAT_ERROR_BLOCK_COUNT, &uncorrected_blocks);
	dvb_fe_retrieve_stats(parms, DTV_PRE_BER, &pre_ber);
	dvb_fe_retrieve_stats(parms, DTV_PER, &per);

	fprintf(fd,"status %02x | quality %02x | signal %04x | snr %04x | ber %08x | unc %08x | pre_ber %08x | per %08x | ",
		status, quality, _signal, snr, ber, uncorrected_blocks, pre_ber, per);

	if (status & FE_HAS_LOCK)
		fprintf(fd, "FE_HAS_LOCK");

	fprintf(fd, "\n");
	fflush(fd);

	return 0;
}

static int print_frontend_stats(FILE *fd,
				struct arguments *args,
				struct dvb_v5_fe_parms *parms)
{
	char buf[512], *p;
	int rc, i, len, show;
	uint32_t status = 0;

	if (args->non_human)
		return print_non_human_stats(fd, parms);

	/* Move cursor up and cleans down */
	if (isatty(fileno(fd)) && args->n_status_lines)
		fprintf(fd, "\r\x1b[%dA\x1b[J", args->n_status_lines);

	args->n_status_lines = 0;

	rc = dvb_fe_get_stats(parms);
	if (rc) {
		ERROR("dvb_fe_get_stats failed");
		return -1;
	}

	p = buf;
	len = sizeof(buf);
	dvb_fe_snprintf_stat(parms,  DTV_STATUS, NULL, 0, &p, &len, &show);

	for (i = 0; i < MAX_DTV_STATS; i++) {
		show = 1;

		dvb_fe_snprintf_stat(parms, DTV_QUALITY, _("Quality"),
				     i, &p, &len, &show);

		dvb_fe_snprintf_stat(parms, DTV_STAT_SIGNAL_STRENGTH, _("Signal"),
				     i, &p, &len, &show);

		dvb_fe_snprintf_stat(parms, DTV_STAT_CNR, _("C/N"),
				     i, &p, &len, &show);

		dvb_fe_snprintf_stat(parms, DTV_STAT_ERROR_BLOCK_COUNT, _("UCB"),
				     i,  &p, &len, &show);

		dvb_fe_snprintf_stat(parms, DTV_BER, _("postBER"),
				     i,  &p, &len, &show);

		dvb_fe_snprintf_stat(parms, DTV_PRE_BER, _("preBER"),
				     i,  &p, &len, &show);

		dvb_fe_snprintf_stat(parms, DTV_PER, _("PER"),
				     i,  &p, &len, &show);

		if (p != buf) {
			if (args->n_status_lines)
				fprintf(fd, "\t%s\n", buf);
			else
				fprintf(fd, "%s\n", buf);

			args->n_status_lines++;

			p = buf;
			len = sizeof(buf);
		}
	}

	fflush(fd);

	/* While not lock, display status on a new line */
	dvb_fe_retrieve_stats(parms, DTV_STATUS, &status);
	if (!isatty(fileno(fd)) || !(status & FE_HAS_LOCK))
		fprintf(fd, "\n");

	return 0;
}

static int check_frontend(struct arguments *args,
			  struct dvb_v5_fe_parms *parms)
{
	int rc;
	fe_status_t status = 0;
	do {
		rc = dvb_fe_get_stats(parms);
		if (rc) {
			ERROR("dvb_fe_get_stats failed");
			usleep(1000000);
			continue;
		}

		status = 0;
		rc = dvb_fe_retrieve_stats(parms, DTV_STATUS, &status);
		if (!args->silent)
			print_frontend_stats(stderr, args, parms);
		if (status & FE_HAS_LOCK)
			break;
		usleep(1000000);
	} while (!timeout_flag);
	if (args->silent < 2)
		print_frontend_stats(stderr, args, parms);

	return status & FE_HAS_LOCK;
}

static void get_show_stats(FILE *fp, struct arguments *args,
			   struct dvb_v5_fe_parms *parms,
			   int loop)
{
	int rc;

	args->n_status_lines = 0;
	do {
		rc = dvb_fe_get_stats(parms);
		if (!rc)
			print_frontend_stats(fp, args, parms);
		if (!timeout_flag && loop)
			usleep(1000000);
	} while (!timeout_flag && loop);
}

static struct timespec *elapsed_time(struct timespec *start)
{
	static struct timespec elapsed;
	struct timespec end;

	if (!start->tv_sec && !start->tv_nsec)
		return NULL;

	if (clock_gettime(CLOCK_MONOTONIC, &end))
		return NULL;

	elapsed.tv_sec = end.tv_sec - start->tv_sec;
	elapsed.tv_nsec = end.tv_nsec - start->tv_nsec;
	if (elapsed.tv_nsec < 0) {
		elapsed.tv_sec--;
		elapsed.tv_nsec += NANO_SECONDS_IN_SEC;
	}
	return &elapsed;
}

static void copy_to_file(struct dvb_open_descriptor *in_fd, int out_fd,
			 int timeout, int silent)
{
	char buf[BUFLEN];
	int r, first = 1;
	long long int rc = 0LL;
	struct timespec start, *elapsed;

	while (timeout_flag == 0) {
		r = dvb_dev_read(in_fd, buf, sizeof(buf));
		if (r < 0) {
			if (r == -EOVERFLOW) {
				elapsed = elapsed_time(&start);
				if (!elapsed)
					fprintf(stderr, _("buffer overrun at %lld\n"), rc);
				else
					fprintf(stderr, _("buffer overrun after %lld.%02ld seconds\n"),
						(long long)elapsed->tv_sec,
						elapsed->tv_nsec / 10000000);
				continue;
			}
			ERROR("Read failed");
			break;
		}

		/*
		 * It takes a while for a DVB device to start streaming, as the
		 * hardware may be waiting for some locks. The safest way to
		 * ensure that a program record will have the start amount of
		 * time specified by the user is to restart the timeout alarm
		 * here, after the first succeded read.
		 */
		if (first) {
			if (timeout > 0)
				alarm(timeout);

			clock_gettime(CLOCK_MONOTONIC, &start);
			first = 0;
		}

		if (write(out_fd, buf, r) < 0) {
			PERROR(_("Write failed"));
			break;
		}

		rc += r;
	}
	if (silent < 2) {
		if (timeout)
			fprintf(stderr, _("received %lld bytes (%lld Kbytes/sec)\n"), rc,
				rc / (1024 * timeout));
		else
			fprintf(stderr, _("received %lld bytes\n"), rc);
	}
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
	case 't':
		args->timeout = strtoul(optarg, NULL, 0);
		break;
	case 'I':
		args->input_format = dvb_parse_format(optarg);
		break;
	case 'o':
		args->filename = strdup(optarg);
		/* fall through */
	case 'r':
		args->dvr = 1;
		break;
	case 'p':
		args->rec_psi = 1;
		break;
	case 'x':
		args->exit_after_tuning = 1;
		break;
	case 'c':
		args->confname = strdup(optarg);
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
		args->lnb_name = strdup(optarg);
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
	case 's':
		args->silent++;
		break;
	case 'v':
		args->verbose++;
		break;
	case 'A':
		args->n_apid = strtoul(optarg, NULL, 0);
		break;
	case 'V':
		args->n_vpid = strtoul(optarg, NULL, 0);
		break;
	case 'P':
		args->all_pids++;
		break;
	case 'm':
		args->traffic_monitor = 1;
		break;
	case 'N':
		args->non_human = 1;
		break;
	case 'X':
		args->low_traffic = atoi(optarg);
		break;
	case 'L':
		args->search = strdup(optarg);
		break;
	case 'C':
		args->cc = strndup(optarg, 2);
		break;
	case 'H':
		args->server = strdup(optarg);
		break;
	case 'T':
		args->port = atoi(optarg);
		break;
	case 'D':
		args->dvr_pipe = strdup(optarg);
		break;
	case '?':
		argp_state_help(state, state->out_stream,
				ARGP_HELP_SHORT_USAGE | ARGP_HELP_LONG
				| ARGP_HELP_DOC);
		fprintf(state->out_stream, _("\nReport bugs to %s.\n"), argp_program_bug_address);
		exit(0);
	case -4:
		fprintf (state->out_stream, "%s\n", argp_program_version);
		exit(0);
	case -3:
		argp_state_help(state, state->out_stream, ARGP_HELP_USAGE);
		exit(0);
	default:
		return ARGP_ERR_UNKNOWN;
	};
	return 0;
}

static char *print_bytes(float val)
{
	static char buf[20];
	char *prefix = "";

	if (val >= 500 * 1024 * 1024) {
		prefix = "G";
		val /= 1024 * 1024 * 1024.;
	} else if (val >= 500 * 1024) {
		prefix = "M";
		val /= 1024 * 1024.;
	} else if (val >= 500) {
		prefix = "K";
		val /= 1024.;
	}
	if (*prefix) {
		if (snprintf(buf, sizeof(buf), "%8.3f %s", val, prefix) <= 0)
			return "      NaN ";
	} else {
		if (snprintf(buf, sizeof(buf), "%9.3f ", val) <= 0)
			return "      NaN ";
	}

	return buf;
}

int do_traffic_monitor(struct arguments *args, struct dvb_device *dvb,
		       int out_fd, int timeout)
{
	struct dvb_open_descriptor *fd, *dvr_fd;
	struct timespec startt;
	struct dvb_v5_fe_parms *parms = dvb->fe_parms;
	unsigned long long pidt[0x2001], wait, cont_err = 0;
	unsigned long long err_cnt[0x2000];
	signed char pid_cont[0x2000];
	int i, first = 1;

	memset(pidt, 0, sizeof(pidt));
	memset(err_cnt, 0, sizeof(err_cnt));
	memset(pid_cont, 0, sizeof(pid_cont));

	args->exit_after_tuning = 1;
	check_frontend(args, parms);

	dvr_fd = dvb_dev_open(dvb, args->dvr_dev, O_RDONLY);
	if (!dvr_fd)
		return -1;

	fprintf(stderr, _("dvb_dev_set_bufsize: buffer set to %d\n"), DVB_BUF_SIZE);
	dvb_dev_set_bufsize(dvr_fd, DVB_BUF_SIZE);

	fd = dvb_dev_open(dvb, args->demux_dev, O_RDWR);
	if (!fd) {
		dvb_dev_close(dvr_fd);
		return -1;
	}

	if (args->silent < 2)
		fprintf(stderr, _("  dvb_set_pesfilter to 0x2000\n"));
	if (dvb_dev_dmx_set_pesfilter(fd, 0x2000, DMX_PES_OTHER,
				      DMX_OUT_TS_TAP, 0) < 0) {
		dvb_dev_close(dvr_fd);
		dvb_dev_close(fd);
		return -1;
	}

	if (clock_gettime(CLOCK_MONOTONIC, &startt)) {
		fprintf(stderr, _("Can't get timespec\n"));
                return -1;
	}

	wait = 1000;

	monitor_log(_("%.2fs: Starting capture\n"));
	while (1) {
		struct timespec *elapsed;
		unsigned char buffer[BUFLEN];
		int pid, ok, diff;
		ssize_t r;

		if (timeout_flag)
			break;

		if ((r = dvb_dev_read(dvr_fd, buffer, BUFLEN)) <= 0) {
			if (r == -EOVERFLOW) {
				monitor_log(_("%.2fs: buffer overrun\n"));
				continue;
			}
			monitor_log(_("%.2fs: read() returned error %zd\n"), r);
			break;
		}

		/*
		 * It takes a while for a DVB device to start streaming, as the
		 * hardware may be waiting for some locks. The safest way to
		 * ensure that a program record will have the start amount of
		 * time specified by the user is to restart the timeout alarm
		 * here, after the first succeded read.
		 */
		if (first) {
			if (timeout > 0)
				alarm(timeout);
			first = 0;
		}
		if (out_fd >= 0) {
			if (write(out_fd, buffer, r) < 0) {
				PERROR(_("Write failed"));
				break;
			}
		}
		if (r != BUFLEN) {
			monitor_log(_("%.2fs: only read %zd bytes\n"), r);
			break;
		}

		for (i = 0; i < BUFLEN; i += 188) {
			struct dvb_ts_packet_header *h = (void *)&buffer[i];
			if (h->sync_byte != 0x47) {
				monitor_log(_("%.2fs: invalid sync byte. Discarding %zd bytes\n"), r);
				continue;
			}

			bswap16(h->bitfield);

#if 0
			/*
			 * ITU-T Rec. H.222.0 decoders shall discard Transport
			 * Stream packets with the adaptation_field_control
			 * field set to a value of '00' (invalid). Packets with
			 * a value of '01' are NULL packets. Yet, as those are
			 * actually part of the stream, we won't be discarding,
			 * as we want to take them into account for traffic
			 * estimation purposes.
			 */
			if (h->adaptation_field_control == 0)
				continue;
#endif
			ok = 1;
			pid = h->pid;

			if (pid > 0x1fff) {
				monitor_log(_("%.2fs: invalid pid: 0x%04x\n"),
					    pid);
				pid = 0x1fff;
			}

			/*
			 * After 1 second of processing, check if are there
			 * any issues with regards to frame continuity for
			 * non-NULL packets.
			 *
			 * According to ITU-T H.222.0 | ISO/IEC 13818-1, the
			 * continuity counter isn't incremented if the packet
			 * is 00 or 10. It is only incremented on odd values.
			 *
			 * Also, don't check continuity errors on the first
			 * second, as the frontend is still starting streaming
			 */
			if (pid < 0x1fff && h->adaptation_field_control & 1) {
				int discontinued = 0;

				if (err_cnt[pid] < 0)
					err_cnt[pid] = 0;

				if (h->adaptation_field_control & 2) {
					if (h->adaptation_field_length >= 1) {
						discontinued = h->discontinued;
					} else {
						monitor_log(_("%.2fs: pid %d has adaption layer, but size is too small!\n"),
							    pid);
					}
				}

				if (wait < 2000)
					discontinued = 1;

				if (!discontinued && pid_cont[pid] >= 0) {
					unsigned int next = (pid_cont[pid] + 1) % 16;
					if (next != h->continuity_counter) {
						monitor_log(_("%.2fs: pid %d, expecting %d received %d\n"),
							    pid, next,
							    h->continuity_counter);
						discontinued = 1;
						cont_err++;
						err_cnt[pid]++;
					}
				}
				if (discontinued)
					pid_cont[pid] = -1;
				else
					pid_cont[pid] = h->continuity_counter;
			}

			if (args->search) {
				int i, sl = strlen(args->search);
				ok = 0;
				if (pid != 0x1fff) {
					for (i = 0; i < (188 - sl); ++i) {
						if (!memcmp((char *)h + i, args->search, sl))
							ok = 1;
					}
				}
			}

			if (ok) {
				pidt[pid]++;
				pidt[0x2000]++;
			}
		}

		elapsed = elapsed_time(&startt);
		if (!elapsed)
			diff = wait;
		else
			diff = (unsigned long long)elapsed->tv_sec * 1000
				+ elapsed->tv_nsec * 1000 / NANO_SECONDS_IN_SEC;

		if (diff > wait) {
			unsigned long long other_pidt = 0, other_err_cnt = 0;

			if (isatty(STDOUT_FILENO))
				printf("\x1b[1H\x1b[2J");

			args->n_status_lines = 0;
			printf(_(" PID           FREQ         SPEED       TOTAL\n"));
			int _pid = 0;
			for (_pid = 0; _pid < 0x2000; _pid++) {
				if (pidt[_pid]) {
					if (args->low_traffic && (pidt[_pid] * 1000. / diff) < args->low_traffic) {
						other_pidt += pidt[_pid];
						other_err_cnt += err_cnt[_pid];
						continue;
					}
					printf("%5d %9.2f p/s %sbps ",
						_pid,
						pidt[_pid] * 1000. / diff,
						print_bytes(pidt[_pid] * 1000. * 8 * 188/ diff));
					if (pidt[_pid] * 188 / 1024)
						printf("%8llu KB", (pidt[_pid] * 188 + 512) / 1024);
					else
						printf(" %8llu B", pidt[_pid] * 188);
					if (err_cnt[_pid] > 0)
						printf(" %8llu continuity errors",
						       err_cnt[_pid]);

					printf("\n");
				}
			}
			if (other_pidt) {
				printf(_("OTHER"));
				printf(" %9.2f p/s %sbps ",
					other_pidt * 1000. / diff,
					print_bytes(other_pidt * 1000. * 8 * 188/ diff));
				if (other_pidt * 188 / 1024)
					printf("%8llu KB", (other_pidt * 188 + 512) / 1024);
				else
					printf(" %8llu B", other_pidt * 188);
				if (other_err_cnt > 0)
					printf(" %8llu continuity errors",
					       other_err_cnt);
				printf("\n");
			}

			/* 0x2000 is the total traffic */
			printf("TOT %11.2f p/s %sbps %8llu KB\n",
				pidt[_pid] * 1000. / diff,
				print_bytes(pidt[_pid] * 1000. * 8 * 188/ diff),
				(pidt[_pid] * 188 + 512) / 1024);
			printf("\n");
			get_show_stats(stdout, args, parms, 0);
			wait += 1000;
			if (cont_err)
				printf("CONTINUITY errors: %llu\n", cont_err);
		}
	}
	monitor_log(_("%.2fs: Stopping capture\n"));
	dvb_dev_close(dvr_fd);
	dvb_dev_close(fd);
	return 0;
}

static void set_signals(struct arguments *args)
{
	signal(SIGTERM, do_timeout);
	signal(SIGINT, do_timeout);
	if (args->timeout > 0) {
		signal(SIGALRM, do_timeout);
		alarm(args->timeout);
	}
}

int main(int argc, char **argv)
{
	struct arguments args;
	char *homedir = getenv("HOME");
	char *channel = NULL;
	int lnb = -1, idx = -1;
	int vpid = -1, apid = -1, sid = -1;
	int pmtpid = 0;
	struct dvb_open_descriptor *pat_fd = NULL, *pmt_fd = NULL;
	struct dvb_open_descriptor *sid_fd = NULL, *dvr_fd = NULL;
	struct dvb_open_descriptor *audio_fd = NULL, *video_fd = NULL;
	int file_fd = -1;
	int err = -1;
	int r, ret;
	struct dvb_v5_fe_parms *parms = NULL;
	struct dvb_device *dvb;
	struct dvb_dev_list *dvb_dev;
	const struct argp argp = {
		.options = options,
		.parser = parse_opt,
		.doc = N_("DVB zap utility"),
		.args_doc = N_("<channel name> [or <frequency> if in monitor mode]"),
	};

#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);
#endif

	memset(&args, 0, sizeof(args));
	args.sat_number = -1;
	args.lna = LNA_AUTO;
	args.input_format = FILE_DVBV5;
	args.dvr_pipe = "/tmp/dvr-pipe";
	args.low_traffic = 1;

	if (argp_parse(&argp, argc, argv, ARGP_NO_HELP | ARGP_NO_EXIT, &idx, &args)) {
		argp_help(&argp, stderr, ARGP_HELP_SHORT_USAGE, PROGRAM_NAME);
		return -1;
	}

	if (idx < argc)
		channel = argv[idx];

	if (!channel) {
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, PROGRAM_NAME);
		return -1;
	}

	if (args.input_format == FILE_UNKNOWN) {
		ERROR("Please specify a valid format\n");
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, PROGRAM_NAME);
		return -1;
	}

	if (!args.traffic_monitor && args.search) {
		ERROR("search string can be used only on monitor mode\n");
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, PROGRAM_NAME);
		return -1;
	}

	if (args.lnb_name) {
		lnb = dvb_sat_search_lnb(args.lnb_name);
		if (lnb < 0) {
			printf(_("Please select one of the LNBf's below:\n"));
			dvb_print_all_lnb();
			exit(1);
		} else {
			printf(_("Using LNBf "));
			dvb_print_lnb(lnb);
		}
	}

	dvb = dvb_dev_alloc();
	if (!dvb)
		return -1;

	if (args.server && args.port) {
		printf(_("Connecting to %s:%d\n"), args.server, args.port);
		ret = dvb_dev_remote_init(dvb, args.server, args.port);
		if (ret < 0)
			return -1;
	}

	dvb_dev_set_log(dvb, args.verbose, NULL);
	dvb_dev_find(dvb, NULL, NULL);
	parms = dvb->fe_parms;

	dvb_dev = dvb_dev_seek_by_adapter(dvb, args.adapter, args.demux, DVB_DEVICE_DEMUX);
	if (!dvb_dev) {
		fprintf(stderr, _("Couldn't find demux device node\n"));
		dvb_dev_free(dvb);
		return -1;
	}
	args.demux_dev = dvb_dev->sysname;

	dvb_dev = dvb_dev_seek_by_adapter(dvb, args.adapter, args.demux, DVB_DEVICE_DVR);
	if (!dvb_dev) {
		fprintf(stderr, _("Couldn't find dvr device node\n"));
		dvb_dev_free(dvb);
		return -1;
	}
	args.dvr_dev = dvb_dev->sysname;
	args.dvr_fname = dvb_dev->path;

	if (args.silent < 2)
		fprintf(stderr, _("using demux '%s'\n"), args.demux_dev);

	if (!args.confname) {
		if (!homedir)
			ERROR("$HOME not set");
		r = asprintf(&args.confname, "%s/.tzap/%i/%s",
			 homedir, args.adapter, CHANNEL_FILE);
		if (access(args.confname, R_OK)) {
			free(args.confname);
			r = asprintf(&args.confname, "%s/.tzap/%s",
				homedir, CHANNEL_FILE);
		}
	}
	fprintf(stderr, _("reading channels from file '%s'\n"), args.confname);

	dvb_dev = dvb_dev_seek_by_adapter(dvb, args.adapter, args.frontend,
					  DVB_DEVICE_FRONTEND);
	if (!dvb_dev)
		return -1;

	if (!dvb_dev_open(dvb, dvb_dev->sysname, O_RDWR))
		goto err;
	if (lnb >= 0)
		parms->lnb = dvb_sat_get_lnb(lnb);
	if (args.sat_number >= 0)
		parms->sat_number = args.sat_number;
	parms->diseqc_wait = args.diseqc_wait;
	parms->freq_bpf = args.freq_bpf;
	parms->lna = args.lna;

	r = dvb_fe_set_default_country(parms, args.cc);
	if (r < 0)
		fprintf(stderr, _("Failed to set the country code:%s\n"), args.cc);

	if (parse(&args, parms, channel, &vpid, &apid, &sid))
		goto err;

	if (setup_frontend(&args, parms) < 0)
		goto err;

	if (args.exit_after_tuning) {
		set_signals(&args);
		err = 0;
		check_frontend(&args, parms);
		goto err;
	}

	if (args.traffic_monitor) {
		if (args.filename) {
			file_fd = open(args.filename,
					 O_LARGEFILE |
					 O_WRONLY | O_CREAT | O_TRUNC,
					 0644);
			if (file_fd < 0) {
				PERROR(_("open of '%s' failed"), args.filename);
				return -1;
			}
		}
		set_signals(&args);
		err = do_traffic_monitor(&args, dvb, file_fd, args.timeout);
		goto err;
	}

	if (args.rec_psi) {
		if (sid < 0) {
			fprintf(stderr, _("Service id 0x%04x was not specified at the file\n"),
				sid);
			goto err;
		}

		sid_fd = dvb_dev_open(dvb, args.demux_dev, O_RDWR);
		if (!sid_fd) {
			ERROR("opening sid demux failed");
			return -1;
		}
		pmtpid = dvb_dev_dmx_get_pmt_pid(sid_fd, sid);
		dvb_dev_close(sid_fd);
		if (pmtpid <= 0) {
			fprintf(stderr, _("couldn't find pmt-pid for sid %04x\n"),
				sid);

			goto err;
		}

		pat_fd = dvb_dev_open(dvb, args.demux_dev, O_RDWR);
		if (!pat_fd) {
			ERROR("opening pat demux failed");
			goto err;
		}
		if (dvb_dev_dmx_set_pesfilter(pat_fd, 0, DMX_PES_OTHER,
				args.dvr ? DMX_OUT_TS_TAP : DMX_OUT_DECODER,
				args.dvr ? 64 * 1024 : 0) < 0)
			goto err;

		pmt_fd = dvb_dev_open(dvb, args.demux_dev, O_RDWR);
		if (!pmt_fd) {
			ERROR("opening pmt demux failed");
			goto err;
		}
		if (dvb_dev_dmx_set_pesfilter(pmt_fd, pmtpid, DMX_PES_OTHER,
				args.dvr ? DMX_OUT_TS_TAP : DMX_OUT_DECODER,
				args.dvr ? 64 * 1024 : 0) < 0)
			goto err;
	}

	if (args.all_pids++) {
		vpid = 0x2000;
		apid = 0;
	}
	if (vpid >= 0) {
		if (args.silent < 2) {
			if (vpid == 0x2000)
				fprintf(stderr, _("pass all PID's to TS\n"));
			else
				fprintf(stderr, _("video pid %d\n"), vpid);
		}
		video_fd = dvb_dev_open(dvb, args.demux_dev, O_RDWR);
		if (!video_fd) {
			ERROR("failed opening '%s'", args.demux_dev);
			goto err;
		}

		if (args.silent < 2)
			fprintf(stderr, _("  dvb_set_pesfilter %d\n"), vpid);

		fprintf(stderr, _("dvb_dev_set_bufsize: buffer set to %d\n"), DVB_BUF_SIZE);
		dvb_dev_set_bufsize(video_fd, DVB_BUF_SIZE);

		if (vpid == 0x2000) {
			if (dvb_dev_dmx_set_pesfilter(video_fd, vpid, DMX_PES_OTHER,
					      DMX_OUT_TS_TAP, 0) < 0)
				goto err;
		} else {
			if (dvb_dev_dmx_set_pesfilter(video_fd, vpid, DMX_PES_VIDEO,
				args.dvr ? DMX_OUT_TS_TAP : DMX_OUT_DECODER,
				args.dvr ? 64 * 1024 : 0) < 0)
				goto err;
		}
	}

	if (apid > 0) {
		if (args.silent < 2)
			fprintf(stderr, _("audio pid %d\n"), apid);
		audio_fd = dvb_dev_open(dvb, args.demux_dev, O_RDWR);
		if (!audio_fd) {
			ERROR("failed opening '%s'", args.demux_dev);
			goto err;
		}
		if (args.silent < 2)
			fprintf(stderr, _("  dvb_set_pesfilter %d\n"), apid);
		if (dvb_dev_dmx_set_pesfilter(audio_fd, apid, DMX_PES_AUDIO,
				args.dvr ? DMX_OUT_TS_TAP : DMX_OUT_DECODER,
				args.dvr ? 64 * 1024 : 0) < 0)
			goto err;
	}

	set_signals(&args);

	if (!check_frontend(&args, parms)) {
		err = 1;
		fprintf(stderr, _("frontend doesn't lock\n"));
		goto err;
	}

	if (args.dvr) {
		if (args.filename) {
			file_fd = STDOUT_FILENO;

			if (strcmp(args.filename, "-") != 0) {
				file_fd = open(args.filename,
					 O_LARGEFILE |
					 O_WRONLY | O_CREAT | O_TRUNC,
					 0644);
				if (file_fd < 0) {
					PERROR(_("open of '%s' failed"),
					       args.filename);
					return -1;
				}
			}
		}

		if (args.silent < 2)
			get_show_stats(stderr, &args, parms, 0);

		if (file_fd >= 0) {
			dvr_fd = dvb_dev_open(dvb, args.dvr_dev, O_RDONLY);
			if (!dvr_fd) {
				ERROR("failed opening '%s'", args.dvr_dev);
				goto err;
			}
			if (!timeout_flag)
				fprintf(stderr, _("Record to file '%s' started\n"), args.filename);
			copy_to_file(dvr_fd, file_fd, args.timeout, args.silent);
		} else if (args.server && args.port) {
			struct stat st;
			if (stat(args.dvr_pipe, &st) == -1) {
				if (mknod(args.dvr_pipe,
					S_IRUSR | S_IWUSR | S_IFIFO, 0) < 0) {
					PERROR("Can't create pipe %s",
					args.dvr_pipe);
					return -1;
				}
			} else {
				if (!S_ISFIFO(st.st_mode)) {
					ERROR("%s exists but is not a pipe",
					args.dvr_pipe);
					return -1;
				}
			}

			fprintf(stderr, _("DVR pipe interface '%s' will be opened\n"), args.dvr_pipe);

			dvr_fd = dvb_dev_open(dvb, args.dvr_dev, O_RDONLY);
			if (!dvr_fd) {
				ERROR("failed opening '%s'", args.dvr_dev);
				err = -1;
				goto err;
			}

			file_fd = open(args.dvr_pipe,
#ifdef O_LARGEFILE
					O_LARGEFILE |
#endif
					O_WRONLY,
					0644);
			if (file_fd < 0) {
				PERROR(_("open of '%s' failed"),
					args.filename);
				err = -1;
				goto err;
			}
			copy_to_file(dvr_fd, file_fd, args.timeout, args.silent);
		} else {
			if (!timeout_flag)
				fprintf(stderr, _("DVR interface '%s' can now be opened\n"), args.dvr_fname);

			get_show_stats(stderr, &args, parms, 1);
		}
		if (args.silent < 2)
			get_show_stats(stderr, &args, parms, 0);
	} else {
		/* Wait until timeout or being killed */
		while (1) {
			get_show_stats(stderr, &args, parms, 1);
			usleep(1000000);
		}
	}
	err = 0;

err:
	if (args.confname)
		free(args.confname);
	dvb_dev_free(dvb);

	return err;
}
