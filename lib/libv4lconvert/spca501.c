/*
#             (C) 2008 Hans de Goede <hdegoede@redhat.com>

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
# Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#include <string.h>
#include "libv4lconvert-priv.h"

/* YUYV per line */
void v4lconvert_spca501_to_yuv420(const unsigned char *src, unsigned char *dst,
		int width, int height, int yvu)
{
	int i, j;
	unsigned long *lsrc = (unsigned long *)src;

	for (i = 0; i < height; i += 2) {
		/* -128 - 127 --> 0 - 255 and copy first line Y */
		unsigned long *ldst = (unsigned long *)(dst + i * width);

		for (j = 0; j < width; j += sizeof(long)) {
			*ldst = *lsrc++;
			*ldst++ ^= 0x8080808080808080ULL;
		}

		/* -128 - 127 --> 0 - 255 and copy 1 line U */
		if (yvu)
			ldst = (unsigned long *)(dst + (width * height * 5) / 4 + i * width / 4);
		else
			ldst = (unsigned long *)(dst + width * height + i * width / 4);
		for (j = 0; j < width/2; j += sizeof(long)) {
			*ldst = *lsrc++;
			*ldst++ ^= 0x8080808080808080ULL;
		}

		/* -128 - 127 --> 0 - 255 and copy second line Y */
		ldst = (unsigned long *)(dst + i * width + width);
		for (j = 0; j < width; j += sizeof(long)) {
			*ldst = *lsrc++;
			*ldst++ ^= 0x8080808080808080ULL;
		}

		/* -128 - 127 --> 0 - 255 and copy 1 line V */
		if (yvu)
			ldst = (unsigned long *)(dst + width * height + i * width / 4);
		else
			ldst = (unsigned long *)(dst + (width * height * 5) / 4 + i * width / 4);
		for (j = 0; j < width/2; j += sizeof(long)) {
			*ldst = *lsrc++;
			*ldst++ ^= 0x8080808080808080ULL;
		}
	}
}

/* YYUV per line */
void v4lconvert_spca505_to_yuv420(const unsigned char *src, unsigned char *dst,
		int width, int height, int yvu)
{
	int i, j;
	unsigned long *lsrc = (unsigned long *)src;

	for (i = 0; i < height; i += 2) {
		/* -128 - 127 --> 0 - 255 and copy 2 lines of Y */
		unsigned long *ldst = (unsigned long *)(dst + i * width);

		for (j = 0; j < width*2; j += sizeof(long)) {
			*ldst = *lsrc++;
			*ldst++ ^= 0x8080808080808080ULL;
		}

		/* -128 - 127 --> 0 - 255 and copy 1 line U */
		if (yvu)
			ldst = (unsigned long *)(dst + (width * height * 5) / 4 + i * width / 4);
		else
			ldst = (unsigned long *)(dst + width * height + i * width / 4);
		for (j = 0; j < width/2; j += sizeof(long)) {
			*ldst = *lsrc++;
			*ldst++ ^= 0x8080808080808080ULL;
		}

		/* -128 - 127 --> 0 - 255 and copy 1 line V */
		if (yvu)
			ldst = (unsigned long *)(dst + width * height + i * width / 4);
		else
			ldst = (unsigned long *)(dst + (width * height * 5) / 4 + i * width / 4);
		for (j = 0; j < width/2; j += sizeof(long)) {
			*ldst = *lsrc++;
			*ldst++ ^= 0x8080808080808080ULL;
		}
	}
}

/* YUVY per line */
void v4lconvert_spca508_to_yuv420(const unsigned char *src, unsigned char *dst,
		int width, int height, int yvu)
{
	int i, j;
	unsigned long *lsrc = (unsigned long *)src;

	for (i = 0; i < height; i += 2) {
		/* -128 - 127 --> 0 - 255 and copy first line Y */
		unsigned long *ldst = (unsigned long *)(dst + i * width);
		for (j = 0; j < width; j += sizeof(long)) {
			*ldst = *lsrc++;
			*ldst++ ^= 0x8080808080808080ULL;
		}

		/* -128 - 127 --> 0 - 255 and copy 1 line U */
		if (yvu)
			ldst = (unsigned long *)(dst + (width * height * 5) / 4 + i * width / 4);
		else
			ldst = (unsigned long *)(dst + width * height + i * width / 4);
		for (j = 0; j < width/2; j += sizeof(long)) {
			*ldst = *lsrc++;
			*ldst++ ^= 0x8080808080808080ULL;
		}

		/* -128 - 127 --> 0 - 255 and copy 1 line V */
		if (yvu)
			ldst = (unsigned long *)(dst + width * height + i * width / 4);
		else
			ldst = (unsigned long *)(dst + (width * height * 5) / 4 + i * width / 4);
		for (j = 0; j < width/2; j += sizeof(long)) {
			*ldst = *lsrc++;
			*ldst++ ^= 0x8080808080808080ULL;
		}

		/* -128 - 127 --> 0 - 255 and copy second line Y */
		ldst = (unsigned long *)(dst + i * width + width);
		for (j = 0; j < width; j += sizeof(long)) {
			*ldst = *lsrc++;
			*ldst++ ^= 0x8080808080808080ULL;
		}
	}
}

/* Note this is not a spca specific format, bit it fits in this file in that
   it is another funny yuv format */
/* one line of Y then 1 line of VYUY */
void v4lconvert_cit_yyvyuy_to_yuv420(const unsigned char *src,
		unsigned char *ydest,
		int width, int height, int yvu)
{
	int x, y;
	unsigned char *udest, *vdest;

	if (yvu) {
		vdest = ydest + width * height;
		udest = vdest + (width * height) / 4;
	} else {
		udest = ydest + width * height;
		vdest = udest + (width * height) / 4;
	}

	for (y = 0; y < height; y += 2) {
		/* copy 1 line of Y */
		memcpy(ydest, src, width);
		src += width;
		ydest += width;

		/* Split one line of VYUY */
		for (x = 0; x < width; x += 2) {
			*vdest++ = *src++;
			*ydest++ = *src++;
			*udest++ = *src++;
			*ydest++ = *src++;
		}
	}
}

/* Note this is not a spca specific format, but it fits in this file in that
   it is another funny yuv format */
/* The konica gspca subdriver using cams send data in blocks of 256 pixels
   in YUV420 format. */
void v4lconvert_konica_yuv420_to_yuv420(const unsigned char *src,
		unsigned char *ydest,
		int width, int height, int yvu)
{
	int i, no_blocks;
	unsigned char *udest, *vdest;

	if (yvu) {
		vdest = ydest + width * height;
		udest = vdest + (width * height) / 4;
	} else {
		udest = ydest + width * height;
		vdest = udest + (width * height) / 4;
	}

	no_blocks = width * height / 256;
	for (i = 0; i < no_blocks; i++) {
		/* copy 256 Y pixels */
		memcpy(ydest, src, 256);
		src += 256;
		ydest += 256;

		/* copy 64 U pixels */
		memcpy(udest, src, 64);
		src += 64;
		udest += 64;

		/* copy 64 V pixels */
		memcpy(vdest, src, 64);
		src += 64;
		vdest += 64;
	}
}

/* And another not a spca specific format, but fitting in this file in that
   it is another funny yuv format */
/* Two lines of Y then 1 line of UV */
void v4lconvert_m420_to_yuv420(const unsigned char *src,
		unsigned char *ydest,
		int width, int height, int yvu)
{
	int x, y;
	unsigned char *udest, *vdest;

	if (yvu) {
		vdest = ydest + width * height;
		udest = vdest + (width * height) / 4;
	} else {
		udest = ydest + width * height;
		vdest = udest + (width * height) / 4;
	}

	for (y = 0; y < height; y += 2) {
		/* copy 2 lines of Y */
		memcpy(ydest, src, 2 * width);
		src += 2 * width;
		ydest += 2 * width;

		/* Split one line of UV */
		for (x = 0; x < width; x += 2) {
			*udest++ = *src++;
			*vdest++ = *src++;
		}
	}
}
