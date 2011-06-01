/*
 * MR97310A decoder
 *
 * Copyright (C) 2004-2009 Theodore Kilgore <kilgota@auburn.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02111-1307, USA.
 */

#include <unistd.h>
#include "libv4lconvert-priv.h"
#include "libv4lsyscall-priv.h"

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 0xff) ? 0xff : (x))

#define MIN_CLOCKDIV_CID V4L2_CID_PRIVATE_BASE

/* FIXME not threadsafe */
static int decoder_initialized;

static struct {
	unsigned char is_abs;
	unsigned char len;
	signed char val;
} table[256];

static void init_mr97310a_decoder(void)
{
	int i;
	int is_abs, val, len;

	for (i = 0; i < 256; ++i) {
		is_abs = 0;
		val = 0;
		len = 0;
		if ((i & 0x80) == 0) {
			/* code 0 */
			val = 0;
			len = 1;
		} else if ((i & 0xe0) == 0xc0) {
			/* code 110 */
			val = -3;
			len = 3;
		} else if ((i & 0xe0) == 0xa0) {
			/* code 101 */
			val = 3;
			len = 3;
		} else if ((i & 0xf0) == 0x80) {
			/* code 1000 */
			val = 8;
			len = 4;
		} else if ((i & 0xf0) == 0x90) {
			/* code 1001 */
			val = -8;
			len = 4;
		} else if ((i & 0xf0) == 0xf0) {
			/* code 1111 */
			val = -20;
			len = 4;
		} else if ((i & 0xf8) == 0xe0) {
			/* code 11100 */
			val = 20;
			len = 5;
		} else if ((i & 0xf8) == 0xe8) {
			/* code 11101xxxxx */
			is_abs = 1;
			val = 0;  /* value is calculated later */
			len = 5;
		}
		table[i].is_abs = is_abs;
		table[i].val = val;
		table[i].len = len;
	}
	decoder_initialized = 1;
}

static inline unsigned char get_byte(const unsigned char *inp,
		unsigned int bitpos)
{
	const unsigned char *addr;

	addr = inp + (bitpos >> 3);
	return (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)));
}

int v4lconvert_decode_mr97310a(struct v4lconvert_data *data,
		const unsigned char *inp, int src_size,
		unsigned char *outp, int width, int height)
{
	int row, col;
	int val;
	int bitpos;
	unsigned char code;
	unsigned char lp, tp, tlp, trp;
	struct v4l2_control min_clockdiv = { .id = MIN_CLOCKDIV_CID };

	if (!decoder_initialized)
		init_mr97310a_decoder();

	/* remove the header */
	inp += 12;

	bitpos = 0;

	/* main decoding loop */
	for (row = 0; row < height; ++row) {
		col = 0;

		/* first two pixels in first two rows are stored as raw 8-bit */
		if (row < 2) {
			code = get_byte(inp, bitpos);
			bitpos += 8;
			*outp++ = code;

			code = get_byte(inp, bitpos);
			bitpos += 8;
			*outp++ = code;

			col += 2;
		}

		while (col < width) {
			/* get bitcode */
			code = get_byte(inp, bitpos);
			/* update bit position */
			bitpos += table[code].len;

			/* calculate pixel value */
			if (table[code].is_abs) {
				/* get 5 more bits and use them as absolute value */
				code = get_byte(inp, bitpos);
				val = (code & 0xf8);
				bitpos += 5;

			} else {
				/* value is relative to top or left pixel */
				val = table[code].val;
				lp = outp[-2];
				if (row > 1) {
					tlp = outp[-2 * width - 2];
					tp  = outp[-2 * width];
					trp = outp[-2 * width + 2];
				}
				if (row < 2) {
					/* top row: relative to left pixel */
					val += lp;
				} else if (col < 2) {
					/* left column: relative to top pixel */
					/* initial estimate */
					val += (tp + trp) / 2;
				} else if (col > width - 3) {
					/* left column: relative to top pixel */
					val += (tp + lp + tlp + 1) / 3;
					/* main area: weighted average of tlp, trp,
					 * lp, and tp */
				} else {
					tlp >>= 1;
					trp >>= 1;
					/* initial estimate for predictor */
					val += (lp + tp + tlp + trp + 1) / 3;
				}
			}
			/* store pixel */
			*outp++ = CLIP(val);
			++col;
		}

		/* src_size - 12 because of 12 byte footer */
		if (((bitpos - 1) / 8) >= (src_size - 12)) {
			data->frames_dropped++;
			if (data->frames_dropped == 3) {
				/* Tell the driver to go slower as
				   the compression engine is not able to
				   compress the image enough, we may
				   fail to do this because older
				   drivers don't support this */
				SYS_IOCTL(data->fd, VIDIOC_G_CTRL,
						&min_clockdiv);
				min_clockdiv.value++;
				SYS_IOCTL(data->fd, VIDIOC_S_CTRL,
						&min_clockdiv);
				/* We return success here, because if we
				   return failure for too many frames in a row
				   libv4l2 will return an error to the
				   application and some applications abort
				   on the first error received. */
				data->frames_dropped = 0;
				return 0;
			}
			V4LCONVERT_ERR("incomplete mr97310a frame\n");
			return -1;
		}
	}

	data->frames_dropped = 0;
	return 0;
}
