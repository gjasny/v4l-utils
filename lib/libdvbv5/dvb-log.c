/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab <mchehab@redhat.com>
 * Copyright (c) 2012 - Andre Roth <neolynx@gmail.com>
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

#include "dvb-log.h"

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

static const struct loglevel {
	const char *name;
	const char *color;
	int fd;
} loglevels[9] = {
	{"EMERG   ", "\033[31m", STDERR_FILENO },
	{"ALERT   ", "\033[31m", STDERR_FILENO },
	{"CRITICAL", "\033[31m", STDERR_FILENO },
	{"ERROR   ", "\033[31m", STDERR_FILENO },
	{"WARNING ", "\033[33m", STDOUT_FILENO },
	{"NOTICE  ", "\033[36m", STDOUT_FILENO },
	{"INFO    ", "\033[36m", STDOUT_FILENO },
	{"DEBUG   ", "\033[32m", STDOUT_FILENO },
	{"",         "\033[0m",  STDOUT_FILENO },
};
#define LOG_COLOROFF 8

void dvb_default_log(int level, const char *fmt, ...)
{
	if(level > sizeof(loglevels) / sizeof(struct loglevel) - 2) // ignore LOG_COLOROFF as well
		level = LOG_INFO;
	va_list ap;
	va_start(ap, fmt);
	FILE *out = stdout;
	if(STDERR_FILENO == loglevels[level].fd)
		out = stderr;
	if(isatty(loglevels[level].fd))
		fputs(loglevels[level].color, out);
	fprintf(out, "%s ", loglevels[level].name);
	vfprintf(out, fmt, ap);
	fprintf(out, "\n");
	if(isatty(loglevels[level].fd))
		fputs(loglevels[LOG_COLOROFF].color, out);
	va_end(ap);
}

