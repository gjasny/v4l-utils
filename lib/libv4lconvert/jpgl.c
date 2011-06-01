/*
 * Implementation of JPEG Lite decoding algorithm
 *
 * Author & Copyright (c) 2003 : Sylvain Munaut <nw8xx ]at[ 246tNt.com>
 *
 * v4l library adaptation: Jean-François Moine <moinejf@free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA

 * Note this code was originally licensed under the GNU GPL instead of the
 * GNU LGPL, its license has been changed with permission, see the permission
 * mail at the end of this file.
 */

/* Original WebSite: nw802.sourceforge.net */

#include <stdlib.h>
#include "libv4lconvert-priv.h"

#define RING_QUEUE_ADVANCE_INDEX(rq,ind,n) (rq)->ind = ((rq)->ind + (n))
#define RING_QUEUE_DEQUEUE_BYTES(rq,n) RING_QUEUE_ADVANCE_INDEX(rq,ri,n)
#define RING_QUEUE_PEEK(rq,ofs) ((rq)->queue[((ofs) + (rq)->ri)])

struct RingQueue {
	const unsigned char *queue;
	int length;
	int ri;
};

/* ============================================================================
 * RingQueue bit reader
 * ============================================================================
 * All what is needed to read bit by nit from the RingQueue pump 
 * provided by usbvideo
 * Critical part are macro and not functions to speed things up
 * Rem: Data are read from the RingQueue as if they were 16bits Little Endian
 *      words. Most Significants Bits are outputed first.
 */

/* Structure used to store what we need. */
/* (We may need multiple simultaneous instance from several cam) */
struct rqBitReader {
	int cur_bit;
	unsigned int cur_data;
	struct RingQueue *rq;
};

static inline void rqBR_init( struct rqBitReader *br, struct RingQueue *rq )
{
	br->cur_bit = 16;
	br->cur_data =
		RING_QUEUE_PEEK( rq, 2 )        |
		RING_QUEUE_PEEK( rq, 3 ) << 8   |
		RING_QUEUE_PEEK( rq, 0 ) << 16  |
		RING_QUEUE_PEEK( rq, 1 ) << 24  ;
	RING_QUEUE_DEQUEUE_BYTES( rq, 2 );
	br->rq = rq;
}

#define rqBR_peekBits(br,n) ( br->cur_data >> (32-n) )

#define rqBR_flushBits(br,n) do {                                   \
        br->cur_data <<= n;                                         \
        if ( (br->cur_bit -= n) <= 0 ) {                            \
            br->cur_data |=                                         \
                RING_QUEUE_PEEK( br->rq, 2 ) << -br->cur_bit  |     \
                RING_QUEUE_PEEK( br->rq, 3 ) << (8 - br->cur_bit);  \
            RING_QUEUE_DEQUEUE_BYTES( br->rq, 2 );                  \
            br->cur_bit += 16;                                      \
        }                                                           \
	} while (0)

/* ============================================================================
 * Real JPEG Lite stuff
 * ============================================================================
 *
 * Precomputed tables
 * Theses are computed at init time to make real-time operations faster.
 * It takes some space ( about 9k ). But believe me it worth it !
 */

/* Variable Lenght Coding related tables, used for AC coefficient decoding
 * TODO Check that 7 bits is enough ! */
static signed char vlcTbl_len[1<<10];	/* Meaningful bit count */
static signed char vlcTbl_run[1<<10];	/* Run */
static signed char vlcTbl_amp[1<<10];	/* Amplitude (without the sign) */

/* YUV->RGB conversion table */
static int yuvTbl_y[256];
static int yuvTbl_u1[256];
static int yuvTbl_u2[256];
static int yuvTbl_v1[256];
static int yuvTbl_v2[256];

/* Clamping table */
#define SAFE_CLAMP
#ifdef SAFE_CLAMP
static inline unsigned char clamp(int x) {
	if (x > 255)
		return 255;
	if (x < 0)
		return 0;
	return x;
}
#define clamp_adjust(x) clamp(x+128)
#else
#define clamp(x) clampTbl[(x)+512]
#define clamp_adjust(x) clampTbl[(x)+640]
static char clampTbl[1280];
#endif

/* Code to initialize those tables */
static void vlcTbl_init(void)
{
	/* Bases tables used to compute the bigger one
	 * To understands theses, look at the VLC doc in the
	 * US patent document. */

	static const int vlc_num = 28;
	static const int vlc_len[] =
		{ 2, 2, 3, 3, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 7,
		  8 ,8 ,8 ,9, 9, 9, 10, 10, 10, 10, 10, 10 };
	static const int vlc_run[] =
		{ 0, 0, 0, 1, 0, 2, 3, 1, 0, 4, 0, 5, 1, 0, -1, -2,
		  2, 6, 0, 3, 1, 0, 1, 0, 7, 2, 0, 8 };
	static const int vlc_amp[] =
		{ 0, 1, 2, 1, 3, 1, 1, 2, 4, 1 ,5 ,1 ,3 ,6, -1, -2,
		  2, 1, 7, 2, 4, 8, 5, 9, 1 ,3, 10, 1 };
	static const int vlc_cod[] =
		{ 0x000, 0x002, 0x003, 0x006, 0x00E, 0x008, 0x00B, 0x012,
		  0x014, 0x03D, 0x03E, 0x078, 0x079, 0x07E, 0x026, 0x027,
		  0x054, 0x057, 0x0FF, 0x0AA, 0x0AC, 0x1FC, 0x156, 0x157,
		  0x15A, 0x15B, 0x3FA, 0x3FB };
	
	/* Vars */
	int i,j;

	/* Main filling loop */
	for ( i=0 ; i<(1<<10) ; i++ ) {

		/* Find the matching one */
		for ( j=0 ; j<vlc_num ; j++ ) {
			if ( (i >> (10-vlc_len[j])) == vlc_cod[j] ) {
				if ( vlc_run[j] >= 0 )
					if ( vlc_amp[j] != 0 )
						vlcTbl_len[i] = vlc_len[j] + 1;
					else
						vlcTbl_len[i] = vlc_len[j]; /* EOB */
				else
					vlcTbl_len[i] = 16;
				vlcTbl_run[i] = vlc_run[j];
				vlcTbl_amp[i] = vlc_amp[j];
				break;
			}
		}
	}
}

static void yuvTbl_init(void)
{
	/* These tables are just pre-multiplied and pre-offseted
	 * YUV by the book
	 * R = 1.164 * (Y-16) + 1.596 * (U-128)
	 * G = 1.164 * (Y-16) - 0.813 * (U-128) - 0.391 * (V-128)
	 * B = 1.164 * (Y-16)                   + 2.018 * (V-128) */

	int i;

	/* We use fixed point << 16 */
	for ( i=0 ; i < 256 ; i++ ) {
		yuvTbl_y[i]  =  76284 * (i- 16);
		yuvTbl_u1[i] = 104595 * (i-128);
		yuvTbl_u2[i] =  53281 * (i-128);
		yuvTbl_v1[i] =  25625 * (i-128); 
		yuvTbl_v2[i] = 132252 * (i-128);
	}
}

#ifndef SAFE_CLAMP
static void clampTbl_init(void)
{
	/* Instead of doing if(...) to test for overrange, we use
	 * a clamping table */
	
	int i;

	for (i=0 ; i < 512 ; i++)
		clampTbl[i] = 0;
	for (i=512 ; i < 768 ; i++ )
		clampTbl[i] = i - 512;
	for (i=768 ; i < 1280 ; i++ )
		clampTbl[i] = 255;

}
#endif

/*
 * Internal helpers
 */

static inline int readAC( struct rqBitReader *br, int *run, int *amp )
{
	/* Vars */
	unsigned int cod;

	/* Get 16 bits */
	cod = rqBR_peekBits(br,16);

	/* Lookup in the table */
	*run = vlcTbl_run[cod>>6];
	*amp = vlcTbl_amp[cod>>6];
	rqBR_flushBits(br,vlcTbl_len[cod>>6]);

	if (*amp > 0) {

		/* Normal stuff, just correct the sign */
		if (cod & (0x10000 >> vlcTbl_len[cod>>6]))
			*amp = - *amp;
	} else {

		/* Handle special cases */
		if (!*amp)
			return 0;
		if (*amp == -1) {

			/* 0100110srrraaaaa */
			*run = ( cod >> 5 ) & 0x07;
			*amp = ( cod & 0x100) ?
				-(cod&0x1F) : (cod&0x1F);
		} else {

			/* 0100111srrrraaaa */
			*run = ( cod >> 4 ) & 0x0F;
			*amp = ( cod & 0x100) ?
				-(cod&0x0F) : (cod&0x0F);
		}
	}

	return 1;
}


#define iDCT_column(b0,b1,b2,b3) do {	\
	int t0,t1,t2,t3;                    \
					\
	t0 = ( b1 + b3 ) << 5;              \
	t2 = t0 - (b3 << 4);                \
	t3 = (b1 *  47) - t0;               \
	t0 = b0 + b2;                       \
	t1 = b0 - b2;                       \
					\
	b0 = ( t0 + t2 );                   \
	b1 = ( t1 + t3 );                   \
	b3 = ( t0 - t2 );                   \
	b2 = ( t1 - t3 );                   \
} while (0)

#define iDCT_line(b0,b1,b2,b3) do {		\
	int t0,t1,t2,t3,bm0,bm2;            \
					\
	bm0 = b0 << 7;                      \
	bm2 = b2 << 7;                      \
					\
	t0 = bm0 + bm2;                     \
	t1 = bm0 - bm2;                     \
	t2 = b1 * 183 + b3 *  86;           \
	t3 = b1 *  86 - b3 * 183;           \
					\
	b0 = ( t0 + t2 ) >> 22;             \
	b1 = ( t1 + t3 ) >> 22;             \
	b3 = ( t0 - t2 ) >> 22;             \
	b2 = ( t1 - t3 ) >> 22;             \
} while (0)

/* Decode a block
 * Basic ops : get the DC - get the ACs - deZigZag - deWeighting - 
 *             deQuantization - iDCT
 * Here they are a little mixed-up to speed all this up.
 */
static inline int decodeBlock( struct rqBitReader *br, int *block, int *dc )
{
	/* Tables used for block decoding */
	
		/* deZigZag table
		 *
		 * ZigZag: each of the coefficient of the DCT transformed 4x4
		 *         matrix is taken in a certain order to make a linear
		 *         array with the high frequency AC at the end
		 *
		 * / 0  1  5  6 \    .
		 * | 2  4  7 12 |    This is the order taken. We must deZigZag
		 * | 3  8 11 13 |    to reconstitute the original matrix
		 * \ 9 10 14 15 /
		 */
	static const int iZigZagTbl[16] =
		{ 0, 1, 4, 8, 5, 2, 3, 6,  9,12, 13, 10, 7, 11, 14, 15 };

		/* deQuantization, deWeighting & iDCT premultiply */
	
		/*
		 * Weighting : Each DCT coefficient is weighted by a certain factor. We
		 *             must compensate for this to rebuilt the original DCT matrix.
		 *
		 * Quantization: According to the read Q factor, DCT coefficient are
		 *               quantized. We need to compensate for this. 
		 *
		 * iDCT premultiply: Since for the first iDCT pass ( column ), we'll need
		 *                   to do some multiplication, the ones that we can
		 *                   integrate here, we do.
		 *
		 * Rem: - The factors are here presented in the ZigZaged order,
		 *      because we will need those BEFORE the deZigZag
		 *      - For more informations, consult jpgl_tbl.c, it's the little
		 *      prog that computes this table
		 */
	static const int iQWTbl[4][16] = {
		{  32768,  17808,    794,  18618,    850,  18618,  43115,   1828,
		   40960,   1924,   2089,  45511,   2089,  49648,   2216,   2521 },
		{  32768,  35617,   1589,  37236,   1700,  37236,  86231,   3656,
		   81920,   3849,   4179,  91022,   4179,  99296,   4432,   5043 },
		{  32768,  71234,   3179,  74472,   3401,  74472, 172463,   7313,
		  163840,   7698,   8358, 182044,   8358, 198593,   8865,  10087 },
		{  32768, 142469,   6359, 148945,   6803, 148945, 344926,  14627,
		  327680,  15397,  16716, 364088,  16716, 397187,  17730,  20175 }
	};	

	/* Vars */
	int hdr;
	int *eff_iQWTbl;
	int cc, run, amp;

	/* Read & Decode the block header ( Q, T, DC ) */
	hdr = rqBR_peekBits(br,11);

	if (hdr & 0x100) {
		/* Differential mode */
		if (hdr & 0x80)
			*dc += ( hdr >> 3 ) | ~0xF;
		else
			*dc += ( hdr >> 3 ) & 0xF;

		/* Flush the header bits */
		rqBR_flushBits(br,8);
	} else {
		/* Direct mode */
		if ( hdr & 0x80 )
			*dc = hdr | ~0x7F;
		else
			*dc = hdr & 0x7F;
			
		/* Flush the header bits */
		rqBR_flushBits(br,11);
	}

	/* Clear the block & store DC ( with pre-multiply ) */
	block[0] = *dc << 15;
	block[1] = 0x00;
	block[2] = 0x00;
	block[3] = 0x00;
	block[4] = 0x00;
	block[5] = 0x00;
	block[6] = 0x00;
	block[7] = 0x00;
	block[8] = 0x00;
	block[9] = 0x00;
	block[10] = 0x00;
	block[11] = 0x00;
	block[12] = 0x00;
	block[13] = 0x00;
	block[14] = 0x00;
	block[15] = 0x00;
	
	/* Read the AC coefficients
	 * at the same time, deZigZag, deQuantization, deWeighting & iDCT premultiply
	 */
	eff_iQWTbl = (int*) iQWTbl[hdr>>9];
	cc = 0;
	
	while ( readAC(br,&run,&amp) ) {
		cc += run + 1;
		if ( cc > 15 )
			return -1;
		block[iZigZagTbl[cc]] = amp * eff_iQWTbl[cc];
	}
	
	/* Do the column iDCT ( what's left to do ) */
	iDCT_column(block[0], block[4], block[8], block[12]);
	iDCT_column(block[1], block[5], block[9], block[13]);
	iDCT_column(block[2], block[6], block[10], block[14]);
	iDCT_column(block[3], block[7], block[11], block[15]);
	
	/* Do the line iDCT ( complete one here ) */
	iDCT_line(block[0], block[1], block[2], block[3]);
	iDCT_line(block[4], block[5], block[6], block[7]);
	iDCT_line(block[8], block[9], block[10], block[11]);
	iDCT_line(block[12], block[13], block[14], block[15]);

	return !(hdr & 0x700);
}

int v4lconvert_decode_jpgl(const unsigned char *inp, int src_size,
		unsigned int dest_pix_fmt, unsigned char *fb,
		int img_width, int img_height)
{
	/* Vars */
	struct RingQueue rq;
	struct rqBitReader br;

	int row, col;	/* Row & Column in the image */

	int x,y;
	int block_idx;

	unsigned char *Yline_baseptr, *Uline_baseptr, *Vline_baseptr;
	unsigned char *Yline, *Uline, *Vline;
	int Yline_baseofs, UVline_baseofs;

	int dc_y, dc_u, dc_v;	/* DC Coefficients */
	int block_y[16*4];		/* Y blocks */
	int block_u[16];		/* U block */
	int block_v[16];		/* V block */

	unsigned char *mainbuffer;

	int yc,uc,vc;

	/* init the decoder */
	if (yuvTbl_y[0] == 0) {
		vlcTbl_init();
		yuvTbl_init();
#ifndef SAFE_CLAMP
		clampTbl_init();
#endif
	}

	img_height /= 4;

	/* Prepare a bit-by-bit reader */
	rq.queue = inp;
	rq.length = src_size;
	rq.ri = 0;
	rqBR_init(&br, &rq);

	/* Allocate a big buffer & setup pointers */
	switch (dest_pix_fmt) {
	default:
/*	case V4L2_PIX_FMT_RGB24: */
/*	case V4L2_PIX_FMT_BGR24: */
		mainbuffer = malloc(4 * (img_width + (img_width >> 1) + 2));
	
		Yline_baseptr = mainbuffer;
		Uline_baseptr = mainbuffer + (4 * img_width);
		Vline_baseptr = Uline_baseptr + (img_width + 4);
		break;
	case V4L2_PIX_FMT_YUV420:
		mainbuffer = NULL;
		Yline_baseptr = fb;
		Uline_baseptr = fb + img_width * img_height * 16;
		Vline_baseptr = Uline_baseptr + img_width * img_height * 4;
		break;
	case V4L2_PIX_FMT_YVU420:
		mainbuffer = NULL;
		Yline_baseptr = fb;
		Vline_baseptr = fb + img_width * img_height * 16;
		Uline_baseptr = Vline_baseptr + img_width * img_height * 4;
		break;
	}

	Yline_baseofs = img_width - 4;
	UVline_baseofs = (img_width >> 2) - 3;

	/* Process 4 lines at a time ( one block height ) */
	for ( row=0 ; row<img_height ; row++ ) {
		/* Line start reset DC */
		dc_y = dc_u = dc_v = 0;

		/* Process 16 columns at a time ( 4 block width ) */
		for ( col=0 ; col<img_width ; col+=16 ) {
			/* Decode blocks
			 * Block order : Y Y Y Y V U ( Why V before U ?
			 * that just depends what you call U&V ... I took the
			 * 'by-the-book' names and that make V and then U,
			 * ... just ask the DivIO folks ;) )
			 */
			if ( decodeBlock(&br, block_y, &dc_y) && (!col) )
/*				return;		 * Bad block, so bad frame ... */
				;

			decodeBlock(&br, block_y + 16, &dc_y);
			decodeBlock(&br, block_y + 32, &dc_y);
			decodeBlock(&br, block_y + 48, &dc_y);
			decodeBlock(&br, block_v, &dc_v);
			decodeBlock(&br, block_u, &dc_u);
			
			/* Copy data to temporary buffers ( to make a complete line ) */
			block_idx = 0;
			Yline = Yline_baseptr + col;
			Uline = Uline_baseptr + (col >> 2);
			Vline = Vline_baseptr + (col >> 2);

			for ( y=0 ; y<4 ; y++) {
				/* Scan line */
				for ( x=0 ; x<4 ; x++ ) {
					/* Y block */
					Yline[ 0] = clamp_adjust(block_y[block_idx   ]);
					Yline[ 4] = clamp_adjust(block_y[block_idx+16]);
					Yline[ 8] = clamp_adjust(block_y[block_idx+32]);
					Yline[12] = clamp_adjust(block_y[block_idx+48]);

					/* U block */
					*Uline = clamp_adjust(block_u[block_idx]);

					/* V block */
					*Vline = clamp_adjust(block_v[block_idx]);

					/* Ajust pointers & index */
					block_idx++;
					Yline++;
					Uline++;
					Vline++;
				}

				/* Adjust pointers */
				Yline += Yline_baseofs;
				Uline += UVline_baseofs;
				Vline += UVline_baseofs;
			}
		}

		/* Handle interpolation special case ( at the end of the lines ) */
		Uline = Uline_baseptr + (UVline_baseofs+2);
		Vline = Vline_baseptr + (UVline_baseofs+2);
		for ( y=0 ; y<4 ; y++ ) {
			/* Copy the last pixel */
			Uline[1] = Uline[0];
			Vline[1] = Vline[0];
	
			/* Adjust ptr */
			Uline += UVline_baseofs+4;	
			Vline += UVline_baseofs+4;	
		}

		/* We have 4 complete lines, so tempbuffer<YUV> -> framebuffer<RGB>
		 * Go line by line */

		switch (dest_pix_fmt) {
		case V4L2_PIX_FMT_RGB24:
		    Yline = Yline_baseptr;
		    Uline = Uline_baseptr;
		    Vline = Vline_baseptr;
		    for ( y=0 ; y<4 ; y++ ) {
			/* Process 4 pixel at a time to handle interpolation
			 * for U & V values */
			for ( x=0 ; x<img_width ; x+=4 ) {
				/* First pixel */
				yc = yuvTbl_y[*(Yline++)];
				uc = Uline[0];
				vc = Vline[0];

				*(fb++) = clamp(( yc + yuvTbl_u1[uc] ) >> 16);
				*(fb++) = clamp(( yc - yuvTbl_u2[uc] - yuvTbl_v1[vc] ) >> 16);
				*(fb++) = clamp(( yc + yuvTbl_v2[vc] ) >> 16);

				/* Second pixel */
				yc = yuvTbl_y[*(Yline++)];
				uc = ( 3*Uline[0] + Uline[1] ) >> 2;
				vc = ( 3*Vline[0] + Vline[1] ) >> 2;
				
				*(fb++) = clamp(( yc + yuvTbl_u1[uc] ) >> 16);
				*(fb++) = clamp(( yc - yuvTbl_u2[uc] - yuvTbl_v1[vc] ) >> 16);
				*(fb++) = clamp(( yc + yuvTbl_v2[vc] ) >> 16);

				/* Third pixel */
				yc = yuvTbl_y[*(Yline++)];
				uc = ( Uline[0] + Uline[1] ) >> 1;
				vc = ( Vline[0] + Vline[1] ) >> 1;

				*(fb++) = clamp(( yc + yuvTbl_u1[uc] ) >> 16);
				*(fb++) = clamp(( yc - yuvTbl_u2[uc] - yuvTbl_v1[vc] ) >> 16);
				*(fb++) = clamp(( yc + yuvTbl_v2[vc] ) >> 16);

				/* Fourth pixel */
				yc = yuvTbl_y[*(Yline++)];
				uc = ( Uline[0] + 3*Uline[1] ) >> 2;
				vc = ( Vline[0] + 3*Vline[1] ) >> 2;

				*(fb++) = clamp(( yc + yuvTbl_u1[uc] ) >> 16);
				*(fb++) = clamp(( yc - yuvTbl_u2[uc] - yuvTbl_v1[vc] ) >> 16);
				*(fb++) = clamp(( yc + yuvTbl_v2[vc] ) >> 16);

				/* Adjust pointers */
				Uline++;
				Vline++;
			}
				
			/* Adjust pointers */
			Uline++;
			Vline++;
		    }
		    break;
		case V4L2_PIX_FMT_BGR24:
		    Yline = Yline_baseptr;
		    Uline = Uline_baseptr;
		    Vline = Vline_baseptr;
		    for ( y=0 ; y<4 ; y++ ) {
			/* Process 4 pixel at a time to handle interpolation
			 * for U & V values */
			for ( x=0 ; x<img_width ; x+=4 ) {
				/* First pixel */
				yc = yuvTbl_y[*(Yline++)];
				uc = Uline[0];
				vc = Vline[0];

				*(fb++) = clamp(( yc + yuvTbl_v2[vc] ) >> 16);
				*(fb++) = clamp(( yc - yuvTbl_u2[uc] - yuvTbl_v1[vc] ) >> 16);
				*(fb++) = clamp(( yc + yuvTbl_u1[uc] ) >> 16);

				/* Second pixel */
				yc = yuvTbl_y[*(Yline++)];
				uc = ( 3*Uline[0] + Uline[1] ) >> 2;
				vc = ( 3*Vline[0] + Vline[1] ) >> 2;
				
				*(fb++) = clamp(( yc + yuvTbl_v2[vc] ) >> 16);
				*(fb++) = clamp(( yc - yuvTbl_u2[uc] - yuvTbl_v1[vc] ) >> 16);
				*(fb++) = clamp(( yc + yuvTbl_u1[uc] ) >> 16);

				/* Third pixel */
				yc = yuvTbl_y[*(Yline++)];
				uc = ( Uline[0] + Uline[1] ) >> 1;
				vc = ( Vline[0] + Vline[1] ) >> 1;

				*(fb++) = clamp(( yc + yuvTbl_v2[vc] ) >> 16);
				*(fb++) = clamp(( yc - yuvTbl_u2[uc] - yuvTbl_v1[vc] ) >> 16);
				*(fb++) = clamp(( yc + yuvTbl_u1[uc] ) >> 16);

				/* Fourth pixel */
				yc = yuvTbl_y[*(Yline++)];
				uc = ( Uline[0] + 3*Uline[1] ) >> 2;
				vc = ( Vline[0] + 3*Vline[1] ) >> 2;

				*(fb++) = clamp(( yc + yuvTbl_v2[vc] ) >> 16);
				*(fb++) = clamp(( yc - yuvTbl_u2[uc] - yuvTbl_v1[vc] ) >> 16);
				*(fb++) = clamp(( yc + yuvTbl_u1[uc] ) >> 16);

				/* Adjust pointers */
				Uline++;
				Vline++;
			}
				
			/* Adjust pointers */
			Uline++;
			Vline++;
		    }
		    break;
		case V4L2_PIX_FMT_YUV420:
		case V4L2_PIX_FMT_YVU420:
		    Yline_baseptr += img_width * 4;
		    Uline_baseptr += img_width;
		    Vline_baseptr += img_width;
		    break;
		}
	}

	/* Free our buffer */
	if (mainbuffer != NULL)
		free(mainbuffer);

	return 0;
}

/*
Return-Path: tnt@246tNt.com
Received: from zimbra16-e3.priv.proxad.net (LHLO
 zimbra16-e3.priv.proxad.net) (172.20.243.166) by
 zimbra16-e3.priv.proxad.net with LMTP; Mon, 14 Feb 2011 21:10:38 +0100
 (CET)
Received: from mailrelay011.isp.belgacom.be (mx26-g26.priv.proxad.net [172.20.243.96])
	by zimbra16-e3.priv.proxad.net (Postfix) with ESMTP id 1A661157C5B
	for <moinejf@free.fr>; Mon, 14 Feb 2011 21:10:38 +0100 (CET)
Received: from mailrelay011.isp.belgacom.be ([195.238.6.178])
	by mx1-g20.free.fr (MXproxy) for moinejf@free.fr ;
	Mon, 14 Feb 2011 21:10:36 +0100 (CET)
X-ProXaD-SC: state=HAM score=0
X-Belgacom-Dynamic: yes
X-IronPort-Anti-Spam-Filtered: true
X-IronPort-Anti-Spam-Result: ApIBAKsaWU1XQ5W2/2dsb2JhbAAMhBHOEpA5gSeBaYFYdgSLfw
Received: from 182.149-67-87.adsl-dyn.isp.belgacom.be (HELO [10.0.0.129]) ([87.67.149.182])
  by relay.skynet.be with ESMTP; 14 Feb 2011 21:10:36 +0100
Message-ID: <4D598C7C.7080307@246tNt.com>
Date: Mon, 14 Feb 2011 21:11:40 +0100
From: Sylvain Munaut <tnt@246tNt.com>
User-Agent: Mozilla/5.0 (X11; U; Linux x86_64; en-US; rv:1.9.2.13) Gecko/20101219 Lightning/1.0b3pre Thunderbird/3.1.7
MIME-Version: 1.0
To: Jean-Francois Moine <moinejf@free.fr>
CC: Kjell Claesson <keyson@users.sourceforge.net>
Subject: Re: nw80x as a gspca subdriv
References: <20110209204208.4b19df88@tele>	<4D53B3BF.9050908@246tNt.com> <20110214205107.18c29303@tele>
In-Reply-To: <20110214205107.18c29303@tele>
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: 7bit

	[snip]
> May I have your permission to relicense your JPEG Lite decompression
> code under the LGPL (version 2 or later)?  

Yes, sure.

"""
I hereby allow the nw80x driver code, including the jpeg lite decoding
routines, to be used and distributed under the LGPL v2 or later.
"""
	[snip]
Cheers,

    Sylvain
 */
