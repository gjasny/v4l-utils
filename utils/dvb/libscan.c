/******************************************************************************
 * Parse DVB tables
 * According with:
 *	ETSI EN 301 192 V1.5.1 (2009-11)
 * 	ISO/IEC 13818-1:2007
 *****************************************************************************/

#include "libscan.h"

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

static int parse_pat(struct dvb_descriptors *dvb_desc,
		      const unsigned char *buf, int *section_length)
{
	int service_id, pmt_pid;
	int next, n;

	dvb_desc->pat_table.ts_id = (buf[3] << 8) | buf[4];
	dvb_desc->pat_table.version = (buf[5] >> 1) & 0x1f;
	printf("PAT TS ID=0x%04x, version %d\n",
	       dvb_desc->pat_table.ts_id,
		dvb_desc->pat_table.version);

	next = (buf[6] == buf[7]) ? 0 : 1;
	printf("section %d, last section %d\n", buf[6], buf[7]);

	buf += 8;
	*section_length -= 8;

	n = dvb_desc->pat_table.pid_table_len;
	while (*section_length > 3) {
		dvb_desc->pat_table.pid_table = realloc(
			dvb_desc->pat_table.pid_table,
			(n + 1) *
			sizeof(*dvb_desc->pat_table.pid_table));

		service_id = (buf[0] << 8) | buf[1];
		pmt_pid = ((buf[2] & 0x1f) << 8) | buf[3];

		dvb_desc->pat_table.pid_table[n].program_number = service_id;
		dvb_desc->pat_table.pid_table[n].pid = pmt_pid;

		printf("service_id %d, pmt_pid %d\n", service_id, pmt_pid);

		buf += 4;
		*section_length -= 4;
		n++;
	}
	dvb_desc->pat_table.pid_table_len = n;

	return !next;
}

static void hexdump(const unsigned char *buf, int len)
{
	int i;

	printf("[%d]", len);
	for (i = 0; i < len; i++) {
		if (!(i % 16))
			printf("\n\t");
		printf("%02x ", (uint8_t) *(buf + i));
	}
	printf("\n");
}

static int read_section(int dmx_fd, struct dvb_descriptors *dvb_desc)
{
	int count;
	int section_length, table_id;
	unsigned char buf[4096];
	unsigned char *p;
	int finish = 0;

	while (!finish) {
		do {
			count = read(dmx_fd, buf, sizeof(buf));
		} while (count < 0 && errno == EOVERFLOW);

		if (count < 0) {
			perror("read_sections: read error");
			close(dmx_fd);
			return -1;
		}

		p = buf;

		hexdump(buf, count);
		if (count < 3)
			continue;

		table_id = *p;
		section_length = ((p[1] & 0x0f) << 8) | p[2];

		printf("section_length = %d\n", section_length);

		if (count != section_length + 3)
			continue;

		switch (table_id) {
		case 0x00:	/* PAT */
			finish = parse_pat(dvb_desc, p, &section_length);
			break;
//		case 0x42:	/* NIT */
		}
	}
	return 0;
}

struct dvb_descriptors *get_dvb_ts_tables(char *dmxdev)
{
	int dmx_fd;
	struct dmx_sct_filter_params f;
	struct dvb_descriptors *dvb_desc;


	memset(&f, 0, sizeof(f));

	if ((dmx_fd = open(dmxdev, O_RDWR)) < 0) {
		perror("openening pat demux failed");
		return NULL;
	}

	dvb_desc = calloc(sizeof(*dvb_desc), 1);
	if (!dvb_desc) {
		close (dmx_fd);
		return NULL;
	}

	/* PAT table */
	f.pid = 0;
	f.filter.filter[0] = 0x00;
	f.filter.mask[0] = 0xff;
	f.timeout = 0;
	f.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;
	if (ioctl(dmx_fd, DMX_SET_FILTER, &f) == -1) {
		perror("ioctl DMX_SET_FILTER failed");
		close(dmx_fd);
		return NULL;
	}
	read_section(dmx_fd, dvb_desc);

#if 0
	/* NIT table */
	f.pid = 0x11;
	f.filter.filter[0] = 0x42;
	f.filter.mask[0] = 0xff;
	f.timeout = 0;
	f.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;
	if (ioctl(dmx_fd, DMX_SET_FILTER, &f) == -1) {
		perror("ioctl DMX_SET_FILTER failed");
		close(dmx_fd);
		return NULL;
	}
	read_section(dmx_fd, dvb_desc);
#endif

	close(dmx_fd);

	return dvb_desc;
}
