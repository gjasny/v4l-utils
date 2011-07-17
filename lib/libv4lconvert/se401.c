/*
#             (C) 2011 Hans de Goede <hdegoede@redhat.com>

# The compression algorithm has been taken from the v4l1 se401 linux kernel
# driver by Jeroen B. Vreeken (pe1rxq@amsat.org)

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

#include "libv4lconvert-priv.h"
#include <errno.h>

/* The se401 compression algorithm uses a fixed quant factor, which
   can be configured by setting the high nibble of the SE401_OPERATINGMODE
   feature. This needs to exactly match what is in the SE401 driver! */
#define SE401_QUANT_FACT 8

static void wr_pixel(int p, uint8_t **dest, int pitch, int *x)
{
	int i = *x;

	/* First 3 pixels of each line are absolute */
	if (i < 3) {
		(*dest)[i] = p * SE401_QUANT_FACT;
	} else {
		(*dest)[i] = (*dest)[i - 3] + p * SE401_QUANT_FACT;
	}

	*x += 1;
	if (*x == pitch) {
		*x = 0;
		*dest += pitch;
	}
}

enum decode_state {
	get_len,
	sign_bit,
	other_bits,
};

static int decode_JangGu(const uint8_t *data, int bits, int plen, int pixels,
			 uint8_t **dest, int pitch, int *x)
{
	enum decode_state state = get_len;
	int len = 0;
	int value = 0;
	int bitnr;
	int bit;

	while (plen) {
		bitnr = 8;
		while (bitnr && bits) {
			bit = ((*data) >> (bitnr-1))&1;
			switch (state) {
			case get_len:
				if (bit) {
					len++;
				} else {
					if (!len) {
						wr_pixel(0, dest, pitch, x);
						if (!--pixels)
							return 0;
					} else {
						state = sign_bit;
						value = 0;
					}
				}
				break;
			case sign_bit:
				if (bit)
					value = 0;
				else
					value = -(1 << len) + 1;
				state = other_bits;
				/* fall through for positive number and
				   len == 1 handling */
			case other_bits:
				len--;
				value += bit << len;
				if (len == 0) {
					/* Done write pixel and get bit len of
					   the next one */
					state = get_len;
					wr_pixel(value, dest, pitch, x);
					if (!--pixels)
						return 0;
				}
				break;
			}
			bitnr--;
			bits--;
		}
		data++;
		plen--;
	}
	return -1;
}

int v4lconvert_se401_to_rgb24(struct v4lconvert_data *data,
		const unsigned char *src, int src_size,
		unsigned char *dest, int width, int height)
{
	int in, plen, bits, pixels, info;
	int x = 0, total_pixels = 0;

	for (in = 0; in + 4 < src_size; in += plen) {
		bits   = src[in + 3] + (src[in + 2] << 8);
		pixels = src[in + 1] + ((src[in + 0] & 0x3f) << 8);
		info   = (src[in + 0] & 0xc0) >> 6;
		plen   = ((bits + 47) >> 4) << 1;
		/* Sanity checks */
		if (plen > 1024) {
			V4LCONVERT_ERR("invalid se401 packet len %d", plen);
			goto error;
		}
		if (in + plen > src_size) {
			V4LCONVERT_ERR("incomplete se401 packet");
			goto error;
		}
		if (total_pixels + pixels > width * height) {
			V4LCONVERT_ERR("se401 frame overflow");
			goto error;
		}
		/* info: 0 inter packet, 1 eof, 2 sof, 3 not used */
		if ((in == 0 && info != 2) ||
		    (in > 0 && in + plen < src_size && info != 0) ||
		    (in + plen == src_size && info != 1)) {
			V4LCONVERT_ERR("invalid se401 frame info value");
			goto error;
		}
		if (decode_JangGu(&src[in + 4], bits, plen, pixels * 3,
				  &dest, width * 3, &x)) {
			V4LCONVERT_ERR("short se401 packet");
			goto error;
		}
		total_pixels += pixels;
	}

	if (in != src_size || total_pixels != width * height) {
		V4LCONVERT_ERR("se401 frame size mismatch");
		goto error;
	}

	return 0;

error:
	errno = EIO;
	return -1;
}
