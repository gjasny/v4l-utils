/*
 * Sonix SN9C20X decoder
 * Vasily Khoruzhick, (C) 2008-2009
 * Algorithm based on Java code written by Jens on microdia google group
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Note this code was originally licensed under the GNU GPL instead of the
 * GNU LGPL, its license has been changed by its author.
 */

#include "libv4lconvert-priv.h"

#define DO_SANITY_CHECKS 0

static const int UVTranslate[32] = {0, 1, 2, 3,
			8, 9, 10, 11,
			16, 17, 18, 19,
			24, 25, 26, 27,
			4, 5, 6, 7,
			12, 13, 14, 15,
			20, 21, 22, 23,
			28, 29, 30, 31};

static const int Y_coords_624x[128][2] = {
{ 0,  0}, { 1,  0}, { 2,  0}, { 3,  0}, { 4,  0}, { 5,  0}, { 6,  0}, { 7,  0},
{ 0,  1}, { 1,  1}, { 2,  1}, { 3,  1}, { 4,  1}, { 5,  1}, { 6,  1}, { 7,  1},
{ 0,  2}, { 1,  2}, { 2,  2}, { 3,  2}, { 4,  2}, { 5,  2}, { 6,  2}, { 7,  2},
{ 0,  3}, { 1,  3}, { 2,  3}, { 3,  3}, { 4,  3}, { 5,  3}, { 6,  3}, { 7,  3},

{ 0,  4}, { 1,  4}, { 2,  4}, { 3,  4}, { 4,  4}, { 5,  4}, { 6,  4}, { 7,  4},
{ 0,  5}, { 1,  5}, { 2,  5}, { 3,  5}, { 4,  5}, { 5,  5}, { 6,  5}, { 7,  5},
{ 0,  6}, { 1,  6}, { 2,  6}, { 3,  6}, { 4,  6}, { 5,  6}, { 6,  6}, { 7,  6},
{ 0,  7}, { 1,  7}, { 2,  7}, { 3,  7}, { 4,  7}, { 5,  7}, { 6,  7}, { 7,  7},

{ 8,  0}, { 9,  0}, {10,  0}, {11,  0}, {12,  0}, {13,  0}, {14,  0}, {15,  0},
{ 8,  1}, { 9,  1}, {10,  1}, {11,  1}, {12,  1}, {13,  1}, {14,  1}, {15,  1},
{ 8,  2}, { 9,  2}, {10,  2}, {11,  2}, {12,  2}, {13,  2}, {14,  2}, {15,  2},
{ 8,  3}, { 9,  3}, {10,  3}, {11,  3}, {12,  3}, {13,  3}, {14,  3}, {15,  3},

{ 8,  4}, { 9,  4}, {10,  4}, {11,  4}, {12,  4}, {13,  4}, {14,  4}, {15,  4},
{ 8,  5}, { 9,  5}, {10,  5}, {11,  5}, {12,  5}, {13,  5}, {14,  5}, {15,  5},
{ 8,  6}, { 9,  6}, {10,  6}, {11,  6}, {12,  6}, {13,  6}, {14,  6}, {15,  6},
{ 8,  7}, { 9,  7}, {10,  7}, {11,  7}, {12,  7}, {13,  7}, {14,  7}, {15,  7}
};

static void do_write_u(const unsigned char *buf, unsigned char *ptr,
	int i, int j)
{
	*ptr = buf[i + 128 + j];
}

static void do_write_v(const unsigned char *buf, unsigned char *ptr,
	int i, int j)
{
	*ptr = buf[i + 160 + j];
}

void v4lconvert_sn9c20x_to_yuv420(const unsigned char *raw, unsigned char *i420,
  int width, int height, int yvu)
{
	int i = 0, x = 0, y = 0, j, relX, relY, x_div2, y_div2;
	const unsigned char *buf = raw;
	unsigned char *ptr;
	int frame_size = width * height;
	int frame_size_div2 = frame_size >> 1;
	int frame_size_div4 = frame_size >> 2;
	int width_div2 = width >> 1;
	int height_div2 = height >> 1;
	void (*do_write_uv1)(const unsigned char *buf, unsigned char *ptr, int i,
	int j) = NULL;
	void (*do_write_uv2)(const unsigned char *buf, unsigned char *ptr, int i,
	int j) = NULL;

	if (yvu) {
		do_write_uv1 = do_write_v;
		do_write_uv2 = do_write_u;
	}
	else {
		do_write_uv1 = do_write_u;
		do_write_uv2 = do_write_v;
	}

	while (i < (frame_size + frame_size_div2)) {
		for (j = 0; j < 128; j++) {
			relX = x + Y_coords_624x[j][0];
			relY = y + Y_coords_624x[j][1];

#if (DO_SANITY_CHECKS==1)
			if ((relX < width) && (relY < height)) {
#endif
				ptr = i420 + relY * width + relX;
				*ptr = buf[i + j];
#if (DO_SANITY_CHECKS==1)
			}
#endif

		}
		x_div2 = x >> 1;
		y_div2 = y >> 1;
		for (j = 0; j < 32; j++) {
			relX = (x_div2) + (j & 0x07);
			relY = (y_div2) + (j >> 3);

#if (DO_SANITY_CHECKS==1)
			if ((relX < width_div2) && (relY < height_div2)) {
#endif
				ptr = i420 + frame_size +
					relY * width_div2 + relX;
				do_write_uv1(buf, ptr, i, j);
				ptr += frame_size_div4;
				do_write_uv2(buf, ptr, i, j);
#if (DO_SANITY_CHECKS==1)
			}
#endif
		}

		i += 192;
		x += 16;
		if (x >= width) {
			x = 0;
			y += 8;
		}
	}
}
