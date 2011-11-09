/*
 * Copyright (c) 2011 - Mauro Carvalho Chehab <mchehab@redhat.com>
 */

#include "dvb-fe.h"
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
	{ 0, 0, 0, 0, 0, 0 }
};

static int adapter = 0;
static int frontend = 0;
static int verbose = 1;		/* FIXME */

static error_t parse_opt(int k, char *arg, struct argp_state *state)
{
	switch (k) {
	case 'a':
		adapter = atoi(arg);
		break;
	case 'f':
		frontend = atoi(arg);
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

	parms = dvb_fe_open(adapter, frontend, verbose);
	dvb_fe_close(parms);

	return 0;
}
