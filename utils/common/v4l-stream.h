/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * V4L2 run-length image encoder header
 *
 * Copyright 2016 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef _V4L_STREAM_H_
#define _V4L_STREAM_H_

#include <linux/videodev2.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <codec-v4l2-fwht.h>

/* Default port */
#define V4L_STREAM_PORT 8362

/*
 * The stream starts with the stream ID followed by the version
 * number.
 *
 * This is followed by FRAME_VIDEO packets with optional FMT_VIDEO
 * packets in between if the format changes from one frame to another.
 *
 * Before sending the initial FRAME_VIDEO packet you must send the
 * FMT_VIDEO packet first.
 *
 * All values (IDs, sizes, etc) are all uint32_t in network order.
 */
#define V4L_STREAM_ID			v4l2_fourcc('V', '4', 'L', '2')
#define V4L_STREAM_VERSION		2

/*
 * Each packet is followed by the size of the packet (not including
 * the packet ID + size).
 */

/*
 * The video format is defined as follows:
 *
 * uint32_t size_fmt; 	// size in bytes of data after size_fmt up to fmt_plane
 * uint32_t num_planes;
 * uint32_t pixelformat;
 * uint32_t width;
 * uint32_t height;
 * uint32_t field;
 * uint32_t colorspace;
 * uint32_t ycbcr_enc;
 * uint32_t quantization;
 * uint32_t xfer_func;
 * uint32_t flags;
 * uint32_t pixel_aspect_numerator;	(pixel_aspect = y/x, same as VIDIOC_CROPCAP)
 * uint32_t pixel_aspect_denominator;
 *
 * struct fmt_plane {
 * 	uint32_t size_fmt_plane; // size in bytes of this plane format struct excluding this field
 * 	uint32_t sizeimage;
 * 	uint32_t bytesperline;
 * } fmt_planes[num_planes];
 */
#define V4L_STREAM_PACKET_FMT_VIDEO			v4l2_fourcc('f', 'm', 't', 'v')
#define V4L_STREAM_PACKET_FMT_VIDEO_SIZE_FMT		(12 * 4)
#define V4L_STREAM_PACKET_FMT_VIDEO_SIZE_FMT_PLANE	(2 * 4)
#define V4L_STREAM_PACKET_FMT_VIDEO_SIZE(planes)	(V4L_STREAM_PACKET_FMT_VIDEO_SIZE_FMT + 4 + \
							 (planes) * (V4L_STREAM_PACKET_FMT_VIDEO_SIZE_FMT_PLANE + 4))

/* Run-length encoded frame video packet */
#define V4L_STREAM_PACKET_FRAME_VIDEO_RLE		v4l2_fourcc('f', 'r', 'm', 'v')
/* FWHT compressed frame video packet */
#define V4L_STREAM_PACKET_FRAME_VIDEO_FWHT		v4l2_fourcc('f', 'r', 'm', 'V')

#define V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_HDR		(8 * 4)
#define V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_PLANE_HDR	(8 * 4)
#define V4L_STREAM_PACKET_FRAME_VIDEO_SIZE(planes) 	(V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_HDR + 4 + \
							 (planes) * (V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_PLANE_HDR + 4))

/*
 * The frame video packet content is defined as follows:
 *
 * uint32_t size_hdr;	// size in bytes of data after size_hdr until planes[]
 * uint32_t field;
 * uint32_t flags;
 * struct plane {
 * 	uint32_t size_plane_hdr; // size in bytes of data after size_plane_hdr until data[]
 * 	uint32_t bytesused;
 * 	uint32_t data_size;
 * 	uint8_t data[data_size];
 * } planes[num_planes];
 */

/* Run-length encoding desciption: */

#define V4L_STREAM_PACKET_FRAME_VIDEO_X_RLE		0x02dead43
#define V4L_STREAM_PACKET_FRAME_VIDEO_Y_RLE		0x02dead41
#define V4L_STREAM_PACKET_FRAME_VIDEO_RPLC		0x02dead42

/*
 * The run-length encoding used is optimized for use with test patterns.
 * The data_size value is always <= bytesused. If it is equal to bytesused
 * then no RLE was used.
 *
 * The RLE works on groups of 4 bytes. The VIDEO_Y_RLE value is only used
 * if the corresponding bytesperline value of the plane is != 0.
 *
 * If duplicate lines are found, then an Y_RLE value followed by the number
 * of duplicate lines - 1 is inserted (so two identical lines are replaced
 * by Y_RLE followed by 1 followed by the encoded line).
 *
 * If there are more than 3 identical values, then those are replaced by an
 * X_RLE value followed by the duplicated value followed by the number of
 * duplicate values within the line. So 0xff, 0xff, 0xff, 0xff would become
 * X_RLE, 0xff, 4.
 *
 * If X_RLE or Y_RLE (if bytesperline != 0) is found in the stream, then
 * those are replaced by the RPLC value (this makes the encoding very slightly
 * lossy)
 */

/*
 * FWHT-compression desciption:
 *
 * The compressed encoding is simple but good enough for debugging.
 * The size value is always <= bytesused + sizeof(struct fwht_cframe_hdr).
 *
 * See codec-fwht.h for more information about the compression
 * details.
 */

/*
 * This packet ends the stream and, after reading this, the socket can be closed
 * since no more data will follow.
 */
#define V4L_STREAM_PACKET_END				v4l2_fourcc('e', 'n', 'd', ' ')

struct codec_ctx {
	struct v4l2_fwht_state	state;
	unsigned int		flags;
	unsigned int		size;
	u32			field;
	u32			comp_max_size;
};

unsigned rle_compress(__u8 *buf, unsigned size, unsigned bytesperline);
void rle_decompress(__u8 *buf, unsigned size, unsigned rle_size, unsigned bytesperline);
struct codec_ctx *fwht_alloc(unsigned pixfmt, unsigned visible_width, unsigned visible_height,
			     unsigned coded_width, unsigned coded_height, unsigned field,
			     unsigned colorspace, unsigned xfer_func, unsigned ycbcr_enc,
			     unsigned quantization);
void fwht_free(struct codec_ctx *ctx);
__u8 *fwht_compress(struct codec_ctx *ctx, __u8 *buf, unsigned size, unsigned *comp_size);
bool fwht_decompress(struct codec_ctx *ctx, __u8 *read_buf, unsigned comp_size,
		     __u8 *buf, unsigned size);
unsigned rle_calc_bpl(unsigned bpl, __u32 pixelformat);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
