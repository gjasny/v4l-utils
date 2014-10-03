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
#include <config.h>
#include <argp.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define PROGRAM_NAME	"dvb-fe-tool"

const char *argp_program_version = PROGRAM_NAME " version " V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <m.chehab@samsung.com>";

static const char doc[] = "\nA DVB frontend tool using API version 5\n"
	"\nOn the options bellow, the arguments are:\n"
	"  ADAPTER      - the dvb adapter to control\n"
	"  FRONTEND     - the dvb frontend to control";

static const struct argp_option options[] = {
	{"verbose",	'v',	0,		0,	"enables debug messages", 0},
	{"adapter",	'a',	"ADAPTER",	0,	"dvb adapter", 0},
	{"frontend",	'f',	"FRONTEND",	0,	"dvb frontend", 0},
	{"set-delsys",	'd',	"PARAMS",	0,	"set delivery system", 0},
	{"femon",	'm',	0,		0,	"monitors frontend stats on an streaming frontend", 0},
	{"acoustical",	'A',	0,		0,	"bips if signal quality is good. Also enables femon mode. Please notice that console bip should be enabled on your wm.", 0},
#if 0 /* Currently not implemented */
	{"set",		's',	"PARAMS",	0,	"set frontend", 0},
#endif
	{"get",		'g',	0,		0,	"get frontend", 0},
	{"dvbv3",	'3',	0,		0,	"Use DVBv3 only", 0},
	{ 0, 0, 0, 0, 0, 0 }
};

static int adapter = 0;
static int frontend = 0;
static unsigned get = 0;
static char *set_params = NULL;
static int verbose = 0;
static int dvbv3 = 0;
static int delsys = 0;
static int femon = 0;
static int acoustical = 0;
static int timeout_flag = 0;

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

#define PERROR(x...)                                                    \
	do {                                                            \
		fprintf(stderr, "ERROR: ");                             \
		fprintf(stderr, x);                                     \
		fprintf(stderr, " (%s)\n", strerror(errno));		\
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
	case '3':
		dvbv3++;
		break;
	case 'v':
		verbose	++;
		break;
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
		if (!timeout_flag)
			usleep(1000000);
	} while (!timeout_flag);
}

int main(int argc, char *argv[])
{
	struct dvb_v5_fe_parms *parms;
	int fe_flags = O_RDWR;

	argp_parse(&argp, argc, argv, 0, 0, 0);

	/*
	 * If called without any option, be verbose, to print the
	 * DVB frontend information.
	 */
	if (!get && !delsys && !set_params && !femon)
		verbose++;

	if (!delsys && !set_params)
		fe_flags = O_RDONLY;

	parms = dvb_fe_open_flags(adapter, frontend, verbose, dvbv3,
				  NULL, fe_flags);
	if (!parms)
		return -1;

	if (delsys) {
		printf("Changing delivery system to: %s\n",
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
	dvb_fe_close(parms);

	return 0;
}
