/*
    V4L2 pixfmt test

    Copyright (C) 2007, 2008 Michael H. Schimek <mschimek@gmx.at>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define _GNU_SOURCE 1

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#undef MAX
#define MAX(x, y) ({							\
	__typeof__ (x) _x = (x);					\
	__typeof__ (y) _y = (y);					\
	(void)(&_x == &_y); /* alert when type mismatch */		\
	(_x > _y) ? _x : _y;						\
})

#define N_ELEMENTS(array) (sizeof (array) / sizeof ((array)[0]))
#define CLEAR(var) memset (&(var), 0, sizeof (var))

typedef enum {
	/* Packed RGB formats. */

				/* in memory */
	BGRA8888_LE = 1,	/* bbbbbbbb gggggggg rrrrrrrr aaaaaaaa */
	BGRA8888_BE,		/* aaaaaaaa rrrrrrrr gggggggg bbbbbbbb */
	RGBA8888_LE,		/* rrrrrrrr gggggggg bbbbbbbb aaaaaaaa */
	RGBA8888_BE,		/* aaaaaaaa bbbbbbbb gggggggg rrrrrrrr */

	BGR888_LE,		/* bbbbbbbb gggggggg rrrrrrrr */
	BGR888_BE,		/* rrrrrrrr gggggggg bbbbbbbb */

	BGR565_LE,		/* gggbbbbb rrrrrggg */
	BGR565_BE,		/* rrrrrggg gggbbbbb */
	RGB565_LE,		/* gggrrrrr bbbbbggg */
	RGB565_BE,		/* bbbbbggg gggrrrrr */

	BGRA5551_LE,		/* gggbbbbb arrrrrgg */
	BGRA5551_BE,		/* arrrrrgg gggbbbbb */
	RGBA5551_LE,		/* gggrrrrr abbbbbgg */
	RGBA5551_BE,		/* abbbbbgg gggrrrrr */

	ABGR1555_LE,		/* ggbbbbba rrrrrggg */
	ABGR1555_BE,		/* rrrrrggg ggbbbbba */
	ARGB1555_LE,		/* ggrrrrra bbbbbggg */
	ARGB1555_BE,		/* bbbbbggg ggrrrrra */

	BGRA4444_LE,		/* ggggbbbb aaaarrrr */
	BGRA4444_BE,		/* aaaarrrr ggggbbbb */
	RGBA4444_LE,		/* ggggrrrr aaaabbbb */
	RGBA4444_BE,		/* aaaabbbb ggggrrrr */

	ABGR4444_LE,		/* bbbbaaaa rrrrgggg */
	ABGR4444_BE,		/* rrrrgggg bbbbaaaa */
	ARGB4444_LE,		/* rrrraaaa bbbbgggg */
	ARGB4444_BE,		/* bbbbgggg rrrraaaa */

	BGR233,			/* rrrgggbb */
	RGB332,			/* bbgggrrr */

	/* Bayer formats. */

	BGGR8,			/* bbbbbbbb gggggggg */
				/* gggggggg rrrrrrrr */
	GBRG8,			/* gggggggg bbbbbbbb */
				/* rrrrrrrr gggggggg */
	RGGB8,			/* rrrrrrrr gggggggg */
				/* gggggggg bbbbbbbb */
	GRBG8,			/* gggggggg rrrrrrrr */
				/* bbbbbbbb gggggggg */

	BGGR16,			/* b7...b0 b15...b8 g7...g0 g15...g8 */
				/* g7...g0 g15...g8 r7...r0 r15...r8 */
	GBRG16,			/* g7...g0 g15...g8 b7...b0 b15...b8 */
				/* r7...r0 r15...r8 g7...g0 g15...g8 */
	RGGB16,			/* r7...r0 r15...r8 g7...g0 g15...g8 */
				/* g7...g0 g15...g8 b7...b0 b15...b8 */
	GRBG16,			/* g7...g0 g15...g8 r7...r0 r15...r8 */
				/* b7...b0 b15...b8 g7...g0 g15...g8 */
} pixfmt;

/* A pixfmt set would be nicer, but I doubt all
   YUV and RGB formats will fit in 64 bits. */
typedef enum {
	PACKED_RGB	= (1 << 0),
	BAYER		= (1 << 1)
} pixfmt_class;

typedef enum {
	LE = 1,
	BE
} byte_order;

typedef struct {
	/* Our name for this format. */
	const char *		name;

	/* V4L2's name "V4L2_PIX_FMT_..." or NULL. */
	const char *		v4l2_fourcc_name;

	/* Our ID for this format. */
	pixfmt			pixfmt;

	/* Same pixfmt with opposite byte order.
	   Applies only to packed RGB formats. */
	pixfmt			pixfmt_opposite_byte_order;

	/* Same pixfmt with red and blue bits swapped.
	   Applies only to RGB formats. */
	pixfmt			pixfmt_swap_red_blue;

	/* Same pixfmt with alpha bits at the other end.
	   Applies only to packed RGB formats. */
	pixfmt			pixfmt_opposite_alpha;

	pixfmt_class		pixfmt_class;

	/* V4L2's FOURCC or 0. */
	uint32_t		v4l2_fourcc;

	/* LE or BE. Applies only to packed RGB formats. */
	byte_order		byte_order;

	/* Applies only to RGB formats. */
	uint8_t			bits_per_pixel;

	/* Number of blue, green and red bits per pixel.
	   Applies only to RGB formats. */
	uint8_t			color_depth;

	/* Blue, green, red, alpha bit masks.
	   Applies only to packed RGB formats. */
	uint32_t		mask[4];

	/* Number of blue, green, red, alpha bits.
	   Applies only to packed RGB formats. */
	uint8_t			n_bits[4];

	/* Number of zero bits above the blue, green, red, alpha MSB.
	   E.g. 0x80001234 -> 0, 0x00000001 -> 31, 0 -> 32.
	   Applies only to packed RGB formats. */
	uint8_t			shr[4];

} pixel_format;

/* Population count in 32 bit constant, e.g. 0x70F -> 7. */
#define PC32b(m) ((m) - (((m) >> 1) & 0x55555555))
#define PC32a(m) ((PC32b (m) & 0x33333333) + ((PC32b (m) >> 2) & 0x33333333))
#define PC32(m) ((((uint64_t)((PC32a (m) & 0x0F0F0F0F)			\
			      + ((PC32a (m) >> 4) & 0x0F0F0F0F))	\
		   * 0x01010101) >> 24) & 0xFF)

/* Find first set bit in 32 bit constant, see man 3 ffs(). */
#define FFS2(m) ((m) & 0x2 ? 2 : (m))
#define FFS4(m) ((m) & 0xC ? 2 + FFS2 ((m) >> 2) : FFS2 (m))
#define FFS8(m) ((m) & 0xF0 ? 4 + FFS4 ((m) >> 4) : FFS4 (m))
#define FFS16(m) ((m) & 0xFF00 ? 8 + FFS8 ((m) >> 8) : FFS8 (m))
#define FFS32(m) ((m) & 0xFFFF0000 ? 16 + FFS16 ((m) >> 16) : FFS16 (m))

#define PF_RGB(tn, vn, pf, pfxbo, pfxrb, pfxa, vpf, bo, b, g, r, a)	\
	[pf] = {							\
		.name = tn,						\
		.v4l2_fourcc_name = (0 == vpf) ? NULL : vn,		\
		.pixfmt = pf,						\
		.pixfmt_opposite_byte_order = pfxbo,			\
		.pixfmt_swap_red_blue = pfxrb,				\
		.pixfmt_opposite_alpha = pfxa,				\
		.pixfmt_class = PACKED_RGB,				\
		.v4l2_fourcc = vpf,					\
		.byte_order = bo,					\
		.bits_per_pixel = PC32 ((b) | (g) | (r) | (a)),		\
		.color_depth = PC32 ((b) | (g) | (r)),			\
		.mask = { b, g, r, a },					\
		.n_bits = { PC32 (b), PC32 (g), PC32 (r), PC32 (a) },	\
		.shr = { 32 - FFS32 (b), 32 - FFS32 (g),		\
			 32 - FFS32 (r), 32 - FFS32 (a) }		\
	}

#define PF_RGB8(pf, pfxrb, vpf, b, g, r, a)				\
	PF_RGB (# pf, # vpf, pf, pf, pfxrb, 0, vpf, LE, b, g, r, a)

#define PF_RGB16(fmt, bo, pfxrb, pfxa, vpf, b, g, r, a)			\
	PF_RGB (# fmt "_" # bo, # vpf,					\
		fmt ## _ ## bo,						\
		(bo == LE) ? fmt ## _ ## BE : fmt ## _ ## LE,		\
		pfxrb, pfxa, vpf, bo, b, g, r, a)

#define PF_RGB24 PF_RGB16
#define PF_RGB32 PF_RGB16

#define PF_BAYER(pf, pfxrb, bpp, vpf)					\
	[pf] = {							\
		.name = # pf,						\
		.v4l2_fourcc_name = (0 == vpf) ? NULL : # vpf,		\
		.pixfmt = pf,						\
		.pixfmt_opposite_byte_order = pf,			\
		.pixfmt_swap_red_blue = pfxrb,				\
		.pixfmt_opposite_alpha = pf,				\
		.pixfmt_class = BAYER,					\
		.v4l2_fourcc = vpf,					\
		.byte_order = LE,					\
		.bits_per_pixel = bpp,					\
		.color_depth = bpp * 3 /* sort of */			\
	}

static const pixel_format
pixel_formats [] = {
	PF_RGB32 (BGRA8888, LE, RGBA8888_LE, RGBA8888_BE,
		  V4L2_PIX_FMT_BGR32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000),
	PF_RGB32 (BGRA8888, BE, RGBA8888_BE, RGBA8888_LE,
		  V4L2_PIX_FMT_RGB32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000),
	PF_RGB32 (RGBA8888, LE, BGRA8888_LE, BGRA8888_BE,
		  0, 0xFF0000, 0xFF00, 0xFF, 0xFF000000),
	PF_RGB32 (RGBA8888, BE, BGRA8888_BE, BGRA8888_LE,
		  0, 0xFF0000, 0xFF00, 0xFF, 0xFF000000),

	PF_RGB24 (BGR888, LE, BGR888_BE, 0,
		  V4L2_PIX_FMT_BGR24, 0xFF, 0xFF00, 0xFF0000, 0),
	PF_RGB24 (BGR888, BE, BGR888_LE, 0,
		  V4L2_PIX_FMT_RGB24, 0xFF, 0xFF00, 0xFF0000, 0),

	PF_RGB16 (BGR565, LE, RGB565_LE, 0,
		  V4L2_PIX_FMT_RGB565, 0x001F, 0x07E0, 0xF800, 0),
	PF_RGB16 (BGR565, BE, RGB565_BE, 0,
		  V4L2_PIX_FMT_RGB565X, 0x001F, 0x07E0, 0xF800, 0),
	PF_RGB16 (RGB565, LE, BGR565_LE, 0, 0, 0xF800, 0x07E0, 0x001F, 0),
	PF_RGB16 (RGB565, BE, BGR565_BE, 0, 0, 0xF800, 0x07E0, 0x001F, 0),

	PF_RGB16 (BGRA5551, LE, RGBA5551_LE, ABGR1555_LE,
		  V4L2_PIX_FMT_RGB555, 0x001F, 0x03E0, 0x7C00, 0x8000),
	PF_RGB16 (BGRA5551, BE, RGBA5551_BE, ABGR1555_BE,
		  V4L2_PIX_FMT_RGB555X, 0x001F, 0x03E0, 0x7C00, 0x8000),
	PF_RGB16 (RGBA5551, LE, BGRA5551_LE, ARGB1555_LE,
		  0, 0x7C00, 0x03E0, 0x001F, 0x8000),
	PF_RGB16 (RGBA5551, BE, BGRA5551_BE, ARGB1555_BE,
		  0, 0x7C00, 0x03E0, 0x001F, 0x8000),

	PF_RGB16 (ABGR1555, LE, ARGB1555_LE, BGRA5551_LE,
		  0, 0x003E, 0x07C0, 0xF800, 0x0001),
	PF_RGB16 (ABGR1555, BE, ARGB1555_BE, BGRA5551_BE,
		  0, 0x003E, 0x07C0, 0xF800, 0x0001),
	PF_RGB16 (ARGB1555, LE, ABGR1555_LE, RGBA5551_LE,
		  0, 0xF800, 0x07C0, 0x003E, 0x0001),
	PF_RGB16 (ARGB1555, BE, ABGR1555_BE, RGBA5551_BE,
		  0, 0xF800, 0x07C0, 0x003E, 0x0001),

	PF_RGB16 (BGRA4444, LE, RGBA4444_LE, ABGR4444_LE,
		  V4L2_PIX_FMT_RGB444, 0x000F, 0x00F0, 0x0F00, 0xF000),
	PF_RGB16 (BGRA4444, BE, RGBA4444_BE, ABGR4444_BE,
		  0, 0x000F, 0x00F0, 0x0F00, 0xF000),
	PF_RGB16 (RGBA4444, LE, BGRA4444_LE, ARGB4444_LE,
		  0, 0x0F00, 0x00F0, 0x000F, 0xF000),
	PF_RGB16 (RGBA4444, BE, BGRA4444_BE, ARGB4444_BE,
		  0, 0x0F00, 0x00F0, 0x000F, 0xF000),

	PF_RGB16 (ABGR4444, LE, ARGB4444_LE, BGRA4444_LE,
		  0, 0x00F0, 0x0F00, 0xF000, 0x000F),
	PF_RGB16 (ABGR4444, BE, ARGB4444_BE, BGRA4444_BE,
		  0, 0x00F0, 0x0F00, 0xF000, 0x000F),
	PF_RGB16 (ARGB4444, LE, ABGR4444_LE, RGBA4444_LE,
		  0, 0xF000, 0x0F00, 0x00F0, 0x000F),
	PF_RGB16 (ARGB4444, BE, ABGR4444_BE, RGBA4444_BE,
		  0, 0xF000, 0x0F00, 0x00F0, 0x000F),

	PF_RGB8 (BGR233, RGB332,
		 V4L2_PIX_FMT_RGB332, 0x03, 0x1C, 0xE0, 0),
	PF_RGB8 (RGB332, BGR233,
		 0, 0xE0, 0x1C, 0x03, 0),

	PF_BAYER (BGGR8, RGGB8, 8, V4L2_PIX_FMT_SBGGR8),
	PF_BAYER (RGGB8, BGGR8, 8, 0),
	PF_BAYER (GBRG8, GRBG8, 8, 0),
	PF_BAYER (GRBG8, GBRG8, 8, 0),

	PF_BAYER (BGGR16, RGGB16, 16, V4L2_PIX_FMT_SBGGR16),
	PF_BAYER (RGGB16, BGGR16, 16, 0),
	PF_BAYER (GBRG16, GRBG16, 16, 0),
	PF_BAYER (GRBG16, GBRG16, 16, 0),
};

static const pixel_format *
find_v4l2_fourcc		(uint32_t		fourcc)
{
	const pixel_format *pf;

	for (pf = pixel_formats;
	     pf < pixel_formats + N_ELEMENTS (pixel_formats); ++pf) {
		if (fourcc == pf->v4l2_fourcc)
			return pf;
	}

	return NULL;
}

static const pixel_format *
next_converter			(const pixel_format *	pf)
{
	const pixel_format *next_pf;

	if (NULL == pf)
		pf = pixel_formats;
	else
		pf = pixel_formats + pf->pixfmt;

	next_pf = pf;

	for (;;) {
		if (++next_pf >= pixel_formats + N_ELEMENTS (pixel_formats))
			next_pf = pixel_formats;

		if (next_pf == pf)
			break;

		if (0 == next_pf->pixfmt)
			continue;

		if (pf->pixfmt_class == next_pf->pixfmt_class
		    && pf->bits_per_pixel == next_pf->bits_per_pixel)
			break;
	}

	return next_pf;
}

typedef enum {
	IO_METHOD_READ = 1,
	IO_METHOD_MMAP,
} io_methods;

typedef struct {
	void *			start;
	size_t			length;
} io_buffer;

static const char *		my_name;

static const char *		dev_name = "/dev/video";

static int			dev_fd;
static v4l2_std_id		std_id;
static io_methods		io_method;
static struct v4l2_format	fmt;
static io_buffer *		buffers;
static unsigned int		n_buffers;

static Display *		display;
static int			screen;
static Window			window;
static GC			gc;
static Atom			xa_delete_window;

static XImage *			ximage;
static const pixel_format *	ximage_pf;

static void
error_exit			(const char *		templ,
				 ...)
{
	va_list ap;

	fprintf (stderr, "%s: ", my_name);
	va_start (ap, templ);
	vfprintf (stderr, templ, ap);
	va_end (ap);

	exit (EXIT_FAILURE);
}

static void
errno_exit			(const char *		s)
{
	error_exit ("%s error %d, %s\n",
		    s, errno, strerror (errno));
}

static void
write_rgb_pixel			(uint8_t *		dst,
				 const pixel_format *	dst_pf,
				 unsigned int		b,
				 unsigned int		g,
				 unsigned int		r,
				 unsigned int		depth)
{
	unsigned int dst_pixel;
	unsigned int shl;

	shl = 32 - depth;
	dst_pixel  = ((b << shl) >> dst_pf->shr[0]) & dst_pf->mask[0];
	dst_pixel |= ((g << shl) >> dst_pf->shr[1]) & dst_pf->mask[1];
	dst_pixel |= ((r << shl) >> dst_pf->shr[2]) & dst_pf->mask[2];

	switch (dst_pf->byte_order * 256 + dst_pf->bits_per_pixel) {
	case LE * 256 + 32:
		dst[3] = dst_pixel >> 24;
		/* fall through */
	case LE * 256 + 24:
		dst[2] = dst_pixel >> 16;
		/* fall through */
	case LE * 256 + 16:
		dst[1] = dst_pixel >> 8;
		/* fall through */
	case LE * 256 + 8:
		dst[0] = dst_pixel;
		break;

	case BE * 256 + 32:
		*dst++ = dst_pixel >> 24;
		/* fall through */
	case BE * 256 + 24:
		*dst++ = dst_pixel >> 16;
		/* fall through */
	case BE * 256 + 16:
		*dst++ = dst_pixel >> 8;
		/* fall through */
	case BE * 256 + 8:
		*dst = dst_pixel;
		break;

	default:
		assert (0);
		break;
	}
}

static void
convert_bayer8_image		(uint8_t *		dst,
				 const pixel_format *	dst_pf,
				 unsigned long		dst_bpl,
				 const uint8_t *	src,
				 const pixel_format *	src_pf,
				 unsigned long		src_bpl,
				 unsigned int		width,
				 unsigned int		height)
{
	unsigned long dst_padding;
	unsigned int tile;
	unsigned int y;

	assert (PACKED_RGB == dst_pf->pixfmt_class);
	assert (BAYER == src_pf->pixfmt_class);

	assert (width >= 2 && 0 == (width & 1));
	assert (height >= 2 && 0 == (height & 1));

	dst_padding = dst_bpl - width * (dst_pf->bits_per_pixel >> 3);
	assert ((long) dst_padding >= 0);

	switch (src_pf->pixfmt) {
	case BGGR8:
		tile = 0;
		break;

	case GBRG8:
		tile = 1;
		break;

	case RGGB8:
		tile = 2;
		break;

	case GRBG8:
		tile = 3;
		break;

	default:
		assert (0);
		break;
	}

	for (y = 0; y < height; ++y) {
		const uint8_t *srcm;
		const uint8_t *srcp;
		unsigned int x;

		srcm = srcp = src - src_bpl;

		if (0 == y)
			srcm += src_bpl * 2;

		if (y != height - 1)
			srcp += src_bpl * 2;

		for (x = 0; x < width; ++x) {
			int xm, xp;

			xm = (((0 == x) - 1) | 1) + x;
			xp = (((x != width - 1) - 1) | 1) + x;

			switch (tile) {
			case 0: /* BG
				   GR */
				write_rgb_pixel (dst, dst_pf,
						 /* b */ src[x],
						 /* g */ (src[xm] +
							  src[xp] +
							  srcm[x] +
							  srcp[x] + 2) >> 2,
						 /* r */ (srcm[xm] +
							  srcm[xp] +
							  srcp[xm] +
							  srcp[xp] + 2) >> 2,
						 /* depth */ 8);
				break;

			case 1: /* GB
				   RG */
				write_rgb_pixel (dst, dst_pf,
						 /* b */ (src[xm] +
							  src[xp] + 1) >> 1,
						 /* g */ src[x],
						 /* r */ (srcm[x] +
							  srcp[x] + 1) >> 1,
						 /* depth */ 8);
				break;

			case 2: /* GR
				   BG */
				write_rgb_pixel (dst, dst_pf,
						 /* b */ (srcm[x] +
							  srcp[x] + 1) >> 1,
						 /* g */ src[x],
						 /* r */ (src[xm] +
							  src[xp] + 1) >> 1,
						 /* depth */ 8);
				break;

			case 3: /* RG
				   GB */
				write_rgb_pixel (dst, dst_pf,
						 /* b */ (srcm[xm] +
							  srcm[xp] +
							  srcp[xm] +
							  srcp[xp] + 2) >> 2,
						 /* g */ (src[xm] +
							  src[xp] +
							  srcm[x] +
							  srcp[x] + 2) >> 2,
						 /* r */ src[x],
						 /* depth */ 8);
				break;

			default:
				assert (0);
				break;
			}

			tile ^= 1;

			dst += dst_pf->bits_per_pixel >> 3;
		}

		tile ^= 2;

		dst += dst_padding;
		src += src_bpl;
	}
}

static void
convert_bayer16_image		(uint8_t *		dst,
				 const pixel_format *	dst_pf,
				 unsigned long		dst_bpl,
				 const uint16_t *	src,
				 const pixel_format *	src_pf,
				 unsigned long		src_bpl,
				 unsigned int		width,
				 unsigned int		height)
{
	unsigned long dst_padding;
	unsigned int tile;
	unsigned int y;

	assert (PACKED_RGB == dst_pf->pixfmt_class);
	assert (BAYER == src_pf->pixfmt_class);

	assert (width >= 2 && 0 == (width & 1));
	assert (height >= 2 && 0 == (height & 1));

	dst_padding = dst_bpl - width * (dst_pf->bits_per_pixel >> 3);
	assert ((long) dst_padding >= 0);

	switch (src_pf->pixfmt) {
	case BGGR16:
		tile = 0;
		break;

	case GBRG16:
		tile = 1;
		break;

	case RGGB16:
		tile = 2;
		break;

	case GRBG16:
		tile = 3;
		break;

	default:
		assert (0);
		break;
	}

	for (y = 0; y < height; ++y) {
		const uint16_t *srcm;
		const uint16_t *srcp;
		unsigned int x;

		srcm = srcp = (const uint16_t *)
			((char *) src - src_bpl);

		if (0 == y)
			srcm = (const uint16_t *)
				((char *) srcm + src_bpl * 2);

		if (y != height - 1)
			srcp = (const uint16_t *)
				((char *) srcp + src_bpl * 2);

		for (x = 0; x < width; ++x) {
			int xm, xp;

			xm = (((0 == x) - 1) | 1) + x;
			xp = (((x != width - 1) - 1) | 1) + x;

			switch (tile) {
			case 0: /* BG
				   GR */
				write_rgb_pixel (dst, dst_pf,
						 /* b */ src[x],
						 /* g */ (src[xm] +
							  src[xp] +
							  srcm[x] +
							  srcp[x] + 2) >> 2,
						 /* r */ (srcm[xm] +
							  srcm[xp] +
							  srcp[xm] +
							  srcp[xp] + 2) >> 2,
						 /* depth */ 10);
				break;

			case 1: /* GB
				   RG */
				write_rgb_pixel (dst, dst_pf,
						 /* b */ (src[xm] +
							  src[xp] + 1) >> 1,
						 /* g */ src[x],
						 /* r */ (srcm[x] +
							  srcp[x] + 1) >> 1,
						 /* depth */ 10);
				break;

			case 2: /* GR
				   BG */
				write_rgb_pixel (dst, dst_pf,
						 /* b */ (srcm[x] +
							  srcp[x] + 1) >> 1,
						 /* g */ src[x],
						 /* r */ (src[xm] +
							  src[xp] + 1) >> 1,
						 /* depth */ 10);
				break;

			case 3: /* RG
				   GB */
				write_rgb_pixel (dst, dst_pf,
						 /* b */ (srcm[xm] +
							  srcm[xp] +
							  srcp[xm] +
							  srcp[xp] + 2) >> 2,
						 /* g */ (src[xm] +
							  src[xp] +
							  srcm[x] +
							  srcp[x] + 2) >> 2,
						 /* r */ src[x],
						 /* depth */ 10);
				break;

			default:
				assert (0);
				break;
			}

			tile ^= 1;

			dst += dst_pf->bits_per_pixel >> 3;
		}

		tile ^= 2;

		dst += dst_padding;
		src = (const uint16_t *)((char *) src + src_bpl);
	}
}

static void
convert_packed_rgb_pixel	(uint8_t *		dst,
				 const pixel_format *	dst_pf,
				 const uint8_t *	src,
				 const pixel_format *	src_pf)
{
	uint32_t dst_pixel;
	uint32_t src_pixel;
	unsigned int i;

	src_pixel = 0;

	switch (src_pf->byte_order * 256 + src_pf->bits_per_pixel) {
	case LE * 256 + 32:
		src_pixel = src[3] << 24;
		/* fall through */
	case LE * 256 + 24:
		src_pixel |= src[2] << 16;
	case LE * 256 + 16:
		src_pixel |= src[1] << 8;
	case LE * 256 + 8:
		src_pixel |= src[0];
		break;

	case BE * 256 + 32:
		src_pixel = *src++ << 24;
	case BE * 256 + 24:
		src_pixel |= *src++ << 16;
	case BE * 256 + 16:
		src_pixel |= *src++ << 8;
	case BE * 256 + 8:
		src_pixel |= *src;
		break;

	default:
		assert (0);
		break;
	}

	dst_pixel = 0;

	for (i = 0; i < 3; ++i) {
		unsigned int c;

		c = (src_pixel & src_pf->mask[i]) << src_pf->shr[i];

		/* XXX Check if CPU supports only signed right shift. */
		c |= c >> src_pf->n_bits[i];
		c |= c >> (src_pf->n_bits[i] * 2);

		dst_pixel |= (c >> dst_pf->shr[i]) & dst_pf->mask[i];
	}

	switch (dst_pf->byte_order * 256 + dst_pf->bits_per_pixel) {
	case LE * 256 + 32:
		dst[3] = dst_pixel >> 24;
		/* fall through */
	case LE * 256 + 24:
		dst[2] = dst_pixel >> 16;
	case LE * 256 + 16:
		dst[1] = dst_pixel >> 8;
	case LE * 256 + 8:
		dst[0] = dst_pixel;
		break;

	case BE * 256 + 32:
		*dst++ = dst_pixel >> 24;
	case BE * 256 + 24:
		*dst++ = dst_pixel >> 16;
	case BE * 256 + 16:
		*dst++ = dst_pixel >> 8;
	case BE * 256 + 8:
		*dst = dst_pixel;
		break;

	default:
		assert (0);
		break;
	}
}

static void
convert_rgb_image		(uint8_t *		dst,
				 const pixel_format *	dst_pf,
				 unsigned long		dst_bpl,
				 const uint8_t *	src,
				 const pixel_format *	src_pf,
				 unsigned long		src_bpl,
				 unsigned int		width,
				 unsigned int		height)
{
	unsigned long dst_padding;
	unsigned long src_padding;

	assert (PACKED_RGB == dst_pf->pixfmt_class);

	if (BAYER == src_pf->pixfmt_class) {
		if (8 == src_pf->bits_per_pixel) {
			convert_bayer8_image (dst, dst_pf, dst_bpl,
					      src, src_pf, src_bpl,
					      width, height);
		} else {
			convert_bayer16_image (dst, dst_pf, dst_bpl,
					       (const uint16_t *) src,
					       src_pf, src_bpl,
					       width, height);
		}
		return;
	}

	assert (width > 0);
	assert (height > 0);

	dst_padding = dst_bpl - width * (dst_pf->bits_per_pixel >> 3);
	src_padding = src_bpl - width * (src_pf->bits_per_pixel >> 3);

	assert ((long)(dst_padding | src_padding) >= 0);

	do {
		unsigned int count = width;

		do {
			convert_packed_rgb_pixel (dst, dst_pf, src, src_pf);

			dst += dst_pf->bits_per_pixel >> 3;
			src += src_pf->bits_per_pixel >> 3;
		} while (--count > 0);

		dst += dst_padding;
		src += src_padding;
	} while (--height > 0);
}

typedef enum {
	NEXT_FORMAT = 1,
	NEXT_CONVERTER
} my_event;

static my_event
x_event				(void)
{
	while (XPending (display)) {
		XEvent event;
		int key;

		XNextEvent (display, &event);

		switch (event.type) {
		case KeyPress:
			key = XLookupKeysym (&event.xkey, 0);

			switch (key) {
			case 'n':
				return NEXT_FORMAT;

			case 'c':
				if (event.xkey.state & ControlMask)
					exit (EXIT_SUCCESS);
				return NEXT_CONVERTER;

			case 'q':
				exit (EXIT_SUCCESS);

			default:
				break;
			}

			break;

		case ClientMessage:
			/* We requested only delete_window messages. */
			exit (EXIT_SUCCESS);

		default:
			break;
		}
	}

	return 0;
}

static XImage *
create_ximage			(const pixel_format **	pf,
				 unsigned int		width,
				 unsigned int		height)
{
	XImage *xi;
	unsigned int image_size;
	unsigned int i;

	assert (NULL != display);

	xi = XCreateImage (display,
			       DefaultVisual (display, screen),
			       DefaultDepth (display, screen),
			       ZPixmap,
			       /* offset */ 0,
			       /* data */ NULL,
			       width,
			       height,
			       /* bitmap_pad (n/a) */ 8,
			       /* bytes_per_line: auto */ 0);
	if (NULL == xi) {
		error_exit ("Cannot allocate XImage.\n");
	}

	for (i = 0; i < N_ELEMENTS (pixel_formats); ++i) {
		if (PACKED_RGB != pixel_formats[i].pixfmt_class)
			continue;
		if ((LSBFirst == xi->byte_order)
		    != (LE == pixel_formats[i].byte_order))
			continue;
		if (xi->bits_per_pixel
		    != pixel_formats[i].bits_per_pixel)
			continue;
		if (xi->blue_mask != pixel_formats[i].mask[0])
			continue;
		if (xi->green_mask != pixel_formats[i].mask[1])
			continue;
		if (xi->red_mask != pixel_formats[i].mask[2])
			continue;
		break;
	}

	if (i >= N_ELEMENTS (pixel_formats)) {
		error_exit ("Unknown XImage pixel format "
			    "(bpp=%u %s b=0x%08x g=0x%08x r=0x%08x).\n",
			    xi->bits_per_pixel,
			    (LSBFirst == xi->byte_order) ?
			    "LSBFirst" : "MSBFirst",
			    xi->blue_mask,
			    xi->green_mask,
			    xi->red_mask);
	}

	if (NULL != pf)
		*pf = pixel_formats + i;

	image_size = (xi->bytes_per_line * xi->height);

	xi->data = malloc (image_size);
	if (NULL == xi->data) {
		error_exit ("Cannot allocate XImage data (%u bytes).\n",
			    image_size);
		exit (EXIT_FAILURE);
	}

	return xi;
}

static void
resize_window			(unsigned int		image_width,
				 unsigned int		image_height,
				 unsigned int		text_width,
				 unsigned int		text_height)
{
	assert (0 != window);

	XResizeWindow (display, window,
		       MAX (image_width, text_width),
		       image_height + text_height);

	if (NULL != ximage) {
		free (ximage->data);
		ximage->data = NULL;

		XDestroyImage (ximage);
	}

	ximage = create_ximage (&ximage_pf, image_width, image_height);
}

static const char *
pixel_format_bit_string		(const pixel_format *	pf)
{
	static char buf[64];
	char *d;
	unsigned int i;

	if (PACKED_RGB != pf->pixfmt_class)
		return NULL;

	d = buf;

	for (i = 0; i < pf->bits_per_pixel; i += 8) {
		unsigned int ii;
		int j;

		if (0 != i)
			*d++ = ' ';

		ii = i;
		if (BE == pf->byte_order)
			ii = pf->bits_per_pixel - i - 8;

		for (j = 7; j >= 0; --j) {
			unsigned int k;

			for (k = 0; k < 4; ++k) {
				if (pf->mask[k] & (1 << (ii + j))) {
					*d++ = "bgra"[k];
					break;
				}
			}
		}
	}

	*d = 0;

	return buf;
}

static void
display_image			(const uint8_t *	image,
				 uint32_t		v4l2_fourcc,
				 const pixel_format *	image_pf,
				 unsigned long		image_bpl,
				 unsigned int		image_width,
				 unsigned int		image_height)
{
	XWindowAttributes wa;
	XFontStruct *font;
	unsigned int text_height;
	XTextItem xti;
	const char *v4l2_fourcc_name;
	unsigned int i;

	assert (NULL != ximage);

	if (!XGetWindowAttributes (display, window, &wa)) {
		error_exit ("Cannot determine current X11 window size.\n");
	}

	font = XQueryFont (display, XGContextFromGC (gc));
	text_height = font->max_bounds.ascent + font->max_bounds.descent;

	if (image_width > (unsigned int) ximage->width
	    || image_width != (unsigned int) wa.width
	    || image_height > (unsigned int) ximage->height
	    || image_height + text_height != (unsigned int) wa.height) {
		resize_window (image_width,
			       image_height,
			       /* text_width */ image_width,
			       text_height);
	}

	convert_rgb_image ((uint8_t *) ximage->data,
			   ximage_pf,
			   ximage->bytes_per_line,
			   image,
			   image_pf,
			   image_bpl,
			   image_width,
			   image_height);

	XPutImage (display,
		   window,
		   gc,
		   ximage,
		   /* src_x */ 0,
		   /* src_y */ 0,
		   /* dst_x */ 0,
		   /* dst_y */ 0,
		   /* width */ image_width,
		   /* height */ image_height);


	XSetForeground (display, gc, XBlackPixel (display, screen));

	XFillRectangle (display,
			window,
			gc,
			/* x */ 0,
			/* y */ image_height,
			wa.width,
			text_height);

	XSetForeground (display, gc, XWhitePixel (display, screen));

	v4l2_fourcc_name = "?";

	for (i = 0; i < N_ELEMENTS (pixel_formats); ++i) {
		if (v4l2_fourcc == pixel_formats[i].v4l2_fourcc) {
			v4l2_fourcc_name = pixel_formats[i].v4l2_fourcc_name;
			break;
		}
	}


	CLEAR (xti);

	if (PACKED_RGB == image_pf->pixfmt_class) {
		xti.nchars = asprintf (&xti.chars,
				       "Format %s, converter %s (%s)",
				       v4l2_fourcc_name,
				       image_pf->name,
				       pixel_format_bit_string (image_pf));
	} else {
		xti.nchars = asprintf (&xti.chars,
				       "Format %s, converter %s",
				       v4l2_fourcc_name,
				       image_pf->name);
	}

	if (xti.nchars < 0) {
		error_exit ("Cannot allocate text buffer.\n");
	}

	XDrawText (display, window, gc,
		   /* x */ 4,
		   /* y */ image_height + font->max_bounds.ascent,
		   &xti,
		   /* n_items */ 1);

	free (xti.chars);

	XFreeFontInfo (/* names */ NULL, font, 1);
}

static void
open_window			(unsigned int		width,
				 unsigned int		height)
{
	GC default_gc;
	XFontStruct *font;
	unsigned int text_height;

	display = XOpenDisplay (NULL);
	if (NULL == display) {
		error_exit ("Cannot open X11 display.\n");
	}

	screen = DefaultScreen (display);

	default_gc = XDefaultGC (display, screen);
	font = XQueryFont (display, XGContextFromGC (default_gc));
	text_height = font->max_bounds.ascent + font->max_bounds.descent;

	window = XCreateSimpleWindow (display,
				      RootWindow (display, screen),
				      /* x */ 0,
				      /* y */ 0,
				      width,
				      height + text_height,
				      /* border width */ 2,
				      /* foreground */
				      XWhitePixel (display, screen),
				      /* background */
				      XBlackPixel (display, screen));
	if (0 == window) {
		error_exit ("Cannot open X11 window.\n");
	}

	gc = XCreateGC (display, window,
			/* valuemask */ 0,
			/* values */ NULL);

	XSetFunction (display, gc, GXcopy);
	XSetFillStyle (display, gc, FillSolid);

	ximage = create_ximage (&ximage_pf, width, height);

	XSelectInput (display, window,
		      (KeyPressMask |
		       ExposureMask |
		       StructureNotifyMask));

	xa_delete_window = XInternAtom (display, "WM_DELETE_WINDOW",
					/* only_if_exists */ False);

	XSetWMProtocols (display, window, &xa_delete_window, /* count */ 1);

	XStoreName (display, window,
		    "V4L2 Pixfmt Test - "
		    "Press [n] for next format, [c] for next converter");

	XMapWindow (display, window);

	XSync (display, /* discard all events */ False);
}

static int
xioctl                          (int                    fd,
				 unsigned long int      request,
				 void *                 arg)
{
	int r;

	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);

	return r;
}

static bool
read_and_display_frame		(const pixel_format *	conv_pf)
{
	struct v4l2_buffer buf;

	switch (io_method) {
	case IO_METHOD_READ:
		if (-1 == read (dev_fd, buffers[0].start,
				buffers[0].length)) {
			switch (errno) {
			case EAGAIN:
				return false;

			default:
				errno_exit ("read");
			}
		}

		display_image (buffers[0].start,
			       fmt.fmt.pix.pixelformat,
			       conv_pf,
			       fmt.fmt.pix.bytesperline,
			       fmt.fmt.pix.width,
			       fmt.fmt.pix.height);

		break;

	case IO_METHOD_MMAP:
		CLEAR (buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;

		if (-1 == xioctl (dev_fd, VIDIOC_DQBUF, &buf)) {
			switch (errno) {
			case EAGAIN:
				return false;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit ("VIDIOC_DQBUF");
			}
		}

		assert (buf.index < n_buffers);

		display_image (buffers[buf.index].start,
			       fmt.fmt.pix.pixelformat,
			       conv_pf,
			       fmt.fmt.pix.bytesperline,
			       fmt.fmt.pix.width,
			       fmt.fmt.pix.height);

		if (-1 == xioctl (dev_fd, VIDIOC_QBUF, &buf))
			errno_exit ("VIDIOC_QBUF");

		break;
	}

	return true;
}

static void
wait_for_next_frame		(void)
{
	for (;;) {
		struct timeval timeout;
		fd_set fds;
		int r;

		FD_ZERO (&fds);
		FD_SET (dev_fd, &fds);

		timeout.tv_sec = 2;
		timeout.tv_usec = 0;

		r = select (dev_fd + 1, &fds, NULL, NULL, &timeout);
		if (-1 == r) {
			if (EINTR == errno)
				continue;

			errno_exit ("select");
		} else if (0 == r) {
			error_exit ("select timeout.\n");
		} else {
			break;
		}
	}
}

static void
flush_capture_queue		(void)
{
	struct v4l2_buffer buf;

	for (;;) {
		switch (io_method) {
		case IO_METHOD_READ:
			/* Nothing to do. */
			return;

		case IO_METHOD_MMAP:
			CLEAR (buf);

			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;

			if (-1 == xioctl (dev_fd, VIDIOC_DQBUF, &buf)) {
				switch (errno) {
				case EAGAIN:
					return;

				default:
					errno_exit ("VIDIOC_DQBUF");
				}
			}

			if (-1 == xioctl (dev_fd, VIDIOC_QBUF, &buf))
				errno_exit ("VIDIOC_QBUF");

			break;

		default:
			assert (0);
			break;
		}
	}
}

static void
capture_loop			(void)
{
	const pixel_format *conv_pf;

	conv_pf = find_v4l2_fourcc (fmt.fmt.pix.pixelformat);
	assert (NULL != conv_pf);

	for (;;) {
		/* Remove images from the capture queue if
		   we can't display them fast enough. */
		flush_capture_queue ();

		do {
			wait_for_next_frame ();
		} while (!read_and_display_frame (conv_pf));

		switch (x_event ()) {
		case NEXT_CONVERTER:
			conv_pf = next_converter (conv_pf);
			break;

		case NEXT_FORMAT:
			return;

		default:
			break;
		}
	}
}

static void
stop_capturing			(void)
{
	enum v4l2_buf_type type;

	switch (io_method) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl (dev_fd, VIDIOC_STREAMOFF, &type))
			errno_exit ("VIDIOC_STREAMOFF");

		break;
	}
}

static void
start_capturing			(void)
{
	unsigned int i;
	enum v4l2_buf_type type;

	switch (io_method) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < n_buffers; ++i) {
			struct v4l2_buffer buf;

			CLEAR (buf);

			buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory	= V4L2_MEMORY_MMAP;
			buf.index	= i;

			if (-1 == xioctl (dev_fd, VIDIOC_QBUF, &buf))
				errno_exit ("VIDIOC_QBUF");
		}

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl (dev_fd, VIDIOC_STREAMON, &type))
			errno_exit ("VIDIOC_STREAMON");

		break;

	default:
		assert (0);
		break;
	}
}

static void
free_io_buffers			(void)
{
	unsigned int i;

	switch (io_method) {
	case IO_METHOD_READ:
		free (buffers[0].start);
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < n_buffers; ++i) {
			if (-1 == munmap (buffers[i].start,
					  buffers[i].length)) {
				errno_exit ("munmap");
			}
		}

		break;

	default:
		assert (0);
		break;
	}

	free (buffers);
	buffers = NULL;
}

static void
init_read_io			(unsigned int		buffer_size)
{
	buffers = calloc (1, sizeof (*buffers));

	if (NULL == buffers) {
		error_exit ("Cannot allocate capture buffer (%u bytes).\n",
			    sizeof (*buffers));
	}

	buffers[0].length = buffer_size;
	buffers[0].start = malloc (buffer_size);

	if (NULL == buffers[0].start) {
		error_exit ("Cannot allocate capture buffer (%u bytes).\n",
			    buffer_size);
	}
}

static void
init_mmap_io			(void)
{
	struct v4l2_requestbuffers req;

	CLEAR (req);

	req.count	= 4;
	req.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory	= V4L2_MEMORY_MMAP;

	if (-1 == xioctl (dev_fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			error_exit ("%s does not support "
				    "memory mapping.\n", dev_name);
		} else {
			errno_exit ("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) {
		error_exit ("Insufficient buffer memory on %s.\n",
			    dev_name);
	}

	buffers = calloc (req.count, sizeof (*buffers));
	if (NULL == buffers) {
		error_exit ("Cannot allocate capture buffer (%u bytes).\n",
			    req.count * sizeof (*buffers));
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;

		CLEAR (buf);

		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= n_buffers;

		if (-1 == xioctl (dev_fd, VIDIOC_QUERYBUF, &buf))
			errno_exit ("VIDIOC_QUERYBUF");

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start =
			mmap (NULL /* start anywhere */,
			      buf.length,
			      PROT_READ | PROT_WRITE /* required */,
			      MAP_SHARED /* recommended */,
			      dev_fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start)
			errno_exit ("mmap");
	}
}

static void
mainloop			(void)
{
	bool checked_formats[N_ELEMENTS (pixel_formats)];

	CLEAR (checked_formats);

	for (;;) {
		const pixel_format *pf;
		const pixel_format *actual_pf;
		unsigned int width;
		unsigned int height;
		unsigned int min_bpl;
		unsigned int min_size;
		unsigned int i;

		for (i = 0; i < N_ELEMENTS (pixel_formats); ++i) {
			if (checked_formats[i])
				continue;
			checked_formats[i] = true;
			if (0 != pixel_formats[i].v4l2_fourcc)
				break;
		}

		if (i >= N_ELEMENTS (pixel_formats))
			return; /* all done */

		pf = pixel_formats + i;

		CLEAR (fmt);

		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		height = 480;
		if (std_id & V4L2_STD_625_50)
			height = 576;

		width = height * 4 / 3;

		fmt.fmt.pix.width = width;
		fmt.fmt.pix.height = height;

		fmt.fmt.pix.pixelformat = pf->v4l2_fourcc;

		fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

		if (-1 == xioctl (dev_fd, VIDIOC_S_FMT, &fmt)) {
			if (EINVAL != errno) {
				errno_exit ("VIDIOC_S_FMT");
			}

			fprintf (stderr, "Format %s %ux%u "
				 "not supported by driver.\n",
				 pf->v4l2_fourcc_name,
				 width,
				 height);

			continue;
		}

		actual_pf = find_v4l2_fourcc (fmt.fmt.pix.pixelformat);
		if (0 == actual_pf) {
			fprintf (stderr,
				 "Requested pixelformat %s, driver "
				 "returned unknown pixelformat 0x%08x.\n",
				 pf->v4l2_fourcc_name,
				 fmt.fmt.pix.pixelformat);
			continue;
		} else if (pf != actual_pf) {
			/* Some drivers change pixelformat. */
			checked_formats[actual_pf->pixfmt] = true;
			pf = actual_pf;
		}

		min_bpl = (fmt.fmt.pix.width * pf->bits_per_pixel) >> 3;

		if (fmt.fmt.pix.bytesperline < min_bpl) {
			error_exit ("Driver returned fmt.pix.pixelformat=%s "
				    "width=%u height=%u bytesperline=%u. "
				    "Expected bytesperline >= %u.\n",
				    pf->v4l2_fourcc_name,
				    fmt.fmt.pix.width,
				    fmt.fmt.pix.height,
				    fmt.fmt.pix.bytesperline,
				    min_bpl);
			continue;
		}

		min_size = (fmt.fmt.pix.height - 1)
			* MAX (min_bpl, fmt.fmt.pix.bytesperline)
			+ min_bpl;

		if (fmt.fmt.pix.sizeimage < min_size) {
			error_exit ("Driver returned fmt.pix.pixelformat=%s "
				    "width=%u height=%u bytesperline=%u "
				    "size=%u. Expected size >= %u.\n",
				    pf->v4l2_fourcc_name,
				    fmt.fmt.pix.width,
				    fmt.fmt.pix.height,
				    fmt.fmt.pix.bytesperline,
				    fmt.fmt.pix.sizeimage,
				    min_size);
			continue;
		}

		if (0 == window) {
			open_window (fmt.fmt.pix.width,
				     fmt.fmt.pix.height);
		}

		switch (io_method) {
		case IO_METHOD_READ:
			init_read_io (fmt.fmt.pix.sizeimage);
			break;

		case IO_METHOD_MMAP:
			init_mmap_io ();
			break;
		}

		start_capturing ();

		capture_loop ();

		stop_capturing ();

		free_io_buffers ();
	}
}

static void
init_device			(void)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;

	if (-1 == xioctl (dev_fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			error_exit ("%s is not a V4L2 device.\n");
		} else {
			errno_exit ("VIDIOC_QUERYCAP");
		}
	}

	if (io_method == 0) {
		if (cap.capabilities & V4L2_CAP_STREAMING) {
			io_method = IO_METHOD_MMAP;
		} else if (cap.capabilities & V4L2_CAP_READWRITE) {
			io_method = IO_METHOD_READ;
		} else {
			error_exit ("%s does not support reading or "
					"streaming.\n");
		}
	}

	switch (io_method) {
	case IO_METHOD_READ:
		if (0 == (cap.capabilities & V4L2_CAP_READWRITE)) {
			error_exit ("%s does not support read i/o.\n");
		}

		break;

	case IO_METHOD_MMAP:
		if (0 == (cap.capabilities & V4L2_CAP_STREAMING)) {
			error_exit ("%s does not support streaming i/o.\n");
		}

		break;

	default:
		assert (0);
		break;
	}

	CLEAR (cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl (dev_fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = cropcap.type;
		crop.c = cropcap.defrect; /* reset to default */

		/* Errors ignored. */
		xioctl (dev_fd, VIDIOC_S_CROP, &crop);
	} else {
		/* Errors ignored. */
	}

	/* Webcams may not support any standard at all, see
	   http://v4l2spec.bytesex.org/spec/x448.htm for details */
	if (-1 == xioctl (dev_fd, VIDIOC_G_STD, &std_id))
		std_id = 0;
}

static void
open_device			(void)
{
	struct stat st;

	if (-1 == stat (dev_name, &st)) {
		error_exit ("Cannot identify '%s'. %s.\n",
			    dev_name, strerror (errno));
	}

	if (!S_ISCHR (st.st_mode)) {
		error_exit ("%s is not a device file.\n", dev_name);
	}

	dev_fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);
	if (-1 == dev_fd) {
		error_exit ("Cannot open %s. %s.\n",
			    dev_name, strerror (errno));
	}
}

static void
self_test			(void)
{
	const pixel_format *pf;

	assert (0 == pixel_formats[0].pixfmt);
	assert (N_ELEMENTS (pixel_formats) > 0);

	for (pf = pixel_formats + 1;
	     pf < pixel_formats + N_ELEMENTS (pixel_formats); ++pf) {
		const pixel_format *pf2;
		unsigned int i;

#define pf_assert(expr)							\
do {									\
	if (!(expr)) {							\
		error_exit ("Assertion %s failed in "			\
			    "pixel_format[%d = %s].\n",			\
			    #expr, (int)(pf - pixel_formats),		\
			    pf->name ? pf->name : "?");			\
	}								\
} while (0)

		pf_assert (0 != pf->pixfmt);
		pf_assert (NULL != pf->name);

		pf_assert ((0 == pf->v4l2_fourcc)
			   == (NULL == pf->v4l2_fourcc_name));

		pf_assert (0 != pf->pixfmt_swap_red_blue);

		pf_assert (LE == pf->byte_order
			   || BE == pf->byte_order);

		pf_assert (PACKED_RGB == pf->pixfmt_class
			   || BAYER == pf->pixfmt_class);

		if (PACKED_RGB == pf->pixfmt_class) {
			pf_assert (pf->color_depth == (pf->n_bits[0] +
						       pf->n_bits[1] +
						       pf->n_bits[2]));

			pf_assert (pf->bits_per_pixel == (pf->n_bits[0] +
							  pf->n_bits[1] +
							  pf->n_bits[2] +
							  pf->n_bits[3]));

			pf_assert (0 != pf->pixfmt_opposite_byte_order);

			if (0 != pf->mask[3]) /* has alpha */
				pf_assert (0 != pf->pixfmt_opposite_alpha);
			else
				pf_assert (0 == pf->pixfmt_opposite_alpha);

			for (i = 0; i < N_ELEMENTS (pf->mask); ++i) {
				pf_assert (pf->n_bits[i] + pf->shr[i] <= 32);
				pf_assert (pf->mask[i]
					   == (((1u << pf->n_bits[i]) - 1)
					       << (32 - pf->n_bits[i]
						   - pf->shr[i])));
			}
		}

		for (pf2 = pf + 1;
		     pf2 < pixel_formats + N_ELEMENTS (pixel_formats);
		     ++pf2) {
			if (pf->pixfmt == pf2->pixfmt
			    || 0 == strcmp (pf->name, pf2->name)) {
				error_exit ("Assertion failure: pixfmt "
					    "%u (%s) twice in "
					    "pixel_formats[] table.\n",
					    pf->pixfmt, pf->name);
			}

			if (0 != pf->v4l2_fourcc
			    && 0 != pf2->v4l2_fourcc
			    && (pf->v4l2_fourcc == pf2->v4l2_fourcc
				|| 0 == strcmp (pf->v4l2_fourcc_name,
						pf2->v4l2_fourcc_name))) {
				error_exit ("Assertion failure: V4L2 "
					    "fourcc 0x%08x (%s) twice in "
					    "pixel_formats[] table.\n",
					    pf->v4l2_fourcc,
					    pf->v4l2_fourcc_name);
			}
		}

#undef pf_assert

	}

	/* XXX Should also test the converters here. */
}

static void
usage				(FILE *			fp,
				 int			argc,
				 char **		argv)
{
	fprintf (fp, "\
V4L2 pixfmt test " V4L_UTILS_VERSION "\n\
Copyright (C) 2007 Michael H. Schimek\n\
This program is licensed under GPL 2 or later. NO WARRANTIES.\n\n\
Usage: %s [options]\n\n\
Options:\n\
-d | --device name  Video device name [%s]\n\
-h | --help         Print this message\n\
-m | --mmap         Use memory mapped buffers (auto)\n\
-r | --read         Use read() calls (auto)\n\
",
		 my_name, dev_name);
}

static const char short_options [] = "d:hmr";

static const struct option
long_options [] = {
	{ "device",	required_argument,	NULL,		'd' },
	{ "help",	no_argument,		NULL,		'h' },
	{ "mmap",	no_argument,		NULL,		'm' },
	{ "read",	no_argument,		NULL,		'r' },
	{ "usage",	no_argument,		NULL,		'h' },
	{ 0, 0, 0, 0 }
};

int
main				(int			argc,
				 char **		argv)
{
	my_name = argv[0];

	self_test ();

	for (;;) {
		int opt_index;
		int c;

		c = getopt_long (argc, argv,
				 short_options, long_options,
				 &opt_index);

		if (-1 == c)
			break;

		switch (c) {
		case 0: /* getopt_long() flag */
			break;

		case 'd':
			dev_name = optarg;
			break;

		case 'h':
			usage (stdout, argc, argv);
			exit (EXIT_SUCCESS);

		case 'm':
			io_method = IO_METHOD_MMAP;
			break;

		case 'r':
			io_method = IO_METHOD_READ;
			break;

		default:
			usage (stderr, argc, argv);
			exit (EXIT_FAILURE);
		}
	}

	open_device ();

	init_device ();

	mainloop ();

	exit (EXIT_SUCCESS);

	return 0;
}
