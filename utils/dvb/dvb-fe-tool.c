/*
 * Copyright (c) 2011 - Mauro Carvalho Chehab <mchehab@redhat.com>
 */

#include "dvb-fe.h"
#include <config.h>
#include <argp.h>
#include <stdlib.h>
#include <stdio.h>

const char *argp_program_version = "DVBv5 scan version "V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <mchehab@redhat.com>";

static const char doc[] = "\nAllows scanning DVB using API version 5\n"
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
	int i;

	switch (k) {
	case 'a':
		adapter = atoi(arg);
		break;
	case 'f':
		frontend = atoi(arg);
		break;
	case 'd':
		for (i = 0; i < ARRAY_SIZE(delivery_system_name); i++)
			if (delivery_system_name[i] &&
			    !strcasecmp(arg, delivery_system_name[i]))
				break;
		if (i < ARRAY_SIZE(delivery_system_name)) {
			delsys = i;
			break;
		}
		/* Not found. Print all possible values */
		fprintf(stderr, "Delivery system %s is not known. Valid values are:\n",
			arg);
		for (i = 0; i < ARRAY_SIZE(delivery_system_name) - 1; i++) {
			fprintf(stderr, "%-15s", delivery_system_name[i]);
			if (!((i + 1) % 5))
				fprintf(stderr, "\n");
		}
		fprintf(stderr, "\n");
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
		dvb_fe_prt_parms(stdout, parms);
	}

	dvb_fe_close(parms);

	return 0;
}
