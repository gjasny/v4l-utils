/*
#             (C) 2010 Hans de Goede <hdegoede@redhat.com>

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
#include <string.h>

#define MAGIC_0		0x19
#define MAGIC_1		0x68
#define SUBSAMPLE_420	0
#define SUBSAMPLE_422	1
#define YUVORDER_YUYV	0
#define YUVORDER_UYVY	1
#define NOT_COMPRESSED	0
#define COMPRESSED	1
#define NO_DECIMATION	0
#define DECIMATION_ENAB	1
#define EOI		0xff	/* End Of Image */
#define EOL		0xfd	/* End Of Line */
#define FRAME_HEADER_SIZE	64

/* CPIA YUYV (sometimes sort of compressed) */
int v4lconvert_cpia1_to_yuv420(struct v4lconvert_data *data,
		const unsigned char *src, int src_size,
		unsigned char *dest, int width, int height, int yvu)
{
	int x, y, ll, compressed;
	unsigned char *udest, *vdest;

	if (width > 352 || height > 288) {
		fprintf(stderr, "FATAL ERROR CPIA1 size > 352x288, please report!\n");
		return -1;
	}

	if (data->previous_frame == NULL) {
		data->previous_frame = malloc(352 * 288 * 3 / 2);
		if (data->previous_frame == NULL) {
			fprintf(stderr, "cpia1 decode error: could not allocate buffer!\n");
			return -1;
		}
	}

	if (yvu) {
		vdest = dest + width * height;
		udest = vdest + width * height / 4;
	} else {
		udest = dest + width * height;
		vdest = udest + width * height / 4;
	}

	/* Verify header */
	if (src_size < FRAME_HEADER_SIZE ||
			src[0] != MAGIC_0 || src[1] != MAGIC_1 ||
			src[17] != SUBSAMPLE_420 ||
			src[18] != YUVORDER_YUYV ||
			(src[25] - src[24]) * 8 != width ||
			(src[27] - src[26]) * 4 != height ||
			(src[28] != NOT_COMPRESSED && src[28] != COMPRESSED) ||
			(src[29] != NO_DECIMATION && src[29] != DECIMATION_ENAB)) {
		fprintf(stderr, "cpia1 decode error: invalid header\n");
		return -1;
	}

	if (src[29] == DECIMATION_ENAB) {
		fprintf(stderr, "cpia1 decode error: decimation is not supported\n");
		return -1;
	}

	compressed = src[28] == COMPRESSED;

	src += FRAME_HEADER_SIZE;
	src_size -= FRAME_HEADER_SIZE;

	if (!compressed) {
		for (y = 0; y < height && src_size > 2; y++) {
			ll = src[0] | (src[1] << 8);
			src += 2;
			src_size -= 2;
			if (src_size < ll) {
				fprintf(stderr, "cpia1 decode error: short frame\n");
				return -1;
			}
			if (src[ll - 1] != EOL) {
				fprintf(stderr, "cpia1 decode error: invalid terminated line\n");
				return -1;
			}

			if (!(y & 1)) { /* Even line Y + UV in the form of YUYV */
				if (ll != 2 * width + 1) {
					fprintf(stderr, "cpia1 decode error: invalid uncompressed even ll\n");
					return -1;
				}

				/* copy the Y values */
				for (x = 0; x < width; x += 2) {
					*dest++ = src[0];
					*dest++ = src[2];
					src += 4;
				}

				/* copy the UV values */
				src -= 2 * width;
				for (x = 0; x < width; x += 2) {
					*udest++ = src[1];
					*vdest++ = src[3];
					src += 4;
				}
			} else { /* Odd line only Y values */
				if (ll != width + 1) {
					fprintf(stderr, "cpia1 decode error: invalid uncompressed odd ll\n");
					return -1;
				}

				memcpy(dest, src, width);
				dest += width;
				src += width;
			}
			src++; /* Skip EOL */
			src_size -= ll;
		}
	} else { /* compressed */
		/* Pre-fill dest with previous frame, as the cpia1 "compression" consists
		   of simply ommitting certain pixels */
		memcpy(dest, data->previous_frame, width * height * 3 / 2);

		for (y = 0; y < height && src_size > 2; y++) {
			ll = src[0] | (src[1] << 8);
			src += 2;
			src_size -= 2;
			if (src_size < ll) {
				fprintf(stderr, "cpia1 decode error: short frame\n");
				return -1;
			}
			if (src[ll - 1] != EOL) {
				fprintf(stderr, "cpia1 decode error: invalid terminated line\n");
				return -1;
			}

			/* Do this now as we use ll as loop variable below */
			src_size -= ll;
			for (x = 0; x < width && ll > 1; ) {
				if (*src & 1) { /* skip N pixels */
					int skip = *src >> 1;

					if (skip & 1) {
						fprintf(stderr, "cpia1 decode error: odd number of pixels to skip");
						return -1;
					}

					if (!(y & 1)) { /* Even line Y + UV in the form of YUYV */
						dest += skip;
						udest += skip / 2;
						vdest += skip / 2;
					} else { /* Odd line only Y values */
						dest += skip;
					}
					x += skip;
					src++;
					ll--;
				} else {
					if (!(y & 1)) { /* Even line Y + UV in the form of YUYV */
						*dest++ = *src++;
						*udest++ = *src++;
						*dest++ = *src++;
						*vdest++ = *src++;
						ll -= 4;
					} else { /* Odd line only Y values */
						*dest++ = *src++;
						*dest++ = *src++;
						ll -= 2;
					}
					x += 2;
				}
			}
			if (ll != 1 || x != width) {
				fprintf(stderr, "cpia1 decode error: line length mismatch\n");
				return -1;
			}
			src++; /* Skip EOL */
		}
	}

	if (y != height) {
		fprintf(stderr, "cpia1 decode error: frame height mismatch\n");
		return -1;
	}

	if (src_size < 4 || src[src_size - 4] != EOI || src[src_size - 3] != EOI ||
			    src[src_size - 2] != EOI || src[src_size - 1] != EOI) {
		fprintf(stderr, "cpia1 decode error: invaled EOI marker\n");
		return -1;
	}

	/* Safe frame for decompression of the next frame */
	dest -= width * height;
	memcpy(data->previous_frame, dest, width * height * 3 / 2);

	return 0;
}
