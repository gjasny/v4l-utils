/*
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
 *
 * These routines were written as part of the dvb-apps, as:
 *	util functions for various ?zap implementations
 *
 *	Copyright (C) 2001 Johannes Stezenbach (js@convergence.de)
 *	for convergence integrated media
 *
 *	Originally licensed as GPLv2 or upper
 *
 * All subsequent changes are under GPLv2 only and are:
 *	Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 *
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h> /* free */

#include <libdvbv5/dvb-demux.h>
#include <libdvbv5/dvb-dev.h>

#define MAX_TIME		10	/* 1.0 seconds */

#define xioctl(fh, request, arg...) ({					\
	int __rc;							\
	struct timespec __start, __end;					\
									\
	clock_gettime(CLOCK_MONOTONIC, &__start);			\
	do {								\
		__rc = ioctl(fh, request, ##arg);			\
		if (__rc != -1)						\
			break;						\
		if ((errno != EINTR) && (errno != EAGAIN))		\
			break;						\
		clock_gettime(CLOCK_MONOTONIC, &__end);			\
		if (__end.tv_sec * 10 + __end.tv_nsec / 100000000 >	\
		    __start.tv_sec * 10 + __start.tv_nsec / 100000000 +	\
		    MAX_TIME)						\
			break;						\
	} while (1);							\
									\
	__rc;								\
})

int dvb_dmx_open(int adapter, int demux)
{
	int fd_demux;
	struct dvb_device *dvb;
	struct dvb_dev_list *dvb_dev;

	dvb = dvb_dev_alloc();
	dvb_dev_find(dvb, 0);
	dvb_dev = dvb_dev_seek_by_sysname(dvb, adapter, demux, DVB_DEVICE_DEMUX);
	if (!dvb_dev) {
		dvb_dev_free(dvb);
		return -1;
	}

	fd_demux = open(dvb_dev->path, O_RDWR | O_NONBLOCK);
	dvb_dev_free(dvb);
	return fd_demux;
}

void dvb_dmx_close(int dmx_fd)
{
	(void)xioctl(dmx_fd, DMX_STOP);
	close(dmx_fd);
}

void dvb_dmx_stop(int dmx_fd)
{
	(void)xioctl(dmx_fd, DMX_STOP);
}

int dvb_set_pesfilter(int dmxfd, int pid, dmx_pes_type_t type,
		      dmx_output_t output, int buffersize)
{
	struct dmx_pes_filter_params pesfilter;

	if (buffersize) {
		if (xioctl(dmxfd, DMX_SET_BUFFER_SIZE, buffersize) == -1)
			perror("DMX_SET_BUFFER_SIZE failed");
	}

	memset(&pesfilter, 0, sizeof(pesfilter));

	pesfilter.pid = pid;
	pesfilter.input = DMX_IN_FRONTEND;
	pesfilter.output = output;
	pesfilter.pes_type = type;
	pesfilter.flags = DMX_IMMEDIATE_START;

	if (xioctl(dmxfd, DMX_SET_PES_FILTER, &pesfilter) == -1) {
		fprintf(stderr, "DMX_SET_PES_FILTER failed "
		"(PID = 0x%04x): %d %m\n", pid, errno);
		return -1;
	}

	return 0;
}

int dvb_set_section_filter(int dmxfd, int pid, unsigned filtsize,
			   unsigned char *filter,
			   unsigned char *mask,
			   unsigned char *mode,
			   unsigned int flags)
{
	struct dmx_sct_filter_params sctfilter;

	if (filtsize > DMX_FILTER_SIZE)
		filtsize = DMX_FILTER_SIZE;

	memset(&sctfilter, 0, sizeof(sctfilter));

	sctfilter.pid = pid;

	if (filter)
		memcpy(sctfilter.filter.filter, filter, filtsize);
	if (mask)
		memcpy(sctfilter.filter.mask, mask, filtsize);
	if (mode)
		memcpy(sctfilter.filter.mode, mode, filtsize);

	sctfilter.flags = flags;

	if (xioctl(dmxfd, DMX_SET_FILTER, &sctfilter) == -1) {
		fprintf(stderr, "DMX_SET_FILTER failed (PID = 0x%04x): %d %m\n",
			pid, errno);
		return -1;
	}

	return 0;
}

int dvb_get_pmt_pid(int patfd, int sid)
{
	int count;
	int pmt_pid = 0;
	int patread = 0;
	int section_length;
	unsigned char buft[4096];
	unsigned char *buf = buft;
	struct dmx_sct_filter_params f;

	memset(&f, 0, sizeof(f));
	f.pid = 0;
	f.filter.filter[0] = 0x00;
	f.filter.mask[0] = 0xff;
	f.timeout = 0;
	f.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;

	if (xioctl(patfd, DMX_SET_FILTER, &f) == -1) {
		perror("ioctl DMX_SET_FILTER failed");
		return -1;
	}

	while (!patread){
		if (((count = read(patfd, buf, sizeof(buft))) < 0) && errno == EOVERFLOW)
		count = read(patfd, buf, sizeof(buft));
		if (count < 0) {
		perror("read_sections: read error");
		return -1;
		}

		section_length = ((buf[1] & 0x0f) << 8) | buf[2];
		if (count != section_length + 3)
		continue;

		buf += 8;
		section_length -= 8;

		patread = 1; /* assumes one section contains the whole pat */
		while (section_length > 0) {
		int service_id = (buf[0] << 8) | buf[1];
		if (service_id == sid) {
			pmt_pid = ((buf[2] & 0x1f) << 8) | buf[3];
			section_length = 0;
		}
		buf += 4;
		section_length -= 4;
		}
	}

	return pmt_pid;
}
