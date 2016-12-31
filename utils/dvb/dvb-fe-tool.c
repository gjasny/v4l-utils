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
 */

#include "libdvbv5/dvb-file.h"
#include "libdvbv5/dvb-dev.h"
#include <config.h>
#include <argp.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

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

#define PROGRAM_NAME	"dvb-fe-tool"

const char *argp_program_version = PROGRAM_NAME " version " V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <m.chehab@samsung.com>";

static const char doc[] = N_(
	"\nA DVB frontend tool using API version 5\n"
	"\nOn the options below, the arguments are:\n"
	"  ADAPTER      - the dvb adapter to control\n"
	"  FRONTEND     - the dvb frontend to control\n"
	"  SERVER       - server address whith is running the dvb5-daemon\n"
	"  PORT         - server port used by the dvb5-daemon\n");

static const struct argp_option options[] = {
	{"verbose",	'v',	0,		0,	N_("enables debug messages"), 0},
	{"adapter",	'a',	N_("ADAPTER"),	0,	N_("dvb adapter"), 0},
	{"frontend",	'f',	N_("FRONTEND"),	0,	N_("dvb frontend"), 0},
	{"set-delsys",	'd',	N_("PARAMS"),	0,	N_("set delivery system"), 0},
	{"femon",	'm',	0,		0,	N_("monitors frontend stats on an streaming frontend"), 0},
	{"acoustical",	'A',	0,		0,	N_("bips if signal quality is good. Also enables femon mode. Please notice that console bip should be enabled on your wm."), 0},
#if 0 /* Currently not implemented */
	{"set",		's',	N_("PARAMS"),	0,	N_("set frontend"), 0},
#endif
	{"get",		'g',	0,		0,	N_("get frontend"), 0},
	{"server",	'H',	N_("SERVER"),	0, 	N_("dvbv5-daemon host IP address"), 0},
	{"tcp-port",	'T',	N_("PORT"),	0, 	N_("dvbv5-daemon host tcp port"), 0},
	{"device-mon",	'D',	0,		0,	N_("monitors device insert/removal"), 0},
	{"count",	'c',	N_("COUNT"),	0,	N_("samples to take (default 0 = infinite)"), 0},
	{"help",        '?',	0,		0,	N_("Give this help list"), -1},
	{"usage",	-3,	0,		0,	N_("Give a short usage message")},
	{"version",	'V',	0,		0,	N_("Print program version"), -1},
	{ 0, 0, 0, 0, 0, 0 }
};

static int adapter = 0;
static int frontend = 0;
static unsigned get = 0;
static char *set_params = NULL;
static char *server = NULL;
static unsigned port = 0;
static int verbose = 0;
static int delsys = 0;
static int femon = 0;
static int acoustical = 0;
static int timeout_flag = 0;
static int device_mon = 0;
static int count = 0;

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

#define ERROR(x...)                                                     \
	do {                                                            \
		fprintf(stderr, _("ERROR: "));                             \
		fprintf(stderr, x);                                     \
		fprintf(stderr, "\n");                                 \
	} while (0)


static error_t parse_opt(int k, char *arg, struct argp_state *state)
{
	switch (k) {
	case 'a':
		adapter = atoi(arg);
		break;
	case 'f':
		frontend = atoi(arg);
		break;
	case 'd':
		delsys = dvb_parse_delsys(arg);
		if (delsys < 0)
			return ARGP_ERR_UNKNOWN;
		break;
	case 'm':
		femon++;
		break;
	case 'A':
		femon++;
		acoustical++;
		break;
#if 0
	case 's':
		set_params = arg;
		break;
#endif
	case 'g':
		get++;
		break;
	case 'D':
		device_mon++;
		break;
	case 'H':
		server = arg;
		break;
	case 'T':
		port = atoi(arg);
		break;
	case 'v':
		verbose	++;
		break;
	case 'c':
		count = atoi(arg);
		break;
	case '?':
		argp_state_help(state, state->out_stream,
				ARGP_HELP_SHORT_USAGE | ARGP_HELP_LONG
				| ARGP_HELP_DOC);
		fprintf(state->out_stream, _("\nReport bugs to %s.\n"), argp_program_bug_address);
		exit(0);
	case 'V':
		fprintf (state->out_stream, "%s\n", argp_program_version);
		exit(0);
	case -3:
		argp_state_help(state, state->out_stream, ARGP_HELP_USAGE);
		exit(0);
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = {
	.options = options,
	.parser = parse_opt,
	.doc = doc,
};

static int print_frontend_stats(FILE *fd,
				struct dvb_v5_fe_parms *parms)
{
	char buf[512], *p;
	int rc, i, len, show, n_status_lines = 0;

	rc = dvb_fe_get_stats(parms);
	if (rc) {
		ERROR(_("dvb_fe_get_stats failed"));
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
			if (isatty(fileno(fd))) {
				enum dvb_quality qual;
				int color;

				qual = dvb_fe_retrieve_quality(parms, 0);

				switch (qual) {
				case DVB_QUAL_POOR:
					color = 31;
					break;
				case DVB_QUAL_OK:
					color = 36;
					break;
				case DVB_QUAL_GOOD:
					color = 32;
					break;
				case DVB_QUAL_UNKNOWN:
				default:
					color = 0;
					break;
				}
				fprintf(fd, "\033[%dm", color);
				/*
				 * It would be great to change the BELL
				 * tone depending on the quality. The legacy
				 * femon used to to that, but this doesn't
				 * work anymore with modern Linux distros.
				 *
				 * So, just print a bell if quality is good.
				 *
				 * The console audio should be enabled
				 * at the window manater for this to
				 * work.
				 */
				if (acoustical) {
					if (qual == DVB_QUAL_GOOD)
						fprintf(fd, "\a");
				}
			}

			if (n_status_lines)
				fprintf(fd, "\t%s\n", buf);
			else
				fprintf(fd, "%s\n", buf);

			n_status_lines++;

			p = buf;
			len = sizeof(buf);
		}
	}

	fflush(fd);

	return 0;
}

static void get_show_stats(struct dvb_v5_fe_parms *parms)
{
	int rc;

	signal(SIGTERM, do_timeout);
	signal(SIGINT, do_timeout);

	do {
		rc = dvb_fe_get_stats(parms);
		if (!rc)
			print_frontend_stats(stderr, parms);
		if (count > 0 && !--count)
			break;
		if (!timeout_flag)
			usleep(1000000);
	} while (!timeout_flag);
}

static const char const *event_type[] = {
	[DVB_DEV_ADD] = "added",
	[DVB_DEV_CHANGE] = "changed",
	[DVB_DEV_REMOVE] = "removed",
};

static int dev_change_monitor(char *sysname,
			       enum dvb_dev_change_type type)
{
	if (type > ARRAY_SIZE(event_type))
		printf("unknown event on device %s\n", sysname);
	else
		printf("device %s was %s\n", sysname, event_type[type]);
	free(sysname);

	return 0;
}

int main(int argc, char *argv[])
{
	struct dvb_device *dvb;
	struct dvb_dev_list *dvb_dev;
	struct dvb_v5_fe_parms *parms;
	int ret, fe_flags = O_RDWR;

#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);
#endif

	if (argp_parse(&argp, argc, argv, ARGP_NO_HELP | ARGP_NO_EXIT, 0, 0)) {
		argp_help(&argp, stderr, ARGP_HELP_SHORT_USAGE, PROGRAM_NAME);
		return -1;
	}

	/*
	 * If called without any option, be verbose, to print the
	 * DVB frontend information.
	 */
	if (!get && !delsys && !set_params && !femon)
		verbose++;

	if (!delsys && !set_params)
		fe_flags = O_RDONLY;

	dvb = dvb_dev_alloc();
	if (!dvb)
		return -1;

	if (server && port) {
		printf(_("Connecting to %s:%d\n"), server, port);
		ret = dvb_dev_remote_init(dvb, server, port);
		if (ret < 0)
			return -1;
	}

	dvb_dev_set_log(dvb, verbose, NULL);
	if (device_mon) {
		dvb_dev_find(dvb, &dev_change_monitor);
		while (1) {
			usleep(1000000);
		}
	}
	dvb_dev_find(dvb, NULL);
	parms = dvb->fe_parms;

	dvb_dev = dvb_dev_seek_by_sysname(dvb, adapter, frontend,
					  DVB_DEVICE_FRONTEND);
	if (!dvb_dev)
		return -1;

	if (!dvb_dev_open(dvb, dvb_dev->sysname, fe_flags))
		return -1;

	if (delsys) {
		printf(_("Changing delivery system to: %s\n"),
			delivery_system_name[delsys]);
		dvb_set_sys(parms, delsys);
		goto ret;
	}

#if 0
	if (set_params)
		do_something();
#endif
	if (get) {
		dvb_fe_get_parms(parms);
		dvb_fe_prt_parms(parms);
	}

	if (femon)
		get_show_stats(parms);

ret:
	dvb_dev_free(dvb);

	return 0;
}
