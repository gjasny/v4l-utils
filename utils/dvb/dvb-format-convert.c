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

#include "dvb-file.h"
#include "dvb-demux.h"
#include "libscan.h"

static int convert_file(const char *fname, const char *delsys,
			int format, const char *fout)
{
	struct dvb_file *dvb_file = NULL;
	uint32_t sys;

	printf("Reading file %s\n", fname);
	switch (format) {
	case 1:			/* DVB channel/transponder old format */
		dvb_file = parse_format_oneline(fname, " \n", SYS_UNDEFINED,
						channel_formats);
		break;
	case 2: 			/* DVB old zap format */
		if (!delsys) {
			fprintf(stderr, "Delivery system need to be specified, for zap format\n");
			return -1;
		}
		if (!strcasecmp(delsys, "DVB-T"))
			sys = SYS_DVBT;
		else if (!strcasecmp(delsys, "DVB-C"))
			sys = SYS_DVBC_ANNEX_A;
		else if (!strcasecmp(delsys, "DVB-S"))
			sys = SYS_DVBS;
		else if (!strcasecmp(delsys, "ATSC"))
			sys = SYS_ATSC;
		else {
			fprintf(stderr, "Delivery system unknown\n");
			return -1;
		}
		dvb_file = parse_format_oneline(fname, ":", sys, zap_formats);
		break;
	}
	if (!dvb_file)
		return -2;

	printf("Writing file %s\n", fout);
	write_dvb_file(fout, dvb_file);

	dvb_file_free(dvb_file);
	return 0;
}

static char *usage =
    "usage:\n"
    "       dvb-format-convert [options] <channels_file> <out file>\n"
    "        scan DVB services using the channel file\n"
    "     -v        : be (very) verbose\n"
    "     -o file   : output filename (use -o - for stdout)\n"
    "     -O        : uses old channel format\n"
    "     -z        : uses zap services file, discarding video/audio pid's\n"
    "     -s        : delivery system (DVB-T, DVB-C, DVB-S or ATSC) for zap format\n"
    "     -h -?     : display this help and exit\n\n"
    "-O or -z parameters are mandatory\n";

int main(int argc, char **argv)
{
	char *confname = NULL, *outfile = NULL, *delsys = NULL;
	int opt, format = 0;

	while ((opt = getopt(argc, argv, "H?hzOs:")) != -1) {
		switch (opt) {
		case 'O':
			format = 1;
			break;
		case 'z':
			format = 2;
			break;
		case 's':
			delsys = optarg;
			break;
		case '?':
		case 'h':
		default:
			fprintf(stderr, usage, argv[0]);
			return -1;
		};
	}

	if (!format) {
		fprintf(stderr, usage, argv[0]);
		return -1;
	}

	if (optind < argc + 1) {
		confname = argv[optind];
		outfile = argv[optind + 1];
	}

	if (!confname) {
		fprintf(stderr, usage, argv[0]);
		return -1;
	}

	return convert_file(confname, delsys, format, outfile);
}
