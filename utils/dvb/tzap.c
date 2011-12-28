/* tzap -- DVB-T zapping utility
 */

/*
 * Added recording to a file
 * arguments:
 *
 * -t	timeout (seconds)
 * -o filename		output filename (use -o - for stdout)
 * -s	only print summary
 * -S	run silently (no output)
 *
 * Bernard Hatt 24/2/04
 */



#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1
#define _LARGEFILE64_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "util.h"

static char FRONTEND_DEV [80];
static char DEMUX_DEV [80];
static char DVR_DEV [80];
static int timeout_flag=0;
static int silent=0,timeout=0;
static int exit_after_tuning;

#define CHANNEL_FILE "channels.conf"

#define ERROR(x...)                                                     \
        do {                                                            \
                fprintf(stderr, "ERROR: ");                             \
                fprintf(stderr, x);                                     \
                fprintf (stderr, "\n");                                 \
        } while (0)

#define PERROR(x...)                                                    \
        do {                                                            \
                fprintf(stderr, "ERROR: ");                             \
                fprintf(stderr, x);                                     \
                fprintf (stderr, " (%s)\n", strerror(errno));		\
        } while (0)


typedef struct {
	char *name;
	int value;
} Param;

static const Param inversion_list [] = {
	{ "INVERSION_OFF", INVERSION_OFF },
	{ "INVERSION_ON", INVERSION_ON },
	{ "INVERSION_AUTO", INVERSION_AUTO }
};

static const Param bw_list [] = {
	{ "BANDWIDTH_6_MHZ", BANDWIDTH_6_MHZ },
	{ "BANDWIDTH_7_MHZ", BANDWIDTH_7_MHZ },
	{ "BANDWIDTH_8_MHZ", BANDWIDTH_8_MHZ }
};

static const Param fec_list [] = {
	{ "FEC_1_2", FEC_1_2 },
	{ "FEC_2_3", FEC_2_3 },
	{ "FEC_3_4", FEC_3_4 },
	{ "FEC_4_5", FEC_4_5 },
	{ "FEC_5_6", FEC_5_6 },
	{ "FEC_6_7", FEC_6_7 },
	{ "FEC_7_8", FEC_7_8 },
	{ "FEC_8_9", FEC_8_9 },
	{ "FEC_AUTO", FEC_AUTO },
	{ "FEC_NONE", FEC_NONE }
};

static const Param guard_list [] = {
	{"GUARD_INTERVAL_1_16", GUARD_INTERVAL_1_16},
	{"GUARD_INTERVAL_1_32", GUARD_INTERVAL_1_32},
	{"GUARD_INTERVAL_1_4", GUARD_INTERVAL_1_4},
	{"GUARD_INTERVAL_1_8", GUARD_INTERVAL_1_8},
	{"GUARD_INTERVAL_AUTO", GUARD_INTERVAL_AUTO}
};

static const Param hierarchy_list [] = {
	{ "HIERARCHY_1", HIERARCHY_1 },
	{ "HIERARCHY_2", HIERARCHY_2 },
	{ "HIERARCHY_4", HIERARCHY_4 },
	{ "HIERARCHY_NONE", HIERARCHY_NONE },
	{ "HIERARCHY_AUTO", HIERARCHY_AUTO }
};

static const Param constellation_list [] = {
	{ "QPSK", QPSK },
	{ "QAM_128", QAM_128 },
	{ "QAM_16", QAM_16 },
	{ "QAM_256", QAM_256 },
	{ "QAM_32", QAM_32 },
	{ "QAM_64", QAM_64 },
	{ "QAM_AUTO", QAM_AUTO }
};

static const Param transmissionmode_list [] = {
	{ "TRANSMISSION_MODE_2K", TRANSMISSION_MODE_2K },
	{ "TRANSMISSION_MODE_8K", TRANSMISSION_MODE_8K },
	{ "TRANSMISSION_MODE_AUTO", TRANSMISSION_MODE_AUTO }
};

#define LIST_SIZE(x) sizeof(x)/sizeof(Param)


static
int parse_param (int fd, const Param * plist, int list_size, int *param)
{
	char c;
	int character = 0;
	int _index = 0;

	while (1) {
		if (read(fd, &c, 1) < 1)
			return -1;	/*  EOF? */

		if ((c == ':' || c == '\n')
		    && plist->name[character] == '\0')
			break;

		while (toupper(c) != plist->name[character]) {
			_index++;
			plist++;
			if (_index >= list_size)	 /*  parse error, no valid */
				return -2;	 /*  parameter name found  */
		}

		character++;
	}

	*param = plist->value;

	return 0;
}


static
int parse_int(int fd, int *val)
{
	char number[11];	/* 2^32 needs 10 digits... */
	int character = 0;

	while (1) {
		if (read(fd, &number[character], 1) < 1)
			return -1;	/*  EOF? */

		if (number[character] == ':' || number[character] == '\n') {
			number[character] = '\0';
			break;
		}

		if (!isdigit(number[character]))
			return -2;	/*  parse error, not a digit... */

		character++;

		if (character > 10)	/*  overflow, number too big */
			return -3;	/*  to fit in 32 bit */
	};

	errno = 0;
	*val = strtol(number, NULL, 10);
	if (errno == ERANGE)
		return -4;

	return 0;
}


static
int find_channel(int fd, const char *channel)
{
	int character = 0;

	while (1) {
		char c;

		if (read(fd, &c, 1) < 1)
			return -1;	/*  EOF! */

		if ( '\n' == c ) /* start of line */
			character = 0;
		else if ( character >= 0 ) { /* we are in the namefield */

			if (c == ':' && channel[character] == '\0')
				break;

			if (toupper(c) == toupper(channel[character]))
				character++;
			else
				character = -1;
		}
	};

	return 0;
}


static
int try_parse_int(int fd, int *val, const char *pname)
{
	int err;

	err = parse_int(fd, val);

	if (err)
		ERROR("error while parsing %s (%s)", pname,
		      err == -1 ? "end of file" :
		      err == -2 ? "not a number" : "number too big");

	return err;
}


static
int try_parse_param(int fd, const Param * plist, int list_size, int *param,
		    const char *pname)
{
	int err;

	err = parse_param(fd, plist, list_size, param);

	if (err)
		ERROR("error while parsing %s (%s)", pname,
		      err == -1 ? "end of file" : "syntax error");

	return err;
}

static int check_fec(fe_code_rate_t *fec)
{
	switch (*fec)
	{
	case FEC_NONE:
		*fec = FEC_AUTO;
	case FEC_AUTO:
	case FEC_1_2:
	case FEC_2_3:
	case FEC_3_4:
	case FEC_5_6:
	case FEC_7_8:
		return 0;
	default:
		;
	}
	return 1;
}


int parse(const char *fname, const char *channel,
	  struct dvb_frontend_parameters *frontend, int *vpid, int *apid,
	  int *sid)
{
	int fd;
	int err;
	int tmp;

	if ((fd = open(fname, O_RDONLY | O_NONBLOCK)) < 0) {
		PERROR ("could not open file '%s'", fname);
		perror ("");
		return -1;
	}

	if (find_channel(fd, channel) < 0) {
		ERROR("could not find channel '%s' in channel list", channel);
		return -2;
	}

	if ((err = try_parse_int(fd, &tmp, "frequency")))
		return -3;
	frontend->frequency = tmp;

	if ((err = try_parse_param(fd,
				   inversion_list, LIST_SIZE(inversion_list),
				   &tmp, "inversion")))
		return -4;
	frontend->inversion = tmp;

	if ((err = try_parse_param(fd, bw_list, LIST_SIZE(bw_list),
				   &tmp, "bandwidth")))
		return -5;
	frontend->u.ofdm.bandwidth = tmp;

	if ((err = try_parse_param(fd, fec_list, LIST_SIZE(fec_list),
				   &tmp, "code_rate_HP")))
		return -6;
	frontend->u.ofdm.code_rate_HP = tmp;
	if (check_fec(&frontend->u.ofdm.code_rate_HP))
		return -6;

	if ((err = try_parse_param(fd, fec_list, LIST_SIZE(fec_list),
				   &tmp, "code_rate_LP")))
		return -7;
	frontend->u.ofdm.code_rate_LP = tmp;
	if (check_fec(&frontend->u.ofdm.code_rate_LP))
		return -7;

	if ((err = try_parse_param(fd, constellation_list,
				   LIST_SIZE(constellation_list),
				   &tmp, "constellation")))
		return -8;
	frontend->u.ofdm.constellation = tmp;

	if ((err = try_parse_param(fd, transmissionmode_list,
				   LIST_SIZE(transmissionmode_list),
				   &tmp, "transmission_mode")))
		return -9;
	frontend->u.ofdm.transmission_mode = tmp;

	if ((err = try_parse_param(fd, guard_list, LIST_SIZE(guard_list),
				   &tmp, "guard_interval")))
		return -10;
	frontend->u.ofdm.guard_interval = tmp;

	if ((err = try_parse_param(fd, hierarchy_list,
				   LIST_SIZE(hierarchy_list),
				   &tmp, "hierarchy_information")))
		return -11;
	frontend->u.ofdm.hierarchy_information = tmp;

	if ((err = try_parse_int(fd, vpid, "Video PID")))
		return -12;

	if ((err = try_parse_int(fd, apid, "Audio PID")))
		return -13;
	
	if ((err = try_parse_int(fd, sid, "Service ID")))
	    return -14;
	
	
	close(fd);

	return 0;
}


static
int setup_frontend (int fe_fd, struct dvb_frontend_parameters *frontend)
{
	struct dvb_frontend_info fe_info;

	if (ioctl(fe_fd, FE_GET_INFO, &fe_info) < 0) {
		PERROR("ioctl FE_GET_INFO failed");
		return -1;
	}

	if (fe_info.type != FE_OFDM) {
		ERROR ("frontend device is not a OFDM (DVB-T) device");
		return -1;
	}

	if (silent<2)
		fprintf (stderr,"tuning to %i Hz\n", frontend->frequency);

	if (ioctl(fe_fd, FE_SET_FRONTEND, frontend) < 0) {
		PERROR("ioctl FE_SET_FRONTEND failed");
		return -1;
	}

	return 0;
}

static void
do_timeout(int x)
{
	(void)x;
	if (timeout_flag==0)
	{
		timeout_flag=1;
		alarm(2);
		signal(SIGALRM, do_timeout);
	}
	else
	{
		/* something has gone wrong ... exit */
		exit(1);
	}
}

static void
print_frontend_stats (int fe_fd, int human_readable)
{
	fe_status_t status;
	uint16_t snr, _signal;
	uint32_t ber, uncorrected_blocks;

	ioctl(fe_fd, FE_READ_STATUS, &status);
	ioctl(fe_fd, FE_READ_SIGNAL_STRENGTH, &_signal);
	ioctl(fe_fd, FE_READ_SNR, &snr);
	ioctl(fe_fd, FE_READ_BER, &ber);
	ioctl(fe_fd, FE_READ_UNCORRECTED_BLOCKS, &uncorrected_blocks);

	if (human_readable) {
		printf ("status %02x | signal %3u%% | snr %3u%% | ber %d | unc %d | ",
			status, (_signal * 100) / 0xffff, (snr * 100) / 0xffff, ber, uncorrected_blocks);
	} else {
		fprintf (stderr, "status %02x | signal %04x | snr %04x | ber %08x | unc %08x | ",
			status, _signal, snr, ber, uncorrected_blocks);
	}

	if (status & FE_HAS_LOCK)
		fprintf(stderr,"FE_HAS_LOCK");

	fprintf(stderr,"\n");
}

static
int check_frontend (int fe_fd, int human_readable)
{
	fe_status_t status;
	do {
	        ioctl(fe_fd, FE_READ_STATUS, &status);
		if (!silent)
			print_frontend_stats(fe_fd, human_readable);
		if (exit_after_tuning && (status & FE_HAS_LOCK))
			break;
		usleep(1000000);
	} while (!timeout_flag);
	if (silent < 2)
		print_frontend_stats (fe_fd, human_readable);

	return 0;
}

#define BUFLEN (188*256)
static
void copy_to_file(int in_fd, int out_fd)
{
	char buf[BUFLEN];
	int r;
	long long int rc = 0LL;
	while(timeout_flag==0)
	{
		r=read(in_fd,buf,BUFLEN);
		if (r < 0) {
			if (errno == EOVERFLOW) {
				printf("buffer overrun\n");
				continue;
			}
			PERROR("Read failed");
			break;
		}
		if (write(out_fd,buf,r) < 0) {
			PERROR("Write failed");
			break;
		}
		rc+=r;
	}
	if (silent<2)
	{
		fprintf(stderr, "copied %lld bytes (%lld Kbytes/sec)\n",rc,rc/(1024*timeout));
	}
}

static char *usage =
    "usage:\n"
    "       tzap [options] <channel_name>\n"
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
	struct dvb_frontend_parameters frontend_param;
	char *homedir = getenv ("HOME");
	char *confname = NULL;
	char *channel = NULL;
	int adapter = 0, frontend = 0, demux = 0, dvr = 0;
	int vpid, apid, sid, pmtpid = 0;
	int pat_fd, pmt_fd;
	int frontend_fd, audio_fd = 0, video_fd = 0, dvr_fd, file_fd;
	int opt;
	int record = 0;
	int frontend_only = 0;
	char *filename = NULL;
	int human_readable = 0, rec_psi = 0;

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
			record=1;
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
			fprintf (stderr, usage, argv[0]);
			return -1;
		};
	}

	if (optind < argc)
		channel = argv[optind];

	if (!channel) {
		fprintf (stderr, usage, argv[0]);
		return -1;
	}

	snprintf (FRONTEND_DEV, sizeof(FRONTEND_DEV),
		  "/dev/dvb/adapter%i/frontend%i", adapter, frontend);

	snprintf (DEMUX_DEV, sizeof(DEMUX_DEV),
		  "/dev/dvb/adapter%i/demux%i", adapter, demux);

	snprintf (DVR_DEV, sizeof(DVR_DEV),
		  "/dev/dvb/adapter%i/dvr%i", adapter, demux);

	if (silent<2)
		fprintf (stderr,"using '%s' and '%s'\n", FRONTEND_DEV, DEMUX_DEV);

	if (!confname)
	{
		int len = strlen(homedir) + strlen(CHANNEL_FILE) + 18;
		if (!homedir)
			ERROR ("$HOME not set");
		confname = malloc (len);
		snprintf (confname, len, "%s/.tzap/%i/%s",
			  homedir, adapter, CHANNEL_FILE);
		if (access (confname, R_OK))
			snprintf (confname, len, "%s/.tzap/%s",
				  homedir, CHANNEL_FILE);
	}
	printf("reading channels from file '%s'\n", confname);

	memset(&frontend_param, 0, sizeof(struct dvb_frontend_parameters));

	if (parse (confname, channel, &frontend_param, &vpid, &apid, &sid))
		return -1;

	if ((frontend_fd = open(FRONTEND_DEV, O_RDWR)) < 0) {
		PERROR ("failed opening '%s'", FRONTEND_DEV);
		return -1;
	}

	if (setup_frontend (frontend_fd, &frontend_param) < 0)
		return -1;

	if (frontend_only)
		goto just_the_frontend_dude;

	if (rec_psi) {
	    pmtpid = get_pmt_pid(DEMUX_DEV, sid);
	    if (pmtpid <= 0) {
		fprintf(stderr,"couldn't find pmt-pid for sid %04x\n",sid);
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

	if (silent<2)
		fprintf (stderr,"video pid 0x%04x, audio pid 0x%04x\n", vpid, apid);

	if (set_pesfilter (video_fd, vpid, DMX_PES_VIDEO, dvr) < 0)
		return -1;

	if ((audio_fd = open(DEMUX_DEV, O_RDWR)) < 0) {
                PERROR("failed opening '%s'", DEMUX_DEV);
                return -1;
        }

	if (set_pesfilter (audio_fd, apid, DMX_PES_AUDIO, dvr) < 0)
		return -1;

	signal(SIGALRM,do_timeout);
	if (timeout>0)
		alarm(timeout);


	if (record)
	{
		if (filename!=NULL)
		{
			if (strcmp(filename,"-")!=0)
			{
				file_fd = open (filename,O_WRONLY|O_LARGEFILE|O_CREAT,0644);
				if (file_fd<0)
				{
					PERROR("open of '%s' failed",filename);
					return -1;
				}
			}
			else
			{
				file_fd=1;
			}
		}
		else
		{
			PERROR("Record mode but no filename!");
			return -1;
		}

		if ((dvr_fd = open(DVR_DEV, O_RDONLY)) < 0) {
	                PERROR("failed opening '%s'", DVR_DEV);
	                return -1;
	        }
		if (silent<2)
			print_frontend_stats (frontend_fd, human_readable);

		copy_to_file(dvr_fd,file_fd);

		if (silent<2)
			print_frontend_stats (frontend_fd, human_readable);
	}
	else {
just_the_frontend_dude:
		check_frontend (frontend_fd, human_readable);
	}

	close (pat_fd);
	close (pmt_fd);
	close (audio_fd);
	close (video_fd);
	close (frontend_fd);

	return 0;
}
