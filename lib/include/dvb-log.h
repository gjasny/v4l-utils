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

#ifndef _LOG_H
#define _LOG_H

#include <syslog.h>

typedef void (*dvb_logfunc)(int level, const char *fmt, ...) __attribute__ (( format( printf, 2, 3 )));

#define dvb_log(fmt, arg...) do {\
	parms->logfunc(LOG_INFO, fmt, ##arg); \
} while (0)
#define dvb_logerr(fmt, arg...) do {\
	parms->logfunc(LOG_ERR, fmt, ##arg); \
} while (0)
#define dvb_logdbg(fmt, arg...) do {\
	parms->logfunc(LOG_DEBUG, fmt, ##arg); \
} while (0)
#define dvb_logwarn(fmt, arg...) do {\
	parms->logfunc(LOG_WARNING, fmt, ##arg); \
} while (0)


#define dvb_perror(msg) do {\
	parms->logfunc(LOG_ERR, "%s: %s", msg, strerror(errno)); \
} while (0)

void dvb_default_log(int level, const char *fmt, ...) __attribute__ (( format( printf, 2, 3 )));

#endif
