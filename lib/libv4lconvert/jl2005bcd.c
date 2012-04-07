/* jl2005bcd.c
 *
 * Converts raw output from Jeilin JL2005B/C/D to Bayer output.
 *
 * The code is based upon what is found in libgphoto2/camlibs/jl2005c,
 *   Copyright (c) 2010 Theodore Kilgore <kilgota@auburn.edu>
 *
 * The decompression code used is
 *   Copyright (c) 2010 Hans de Goede <hdegoede@redhat.com> 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <config.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_JPEG
#include "libv4lconvert-priv.h"
#include "jpeg_memsrcdest.h"
#include "libv4lsyscall-priv.h"


#define JPEG_HEADER_SIZE	338
#define JPEG_HEIGHT_OFFSET	 94

static int find_eoi(struct v4lconvert_data *data,
        const unsigned char *jpeg_data, int jpeg_data_idx, int jpeg_data_size)
{
	int i;

	for (i = jpeg_data_idx; i < (jpeg_data_size - 1); i++)
		if (jpeg_data[i] == 0xff && jpeg_data[i + 1] == 0xd9)
			break;

	if (i >= (jpeg_data_size - 1)) {
		V4LCONVERT_ERR("incomplete jl2005bcd frame\n");
		return -1;
	}

	return i + 2; /* + 2 -> Point to after EOI marker */
}


int v4lconvert_decode_jl2005bcd(struct v4lconvert_data *data,
		const unsigned char *src, int src_size,
                unsigned char *dest, int width, int height)
{
	unsigned char jpeg_stripe[50000];
	int q;
	struct jpeg_compress_struct cinfo;
	struct jpeg_decompress_struct dinfo;
	struct jpeg_error_mgr jcerr, jderr;
	JOCTET *jpeg_header = NULL;
	unsigned long jpeg_header_size = 0;
	int i, x, y, x1, y1, jpeg_data_size, jpeg_data_idx, eoi, size;

	/* src_size had better be bigger than 16 */
	if (src_size < 16)
		return 1;

	/* The first 16 bytes of src are a raw frame header */
	q = src[13] & 0x7f;

	if (height != src[4] << 3) {
		V4LCONVERT_ERR("Height is %d, not %d\n", src[4] << 3, height);
		return 1;
	}

	if (width != src[5] << 3) {
		V4LCONVERT_ERR("Width is %d, not %d\n", src[5] << 3, width);
		return 1;
	}

	/*
	 * And the fun begins, first of all create a dummy jpeg, which we use
	 * to get the headers from to feed to libjpeg when decompressing the
	 * stripes. This way we can use libjpeg's quant table handling
	 * (and built in default huffman tables).
	 */
	cinfo.err = jpeg_std_error (&jcerr);
	jpeg_create_compress (&cinfo);
	jpeg_mem_dest (&cinfo, &jpeg_header, &jpeg_header_size);
	cinfo.image_width = 16;
	cinfo.image_height = 16;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults (&cinfo);
	/* Make comp[0] (which will be green) 1x2 subsampled */
	cinfo.comp_info[0].h_samp_factor = 1;
	cinfo.comp_info[0].v_samp_factor = 2;
	/* Make comp[1] and [2] use huffman table and quanttable 0, as all
	 * components use luminance settings with the jl2005c/d/e */
	cinfo.comp_info[1].quant_tbl_no = 0;
	cinfo.comp_info[1].dc_tbl_no = 0;
	cinfo.comp_info[1].ac_tbl_no = 0;
	cinfo.comp_info[2].quant_tbl_no = 0;
	cinfo.comp_info[2].dc_tbl_no = 0;
	cinfo.comp_info[2].ac_tbl_no = 0;
	/* Apply the quality setting from the header */
	if (q <= 0)
		i = 5000;
	else if (q <= 50)
		i = 5000 / q;
	else if (q <= 100)
		i = 2 * (100 - q);
	else
		i = 0;
	jpeg_set_linear_quality(&cinfo, i, TRUE);

	jpeg_start_compress (&cinfo, TRUE);
	while( cinfo.next_scanline < cinfo.image_height ) {
		JOCTET row[16 * 3];
		JSAMPROW row_pointer[1] = { row };
		jpeg_write_scanlines (&cinfo, row_pointer, 1);
	}
	jpeg_finish_compress (&cinfo);
	jpeg_destroy_compress (&cinfo);

	JSAMPLE green[8 * 16];
	JSAMPLE red[8 * 8];
	JSAMPLE blue[8 * 8];
	JSAMPROW green_row_pointer[16];
	JSAMPROW red_row_pointer[8];
	JSAMPROW blue_row_pointer[8];

	for (i = 0; i < 16; i++)
		green_row_pointer[i] = green + i * 8;

	for (i = 0; i < 8; i++) {
		red_row_pointer[i] = red + i * 8;
		blue_row_pointer[i] = blue + i * 8;
	}

	JSAMPARRAY samp_image[3] = { green_row_pointer,
				     red_row_pointer,
				     blue_row_pointer };

	memcpy(jpeg_stripe, jpeg_header, JPEG_HEADER_SIZE);
	jpeg_stripe[JPEG_HEIGHT_OFFSET    ] = height >> 8;
	jpeg_stripe[JPEG_HEIGHT_OFFSET + 1] = height;
	jpeg_stripe[JPEG_HEIGHT_OFFSET + 2] = 0;
	jpeg_stripe[JPEG_HEIGHT_OFFSET + 3] = 8;
	free (jpeg_header);
	/* Get past the raw frame header. */
	src += 16;
	jpeg_data_size = src_size - 16;

	jpeg_data_idx = 0;


	dinfo.err = jpeg_std_error (&jderr);
	jpeg_create_decompress (&dinfo);
	for (x = 0; x < width; x += 16) {
		eoi = find_eoi(data, src, jpeg_data_idx, jpeg_data_size);
		if (eoi < 0)
			return eoi;

		size = eoi - jpeg_data_idx;
		if ((JPEG_HEADER_SIZE + size) > sizeof(jpeg_stripe)) {
			V4LCONVERT_ERR("stripe size too big %d > %zd\n",
				       JPEG_HEADER_SIZE + size,
				       sizeof(jpeg_stripe));
			return 1;
		}
		memcpy (jpeg_stripe + JPEG_HEADER_SIZE,	
			src + jpeg_data_idx, size);

		jpeg_mem_src (&dinfo, jpeg_stripe, JPEG_HEADER_SIZE + size);
		jpeg_read_header (&dinfo, TRUE);
		dinfo.raw_data_out = TRUE;
#if JPEG_LIB_VERSION >= 70
		dinfo.do_fancy_upsampling = FALSE;
#endif
		jpeg_start_decompress (&dinfo);
		for (y = 0; y < height; y += 16) {
			jpeg_read_raw_data (&dinfo, samp_image, 16);
			for (y1 = 0; y1 < 16; y1 += 2) {
				for (x1 = 0; x1 < 16; x1 += 2) {
					dest[((y + y1 + 0) * width
							+ x + x1 + 0)]
						= red[y1 * 4 + x1 / 2];
					dest[((y + y1 + 0) * width
							+ x + x1 + 1)]
						= green[y1 * 8 + x1 / 2];
					dest[((y + y1 + 1) * width
							+ x + x1 + 0)]
						= green[y1 * 8 + 8 + x1 / 2];
					dest[((y + y1 + 1) * width
							+ x + x1 + 1)]
						= blue[y1 * 4 + x1 / 2];
				}
			}
		}
		jpeg_finish_decompress (&dinfo);

		/* Set jpeg_data_idx for the next stripe */
		jpeg_data_idx = (jpeg_data_idx + size + 0x0f) & ~0x0f;
	}
	jpeg_destroy_decompress(&dinfo);
	return 0;
}

#endif /* HAVE_JPEG */
