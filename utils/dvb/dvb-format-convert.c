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

#include "libdvbv5/dvb-file.h"
#include "libdvbv5/dvb-demux.h"
#include "libdvbv5/dvb-scan.h"

#define PROGRAM_NAME	"dvb-format-convert"

struct arguments {
	char *input_file, *output_file;
	enum dvb_file_formats input_format, output_format;
	int delsys;
};

static const struct argp_option options[] = {
	{"input-format",	'I',	N_("format"),	0, N_("Valid input formats: ZAP, CHANNEL, DVBV5"), 0},
	{"output-format",	'O',	N_("format"),	0, N_("Valid output formats: VDR, ZAP, CHANNEL, DVBV5"), 0},
	{"delsys",		's',	N_("system"),	0, N_("Delivery system type. Needed if input or output format is ZAP"), 0},
	{"help",        '?',	0,		0,	N_("Give this help list"), -1},
	{"usage",	-3,	0,		0,	N_("Give a short usage message")},
	{"version",	'V',	0,		0,	N_("Print program version"), -1},
	{ 0, 0, 0, 0, 0, 0 }
};

const char *argp_program_version = PROGRAM_NAME " version " V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <m.chehab@samsung.com>";

static error_t parse_opt(int k, char *optarg, struct argp_state *state)
{
	struct arguments *args = state->input;
	switch (k) {
	case 'I':
		args->input_format = dvb_parse_format(optarg);
		break;
	case 'O':
		args->output_format = dvb_parse_format(optarg);
		break;
	case 's':
		args->delsys = dvb_parse_delsys(optarg);
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
	};
	return 0;
}

static int convert_file(struct arguments *args)
{
	struct dvb_file *dvb_file = NULL;
	int ret;

	printf(_("Reading file %s\n"), args->input_file);

	dvb_file = dvb_read_file_format(args->input_file, args->delsys,
				    args->input_format);
	if (!dvb_file) {
		fprintf(stderr, _("Error reading file %s\n"), args->input_file);
		return -1;
	}

	printf(_("Writing file %s\n"), args->output_file);
	ret = dvb_write_file_format(args->output_file, dvb_file,
				    args->delsys, args->output_format);
	dvb_file_free(dvb_file);

	return ret;
}

int main(int argc, char **argv)
{
	struct arguments args;
	int idx = -1, missing = 0;
	const struct argp argp = {
		.options = options,
		.parser = parse_opt,
		.doc = N_("scan DVB services using the channel file"),
		.args_doc = N_("<input file> <output file>"),
	};

#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);
#endif

	memset(&args, 0, sizeof(args));
	argp_parse(&argp, argc, argv, ARGP_NO_HELP | ARGP_NO_EXIT, &idx, &args);

	if (idx + 1 < argc) {
		args.input_file = argv[idx];
		args.output_file = argv[idx + 1];
	}

	if (args.input_format == FILE_UNKNOWN) {
		fprintf(stderr, _("ERROR: Please specify a valid input format\n"));
		missing = 1;
	} else if (!args.input_file) {
		fprintf(stderr, _("ERROR: Please specify a valid input file\n"));
		missing = 1;
	} else if (args.output_format == FILE_UNKNOWN) {
		fprintf(stderr, _("ERROR: Please specify a valid output format\n"));
		missing = 1;
	} else if (!args.output_file) {
		fprintf(stderr, _("ERROR: Please specify a valid output file\n"));
		missing = 1;
	} else if (((args.input_format == FILE_ZAP) ||
		   (args.output_format == FILE_ZAP)) &&
		   (args.delsys <= 0 || args.delsys == SYS_ISDBS)) {
		fprintf(stderr, _("ERROR: Please specify a valid delivery system for ZAP format\n"));
		missing = 1;
	}
	if (missing) {
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, PROGRAM_NAME);
		return -1;
	}

	return convert_file(&args);
}
