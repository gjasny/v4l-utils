/*
    V4L2 API compliance color checking tests.

    Copyright (C) 2015  Hans Verkuil <hverkuil@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <math.h>
#include "v4l2-compliance.h"

static void setupPlanes(const cv4l_fmt &fmt, __u8 *planes[3])
{
	if (fmt.g_num_planes() > 1)
		return;

	unsigned bpl = fmt.g_bytesperline();
	unsigned h = fmt.g_height();
	unsigned size = bpl * h;

	switch (fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		planes[1] = planes[0] + size;
		planes[2] = planes[1] + size / 4;
		break;
	case V4L2_PIX_FMT_YUV422P:
		planes[1] = planes[0] + size;
		planes[2] = planes[1] + size / 2;
		break;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
		planes[1] = planes[0] + size;
		break;
	default:
		break;
	}
}

struct color {
	double r, g, b, a;
};

static const double bt601[3][3] = {
	{ 1, 0,       1.4020  },
	{ 1, -0.3441, -0.7141 },
	{ 1, 1.7720,  0       },
};
static const double rec709[3][3] = {
	{ 1, 0,       1.5748  },
	{ 1, -0.1873, -0.4681 },
	{ 1, 1.8556,  0       },
};
static const double smpte240m[3][3] = {
	{ 1, 0,       1.5756  },
	{ 1, -0.2253, -0.4767 },
	{ 1, 1.8270,  0       },
};
static const double bt2020[3][3] = {
	{ 1, 0,       1.4746  },
	{ 1, -0.1646, -0.5714 },
	{ 1, 1.8814,  0       },
};

static void ycbcr2rgb(const double m[3][3], double y, double cb, double cr,
			color &c)
{
	c.r = m[0][0] * y + m[0][1] * cb + m[0][2] * cr;
	c.g = m[1][0] * y + m[1][1] * cb + m[1][2] * cr;
	c.b = m[2][0] * y + m[2][1] * cb + m[2][2] * cr;
}

static void getColor(const cv4l_fmt &fmt, __u8 * const planes[3],
		unsigned y, unsigned x, color &c)
{
	unsigned bpl = fmt.g_bytesperline();
	unsigned yeven = y & ~1;
	unsigned xeven = x & ~1;
	unsigned offset = bpl * y;
	unsigned hoffset = (bpl / 2) * y;
	unsigned voffset = bpl * (y / 2);
	unsigned hvoffset = (bpl / 2) * (y / 2);
	const __u8 *p8 = planes[0] + offset;
	__u8 v8 = 0;
	__u16 v16 = 0;
	__u32 v32 = 0;

	/* component order: ARGB or AYUV */
	switch (fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_RGB332:
		v8 = p8[x];
		break;
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_XRGB444:
	case V4L2_PIX_FMT_ARGB444:
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_YUV565:
		v16 = p8[2 * x] + (p8[2 * x + 1] << 8);
		break;
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
	case V4L2_PIX_FMT_ARGB555X:
		v16 = p8[2 * x + 1] + (p8[2 * x] << 8);
		break;
	case V4L2_PIX_FMT_RGB24:
		v32 = p8[3 * x + 2] + (p8[3 * x + 1] << 8) +
		      (p8[3 * x] << 16);
		break;
	case V4L2_PIX_FMT_BGR24:
		v32 = p8[3 * x] + (p8[3 * x + 1] << 8) +
		      (p8[3 * x + 2] << 16);
		break;
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_YUV32:
		v32 = p8[4 * x + 3] + (p8[4 * x + 2] << 8) +
		      (p8[4 * x + 1] << 16) + (p8[4 * x] << 24);
		break;
	case V4L2_PIX_FMT_BGR666:
		v32 = ((p8[4 * x + 2] & 0xc0) << 10) + ((p8[4 * x + 1] & 0xf) << 18) +
		      ((p8[4 * x + 1] & 0xf0) << 4) +
		      ((p8[4 * x] & 0x3) << 12) + ((p8[4 * x] & 0xfc) >> 2);
		break;
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_ABGR32:
		v32 = p8[4 * x] + (p8[4 * x + 1] << 8) +
		      (p8[4 * x + 2] << 16) + (p8[4 * x + 3] << 24);
		break;
	case V4L2_PIX_FMT_SBGGR8:
		p8 = planes[0] + bpl * yeven + xeven;
		v32 = p8[0] + (p8[(y & 1) * bpl + 1 - (y & 1)] << 8) + (p8[bpl + 1] << 16);
		break;
	case V4L2_PIX_FMT_SGBRG8:
		p8 = planes[0] + bpl * yeven + xeven;
		v32 = (p8[bpl] << 16) + (p8[(y & 1) * bpl + (y & 1)] << 8) + p8[1];
		break;
	case V4L2_PIX_FMT_SGRBG8:
		p8 = planes[0] + bpl * yeven + xeven;
		v32 = p8[bpl] + (p8[(y & 1) * bpl + (y & 1)] << 8) + (p8[1] << 16);
		break;
	case V4L2_PIX_FMT_SRGGB8:
		p8 = planes[0] + bpl * yeven + xeven;
		v32 = (p8[0] << 16) + (p8[(y & 1) * bpl + 1 - (y & 1)] << 8) + p8[bpl + 1];
		break;
	case V4L2_PIX_FMT_YUYV:
		v32 = (p8[2 * x] << 16) + (p8[2 * xeven + 1] << 8) + p8[2 * xeven + 3];
		break;
	case V4L2_PIX_FMT_UYVY:
		v32 = (p8[2 * x + 1] << 16) + (p8[2 * xeven] << 8) + p8[2 * xeven + 2];
		break;
	case V4L2_PIX_FMT_YVYU:
		v32 = (p8[2 * x] << 16) + (p8[2 * xeven + 3] << 8) + p8[2 * xeven + 1];
		break;
	case V4L2_PIX_FMT_VYUY:
		v32 = (p8[2 * x + 1] << 16) + (p8[2 * xeven + 2] << 8) + p8[2 * xeven];
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV12M:
		v32 = (p8[x] << 16) +
		      (planes[1][voffset + xeven] << 8) +
		      planes[1][voffset + xeven + 1];
		break;
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV21M:
		v32 = (p8[x] << 16) +
		      (planes[1][voffset + xeven + 1] << 8) +
		      planes[1][voffset + xeven];
		break;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV16M:
		v32 = (p8[x] << 16) +
		      (planes[1][offset + xeven] << 8) +
		      planes[1][offset + xeven + 1];
		break;
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV61M:
		v32 = (p8[x] << 16) +
		      (planes[1][offset + xeven + 1] << 8) +
		      planes[1][offset + xeven];
		break;
	case V4L2_PIX_FMT_YVU422M:
		v32 = (p8[x] << 16) +
		      (planes[2][hoffset + x / 2] << 8) +
		      planes[1][hoffset + x / 2];
		break;
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YUV422P:
		v32 = (p8[x] << 16) +
		      (planes[1][hoffset + x / 2] << 8) +
		      planes[2][hoffset + x / 2];
		break;
	case V4L2_PIX_FMT_YUV444M:
		v32 = (p8[x] << 16) + (planes[1][offset + x] << 8) +
		      planes[2][offset + x];
		break;
	case V4L2_PIX_FMT_YVU444M:
		v32 = (p8[x] << 16) + (planes[2][offset + x] << 8) +
		      planes[1][offset + x];
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YUV420M:
		v32 = (p8[x] << 16) +
		      (planes[1][hvoffset + x / 2] << 8) +
		      planes[2][hvoffset + x / 2];
		break;
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YVU420M:
		v32 = (p8[x] << 16) +
		      (planes[2][hvoffset + x / 2] << 8) +
		      planes[1][hvoffset + x / 2];
		break;
	case V4L2_PIX_FMT_NV24:
		v32 = (p8[x] << 16) +
		      (planes[1][bpl * 2 * y + 2 * x] << 8) +
		      planes[1][bpl * 2 * y + 2 * x + 1];
		break;
	case V4L2_PIX_FMT_NV42:
		v32 = (p8[x] << 16) +
		      (planes[1][bpl * 2 * y + 2 * x + 1] << 8) +
		      planes[1][bpl * 2 * y + 2 * x];
		break;
	}

	switch (fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_RGB332:
		c.r = (v8 >> 5) / 7.0;
		c.g = ((v8 >> 2) & 7) / 7.0;
		c.b = (v8 & 3) / 3.0;
		break;
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_YUV565:
		c.r = (v16 >> 11) / 31.0;
		c.g = ((v16 >> 5) & 0x3f) / 63.0;
		c.b = (v16 & 0x1f) / 31.0;
		break;
	case V4L2_PIX_FMT_ARGB444:
		c.a = (v16 >> 12) / 15.0;
		/* fall through */
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_XRGB444:
		c.r = ((v16 >> 8) & 0xf) / 15.0;
		c.g = ((v16 >> 4) & 0xf) / 15.0;
		c.b = (v16 & 0xf) / 15.0;
		break;
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_ARGB555X:
		c.a = v16 >> 15;
		/* fall through */
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
		c.r = ((v16 >> 10) & 0x1f) / 31.0;
		c.g = ((v16 >> 5) & 0x1f) / 31.0;
		c.b = (v16 & 0x1f) / 31.0;
		break;
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_VYUY:
		c.r = (v32 >> 16) / 255.0;
		c.g = ((v32 >> 8) & 0xff) / 255.0;
		c.b = (v32 & 0xff) / 255.0;
		break;
	case V4L2_PIX_FMT_BGR666:
		c.r = ((v32 >> 16) & 0x3f) / 63.0;
		c.g = ((v32 >> 8) & 0x3f) / 63.0;
		c.b = (v32 & 0x3f) / 63.0;
		break;
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_ABGR32:
		c.a = ((v32 >> 24) & 0xff) / 255.0;
		/* fall through */
	default:
		c.r = ((v32 >> 16) & 0xff) / 255.0;
		c.g = ((v32 >> 8) & 0xff) / 255.0;
		c.b = (v32 & 0xff) / 255.0;
		break;
	}

	switch (fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_YUV565:
	case V4L2_PIX_FMT_YUV32:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV61M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
		break;
	default:
		if (fmt.g_quantization() == V4L2_QUANTIZATION_LIM_RANGE ||
		    (fmt.g_colorspace() == V4L2_YCBCR_ENC_BT2020 &&
		     fmt.g_quantization() == V4L2_QUANTIZATION_DEFAULT)) {
			c.r = (c.r - 16.0 / 255.0) * 255.0 / 219.0;
			c.g = (c.g - 16.0 / 255.0) * 255.0 / 219.0;
			c.b = (c.b - 16.0 / 255.0) * 255.0 / 219.0;
		}

		return;
	}

	double Y = c.r;
	double cb = c.g - 0.5;
	double cr = c.b - 0.5;

	if (fmt.g_quantization() != V4L2_QUANTIZATION_FULL_RANGE) {
		Y = (Y - 16.0 / 255.0) * 255.0 / 219.0;
		cb *= 255.0 / 224.0;
		cr *= 255.0 / 224.0;
	}

	switch (fmt.g_ycbcr_enc()) {
	case V4L2_YCBCR_ENC_XV601:
		Y = (Y - 16.0 / 255.0) * 255.0 / 219.0;
		cb *= 255.0 / 224.0;
		cr *= 255.0 / 224.0;
		/* fall through */
	case V4L2_YCBCR_ENC_601:
	default:
		ycbcr2rgb(bt601, Y, cb, cr, c);
		break;
	case V4L2_YCBCR_ENC_XV709:
		Y = (Y - 16.0 / 255.0) * 255.0 / 219.0;
		cb *= 255.0 / 224.0;
		cr *= 255.0 / 224.0;
		/* fall through */
	case V4L2_YCBCR_ENC_709:
		ycbcr2rgb(rec709, Y, cb, cr, c);
		break;
	case V4L2_YCBCR_ENC_SMPTE240M:
		ycbcr2rgb(smpte240m, Y, cb, cr, c);
		break;
	/*
	 * For now just interpret BT2020_CONST_LUM as BT2020.
	 * It's pretty complex to handle this correctly, so
	 * for now approximate it with BT2020.
	 */
	case V4L2_YCBCR_ENC_BT2020_CONST_LUM:
	case V4L2_YCBCR_ENC_BT2020:
		ycbcr2rgb(bt2020, Y, cb, cr, c);
		break;
	}
}

static const char * const colors[] = {
	"red",
	"green",
	"blue"
};

static int testColorsFmt(struct node *node, unsigned component,
		unsigned skip, unsigned perc)
{
	cv4l_queue q;
	cv4l_fmt fmt;
	__u8 *planes[3] = { 0, 0, 0 };
	skip++;

	node->g_fmt(fmt);

	if (node->g_caps() & V4L2_CAP_STREAMING) {
		cv4l_buffer buf;

		q.init(node->g_type(), V4L2_MEMORY_MMAP);
		buf.init(q);
		fail_on_test(q.reqbufs(node, 3));
		fail_on_test(q.obtain_bufs(node));
		fail_on_test(q.queue_all(node));
		fail_on_test(node->streamon());

		while (node->dqbuf(buf) == 0) {
			if (--skip == 0)
				break;
			fail_on_test(node->qbuf(buf));
		}
		fail_on_test(skip);
		for (unsigned i = 0; i < fmt.g_num_planes(); i++)
			planes[i] = (__u8 *)q.g_dataptr(buf.g_index(), i);

	} else {
		fail_on_test(!(node->g_caps() & V4L2_CAP_READWRITE));

		int size = fmt.g_sizeimage();
		void *tmp = malloc(size);

		for (unsigned i = 0; i < skip; i++) {
			int ret;

			ret = node->read(tmp, size);
			fail_on_test(ret != size);
		}
		planes[0] = (__u8 *)tmp;
	}

	setupPlanes(fmt, planes);

	if (fmt.g_ycbcr_enc() == V4L2_YCBCR_ENC_DEFAULT) {
		switch (fmt.g_colorspace()) {
		case V4L2_COLORSPACE_REC709:
			fmt.s_ycbcr_enc(V4L2_YCBCR_ENC_709);
			break;
		case V4L2_COLORSPACE_SMPTE240M:
			fmt.s_ycbcr_enc(V4L2_YCBCR_ENC_SMPTE240M);
			break;
		case V4L2_COLORSPACE_BT2020:
			fmt.s_ycbcr_enc(V4L2_YCBCR_ENC_BT2020);
			break;
		default:
			fmt.s_ycbcr_enc(V4L2_YCBCR_ENC_601);
			break;
		}
	}

	unsigned h = fmt.g_height();
	unsigned w = fmt.g_width();
	unsigned color_cnt[3] = { 0, 0, 0 };
	v4l2_std_id std;
	bool is_50hz = false;
	unsigned total;

	if ((node->cur_io_caps & V4L2_IN_CAP_STD) &&
	    !node->g_std(std) && (std & V4L2_STD_625_50))
		is_50hz = true;

	total = w * h - (is_50hz ? w / 2 : 0);

	for (unsigned y = 0; y < h; y++) {
		/*
		 * 50 Hz (PAL/SECAM) formats have a garbage first half-line,
		 * so skip that.
		 */
		for (unsigned x = (y == 0 && is_50hz) ? w / 2 : 0; x < w; x++) {
			color c = { 0, 0, 0, 0 };

			getColor(fmt, planes, y, x, c);
			if (c.r > c.b && c.r > c.g)
				color_cnt[0]++;
			else if (c.g > c.r && c.g > c.b)
				color_cnt[1]++;
			else
				color_cnt[2]++;
		}
	}
	if (node->g_caps() & V4L2_CAP_STREAMING)
		q.free(node);
	else
		free(planes[0]);

	if (color_cnt[component] < total * perc / 100) {
		return fail("red: %u%% green: %u%% blue: %u%% expected: %s >= %u%%\n",
			color_cnt[0] * 100 / total,
			color_cnt[1] * 100 / total,
			color_cnt[2] * 100 / total,
			colors[component], perc);
	}
	return 0;
}

int testColorsAllFormats(struct node *node, unsigned component,
			 unsigned skip, unsigned perc)
{
	v4l2_fmtdesc fmtdesc;

	if (node->enum_fmt(fmtdesc, true))
		return 0;
	do {
		if (fmtdesc.flags & V4L2_FMT_FLAG_COMPRESSED)
			continue;
		switch (fmtdesc.pixelformat) {
		case V4L2_PIX_FMT_GREY:
		case V4L2_PIX_FMT_Y4:
		case V4L2_PIX_FMT_Y6:
		case V4L2_PIX_FMT_Y10:
		case V4L2_PIX_FMT_Y12:
		case V4L2_PIX_FMT_Y16:
		case V4L2_PIX_FMT_Y16_BE:
		case V4L2_PIX_FMT_Y10BPACK:
		case V4L2_PIX_FMT_PAL8:
		case V4L2_PIX_FMT_UV8:
			continue;
		}

		restoreFormat(node);

		cv4l_fmt fmt;
		node->g_fmt(fmt);
		fmt.s_pixelformat(fmtdesc.pixelformat);
		node->s_fmt(fmt);
		printf("\ttest %u%% %s for format %s: %s\n",
				perc, colors[component],
				pixfmt2s(fmt.g_pixelformat()).c_str(),
				ok(testColorsFmt(node, component, skip, perc)));
	} while (!node->enum_fmt(fmtdesc));
	printf("\n");
	return 0;
}
