// SPDX-License-Identifier: LGPL-2.1-only
/*
 * V4L2 run-length image encoder source
 *
 * Copyright 2016 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "v4l-stream.h"
#include "codec-fwht.h"

#define MIN_WIDTH  64
#define MAX_WIDTH  4096
#define MIN_HEIGHT 64
#define MAX_HEIGHT 2160

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
	case V4L2_PIX_FMT_SGBRG16:
	case V4L2_PIX_FMT_SGRBG16:
	case V4L2_PIX_FMT_SRGGB16:
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
		}
		if (v == magic_x) {
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

struct codec_ctx *fwht_alloc(unsigned pixfmt, unsigned visible_width, unsigned visible_height,
			     unsigned coded_width, unsigned coded_height,
			     unsigned field, unsigned colorspace, unsigned xfer_func,
			     unsigned ycbcr_enc, unsigned quantization)
{
	struct codec_ctx *ctx;
	const struct v4l2_fwht_pixfmt_info *info = v4l2_fwht_find_pixfmt(pixfmt);
	unsigned int chroma_div;
	unsigned int size = coded_width * coded_height;

	// fwht expects macroblock alignment, check can be dropped once that
	// restriction is lifted.
	if (!info || coded_width % 8 || coded_height % 8)
		return NULL;

	ctx = malloc(sizeof(*ctx));
	if (!ctx)
		return NULL;
	ctx->state.coded_width = coded_width;
	ctx->state.coded_height = coded_height;
	ctx->state.visible_width = visible_width;
	ctx->state.visible_height = visible_height;
	ctx->state.stride = coded_width * info->bytesperline_mult;
	ctx->state.ref_stride = coded_width * info->luma_alpha_step;
	ctx->state.info = info;
	ctx->field = field;
	ctx->state.colorspace = colorspace;
	ctx->state.xfer_func = xfer_func;
	ctx->state.ycbcr_enc = ycbcr_enc;
	ctx->state.quantization = quantization;
	ctx->flags = 0;
	chroma_div = info->width_div * info->height_div;
	ctx->size = size;
	if (info->components_num == 4)
		ctx->size = 2 * size + 2 * (size / chroma_div);
	else if (info->components_num == 3)
		ctx->size = size + 2 * (size / chroma_div);
	ctx->state.ref_frame.buf = malloc(ctx->size);
	ctx->state.ref_frame.luma = ctx->state.ref_frame.buf;
	ctx->comp_max_size = ctx->size + sizeof(struct fwht_cframe_hdr);
	ctx->state.compressed_frame = malloc(ctx->comp_max_size);
	if (!ctx->state.ref_frame.luma || !ctx->state.compressed_frame) {
		free(ctx->state.ref_frame.luma);
		free(ctx->state.compressed_frame);
		free(ctx);
		return NULL;
	}
	if (info->components_num >= 3) {
		ctx->state.ref_frame.cb = ctx->state.ref_frame.luma + size;
		ctx->state.ref_frame.cr = ctx->state.ref_frame.cb + size / chroma_div;
	} else {
		ctx->state.ref_frame.cb = NULL;
		ctx->state.ref_frame.cr = NULL;
	}

	if (info->components_num == 4)
		ctx->state.ref_frame.alpha =
			ctx->state.ref_frame.cr + size / chroma_div;
	else
		ctx->state.ref_frame.alpha = NULL;
	ctx->state.gop_size = 10;
	ctx->state.gop_cnt = 0;
	return ctx;
}

void fwht_free(struct codec_ctx *ctx)
{
	free(ctx->state.ref_frame.luma);
	free(ctx->state.compressed_frame);
	free(ctx);
}

__u8 *fwht_compress(struct codec_ctx *ctx, __u8 *buf, unsigned uncomp_size, unsigned *comp_size)
{
	ctx->state.i_frame_qp = ctx->state.p_frame_qp = 20;
	*comp_size = v4l2_fwht_encode(&ctx->state, buf, ctx->state.compressed_frame);
	return ctx->state.compressed_frame;
}

static void copy_cap_to_ref(const u8 *cap, const struct v4l2_fwht_pixfmt_info *info,
			    struct v4l2_fwht_state *state)
{
	int plane_idx;
	u8 *p_ref = state->ref_frame.buf;
	unsigned int cap_stride = state->stride;
	unsigned int ref_stride = state->ref_stride;

	for (plane_idx = 0; plane_idx < info->planes_num; plane_idx++) {
		int i;
		unsigned int h_div = (plane_idx == 1 || plane_idx == 2) ?
			info->height_div : 1;
		const u8 *row_cap = cap;
		u8 *row_ref = p_ref;

		if (info->planes_num == 3 && plane_idx == 1) {
			cap_stride /= 2;
			ref_stride /= 2;
		}

		if (plane_idx == 1 &&
		    (info->id == V4L2_PIX_FMT_NV24 ||
		     info->id == V4L2_PIX_FMT_NV42)) {
			cap_stride *= 2;
			ref_stride *= 2;
		}

		for (i = 0; i < state->visible_height / h_div; i++) {
			memcpy(row_ref, row_cap, ref_stride);
			row_ref += ref_stride;
			row_cap += cap_stride;
		}
		cap += cap_stride * (state->coded_height / h_div);
		p_ref += ref_stride * (state->coded_height / h_div);
	}
}

bool fwht_decompress(struct codec_ctx *ctx, __u8 *p_in, unsigned comp_size,
		     __u8 *p_out, unsigned uncomp_size)
{
	memcpy(&ctx->state.header, p_in, sizeof(ctx->state.header));
	p_in += sizeof(ctx->state.header);
	if (v4l2_fwht_decode(&ctx->state, p_in, p_out))
		return false;
	copy_cap_to_ref(p_out, ctx->state.info, &ctx->state);
	return true;
}
