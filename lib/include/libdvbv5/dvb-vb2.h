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
#ifndef _LIBVB2_H
#define _LIBVB2_H

#include <stdint.h>
#include <linux/dvb/dmx.h>

/**
 * @file dvb-vb2.h
 * @ingroup frontend_scan
 * @brief Provides interfaces to videobuf2 streaming for DVB.
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Satendra Singh Thakur
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */


#ifdef __cplusplus
extern "C" {
#endif

#define memzero(x) memset(&(x), 0, sizeof(x))

/**Max count of the buffers*/
#define MAX_STREAM_BUF_CNT	10

/**
 * struct stream_ctx - Streaming context
 *
 * @param in_fd		File descriptor of streaming device
 * @param out_fd	File descriptor of output file
 * @param buf_cnt	Count of the buffers to be queued/dequeued
 * @param buf_size	Size of one such buffer
 * @param buf		Pointer to array of buffers
 * @param buf_flags	Array of boolean flags corresponding to buffers
 * @param exp_fd	Array of file descriptors of exported buffers
 * @param error		Error flag
 */
struct stream_ctx {
	int in_fd;
	int out_fd;
	int buf_cnt;
	int buf_size;
	unsigned char *buf[MAX_STREAM_BUF_CNT];
	int buf_flag[MAX_STREAM_BUF_CNT];
	int exp_fd[MAX_STREAM_BUF_CNT];
	int error;
};
/**
 * stream_qbuf - Enqueues a buffer specified by index n
 *
 * @param sc		Context for streaming management
 *			Pointer to &struct stream_ctx
 * @param idx		Index of the buffer
 *
 * @return At return, it returns a negative value if error or
 * zero on success.
 */
int stream_qbuf(struct stream_ctx *sc, int idx);

/**
 * sream_dqbuf - Dequeues a buffer specified by buf argument
 *
 * @param sc		Context for streaming management
 *			Pointer to &struct stream_ctx
 * @param buf		Pointer to &struct dmx_buffer
 *
 * @return At return, it returns a negative value if error or
 * zero on success.
 */
int stream_dqbuf(struct stream_ctx *sc, struct dmx_buffer *buf);
/**
 * sream_expbuf - Exports a buffer specified by buf argument
 *
 * @param sc		Context for streaming management
 *			Pointer to &struct stream_ctx
 * @param idx		Index of the buffer
 *
 * @return At return, it returns a negative value if error or
 * zero on success.
 */
int stream_expbuf(struct stream_ctx *sc, int idx);
/**
 * stream_init - Requests number of buffers from memory
 * Gets pointer to the buffers from driver, mmaps those buffers
 * and stores them in an array
 * Also, optionally exports those buffers
 *
 * @param sc		Context for streaming management
 * @param in_fd		File descriptor of the streaming device
 * @param buf_size	Size of the buffer
 * @param buf_cnt	Number of buffers
 *
 * @return At return, it returns a negative value if error or
 * zero on success.
 */
int stream_init(struct stream_ctx *sc, int in_fd, int buf_size, int buf_cnt);

/**
 * @struct dvb_table_filter
 * @brief De-initiazes streaming
 * @ingroup frontend_scan
 *
 * @param sc		Pointer to &struct stream_ctx
 */
void stream_deinit(struct stream_ctx *sc);
/**
 * stream_to_file - Implements enqueue and dequeue logic
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
void stream_to_file(int in_fd, int out_fd, int timeout, int dbg_level,
			int *exit_flag);

#ifdef __cplusplus
}
#endif

#endif
