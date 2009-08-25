/*
 * sn9c2028-decomp.c
 *
 * Decompression function for the Sonix SN9C2028 dual-mode cameras.
 *
 * Code adapted from libgphoto2/camlibs/sonix, original version of which was
 * Copyright (c) 2005 Theodore Kilgore <kilgota@auburn.edu>
 *
 * History:
 *
 * This decoding algorithm originates from the work of Bertrik Sikken for the
 * SN9C102 cameras. This version is an adaptation of work done by Mattias
 * Krauss for the webcam-osx (macam) project. There, it was further adapted
 * for use with the Vivitar Vivicam 3350B (an SN9C2028 camera) by
 * Harald Ruda <hrx@users.sourceforge.net>. Harald brought to my attention the
 * work done in the macam project and suggested that I use it. One improvement
 * of my own was to notice that the even and odd columns of the image have been
 * reversed by the decompression algorithm, and this needs to be corrected
 * during the decompression.
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "libv4lconvert-priv.h"

/* Four defines for bitstream operations, used in the decode function */

#define PEEK_BITS(num,to) {\
    if (bitBufCount < num) {\
	do {\
	    bitBuf = (bitBuf << 8)|(*(src++));\
	    bitBufCount += 8; \
	} \
	while\
	    (bitBufCount < 24);\
    } \
    to = bitBuf >> (bitBufCount-num);\
}

/*
 * PEEK_BITS puts the next <num> bits into the low bits of <to>.
 * when the buffer is empty, it is completely refilled.
 * This strategy tries to reduce memory access. Note that the high bits
 * are NOT set to zero!
 */

#define EAT_BITS(num) { bitBufCount -= num; bits_eaten += num; }

/*
 * EAT_BITS consumes <num> bits (PEEK_BITS does not consume anything,
 * it just peeks)
 */

#define PARSE_PIXEL(val) {\
    PEEK_BITS(10, bits);\
    if ((bits&0x200) == 0) {\
	EAT_BITS(1);\
    } \
    else if ((bits&0x380) == 0x280) {\
	EAT_BITS(3);\
	val += 3;\
	if (val > 255)\
	    val = 255;\
    } \
    else if ((bits&0x380) == 0x300) {\
	EAT_BITS(3);\
	val -= 3;\
	if (val < 0)\
	    val = 0;\
    } \
    else if ((bits&0x3c0) == 0x200) {\
	EAT_BITS(4);\
	val += 8;\
	if (val > 255)\
	    val = 255;\
    } \
    else if ((bits&0x3c0) == 0x240) {\
	EAT_BITS(4);\
	val -= 8;\
	if (val < 0)\
	    val = 0;\
    } \
    else if ((bits&0x3c0) == 0x3c0) {\
	EAT_BITS(4);\
	val -= 20;\
	if (val < 0)\
	    val = 0;\
    } \
    else if ((bits&0x3e0) == 0x380) {\
	EAT_BITS(5);\
	val += 20;\
	if (val > 255)\
	    val = 255;\
    } \
    else {\
	EAT_BITS(10);\
	val = 8*(bits&0x1f)+0;\
    } \
}


#define PUT_PIXEL_PAIR {\
    long pp;\
    pp = (c1val<<8)+c2val;\
    *((unsigned short *) (dst+dst_index)) = pp;\
    dst_index += 2;\
}

/* Now the decode function itself */

void v4lconvert_decode_sn9c2028(const unsigned char *src, unsigned char *dst,
  int width, int height)
{
    long dst_index = 0;
    int starting_row = 0;
    unsigned short bits;
    short c1val, c2val;
    int x, y;
    unsigned long bitBuf = 0;
    unsigned long bitBufCount = 0;
    unsigned long bits_eaten = 0;

    src += 12;    /* Remove the header */

    for (y = starting_row; y < height; y++) {
	PEEK_BITS(8, bits);
	EAT_BITS(8);
	c2val = (bits & 0xff);
	PEEK_BITS(8, bits);
	EAT_BITS(8);
	c1val = (bits & 0xff);

	PUT_PIXEL_PAIR;

	for (x = 2; x < width ; x += 2) {
	    /* The compression reversed the even and odd columns.*/
	    PARSE_PIXEL(c2val);
	    PARSE_PIXEL(c1val);
	    PUT_PIXEL_PAIR;
	}
    }
}
