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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <argp.h>

#include <config.h>

#include "dvb-file.h"
#include "dvb-demux.h"
#include "dvb-scan.h"

#define PROGRAM_NAME	"dvb-format-convert"

struct arguments {
	char *input_file, *output_file;
	enum file_formats input_format, output_format;
	int delsys;
};

static const struct argp_option options[] = {
	{"input-format",	'I',	"format",	0, "Input format: ZAP, CHANNEL, DVBV5", 0},
	{"output-format",	'O',	"format",	0, "Input format: ZAP, CHANNEL, DVBV5", 0},
	{"delsys",		's',	"system",	0, "Delivery system type. Needed if input or output format is ZAP", 0},
	{ 0, 0, 0, 0, 0, 0 }
};

const char *argp_program_version = PROGRAM_NAME " version " V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <mchehab@redhat.com>";

static error_t parse_opt(int k, char *optarg, struct argp_state *state)
{
	struct arguments *args = state->input;
	switch (k) {
	case 'I':
		args->input_format = parse_format(optarg);
		break;
	case 'O':
		args->output_format = parse_format(optarg);
		break;
	case 's':
		args->delsys = parse_delsys(optarg);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	};
	return 0;
}

static int convert_file(struct arguments *args)
{
	struct dvb_file *dvb_file = NULL;
	int ret;

	printf("Reading file %s\n", args->input_file);

	dvb_file = dvb_read_file_format(args->input_file, args->delsys,
				    args->input_format);
	if (!dvb_file) {
		fprintf(stderr, "Error reading file %s\n", args->input_file);
		return -1;
	}

	printf("Writing file %s\n", args->output_file);
	ret = write_file_format(args->output_file, dvb_file,
				args->delsys, args->output_format);

	return ret;
}

int main(int argc, char **argv)
{
	struct arguments args;
	int idx = -1, missing = 0;
	const struct argp argp = {
		.options = options,
		.parser = parse_opt,
		.doc = "scan DVB services using the channel file",
		.args_doc = "<input file> <output file>",
	};

	memset(&args, 0, sizeof(args));
	argp_parse(&argp, argc, argv, 0, &idx, &args);

	if (idx + 1 < argc) {
		args.input_file = argv[idx];
		args.output_file = argv[idx + 1];
	}

	if (args.input_format == FILE_UNKNOWN) {
		fprintf(stderr, "ERROR: Please specify a valid input format\n");
		missing = 1;
	} else if (!args.input_file) {
		fprintf(stderr, "ERROR: Please specify a valid input file\n");
		missing = 1;
	} else if (args.output_format == FILE_UNKNOWN) {
		fprintf(stderr, "ERROR: Please specify a valid output format\n");
		missing = 1;
	} else if (!args.output_file) {
		fprintf(stderr, "ERROR: Please specify a valid output file\n");
		missing = 1;
	} else if (((args.input_format == FILE_ZAP) ||
		   (args.output_format == FILE_ZAP)) && args.delsys <= 0) {
		fprintf(stderr, "ERROR: Please specify a valid delivery system for ZAP format\n");
		missing = 1;
	}
	if (missing) {
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, PROGRAM_NAME);
		return -1;
	}

	return convert_file(&args);
}
