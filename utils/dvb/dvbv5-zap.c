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

#include <linux/dvb/dmx.h>
#include "dvb-file.h"
#include "dvb-demux.h"
#include "libscan.h"

#define CHANNEL_FILE	"channels.conf"
#define PROGRAM_NAME	"dvbv5-zap"

const char *argp_program_version = PROGRAM_NAME " version " V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <mchehab@redhat.com>";

struct arguments {
	char *confname, *lnb_name, *output, *demux_dev, *dvr_dev;
	char *filename;
	unsigned adapter, frontend, demux, get_detected, get_nit;
	int lnb, sat_number;
	unsigned diseqc_wait, silent, frontend_only, freq_bpf;
	unsigned timeout, dvr, rec_psi, exit_after_tuning;
	unsigned human_readable, record;
	unsigned n_apid, n_vpid;
	enum file_formats input_format, output_format;
};

static const struct argp_option options[] = {
	{"adapter",	'a', "adapter#",		0, "use given adapter (default 0)", 0},
	{"frontend",	'f', "frontend#",		0, "use given frontend (default 0)", 0},
	{"demux",	'd', "demux#",			0, "use given demux (default 0)", 0},
	{"audio_pid",	'A', "audio_pid#",		0, "audio pid program to use (default 0)", 0},
	{"video_pid",	'V', "video_pid#",		0, "video pid program to use (default 0)", 0},
	{"lnbf",	'l', "LNBf_type",		0, "type of LNBf to use. 'help' lists the available ones", 0},
	{"sat_number",	'S', "satellite_number",	0, "satellite number. If not specified, disable DISEqC", 0},
	{"freq_bpf",	'U', "frequency",		0, "SCR/Unicable band-pass filter frequency to use, in kHz", 0},
	{"wait",	'W', "time",			0, "adds additional wait time for DISEqC command completion", 0},
	{"channels",	'c', "file",			0, "read channels list from 'file'", 0},
	{"exit",	'x', NULL,			0, "exit after tuning", 0},
	{"record",	'r', NULL,			0, "set up /dev/dvb/adapterX/dvr0 for TS recording", 0},
	{"pat",		'p', NULL,			0, "add pat and pmt to TS recording (implies -r)", 0},
	{"silence",	's', NULL,			0, "increases silence (can be used more than once)", 0},
	{"human",	'H', NULL,			0, "human readable output", 0},
	{"frontend",	'F', NULL,			0, "set up frontend only, don't touch demux", 0},
	{"timeout",	't', "seconds",			0, "timeout for zapping and for recording", 0},
	{"output",	'o', "file",			0, "output filename (use -o - for stdout)", 0},
	{"input-format", 'I',	"format",		0, "Input format: ZAP, CHANNEL, DVBV5 (default: DVBV5)", 0},
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
	dvb_file = read_file_format(args->confname, sys,
				    args->input_format);
	if (!dvb_file)
		return -2;

	for (entry = dvb_file->first_entry; entry != NULL; entry = entry->next) {
		if (!strcmp(entry->channel, channel))
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
			if (!strcasecmp(entry->channel, channel))
				break;
		}
	}
	if (!entry) {
		ERROR("Can't find channel");
		return -3;
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
					printf("\n");
				printf("service has pid type %02x: ", type);
			}
			printf(" %d", entry->other_el_pid[i].pid);
		}
		printf("\n");
	}
	*sid = entry->service_id;

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

#if 0
	/* HACK to test the write file function */
	write_dvb_file("dvb_channels.conf", dvb_file);
#endif

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

static int old_status = 0;

static int print_frontend_stats(struct dvb_v5_fe_parms *parms,
				int human_readable)
{
	int rc;
	fe_status_t status;
	uint32_t snr = 0, _signal = 0;
	uint32_t ber = 0, uncorrected_blocks = 0;

	rc = dvb_fe_get_stats(parms);
	if (rc < 0) {
		PERROR("dvb_fe_get_stats failed");
		return -1;
	}

	rc = dvb_fe_retrieve_stats(parms, DTV_STATUS, &status);
	rc += dvb_fe_retrieve_stats(parms, DTV_BER, &ber);
	rc += dvb_fe_retrieve_stats(parms, DTV_SIGNAL_STRENGTH, &_signal);
	rc += dvb_fe_retrieve_stats(parms, DTV_UNCORRECTED_BLOCKS,
				    &uncorrected_blocks);
	rc += dvb_fe_retrieve_stats(parms, DTV_SNR, &snr);

	if (human_readable) {
		printf("status %02x | signal %3u%% | snr %3u%% | ber %d | unc %d | ",
		     status, (_signal * 100) / 0xffff, (snr * 100) / 0xffff,
		     ber, uncorrected_blocks);
	} else {
		fprintf(stderr,
			"status %02x | signal %04x | snr %04x | ber %08x | unc %08x | ",
			status, _signal, snr, ber, uncorrected_blocks);
	}

	if (status & FE_HAS_LOCK) {
		fprintf(stderr, "FE_HAS_LOCK");
		if (!(old_status & FE_HAS_LOCK)) {
			fprintf(stderr, "\n");
	                dvb_fe_get_parms(parms);
		}
	}
	old_status = status;

	fprintf(stderr, "\n");
	return 0;
}

static int check_frontend(struct arguments *args,
			  struct dvb_v5_fe_parms *parms)
{
	int rc;
	fe_status_t status;
	do {
		rc = dvb_fe_get_stats(parms);
		if (rc < 0)
			PERROR("dvb_fe_get_stats failed");

		rc = dvb_fe_retrieve_stats(parms, DTV_STATUS, &status);
		if (!args->silent)
			print_frontend_stats(parms, args->human_readable);
		if (args->exit_after_tuning && (status & FE_HAS_LOCK))
			break;
		usleep(1000000);
	} while (!timeout_flag);
	if (args->silent < 2)
		print_frontend_stats(parms, args->human_readable);

	return 0;
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
				printf("buffer overrun\n");
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
		args->record = 1;
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
	case 'F':
		args->frontend_only = 1;
		break;
	case 'H':
		args->human_readable = 1;
		break;
	case 'A':
		args->n_apid = strtoul(optarg, NULL, 0);
		break;
	case 'V':
		args->n_vpid = strtoul(optarg, NULL, 0);
	default:
		return ARGP_ERR_UNKNOWN;
	};
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
	int audio_fd = 0, video_fd = 0;
	int dvr_fd, file_fd;
	struct dvb_v5_fe_parms *parms;
	const struct argp argp = {
		.options = options,
		.parser = parse_opt,
		.doc = "DVB zap utility",
		.args_doc = "<initial file>",
	};

	memset(&args, 0, sizeof(args));
	args.sat_number = -1;

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

	if (args.lnb_name) {
		lnb = search_lnb(args.lnb_name);
		if (lnb < 0) {
			printf("Please select one of the LNBf's below:\n");
			print_all_lnb();
			exit(1);
		} else {
			printf("Using LNBf ");
			print_lnb(lnb);
		}
	}

	asprintf(&args.demux_dev,
		 "/dev/dvb/adapter%i/demux%i", args.adapter, args.demux);

	asprintf(&args.dvr_dev,
		 "/dev/dvb/adapter%i/dvr%i", args.adapter, args.demux);

	if (args.silent < 2)
		fprintf(stderr, "using demux '%s'\n", args.demux_dev);

	if (!args.confname) {
		if (!homedir)
			ERROR("$HOME not set");
		asprintf(&args.confname, "%s/.tzap/%i/%s",
			 homedir, args.adapter, CHANNEL_FILE);
		if (access(args.confname, R_OK))
			asprintf(&args.confname, "%s/.tzap/%s",
				homedir, CHANNEL_FILE);
	}
	printf("reading channels from file '%s'\n", args.confname);

	parms = dvb_fe_open(args.adapter, args.frontend, 0, 0);
	if (!parms)
		return -1;
	if (lnb)
		parms->lnb = get_lnb(lnb);
	if (args.sat_number > 0)
		parms->sat_number = args.sat_number % 3;
	parms->diseqc_wait = args.diseqc_wait;
	parms->freq_bpf = args.freq_bpf;

	if (parse(&args, parms, channel, &vpid, &apid, &sid))
		return -1;

	if (setup_frontend(&args, parms) < 0)
		return -1;

	if (args.frontend_only) {
		check_frontend(&args, parms);
		dvb_fe_close(parms);
		return 0;
	}

	if (args.rec_psi) {
		if (sid < 0) {
			fprintf(stderr, "Service id 0x%04x was not specified at the file\n",
				sid);
			return -1;
		}
		pmtpid = get_pmt_pid(args.demux_dev, sid);
		if (pmtpid <= 0) {
			fprintf(stderr, "couldn't find pmt-pid for sid %04x\n",
				sid);
			return -1;
		}

		if ((pat_fd = open(args.demux_dev, O_RDWR)) < 0) {
			perror("opening pat demux failed");
			return -1;
		}
		if (set_pesfilter(pat_fd, 0, DMX_PES_OTHER, args.dvr) < 0)
			return -1;

		if ((pmt_fd = open(args.demux_dev, O_RDWR)) < 0) {
			perror("opening pmt demux failed");
			return -1;
		}
		if (set_pesfilter(pmt_fd, pmtpid, DMX_PES_OTHER, args.dvr) < 0)
			return -1;
	}

	if (vpid >= 0) {
		if (args.silent < 2)
			fprintf(stderr, "video pid %d\n", vpid);
		if ((video_fd = open(args.demux_dev, O_RDWR)) < 0) {
			PERROR("failed opening '%s'", args.demux_dev);
			return -1;
		}
		if (set_pesfilter(video_fd, vpid, DMX_PES_VIDEO, args.dvr) < 0)
			return -1;
	}

	if (apid >= 0) {
		if (args.silent < 2)
			fprintf(stderr, "audio pid %d\n", apid);
		if ((audio_fd = open(args.demux_dev, O_RDWR)) < 0) {
			PERROR("failed opening '%s'", args.demux_dev);
			return -1;
		}

		if (set_pesfilter(audio_fd, apid, DMX_PES_AUDIO, args.dvr) < 0)
			return -1;
	}

	signal(SIGALRM, do_timeout);
	if (args.timeout > 0)
		alarm(args.timeout);

	if (args.record) {
		if (args.filename != NULL) {
			if (strcmp(args.filename, "-") != 0) {
				file_fd =
				    open(args.filename,
					 O_WRONLY | O_LARGEFILE | O_CREAT,
					 0644);
				if (file_fd < 0) {
					PERROR("open of '%s' failed",
					       args.filename);
					return -1;
				}
			} else {
				file_fd = 1;
			}
		} else {
			PERROR("Record mode but no filename!");
			return -1;
		}

		if ((dvr_fd = open(args.dvr_dev, O_RDONLY)) < 0) {
			PERROR("failed opening '%s'", args.dvr_dev);
			return -1;
		}
		if (args.silent < 2)
			print_frontend_stats(parms, args.human_readable);

		copy_to_file(dvr_fd, file_fd, args.timeout, args.silent);

		if (args.silent < 2)
			print_frontend_stats(parms, args.human_readable);
	} else {
		check_frontend(&args, parms);
	}

	close(pat_fd);
	close(pmt_fd);
	close(audio_fd);
	close(video_fd);
	dvb_fe_close(parms);

	return 0;
}
