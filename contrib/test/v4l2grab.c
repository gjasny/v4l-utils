/* V4L2 video picture grabber
   Copyright (C) 2009 Mauro Carvalho Chehab <mchehab@kernel.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 */

#include <argp.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>

#include "../../lib/include/libv4l2.h"

#define CLEAR_P(x,s) memset((x), 0, s)
#define CLEAR(x) CLEAR_P(&(x), sizeof(x))

#define ARRAY_SIZE(a)  (sizeof(a)/sizeof(*a))

static int libv4l = 1;
static int ppm_output = 1;

struct buffer {
	void   *start;
	size_t length;
};

enum io_method {
	IO_METHOD_READ,
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR,
};

static void xioctl(int fh, unsigned long int request, void *arg)
{
	int r;

	do {
		if (libv4l)
			r = v4l2_ioctl(fh, request, arg);
		else
			r = ioctl(fh, request, arg);

	} while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));

	if (r == -1) {
		fprintf(stderr, "%s(%lu): error %d, %s\n", __func__,
			_IOC_NR(request), errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

struct video_formats {
	unsigned int pixformat;
	unsigned int depth;
	int y_decimation;
	int x_decimation;

	unsigned int is_rgb:1;
};

struct colorspace_parms {
	enum v4l2_colorspace		colorspace;
	enum v4l2_xfer_func		xfer_func;
	enum v4l2_ycbcr_encoding	ycbcr_enc;
	enum v4l2_quantization		quantization;
};

static const struct video_formats supported_formats[] = {
	{ V4L2_PIX_FMT_BGR32,   32, -1, -1, 1},
	{ V4L2_PIX_FMT_ABGR32,  32, -1, -1, 1},
	{ V4L2_PIX_FMT_XBGR32,  32, -1, -1, 1},
	{ V4L2_PIX_FMT_RGB32,   32, -1, -1, 1},
	{ V4L2_PIX_FMT_ARGB32,  32, -1, -1, 1},
	{ V4L2_PIX_FMT_XRGB32,  32, -1, -1, 1},
	{ V4L2_PIX_FMT_BGR24,   24, -1, -1, 1},
	{ V4L2_PIX_FMT_RGB24,   24, -1, -1, 1},
	{ V4L2_PIX_FMT_RGB565,  16, -1, -1, 1},
	{ V4L2_PIX_FMT_RGB565X, 16, -1, -1, 1},
	{ V4L2_PIX_FMT_YUYV,    16, -1, -1, 0},
	{ V4L2_PIX_FMT_UYVY,    16, -1, -1, 0},
	{ V4L2_PIX_FMT_YVYU,    16, -1, -1, 0},
	{ V4L2_PIX_FMT_VYUY,    16, -1, -1, 0},
	{ V4L2_PIX_FMT_NV12,     8, 1, -1, 0},
	{ V4L2_PIX_FMT_NV21,     8, 1, -1, 0},
	{ V4L2_PIX_FMT_NV16,     8, 0, -1, 0},
	{ V4L2_PIX_FMT_NV61,     8, 0, -1, 0},
	{ V4L2_PIX_FMT_YUV420,   8, 1, 1, 0},
	{ V4L2_PIX_FMT_YVU420,   8, 1, 1, 0},
	{ V4L2_PIX_FMT_YUV422P,  8, 0, 1, 0},
};

static const struct video_formats *video_fmt_props(unsigned int pixformat)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_formats); i++)
		if (supported_formats[i].pixformat == pixformat)
			return &supported_formats[i];

	return NULL;
}

static int is_format_supported(unsigned int pixformat)
{
	if (video_fmt_props(pixformat))
		return 1;
	return 0;
}

static void get_colorspace_data(struct colorspace_parms *c,
			        struct v4l2_format *fmt)
{
	const struct video_formats *video_fmt;

	memset(c, 0, sizeof(*c));

	video_fmt = video_fmt_props(fmt->fmt.pix.pixelformat);
	if (!video_fmt)
		return;

	/*
	 * A more complete colorspace default detection would need to
	 * implement timings API, in order to check for SDTV/HDTV.
	 */
	if (fmt->fmt.pix.colorspace == V4L2_COLORSPACE_DEFAULT)
		c->colorspace = video_fmt->is_rgb ?
				V4L2_COLORSPACE_SRGB :
				V4L2_COLORSPACE_REC709;
	else
		c->colorspace = fmt->fmt.pix.colorspace;

	if (fmt->fmt.pix.xfer_func == V4L2_XFER_FUNC_DEFAULT)
		c->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(c->colorspace);
	else
		c->xfer_func = fmt->fmt.pix.xfer_func;

	if (!video_fmt->is_rgb) {
		if (fmt->fmt.pix.ycbcr_enc == V4L2_YCBCR_ENC_DEFAULT)
			c->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(c->colorspace);
		else
			c->ycbcr_enc = fmt->fmt.pix.ycbcr_enc;
	}

	if (fmt->fmt.pix.quantization == V4L2_QUANTIZATION_DEFAULT)
		c->quantization = V4L2_MAP_QUANTIZATION_DEFAULT(video_fmt->is_rgb,
								c->colorspace,
								c->ycbcr_enc);
}

/*
 * Querycap implementation
 */

static char *prt_caps(uint32_t caps, char *s)
{
	if (V4L2_CAP_VIDEO_CAPTURE & caps)
		strcat (s,"CAPTURE ");
	if (V4L2_CAP_VIDEO_CAPTURE_MPLANE & caps)
		strcat (s,"CAPTURE_MPLANE ");
	if (V4L2_CAP_VIDEO_OUTPUT & caps)
		strcat (s,"OUTPUT ");
	if (V4L2_CAP_VIDEO_OUTPUT_MPLANE & caps)
		strcat (s,"OUTPUT_MPLANE ");
	if (V4L2_CAP_VIDEO_M2M & caps)
		strcat (s,"M2M ");
	if (V4L2_CAP_VIDEO_M2M_MPLANE & caps)
		strcat (s,"M2M_MPLANE ");
	if (V4L2_CAP_VIDEO_OVERLAY & caps)
		strcat (s,"OVERLAY ");
	if (V4L2_CAP_VBI_CAPTURE & caps)
		strcat (s,"VBI_CAPTURE ");
	if (V4L2_CAP_VBI_OUTPUT & caps)
		strcat (s,"VBI_OUTPUT ");
	if (V4L2_CAP_SLICED_VBI_CAPTURE & caps)
		strcat (s,"SLICED_VBI_CAPTURE ");
	if (V4L2_CAP_SLICED_VBI_OUTPUT & caps)
		strcat (s,"SLICED_VBI_OUTPUT ");
	if (V4L2_CAP_RDS_CAPTURE & caps)
		strcat (s,"RDS_CAPTURE ");
	if (V4L2_CAP_RDS_OUTPUT & caps)
		strcat (s,"RDS_OUTPUT ");
	if (V4L2_CAP_SDR_CAPTURE & caps)
		strcat (s,"SDR_CAPTURE ");
	if (V4L2_CAP_TUNER & caps)
		strcat (s,"TUNER ");
	if (V4L2_CAP_HW_FREQ_SEEK & caps)
		strcat (s,"HW_FREQ_SEEK ");
	if (V4L2_CAP_MODULATOR & caps)
		strcat (s,"MODULATOR ");
	if (V4L2_CAP_AUDIO & caps)
		strcat (s,"AUDIO ");
	if (V4L2_CAP_RADIO & caps)
		strcat (s,"RADIO ");
	if (V4L2_CAP_READWRITE & caps)
		strcat (s,"READWRITE ");
	if (V4L2_CAP_STREAMING & caps)
		strcat (s,"STREAMING ");
	if (V4L2_CAP_EXT_PIX_FORMAT & caps)
		strcat (s,"EXT_PIX_FORMAT ");
	if (V4L2_CAP_DEVICE_CAPS & caps)
		strcat (s,"DEVICE_CAPS ");
	if(V4L2_CAP_VIDEO_OUTPUT_OVERLAY & caps)
		strcat (s,"VIDEO_OUTPUT_OVERLAY ");
	if(V4L2_CAP_HW_FREQ_SEEK & caps)
		strcat (s,"HW_FREQ_SEEK ");
	if(V4L2_CAP_VIDEO_M2M_MPLANE & caps)
		strcat (s,"VIDEO_M2M_MPLANE ");
	if(V4L2_CAP_VIDEO_M2M & caps)
		strcat (s,"VIDEO_M2M ");
	if(V4L2_CAP_SDR_OUTPUT & caps)
		strcat (s,"SDR_OUTPUT ");
	if(V4L2_CAP_META_CAPTURE & caps)
		strcat (s,"META_CAPTURE ");
	if(V4L2_CAP_META_OUTPUT & caps)
		strcat (s,"META_OUTPUT ");
	if(V4L2_CAP_TOUCH & caps)
		strcat (s,"TOUCH ");
	if(V4L2_CAP_IO_MC & caps)
		strcat (s,"IO_MC ");

	return s;
}

static void querycap(char *fname, int fd, enum io_method method)
{
	struct v4l2_capability cap;
	char s[4096] = "";

	xioctl(fd, VIDIOC_QUERYCAP, &cap);

	printf("Device caps: %s\n", prt_caps(cap.capabilities, s));

	s[0] = '\0';

	if (cap.capabilities & V4L2_CAP_DEVICE_CAPS)
		printf("Driver caps: %s\n", prt_caps(cap.device_caps, s));

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n",
			fname);
		exit(EXIT_FAILURE);
	}

	switch (method) {
	case IO_METHOD_READ:
		if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
			fprintf(stderr, "%s does not support read I/O\n",
				fname);
			exit(EXIT_FAILURE);
		}
		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
			fprintf(stderr, "%s does not support streaming I/O\n",
				 fname);
			exit(EXIT_FAILURE);
		}
		break;
	}
}

#define CLAMP(a)	((a) < 0 ? 0 : (a) > 255 ? 255 : (a))

static void convert_yuv(struct colorspace_parms *c,
			int32_t y, int32_t u, int32_t v,
			unsigned char **dst)
{
	if (c->quantization == V4L2_QUANTIZATION_FULL_RANGE)
		y *= 65536;
	else
		y = (y - 16) * 76284;

	u -= 128;
	v -= 128;

	/*
	 * TODO: add BT2020 and SMPTE240M and better handle
	 * other differences
	 */
	switch (c->ycbcr_enc) {
	case V4L2_YCBCR_ENC_601:
	case V4L2_YCBCR_ENC_XV601:
	case V4L2_YCBCR_ENC_SYCC:
		/*
		* ITU-R BT.601 matrix:
		*    R = 1.164 * y +    0.0 * u +  1.596 * v
		*    G = 1.164 * y + -0.392 * u + -0.813 * v
		*    B = 1.164 * y +  2.017 * u +    0.0 * v
		*/
		*(*dst)++ = CLAMP((y              + 104595 * v) >> 16);
		*(*dst)++ = CLAMP((y -  25690 * u -  53281 * v) >> 16);
		*(*dst)++ = CLAMP((y + 132186 * u             ) >> 16);
		break;
	default:
		/*
		* ITU-R BT.709 matrix:
		*    R = 1.164 * y +    0.0 * u +  1.793 * v
		*    G = 1.164 * y + -0.213 * u + -0.533 * v
		*    B = 1.164 * y +  2.112 * u +    0.0 * v
		*/
		*(*dst)++ = CLAMP((y              + 117506 * v) >> 16);
		*(*dst)++ = CLAMP((y -  13959 * u -  34931 * v) >> 16);
		*(*dst)++ = CLAMP((y + 138412 * u             ) >> 16);
		break;
	}
}

static void copy_two_pixels(struct v4l2_format *fmt,
			    struct colorspace_parms *c,
			    unsigned char *plane0,
			    unsigned char *plane1,
			    unsigned char *plane2,
			    unsigned char **dst)
{
	uint32_t fourcc = fmt->fmt.pix.pixelformat;
	int32_t y_off, u_off, u, v;
	uint16_t pix;
	int i;

	switch (fmt->fmt.pix.pixelformat) {
	case V4L2_PIX_FMT_RGB565: /* rrrrrggg gggbbbbb */
		for (i = 0; i < 2; i++) {
			pix = (plane0[0] << 8) + plane0[1];

			*(*dst)++ = (unsigned char)(((pix & 0xf800) >> 11) << 3) | 0x07;
			*(*dst)++ = (unsigned char)((((pix & 0x07e0) >> 5)) << 2) | 0x03;
			*(*dst)++ = (unsigned char)((pix & 0x1f) << 3) | 0x07;

			plane0 += 2;
		}
		break;
	case V4L2_PIX_FMT_RGB565X: /* gggbbbbb rrrrrggg */
		for (i = 0; i < 2; i++) {
			pix = (plane0[1] << 8) + plane0[0];

			*(*dst)++ = (unsigned char)(((pix & 0xf800) >> 11) << 3) | 0x07;
			*(*dst)++ = (unsigned char)((((pix & 0x07e0) >> 5)) << 2) | 0x03;
			*(*dst)++ = (unsigned char)((pix & 0x1f) << 3) | 0x07;

			plane0 += 2;
		}
		break;
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_VYUY:
		y_off = (fourcc == V4L2_PIX_FMT_YUYV || fourcc == V4L2_PIX_FMT_YVYU) ? 0 : 1;
		u_off = (fourcc == V4L2_PIX_FMT_YUYV || fourcc == V4L2_PIX_FMT_UYVY) ? 0 : 2;

		u = plane0[(1 - y_off) + u_off];
		v = plane0[(1 - y_off) + (2 - u_off)];

		for (i = 0; i < 2; i++)
			convert_yuv(c, plane0[y_off + (i << 1)], u, v, dst);

		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV16:
		u = plane1[0];
		v = plane1[1];

		for (i = 0; i < 2; i++)
			convert_yuv(c, plane0[i], u, v, dst);

		break;
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV61:
		v = plane1[0];
		u = plane1[1];

		for (i = 0; i < 2; i++)
			convert_yuv(c, plane0[i], u, v, dst);

		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YUV422P:
		u = plane1[0];
		v = plane2[0];

		for (i = 0; i < 2; i++)
			convert_yuv(c, plane0[i], u, v, dst);

		break;
	case V4L2_PIX_FMT_YVU420:
		v = plane1[0];
		u = plane2[0];

		for (i = 0; i < 2; i++)
			convert_yuv(c, plane0[i], u, v, dst);

		break;
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_XRGB32:
		for (i = 0; i < 2; i++) {
			*(*dst)++ = plane0[1];
			*(*dst)++ = plane0[2];
			*(*dst)++ = plane0[3];

			plane0 += 4;
		}
		break;
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_XBGR32:
		for (i = 0; i < 2; i++) {
			*(*dst)++ = plane0[2];
			*(*dst)++ = plane0[1];
			*(*dst)++ = plane0[0];

			plane0 += 4;
		}
		break;
	default:
	case V4L2_PIX_FMT_BGR24:
		for (i = 0; i < 2; i++) {
			*(*dst)++ = plane0[2];
			*(*dst)++ = plane0[1];
			*(*dst)++ = plane0[0];

			plane0 += 3;
		}
		break;
	}
}

static unsigned int convert_to_rgb24(struct v4l2_format *fmt,
				     unsigned int imagesize,
				     unsigned char *plane0,
				     unsigned char *p_out)
{
	uint32_t width = fmt->fmt.pix.width;
	uint32_t height = fmt->fmt.pix.height;
	uint32_t bytesperline = fmt->fmt.pix.bytesperline;
	const struct video_formats *video_fmt;
	unsigned char *plane0_start = plane0;
	unsigned char *plane1_start = NULL;
	unsigned char *plane2_start = NULL;
	unsigned char *plane1 = NULL;
	unsigned char *plane2 = NULL;
	struct colorspace_parms c;
	unsigned int needed_size;
	unsigned int x, y, depth;
	uint32_t num_planes = 1;
	unsigned char *p_start;
	uint32_t plane0_size;
	uint32_t w_dec = 0;
	uint32_t h_dec = 0;

	get_colorspace_data(&c, fmt);

	video_fmt = video_fmt_props(fmt->fmt.pix.pixelformat);
	if (!video_fmt)
		return 0;

	depth = video_fmt->depth;

	plane0_size = width * height * depth;
	needed_size = plane0_size;

	if (video_fmt->y_decimation >= 0) {
		num_planes++;
		h_dec = video_fmt->y_decimation;
		needed_size += plane0_size >> h_dec;
	}

	if (video_fmt->x_decimation >= 0) {
		num_planes++;
		w_dec = video_fmt->x_decimation;
		needed_size += plane0_size >> w_dec;
	}

	plane0_size = plane0_size >> 3;
	needed_size = needed_size >> 3;

	if (imagesize < needed_size) {
		fprintf(stderr, "Warning: Image too small! ");
		fprintf(stderr, "Image size: %u bytes, need %u, being:\n", imagesize, needed_size);
		fprintf(stderr, "\tPlane0 size: %u bytes\n", plane0_size);
		if (video_fmt->y_decimation >= 0)
			fprintf(stderr, "\tH Plane size: %u bytes\n", plane0_size >> h_dec);
		if (video_fmt->x_decimation >= 0)
			fprintf(stderr, "\tW Plane size: %u bytes\n", plane0_size >> w_dec);

		// FIXME: should we bail-out here?
		// exit(EXIT_FAILURE);
	}

	p_start = p_out;

	if (num_planes > 1)
		plane1_start = plane0_start + plane0_size;

	if (num_planes > 2)
		    plane2_start = plane1_start + (plane0_size >> (w_dec + h_dec));

	for (y = 0; y < height; y++) {
		plane0 = plane0_start + bytesperline * y;
		if (num_planes > 1)
			plane1 = plane1_start + (bytesperline >> w_dec) * (y >> h_dec);
		if (num_planes > 2)
			plane2 = plane2_start + (bytesperline >> w_dec) * (y >> h_dec);

		for (x = 0; x < width >> 1; x++) {
			copy_two_pixels(fmt, &c, plane0, plane1, plane2, &p_out);

			plane0 += depth >> 2;
			if (num_planes > 1)
				plane1 += depth >> (2 + w_dec);
			if (num_planes > 2)
				plane2 += depth >> (2 + w_dec);
		}
	}

	return p_out - p_start;
}

/*
 * Read I/O
 */
static int read_capture_loop(int fd, struct buffer *buffers,
			     struct v4l2_format *fmt, int n_frames,
			     unsigned char *out_buf, char *out_dir)
{
	unsigned int			i;
	struct timeval			tv;
	int				r;
	fd_set				fds;
	ssize_t				size;
	FILE				*fout;
	char				out_name[25 + strlen(out_dir)];

	for (i = 0; i < n_frames; i++) {
		do {
			do {
				FD_ZERO(&fds);
				FD_SET(fd, &fds);

				/* Timeout. */
				tv.tv_sec = 2;
				tv.tv_usec = 0;

				r = select(fd + 1, &fds, NULL, NULL, &tv);
			} while ((r == -1 && (errno == EINTR)));
			if (r == -1) {
				perror("select");
				return errno;
			}

			size = read(fd, buffers[0].start, buffers[0].length);

			if (size == -1 && errno != EAGAIN) {
				perror("Cannot read image");
				exit(EXIT_FAILURE);
			}
		} while (size != -1);

		if (ppm_output)
			sprintf(out_name, "%s/out%03d.ppm", out_dir, i);
		else
			sprintf(out_name, "%s/out%03d.raw", out_dir, i);

		fout = fopen(out_name, "w");
		if (!fout) {
			perror("Cannot open image");
			exit(EXIT_FAILURE);
		}
		if (ppm_output)
			fprintf(fout, "P6\n%d %d 255\n",
				fmt->fmt.pix.width, fmt->fmt.pix.height);

		if (!ppm_output || !out_buf) {
			out_buf = buffers[0].start;
		} else {
			size = convert_to_rgb24(fmt, size,
						buffers[0].start, out_buf);
		}

		fwrite(out_buf, size, 1, fout);
		fclose(fout);
	}
	return 0;
}

static int read_capture(int fd, int n_frames, unsigned char *out_buf, char *out_dir,
			struct v4l2_format *fmt, struct timeval *start)
{
	struct buffer                   *buffers;
	struct timezone			tz = { 0 };

	buffers = calloc(1, sizeof(*buffers));

	gettimeofday(start, &tz);

	return read_capture_loop(fd, buffers, fmt, n_frames, out_buf, out_dir);
}

/*
 * userptr mmapped capture I/O
 */
static int userptr_capture_loop(int fd, struct buffer *buffers,
				int n_buffers, struct v4l2_format *fmt,
				int n_frames, unsigned char *out_buf, char *out_dir)
{
	struct v4l2_buffer		buf;
	unsigned int			i;
	struct timeval			tv;
	int				r;
	fd_set				fds;
	FILE				*fout;
	unsigned int			size;
	char				out_name[25 + strlen(out_dir)];

	for (i = 0; i < n_frames; i++) {
		do {
			FD_ZERO(&fds);
			FD_SET(fd, &fds);

			/* Timeout. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(fd + 1, &fds, NULL, NULL, &tv);
		} while ((r == -1 && (errno == EINTR)));
		if (r == -1) {
			perror("select");
			return errno;
		}

		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_USERPTR;

		xioctl(fd, VIDIOC_DQBUF, &buf);

		if (ppm_output)
			sprintf(out_name, "%s/out%03d.ppm", out_dir, i);
		else
			sprintf(out_name, "%s/out%03d.raw", out_dir, i);

		fout = fopen(out_name, "w");
		if (!fout) {
			perror("Cannot open image");
			exit(EXIT_FAILURE);
		}
		if (ppm_output)
			fprintf(fout, "P6\n%d %d 255\n",
				fmt->fmt.pix.width, fmt->fmt.pix.height);

		if (!ppm_output || !out_buf) {
			out_buf = buffers[buf.index].start;
			size = buf.bytesused;
		} else {
			size = convert_to_rgb24(fmt, buf.bytesused,
						buffers[buf.index].start,
						out_buf);
		}

		fwrite(out_buf, size, 1, fout);
		fclose(fout);

		if (i + 1 < n_frames)
			xioctl(fd, VIDIOC_QBUF, &buf);
	}
	return 0;
}

static int userptr_capture(int fd, int n_frames, unsigned char *out_buf, char *out_dir,
			   struct v4l2_format *fmt, struct timeval *start)
{
	struct v4l2_buffer              buf;
	struct v4l2_requestbuffers      req;
	unsigned int                    ret, i, n_buffers;
	struct buffer                   *buffers;
	struct timezone			tz = { 0 };

	CLEAR(req);
	req.count  = 2;
	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;
	xioctl(fd, VIDIOC_REQBUFS, &req);

	buffers = calloc(req.count, sizeof(*buffers));
	if (!buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	/* Some compressed formats could return zero */
	if (!fmt->fmt.pix.sizeimage)
		fmt->fmt.pix.sizeimage = fmt->fmt.pix.width * fmt->fmt.pix.height * 3;

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		buffers[n_buffers].length = fmt->fmt.pix.sizeimage;
		buffers[n_buffers].start = calloc(1, fmt->fmt.pix.sizeimage);

		if (!buffers[n_buffers].start) {
			fprintf(stderr, "Out of memory\n");
			exit(EXIT_FAILURE);
		}
	}

	for (i = 0; i < n_buffers; ++i) {
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_USERPTR;
		buf.index = i;
		buf.m.userptr = (unsigned long)buffers[i].start;
		buf.length = buffers[i].length;
		xioctl(fd, VIDIOC_QBUF, &buf);
	}

	xioctl(fd, VIDIOC_STREAMON, &req.type);

	gettimeofday(start, &tz);

	ret = userptr_capture_loop(fd, buffers, req.count, fmt,
				   n_frames, out_buf, out_dir);

	xioctl(fd, VIDIOC_STREAMOFF, &req.type);

	return ret;
}

/*
 * Normal mmapped capture I/O
 */

/* Used by the multi thread capture version */
struct buffer_queue {
	struct v4l2_buffer *buffers;
	int buffers_size;

	int read_pos;
	int write_pos;
	int n_frames;

	int fd;

	pthread_mutex_t mutex;
	pthread_cond_t buffer_cond;
};

/* Gets a buffer and puts it in the buffers list at write position, then
 * notifies the consumer that a new buffer is ready to be used */
static void *produce_buffer (void * p)
{
	struct buffer_queue 		*bq;
	fd_set				fds;
	struct timeval			tv;
	int				i;
	struct v4l2_buffer		*buf;
	int				r;

	bq = p;

	for (i = 0; i < bq->n_frames; i++) {
		printf ("Prod: %d\n", i);
		buf = &bq->buffers[bq->write_pos % bq->buffers_size];
		do {
			FD_ZERO(&fds);
			FD_SET(bq->fd, &fds);

			/* Timeout. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(bq->fd + 1, &fds, NULL, NULL, &tv);
		} while ((r == -1 && (errno == EINTR)));
		if (r == -1) {
			perror("select");
			pthread_exit (NULL);
			return NULL;
		}

		CLEAR_P(buf, sizeof(struct v4l2_buffer));
		buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf->memory = V4L2_MEMORY_MMAP;
		xioctl(bq->fd, VIDIOC_DQBUF, buf);

		pthread_mutex_lock (&bq->mutex);
		bq->write_pos++;
		printf ("Prod: %d (done)\n", i);
		pthread_cond_signal (&bq->buffer_cond);
		pthread_mutex_unlock (&bq->mutex);

	}

	pthread_exit (NULL);
}

/* will create a separate thread that will produce the buffers and put
 * into a circular array while this same thread will get the buffers from
 * this array and 'render' them */
static int mmap_capture_threads(int fd, struct buffer *buffers,
				int bufpool_size, struct v4l2_format *fmt,
				int n_frames, unsigned char *out_buf, char *out_dir,
				int sleep_ms)
{
	struct v4l2_buffer		buf;
	unsigned int			i, size;
	struct buffer_queue		buf_queue;
	pthread_t			producer;
	char				out_name[25 + strlen(out_dir)];
	FILE				*fout;
	struct timespec			sleeptime;

	if (sleep_ms) {
		sleeptime.tv_sec = sleep_ms / 1000;
		sleeptime.tv_nsec = (sleep_ms % 1000) * 1000000;
	}

	buf_queue.buffers_size = bufpool_size * 2;
	buf_queue.buffers = calloc(buf_queue.buffers_size,
				   sizeof(struct v4l2_buffer));
	buf_queue.fd = fd;
	buf_queue.read_pos = 0;
	buf_queue.write_pos = 0;
	buf_queue.n_frames = n_frames;
	pthread_mutex_init (&buf_queue.mutex, NULL);
	pthread_cond_init (&buf_queue.buffer_cond, NULL);

	pthread_create (&producer, NULL, produce_buffer, &buf_queue);

	for (i = 0; i < n_frames; i++) {
		printf ("Read: %d\n", i);

		/* wait for a buffer to be available in the queue */
		pthread_mutex_lock (&buf_queue.mutex);
		while (buf_queue.read_pos == buf_queue.write_pos) {
			pthread_cond_wait (&buf_queue.buffer_cond,
					   &buf_queue.mutex);
		}
		pthread_mutex_unlock (&buf_queue.mutex);

		if (sleep_ms)
			nanosleep (&sleeptime, NULL);

		if (ppm_output)
			sprintf(out_name, "%s/out%03d.ppm", out_dir, i);
		else
			sprintf(out_name, "%s/out%03d.raw", out_dir, i);

		fout = fopen(out_name, "w");
		if (!fout) {
			perror("Cannot open image");
			exit(EXIT_FAILURE);
		}
		if (ppm_output)
			fprintf(fout, "P6\n%d %d 255\n",
				fmt->fmt.pix.width, fmt->fmt.pix.height);

		buf = buf_queue.buffers[buf_queue.read_pos %
					buf_queue.buffers_size];

		if (!ppm_output || !out_buf) {
			out_buf = buffers[buf.index].start;
			size = buf.bytesused;
		} else {
			size = convert_to_rgb24(fmt, buf.bytesused,
						buffers[buf.index].start,
						out_buf);
		}

		fwrite(out_buf, size, 1, fout);
		fclose(fout);

		xioctl(fd, VIDIOC_QBUF, &buf);

		pthread_mutex_lock (&buf_queue.mutex);
		buf_queue.read_pos++;
		printf ("Read: %d (done)\n", i);
		pthread_cond_signal (&buf_queue.buffer_cond);
		pthread_mutex_unlock (&buf_queue.mutex);
	}

	pthread_mutex_destroy (&buf_queue.mutex);
	pthread_cond_destroy (&buf_queue.buffer_cond);
	free (buf_queue.buffers);
	return 0;
}

static int mmap_capture_loop(int fd, struct buffer *buffers,
			     struct v4l2_format *fmt, int n_frames,
			     unsigned char *out_buf, char *out_dir)
{
	struct v4l2_buffer		buf;
	unsigned int			i;
	struct timeval			tv;
	int				r;
	unsigned int			size;
	fd_set				fds;
	FILE				*fout;
	char				out_name[25 + strlen(out_dir)];

	for (i = 0; i < n_frames; i++) {
		do {
			FD_ZERO(&fds);
			FD_SET(fd, &fds);

			/* Timeout. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(fd + 1, &fds, NULL, NULL, &tv);
		} while ((r == -1 && (errno == EINTR)));
		if (r == -1) {
			perror("select");
			return errno;
		}

		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		xioctl(fd, VIDIOC_DQBUF, &buf);

		if (ppm_output)
			sprintf(out_name, "%s/out%03d.ppm", out_dir, i);
		else
			sprintf(out_name, "%s/out%03d.raw", out_dir, i);

		fout = fopen(out_name, "w");
		if (!fout) {
			perror("Cannot open image");
			exit(EXIT_FAILURE);
		}
		if (ppm_output)
			fprintf(fout, "P6\n%d %d 255\n",
				fmt->fmt.pix.width, fmt->fmt.pix.height);

		if (!ppm_output || !out_buf) {
			out_buf = buffers[buf.index].start;
			size = buf.bytesused;
		} else {
			size = convert_to_rgb24(fmt, buf.bytesused,
						buffers[buf.index].start,
						out_buf);
		}
		fwrite(out_buf, size, 1, fout);
		fclose(fout);

		if (i + 1 < n_frames)
			xioctl(fd, VIDIOC_QBUF, &buf);
	}
	return 0;
}

static int mmap_capture(int fd, int n_frames, unsigned char *out_buf,
			char *out_dir, int threads, int sleep_ms,
			struct v4l2_format *fmt, struct timeval *start)
{
	struct v4l2_buffer              buf;
	struct v4l2_requestbuffers      req;
	enum v4l2_buf_type              type;
	unsigned int                    i, n_buffers;
	struct buffer                   *buffers;
	struct timezone			tz = { 0 };

	CLEAR(req);
	req.count = 2;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	xioctl(fd, VIDIOC_REQBUFS, &req);

	buffers = calloc(req.count, sizeof(*buffers));
	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		CLEAR(buf);

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = n_buffers;

		xioctl(fd, VIDIOC_QUERYBUF, &buf);

		buffers[n_buffers].length = buf.length;
		if (!buffers) {
			fprintf(stderr, "Out of memory\n");
			exit(EXIT_FAILURE);
		}

		if (libv4l)
			buffers[n_buffers].start = v4l2_mmap(NULL, buf.length,
					    PROT_READ | PROT_WRITE, MAP_SHARED,
					    fd, buf.m.offset);
		else
			buffers[n_buffers].start = mmap(NULL, buf.length,
					    PROT_READ | PROT_WRITE, MAP_SHARED,
					    fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start) {
			perror("mmap");
			exit(EXIT_FAILURE);
		}
	}

	for (i = 0; i < n_buffers; ++i) {
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		xioctl(fd, VIDIOC_QBUF, &buf);
	}
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	xioctl(fd, VIDIOC_STREAMON, &type);

	gettimeofday(start, &tz);

	if (threads)
		mmap_capture_threads(fd, buffers, 2, fmt, n_frames, out_buf,
				     out_dir, sleep_ms);
	else
		mmap_capture_loop(fd, buffers, fmt, n_frames, out_buf, out_dir);

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	xioctl(fd, VIDIOC_STREAMOFF, &type);
	for (i = 0; i < n_buffers; ++i) {
		if (libv4l)
			v4l2_munmap(buffers[i].start, buffers[i].length);
		else
			munmap(buffers[i].start, buffers[i].length);
	}

	return 0;
}

/*
 * Main routine. Basically, reads parameters via argp.h and passes it to the
 * capture routine
 */

const char *argp_program_version = "V4L2 grabber version " V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <mchehab@kernel.org>";

static const char doc[] = "\nCapture images using libv4l, storing them as ppm files\n";

static const struct argp_option options[] = {
	{"device",	'd',	"DEV",		0,	"video device (default: /dev/video0)", 0},
	{"no-libv4l",	'D',	NULL,		0,	"Don't use libv4l", 0},
	{"out-dir",	'o',	"OUTDIR",	0,	"output directory (default: current dir)", 0},
	{"xres",	'x',	"XRES",		0,	"horizontal resolution", 0},
	{"yres",	'y',	"YRES",		0,	"vertical resolution", 0},
	{"fourcc",	'f',	"FOURCC",	0,	"Linux fourcc code", 0},
	{"userptr",	'u',	NULL,		0,	"Use user-defined memory capture method", 0},
	{"raw",		'R',	NULL,		0,	"Save image in raw mode", 0},
	{"read",	'r',	NULL,		0,	"Use read capture method", 0},
	{"n-frames",	'n',	"NFRAMES",	0,	"number of frames to capture", 0},
	{"thread-enable", 't',	"THREADS",	0,	"if different threads should capture and save", 0},
	{"blockmode-enable", 'b', NULL,		0,	"if blocking mode should be used", 0},
	{"sleep-time",	's',	"SLEEP",	0,	"how long should the consumer thread sleep to simulate the processing of a buffer (in ms)"},
	{ 0, 0, 0, 0, 0, 0 }
};

/* Static vars to store the parameters */
static char 	*dev_name = "/dev/video0";
static char	*out_dir = ".";
static int	x_res = 640;
static int	y_res = 480;
static int	n_frames = 20;
static int	threads = 0;
static int	block = 0;
static int	sleep_ms = 0;
static int	raw_mode = 0;
static uint32_t fourcc = V4L2_PIX_FMT_RGB24;
enum io_method  method = IO_METHOD_MMAP;

static error_t parse_opt(int k, char *arg, struct argp_state *state)
{
	int val, i, len;
	char s[4];

	switch (k) {
	case 'd':
		dev_name = arg;
		break;
	case 'o':
		out_dir = arg;
		break;
	case 'x':
		val = atoi(arg);
		if (val)
			x_res = val;
		break;
	case 'y':
		val = atoi(arg);
		if (val)
			y_res = val;
		break;
	case 'f':
		len = strlen(arg);
		if (len < 1 || len > 4)
			return ARGP_ERR_UNKNOWN;
		memcpy(s, arg, len);
		for (i = len; i < 4; i++)
			s[i] = ' ';
		fourcc = v4l2_fourcc(s[0], s[1], s[2], s[3]);
		break;
	case 'D':
		libv4l = 0;
		break;
	case 'n':
		val = atoi(arg);
		if (val)
			n_frames = val;
		break;
	case 't':
		threads = 1;
		break;
	case 'b':
		block = 1;
		break;
	case 'u':
		method = IO_METHOD_USERPTR;
		break;
	case 'r':
		method = IO_METHOD_READ;
		break;
	case 'R':
		raw_mode = 1;
		break;
	case 's':
		val = atoi(arg);
		if (val)
			sleep_ms = val;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = {
	.options = options,
	.parser = parse_opt,
	.doc = doc,
};

static int elapsed_time(struct timeval *x, struct timeval *y)
{
	long int res;
	int nsec;

	nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
	y->tv_usec -= 1000000 * nsec;
	y->tv_sec += nsec;

	res = (x->tv_sec - y->tv_sec) * 1000000;
	res += x->tv_usec - y->tv_usec;

	return res;
}

int main(int argc, char **argv)
{
	struct v4l2_format fmt;
	int ret = EXIT_FAILURE, fd = -1;
	unsigned char *out_buf = NULL;
	struct timezone tz = { 0 };
	struct timeval start, end;
	long int elapsed;

	argp_parse(&argp, argc, argv, 0, 0, 0);

	if (libv4l) {
		if (block)
			fd = v4l2_open(dev_name, O_RDWR, 0);
		else
			fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
	} else {
		if (block)
			fd = open(dev_name, O_RDWR, 0);
		else
			fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
	}
	if (fd < 0) {
		perror("Cannot open device");
		exit(EXIT_FAILURE);
	}

	/* Check if the device has the needed caps */
	querycap(dev_name, fd, method);

	/* Sets the video format */
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = x_res;
	fmt.fmt.pix.height      = y_res;
	fmt.fmt.pix.pixelformat = fourcc;
	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
	xioctl(fd, VIDIOC_S_FMT, &fmt);

	if (raw_mode) {
		printf("Raw mode: libv4l won't be used.\n");

		libv4l = 0;
		ppm_output = 0;
	} else {
		if (is_format_supported(fmt.fmt.pix.pixelformat)) {
			out_buf = malloc(3 * fmt.fmt.pix.width * fmt.fmt.pix.height);
			if (!out_buf) {
				perror("Cannot allocate memory");
				exit(EXIT_FAILURE);
			}
		} else {
			/* Unknown formats */
			if (libv4l) {
				char *p = (void *)&fmt.fmt.pix.pixelformat;
				printf("Doesn't know how to convert %c%c%c%c to PPM.\n",
				    p[0], p[1], p[2], p[3]);
				exit(EXIT_FAILURE);
			} else {
				printf("File output won't be in PPM format.\n");
				ppm_output = 0;
			}
		}
	}
	if ((fmt.fmt.pix.width != x_res) || (fmt.fmt.pix.height != y_res))
		printf("Warning: driver is sending image at %dx%d\n",
			fmt.fmt.pix.width, fmt.fmt.pix.height);

	/* Calls the desired capture method */
	switch (method) {
	case IO_METHOD_READ:
		ret = read_capture(fd, n_frames, out_buf, out_dir,
				   &fmt, &start);
		break;
	case IO_METHOD_MMAP:
		ret = mmap_capture(fd, n_frames, out_buf, out_dir,
				   threads, sleep_ms, &fmt, &start);
		break;
	case IO_METHOD_USERPTR:
		ret = userptr_capture(fd, n_frames, out_buf, out_dir, &fmt,
				      &start);
		break;
	}

	if (!ret) {
		gettimeofday(&end, &tz);

		elapsed = elapsed_time(&end, &start);

		printf("Received %d frames in %.2f seconds: %.2f fps\n",
		    n_frames, elapsed / 1000000., 1000000. * n_frames/ elapsed);
	}

	/* Closes the file descriptor */
	if (libv4l)
		v4l2_close(fd);
	else
		close(fd);

	return ret;
}
