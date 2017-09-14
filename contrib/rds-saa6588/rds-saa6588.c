/*
 * $Id: rds-saa6588.c,v 1.3 2005/06/12 04:19:19 mchehab Exp $
 *
 * poll i2c RDS receiver [Philips saa6588]
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

int debug;

/* ----------------------------------------------------------------- */

char rds_psn[9];
char rds_txt[65];

static void rds_decode(int blkno,int b1, int b2)
{
    static int group,spare,c1,c2;

    switch (blkno) {
    case 0:
	if (debug)
	    fprintf(stderr,"block A - id=%d\n",
		    (b1 << 8) | b2);
	break;
    case 1:
	if (debug)
	    fprintf(stderr,"block B - group=%d%c tp=%d pty=%d spare=%d\n",
		    (b1 >> 4) & 0x0f,
		    ((b1 >> 3) & 0x01) + 'A',
		    (b1 >> 2) & 0x01,
		    ((b1 << 3) & 0x18) | ((b2 >> 5) & 0x07),
		    b2 & 0x1f);
	group = (b1 >> 3) & 0x1f;
	spare = b2 & 0x1f;
	break;
    case 2:
	if (debug)
	    fprintf(stderr,"block C - 0x%02x 0x%02x\n",b1,b2);
	c1 = b1;
	c2 = b2;
	break;
    case 3:
	if (debug)
	    fprintf(stderr,"block D - 0x%02x 0x%02x\n",b1,b2);
	switch (group) {
	case 0:
	    rds_psn[2*(spare & 0x03)+0] = b1;
	    rds_psn[2*(spare & 0x03)+1] = b2;
	    if ((spare & 0x03) == 0x03)
		fprintf(stderr,"PSN #>%s<#\n",rds_psn);
	    break;
	case 4:
	    rds_txt[4*(spare & 0x0f)+0] = c1;
	    rds_txt[4*(spare & 0x0f)+1] = c2;
	    rds_txt[4*(spare & 0x0f)+2] = b1;
	    rds_txt[4*(spare & 0x0f)+3] = b2;
	    if ((spare & 0x0f) == 0x0f)
		fprintf(stderr,"TXT #>%s<#\n",rds_txt);
	    break;
	}
	break;
    default:
	fprintf(stderr,"unknown block [%d]\n",blkno);
    }
}

int
main(int argc, char *argv[])
{
    int  c,f,rc, no, lastno = -1;
    unsigned char b[40];
    char *device = "/dev/i2c-0";

    /* parse options */
    while (-1 != (c=getopt(argc,argv,"hvd:"))) {
	switch (c){
	case 'd':
	    if (optarg)
		device = optarg;
	    break;
	case 'v':
	    debug = 1;
	    break;
	case 'h':
	default:
	    printf("poll i2c RDS receiver [saa6588] via chardev\n");
	    printf("usage: %s [ -d i2c-device ]\n",argv[0]);
	    exit(1);
	}
    }

    if (-1 == (f = open(device,O_RDWR))) {
	fprintf(stderr,"open %s: %s\n",device,strerror(errno));
	exit(1);
    }
    ioctl(f,I2C_SLAVE,0x20 >> 1);
    for (;;) {
	memset(b,0,sizeof(b));
	rc = read(f,b,6);
	if (6 != rc) {
	    fprintf(stderr,"oops: read: rc=%d, expected 6 [%s]\n",
		    rc,strerror(errno));
	    break;
	}
	if (0 == (b[0] & 0x10)) {
	    fprintf(stderr,"no signal\r");
	    continue;
	}
	if (1 == (b[0] & 0x08)) {
	    fprintf(stderr,"overflow detected\n");
	}
	if (1 == (b[0] & 0x04)) {
	    fprintf(stderr,"reset detected\n");
	}
	if (debug)
	    fprintf(stderr,"raw: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		    b[0],b[1],b[2],b[3],b[4],b[5]);
	no = b[0] >> 5;
	if (lastno != no) {
		rds_decode(no, b[1], b[2]);
		lastno = no;
	}
	usleep(10*1000);
    }
    close(f);
    exit(0);
}
