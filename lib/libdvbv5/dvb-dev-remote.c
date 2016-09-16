/*
 * Copyright (c) 2016 - Mauro Carvalho Chehab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 */

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1
#define _LARGEFILE64_SOURCE 1

#include <config.h>


#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include <ctype.h>
#include <inttypes.h>
#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <pthread.h>
#include <unistd.h>
#include <resolv.h>
#include <string.h>
#include <sys/socket.h>

#include "dvb-fe-priv.h"
#include "dvb-dev-priv.h"

#ifdef ENABLE_NLS
# include "gettext.h"
# include <libintl.h>
# define _(string) dgettext(LIBDVBV5_DOMAIN, string)
#else
# define _(string) string
#endif


/*
 * Expected server version
 *
 * Please notice that the daemon and the library should match the version,
 * as, right now, we don't warrant that the protocol between the server
 * and the client won't change on newer versions. So, both the daemon and
 * the client should have the same version.
 */
const char *daemon_version = "dvbv5-daemon version " V4L_UTILS_VERSION;

/*
 * Internal data structures
 */

#define RINGBUF_SIZE (REMOTE_BUF_SIZE * 32)

struct ringbuffer {
	/* Should be the first member of struct */
	struct dvb_open_descriptor open_dev;

	/* ringbuffer handling */
	int rc;
	ssize_t read, write;
	char buf[RINGBUF_SIZE];
	pthread_mutex_t lock;
};

struct queued_msg {
	int seq;
	char cmd[80];
	int retval;

	pthread_mutex_t lock;
	pthread_cond_t cond;

	char args[REMOTE_BUF_SIZE];
	ssize_t args_size;

	struct queued_msg *next;
};

struct dvb_dev_remote_priv {
	int fd;
	struct sockaddr_in addr;

	int seq, disconnected;

	dvb_dev_change_t notify_dev_change;

	pthread_t recv_id;
	pthread_mutex_t lock_io;

	char output_charset[256];
	char default_charset[256];

	struct queued_msg msgs;
};

void stack_dump(struct dvb_v5_fe_parms_priv *parms)
{
#ifdef HAVE_BACKTRACE
	int i, nptrs = 0;
	void *buffer[10];
	char **strings = NULL;

	nptrs = backtrace(buffer, sizeof(buffer));

	if (nptrs) {
		strings = backtrace_symbols(buffer, nptrs);
		dvb_logdbg("Stack:");
	}

	for (i = 0; i < nptrs; i++)
		dvb_logdbg("   %s", strings[i]);

	free(strings);
#endif
}

/*
 * Functions to send/receive messages to the server
 */

static ssize_t __prepare_data(struct dvb_v5_fe_parms_priv *parms,
			      char *buf, const size_t size,
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
			len = strlen(s);
			if (p + len + 4 > endp) {
				dvb_logdbg("buffer too short for string: pos: %zd, len:%d, buffer size:%zd",
					   p - buf, len, sizeof(buf));
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
				dvb_logdbg("buffer to short for int32_t");
				return -1;
			}

			i32 = htobe32(va_arg(ap, int32_t));
			memcpy(p, &i32, 4);
			p += 4;
			break;
		case 'l':              /* 64-bit unsigned int */
			if (*fmt++ != 'u') {
				dvb_logdbg("invalid long format character: '%c'", *fmt);
				break;
			}
			if (p + 8 > endp) {
				dvb_logdbg("buffer to short for uint64_t");
				return -1;
			}
			u64 = htobe64(va_arg(ap, uint64_t));
			memcpy(p, &u64, 8);
			p += 8;
			break;
		default:
			dvb_logdbg("invalid format character: '%c'", *fmt);
		}
		while (*fmt && *fmt != '%') fmt++;
		if (*fmt == '%') fmt++;
	}
	return p - buf;
}

static ssize_t prepare_data(struct dvb_v5_fe_parms_priv *parms,
			    char *buf, const size_t size,
			      const char *fmt, ...)
	__attribute__ (( format( printf, 4, 5 )));

static ssize_t prepare_data(struct dvb_v5_fe_parms_priv *parms,
			    char *buf, const size_t size,
			    const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = __prepare_data(parms, buf, size, fmt, ap);
	va_end(ap);

	return ret;
}

static struct queued_msg *send_fmt(struct dvb_device_priv *dvb, int fd,
				   const char *cmd, const char *fmt, ...)
	__attribute__ (( format( printf, 4, 5 )));

static struct queued_msg *send_fmt(struct dvb_device_priv *dvb, int fd,
				   const char *cmd, const char *fmt, ...)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct queued_msg *msg, *msgs;
	char buf[REMOTE_BUF_SIZE], *p = buf, *endp = &buf[sizeof(buf)];
	int ret, len, err;
	int32_t i32;
	va_list ap;

	msg = calloc(1, sizeof(*msg));
	if (!msg) {
		dvb_logerr("calloc queued_msg");
		stack_dump(parms);
		return NULL;
	}

	pthread_mutex_init(&msg->lock, NULL);
	pthread_cond_init(&msg->cond, NULL);
	strcpy(msg->cmd, cmd);

	pthread_mutex_lock(&priv->lock_io);
	msg->seq = ++priv->seq;

	/* Encode sequence number */
	i32 = htobe32(msg->seq);
	if (p + 4 > endp) {
		dvb_logdbg("buffer to short for int32_t");
		stack_dump(parms);
		pthread_mutex_unlock(&priv->lock_io);
		return NULL;
	}
	memcpy(p, &i32, 4);
	p += 4;

	/* Encode command */
	len = strlen(cmd);
	if (p + len + 4 > endp) {
		dvb_logdbg("buffer too short for command: pos: %zd, len:%d, buffer size:%zd",
				p - buf, len, sizeof(buf));
		stack_dump(parms);
		pthread_mutex_unlock(&priv->lock_io);
		return NULL;
	}
	i32 = htobe32(len);
	memcpy(p, &i32, 4);
	p += 4;
	memcpy(p, cmd, len);
	p += len;

	/* Encode other parameters */

	va_start(ap, fmt);
	ret = __prepare_data(parms, p, endp - p, fmt, ap);
	va_end(ap);

	if (ret < 0) {
		pthread_mutex_unlock(&priv->lock_io);
		return NULL;
	}

	p += ret;

	pthread_mutex_lock(&msg->lock);
	i32 = htobe32(p - buf);
	ret = send(fd, (void *)&i32, 4, MSG_MORE);
	if (ret != 4) {
		err = 1;
	} else {
		err = 0;
		ret = write(fd, buf, p - buf);
	}
	if (ret < 0 || (ret < p - buf) || err) {
		pthread_mutex_destroy(&msg->lock);
		pthread_cond_destroy(&msg->cond);
		free(msg);
		msg = NULL;
		if (ret < 0)
			dvb_perror("write");
		else
			dvb_logerr("incomplete send");
		stack_dump(parms);
	} else {
		/* Add it to the message queue */
		for (msgs = &priv->msgs; msgs->next; msgs = msgs->next);
		msgs->next = msg;
	}
	pthread_mutex_unlock(&priv->lock_io);

	return msg;
}

static struct queued_msg *send_buf(struct dvb_device_priv *dvb, int fd,
				   const char *cmd,
				   const char *in_buf, const size_t in_size)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct queued_msg *msg, *msgs;
	char buf[REMOTE_BUF_SIZE], *p = buf, *endp = &buf[sizeof(buf)];
	int ret, len, err;
	int32_t i32;

	msg = calloc(1, sizeof(*msg));
	if (!msg) {
		dvb_logerr("calloc queued_msg");
		stack_dump(parms);
		return NULL;
	}

	pthread_mutex_init(&msg->lock, NULL);
	pthread_cond_init(&msg->cond, NULL);
	strcpy(msg->cmd, cmd);

	pthread_mutex_lock(&priv->lock_io);
	msg->seq = ++priv->seq;

	/* Encode sequence number */
	i32 = htobe32(msg->seq);
	if (p + 4 > endp) {
		dvb_logdbg("buffer to short for int32_t");
		stack_dump(parms);
		pthread_mutex_unlock(&priv->lock_io);
		return NULL;
	}
	memcpy(p, &i32, 4);
	p += 4;

	/* Encode command */
	len = strlen(cmd);
	if (p + len + 4 > endp) {
		dvb_logdbg("buffer too short for command: pos: %zd, len:%d, buffer size:%zd",
				p - buf, len, sizeof(buf));
		stack_dump(parms);
		pthread_mutex_unlock(&priv->lock_io);
		return NULL;
	}
	i32 = htobe32(len);
	memcpy(p, &i32, 4);
	p += 4;
	memcpy(p, cmd, len);
	p += len;

	/* Copy buffer contents */
	if (in_size >= p - buf + REMOTE_BUF_SIZE) {
		dvb_logdbg("buffer to big!");
		stack_dump(parms);
		pthread_mutex_unlock(&priv->lock_io);
		return NULL;
	}

	memcpy(p, in_buf, in_size);
	p += in_size;

	i32 = htobe32(p - buf);
	ret = send(fd, (void *)&i32, 4, MSG_MORE);
	if (ret != 4) {
		err = 1;
	} else {
		err = 0;
		ret = write(fd, buf, p - buf);
	}
	if (ret < 0 || (ret < p - buf) || err) {
		pthread_mutex_destroy(&msg->lock);
		pthread_cond_destroy(&msg->cond);
		free(msg);
		msg = NULL;
		if (ret < 0)
			dvb_perror("write");
		else
			dvb_logerr("incomplete send");
		stack_dump(parms);
	} else {
		/* Add it to the message queue */
		for (msgs = &priv->msgs; msgs->next; msgs = msgs->next);
		msgs->next = msg;
	}
	pthread_mutex_unlock(&priv->lock_io);

	return msg;
}

static void free_msg(struct dvb_device_priv *dvb, struct queued_msg *msg)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct queued_msg *msgs;

	pthread_mutex_lock(&priv->lock_io);
	for (msgs = &priv->msgs; msgs; msgs = msgs->next) {
		if (msgs->next == msg) {
			msgs->next = msg->next;
			pthread_mutex_unlock(&priv->lock_io);

			pthread_cond_destroy(&msg->cond);
			pthread_mutex_destroy(&msg->lock);
			free(msg);
			return;
		}
	}
	pthread_mutex_unlock(&priv->lock_io);
	dvb_logerr("message for cmd %s not found at the message queue!", msg->cmd);
};

static ssize_t scan_data(struct dvb_v5_fe_parms_priv *parms, char *buf,
			 int buf_size, const char *fmt, ...)
	__attribute__ (( format( scanf, 4, 5 )));

static ssize_t scan_data(struct dvb_v5_fe_parms_priv *parms, char *buf,
			 int buf_size, const char *fmt, ...)
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
				dvb_logdbg("buffer to short for string length: pos: %zd, len:%d, buffer size:%d",
					   p - buf, 4, buf_size);
				stack_dump(parms);
				return -1;
			}
			len = be32toh(*(int32_t *)p);
			p += 4;
			if (p + len > endp) {
				dvb_logdbg("buffer too short for string: pos: %zd, len:%d, buffer size:%d",
					   p - buf, len, buf_size);
				stack_dump(parms);
				return -1;
			}

			memcpy(s, p, len);
			s[len] = '\0';
			p += len;
			break;
		case 'i':              /* 32-bit int */
			if (p + 4 > endp) {
				dvb_logdbg("buffer to short for int32_t");
				stack_dump(parms);
				return -1;
			}
			i32 = va_arg(ap, int32_t *);

			*i32 = be32toh(*(int32_t *)p);
			p += 4;
			break;
		case 'l':              /* 64-bit unsigned int */
			if (*fmt++ != 'u') {
				dvb_logdbg("invalid long format character: '%c'", *fmt);
				stack_dump(parms);
				break;
			}
			if (p + 8 > endp) {
				dvb_logdbg("buffer to short for uint64_t");
				stack_dump(parms);
				return -1;
			}
			u64 = va_arg(ap, uint64_t *);

			*u64 = be32toh(*(uint64_t *)p);
			p += 8;
			break;
		default:
			dvb_logdbg("invalid format character: '%c'", *fmt);
			stack_dump(parms);
		}
		while (*fmt && *fmt != '%') fmt++;
		if (*fmt == '%') fmt++;
	}
	va_end(ap);

	return p - buf;
}

static void dvb_dev_remote_disconnect(struct dvb_dev_remote_priv *priv)
{
	struct queued_msg *msg;

	priv->disconnected = 1;

	for (msg = &priv->msgs; msg; msg = msg->next) {
		msg->retval = -ENODEV;
		pthread_cond_signal(&msg->cond);
	}
	/* Close the socket */
	if (priv->fd > 0) {
		close(priv->fd);
		priv->fd = 0;
	}
}

static void write_ringbuffer(struct dvb_open_descriptor *open_dev,
			    ssize_t size, char *buf)
{
	struct ringbuffer *ringbuf = (struct ringbuffer *)open_dev;
	ssize_t len = size, split;

	pthread_mutex_lock(&ringbuf->lock);

	split = (ringbuf->write + size > RINGBUF_SIZE) ?
		 RINGBUF_SIZE - ringbuf->write : 0;

	if (split > 0) {
		memcpy(&ringbuf->buf[ringbuf->write], buf, split);
		buf += split;
		len -= split;
		ringbuf->write = 0;
	}

	memcpy(&ringbuf->buf[ringbuf->write], buf, len);
	ringbuf->write = (ringbuf->write + len) % RINGBUF_SIZE;

	/* Detect buffer overflows */
	if ((unsigned)((ringbuf->write - ringbuf->read) % RINGBUF_SIZE) < size)
		ringbuf->rc = -EOVERFLOW;

	pthread_mutex_unlock(&ringbuf->lock);
}

static void read_ringbuffer(struct dvb_open_descriptor *open_dev,
			    size_t *len, char *buf)
{
	struct ringbuffer *ringbuf = (struct ringbuffer *)open_dev;
	ssize_t size, split;

	/* Sets the read size */
	if (*len > REMOTE_BUF_SIZE)
		*len = REMOTE_BUF_SIZE;

	/* Wait for data to arrive */
	pthread_mutex_lock(&ringbuf->lock);
	while ((unsigned)((ringbuf->write - ringbuf->read) % RINGBUF_SIZE) < *len) {
		pthread_mutex_unlock(&ringbuf->lock);
		usleep(1);
		pthread_mutex_lock(&ringbuf->lock);
	}

	size = *len;

	*len = 0;
	split = (ringbuf->read + size > RINGBUF_SIZE) ? RINGBUF_SIZE - ringbuf->read : 0;
	if (split > 0) {

		memcpy(buf, &ringbuf->buf[ringbuf->read], split);
		buf += split;
		size -= split;
		*len += split;
		ringbuf->read = 0;
	}
	memcpy(buf, &ringbuf->buf[ringbuf->read], size);
	*len += size;

	ringbuf->read = (ringbuf->read + size) % RINGBUF_SIZE;

	pthread_mutex_unlock(&ringbuf->lock);
}

static void log_hexdump(struct dvb_v5_fe_parms_priv *parms, int len,
			unsigned char *buf)
{
       char str[80], octet[10];
       int ofs, i, l;

	for (ofs = 0; ofs < len; ofs += 16) {
		sprintf(str, "%03d: ", ofs);

		for (i = 0; i < 16; i++) {
			if ((i + ofs) < len)
				sprintf(octet, "%02x ", (unsigned)buf[ofs + i]);
			else
				strcpy(octet, "   ");

			strcat(str, octet);
		}
		strcat( str,"  ");
		l = strlen(str);

		for (i = 0; (i < 16) && ((i + ofs) < len); i++)
			str[l++] = isprint( buf[ofs + i] ) ? buf[ofs + i] : '.';

		str[l] = '\0';
		dvb_logerr("%s", str);
	}
}

static void *receive_data(void *privdata)
{
	struct dvb_device_priv *dvb = privdata;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct queued_msg *msg;
	struct dvb_open_descriptor *cur;
	char buf[REMOTE_BUF_SIZE + 8], cmd[REMOTE_BUF_SIZE], *args;
	ssize_t size, args_size;
	int ret, retval, seq, handled, uid, found;

	do {
		size = recv(priv->fd, buf, 4, MSG_WAITALL);
		if (size < 4) {
			if (size < 0)
				dvb_perror("recv");
			else
				dvb_logerr("remote end disconnected");
			dvb_dev_remote_disconnect(priv);
			return NULL;
		}
		size = be32toh(*(int32_t *)buf);
		ret = recv(priv->fd, buf, size, MSG_WAITALL);
		if (ret != size) {
			if (size < 0)
				dvb_perror("recv");
			else
				dvb_logerr("remote end disconnected");
			dvb_dev_remote_disconnect(priv);
			return NULL;
		}

		args = buf;
		args_size = size;
		while (args_size > 0) {
			ret = scan_data(parms, args, args_size, "%i%s%i",
					&seq, cmd, &retval);
			if (ret < 0) {
				dvb_logerr("invalid protocol message with size %zd", args_size);
				log_hexdump(parms, args_size, (unsigned char *)args);
				break;
			}
			args += ret;
			args_size -= ret;

			/* Check for messages that aren't command responses */
			if (seq)
				break;

			if (!strcmp(cmd, "log")) {
				ret = scan_data(parms, args, args_size,
						"%s", cmd);
				if (ret > 0) {
					dvb_loglevel(retval, "%s", cmd);
					args += ret;
					args_size -= ret;
				}
			} else if (!strcmp(cmd, "dev_change")) {
				ret = scan_data(parms, args, args_size,
						"%s", cmd);
				/*
				 * FIXME: we should change the logic here to
				 * implement a function and always monitor
				 * changes on remote devices. This way, we
				 * can avoid leaking memory with the current
				 * implementation of dvb_remote_seek_by_sysname
				 */
				if (ret > 0) {
					if (priv->notify_dev_change)
						priv->notify_dev_change(strdup(cmd), retval);
					args += ret;
					args_size -= ret;
				}
			} else if (!strcmp(cmd, "data_read")) {
				ret = scan_data(parms, args, args_size,
						"%i", &uid);
				if (ret <= 0)
					continue;

				args += ret;
				args_size -= ret;

				found = 0;
				for (cur = dvb->open_list.next; cur; cur = cur->next) {
					if (cur->fd == uid) {
						struct ringbuffer *ringbuf = (struct ringbuffer *)cur;

						found = 1;
						if (retval < 0) {
							ringbuf->rc = retval;
							continue;
						}
						write_ringbuffer(cur, args_size, args);
					}
				}
				/* FIXME: should we abort here? */
				if (!found)
					dvb_logerr("received data for unknown ID %d", uid);
				args += args_size;
				args_size = 0;
			} else {
				dvb_logerr("unexpected message type: %s", cmd);
				ret = -1;
				break;
			}
		}
		if (ret <= 0 || !seq)
			continue;

		/* Handle command responses */
		pthread_mutex_lock(&priv->lock_io);

		handled = 0;
		for (msg = &priv->msgs; msg; msg = msg->next) {
			if (seq != msg->seq)
				continue;

			handled = 1;

			if (strcmp(msg->cmd, cmd)) {
				dvb_logerr("msg #%d: Expecting '%s', got '%s'",
						seq, msg->cmd, cmd);
				free_msg(dvb, msg);
				break;
			}
			memcpy(msg->args, args, args_size);
			msg->args_size = args_size;
			msg->retval = retval;
			pthread_mutex_unlock(&priv->lock_io);
			pthread_mutex_lock(&msg->lock);
			ret = pthread_cond_signal(&msg->cond);
			pthread_mutex_unlock(&msg->lock);

			if (ret < 0)
				dvb_perror("pthread_cond_signal");
			break;
		}
		if (handled)
			continue;

		pthread_mutex_unlock(&priv->lock_io);
		dvb_logerr("unexpected command response: %s", cmd);
	} while (1);
}

/*
 * Function handlers
 */
static int dvb_remote_get_version(struct dvb_device_priv *dvb)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct queued_msg *msg;
	char version[REMOTE_BUF_SIZE];
	int ret;

	if (priv->disconnected)
		return -ENODEV;

	msg = send_fmt(dvb, priv->fd, "daemon_get_version", "-");
	if (!msg)
		return -1;

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		goto error;
	}

	if (msg->retval < 0)
		goto error;

	ret = scan_data(parms, msg->args, msg->args_size, "%s", version);
	if (ret < 0) {
		dvb_logerr("Can't get sever's version");
		goto error;
	}

	if (strcmp(version, daemon_version)) {
		dvb_logerr("Wrong version. Expecting '%s', received '%s'",
			daemon_version, version);
		ret = 0;
		goto error;
	}

	/* version matches */
	ret = 1;

error:
	msg->seq = 0; /* Avoids any risk of a recursive call */
	pthread_mutex_unlock(&msg->lock);

	free_msg(dvb, msg);
	return ret;
}

static int dvb_remote_find(struct dvb_device_priv *dvb,
			   dvb_dev_change_t handler)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct queued_msg *msg;
	int ret, enable_monitor;

	if (priv->disconnected)
		return -ENODEV;

	if (handler)
		enable_monitor = 1;
	else
		enable_monitor = 0;

	priv->notify_dev_change = handler;

	msg = send_fmt(dvb, priv->fd, "dev_find", "%i", enable_monitor);
	if (!msg) {
		priv->notify_dev_change = NULL;
		return -1;
	}

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		priv->notify_dev_change = NULL;
		goto error;
	}
	ret = msg->retval;

error:
	msg->seq = 0; /* Avoids any risk of a recursive call */
	pthread_mutex_unlock(&msg->lock);

	free_msg(dvb, msg);
	return ret;
}

static int dvb_remote_stop_monitor(struct dvb_device_priv *dvb)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct queued_msg *msg;
	int ret;

	if (priv->disconnected)
		return -ENODEV;

	msg = send_fmt(dvb, priv->fd, "dev_stop_monitor", "-");
	if (!msg)
		return -1;

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		goto error;
	}

	ret = msg->retval;

error:
	msg->seq = 0; /* Avoids any risk of a recursive call */
	pthread_mutex_unlock(&msg->lock);

	free_msg(dvb, msg);
	return ret;
}

struct dvb_dev_list *dvb_remote_seek_by_sysname(struct dvb_device_priv *dvb,
						unsigned int adapter,
						unsigned int num,
						enum dvb_dev_type type)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct dvb_dev_list *dev = NULL;
	struct queued_msg *msg;
	int ret, int_type;

	if (priv->disconnected)
		return NULL;

	msg = send_fmt(dvb, priv->fd, "dev_seek_by_sysname", "%i%i%i",
				adapter, num, type);
	if (!msg)
		return NULL;

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		goto error;
	}
	if (msg->retval < 0)
		goto error;

	/*
	 * FIXME: dev should be freed. The best would actually to implement
	 * this locally, but that would require device insert/removal
	 * notifications. So, let's postpone it.
	 */
	dev = calloc(1, sizeof(*dev));
	if (!dev)
		goto error;
	dev->syspath = malloc(msg->args_size);
	dev->path = malloc(msg->args_size);
	dev->sysname = malloc(msg->args_size);
	dev->bus_addr = malloc(msg->args_size);
	dev->bus_id = malloc(msg->args_size);
	dev->manufacturer = malloc(msg->args_size);
	dev->product = malloc(msg->args_size);
	dev->serial = malloc(msg->args_size);

	ret = scan_data(parms, msg->args, msg->args_size, "%s%s%s%i%s%s%s%s%s",
			dev->syspath, dev->path, dev->sysname,
			&int_type, dev->bus_addr, dev->bus_id,
			dev->manufacturer, dev->product, dev->serial);

	if (ret < 0) {
		dvb_logerr("Can't get return value");
		goto error;
	}
	if (!*dev->syspath) {
		free(dev);
		dev = NULL;
		goto error;
	}

	dev->dvb_type = int_type;

error:
	msg->seq = 0; /* Avoids any risk of a recursive call */
	pthread_mutex_unlock(&msg->lock);

	free_msg(dvb, msg);
	return dev;
}

int dvb_remote_fe_get_parms(struct dvb_v5_fe_parms *par);

static struct dvb_open_descriptor *dvb_remote_open(struct dvb_device_priv *dvb,
						   const char *sysname,
						   int flags)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct dvb_open_descriptor *open_dev, *cur;
	struct ringbuffer *ringbuf;
	struct queued_msg *msg;
	int ret;

	if (priv->disconnected)
		return NULL;

	ringbuf = calloc(1, sizeof(*ringbuf));
	if (!ringbuf) {
		dvb_perror("Can't create file descriptor");
		return NULL;
	}
	open_dev = &ringbuf->open_dev;

	msg = send_fmt(dvb, priv->fd, "dev_open", "%s%i", sysname, flags);
	if (!msg)
		return NULL;

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		goto error;
	}

	if (msg->retval < 0)
		goto error;

	/* Add the fd to the open descriptor's list */
	open_dev->fd = msg->retval;
	open_dev->dev = NULL;
	open_dev->dvb = dvb;

	/* Initialize ringbuffer data*/
	pthread_mutex_init(&ringbuf->lock, NULL);

	cur = &dvb->open_list;
	while (cur->next)
		cur = cur->next;
	cur->next = open_dev;

	/* Retrieve frontend initial parameters */
	if (strstr(sysname, "frontend"))
		dvb_remote_fe_get_parms(dvb->d.fe_parms);

	return open_dev;

error:
	msg->seq = 0; /* Avoids any risk of a recursive call */
	pthread_mutex_unlock(&msg->lock);

	free_msg(dvb, msg);
	return NULL;
}

static int dvb_remote_close(struct dvb_open_descriptor *open_dev)
{
	struct ringbuffer *ringbuffer = (struct ringbuffer *)open_dev;
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dvb_open_descriptor *cur;
	struct queued_msg *msg;
	int ret = -1;

	if (priv->disconnected)
		return -ENODEV;

	msg = send_fmt(dvb, priv->fd, "dev_close", "%i", open_dev->fd);
	/* Even with errors, we need to close our end */
	if (!msg)
		goto error;

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		goto error;
	}

	ret = msg->retval;

error:
	/*
	 * Even if it can't free the resources remotely, but it needs to
	 * free locally. If the error was due to a remote disconnect,
	 * the code at the dvbv5-daemon will free the remote resources anyway.
	 */
	for (cur = &dvb->open_list; cur->next; cur = cur->next) {
		if (cur->next == open_dev) {
			cur->next = open_dev->next;
			pthread_mutex_destroy(&ringbuffer->lock);
			free(ringbuffer);
			goto ret;
		}
	}

	/* Should never happen */
	dvb_logerr("Couldn't free device");
ret:
	if (msg) {
		msg->seq = 0; /* Avoids any risk of a recursive call */
		pthread_mutex_unlock(&msg->lock);

		free_msg(dvb, msg);
	}
	return ret;
}

static int dvb_remote_dmx_stop(struct dvb_open_descriptor *open_dev)
{
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct queued_msg *msg;
	int ret;

	if (priv->disconnected)
		return -ENODEV;

	msg = send_fmt(dvb, priv->fd, "dev_dmx_stop", "%i", open_dev->fd);
	if (!msg)
		return -1;

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		goto error;
	}

	ret = msg->retval;

error:
	msg->seq = 0; /* Avoids any risk of a recursive call */
	pthread_mutex_unlock(&msg->lock);

	free_msg(dvb, msg);
	return ret;
}

static int dvb_remote_set_bufsize(struct dvb_open_descriptor *open_dev,
			int bufsize)
{
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct queued_msg *msg;
	int ret;

	if (priv->disconnected)
		return -ENODEV;

	msg = send_fmt(dvb, priv->fd, "dev_set_bufsize", "%i%i",
		       open_dev->fd, bufsize);
	if (!msg)
		return -1;

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		goto error;
	}

	ret = msg->retval;

error:
	msg->seq = 0; /* Avoids any risk of a recursive call */
	pthread_mutex_unlock(&msg->lock);

	free_msg(dvb, msg);
	return ret;
}

static ssize_t dvb_remote_read(struct dvb_open_descriptor *open_dev,
		     void *buf, size_t count)
{
	struct ringbuffer *ringbuf = (struct ringbuffer *)open_dev;
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	int ret;

	if (priv->disconnected)
		return -ENODEV;

	if (ringbuf->rc) {
		ret = ringbuf->rc;
		ringbuf->rc = 0;
		return ret;
	}

	read_ringbuffer(open_dev, &count, buf);

	return count;
}

static int dvb_remote_dmx_set_pesfilter(struct dvb_open_descriptor *open_dev,
			      int pid, dmx_pes_type_t type,
			      dmx_output_t output, int bufsize)
{
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct queued_msg *msg;
	int ret;

	if (priv->disconnected)
		return -ENODEV;

	msg = send_fmt(dvb, priv->fd, "dev_dmx_set_pesfilter", "%i%i%i%i%i",
		       open_dev->fd, pid, type, output, bufsize);
	if (!msg)
		return -1;

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		goto error;
	}

	ret = msg->retval;

error:
	msg->seq = 0; /* Avoids any risk of a recursive call */
	pthread_mutex_unlock(&msg->lock);

	free_msg(dvb, msg);
	return ret;
}

static int dvb_remote_dmx_set_section_filter(struct dvb_open_descriptor *open_dev,
				   int pid, unsigned filtsize,
				   unsigned char *filter,
				   unsigned char *mask,
				   unsigned char *mode,
				   unsigned int flags)
{
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct queued_msg *msg;
	int ret;

	msg = send_fmt(dvb, priv->fd, "dmx_set_section_filter",
		       "%i%i%i%s%s%s%i", open_dev->fd, pid, filtsize,
		       filter, mask, mode, flags);
	if (!msg)
		return -1;

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		goto error;
	}

	ret = msg->retval;

error:
	msg->seq = 0; /* Avoids any risk of a recursive call */
	pthread_mutex_unlock(&msg->lock);

	free_msg(dvb, msg);
	return ret;
}

static int dvb_remote_dmx_get_pmt_pid(struct dvb_open_descriptor *open_dev, int sid)
{
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct queued_msg *msg;
	int ret;

	if (priv->disconnected)
		return -ENODEV;

	msg = send_fmt(dvb, priv->fd, "dev_set_bufsize", "%i%i",
		       open_dev->fd, sid);
	if (!msg)
		return -1;

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		goto error;
	}

	ret = msg->retval;

error:
	msg->seq = 0; /* Avoids any risk of a recursive call */
	pthread_mutex_unlock(&msg->lock);

	free_msg(dvb, msg);
	return ret;
}

int dvb_remote_fe_set_sys(struct dvb_v5_fe_parms *p, fe_delivery_system_t sys)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)p;
	struct dvb_device_priv *dvb = parms->dvb;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct queued_msg *msg;
	int ret;

	if (priv->disconnected)
		return -ENODEV;

	msg = send_fmt(dvb, priv->fd, "dev_set_sys", "%i", sys);
	if (!msg)
		return -1;

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		goto error;
	}

	ret = msg->retval;

error:
	msg->seq = 0; /* Avoids any risk of a recursive call */
	pthread_mutex_unlock(&msg->lock);

	free_msg(dvb, msg);
	return ret;
}

int dvb_remote_fe_get_parms(struct dvb_v5_fe_parms *par)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)par;
	struct dvb_frontend_info *info = &par->info;
	struct dvb_device_priv *dvb = parms->dvb;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct queued_msg *msg;
	int i, ret, delsys, country;
	char *p, lnb_name[256];
	size_t size;

	if (priv->disconnected)
		return -ENODEV;

	msg = send_fmt(dvb, priv->fd, "fe_get_parms", "-");
	if (!msg)
		return -1;

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		goto error;
	}

	if (msg->retval < 0) {
		ret = msg->retval;
		goto error;
	}

	p = msg->args;
	size = msg->args_size;

	/* Get first the public params */

	ret = scan_data(parms, p, size, "%s%i%i%i%i%i%i%i",
			info->name, &info->frequency_min,
			&info->frequency_max, &info->frequency_stepsize,
			&info->frequency_tolerance, &info->symbol_rate_min,
			&info->symbol_rate_max, &info->symbol_rate_tolerance);
	if (ret < 0)
		goto error;

	p += ret;
	size -= ret;

	ret = scan_data(parms, p, size, "%i%i%i%i%i%i%i%s%i%i%i%i%s%s",
			&par->version, &par->has_v5_stats, &delsys,
			&par->num_systems, &par->legacy_fe, &par->abort,
		        &par->lna, lnb_name,
			&par->sat_number, &par->freq_bpf, &par->diseqc_wait,
			&par->verbose, priv->default_charset,
			priv->output_charset);
	if (ret < 0)
		goto error;

	par->current_sys = delsys;

	if (*lnb_name) {
		int lnb = dvb_sat_search_lnb(lnb_name);

		if (lnb >= 0) {
			par->lnb = dvb_sat_get_lnb(lnb);
		} else {
			dvb_logerr("Invalid LNBf: %s", lnb_name);
			par->lnb = NULL;
		}
	}

	p += ret;
	size -= ret;

	for (i = 0; i < MAX_DELIVERY_SYSTEMS; i++) {

		ret = scan_data(parms, p, size, "%i", &delsys);
		if (ret < 0)
			goto error;

		par->systems[i] = delsys;

		p += ret;
		size -= ret;
	}

	/* Now, get the private ones - except for stats */

	ret = scan_data(parms, p, size, "%i%i%i%i",
			&parms->n_props, &country,
		        &parms->high_band, &parms->freq_offset);
	if (ret < 0)
		goto error;

	parms->country = country;

	p += ret;
	size -= ret;

	for (i = 0; i < parms->n_props; i++) {
		ret = scan_data(parms, p, size, "%i%i",
				&parms->dvb_prop[i].cmd,
				&parms->dvb_prop[i].u.data);
		if (ret < 0)
			goto error;

		p += ret;
		size -= ret;
	}

	strcpy(priv->output_charset, par->output_charset);
	strcpy(priv->default_charset, par->default_charset);

error:
	msg->seq = 0; /* Avoids any risk of a recursive call */
	pthread_mutex_unlock(&msg->lock);

	free_msg(dvb, msg);
	return ret;
}

int dvb_remote_fe_set_parms(struct dvb_v5_fe_parms *par)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)par;
	struct dvb_device_priv *dvb = parms->dvb;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct queued_msg *msg = NULL;
	int ret, i;
	char buf[REMOTE_BUF_SIZE], lnb_name[80] = "", *p = buf;
	size_t size = sizeof(buf);

	if (priv->disconnected)
		return -ENODEV;

	if (par->lnb)
		strcpy(lnb_name, par->lnb->name);

	ret = prepare_data(parms, p, size, "%i%i%s%i%i%i%i%s%s",
			   par->abort, par->lna, lnb_name,
			   par->sat_number, par->freq_bpf, par->diseqc_wait,
			   par->verbose, priv->default_charset,
		           priv->output_charset);
	if (ret < 0)
		goto error;

	p += ret;
	size -= ret;

	/* Now, the private ones */

	ret = prepare_data(parms, p, size, "%i", parms->country);
	if (ret < 0)
		goto error;

	p += ret;
	size -= ret;

	for (i = 0; i < parms->n_props; i++) {
		ret = prepare_data(parms, p, size, "%i%i",
				   parms->dvb_prop[i].cmd,
				   parms->dvb_prop[i].u.data);
		if (ret < 0)
			goto error;

		p += ret;
		size -= ret;
	}

	msg = send_buf(dvb, priv->fd, "fe_set_parms", buf, p - buf);
	if (!msg)
		goto error;

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		goto error;
	}

	ret = msg->retval;

error:
	if (msg) {
		msg->seq = 0; /* Avoids any risk of a recursive call */
		pthread_mutex_unlock(&msg->lock);

		free_msg(dvb, msg);
	}
	return ret;
}

int dvb_remote_fe_get_stats(struct dvb_v5_fe_parms *par)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)par;
	struct dvb_v5_stats *st = &parms->stats;
	struct dvb_device_priv *dvb = parms->dvb;
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct queued_msg *msg;
	int ret, status, i;
	char *p;
	size_t size;

	if (priv->disconnected)
		return -ENODEV;

	msg = send_fmt(dvb, priv->fd, "fe_get_stats", "-");
	if (!msg)
		return -1;

	ret = pthread_cond_wait(&msg->cond, &msg->lock);
	if (ret < 0) {
		dvb_logerr("error waiting for %s response", msg->cmd);
		goto error;
	}

	if (msg->retval) {
		ret = msg->retval;
		goto error;
	}

	p = msg->args;
	size = msg->args_size;

	ret = scan_data(parms, p, size, "%i", &status);
	if (ret < 0)
		goto error;

	st->prev_status = status;

	p += ret;
	size -= ret;

	for (i = 0; i < DTV_NUM_STATS_PROPS; i++) {
		ret = scan_data(parms, p, size, "%i%i",
				&st->prop[i].cmd,
				&st->prop[i].u.data);
		if (ret < 0)
			goto error;

		p += ret;
		size -= ret;
	}
	for (i = 0; i < MAX_DTV_STATS; i++) {
		struct dvb_v5_counters *prev = st->prev;
		struct dvb_v5_counters *cur = st->cur;

		ret = scan_data(parms, p, size, "%i%i%i",
				&st->has_post_ber[i],
				&st->has_pre_ber[i],
				&st->has_per[i]);
		if (ret < 0)
			goto error;

		p += ret;
		size -= ret;

		ret = scan_data(parms, p, size,
				"%" SCNu64 "%" SCNu64 "%" SCNu64 "%" SCNu64
				"%" SCNu64 "%" SCNu64 "%" SCNu64 "%" SCNu64
				"%" SCNu64 "%" SCNu64 "%" SCNu64 "%" SCNu64,
				&prev->pre_bit_count,
				&prev->pre_bit_error,
				&prev->post_bit_count,
				&prev->post_bit_error,
				&prev->block_count,
				&prev->block_error,
				&cur->pre_bit_count,
				&cur->pre_bit_error,
				&cur->post_bit_count,
				&cur->post_bit_error,
				&cur->block_count,
				&cur->block_error);
		if (ret < 0)
			goto error;

		p += ret;
		size -= ret;
	}

error:
	msg->seq = 0; /* Avoids any risk of a recursive call */
	pthread_mutex_unlock(&msg->lock);

	free_msg(dvb, msg);
	return 0;
}

static struct dvb_v5_descriptors *dvb_remote_scan(struct dvb_open_descriptor *open_dev,
					struct dvb_entry *entry,
					check_frontend_t *check_frontend,
					void *args,
					unsigned other_nit,
					unsigned timeout_multiply)
{
	/* FIXME: not implemented yet*/
	return NULL;
}

static void dvb_dev_remote_free(struct dvb_device_priv *dvb)
{
	struct dvb_dev_remote_priv *priv = dvb->priv;
	struct queued_msg *msg, *next;
	int timer = 0;

	/*
	 * If the application was well-written, the last message would be
	 * a dev_close(). At this point, no other log messages should
	 * happen.
	 */

	pthread_cancel(priv->recv_id);

	/* Cancel any pending messages */
	dvb_dev_remote_disconnect(priv);

	/* Give some time any pending message to be handled */
	do {
		usleep(1000);
		timer++;
	} while (timer < 1000 && priv->msgs.next);

	/* Free any pending message */
	msg = priv->msgs.next;
	while (msg) {
		next = msg->next;
		free(msg);
		msg = next;
	}

	pthread_mutex_destroy(&priv->lock_io);

	/* Close the socket */
	if (priv->fd > 0) {
		close(priv->fd);
		priv->fd = 0;
	}

	free(priv);
}

int dvb_dev_remote_init(struct dvb_device *d, char *server, int port)
{
	struct dvb_device_priv *dvb = (void *)d;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dvb_dev_remote_priv *priv;
	struct dvb_dev_ops *ops = &dvb->ops;
	int fd, ret, bufsize;

	/* Call an implementation-specific free method, if defined */
	if (ops->free)
		ops->free(dvb);

	dvb->priv = priv = calloc(1, sizeof(*priv));
	if (!priv)
		return -ENOMEM;

	strcpy(priv->output_charset, "utf-8");
	strcpy(priv->default_charset, "iso-8859-1");

	/* open socket */

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		dvb_perror("socket");
		return -1;
	}
	priv->fd = fd;

	/* connect socket to the server */

	priv->addr.sin_family = AF_INET;
	priv->addr.sin_port = htons(port);
	if (!inet_aton(server, &priv->addr.sin_addr))
	{
		dvb_perror(server);
		return -1;
	}
	ret = connect(fd, (struct sockaddr*)&priv->addr, sizeof(priv->addr));

	if (ret) {
		dvb_perror("connect");
		return -1;
	}

	/* Set large buffer for read() to work better */
	bufsize = REMOTE_BUF_SIZE;
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
		   (void *)&bufsize, (int)sizeof(bufsize));

	/* Start receiving messsages from the server */
	pthread_mutex_init(&priv->lock_io, NULL);
	ret = pthread_create(&priv->recv_id, NULL, receive_data, dvb);
	if (ret < 0) {
		dvb_perror("pthread_create");
		pthread_mutex_destroy(&priv->lock_io);
		return -1;
	}

	/* Do protocol handshake */
	ret = dvb_remote_get_version(dvb);
	if (ret <= 0) {
		pthread_mutex_destroy(&priv->lock_io);
		pthread_cancel(priv->recv_id);
	}

	/* Everything is OK, initialize data structs */
	ops->find = dvb_remote_find;
	ops->seek_by_sysname = dvb_remote_seek_by_sysname;
	ops->stop_monitor = dvb_remote_stop_monitor;
	ops->open = dvb_remote_open;
	ops->close = dvb_remote_close;

	ops->dmx_stop = dvb_remote_dmx_stop;
	ops->set_bufsize = dvb_remote_set_bufsize;
	ops->read = dvb_remote_read;
	ops->dmx_set_pesfilter = dvb_remote_dmx_set_pesfilter;
	ops->dmx_set_section_filter = dvb_remote_dmx_set_section_filter;
	ops->dmx_get_pmt_pid = dvb_remote_dmx_get_pmt_pid;

	ops->scan = dvb_remote_scan;

	ops->fe_set_sys = dvb_remote_fe_set_sys;
	ops->fe_get_parms = dvb_remote_fe_get_parms;
	ops->fe_set_parms = dvb_remote_fe_set_parms;
	ops->fe_get_stats = dvb_remote_fe_get_stats;

	ops->free = dvb_dev_remote_free;

	return 0;
}
