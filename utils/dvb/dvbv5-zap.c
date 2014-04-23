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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <argp.h>
#include <sys/time.h>

#include <config.h>

#include <linux/dvb/dmx.h>
#include "libdvbv5/dvb-file.h"
#include "libdvbv5/dvb-demux.h"
#include "libdvbv5/dvb-scan.h"
#include "libdvbv5/header.h"

#define CHANNEL_FILE	"channels.conf"
#define PROGRAM_NAME	"dvbv5-zap"

const char *argp_program_version = PROGRAM_NAME " version " V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <m.chehab@samsung.com>";

struct arguments {
	char *confname, *lnb_name, *output, *demux_dev, *dvr_dev;
	char *filename;
	unsigned adapter, frontend, demux, get_detected, get_nit;
	int force_dvbv3, lna, lnb, sat_number;
	unsigned diseqc_wait, silent, verbose, frontend_only, freq_bpf;
	unsigned timeout, dvr, rec_psi, exit_after_tuning;
	unsigned n_apid, n_vpid, all_pids;
	enum file_formats input_format, output_format;
	unsigned traffic_monitor, low_traffic;
	char *search;

	/* Used by status print */
	unsigned n_status_lines;
};

static const struct argp_option options[] = {
	{"dvbv3",	'3', NULL,			0, "Use DVBv3 only", 0},
	{"adapter",	'a', "adapter#",		0, "use given adapter (default 0)", 0},
	{"audio_pid",	'A', "audio_pid#",		0, "audio pid program to use (default 0)", 0},
	{"channels",	'c', "file",			0, "read channels list from 'file'", 0},
	{"demux",	'd', "demux#",			0, "use given demux (default 0)", 0},
	{"frontend",	'f', "frontend#",		0, "use given frontend (default 0)", 0},
	{"input-format", 'I',	"format",		0, "Input format: ZAP, CHANNEL, DVBV5 (default: DVBV5)", 0},
	{"lna",		'w', "LNA (0, 1, -1)",		0, "enable/disable/auto LNA power", 0},
	{"lnbf",	'l', "LNBf_type",		0, "type of LNBf to use. 'help' lists the available ones", 0},
	{"search",	'L', "string",			0, "search/look for a string inside the traffic", 0},
	{"monitor",	'm', NULL,			0, "monitors de DVB traffic", 0},
	{"output",	'o', "file",			0, "output filename (use -o - for stdout)", 0},
	{"pat",		'p', NULL,			0, "add pat and pmt to TS recording (implies -r)", 0},
	{"all-pids",	'P', NULL,			0, "don't filter any pids. Instead, outputs all of them", 0 },
	{"record",	'r', NULL,			0, "set up /dev/dvb/adapterX/dvr0 for TS recording", 0},
	{"silence",	's', NULL,			0, "increases silence (can be used more than once)", 0},
	{"sat_number",	'S', "satellite_number",	0, "satellite number. If not specified, disable DISEqC", 0},
	{"timeout",	't', "seconds",			0, "timeout for zapping and for recording", 0},
	{"freq_bpf",	'U', "frequency",		0, "SCR/Unicable band-pass filter frequency to use, in kHz", 0},
	{"verbose",	'v', NULL,			0, "verbose debug messages (can be used more than once)", 0},
	{"video_pid",	'V', "video_pid#",		0, "video pid program to use (default 0)", 0},
	{"wait",	'W', "time",			0, "adds additional wait time for DISEqC command completion", 0},
	{"exit",	'x', NULL,			0, "exit after tuning", 0},
	{"low_traffic",	'X', NULL,			0, "also shows DVB traffic with less then 1 packet per second", 0},
	{ 0, 0, 0, 0, 0, 0 }
};

static int timeout_flag = 0;

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
	if (!entry && (args->traffic_monitor || args->all_pids || args->exit_after_tuning)) {
		uint32_t f, freq = atoi(channel);
		if (freq) {
			for (entry = dvb_file->first_entry; entry != NULL;
			entry = entry->next) {
				retrieve_entry_prop(entry, DTV_FREQUENCY, &f);
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

	if (entry->lnb) {
		int lnb = dvb_sat_search_lnb(entry->lnb);
		if (lnb == -1) {
			PERROR("unknown LNB %s\n", entry->lnb);
			dvb_file_free(dvb_file);
			return -1;
		}
		parms->lnb = dvb_sat_get_lnb(lnb);
	}

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
				fprintf(stderr, "service has pid type %02x: ", type);
			}
			fprintf(stderr, " %d", entry->other_el_pid[i].pid);
		}
		fprintf(stderr, "\n");
	}
	*sid = entry->service_id;

        /* First of all, set the delivery system */
	retrieve_entry_prop(entry, DTV_DELIVERY_SYSTEM, &sys);
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
			PERROR("can't get the frequency");
			return -1;
		}
		fprintf(stderr, "tuning to %i Hz\n", freq);
	}

	rc = dvb_fe_set_parms(parms);
	if (rc < 0) {
		PERROR("dvb_fe_set_parms failed");
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

static int print_frontend_stats(FILE *fd,
				struct arguments *args,
				struct dvb_v5_fe_parms *parms)
{
	char buf[512], *p;
	int rc, i, len, show;
	uint32_t status = 0;

	/* Move cursor up and cleans down */
	if (isatty(fileno(fd)) && args->n_status_lines)
		fprintf(fd, "\r\x1b[%dA\x1b[J", args->n_status_lines);

	args->n_status_lines = 0;

	rc = dvb_fe_get_stats(parms);
	if (rc) {
		PERROR("dvb_fe_get_stats failed");
		return -1;
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
			PERROR("dvb_fe_get_stats failed");
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

#define BUFLEN (188 * 256)
static void copy_to_file(int in_fd, int out_fd, int timeout, int silent)
{
	char buf[BUFLEN];
	int r;
	long long int rc = 0LL;
	while (timeout_flag == 0) {
		r = read(in_fd, buf, BUFLEN);
		if (r < 0) {
			if (errno == EOVERFLOW) {
				fprintf(stderr, "buffer overrun\n");
				continue;
			}
			PERROR("Read failed");
			break;
		}
		if (write(out_fd, buf, r) < 0) {
			PERROR("Write failed");
			break;
		}
		rc += r;
	}
	if (silent < 2) {
		fprintf(stderr, "copied %lld bytes (%lld Kbytes/sec)\n", rc,
			rc / (1024 * timeout));
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
		args->input_format = parse_format(optarg);
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
	case '3':
		args->force_dvbv3 = 1;
		break;
	case 'm':
		args->traffic_monitor = 1;
		break;
	case 'X':
		args->low_traffic = 1;
		break;
	case 'L':
		args->search = strdup(optarg);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	};
	return 0;
}

#define BSIZE 188

int do_traffic_monitor(struct arguments *args,
		    struct dvb_v5_fe_parms *parms)
{
	int fd, dvr_fd;
	long long unsigned pidt[0x2001], wait;
	int packets = 0;
	struct timeval startt;

	memset(pidt, 0, sizeof(pidt));

	args->exit_after_tuning = 1;
	check_frontend(args, parms);

	if ((dvr_fd = open(args->dvr_dev, O_RDONLY)) < 0) {
		PERROR("failed opening '%s'", args->dvr_dev);
		return -1;
	}

	if (ioctl(dvr_fd, DMX_SET_BUFFER_SIZE, 1024 * 1024) == -1)
		perror("DMX_SET_BUFFER_SIZE failed");

	if ((fd = open(args->demux_dev, O_RDWR)) < 0) {
		PERROR("failed opening '%s'", args->demux_dev);
		close(dvr_fd);
		return -1;
	}

	if (args->silent < 2)
		fprintf(stderr, "  dvb_set_pesfilter to 0x2000\n");
	if (dvb_set_pesfilter(fd, 0x2000, DMX_PES_OTHER,
			      DMX_OUT_TS_TAP, 0) < 0) {
		PERROR("couldn't set filter");
		return -1;
	}

	gettimeofday(&startt, 0);
	wait = 1000;

	while (1) {
		unsigned char buffer[BSIZE];
		struct dvb_ts_packet_header *h = (void *)buffer;
		int pid, ok;
		ssize_t r;

		if (timeout_flag)
			break;

		if ((r = read(dvr_fd, buffer, BSIZE)) <= 0) {
			if (errno == EOVERFLOW) {
				struct timeval now;
				int diff;
				gettimeofday(&now, 0);
				diff =
				    (now.tv_sec - startt.tv_sec) * 1000 +
				    (now.tv_usec - startt.tv_usec) / 1000;
				fprintf(stderr, "%.2fs: buffer overrun\n", diff / 1000.);
				continue;
			}
			perror("read");
			break;
		}
		if (r != BSIZE) {
			fprintf(stderr, "dvbtraffic: only read %zd bytes\n", r);
			break;
		}

		if (h->sync_byte != 0x47) {
			continue;
		}

		bswap16(h->bitfield);

#if 0
		/*
		 * ITU-T Rec. H.222.0 decoders shall discard Transport Stream
		 * packets with theadaptation_field_control field set to
		 * a value of '00'.
		 */
		if (h->adaptation_field_control == 0)
			continue;
#endif
		ok = 1;
		pid = h->pid;

		if (args->search) {
			int i, sl = strlen(args->search);
			ok = 0;
			if (pid != 0x1fff) {
				for (i = 0; i < (188 - sl); ++i) {
					if (!memcmp(buffer + i, args->search, sl))
						ok = 1;
				}
			}
		}

		if (ok) {
			pidt[pid]++;
			pidt[0x2000]++;
		}

		packets++;

		if (!(packets & 0xFF)) {
			struct timeval now;
			int diff;
			gettimeofday(&now, 0);
			diff =
			    (now.tv_sec - startt.tv_sec) * 1000 +
			    (now.tv_usec - startt.tv_usec) / 1000;
			if (diff > wait) {
				if (isatty(STDOUT_FILENO))
			                printf("\x1b[1H\x1b[2J");

				args->n_status_lines = 0;
				printf(" PID          FREQ         SPEED       TOTAL\n");
				int _pid = 0;
				for (_pid = 0; _pid < 0x2000; _pid++) {
					if (pidt[_pid]) {
						if (!args->low_traffic && (pidt[_pid] * 1000. / diff) < 1)
							continue;
						printf("%04x %9.2f p/s %8.1f Kbps ",
						     _pid,
						     pidt[_pid] * 1000. / diff,
						     pidt[_pid] * 1000. / diff * 8 * 188 / 1024);
						if (pidt[_pid] * 188 / 1024)
							printf("%8llu KB\n", pidt[_pid] * 188 / 1024);
						else
							printf(" %8llu B\n", pidt[_pid] * 188);
					}
				}
				/* 0x2000 is the total traffic */
				printf("TOT %10.2f p/s %8.1f Kbps %8llu KB\n",
				     pidt[_pid] * 1000. / diff,
				     pidt[_pid] * 1000. / diff * 8 * 188 / 1024,
				     pidt[_pid] * 188 / 1024);
				printf("\n\n");
				print_frontend_stats(stdout, args, parms);
				wait += 1000;
			}
		}
	}
	close (fd);
	return 0;
}

int main(int argc, char **argv)
{
	struct arguments args;
	char *homedir = getenv("HOME");
	char *channel = NULL;
	int lnb = -1, idx = -1;
	int vpid = -1, apid = -1, sid = -1;
	int pmtpid = 0;
	int pat_fd = -1, pmt_fd = -1;
	int audio_fd = -1, video_fd = -1;
	int dvr_fd = -1, file_fd = -1;
	int err = -1;
	int r;
	struct dvb_v5_fe_parms *parms = NULL;
	const struct argp argp = {
		.options = options,
		.parser = parse_opt,
		.doc = "DVB zap utility",
		.args_doc = "<channel name> [or <frequency> if in monitor mode]",
	};

	memset(&args, 0, sizeof(args));
	args.sat_number = -1;
	args.lna = LNA_AUTO;
	args.input_format = FILE_DVBV5;

	argp_parse(&argp, argc, argv, 0, &idx, &args);

	if (idx < argc)
		channel = argv[idx];

	if (!channel) {
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, PROGRAM_NAME);
		return -1;
	}

	if (args.input_format == FILE_UNKNOWN) {
		fprintf(stderr, "ERROR: Please specify a valid format\n");
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, PROGRAM_NAME);
		return -1;
	}

	if (!args.traffic_monitor && args.search) {
		fprintf(stderr, "ERROR: search string can be used only on monitor mode\n");
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, PROGRAM_NAME);
		return -1;
	}

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

	r = asprintf(&args.demux_dev,
		 "/dev/dvb/adapter%i/demux%i", args.adapter, args.demux);
	if (r < 0) {
		fprintf(stderr, "asprintf error\n");
		return -1;
	}

	r = asprintf(&args.dvr_dev,
		 "/dev/dvb/adapter%i/dvr%i", args.adapter, args.demux);
	if (r < 0) {
		fprintf(stderr, "asprintf error\n");
		return -1;
	}

	if (args.silent < 2)
		fprintf(stderr, "using demux '%s'\n", args.demux_dev);

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
	fprintf(stderr, "reading channels from file '%s'\n", args.confname);

	parms = dvb_fe_open(args.adapter, args.frontend, args.verbose, args.force_dvbv3);
	if (!parms)
		goto err;
	if (lnb)
		parms->lnb = dvb_sat_get_lnb(lnb);
	if (args.sat_number > 0)
		parms->sat_number = args.sat_number % 3;
	parms->diseqc_wait = args.diseqc_wait;
	parms->freq_bpf = args.freq_bpf;
	parms->lna = args.lna;

	if (parse(&args, parms, channel, &vpid, &apid, &sid))
		goto err;

	if (setup_frontend(&args, parms) < 0)
		goto err;

	if (args.exit_after_tuning) {
		err = 0;
		check_frontend(&args, parms);
		goto err;
	}

	if (args.traffic_monitor) {
		signal(SIGTERM, do_timeout);
		signal(SIGINT, do_timeout);
		if (args.timeout > 0) {
			signal(SIGINT, do_timeout);
			alarm(args.timeout);
		}

		err = do_traffic_monitor(&args, parms);
		goto err;
	}

	if (args.rec_psi) {
		if (sid < 0) {
			fprintf(stderr, "Service id 0x%04x was not specified at the file\n",
				sid);
			goto err;
		}
		pmtpid = get_pmt_pid(args.demux_dev, sid);
		if (pmtpid <= 0) {
			fprintf(stderr, "couldn't find pmt-pid for sid %04x\n",
				sid);
			goto err;
		}

		if ((pat_fd = open(args.demux_dev, O_RDWR)) < 0) {
			perror("opening pat demux failed");
			goto err;
		}
		if (dvb_set_pesfilter(pat_fd, 0, DMX_PES_OTHER,
				args.dvr ? DMX_OUT_TS_TAP : DMX_OUT_DECODER,
				args.dvr ? 64 * 1024 : 0) < 0)
			goto err;

		if ((pmt_fd = open(args.demux_dev, O_RDWR)) < 0) {
			perror("opening pmt demux failed");
			goto err;
		}
		if (dvb_set_pesfilter(pmt_fd, pmtpid, DMX_PES_OTHER,
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
				fprintf(stderr, "pass all PID's to TS\n");
			else
				fprintf(stderr, "video pid %d\n", vpid);
		}
		if ((video_fd = open(args.demux_dev, O_RDWR)) < 0) {
			PERROR("failed opening '%s'", args.demux_dev);
			goto err;
		}

		if (args.silent < 2)
			fprintf(stderr, "  dvb_set_pesfilter %d\n", vpid);
		if (vpid == 0x2000) {
			if (ioctl(video_fd, DMX_SET_BUFFER_SIZE, 1024 * 1024) == -1)
				perror("DMX_SET_BUFFER_SIZE failed");
			if (dvb_set_pesfilter(video_fd, vpid, DMX_PES_OTHER,
					      DMX_OUT_TS_TAP, 0) < 0)
				goto err;
		} else {
			if (dvb_set_pesfilter(video_fd, vpid, DMX_PES_VIDEO,
				args.dvr ? DMX_OUT_TS_TAP : DMX_OUT_DECODER,
				args.dvr ? 64 * 1024 : 0) < 0)
				goto err;
		}
	}

	if (apid > 0) {
		if (args.silent < 2)
			fprintf(stderr, "audio pid %d\n", apid);
		if ((audio_fd = open(args.demux_dev, O_RDWR)) < 0) {
			PERROR("failed opening '%s'", args.demux_dev);
			goto err;
		}
		if (args.silent < 2)
			fprintf(stderr, "  dvb_set_pesfilter %d\n", apid);
		if (dvb_set_pesfilter(audio_fd, apid, DMX_PES_AUDIO,
				args.dvr ? DMX_OUT_TS_TAP : DMX_OUT_DECODER,
				args.dvr ? 64 * 1024 : 0) < 0)
			goto err;
	}

	signal(SIGALRM, do_timeout);
	signal(SIGTERM, do_timeout);
	if (args.timeout > 0) {
		signal(SIGINT, do_timeout);
		alarm(args.timeout);
	}

	if (!check_frontend(&args, parms)) {
		err = 1;
		fprintf(stderr, "frontend doesn't lock\n");
		goto err;
	}

	if (args.dvr) {
		if (args.filename) {
			file_fd = STDOUT_FILENO;

			if (strcmp(args.filename, "-") != 0) {
				file_fd = open(args.filename,
#ifdef O_LARGEFILE
					 O_LARGEFILE |
#endif
					 O_WRONLY | O_CREAT,
					 0644);
				if (file_fd < 0) {
					PERROR("open of '%s' failed",
					       args.filename);
					return -1;
				}
			}
		}

		if (args.silent < 2)
			print_frontend_stats(stderr, &args, parms);

		if (file_fd >= 0) {
			if ((dvr_fd = open(args.dvr_dev, O_RDONLY)) < 0) {
				PERROR("failed opening '%s'", args.dvr_dev);
				goto err;
			}
			if (!timeout_flag)
				fprintf(stderr, "Record to file '%s' started\n", args.filename);
			copy_to_file(dvr_fd, file_fd, args.timeout, args.silent);
		} else {
			if (!timeout_flag)
				fprintf(stderr, "DVR interface '%s' can now be opened\n", args.dvr_dev);
			while (timeout_flag == 0)
				sleep(1);
		}
		if (args.silent < 2)
			print_frontend_stats(stderr, &args, parms);
	}
	err = 0;

err:
	if (file_fd > 0)
		close(file_fd);
	if (dvr_fd > 0)
		close(dvr_fd);
	if (pat_fd > 0)
		close(pat_fd);
	if (pmt_fd > 0)
		close(pmt_fd);
	if (audio_fd > 0)
		close(audio_fd);
	if (video_fd > 0)
		close(video_fd);
	if (parms)
		dvb_fe_close(parms);
	if (args.confname)
		free(args.confname);
	if (args.demux_dev)
		free(args.demux_dev);
	if (args.dvr_dev)
		free(args.dvr_dev);

	return err;
}
