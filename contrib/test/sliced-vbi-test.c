/*
    Sliced vbi demonstration utility
    Copyright (C) 2004  Hans Verkuil  <hverkuil@xs4all.nl>

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
    Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

/* This test tool is used to test the sliced VBI implementation. It reads
   from /dev/vbi0 by default (or the device name that is specified as the
   first argument) and shows which packets arrive and what the contents
   is. It also serves as example code on how to use the sliced VBI API.
 */

#include <unistd.h>
#include <features.h>		/* Uses _GNU_SOURCE to define getsubopt in stdlib.h */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <math.h>

#include <linux/videodev2.h>

#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

int valid_char(char c);

int valid_char(char c)
{
	/* Invalid Character */
	if (((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E)
		return 0;
	else			/* Valid Character */
		return 1;
}

int frames = 0;
int lines = 0;
int space_needed = 0;
int text_off = 0;

static const char *formats[] = {
	"Full format 4:3, 576 lines",
	"Letterbox 14:9 centre, 504 lines",
	"Letterbox 14:9 top, 504 lines",
	"Letterbox 16:9 centre, 430 lines",
	"Letterbox 16:9 top, 430 lines",
	"Letterbox > 16:9 centre",
	"Full format 14:9 centre, 576 lines",
	"Anamorphic 16:9, 576 lines"
};
static const char *subtitles[] = {
	"none",
	"in active image area",
	"out of active image area",
	"?"
};

static void decode_wss(struct v4l2_sliced_vbi_data *s)
{
	unsigned char parity;
	int wss;

	wss = s->data[0] | (s->data[1] << 8);

	parity = wss & 15;
	parity ^= parity >> 2;
	parity ^= parity >> 1;

	if (!(parity & 1))
		return;

	printf("WSS: %s; %s mode; %s color coding;\n"
	       "      %s helper; reserved b7=%d; %s\n"
	       "      open subtitles: %s; %scopyright %s; copying %s\n",
	       formats[wss & 7],
	       (wss & 0x10) ? "film" : "camera",
	       (wss & 0x20) ? "MA/CP" : "standard",
	       (wss & 0x40) ? "modulated" : "no",
	       !!(wss & 0x80),
	       (wss & 0x0100) ? "have TTX subtitles; " : "",
	       subtitles[(wss >> 9) & 3],
	       (wss & 0x0800) ? "surround sound; " : "",
	       (wss & 0x1000) ? "asserted" : "unknown",
	       (wss & 0x2000) ? "restricted" : "not restricted");
}

static int odd_parity(uint8_t c)
{
	c ^= (c >> 4);
	c ^= (c >> 2);
	c ^= (c >> 1);

	return c & 1;
}

static void decode_xds(struct v4l2_sliced_vbi_data *s)
{
	char c;

	//printf("XDS: %02x %02x: ", s->data[0], s->data[1]);
	c = odd_parity(s->data[0]) ? s->data[0] & 0x7F : '?';
	c = printable(c);
	//putchar(c);
	c = odd_parity(s->data[1]) ? s->data[1] & 0x7F : '?';
	c = printable(c);
	//putchar(c);
	//putchar('\n');
}

#define CC_SIZE 64

static void decode_cc(struct v4l2_sliced_vbi_data *s)
{
	static int xds_transport = 0;
	char c = s->data[0] & 0x7F;
	static char cc[CC_SIZE + 1];
	static char cc_last[2 + 1] = { 0, 0 };
	char cc_disp[CC_SIZE + 1];
	static int cc_idx;

	if (s->field) {	/* field 2 */
		/* 0x01xx..0x0Exx ASCII_or_NUL[0..32] 0x0Fchks */
		if (odd_parity(s->data[0]) && (c >= 0x01 && c <= 0x0F)) {
			decode_xds(s);
			xds_transport = (c != 0x0F);
		} else if (xds_transport) {
			decode_xds(s);
		}
		return;
	}

	if (s->data[0] == 0x10 ||
	    s->data[0] == 0x13 ||
	    s->data[0] == 0x15 ||
	    s->data[0] == 0x16 ||
	    s->data[0] == 0x91 ||
	    s->data[0] == 0x92 ||
	    s->data[0] == 0x94 || s->data[0] == 0x97 || s->data[0] == 0x1c) {
		if (text_off) {
			if (s->data[0] == 0x94 &&
			    (s->data[1] == 0xad || s->data[1] == 0x25)) {
				text_off = 0;
			}
		} else {
			if (s->data[0] == 0x1c &&
			    (s->data[1] == 0x2a || s->data[1] == 0xab)) {
				text_off = 1;
			}
		}
	}

	if (text_off == 0) {
		c = odd_parity(s->data[0]) ? s->data[0] & 0x7F : '?';

		if (cc_idx >= CC_SIZE) {
			cc_idx = CC_SIZE - 2;
			memcpy(cc, cc + 2, cc_idx);
		}
		cc[cc_idx++] = c;

		c = odd_parity(s->data[1]) ? s->data[1] & 0x7F : '?';

		cc[cc_idx++] = c;

		cc[cc_idx] = 0;
	}

	if (cc_idx == CC_SIZE) {
		int x = 0, y = 0;
		int debug = 0;

		memset(cc_disp, 0, CC_SIZE);

		if (debug)
			fprintf(stderr, "\n");
		for (y = 0, x = 0; y < cc_idx;) {

			/* Control Code or Valid Character */
			if (valid_char(cc[y]) == 0) {
				if (debug) {
					if (cc[y] == 0x00)
						fprintf(stderr, "()");
					else
						fprintf(stderr, "(0x%02x)",
							cc[y]);
				}

				/* skip over control code */
				if (cc[y] >= 0x11 && cc[y] <= 0x1f) {
					if (debug) {
						if (cc[y + 1] == 0x00)
							fprintf(stderr, "()");
						else
							fprintf(stderr,
								"(0x%02x)",
								cc[y + 1]);
					}

					if (space_needed == 1) {
						space_needed = 0;
						cc_disp[x++] = ' ';
						lines++;
					} else if (cc[y] == 0x14
						   && cc[y + 1] == 0x14) {
						space_needed = 0;
						cc_disp[x++] = ' ';
						lines++;
					}

					cc_last[0] = cc[y];
					cc_last[1] = cc[y + 1];
					y += 2;
				} else {
					cc_last[0] = cc_last[1];
					cc_last[1] = cc[y];
					y++;
				}
			} else {
				if (debug)
					fprintf(stderr, "(%c)", cc[y] & 0x7F);

				/* Record character */
				if ((cc[y] & 0x7F) == '\n') {
					cc_disp[x] = ' ';
					lines++;
				} else if (cc_last[1] == 0x2B
					   && cc_last[0] == 0x14
					   && (cc[y] & 0x7F) == '@') {
					/* Do Nothing */
					cc_last[0] = cc_last[1];
					cc_last[1] = cc[y];
					y++;
					continue;
				} else if ((cc[y] & 0x7F) != '\n') {
					cc_disp[x] = cc[y] & 0x7F;
					lines++;
				} else {
					printf("\nOdd Character (%c)\n",
					       cc[y] & 0x7F);
				}

				space_needed = 1;
				x++;
				cc_last[0] = cc_last[1];
				cc_last[1] = cc[y];
				y++;
			}

			/* Insert CC_SIZE char Line Break */
			if (lines >= CC_SIZE && cc_disp[x - 1] == ' ') {
				cc_disp[x++] = '\n';
				lines = 0;
				space_needed = 0;
			}
		}
		if (debug)
			fprintf(stderr, "\n");
		printf("%s", cc_disp);
		memset(cc_disp, 0, CC_SIZE);
		//memset(cc, 0, CC_SIZE);

		cc_idx = 0;
	}
}

const uint8_t vbi_bit_reverse[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

#define PIL(day, mon, hour, min) \
	(((day) << 15) + ((mon) << 11) + ((hour) << 6) + ((min) << 0))

static void dump_pil(int pil)
{
	int day, mon, hour, min;

	day = pil >> 15;
	mon = (pil >> 11) & 0xF;
	hour = (pil >> 6) & 0x1F;
	min = pil & 0x3F;

	if (pil == PIL(0, 15, 31, 63))
		printf(" PDC: Timer-control (no PDC)\n");
	else if (pil == PIL(0, 15, 30, 63))
		printf(" PDC: Recording inhibit/terminate\n");
	else if (pil == PIL(0, 15, 29, 63))
		printf(" PDC: Interruption\n");
	else if (pil == PIL(0, 15, 28, 63))
		printf(" PDC: Continue\n");
	else if (pil == PIL(31, 15, 31, 63))
		printf(" PDC: No time\n");
	else
		printf(" PDC: %05x, 200X-%02d-%02d %02d:%02d\n",
		       pil, mon, day, hour, min);
}

static void decode_vps(struct v4l2_sliced_vbi_data *s)
{
	static char pr_label[20];
	static char label[20];
	static int l = 0;
	int cni, pcs, pty, pil;
	int c;
	unsigned char *buf = s->data;

	c = vbi_bit_reverse[buf[1]];

	if ((int8_t) c < 0) {
		label[l] = 0;
		memcpy(pr_label, label, sizeof(pr_label));
		l = 0;
	}

	c &= 0x7F;

	label[l] = printable(c);

	l = (l + 1) % 16;

	printf("VPS: 3-10: %02x %02x %02x %02x %02x %02x %02x %02x (\"%s\")\n",
	       buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
	       pr_label);

	pcs = buf[2] >> 6;

	cni = +((buf[10] & 3) << 10)
	    + ((buf[11] & 0xC0) << 2)
	    + ((buf[8] & 0xC0) << 0)
	    + (buf[11] & 0x3F);

	pil = ((buf[8] & 0x3F) << 14) + (buf[9] << 6) + (buf[10] >> 2);

	pty = buf[12];

	printf("      CNI: %04x PCS: %d PTY: %d ", cni, pcs, pty);

	dump_pil(pil);
}

static void process(struct v4l2_sliced_vbi_data *s)
{
	if (s->id == 0)
		return;

	//printf("%04d: line %02u field %d type %x\n", frames, s->line, s->field, s->id);
	switch (s->id) {
	case V4L2_SLICED_TELETEXT_B:
		printf("teletext\n");
		break;
	case V4L2_SLICED_VPS:
		if (s->line != 16 || s->field)
			break;
		decode_vps(s);
		break;
	case V4L2_SLICED_WSS_625:
		if (s->line != 23 || s->field)
			break;
		decode_wss(s);
		break;
	case V4L2_SLICED_CAPTION_525:
		if (s->line != 21)
			break;
		decode_cc(s);
		break;
	default:
		printf("unknown\n");
		break;
	}
}

int main(int argc, char **argv)
{
	char *device = "/dev/vbi0";
	struct v4l2_format fmt;
	v4l2_std_id std;
	struct v4l2_sliced_vbi_data *buf;
	int fh;

	if (argc == 2)
		device = argv[1];
	fh = open(device, O_RDONLY);

	if (fh == -1) {
		fprintf(stderr, "cannot open %s\n", device);
		return 1;
	}

	setbuf(stdout, NULL);

	ioctl(fh, VIDIOC_G_STD, &std);
	fmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
	fmt.fmt.sliced.service_set = (std & V4L2_STD_NTSC) ? V4L2_SLICED_VBI_525 : V4L2_SLICED_VBI_625;
	fmt.fmt.sliced.reserved[0] = 0;
	fmt.fmt.sliced.reserved[1] = 0;
	if (ioctl(fh, VIDIOC_S_FMT, &fmt) < 0) {
		perror("vbi");
		close(fh);
		return 1;
	}

	fprintf(stderr, "%08x, %d\n", fmt.fmt.sliced.service_set, fmt.fmt.sliced.io_size);
	buf = malloc(fmt.fmt.sliced.io_size);
	for (;;) {
		int size = read(fh, buf, fmt.fmt.sliced.io_size);
		unsigned i;

		if (size <= 0)
			break;
		frames++;
		for (i = 0; i < size / sizeof(struct v4l2_sliced_vbi_data); i++) {
			process(&buf[i]);
		}
	}
	close(fh);
	return 0;
}
