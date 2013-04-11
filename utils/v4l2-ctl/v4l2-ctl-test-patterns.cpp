#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <string>

#include <linux/videodev2.h>

#include "v4l2-ctl.h"

#define NUM_PATTERNS (4)


/* Note: the code to create the test patterns is derived from the vivi.c kernel
   driver. */

/* Bars and Colors should match positions */

enum colors {
	WHITE,
	AMBER,
	CYAN,
	GREEN,
	MAGENTA,
	RED,
	BLUE,
	BLACK,
	TEXT_BLACK,
};

/* R   G   B */
#define COLOR_WHITE	{204, 204, 204}
#define COLOR_AMBER	{208, 208,   0}
#define COLOR_CYAN	{  0, 206, 206}
#define	COLOR_GREEN	{  0, 239,   0}
#define COLOR_MAGENTA	{239,   0, 239}
#define COLOR_RED	{205,   0,   0}
#define COLOR_BLUE	{  0,   0, 205}
#define COLOR_BLACK	{  0,   0,   0}

struct bar_std {
	__u8 bar[9][3];
};

static struct bar_std bars[NUM_PATTERNS] = {
	{	/* Standard ITU-R color bar sequence */
		{ COLOR_WHITE, COLOR_AMBER, COLOR_CYAN, COLOR_GREEN,
		  COLOR_MAGENTA, COLOR_RED, COLOR_BLUE, COLOR_BLACK, COLOR_BLACK }
	}, {
		{ COLOR_WHITE, COLOR_AMBER, COLOR_BLACK, COLOR_WHITE,
		  COLOR_AMBER, COLOR_BLACK, COLOR_WHITE, COLOR_AMBER, COLOR_BLACK }
	}, {
		{ COLOR_WHITE, COLOR_CYAN, COLOR_BLACK, COLOR_WHITE,
		  COLOR_CYAN, COLOR_BLACK, COLOR_WHITE, COLOR_CYAN, COLOR_BLACK }
	}, {
		{ COLOR_WHITE, COLOR_GREEN, COLOR_BLACK, COLOR_WHITE,
		  COLOR_GREEN, COLOR_BLACK, COLOR_WHITE, COLOR_GREEN, COLOR_BLACK }
	},
};

#define TO_Y(r, g, b) \
	(((16829 * r + 33039 * g + 6416 * b  + 32768) >> 16) + 16)
/* RGB to  V(Cr) Color transform */
#define TO_V(r, g, b) \
	(((28784 * r - 24103 * g - 4681 * b  + 32768) >> 16) + 128)
/* RGB to  U(Cb) Color transform */
#define TO_U(r, g, b) \
	(((-9714 * r - 19070 * g + 28784 * b + 32768) >> 16) + 128)

static __u8 calc_bars[9][3];
static int pixel_size;

/* precalculate color bar values to speed up rendering */
bool precalculate_bars(__u32 pixfmt, unsigned pattern)
{
	__u8 r, g, b;
	int k, is_yuv;

	pattern %= NUM_PATTERNS;
	pixel_size = 2;
	for (k = 0; k < 9; k++) {
		r = bars[pattern].bar[k][0];
		g = bars[pattern].bar[k][1];
		b = bars[pattern].bar[k][2];
		is_yuv = 0;

		switch (pixfmt) {
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_YVYU:
		case V4L2_PIX_FMT_VYUY:
			is_yuv = 1;
			break;
		case V4L2_PIX_FMT_RGB565:
		case V4L2_PIX_FMT_RGB565X:
			r >>= 3;
			g >>= 2;
			b >>= 3;
			break;
		case V4L2_PIX_FMT_RGB555:
		case V4L2_PIX_FMT_RGB555X:
			r >>= 3;
			g >>= 3;
			b >>= 3;
			break;
		case V4L2_PIX_FMT_RGB24:
		case V4L2_PIX_FMT_BGR24:
			pixel_size = 3;
			break;
		case V4L2_PIX_FMT_RGB32:
		case V4L2_PIX_FMT_BGR32:
			pixel_size = 4;
			break;
		default:
			return false;
		}

		if (is_yuv) {
			calc_bars[k][0] = TO_Y(r, g, b);	/* Luma */
			calc_bars[k][1] = TO_U(r, g, b);	/* Cb */
			calc_bars[k][2] = TO_V(r, g, b);	/* Cr */
		} else {
			calc_bars[k][0] = r;
			calc_bars[k][1] = g;
			calc_bars[k][2] = b;
		}
	}
	return true;
}

/* 'odd' is true for pixels 1, 3, 5, etc. and false for pixels 0, 2, 4, etc. */
static void gen_twopix(__u8 *buf, __u32 pixfmt, int colorpos, bool odd)
{
	__u8 r_y, g_u, b_v;
	int color;

	r_y = calc_bars[colorpos][0]; /* R or precalculated Y */
	g_u = calc_bars[colorpos][1]; /* G or precalculated U */
	b_v = calc_bars[colorpos][2]; /* B or precalculated V */

	for (color = 0; color < pixel_size; color++) {
		__u8 *p = buf + color;

		switch (pixfmt) {
		case V4L2_PIX_FMT_YUYV:
			switch (color) {
			case 0:
				*p = r_y;
				break;
			case 1:
				*p = odd ? b_v : g_u;
				break;
			}
			break;
		case V4L2_PIX_FMT_UYVY:
			switch (color) {
			case 0:
				*p = odd ? b_v : g_u;
				break;
			case 1:
				*p = r_y;
				break;
			}
			break;
		case V4L2_PIX_FMT_YVYU:
			switch (color) {
			case 0:
				*p = r_y;
				break;
			case 1:
				*p = odd ? g_u : b_v;
				break;
			}
			break;
		case V4L2_PIX_FMT_VYUY:
			switch (color) {
			case 0:
				*p = odd ? g_u : b_v;
				break;
			case 1:
				*p = r_y;
				break;
			}
			break;
		case V4L2_PIX_FMT_RGB565:
			switch (color) {
			case 0:
				*p = (g_u << 5) | b_v;
				break;
			case 1:
				*p = (r_y << 3) | (g_u >> 3);
				break;
			}
			break;
		case V4L2_PIX_FMT_RGB565X:
			switch (color) {
			case 0:
				*p = (r_y << 3) | (g_u >> 3);
				break;
			case 1:
				*p = (g_u << 5) | b_v;
				break;
			}
			break;
		case V4L2_PIX_FMT_RGB555:
			switch (color) {
			case 0:
				*p = (g_u << 5) | b_v;
				break;
			case 1:
				*p = (r_y << 2) | (g_u >> 3);
				break;
			}
			break;
		case V4L2_PIX_FMT_RGB555X:
			switch (color) {
			case 0:
				*p = (r_y << 2) | (g_u >> 3);
				break;
			case 1:
				*p = (g_u << 5) | b_v;
				break;
			}
			break;
		case V4L2_PIX_FMT_RGB24:
			switch (color) {
			case 0:
				*p = r_y;
				break;
			case 1:
				*p = g_u;
				break;
			case 2:
				*p = b_v;
				break;
			}
			break;
		case V4L2_PIX_FMT_BGR24:
			switch (color) {
			case 0:
				*p = b_v;
				break;
			case 1:
				*p = g_u;
				break;
			case 2:
				*p = r_y;
				break;
			}
			break;
		case V4L2_PIX_FMT_RGB32:
			switch (color) {
			case 0:
				*p = 0;
				break;
			case 1:
				*p = r_y;
				break;
			case 2:
				*p = g_u;
				break;
			case 3:
				*p = b_v;
				break;
			}
			break;
		case V4L2_PIX_FMT_BGR32:
			switch (color) {
			case 0:
				*p = b_v;
				break;
			case 1:
				*p = g_u;
				break;
			case 2:
				*p = r_y;
				break;
			case 3:
				*p = 0;
				break;
			}
			break;
		}
	}
}

void fill_buffer(void *buffer, struct v4l2_pix_format *pix)
{
	for (unsigned y = 0; y < pix->height; y++) {
		__u8 *ptr = (__u8 *)buffer + y * pix->bytesperline;

		for (unsigned x = 0; x < pix->width; x++) {
			int colorpos = x / (pix->width / 8) % 8;

			gen_twopix(ptr + x * pixel_size, pix->pixelformat, colorpos, x & 1);
		}
	}
}
