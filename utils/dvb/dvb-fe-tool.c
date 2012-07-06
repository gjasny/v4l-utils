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
 */

#include "dvb-file.h"
#include <config.h>
#include <argp.h>
#include <stdlib.h>
#include <stdio.h>

#define PROGRAM_NAME	"dvb-fe-tool"

const char *argp_program_version = PROGRAM_NAME " version " V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <mchehab@redhat.com>";

static const char doc[] = "\nA DVB frontend tool using API version 5\n"
	"\nOn the options bellow, the arguments are:\n"
	"  ADAPTER      - the dvb adapter to control\n"
	"  FRONTEND     - the dvb frontend to control";

static const struct argp_option options[] = {
	{"verbose",	'v',	0,		0,	"enables debug messages", 0},
	{"adapter",	'a',	"ADAPTER",	0,	"dvb adapter", 0},
	{"frontend",	'f',	"FRONTEND",	0,	"dvb frontend", 0},
	{"set-delsys",	'd',	"PARAMS",	0,	"set delivery system", 0},
	{"set",		's',	"PARAMS",	0,	"set frontend", 0},
	{"get",		'g',	0,		0,	"get frontend", 0},
	{"dvbv3",	'3',	0,		0,	"Use DVBv3 only", 0},
	{ 0, 0, 0, 0, 0, 0 }
};

static int adapter = 0;
static int frontend = 0;
static unsigned get = 0;
static char *set_params = NULL;
static int verbose = 1;		/* FIXME */
static int dvbv3 = 0;
static int delsys = 0;

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
		delsys = parse_delsys(arg);
		if (delsys < 0)
			return ARGP_ERR_UNKNOWN;
		break;
	case 's':
		set_params = arg;
		break;
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

int main(int argc, char *argv[])
{
	struct dvb_v5_fe_parms *parms;

	argp_parse(&argp, argc, argv, 0, 0, 0);

	parms = dvb_fe_open(adapter, frontend, verbose, dvbv3);
	if (!parms)
		return -1;

	if (delsys) {
		printf("Changing delivery system to: %s\n",
			delivery_system_name[delsys]);
		dvb_set_sys(parms, delsys);
	}

#if 0
	if (set_params)
		do_something();
#endif
	if (get) {
		dvb_fe_get_parms(parms);
		dvb_fe_prt_parms(parms);
	}

	dvb_fe_close(parms);

	return 0;
}
