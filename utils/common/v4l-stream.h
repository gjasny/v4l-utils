/*
 * V4L2 run-length image encoder header
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

#ifndef _V4L_STREAM_H_
#define _V4L_STREAM_H_

#include <linux/videodev2.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Default port */
#define V4L_STREAM_PORT 8362

/*
 * The stream starts with the stream ID followed by the version
 * number.
 *
 * This is followed by FRAME_VIDEO packets with optional FMT_VIDEO
 * packets in between if the format changes from one frame to another.
 *
 * Before sending the initial FRAME_VIDEO packet you must sent the
 * FMT_VIDEO packet first.
 *
 * All values (IDs, sizes, etc) are all uint32_t in network order.
 */
#define V4L_STREAM_ID			v4l2_fourcc('V', '4', 'L', '2')
#define V4L_STREAM_VERSION		1

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

#define V4L_STREAM_PACKET_FRAME_VIDEO_X_RLE 0x02dead43
#define V4L_STREAM_PACKET_FRAME_VIDEO_Y_RLE 0x02dead41
#define V4L_STREAM_PACKET_FRAME_VIDEO_RPLC  0x02dead42

/*
 * The frame content is defined as follows:
 *
 * uint32_t size_hdr;	// size in bytes of data after size_hdr until planes[]
 * uint32_t field;
 * uint32_t flags;
 * struct plane {
 * 	uint32_t size_plane_hdr; // size in bytes of data after size_plane_hdr until data[]
 * 	uint32_t bytesused;
 * 	uint32_t rle_size;
 * 	uint8_t data[rle_size];
 * } planes[num_planes];
 *
 * The run-length encoding used is optimized for use with test patterns.
 * The rle_size value is always <= bytesused. If it is equal to bytesused
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
#define V4L_STREAM_PACKET_FRAME_VIDEO			v4l2_fourcc('f', 'r', 'm', 'v')
#define V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_HDR		(8 * 4)
#define V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_PLANE_HDR	(8 * 4)
#define V4L_STREAM_PACKET_FRAME_VIDEO_SIZE(planes) 	(V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_HDR + 4 + \
							 (planes) * (V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_PLANE_HDR + 4))

/*
 * This packet ends the stream and, after reading this, the socket can be closed
 * since no more data will follow.
 */
#define V4L_STREAM_PACKET_END				v4l2_fourcc('e', 'n', 'd', ' ')

unsigned rle_compress(__u8 *buf, unsigned size, unsigned bytesperline);
void rle_decompress(__u8 *buf, unsigned size, unsigned rle_size, unsigned bytesperline);
unsigned rle_calc_bpl(unsigned bpl, __u32 pixelformat);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
