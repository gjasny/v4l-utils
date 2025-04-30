/*
 * Copyright (c) 2017-2018 - Mauro Carvalho Chehab
 * Copyright (c) 2017-2018 - Junghak Sung <jh1009.sung@samsung.com>
 * Copyright (c) 2017-2018 - Satendra Singh Thakur <satendra.t@samsung.com>
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

/******************************************************************************
 * Implements videobuf2 streaming APIs for DVB
 *****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/time.h>

#include <sys/mman.h>
#include <libdvbv5/dvb-vb2.h>

#define PERROR(x...)                                                    \
	do {                                                            \
		fprintf(stderr, "ERROR: ");                             \
		fprintf(stderr, x);                                     \
		fprintf(stderr, " (%s)\n", strerror(errno));		\
	} while (0)

/**These 2 params are for DVR*/
#define STREAM_BUF_CNT (10)
#define STREAM_BUF_SIZ (188*1024)
/*Sleep time for retry, in case ioctl fails*/
#define SLEEP_US	1000

#define memzero(x) memset(&(x), 0, sizeof(x))

static inline int xioctl(int fd, unsigned long int cmd, void *arg)
{
	int ret;
	struct timespec stime, etime;
	long long etimell = 0, stimell = 0;
	clock_gettime(CLOCK_MONOTONIC, &stime);
	do {
		ret = ioctl(fd, cmd, arg);
		if (ret < 0 && (errno == EINTR || errno == EAGAIN)) {
			clock_gettime(CLOCK_MONOTONIC, &etime);
			etimell = (long long) etime.tv_sec * 1000000000 +
					etime.tv_nsec;
			stimell = (long long) (stime.tv_sec + 1 /*1 sec wait*/)
					* 1000000000 + stime.tv_nsec;
			if (etimell > stimell)
				break;
			/*wait for some time to prevent cpu hogging*/
			usleep(SLEEP_US);
			continue;
		}
		else
			break;
	} while (1);

        return ret;
}


/**
 * dvb_v5_stream_qbuf - Enqueues a buffer specified by index
 *
 * @param sc		Context for streaming management
 *			Pointer to &struct dvb_v5_stream_ctx
 * @param idx		Index of the buffer
 *
 * @return At return, it returns a negative value if error or
 * zero on success.
 */
int dvb_v5_stream_qbuf(struct dvb_v5_stream_ctx *sc, int idx)
{
	struct dmx_buffer buf;
	int ret;

	memzero(buf);
	buf.index = idx;

	ret = xioctl(sc->in_fd, DMX_QBUF, &buf);
	if (ret < 0) {
		PERROR("DMX_QBUF failed: error=%d", ret);
		return ret;
	}

	return ret;
}

/**
 * dvb_v5_stream_dqbuf - Dequeues a buffer specified by index
 *
 * @param sc		Context for streaming management
 *			Pointer to &struct dvb_v5_stream_ctx
 * @param buf		Pointer to &struct dmx_buffer
 *
 * @return At return, it returns a negative value if error or
 * zero on success.
 */
int dvb_v5_stream_dqbuf(struct dvb_v5_stream_ctx *sc, struct dmx_buffer *buf)
{
	int ret;

	ret = xioctl(sc->in_fd, DMX_DQBUF, buf);
	if (ret < 0) {
		PERROR("DMX_DQBUF failed: error=%d", ret);
		return ret;
	}

	return ret;
}
/**
 * dvb_v5_stream_expbuf - Exports a buffer specified by buf argument
 *
 * @param sc		Context for streaming management
 *			Pointer to &struct dvb_v5_stream_ctx
 * @param idx		Buffer index
 *
 * @return At return, it returns a negative value if error or
 * zero on success.
 */
int dvb_v5_stream_expbuf(struct dvb_v5_stream_ctx *sc, int idx)
{
	int ret;
	struct dmx_exportbuffer exp;
	memzero(exp);
	exp.index = idx;
	ret = ioctl(sc->in_fd, DMX_EXPBUF, &exp);
	if (ret) {
		PERROR("DMX_EXPBUF failed: buf=%d error=%d", idx, ret);
		return ret;
	}
	sc->exp_fd[idx] = exp.fd;
	fprintf(stderr, "Export buffer %d (fd=%d)\n",
			idx, sc->exp_fd[idx]);
	return ret;
}

/**
 * dvb_v5_stream_alloc - Allocate stream context
 *
 * @return a stream context or NULL.
 */
struct dvb_v5_stream_ctx *dvb_v5_stream_alloc(void)
{
	return calloc(1, sizeof(struct dvb_v5_stream_ctx));
}

/**
 * dvb_v5_stream_init - Requests number of buffers from memory
 * Gets pointer to the buffers from driver, mmaps those buffers
 * and stores them in an array
 * Also, optionally exports those buffers
 *
 * @param sc		Context for streaming management
 * @param in_fd		File descriptor of the streaming device
 * @param buf_size	Size of the buffer
 * @param buf_cnt	Count of the buffers
 *
 * @return At return, it returns a negative value if error or
 * zero on success.
 */
int dvb_v5_stream_init(struct dvb_v5_stream_ctx *sc, int in_fd, int buf_size, int buf_cnt)
{
	struct dmx_requestbuffers req;
	struct dmx_buffer buf;
	int ret;
	int i;

	memset(sc, 0, sizeof(struct dvb_v5_stream_ctx));
	sc->in_fd = in_fd;
	sc->buf_size = buf_size;
	sc->buf_cnt = buf_cnt;

	memzero(req);
	req.count = sc->buf_cnt;
	req.size = sc->buf_size;

	ret = xioctl(in_fd, DMX_REQBUFS, &req);
	if (ret) {
		PERROR("DMX_REQBUFS failed: error=%d", ret);
		return ret;
	}

	if (sc->buf_cnt != req.count) {
		PERROR("buf_cnt %d -> %d changed !!!", sc->buf_cnt, req.count);
		sc->buf_cnt = req.count;
	}

	for (i = 0; i < sc->buf_cnt; i++) {
		memzero(buf);
		buf.index = i;

		ret = xioctl(in_fd, DMX_QUERYBUF, &buf);
		if (ret) {
			PERROR("DMX_QUERYBUF failed: buf=%d error=%d", i, ret);
			return ret;
		}

		sc->buf[i] = mmap(NULL, buf.length,
					PROT_READ | PROT_WRITE, MAP_SHARED,
					in_fd, buf.offset);

		if (sc->buf[i] == MAP_FAILED) {
			PERROR("Failed to MMAP buffer %d", i);
			return -1;
		}
		/**enqueue the buffers*/
		ret = dvb_v5_stream_qbuf(sc, i);
		if (ret) {
			PERROR("stream_qbuf failed: buf=%d error=%d", i, ret);
			return ret;
		}

		sc->buf_flag[i] = 1;
	}

	return 0;
}

/**
 * dvb_v5_stream_deinit - Dequeues and unmaps the buffers
 *
 * @param sc - Context for streaming management
 *
 * @return At return, it returns a negative value if error or
 * zero on success.
 */
void dvb_v5_stream_deinit(struct dvb_v5_stream_ctx *sc)
{
	struct dmx_buffer buf;
	int ret;
	int i;

	for (i = 0; i < sc->buf_cnt; i++) {
		memzero(buf);
		buf.index = i;

		if (sc->buf_flag[i]) {
			ret = dvb_v5_stream_dqbuf(sc, &buf);
			if (ret) {
				PERROR("stream_dqbuf failed: buf=%d error=%d",
					 i, ret);
			}
		}
		ret = munmap(sc->buf[i], sc->buf_size);
		if (ret) {
			PERROR("munmap failed: buf=%d error=%d", i, ret);
		}

	}
}

/**
 * dvb_v5_stream_free - Free stream context
 *
 * @param sc - Context for streaming management
 */
void dvb_v5_stream_free(struct dvb_v5_stream_ctx *sc)
{
	free(sc);
}

/**
 * dvb_v5_stream_to_file - Implements enqueue and dequeue logic
 * First enqueues all the available buffers then dequeues
 * one buffer, again enqueues it and so on.
 *
 * @param in_fd		File descriptor of the streaming device
 * @param out_fd	File descriptor of output file
 * @param timeout	Timeout in seconds
 * @param dbg_level	Debug flag
 * @param exit_flag	Flag to exit
 *
 * @return void
 */
void dvb_v5_stream_to_file(int in_fd, int out_fd, int timeout, int dbg_level,
			   int *exit_flag)
{
	struct dvb_v5_stream_ctx *sc = dvb_v5_stream_alloc();
	long long int rc = 0LL;
	int ret;

	if (!sc) {
		PERROR("[%s] Failed to allocate stream context", __func__);
		return;
	}
	ret = dvb_v5_stream_init(sc, in_fd, STREAM_BUF_SIZ, STREAM_BUF_CNT);
	if (ret < 0) {
		PERROR("[%s] Failed to initialize stream context", __func__);
		dvb_v5_stream_free(sc);
		return;
	}
	sc->out_fd = out_fd;

	while (!*exit_flag  && !sc->error) {
		/* dequeue the buffer */
		struct dmx_buffer b;

		memzero(b);
		ret = dvb_v5_stream_dqbuf(sc, &b);
		if (ret < 0) {
			sc->error = 1;
			break;
		}
		else {
			sc->buf_flag[b.index] = 0;
			ret = write(sc->out_fd, sc->buf[b.index], b.bytesused);
			if (ret < 0) {
				PERROR("Write failed err=%d", ret);
				break;
			} else
				rc += b.bytesused;
		}

		/* enqueue the buffer */
		ret = dvb_v5_stream_qbuf(sc, b.index);
		if (ret < 0)
			sc->error = 1;
		else
			sc->buf_flag[b.index] = 1;
	}
	if (dbg_level < 2) {
		fprintf(stderr, "copied %lld bytes (%lld Kbytes/sec)\n", rc,
			rc / (1024 * timeout));
	}
	dvb_v5_stream_deinit(sc);
	dvb_v5_stream_free(sc);
}
