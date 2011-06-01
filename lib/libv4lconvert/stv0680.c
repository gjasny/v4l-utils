/*
 * stv0680.c
 *
 * Copyright (c) 2009 Hans de Goede <hdegoede@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#include "libv4lconvert-priv.h"

/* The stv0640 first sends all the red/green pixels for a line (so 1/2 width)
   and then all the green/blue pixels in that line, shuffle this to a regular
   RGGB bayer pattern. */
void v4lconvert_decode_stv0680(const unsigned char *src, unsigned char *dst,
		int width, int height)
{
	int x, y;
	const unsigned char *src1 = src;
	const unsigned char *src2 = src + width / 2;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width / 2; x++) {
			*dst++ = *src1++;
			*dst++ = *src2++;
		}
		src1 += width / 2;
		src2 += width / 2;
	}
}
