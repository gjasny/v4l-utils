/*
   decode_tm6000.c - decode multiplexed format from TM5600/TM6000 USB

   Copyright (C) 2007 Mauro Carvalho Chehab <mchehab@infradead.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation version 2.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA.
 */
#include "../libv4l2util/v4l2_driver.h"
#include <stdio.h>
#include <string.h>
#include <argp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

const char *argp_program_version="decode_tm6000 version 0.0.1";
const char *argp_program_bug_address="Mauro Carvalho Chehab <mchehab@infradead.org>";
const char doc[]="Decodes tm6000 proprietary format streams";
const struct argp_option options[] = {
	{"verbose",	'v',	0,	0,	"enables debug messages", 0},
	{"device",	'd',	"DEV",	0,	"uses device for reading", 0},
	{"output",	'o',	"FILE",	0,	"outputs raw stream to a file", 0},
	{"input",	'i',	"FILE",	0,	"parses a file, instead of a device", 0},
	{"freq",	'f',	"Freq",	0,	"station frequency, in MHz (default is 193.25)", 0},
	{"nbufs",	'n',	"quant",0,	"number of video buffers", 0},
	{"audio",	'a',	0,	0,	"outputs audio on stdout", 0},
	{"read",	'r',	0,	0,	"use read() instead of mmap method", 0},
	{ 0, 0, 0, 0, 0, 0 }
};

static char outbuf[692224];
static int debug=0, audio=0, use_mmap=1, nbufs=4;
static float freq_mhz=193.25;
static char *devicename="/dev/video0";
static char *filename=NULL;
static enum {
	NORMAL,
	INPUT,
	OUTPUT
} mode = NORMAL;

static FILE *fout;

//const char args_doc[]="ARG1 ARG2";

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	switch (key) {
	case 'a':
		audio++;
		break;
	case 'r':
		use_mmap=0;
		break;
	case 'v':
		debug++;
		break;
	case 'd':
		devicename=arg;
		break;
	case 'i':
	case 'o':
		if (mode!=NORMAL) {
			argp_error(state,"You can't use input/output options simultaneously.\n");
			break;
		}
		if (key=='i')
			mode=INPUT;
		else
			mode=OUTPUT;

		filename=arg;
		break;
	case 'f':
		freq_mhz=atof(arg);
		break;
	case 'n':
		nbufs=atoi(arg);
		if  (nbufs<2)
			nbufs=2;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = {
	.options=options,
	.parser=parse_opt,
	.args_doc=NULL,
	.doc=doc,
};

#define TM6000_URB_MSG_LEN 180
enum {
	TM6000_URB_MSG_VIDEO=1,
	TM6000_URB_MSG_AUDIO,
	TM6000_URB_MSG_VBI,
	TM6000_URB_MSG_PTS,
	TM6000_URB_MSG_ERR,
};

const char *tm6000_msg_type[]= {
	"unknown(0)",	//0
	"video",	//1
	"audio",	//2
	"vbi",		//3
	"pts",		//4
	"err",		//5
	"unknown(6)",	//6
	"unknown(7)",	//7
};

#define dprintf(fmt,arg...) \
	if (debug) fprintf(stderr, fmt, ##arg)

static int recebe_buffer (struct v4l2_buffer *v4l2_buf, struct v4l2_t_buf *buf)
{
	dprintf("Received %zd bytes\n", buf->length);
fflush(stdout);
	memcpy (outbuf,buf->start,buf->length);
	return buf->length;
}


static int prepare_read (struct v4l2_driver *drv)
{
	struct v4l2_format fmt;
	double freq;
	int rc;

	memset (drv,0,sizeof(*drv));

	if (v4l2_open (devicename, 1,drv)<0) {
		perror ("Error opening dev");
		return -1;
	}

	memset (&fmt,0,sizeof(fmt));

	uint32_t pixelformat=V4L2_PIX_FMT_TM6000;

	if (v4l2_gettryset_fmt_cap (drv,V4L2_SET,&fmt, 720, 480,
				    pixelformat,V4L2_FIELD_ANY)) {
		perror("set_input to tm6000 raw format");
		return -1;
	}

	if (freq_mhz) {
		freq=freq_mhz * 1000 * 1000;
		rc=v4l2_getset_freq (drv,V4L2_SET, &freq);
		if (rc<0)
			printf ("Cannot set freq to %.3f MHz\n",freq_mhz);
	}

	if (use_mmap) {
		printf("Preparing for receiving frames on %d buffers...\n",nbufs);
		fflush (stdout);

		rc=v4l2_mmap_bufs(drv, nbufs);
		if (rc<0) {
			printf ("Cannot mmap %d buffers\n",nbufs);
			return -1;
		}

//		v4l2_stop_streaming(&drv);
		rc=v4l2_start_streaming(drv);
		if (rc<0) {
			printf ("Cannot start streaming\n");
			return -1;
		}
	}
	printf("Waiting for frames...\n");

	return 0;
}

static int read_stream (struct v4l2_driver *drv, int fd)
{
	if (use_mmap) {
		fd_set fds;
		struct timeval tv;
		int r;

		FD_ZERO (&fds);
		FD_SET (fd, &fds);

		/* Timeout. */
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select (fd + 1, &fds, NULL, NULL, &tv);
		if (-1 == r) {
			if (EINTR == errno) {
				perror ("select");
				return -errno;
			}
		}

		if (0 == r) {
			fprintf (stderr, "select timeout\n");
			return -errno;
		}

		return v4l2_rcvbuf(drv, recebe_buffer);
	} else {
		int size=read(fd, outbuf, sizeof(outbuf));
		return size;
	}

	return 0;
}

static int read_char (struct v4l2_driver *drv, int fd)
{
	static int sizebuf=0;
	static unsigned char *p=NULL;
	unsigned char c;

	if (sizebuf<=0) {
		sizebuf=read_stream(drv,fd);
		if (sizebuf<=0)
			return -1;
		p=(unsigned char *)outbuf;
	}
	c=*p;
	p++;
	sizebuf--;

	return c;
}


int main (int argc, char*argv[])
{
	int fd;
	unsigned int i;
	unsigned char buf[TM6000_URB_MSG_LEN], img[720*2*480];
	unsigned int  cmd, size, field, block, line, pos=0;
	unsigned long header=0;
	int           linesize=720*2,skip=0;
	struct v4l2_driver drv;

	argp_parse (&argp, argc, argv, 0, 0, 0);

	if (mode!=INPUT) {
		if (prepare_read (&drv)<0)
			return -1;
		fd=drv.fd;
	} else {
		/*mode == INPUT */

		fd=open(filename,O_RDONLY);
		if (fd<0) {
			perror ("error opening a file for parsing");
			return -1;
		}
		dprintf("file %s opened for parsing\n",filename);
		use_mmap=0;
	}

	if (mode==OUTPUT) {
		fout=fopen(filename,"w");
		if (!fout) {
			perror ("error opening a file to write");
			return -1;
		}
		dprintf("file %s opened for output\n",filename);

		do {
			size=read_stream (&drv,fd);

			if (size<=0) {
				close (fd);
				return -1;
			}
			dprintf("writing %d bytes\n",size);
			fwrite(outbuf,1, size,fout);
//			fflush(fout);
		} while (1);
	}


	while (1) {
		skip=0;
		header=0;
		do {
			int c;
			c=read_char (&drv,fd);
			if (c<0) {
				perror("read");
				return -1;
			}

			header=(header>>8)&0xffffff;
			header=header|(c<<24);
			skip++;
		} while ( (((header>>24)&0xff) != 0x47) );

		/* split the header fields */
		size  = (((header & 0x7e)<<1) -1) * 4;
		block = (header>>7) & 0xf;
		field = (header>>11) & 0x1;
		line  = (header>>12) & 0x1ff;
		cmd   = (header>>21) & 0x7;

		/* Read the remaining buffer */
		for (i=0;i<sizeof(buf);i++) {
			int c;
			c=read_char (&drv,fd);
			if (c<0) {
				perror("read");
				return -1;
			}
			buf[i]=c;
		}

		/* FIXME: Mounts the image as field0+field1
			* It should, instead, check if the user selected
			* entrelaced or non-entrelaced mode
			*/
		pos=((line<<1)+field)*linesize+
					block*TM6000_URB_MSG_LEN;

			/* Prints debug info */
		dprintf("0x%08x (skip %d), %s size=%d, line=%d, field=%d, block=%d\n",
				(unsigned int)header, skip,
				 tm6000_msg_type[cmd],
				 size, line, field, block);

		/* Don't allow to write out of the buffer */
		if (pos+sizeof(buf) > sizeof(img))
			cmd = TM6000_URB_MSG_ERR;

		/* handles each different URB message */
		switch(cmd) {
		case TM6000_URB_MSG_VIDEO:
			/* Fills video buffer */
			memcpy(buf,&img[pos],sizeof(buf));
		case TM6000_URB_MSG_AUDIO:
			if (audio)
				fwrite(buf,sizeof(buf),1,stdout);
//		case TM6000_URB_MSG_VBI:
//		case TM6000_URB_MSG_PTS:
		break;
		}
	}
	close(fd);
	return 0;
}
