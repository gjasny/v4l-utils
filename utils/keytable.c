/* keytable.c - This program allows checking/replacing keys at IR

   Copyright (C) 2006 Mauro Carvalho Chehab <mchehab@infradead.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 */

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/input.h>
#include <sys/ioctl.h>

void prtcode (int *codes)
{
	if (isprint (codes[1]))
		printf("scancode %d = '%c' (0x%02x)\n", codes[0], codes[1], codes[1]);
	else
		printf("scancode %d = 0x%02x\n", codes[0], codes[1]);
}

int main (int argc, char *argv[])
{
	int fd;
	unsigned int i;
	int codes[2];

	if (argc!=2 && argc!=4) {
		printf ("usage: %s <device> to get table; or\n"
			"       %s <device> <scancode> <keycode>\n",*argv,*argv);
		return -1;
	}

        if ((fd = open(argv[1], O_RDONLY)) < 0) {
                perror("Couldn't open input device");
                return(-1);
        }

	if (argc==2) {
		/* Get scancode table */
		for (i=0;i<256;i++) {
			codes[0] = i;
			if(ioctl(fd, EVIOCGKEYCODE, codes)==0)
				prtcode(codes);
//			else perror ("EVIOGCKEYCODE");
		}
	} else {
		codes[0] = (unsigned) strtol(argv[2], NULL, 0);
		codes[1] = (unsigned) strtol(argv[3], NULL, 0);

		if(ioctl(fd, EVIOCSKEYCODE, codes))
			perror ("EVIOCSKEYCODE");

		if(ioctl(fd, EVIOCGKEYCODE, codes)==0)
			prtcode(codes);
	}

	return 0;
}
