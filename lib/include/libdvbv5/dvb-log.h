/*
 * Copyright (c) 2011-2014 - Mauro Carvalho Chehab
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

/**
 * @file dvb-log.h
 * @ingroup ancillary
 * @brief Provides interfaces to handle libdvbv5 log messages.
 * @copyright GNU General Public License version 2 (GPLv2)
 * @author Mauro Carvalho Chehab
 * @author Andre Roth
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

/**
 * @typedef void (*dvb_logfunc)(int level, const char *fmt, ...)
 * @brief typedef used by dvb_fe_open2 for the log function
 * @ingroup ancillary
 */

typedef void (*dvb_logfunc)(int level, const char *fmt, ...) __attribute__ (( format( printf, 2, 3 )));

/*
 * Macros used internally inside libdvbv5 frontend part, to output logs
 */

#ifndef _DOXYGEN

#ifndef __DVB_FE_PRIV_H

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
#define dvb_loginfo(fmt, arg...) do {\
	parms->logfunc(LOG_NOTICE, fmt, ##arg); \
} while (0)

#define dvb_perror(msg) do {\
	parms->logfunc(LOG_ERR, "%s: %s", msg, strerror(errno)); \
} while (0)

#else

#define dvb_log(fmt, arg...) do {\
	parms->p.logfunc(LOG_INFO, fmt, ##arg); \
} while (0)
#define dvb_logerr(fmt, arg...) do {\
	parms->p.logfunc(LOG_ERR, fmt, ##arg); \
} while (0)
#define dvb_logdbg(fmt, arg...) do {\
	parms->p.logfunc(LOG_DEBUG, fmt, ##arg); \
} while (0)
#define dvb_logwarn(fmt, arg...) do {\
	parms->p.logfunc(LOG_WARNING, fmt, ##arg); \
} while (0)
#define dvb_loginfo(fmt, arg...) do {\
	parms->p.logfunc(LOG_NOTICE, fmt, ##arg); \
} while (0)

#define dvb_perror(msg) do {\
	parms->p.logfunc(LOG_ERR, "%s: %s", msg, strerror(errno)); \
} while (0)

#endif

#endif /* _DOXYGEN */

/**
 * @brief This is the prototype of the internal log function that it is used,
 *	  if the library client doesn't desire to override with something else.
 * @ingroup ancillary
 *
 * @param level		level of the message, as defined at syslog.h
 * @param fmt		format string (same as format string on sprintf)
 */
void dvb_default_log(int level, const char *fmt, ...) __attribute__ (( format( printf, 2, 3 )));

#endif
