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
 *
 * Based on dvb-apps tzap utility, made by:
 *	Bernard Hatt 24/2/04
 */

/*
 * FIXME: It lacks DISEqC support and DVB-CA. Tested only with ISDB-T
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

#include <linux/dvb/dmx.h>
#include "dvb-file.h"

#include "util.h"

static char DEMUX_DEV[80];
static char DVR_DEV[80];
static int timeout_flag = 0;
static int silent = 0, timeout = 0;
static int exit_after_tuning;

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

static int parse(const char *fname, const char *channel,
		 struct dvb_v5_fe_parms *parms,
		 uint32_t *vpid, uint32_t *apid, uint32_t *sid)
{
	struct dvb_file *dvb_file;
	struct dvb_entry *entry;
	int i;
	uint32_t sys;

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
		ERROR("Doesn't know how to emulate the delivery system");
		return -1;
	}

	dvb_file = parse_format_oneline(fname, ":", sys, zap_formats);
	if (!dvb_file)
		return -2;

	for (entry = dvb_file->first_entry; entry != NULL; entry = entry->next) {
		if (!strcmp(entry->channel, channel))
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

	*vpid = entry->video_pid;
	*apid = entry->audio_pid;
	*sid = entry->service_pid;

	/* Copy data into parms */
	for (i = 0; i < entry->n_props; i++) {
		uint32_t data = entry->props[i].u.data;
		/* Don't change the delivery system */
		if (entry->props[i].cmd == DTV_DELIVERY_SYSTEM)
			continue;
		dvb_fe_store_parm(parms, entry->props[i].cmd, data);
		if (parms->current_sys == SYS_ISDBT) {
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

static int setup_frontend(struct dvb_v5_fe_parms *parms)
{
	int rc;
	uint32_t freq;

	if (silent < 2) {
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

	if (status & FE_HAS_LOCK)
		fprintf(stderr, "FE_HAS_LOCK");

	fprintf(stderr, "\n");
	return 0;
}

static int check_frontend(struct dvb_v5_fe_parms *parms, int human_readable)
{
	int rc;
	fe_status_t status;
	do {
		rc = dvb_fe_get_stats(parms);
		if (rc < 0)
			PERROR("dvb_fe_get_stats failed");

		rc = dvb_fe_retrieve_stats(parms, DTV_STATUS, &status);
		if (!silent)
			print_frontend_stats(parms, human_readable);
		if (exit_after_tuning && (status & FE_HAS_LOCK))
			break;
		usleep(1000000);
	} while (!timeout_flag);
	if (silent < 2)
		print_frontend_stats(parms, human_readable);

	return 0;
}

#define BUFLEN (188 * 256)
static void copy_to_file(int in_fd, int out_fd)
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

static char *usage =
    "usage:\n"
    "       dvbzap [options] <channel_name>\n"
    "         zap to channel channel_name (case insensitive)\n"
    "     -a number : use given adapter (default 0)\n"
    "     -f number : use given frontend (default 0)\n"
    "     -d number : use given demux (default 0)\n"
    "     -c file   : read channels list from 'file'\n"
    "     -x        : exit after tuning\n"
    "     -r        : set up /dev/dvb/adapterX/dvr0 for TS recording\n"
    "     -p        : add pat and pmt to TS recording (implies -r)\n"
    "     -s        : only print summary\n"
    "     -S        : run silently (no output)\n"
    "     -H        : human readable output\n"
    "     -F        : set up frontend only, don't touch demux\n"
    "     -t number : timeout (seconds)\n"
    "     -o file   : output filename (use -o - for stdout)\n"
    "     -h -?     : display this help and exit\n";

int main(int argc, char **argv)
{
	char *homedir = getenv("HOME");
	char *confname = NULL;
	char *channel = NULL;
	int adapter = 0, frontend = 0, demux = 0, dvr = 0;
	uint32_t vpid, apid, sid;
	int pmtpid = 0;
	int pat_fd = -1, pmt_fd = -1;
	int audio_fd = 0, video_fd = 0;
	int dvr_fd, file_fd;
	int opt;
	int record = 0;
	int frontend_only = 0;
	char *filename = NULL;
	int human_readable = 0, rec_psi = 0;
	struct dvb_v5_fe_parms *parms;

	while ((opt = getopt(argc, argv, "H?hrpxRsFSn:a:f:d:c:t:o:")) != -1) {
		switch (opt) {
		case 'a':
			adapter = strtoul(optarg, NULL, 0);
			break;
		case 'f':
			frontend = strtoul(optarg, NULL, 0);
			break;
		case 'd':
			demux = strtoul(optarg, NULL, 0);
			break;
		case 't':
			timeout = strtoul(optarg, NULL, 0);
			break;
		case 'o':
			filename = strdup(optarg);
			record = 1;
			/* fall through */
		case 'r':
			dvr = 1;
			break;
		case 'p':
			rec_psi = 1;
			break;
		case 'x':
			exit_after_tuning = 1;
			break;
		case 'c':
			confname = optarg;
			break;
		case 's':
			silent = 1;
			break;
		case 'S':
			silent = 2;
			break;
		case 'F':
			frontend_only = 1;
			break;
		case 'H':
			human_readable = 1;
			break;
		case '?':
		case 'h':
		default:
			fprintf(stderr, usage, argv[0]);
			return -1;
		};
	}

	if (optind < argc)
		channel = argv[optind];

	if (!channel) {
		fprintf(stderr, usage, argv[0]);
		return -1;
	}

	snprintf(DEMUX_DEV, sizeof(DEMUX_DEV),
		 "/dev/dvb/adapter%i/demux%i", adapter, demux);

	snprintf(DVR_DEV, sizeof(DVR_DEV),
		 "/dev/dvb/adapter%i/dvr%i", adapter, demux);

	if (silent < 2)
		fprintf(stderr, "using demug '%s'\n", DEMUX_DEV);

	if (!confname) {
		int len = strlen(homedir) + strlen(CHANNEL_FILE) + 18;
		if (!homedir)
			ERROR("$HOME not set");
		confname = malloc(len);
		snprintf(confname, len, "%s/.tzap/%i/%s",
			 homedir, adapter, CHANNEL_FILE);
		if (access(confname, R_OK))
			snprintf(confname, len, "%s/.tzap/%s",
				 homedir, CHANNEL_FILE);
	}
	printf("reading channels from file '%s'\n", confname);

	parms = dvb_fe_open(adapter, frontend, !silent, 0);

	if (parse(confname, channel, parms, &vpid, &apid, &sid))
		return -1;

	if (setup_frontend(parms) < 0)
		return -1;

	if (frontend_only) {
		check_frontend(parms, human_readable);
		dvb_fe_close(parms);
		return 0;
	}

	if (rec_psi) {
		pmtpid = get_pmt_pid(DEMUX_DEV, sid);
		if (pmtpid <= 0) {
			fprintf(stderr, "couldn't find pmt-pid for sid %04x\n",
				sid);
			return -1;
		}

		if ((pat_fd = open(DEMUX_DEV, O_RDWR)) < 0) {
			perror("opening pat demux failed");
			return -1;
		}
		if (set_pesfilter(pat_fd, 0, DMX_PES_OTHER, dvr) < 0)
			return -1;

		if ((pmt_fd = open(DEMUX_DEV, O_RDWR)) < 0) {
			perror("opening pmt demux failed");
			return -1;
		}
		if (set_pesfilter(pmt_fd, pmtpid, DMX_PES_OTHER, dvr) < 0)
			return -1;
	}

	if ((video_fd = open(DEMUX_DEV, O_RDWR)) < 0) {
		PERROR("failed opening '%s'", DEMUX_DEV);
		return -1;
	}

	if (silent < 2)
		fprintf(stderr, "video pid 0x%04x, audio pid 0x%04x\n", vpid,
			apid);

	if (set_pesfilter(video_fd, vpid, DMX_PES_VIDEO, dvr) < 0)
		return -1;

	if ((audio_fd = open(DEMUX_DEV, O_RDWR)) < 0) {
		PERROR("failed opening '%s'", DEMUX_DEV);
		return -1;
	}

	if (set_pesfilter(audio_fd, apid, DMX_PES_AUDIO, dvr) < 0)
		return -1;

	signal(SIGALRM, do_timeout);
	if (timeout > 0)
		alarm(timeout);

	if (record) {
		if (filename != NULL) {
			if (strcmp(filename, "-") != 0) {
				file_fd =
				    open(filename,
					 O_WRONLY | O_LARGEFILE | O_CREAT,
					 0644);
				if (file_fd < 0) {
					PERROR("open of '%s' failed", filename);
					return -1;
				}
			} else {
				file_fd = 1;
			}
		} else {
			PERROR("Record mode but no filename!");
			return -1;
		}

		if ((dvr_fd = open(DVR_DEV, O_RDONLY)) < 0) {
			PERROR("failed opening '%s'", DVR_DEV);
			return -1;
		}
		if (silent < 2)
			print_frontend_stats(parms, human_readable);

		copy_to_file(dvr_fd, file_fd);

		if (silent < 2)
			print_frontend_stats(parms, human_readable);
	} else {
		check_frontend(parms, human_readable);
	}

	close(pat_fd);
	close(pmt_fd);
	close(audio_fd);
	close(video_fd);
	dvb_fe_close(parms);

	return 0;
}
