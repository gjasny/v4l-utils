/* Utility functions for decompression helpers
 *
 * Copyright (c) 2009 Hans de Goede <hdegoede@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static int v4lconvert_helper_write(int fd, const void *b, size_t count,
  char *progname)
{
  const unsigned char *buf = b;
  size_t ret, written = 0;

  while (written < count) {
    ret = write(fd, buf + written, count - written);
    if (ret == -1) {
      if (errno == EINTR)
	continue;

      if (errno == EPIPE) /* Main program has quited */
	exit(0);

      fprintf(stderr, "%s: error writing: %s\n", progname, strerror(errno));
      return -1;
    }
    written += ret;
  }

  return 0;
}

static int v4lconvert_helper_read(int fd, void *b, size_t count,
  char *progname)
{
  unsigned char *buf = b;
  size_t ret, r = 0;

  while (r < count) {
    ret = read(fd, buf + r, count - r);
    if (ret == -1) {
      if (errno == EINTR)
	continue;

      fprintf(stderr, "%s: error reading: %s\n", progname, strerror(errno));
      return -1;
    }
    if (ret == 0) /* EOF */
      exit(0);

    r += ret;
  }

  return 0;
}
