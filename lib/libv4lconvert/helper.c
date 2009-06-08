/*
#             (C) 2009 Hans de Goede <hdegoede@redhat.com>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include "libv4lconvert-priv.h"

#define READ_END  0
#define WRITE_END 1

/* <sigh> Unfortunately I've failed in contact some Authors of decompression
   code of out of tree drivers. So I've no permission to relicense their code
   their code from GPL to LGPL. To work around this, these decompression
   algorithms are put in separate executables and we pipe data through these
   to decompress.

   The "protocol" is very simple:

   From libv4l to the helper the following is send:
   int			width
   int			height
   int			flags
   int			data length
   unsigned char[]	data (data length long)

   From the helper to libv4l the following is send:
   int			data length (-1 in case of a decompression error)
   unsigned char[]	data (not present when a decompression error happened)
*/

static int v4lconvert_helper_start(struct v4lconvert_data *data,
  const char *helper)
{
  if (pipe(data->decompress_in_pipe)) {
    V4LCONVERT_ERR("with helper pipe: %s\n", strerror(errno));
    goto error;
  }

  if (pipe(data->decompress_out_pipe)) {
    V4LCONVERT_ERR("with helper pipe: %s\n", strerror(errno));
    goto error_close_in_pipe;
  }

  data->decompress_pid = fork();
  if (data->decompress_pid == -1) {
    V4LCONVERT_ERR("with helper fork: %s\n", strerror(errno));
    goto error_close_out_pipe;
  }

  if (data->decompress_pid == 0) {
    /* We are the child */

    /* Closed unused read / write end of the pipes */
    close(data->decompress_out_pipe[WRITE_END]);
    close(data->decompress_in_pipe[READ_END]);

    /* Connect stdin / out to the pipes */
    if (dup2(data->decompress_out_pipe[READ_END], STDIN_FILENO) == -1) {
      perror("libv4lconvert: error with helper dup2");
      exit(1);
    }
    if (dup2(data->decompress_in_pipe[WRITE_END], STDOUT_FILENO) == -1) {
      perror("libv4lconvert: error with helper dup2");
      exit(1);
    }

    /* And execute the helper */
    execl(helper, helper, NULL);

    /* We should never get here */
    perror("libv4lconvert: error starting helper");
    exit(1);
  } else {
    /* Closed unused read / write end of the pipes */
    close(data->decompress_out_pipe[READ_END]);
    close(data->decompress_in_pipe[WRITE_END]);
  }

  return 0;

error_close_out_pipe:
  close(data->decompress_out_pipe[READ_END]);
  close(data->decompress_out_pipe[WRITE_END]);
error_close_in_pipe:
  close(data->decompress_in_pipe[READ_END]);
  close(data->decompress_in_pipe[WRITE_END]);
error:
  return -1;
}

/* IMPROVE ME: we could block SIGPIPE here using pthread_sigmask()
   and then in case of EPIPE consume the signal using
   sigtimedwait (we need to check if a blocked signal wasn't present
   before the write, otherwise we will consume that) and
   after consuming the signal try to restart the helper.

   Note we currently do not do this, as SIGPIPE only happens if the
   decompressor crashes, which in case of an embedded decompressor
   would mean end of program, so by not handling SIGPIPE we treat
   external decompressors identical. */
static int v4lconvert_helper_write(struct v4lconvert_data *data,
  const void *b, size_t count)
{
  const unsigned char *buf = b;
  const int *i = b;
  size_t ret, written = 0;

  while (written < count) {
    ret = write(data->decompress_out_pipe[WRITE_END], buf + written,
		count - written);
    if (ret == -1) {
      if (errno == EINTR)
	continue;

      V4LCONVERT_ERR("writing to helper: %s\n", strerror(errno));
      return -1;
    }
    written += ret;
  }

  return 0;
}

static int v4lconvert_helper_read(struct v4lconvert_data *data, void *b,
  size_t count)
{
  unsigned char *buf = b;
  size_t ret, r = 0;

  while (r < count) {
    ret = read(data->decompress_in_pipe[READ_END], buf + r, count - r);
    if (ret == -1) {
      if (errno == EINTR)
	continue;

      V4LCONVERT_ERR("reading from helper: %s\n", strerror(errno));
      return -1;
    }
    if (ret == 0) {
      V4LCONVERT_ERR("reading from helper: unexpected EOF\n");
      return -1;
    }
    r += ret;
  }

  return 0;
}

int v4lconvert_helper_decompress(struct v4lconvert_data *data,
  const char *helper, const unsigned char *src, int src_size,
  unsigned char *dest, int dest_size, int width, int height, int flags)
{
  int r;

  if (data->decompress_pid == -1) {
    if (v4lconvert_helper_start(data, helper))
      return -1;
  }

  if (v4lconvert_helper_write(data, &width, sizeof(int)))
    return -1;

  if (v4lconvert_helper_write(data, &height, sizeof(int)))
    return -1;

  if (v4lconvert_helper_write(data, &flags, sizeof(int)))
    return -1;

  if (v4lconvert_helper_write(data, &src_size, sizeof(int)))
    return -1;

  if (v4lconvert_helper_write(data, src, src_size))
    return -1;

  if (v4lconvert_helper_read(data, &r, sizeof(int)))
    return -1;

  if (r < 0) {
    V4LCONVERT_ERR("decompressing frame data\n");
    return -1;
  }

  if (dest_size < r) {
    V4LCONVERT_ERR("destination buffer to small\n");
    return -1;
  }

  return v4lconvert_helper_read(data, dest, r);
}

void v4lconvert_helper_cleanup(struct v4lconvert_data *data)
{
  int status;

  if (data->decompress_pid != -1) {
    kill(data->decompress_pid, SIGTERM);
    waitpid(data->decompress_pid, &status, 0);

    close(data->decompress_out_pipe[WRITE_END]);
    close(data->decompress_in_pipe[READ_END]);

    data->decompress_pid = -1;
  }
}
