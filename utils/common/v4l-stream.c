/*
 * V4L2 run-length image encoder source
 *
 * Copyright 2016 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "v4l-stream.h"

/*
 * Since Bayer uses alternating lines of BG and GR color components
 * you cannot compare one line with the next to see if they are identical,
 * instead you need to look at two consecutive lines at a time.
 * So here we double the bytesperline value for Bayer formats.
 */
unsigned rle_calc_bpl(unsigned bpl, __u32 pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR10P:
	case V4L2_PIX_FMT_SGBRG10P:
	case V4L2_PIX_FMT_SGRBG10P:
	case V4L2_PIX_FMT_SRGGB10P:
	case V4L2_PIX_FMT_SBGGR10ALAW8:
	case V4L2_PIX_FMT_SGBRG10ALAW8:
	case V4L2_PIX_FMT_SGRBG10ALAW8:
	case V4L2_PIX_FMT_SRGGB10ALAW8:
	case V4L2_PIX_FMT_SBGGR10DPCM8:
	case V4L2_PIX_FMT_SGBRG10DPCM8:
	case V4L2_PIX_FMT_SGRBG10DPCM8:
	case V4L2_PIX_FMT_SRGGB10DPCM8:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_SBGGR16:
		return 2 * bpl;
	default:
		return bpl;
	}
}

void rle_decompress(__u8 *b, unsigned size, unsigned rle_size, unsigned bpl)
{
	__u32 magic_x = ntohl(V4L_STREAM_PACKET_FRAME_VIDEO_X_RLE);
	__u32 magic_y = ntohl(V4L_STREAM_PACKET_FRAME_VIDEO_Y_RLE);
	unsigned offset = size - rle_size;
	__u32 *dst = (__u32 *)b;
	__u32 *p = (__u32 *)(b + offset);
	__u32 *next_line = NULL;
	unsigned l = 0;
	unsigned i;

	if (size == rle_size)
		return;

	if (bpl & 3)
		bpl = 0;
	if (bpl == 0)
		magic_y = magic_x;

	for (i = 0; i < rle_size; i += 4, p++) {
		__u32 v = *p;
		__u32 n = 1;

		if (bpl && v == magic_y) {
			l = ntohl(*++p);
			i += 4;
			next_line = dst + bpl / 4;
			continue;
		} else if (v == magic_x) {
			v = *++p;
			n = ntohl(*++p);
			i += 8;
		}

		while (n--)
			*dst++ = v;

		if (dst == next_line) {
			while (l--) {
				memcpy(dst, dst - bpl / 4, bpl);
				dst += bpl / 4;
			}
			next_line = NULL;
		}
	}
}

unsigned rle_compress(__u8 *b, unsigned size, unsigned bpl)
{
	__u32 magic_x = ntohl(V4L_STREAM_PACKET_FRAME_VIDEO_X_RLE);
	__u32 magic_y = ntohl(V4L_STREAM_PACKET_FRAME_VIDEO_Y_RLE);
	__u32 magic_r = ntohl(V4L_STREAM_PACKET_FRAME_VIDEO_RPLC);
	__u32 *p = (__u32 *)b;
	__u32 *dst = p;
	unsigned i;

	/*
	 * Only attempt runlength encoding if b is aligned
	 * to a multiple of 4 bytes and if size is a multiple of 4.
	 */
	if (((unsigned long)b & 3) || (size & 3))
		return size;

	if (bpl & 3)
		bpl = 0;
	if (bpl == 0)
		magic_y = magic_x;

	for (i = 0; i < size; i += 4, p++) {
		unsigned n, max;

		if (bpl && i % bpl == 0) {
			unsigned l = 0;

			while (i + (l + 2) * bpl <= size &&
			       !memcmp(p, p + (l + 1) * (bpl / 4), bpl))
				l++;
			if (l) {
				*dst++ = magic_y;
				*dst++ = htonl(l);
				i += l * bpl - 4;
				p += (l * bpl / 4) - 1;
				continue;
			}
		}
		if (*p == magic_x || *p == magic_y) {
			*dst++ = magic_r;
			continue;
		}
		max = bpl ? bpl * (i / bpl + 1) : size;
		if (i >= max - 16) {
			*dst++ = *p;
			continue;
		}
		if (*p != p[1] || *p != p[2] || *p != p[3]) {
			*dst++ = *p;
			continue;
		}
		n = 4;

		while (i + n * 4 < max && *p == p[n])
			n++;
		*dst++ = magic_x;
		*dst++ = p[1];
		*dst++ = htonl(n);
		p += n - 1;
		i += n * 4 - 4;
	}
	return (__u8 *)dst - b;
}
