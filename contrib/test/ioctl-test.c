/*
   ioctl-test - This small utility sends dumb v4l2 ioctl to a device.

   It is meant to check ioctl debug messages generated and to check
   if a function is implemented by that device. By compiling it as a
   32-bit binary on a 64-bit architecture it can also be used to test
   the 32-to-64 bit compat layer.

   In addition it also checks that all the ioctl values haven't changed.
   This tool should be run whenever a new ioctl is added, or when fields
   are added to existing structs used by ioctls.

   To compile as a 32-bit executable when on a 64-bit architecture use:

   	gcc -o ioctl-test32 ioctl-test.c -I../../include -m32

   Copyright (C) 2005-2013 Mauro Carvalho Chehab <mchehab@redhat.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "ioctl-test.h"

int main(int argc, char **argv)
{
	int fd = 0;
	unsigned i;
	unsigned maxlen = 0;
	int cmd_errors = 0;
	char *device = "/dev/video0";
	char marker[8] = { 0xde, 0xad, 0xbe, 0xef, 0xad, 0xbc, 0xcb, 0xda };
	char p[sizeof(union v4l_parms) + 2 * sizeof(marker)];
	static const char *dirs[] = {
		"  IO",
		" IOW",
		" IOR",
		"IOWR"
	};

	if (argv[1])
		device = argv[1];
	
	for (i = 0; i < S_IOCTLS; i++) {
		unsigned cmp_cmd;

		if (strlen(ioctls[i].name) > maxlen)
			maxlen = strlen(ioctls[i].name);
		if (sizeof(long) == 4)
			cmp_cmd = ioctls[i].cmd32;
		else
			cmp_cmd = ioctls[i].cmd64;
		if (cmp_cmd != ioctls[i].cmd) {
			fprintf(stderr, "%s: ioctl is 0x%08x but should be 0x%08x!\n",
					ioctls[i].name, ioctls[i].cmd, cmp_cmd);
			cmd_errors = 1;
		}
	}
	if (cmd_errors)
		return -1;

	if ((fd = open(device, O_RDONLY|O_NONBLOCK)) < 0) {
		fprintf(stderr, "Couldn't open %s\n", device);
		return -1;
	}

	for (i = 0; i < S_IOCTLS; i++) {
		char buf[maxlen + 100];
		const char *name = ioctls[i].name;
		u_int32_t cmd = ioctls[i].cmd;
		int dir = _IOC_DIR(cmd);
		char type = _IOC_TYPE(cmd);
		int nr = _IOC_NR(cmd);
		int sz = _IOC_SIZE(cmd);

		/* Only apply the pertinent ioctl's to the device */
		if (!strstr(device, ioctls[i].type))
			continue;

		/* Check whether the front and back markers aren't overwritten.
		   Useful to verify the compat32 code. */
		memset(&p, 0, sizeof(p));
		memcpy(p, marker, sizeof(marker));
		memcpy(p + sz + sizeof(marker), marker, sizeof(marker));
		sprintf(buf, "ioctl 0x%08x = %s('%c', %2d, %4d) = %-*s",
			cmd, dirs[dir], type, nr, sz, maxlen, name);
		errno = 0;
		ioctl(fd, cmd, (void *)&p[sizeof(marker)]);
		perror(buf);
		if (memcmp(p, marker, sizeof(marker)))
			fprintf(stderr, "%s: front marker overwritten!\n", name);
		if (memcmp(p + sizeof(marker) + sz, marker, sizeof(marker)))
			fprintf(stderr, "%s: back marker overwritten!\n", name);
	}
	close (fd);
	return 0;
}
