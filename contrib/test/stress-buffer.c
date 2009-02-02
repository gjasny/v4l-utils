/*
 *  Copyright (C) 2009 Douglas Schilling Landgraf <dougsland@redhat.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  stress-buffer tool makes infinite calls using read() for
 *  any device specified by the command argument.
 *
 *  The size of buffer shall be a random number from 0 up to 1000.
 *
 *  Also is automatically created a file called: stats-M-D-Y-h-m-s.txt
 *  in current directory with data executed.
 *
 *  The stress test is performed by several read() calls,
 *  and it helped to identify real issues like:
 *
 *  - memory leaks
 *  - specific crashs that are rare and hard to reproduce
 *
 *  To compile:
 *             gcc -o stress-buffer stress-buffer.c -Wall
 *
 *  To execute:
 *             ./stress-buffer /dev/device_for_test
 *
 *  Example:
 *             ./stress-buffer /dev/video0
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>

int main(int argc, char **argv)
{
	char buffer[1000];
	char fname[30];
	int fd, ret, magic_buffer_size, cnt = 0;
	time_t t1, t2;
	double dif;
	time_t current;
	struct tm *timep;

	FILE *fd_file;

	if (argc != 2) {
		printf("Usage: %s /dev/device_to_test\n", argv[0]);
		return -1;
	}

	current = time(NULL);
	timep = localtime(&current);

	memset(fname, 0, sizeof(fname));

	snprintf(fname, sizeof(fname), "stats-%.2d-%.2d-%.2d-%.2d-%.2d-%.2d.txt",
		timep->tm_mon+1, timep->tm_mday, timep->tm_year + 1900,
		timep->tm_hour,	timep->tm_min, timep->tm_sec);

	fd_file = fopen(fname, "a+");
	if (!fd_file) {
		perror("error opening file");
		return -1;
	}

	srand(time(NULL));

	while (1) {

		if (time(&t1) < 0) {
			perror("time_t t1");
			fclose(fd_file);
			return -1;
		}

		fd = open(argv[1], O_RDONLY);
		if (fd < 0) {
			perror("error opening device");
			fclose(fd_file);
			return -1;
		}

		/* Random number 0 - 1000 */
		magic_buffer_size = rand() % sizeof(buffer);

		memset(buffer, 0, sizeof(buffer));

		ret = access(fname, W_OK);
		if (ret < 0) {
			close(fd);
			perror("Error");
			return -1;
		}

		ret = read(fd, buffer, magic_buffer_size);
		if (ret < 0) {
			fprintf(fd_file, "[%s] error reading buffer - [%s]\n",
				argv[1], strerror(errno));
			fflush(fd_file);
			perror("error reading buffer from device");
			return -1;
		}

		if (time(&t2) < 0) {
			perror("time_t t2");
			fclose(fd_file);
			return -1;
		}

		dif = difftime(t2, t1);

		printf("Seconds: [%d] - Test Number: [%d] - Read [%d] bytes\n",
			(int)dif, cnt, ret);

		fprintf(fd_file, "Seconds: [%.2f] - Test number: [%d] - Read [%d] bytes\n",
			dif,  cnt, ret);

		fflush(fd_file);

		cnt++;

		close(fd);

	}
	return 0;
}
