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

#define _XOPEN_SOURCE 600

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1
#define _LARGEFILE64_SOURCE 1

#include <config.h>

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include <argp.h>
#include <config.h>
#include <endian.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <search.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <sys/socket.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "../../lib/libdvbv5/dvb-fe-priv.h"
#include "../../lib/libdvbv5/dvb-dev-priv.h"
#include "libdvbv5/dvb-file.h"
#include "libdvbv5/dvb-dev.h"

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

/* Max number of open files */
#define NUM_FOPEN	1024

/*
 * Argument processing data and logic
 */

#define PROGRAM_NAME	"dvbv5-daemon"

const char *argp_program_version = PROGRAM_NAME " version " V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <m.chehab@samsung.com>";

static const char doc[] = N_(
	"\nA DVB remote daemon using API version 5\n");

static const struct argp_option options[] = {
	{"verbose",	'v',	0,		0,	N_("enables debug messages"), 0},
	{"port",	'p',	"5555",		0,	N_("port to listen"), 0},
	{"help",        '?',	0,		0,	N_("Give this help list"), -1},
	{"usage",	-3,	0,		0,	N_("Give a short usage message")},
	{"version",	'V',	0,		0,	N_("Print program version"), -1},
	{ 0, 0, 0, 0, 0, 0 }
};

static int port = 0;
static int verbose = 0;

static error_t parse_opt(int k, char *arg, struct argp_state *state)
{
	switch (k) {
	case 'p':
		port = atoi(arg);
		break;
	case 'v':
		verbose	++;
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
	}
	return 0;
}

static struct argp argp = {
	.options = options,
	.parser = parse_opt,
	.doc = doc,
};

/*
 * Local log
 *
 * Error messages should ideally be sent to the remote end, but, if
 * an error happens during the processing of a packet, we need to
 * report locally, as reporting remotely may not work.
 */
static const struct loglevel {
	const char *name;
	const char *color;
	int fd;
} loglevels[9] = {
	{N_("EMERG    "), "\033[31m", STDERR_FILENO },
	{N_("ALERT    "), "\033[31m", STDERR_FILENO },
	{N_("CRITICAL "), "\033[31m", STDERR_FILENO },
	{N_("ERROR    "), "\033[31m", STDERR_FILENO },
	{N_("WARNING  "), "\033[33m", STDOUT_FILENO },
	{NULL,            "\033[36m", STDOUT_FILENO }, /* NOTICE */
	{NULL,            NULL,       STDOUT_FILENO }, /* INFO */
	{N_("DEBUG    "), "\033[32m", STDOUT_FILENO },
	{NULL,            "\033[0m",  STDOUT_FILENO }, /* reset*/
};
#define LOG_COLOROFF 8

void local_log(int level, const char *fmt, ...)
{
	if(level > sizeof(loglevels) / sizeof(struct loglevel) - 2) // ignore LOG_COLOROFF as well
		level = LOG_INFO;
	va_list ap;
	va_start(ap, fmt);
	FILE *out = stdout;
	if (STDERR_FILENO == loglevels[level].fd)
		out = stderr;
	if (loglevels[level].color && isatty(loglevels[level].fd))
		fputs(loglevels[level].color, out);
	if (loglevels[level].name)
		fprintf(out, "%s", _(loglevels[level].name));
	vfprintf(out, fmt, ap);
	fprintf(out, "\n");
	if(loglevels[level].color && isatty(loglevels[level].fd))
		fputs(loglevels[LOG_COLOROFF].color, out);
	va_end(ap);
}

#define info(fmt, arg...) do {\
	local_log(LOG_INFO, fmt, ##arg); \
} while (0)

#define warn(fmt, arg...) do {\
	local_log(LOG_WARNING, fmt, ##arg); \
} while (0)

#define err(fmt, arg...) do {\
	local_log(LOG_ERR, "%s: " fmt, __FUNCTION__, ##arg); \
} while (0)

#define dbg(fmt, arg...) do {\
	local_log(LOG_DEBUG, "%s: " fmt, __FUNCTION__, ##arg); \
} while (0)

#define local_perror(msg) do {\
	local_log(LOG_ERR, "%s: %s: %m (%d)",  __FUNCTION__, msg, errno); \
} while (0)

/*
 * Static data used by the code
 */

static pthread_mutex_t msg_mutex;
static pthread_mutex_t dvb_read_mutex;
static pthread_t read_id = 0;

struct dvb_descriptors {
	int uid;
	struct dvb_open_descriptor *open_dev;
};

static struct dvb_device *dvb = NULL;
static void *desc_root = NULL;
static int dvb_fd = -1;

static struct pollfd fds[NUM_FOPEN];
static nfds_t numfds = 0;

static char output_charset[256] = "utf-8";
static char default_charset[256] = "iso-8859-1";

void stack_dump()
{
#ifdef HAVE_BACKTRACE
	int i, nptrs = 0;
	void *buffer[10];
	char **strings = NULL;

	nptrs = backtrace(buffer, sizeof(buffer));

	if (nptrs) {
		strings = backtrace_symbols(buffer, nptrs);
		dbg("Stack:");
	}

	for (i = 0; i < nptrs; i++)
		dbg("   %s", strings[i]);

	free(strings);
#endif
}

/*
 * Open dev descriptor handling
 */

static int dvb_desc_compare(const void *__a, const void *__b)
{
	const struct dvb_descriptors *a = __a, *b = __b;

	return (b->uid - a->uid);
}

static struct dvb_open_descriptor *get_open_dev(int uid)
{
	struct dvb_descriptors desc, **p;

	if (!desc_root)
		return NULL;

	desc.uid = uid;
	p = tfind(&desc, &desc_root, dvb_desc_compare);

	if (!p) {
		err("open element not retrieved!");
		return NULL;
	}

	return (*p)->open_dev;
}

static void destroy_open_dev(int uid)
{
	struct dvb_descriptors desc, **p;

	desc.uid = uid;
	p = tdelete(&desc, &desc_root, dvb_desc_compare);
	if (!p)
		err("can't destroy opened element");
}

static void free_opendevs(void *node)
{
	struct dvb_descriptors *desc = node;

	if (verbose)
		dbg("closing dev %p", desc, desc->open_dev);

	dvb_dev_close(desc->open_dev);
	free (desc);
}

static void close_all_devs(void)
{
	dvb_fd = -1;
	numfds = 0;
	tdestroy(desc_root, free_opendevs);

	desc_root = NULL;
}

/*
 * Signal handling logic
 */

static void sigterm_handler(int const sig_class)
{
	info(PROGRAM_NAME" interrupted.");

	pthread_exit(NULL);

	close_all_devs();
	dvb_dev_free(dvb);
}

static void start_signal_handler(void)
{
	struct sigaction action;

	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_handler = sigterm_handler;
	sigaction(SIGTERM, &action, NULL);
}

static void stop_signal_handler(void)
{
	struct sigaction action;

	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_handler = SIG_DFL;
	sigaction(SIGTERM, &action, NULL);
}

/*
 * Functions to send/receive messages to the client
 */

static ssize_t __prepare_data(char *buf, const size_t size,
			      const char *fmt, va_list ap)
{
	char *p = buf, *endp = &buf[size], *s;
	int len;
	int32_t i32;
	uint64_t u64;

	while (*fmt && *fmt != '%') fmt++;
	if (*fmt == '%') fmt++;
	while (*fmt) {
		switch (*fmt++) {
		case 's':              /* string */
			s = va_arg(ap, char *);
			if (s)
				len = strlen(s);
			else
				len = 0;
			if (p + len + 4 > endp) {
				dbg("buffer to short for string");
				stack_dump();
				return -1;
			}
			i32 = htobe32(len);
			memcpy(p, &i32, 4);
			p += 4;
			if (s)
				memcpy(p, s, len);
			p += len;
			break;
		case 'p':              /* binary data with specified length */
			s = va_arg(ap, char *);
			len = va_arg(ap, ssize_t);
			if (p + len + 4 > endp) {
				dbg("buffer to short for string");
				stack_dump();
				return -1;
			}
			i32 = htobe32(len);
			memcpy(p, &i32, 4);
			p += 4;
			memcpy(p, s, len);
			p += len;
			break;
		case 'i':              /* 32-bit int */
			if (p + 4 > endp) {
				dbg("buffer to short for int32_t");
				stack_dump();
				return -1;
			}

			i32 = htobe32(va_arg(ap, int32_t));
			memcpy(p, &i32, 4);
			p += 4;
			break;
		case 'l':              /* 64-bit unsigned int */
			if (*fmt++ != 'u') {
				dbg("invalid long format character: '%c'", *fmt);
				stack_dump();
				break;
			}
			if (p + 8 > endp) {
				dbg("buffer to short for uint64_t");
				stack_dump();
				return -1;
			}
			u64 = htobe64(va_arg(ap, uint64_t));
			memcpy(p, &u64, 8);
			p += 8;
			break;
		default:
			dbg("invalid format character: '%c'", *fmt);
			stack_dump();
		}
		while (*fmt && *fmt != '%') fmt++;
		if (*fmt == '%') fmt++;
	}
	return p - buf;
}


static ssize_t prepare_data(char *buf, const size_t size,
			      const char *fmt, ...)
	__attribute__ (( format( printf, 3, 4 )));

static ssize_t prepare_data(char *buf, const size_t size,
			      const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = __prepare_data(buf, size, fmt, ap);
	va_end(ap);

	return ret;
}

static int send_buf(int fd, const char *buf, size_t size)
{
	int ret;
	int32_t i32;

	if (fd < 0)
		return ECONNRESET;

	pthread_mutex_lock(&msg_mutex);
	i32 = htobe32(size);
	ret = send(fd, (void *)&i32, 4, MSG_MORE);
	if (ret >= 0)
		ret = send(fd, buf, size, 0);
	pthread_mutex_unlock(&msg_mutex);
	if (ret < 0) {
		local_perror("write");
		if (ret == ECONNRESET)
			close_all_devs();

		return errno;
	}

	return ret;
}

static ssize_t send_data(int fd, const char *fmt, ...)
	__attribute__ (( format( printf, 2, 3 )));

static ssize_t send_data(int fd, const char *fmt, ...)
{
	char buf[REMOTE_BUF_SIZE];
	va_list ap;
	int ret;

	if (verbose)
		dbg("called %s(fd, \"%s\", ...)", __FUNCTION__, fmt);

	va_start(ap, fmt);
	ret = __prepare_data(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (ret < 0)
		return ret;

	return send_buf(fd, buf, ret);
}

static ssize_t scan_data(char *buf, int buf_size, const char *fmt, ...)
	__attribute__ (( format( scanf, 3, 4 )));

static ssize_t scan_data(char *buf, int buf_size, const char *fmt, ...)
{
	char *p = buf, *endp = &buf[buf_size], *s;
	int len;
	int32_t *i32;
	uint64_t *u64;
	va_list ap;

	va_start(ap, fmt);
	while (*fmt && *fmt != '%') fmt++;
	if (*fmt == '%') fmt++;
	while (*fmt) {
		switch (*fmt++) {
		case 's':              /* string */
			s = va_arg(ap, char *);
			if (p + 4 > endp) {
				dbg("buffer to short for string length");
				stack_dump();
				return -1;
			}
			len = be32toh(*(int32_t *)p);
			p += 4;
			if (p + len > endp) {
				dbg("buffer to short for string");
				stack_dump();
				return -1;
			}

			memcpy(s, p, len);
			s[len] = '\0';
			p += len;
			break;
		case 'i':              /* 32-bit int */
			if (p + 4 > endp) {
				dbg("buffer to short for int32_t");
				stack_dump();
				return -1;
			}
			i32 = va_arg(ap, int32_t *);

			*i32 = be32toh(*(int32_t *)p);
			p += 4;
			break;
		case 'l':              /* 64-bit unsigned int */
			if (*fmt++ != 'u') {
				dbg("invalid long format character: '%c'", *fmt);
				stack_dump();
				break;
			}
			if (p + 8 > endp) {
				dbg("buffer to short for uint64_t");
				stack_dump();
				return -1;
			}
			u64 = va_arg(ap, uint64_t *);

			*u64 = be32toh(*(uint64_t *)p);
			p += 8;
			break;
		default:
			dbg("invalid format character: '%c'", *fmt);
			stack_dump();
		}
		while (*fmt && *fmt != '%') fmt++;
		if (*fmt == '%') fmt++;
	}
	va_end(ap);

	return p - buf;
}

/*
 * Remote log
 */
void dvb_remote_log(int level, const char *fmt, ...)
{
	int ret;
	char *buf;

	va_list ap;

	va_start(ap, fmt);
	ret = vasprintf(&buf, fmt, ap);
	if (ret <= 0) {
		local_perror("vasprintf");
		return;
	}

	va_end(ap);

	if (dvb_fd > 0)
		send_data(dvb_fd, "%i%s%i%s", 0, "log", level, buf);
	else
		local_log(level, buf);

	free(buf);
}

static int dev_change_monitor(char *sysname,
			      enum dvb_dev_change_type type)
{
	send_data(dvb_fd, "%i%s%i%s", 0, "dev_change", type, sysname);

	return 0;
}


/*
 * command handler methods
 */
static int daemon_get_version(uint32_t seq, char *cmd, int fd,
			      char *buf, ssize_t size)
{
	int ret = 0;

	return send_data(fd, "%i%s%i%s", seq, cmd, ret, argp_program_version);
}

static int dev_find(uint32_t seq, char *cmd, int fd, char *buf, ssize_t size)
{
	int enable_monitor = 0, ret;
	dvb_dev_change_t handler = NULL;

	ret = scan_data(buf, size, "%i", &enable_monitor);
	if (ret < 0)
		goto error;

	if (enable_monitor)
		handler = &dev_change_monitor;

	ret = dvb_dev_find(dvb, handler);

error:
	return send_data(fd, "%i%s%i", seq, cmd, ret);
}

static int dev_stop_monitor(uint32_t seq, char *cmd, int fd,
			    char *buf, ssize_t size)
{
	dvb_dev_stop_monitor(dvb);

	return send_data(fd, "%i%s%i", seq, cmd, 0);
}

static int dev_seek_by_sysname(uint32_t seq, char *cmd, int fd,
			       char *buf, ssize_t size)
{
	struct dvb_dev_list *dev;
	int adapter, num, type, ret;

	ret = scan_data(buf, size, "%i%i%i", &adapter, &num, &type);
	if (ret < 0)
		goto error;

	dev = dvb_dev_seek_by_sysname(dvb, adapter, num, type);
	if (!dev)
		goto error;

	return send_data(fd, "%i%s%i%s%s%s%i%s%s%s%s%s", seq, cmd, ret,
			 dev->syspath, dev->path, dev->sysname, dev->dvb_type,
			 dev->bus_addr, dev->bus_id, dev->manufacturer,
			 dev->product, dev->serial);
error:
	return send_data(fd, "%i%s%i", seq, cmd, ret);
}

static void *read_data(void *privdata)
{
	struct dvb_open_descriptor *open_dev;
	int timeout;
	int ret, read_ret = -1, fd, i;
	char databuf[REMOTE_BUF_SIZE];
	char buf[REMOTE_BUF_SIZE + 32], *p;
	size_t size;
	size_t count;
	struct pollfd __fds[NUM_FOPEN];
	nfds_t __numfds;

	timeout = 10; /* ms */
	while (1) {
		pthread_mutex_lock(&dvb_read_mutex);
		if (!numfds) {
			pthread_mutex_unlock(&dvb_read_mutex);
			break;
		}
		__numfds = numfds;
		memcpy(__fds, fds, sizeof(fds));
		pthread_mutex_unlock(&dvb_read_mutex);

		ret = poll(__fds, __numfds, timeout);
		if (!ret)
			continue;
		if (ret < 0) {
			err("poll");
			continue;
		}

		for (i = 0; i < __numfds; i++) {
			if (__fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
				continue;
			if (__fds[i].revents)
				break;
		}

		/*
		 * it means that one error condition happened.
		 * Likely the file was closed.
		 */
		if (i == __numfds)
			continue;

		fd = __fds[i].fd;

		if (!desc_root)
			break;

		open_dev = get_open_dev(fd);
		if (!open_dev) {
			err("Couldn't find opened file %d", fd);
			continue;
		}

		count = REMOTE_BUF_SIZE;
		read_ret = dvb_dev_read(open_dev, databuf, count);
		if (verbose) {
			if (read_ret < 0)
				dbg("#%d: read error: %d on %p", fd, read_ret, open_dev);
			else
				dbg("#%d: read %d bytes (count %d)", fd, read_ret, count);
		}

		/* Initialize to the start of the buffer */
		p = buf;
		size = sizeof(buf);

		ret = prepare_data(p, size, "%i%s%i%i", 0, "data_read",
				   read_ret, fd);
		if (ret < 0) {
			err("Failed to prepare answer to dvb_read()");
			break;
		}

		p += ret;
		size -= ret;

		if (read_ret > 0) {
			if (read_ret > size) {
				dbg("buffer to short to store read data!");
				read_ret = -EOVERFLOW;
			} else {
				memcpy(p, databuf, read_ret);
				p += read_ret;
			}
		}

		ret = send_buf(dvb_fd, buf, p - buf);
		if (ret < 0) {
			err("Error %d sending buffer\n", ret);
			if (ret == ECONNRESET) {
				close_all_devs();
				break;
			}
			continue;
		}
	}

	dbg("Finishing kthread");
	read_id = 0;
	return NULL;
}

static int dev_open(uint32_t seq, char *cmd, int fd, char *buf, ssize_t size)
{
	struct dvb_open_descriptor *open_dev;
	struct dvb_dev_list *dev;
	struct dvb_descriptors *desc, **p;
	int ret, flags, uid;
	char sysname[REMOTE_BUF_SIZE];

	desc = calloc(1, sizeof(*desc));
	if (!desc) {
		local_perror("calloc");
		ret = -ENOMEM;
		goto error;
	}

	ret = scan_data(buf, size, "%s%i",  sysname, &flags);
	if (ret < 0) {
		free(desc);
		goto error;
	}

	/*
	 * Discard requests for O_NONBLOCK, as the daemon will use threads
	 * to handle unblocked reads.
	 */
	flags &= ~O_NONBLOCK;

	open_dev = dvb_dev_open(dvb, sysname, flags);
	if (!open_dev) {
		ret = -errno;
		free(desc);
		goto error;
	}


	if (verbose)
		dbg("open dev handler for %s: %p with uid#%d", sysname, open_dev, open_dev->fd);

	dev = open_dev->dev;
	if (dev->dvb_type == DVB_DEVICE_DEMUX ||
	    dev->dvb_type == DVB_DEVICE_DVR) {
		pthread_mutex_lock(&dvb_read_mutex);
		fds[numfds].fd = open_dev->fd;
		fds[numfds].events = POLLIN | POLLPRI;
		numfds++;
		pthread_mutex_unlock(&dvb_read_mutex);
	}

	if (!read_id) {
		ret = pthread_create(&read_id, NULL, read_data, NULL);
		if (ret < 0) {
			local_perror("pthread_create");
			pthread_mutex_unlock(&msg_mutex);
			free(desc);
			return -1;
		}
	}

	pthread_mutex_unlock(&msg_mutex);
	uid = open_dev->fd;

	desc->uid = uid;
	desc->open_dev = open_dev;

	/* Add element to the desc_root tree */
	p = tsearch(desc, &desc_root, dvb_desc_compare);
	if (!p) {
		local_perror("tsearch");
		uid = 0;
	}
	if (*p != desc) {
		err("uid %d was already opened!");
	}

	pthread_mutex_unlock(&msg_mutex);

	ret = uid;
error:
	return send_data(fd, "%i%s%i", seq, cmd, ret);
}

static int dev_close(uint32_t seq, char *cmd, int fd, char *buf, ssize_t size)
{
	struct dvb_open_descriptor *open_dev;
	int uid, ret, i;

	ret = scan_data(buf, size, "%i",  &uid);
	if (ret < 0)
		goto error;

	open_dev = get_open_dev(uid);
	if (!open_dev) {
		err("Can't find uid to close");
		ret = -1;
		goto error;
	}

	/* Delete fd from the opened array */
	pthread_mutex_lock(&dvb_read_mutex);
	for (i = 0; i < numfds; i++) {
		if (fds[i].fd != open_dev->fd)
		    continue;
		if (i < numfds - 1)
			memmove(&fds[i], &fds[i + 1],
				sizeof(*fds)*(numfds - i - 1));
		numfds--;
		break;
	}
	pthread_mutex_unlock(&dvb_read_mutex);
	if (read_id && !numfds) {
		pthread_cancel(read_id);
		read_id = 0;
	}

	dvb_dev_close(open_dev);
	destroy_open_dev(uid);

error:
	return send_data(fd, "%i%s%i", seq, cmd, ret);
}

static int dev_dmx_stop(uint32_t seq, char *cmd, int fd,
			char *buf, ssize_t size)
{
	struct dvb_open_descriptor *open_dev;
	int uid, ret;

	ret = scan_data(buf, size, "%i",  &uid);
	if (ret < 0)
		goto error;

	open_dev = get_open_dev(uid);
	if (!open_dev) {
		ret = -1;
		err("Can't find uid to stop");
		goto error;
	}

	dvb_dev_dmx_stop(open_dev);

error:
	return send_data(fd, "%i%s%i", seq, cmd, ret);
}

static int dev_set_bufsize(uint32_t seq, char *cmd, int fd,
			   char *buf, ssize_t size)
{
	struct dvb_open_descriptor *open_dev;
	int uid, ret, bufsize;

	ret = scan_data(buf, size, "%i%i",  &uid, &bufsize);
	if (ret < 0)
		goto error;

	open_dev = get_open_dev(uid);
	if (!open_dev) {
		ret = -1;
		err("Can't find uid to stop");
		goto error;
	}

	dvb_dev_set_bufsize(open_dev, bufsize);

error:
	return send_data(fd, "%i%s%i", seq, cmd, ret);
}

static int dev_dmx_set_pesfilter(uint32_t seq, char *cmd, int fd,
				 char *buf, ssize_t size)
{
	struct dvb_open_descriptor *open_dev;
	int uid, ret, pid, type, output, bufsize;

	ret = scan_data(buf, size, "%i%i%i%i%i",
			&uid, &pid, &type, &output, &bufsize);
	if (ret < 0)
		goto error;

	open_dev = get_open_dev(uid);
	if (!open_dev) {
		ret = -1;
		err("Can't find uid to set pesfilter");
		goto error;
	}

	ret = dvb_dev_dmx_set_pesfilter(open_dev, pid, type, output, bufsize);

error:
	return send_data(fd, "%i%s%i", seq, cmd, ret);
}

static int dev_dmx_set_section_filter(uint32_t seq, char *cmd, int fd,
				      char *buf, ssize_t size)
{
	struct dvb_open_descriptor *open_dev;
	int uid, ret, pid, filtsize, flags;
	unsigned char filter[17], mask[17], mode[17];

	ret = scan_data(buf, size, "%i%i%i%s%s%s%i",
			&uid, &pid, &filtsize, filter, mask, mode, &flags);
	if (ret < 0)
		goto error;

	open_dev = get_open_dev(uid);
	if (!open_dev) {
		ret = -1;
		err("Can't find uid to set section filter");
		goto error;
	}

	ret = dvb_dev_dmx_set_section_filter(open_dev, pid, filtsize, filter,
					     mask, mode, flags);

error:
	return send_data(fd, "%i%s%i", seq, cmd, ret);
}

static int dev_dmx_get_pmt_pid(uint32_t seq, char *cmd, int fd,
			       char *buf, ssize_t size)
{
	struct dvb_open_descriptor *open_dev;
	int uid, ret, sid;

	ret = scan_data(buf, size, "%i%i",  &uid, &sid);
	if (ret < 0)
		goto error;

	open_dev = get_open_dev(uid);
	if (!open_dev) {
		ret = -1;
		err("Can't find uid to get PMT PID");
		goto error;
	}

	ret = dvb_dev_dmx_get_pmt_pid(open_dev, sid);

error:
	return send_data(fd, "%i%s%i", seq, cmd, ret);
}

static int dev_scan(uint32_t seq, char *cmd, int fd, char *buf, ssize_t size)
{
	int ret = -1;

	/*
	 * FIXME: There are too many stuff here! Maybe we should
	 * split this into smaller per-table calls.
	 */

#if 0
	struct dvb_open_descriptor *open_dev;
	int uid;

	ret = scan_data(buf, size, "%i%i",  &uid);
	if (ret < 0)
		goto error;

	open_dev = get_open_dev(uid);
	if (!open_dev) {
		ret = -1;
		err("Can't find uid to scan");
		goto error;
	}

	ret = dvb_scan(foo);

error:
	return send_data(fd, "%i%s%i", seq, cmd, ret);
#else
	return send_data(fd, "%i%s%i", seq, cmd, ret);
#endif
}

static int dev_set_sys(uint32_t seq, char *cmd, int fd,
		       char *buf, ssize_t size)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->fe_parms;
	struct dvb_v5_fe_parms *p = (void *)parms;
	int sys = 0, ret;

	ret = scan_data(buf, size, "%i", &sys);
	if (ret < 0)
		goto error;

	ret = __dvb_set_sys(p, sys);
error:
	return send_data(fd, "%i%s%i", seq, cmd, ret);
}

static int dev_get_parms(uint32_t seq, char *cmd, int fd,
			 char *inbuf, ssize_t insize)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->fe_parms;
	struct dvb_v5_fe_parms *par = (void *)parms;
	struct dvb_frontend_info *info = &par->info;
	int ret, i;
	char buf[REMOTE_BUF_SIZE], lnb_name[80] = "", *p = buf;
	size_t size = sizeof(buf);

	if (verbose)
		dbg("dev_get_parms called");

	ret = __dvb_fe_get_parms(par);
	if (ret < 0)
		goto error;

	/* Send first the public params */

	ret = prepare_data(p, size, "%i%s%i%s%i%i%i%i%i%i%i", seq, cmd, ret,
			   info->name, info->frequency_min,
			   info->frequency_max, info->frequency_stepsize,
			   info->frequency_tolerance, info->symbol_rate_min,
			   info->symbol_rate_max, info->symbol_rate_tolerance);
	if (ret < 0)
		goto error;

	p += ret;
	size -= ret;

	if (par->lnb)
		strcpy(lnb_name, par->lnb->name);

	ret = prepare_data(p, size, "%i%i%i%i%i%i%i%s%i%i%i%i%s%s",
			   par->version, par->has_v5_stats, par->current_sys,
			   par->num_systems, par->legacy_fe, par->abort,
		           par->lna, lnb_name,
			   par->sat_number, par->freq_bpf, par->diseqc_wait,
			   par->verbose, par->default_charset,
			   par->output_charset);
	if (ret < 0)
		goto error;

	p += ret;
	size -= ret;

	for (i = 0; i < MAX_DELIVERY_SYSTEMS; i++) {
		ret = prepare_data(p, size, "%i", par->systems[i]);
		if (ret < 0)
			goto error;

		p += ret;
		size -= ret;
	}

	/* Now, send the private ones - except for stats */

	ret = prepare_data(p, size, "%i%i%i%i",
			   parms->n_props,
			   parms->country,
			   parms->high_band,
			   parms->freq_offset);
	if (ret < 0)
		goto error;

	p += ret;
	size -= ret;

	for (i = 0; i < parms->n_props; i++) {
		ret = prepare_data(p, size, "%i%i",
				   parms->dvb_prop[i].cmd,
				   parms->dvb_prop[i].u.data);
		if (ret < 0)
			goto error;

		p += ret;
		size -= ret;
	}

	strcpy(output_charset, par->output_charset);
	strcpy(default_charset, par->default_charset);

	return send_buf(fd, buf, p - buf);
error:
	return send_data(fd, "%i%s%i", seq, cmd, ret);
}

static int dev_set_parms(uint32_t seq, char *cmd, int fd,
			 char *buf, ssize_t size)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->fe_parms;
	struct dvb_v5_fe_parms *par = (void *)parms;
	int ret, i;
	char *p = buf;
	const char *old_lnb = "";
	char new_lnb[256];

	if (verbose)
		dbg("dev_set_parms called");

	/* first the public params that aren't read only */

	/* Get current LNB name */
	if (par->lnb)
		old_lnb = par->lnb->name;

	ret = scan_data(p, size, "%i%i%s%i%i%i%i%s%s",
			&par->abort, &par->lna, new_lnb,
			&par->sat_number, &par->freq_bpf, &par->diseqc_wait,
			&par->verbose, default_charset, output_charset);

	if (ret < 0)
		goto error;

	p += ret;
	size -= ret;

	/* Now, the private ones */

	ret = scan_data(p, size, "%i", &i);
	if (ret < 0)
		goto error;
	parms->country = i;

	p += ret;
	size -= ret;

	for (i = 0; i < parms->n_props; i++) {
		ret = scan_data(p, size, "%i%i",
				&parms->dvb_prop[i].cmd,
				&parms->dvb_prop[i].u.data);
		if (ret < 0)
			goto error;

		p += ret;
		size -= ret;
	}

	if (!*new_lnb) {
		par->lnb = NULL;
	} else if (strcmp(old_lnb, new_lnb)) {
		int lnb = dvb_sat_search_lnb(new_lnb);

		if (lnb < 0) {
			dvb_logerr("Invalid lnb: %s", new_lnb);
			ret = -1;
			goto error;
		}

		par->lnb = dvb_sat_get_lnb(lnb);
	}

	par->output_charset = output_charset;
	par->default_charset = default_charset;

	ret = __dvb_fe_set_parms(par);

error:
	return send_data(fd, "%i%s%i", seq, cmd, ret);
}

static int dev_get_stats(uint32_t seq, char *cmd, int fd,
			 char *inbuf, ssize_t insize)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->fe_parms;
	struct dvb_v5_stats *st = &parms->stats;
	struct dvb_v5_fe_parms *par = (void *)parms;
	int ret, i;
	char buf[REMOTE_BUF_SIZE], *p = buf;
	size_t size = sizeof(buf);

	if (verbose)
		dbg("dev_get_stats called");

	ret = __dvb_fe_get_stats(par);
	if (ret < 0)
		goto error;

	ret = prepare_data(p, size, "%i%s%i%i", seq, cmd, ret, st->prev_status);
	if (ret < 0)
		goto error;

	p += ret;
	size -= ret;

	for (i = 0; i < DTV_NUM_STATS_PROPS; i++) {
		ret = prepare_data(p, size, "%i%i",
				   st->prop[i].cmd,
				   st->prop[i].u.data);
		if (ret < 0)
			goto error;

		p += ret;
		size -= ret;
	}
	for (i = 0; i < MAX_DTV_STATS; i++) {
		struct dvb_v5_counters *prev = st->prev;
		struct dvb_v5_counters *cur = st->cur;

		ret = prepare_data(p, size, "%i%i%i",
				   st->has_post_ber[i],
				   st->has_pre_ber[i],
				   st->has_per[i]);
		if (ret < 0)
			goto error;

		p += ret;
		size -= ret;

		ret = prepare_data(p, size,
				   "%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu",
				   prev->pre_bit_count,
				   prev->pre_bit_error,
				   prev->post_bit_count,
				   prev->post_bit_error,
				   prev->block_count,
				   prev->block_error,
				   cur->pre_bit_count,
				   cur->pre_bit_error,
				   cur->post_bit_count,
				   cur->post_bit_error,
				   cur->block_count,
				   cur->block_error);

		if (ret < 0)
			goto error;

		p += ret;
		size -= ret;
	}

	return send_buf(fd, buf, p - buf);
error:
	return send_data(fd, "%i%s%i", seq, cmd, ret);
}

/*
 * Structure with all methods with RPC calls
 */

typedef int (*method_handler) (uint32_t seq, char *cmd, int fd,
			       char *buf, ssize_t size);

struct method_types {
	char *name;
	method_handler handler;
	int locks_dvb;
};

static const struct method_types const methods[] = {
	{"daemon_get_version", &daemon_get_version, 1},
	{"dev_find", &dev_find, 0},
	{"dev_stop_monitor", &dev_stop_monitor, 0},
	{"dev_seek_by_sysname", &dev_seek_by_sysname, 0},
	{"dev_open", &dev_open, 0},
	{"dev_close", &dev_close, 0},
	{"dev_dmx_stop", &dev_dmx_stop, 0},
	{"dev_set_bufsize", &dev_set_bufsize, 0},
	{"dev_dmx_set_pesfilter", &dev_dmx_set_pesfilter, 0},
	{"dev_dmx_set_section_filter", &dev_dmx_set_section_filter, 0},
	{"dev_dmx_get_pmt_pid", &dev_dmx_get_pmt_pid, 0},

	{"dev_scan", &dev_scan, 0},

	{"dev_set_sys", &dev_set_sys, 0},
	{"fe_get_parms", &dev_get_parms, 0},
	{"fe_set_parms", &dev_set_parms, 0},
	{"fe_get_stats", &dev_get_stats, 0},

	{}
};

static void *start_server(void *fd_pointer)
{
	const struct method_types *method;
	int fd = *(int *)fd_pointer, ret, flag = 1;
	char buf[REMOTE_BUF_SIZE + 8], cmd[80], *p;
	ssize_t size;
	uint32_t seq;
	int bufsize;

	if (verbose)
		dbg("Opening socket %d", fd);

	/* Set a large buffer for read() to work better */
	bufsize = REMOTE_BUF_SIZE;
	setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
		   (void *)&bufsize, (int)sizeof(bufsize));

	/* Disable Naggle algorithm, as we want errors to be sent ASAP */
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

	/* Command dispatcher */
	do {
		size = recv(fd, buf, 4, MSG_WAITALL);
		if (size <= 0)
			break;
		size = be32toh(*(int32_t *)buf);
		size = recv(fd, buf, size, MSG_WAITALL);
		if (size <= 0)
			break;

		ret = scan_data(buf, size, "%i%s",  &seq, cmd);
		if (ret < 0) {
			if (verbose)
				dbg("message too short: %d", size);
			send_data(fd, "%i%s%i%s", 0, "log", LOG_ERR,
				  "msg too short");
			continue;
		}

		p = buf + ret;
		size -= ret;

		if (verbose)
			dbg("received command: %i '%s'", seq, cmd);

		if (size > buf + sizeof(buf) - p) {
			if (verbose)
				dbg("data length too big: %d", size);
			send_data(fd, "%i%s%i%s", 0, "log", LOG_ERR,
				  "data length too big");
			continue;
		}

		method = methods;
		while (method->name) {
			if (!strcmp(cmd, method->name)) {
				if (dvb_fd > 0 || method->locks_dvb) {
					ret = method->handler(seq, cmd,
							      fd, p, size);
					if (ret < 0)
						break;
					if (method->locks_dvb)
						dvb_fd = fd;
					break;
				}
				send_data(fd, "%i%s%i%s", 0, "log", LOG_ERR,
					  "daemon busy");
				break;
			}
			method++;
		}
		if (!method->name) {
			if (verbose)
				dbg("invalid command: %s", cmd);
			send_data(fd, "%i%s%i%s", 0, "log", LOG_ERR,
				  "invalid command");
		}
	} while (1);

	if (verbose)
		dbg("Closing socket %d", fd);

	close(fd);
	if (read_id) {
		pthread_cancel(read_id);
		read_id = 0;
	}
	if (dvb_fd > 0)
		close_all_devs();

	return NULL;
}

/*
 * main program
 */

int main(int argc, char *argv[])
{
	int ret;
	int sockfd;
	socklen_t addrlen;
	struct sockaddr_in serv_addr, cli_addr;

#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);
#endif

	if (argp_parse(&argp, argc, argv, ARGP_NO_HELP | ARGP_NO_EXIT, 0, 0)) {
		argp_help(&argp, stderr, ARGP_HELP_SHORT_USAGE, PROGRAM_NAME);
		return -1;
	}

	if (!port) {
		argp_help(&argp, stderr, ARGP_HELP_SHORT_USAGE, PROGRAM_NAME);
		return -1;
	}

	/* Allocate DVB structure and start seek for devices */
	dvb = dvb_dev_alloc();
	if (!dvb) {
		err("Can't allocate DVB data\n");
		return -1;
	}

	dvb_dev_find(dvb, 0);

	/* Create a socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		local_perror("socket");
		goto error;
	}

	/* Initialize listen address struct */
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	/* Bind to the address */
	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (ret < 0) {
		local_perror("bind");
		goto error;
	}

	/* FIXME: should allow the caller to set the verbosity */
	dvb_dev_set_log(dvb, 1, dvb_remote_log);

	/* Listen up to 5 connections */
	listen(sockfd, 5);
	addrlen = sizeof(cli_addr);

	start_signal_handler();
	pthread_mutex_init(&msg_mutex, NULL);
	pthread_mutex_init(&dvb_read_mutex, NULL);

	/* Accept actual connection from the client */

	warn("Support for Digital TV remote access is still highly experimental.\n"
	     "\nKnown issues:\n"
	     "  - Abort needed to be implemented in a proper way;\n"
	     "  - Need to make more stuff opaque, as touching on fields at the local\n"
	     "    end is not automatically reflected remotely;\n"
	     "  - The libdvbv5 API support is incomplete: it misses satellite, scan\n"
	     "    and other functions;\n\n");

	info(PROGRAM_NAME" started.");

	while (1) {
		int fd;
		pthread_t id;

		if (verbose)
			dbg("waiting for connections");
		fd = accept(sockfd, (struct sockaddr *)&cli_addr, &addrlen);
		if (fd < 0) {
			local_perror("accept");
			break;
		}

		if (verbose)
			dbg("accepted connection %d", fd);
		ret = pthread_create(&id, NULL, start_server, (void *)&fd);
		if (ret < 0) {
			local_perror("pthread_create");
			break;
		}
	}

	/* Just in case we add some way for the remote part to stop the daemon */
	stop_signal_handler();

error:
	info(PROGRAM_NAME" stopped.");

	pthread_exit(NULL);

	if (dvb)
		dvb_dev_free(dvb);

	return -1;
}
