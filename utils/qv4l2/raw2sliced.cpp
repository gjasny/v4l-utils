#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/fcntl.h>

#include "raw2sliced.h"

/*
 * The slicing code was copied from libzvbi. The original copyright notice is:
 *
 * Copyright (C) 2000-2004 Michael H. Schimek
 *
 * The vbi_prepare/vbi_parse functions are:
 *
 * Copyright (C) 2012 Hans Verkuil <hans.verkuil@cisco.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the 
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA  02110-1301  USA.
 */

// Modulation used for VBI data transmission.
enum vbi_modulation {
	/*
	 * The data is 'non-return to zero' coded, logical '1' bits
	 * are described by high sample values, logical '0' bits by
	 * low values. The data is last significant bit first transmitted.
	 */
	VBI_MODULATION_NRZ_LSB,
	/*
	 * The data is 'bi-phase' coded. Each data bit is described
	 * by two complementary signalling elements, a logical '1'
	 * by a sequence of '10' elements, a logical '0' by a '01'
	 * sequence. The data is last significant bit first transmitted.
	 */
	VBI_MODULATION_BIPHASE_LSB,
	/*
	 * 'Bi-phase' coded, most significant bit first transmitted.
	 */
	VBI_MODULATION_BIPHASE_MSB
};

// Service definition struct
struct service {
	__u16 service;
	v4l2_std_id std;
	/*
	 * Most scan lines used by the data service, first and last
	 * line of first and second field. ITU-R numbering scheme.
	 * Zero if no data from this field, requires field sync.
	 */
	int		first[2];
        int		last[2];

	/*
	 * Leading edge hsync to leading edge first CRI one bit,
	 * half amplitude points, in nanoseconds.
	 */
	unsigned int		offset;

	unsigned int		cri_rate;	/* Hz */
	unsigned int		bit_rate;	/* Hz */

	/* Clock Run In and FRaming Code, LSB last txed bit of FRC. */
	unsigned int		cri_frc;

	/* CRI and FRC bits significant for identification. */
	unsigned int		cri_frc_mask;

	/*
	 * Number of significat cri_bits (at cri_rate),
	 * frc_bits (at bit_rate).
	 */
	unsigned int		cri_bits;
	unsigned int		frc_bits;

	unsigned int		payload;	/* bits */
	enum vbi_modulation	modulation;
};

// Supported services
static const struct service services[] = {
	{
		V4L2_SLICED_TELETEXT_B,
		V4L2_STD_625_50,
		{ 6, 318 },
		{ 22, 335 },
		10300, 6937500, 6937500, /* 444 x FH */
		0x00AAAAE4, 0xFFFF, 18, 6, 42 * 8,
		VBI_MODULATION_NRZ_LSB,
	}, {
		V4L2_SLICED_VPS,
		V4L2_STD_PAL_BG,
		{ 16, 0 },
		{ 16, 0 },
		12500, 5000000, 2500000, /* 160 x FH */
		0xAAAA8A99, 0xFFFFFF, 32, 0, 13 * 8,
		VBI_MODULATION_BIPHASE_MSB,
	}, {
		V4L2_SLICED_WSS_625,
		V4L2_STD_625_50,
		{ 23, 0 },
		{ 23, 0 },
		11000, 5000000, 833333, /* 160/3 x FH */
		/* ...1000 111 / 0 0011 1100 0111 1000 0011 111x */
		/* ...0010 010 / 0 1001 1001 0011 0011 1001 110x */	
		0x8E3C783E, 0x2499339C, 32, 0, 14 * 1,
		VBI_MODULATION_BIPHASE_LSB,
	}, {
		V4L2_SLICED_CAPTION_525,
		V4L2_STD_525_60,
		{ 21, 284 },
		{ 21, 284 },
		10500, 1006976, 503488, /* 32 x FH */
		/* Test of CRI bits has been removed to handle the
		   incorrect signal observed by Rich Kandel (see
		   _VBI_RAW_SHIFT_CC_CRI). */
		0x03, 0x0F, 4, 0, 2 * 8,
		VBI_MODULATION_NRZ_LSB,
	}
};

static const unsigned int DEF_THR_FRAC = 9;
static const unsigned int LP_AVG = 4;

static inline unsigned int vbi_sample(const uint8_t *raw, unsigned i)
{
	unsigned ii = i >> 8;
	unsigned int raw0 = raw[ii];
	unsigned int raw1 = raw[ii + 1];

	return (int)(raw1 - raw0) * (i & 255) + (raw0 << 8);
}

// Slice the raw data
static bool low_pass_bit_slicer_Y8(struct vbi_bit_slicer *bs, uint8_t *buffer, const uint8_t *raw)
{
	unsigned int i, j;
	unsigned int cl;	/* clock */
	unsigned int thresh0;	/* old 0/1 threshold */
	unsigned int tr;	/* current threshold */
	unsigned int c;		/* current byte */
	unsigned int t;		/* t = raw[0] * j + raw[1] * (1 - j) */
	unsigned int raw0;	/* oversampling temporary */
	unsigned int raw1;
	unsigned char b1;	/* previous bit */
	unsigned int oversampling = 4;

	thresh0 = bs->thresh;

	c = 0;
	cl = 0;
	b1 = 0;

	for (i = bs->cri_samples; i > 0; --i) {
		int r;
		tr = bs->thresh >> bs->thresh_frac;
		raw0 = raw[0];
		raw1 = raw[1];
		raw1 -= raw0;
		r = raw1;
		bs->thresh += (int)(raw0 - tr) * (r < 0 ? -r : r);
		t = raw0 * oversampling;

		for (j = oversampling; j > 0; --j) {
			unsigned int tavg;
			unsigned char b; /* current bit */

			tavg = (t + (oversampling / 2))	/ oversampling;
			b = (tavg >= tr);

			if ((b ^ b1)) {
				cl = bs->oversampling_rate >> 1;
			} else {
				cl += bs->cri_rate;

				if (cl >= bs->oversampling_rate) {
					cl -= bs->oversampling_rate;
					c = c * 2 + b;
					if ((c & bs->cri_mask) == bs->cri)
						break;
				}
			}

			b1 = b;

			if (oversampling > 1)
				t += raw1;
		}
		if (j)
			break;

		raw++;
	}
	if (i == 0) {
		bs->thresh = thresh0;
		return false;
	}

	i = bs->phase_shift; /* current bit position << 8 */
	tr *= 256;
	c = 0;

	for (j = bs->frc_bits; j > 0; --j) {
		raw0 = vbi_sample(raw, i);
		c = c * 2 + (raw0 >= tr);
		i += bs->step; /* next bit */
	}

	if (c != bs->frc) {
		bs->thresh = thresh0;
		return false;
	}

	c = 0;

	if (bs->endian) {
		/* bitwise, lsb first */
		for (j = 0; j < bs->payload; ++j) {
			raw0 = vbi_sample(raw, i);
			c = (c >> 1) + ((raw0 >= tr) << 7);
			i += bs->step;
			if ((j & 7) == 7)
				*buffer++ = c;
		}
		*buffer = c >> ((8 - bs->payload) & 7);
	} else {
		/* bitwise, msb first */
		for (j = 0; j < bs->payload; ++j) {
			raw0 = vbi_sample(raw, i);
			c = c * 2 + (raw0 >= tr);
			i += bs->step;
			if ((j & 7) == 7)
				*buffer++ = c;
		}
		*buffer = c & ((1 << (bs->payload & 7)) - 1);
	}

	return true;
}

// Prepare the vbi_bit_slicer struct
static bool vbi_bit_slicer_prepare(struct vbi_bit_slicer *bs,
		const struct service *s,
		const struct v4l2_vbi_format *fmt)
{
	unsigned int c_mask;
	unsigned int f_mask;
	unsigned int min_samples_per_bit;
	unsigned int oversampling;
	unsigned int data_bits;
	unsigned int data_samples;
	unsigned int cri, cri_mask, frc;
	unsigned int cri_end;

	assert (s->cri_bits <= 32);
	assert (s->frc_bits <= 32);
	assert (s->payload <= 32767);
	assert (fmt->samples_per_line <= 32767);

	cri = s->cri_frc >> s->frc_bits;
	cri_mask = s->cri_frc_mask >> s->frc_bits;
	frc = (s->cri_frc & ((1U << s->frc_bits) - 1));
	if (s->cri_rate > fmt->sampling_rate) {
		fprintf(stderr, "cri_rate %u > sampling_rate %u.\n",
			 s->cri_rate, fmt->sampling_rate);
		return false;
	}

	if (s->bit_rate > fmt->sampling_rate) {
		fprintf(stderr, "bit_rate %u > sampling_rate %u.\n",
			 s->bit_rate, fmt->sampling_rate);
		return false;
	}

	min_samples_per_bit = fmt->sampling_rate / ((s->cri_rate > s->bit_rate) ? s->cri_rate : s->bit_rate);

	c_mask = (s->cri_bits == 32) ? ~0U : (1U << s->cri_bits) - 1;
	f_mask = (s->frc_bits == 32) ? ~0U : (1U << s->frc_bits) - 1;

	oversampling = 4;

	/* 0-1 threshold, start value. */
	bs->thresh = 105 << DEF_THR_FRAC;
	bs->thresh_frac = DEF_THR_FRAC;

	if (min_samples_per_bit > (3U << (LP_AVG - 1))) {
		oversampling = 1;
		bs->thresh <<= LP_AVG - 2;
		bs->thresh_frac += LP_AVG - 2;
	}

	bs->cri_mask = cri_mask & c_mask;
	bs->cri = cri & bs->cri_mask;

	data_bits = s->payload + s->frc_bits;
	data_samples = (fmt->sampling_rate * (int64_t) data_bits) / s->bit_rate;

	cri_end = fmt->samples_per_line - data_samples;

	bs->cri_samples = cri_end;
	bs->cri_rate = s->cri_rate;

	bs->oversampling_rate = fmt->sampling_rate * oversampling;

	bs->frc = frc & f_mask;
	bs->frc_bits = s->frc_bits;

	/* Payload bit distance in 1/256 raw samples. */
	bs->step = (fmt->sampling_rate * (int64_t) 256) / s->bit_rate;

	bs->payload = s->payload;
	bs->endian = 1;

	switch (s->modulation) {
	case VBI_MODULATION_NRZ_LSB:
		bs->phase_shift	= (int)
			(fmt->sampling_rate * 256.0 / s->cri_rate * .5
			 + bs->step * .5 + 128);
		break;

	case VBI_MODULATION_BIPHASE_MSB:
		bs->endian = 0;
		/* fall through */
	case VBI_MODULATION_BIPHASE_LSB:
		/* Phase shift between the NRZ modulated CRI and the
		   biphase modulated rest. */
		bs->phase_shift	= (int)
			(fmt->sampling_rate * 256.0 / s->cri_rate * .5
			 + bs->step * .25 + 128);
		break;
	}
	return true;
}

bool vbi_prepare(struct vbi_handle *vh, const struct v4l2_vbi_format *fmt, v4l2_std_id std)
{
	unsigned i;

	memset(vh, 0, sizeof(*vh));
	// Sanity check
	if ((std & V4L2_STD_525_60) && (std & V4L2_STD_625_50))
		return false;
	vh->start_of_field_2 = (std & V4L2_STD_525_60) ? 263 : 313;
	vh->stride = fmt->samples_per_line;
	vh->interlaced = fmt->flags & V4L2_VBI_INTERLACED;
	vh->start[0] = fmt->start[0];
	vh->start[1] = fmt->start[1];
	vh->count[0] = fmt->count[0];
	vh->count[1] = fmt->count[1];
	for (i = 0; i < sizeof(services) / sizeof(services[0]); i++) {
		const struct service *s = services + i;
		struct vbi_bit_slicer *slicer = vh->slicers + vh->services;

		if (!(std & s->std))
			continue;
		if (s->last[0] < vh->start[0] &&
		    s->last[1] < vh->start[1])
			continue;
		if (s->first[0] >= vh->start[0] + vh->count[0] &&
		    s->first[1] >= vh->start[1] + vh->count[1])
			continue;
		slicer->service = i;
		vbi_bit_slicer_prepare(slicer, s, fmt);
		vh->services++;
	}
	return vh->services;
}

void vbi_parse(struct vbi_handle *vh, const unsigned char *buf,
		struct v4l2_sliced_vbi_format *vbi,
		struct v4l2_sliced_vbi_data *data)
{
	const unsigned char *p;
	unsigned i;
	int y;

	memset(vbi, 0, sizeof(*vbi));
	vbi->io_size = sizeof(*data) * (vh->count[0] + vh->count[1]);
	for (i = 0; i < vh->services; i++) {
		const struct service *s = services + vh->slicers[i].service;

		for (y = s->first[0] - vh->start[0]; y <= s->last[0] - vh->start[0]; y++) {
			if (y < 0 || y >= vh->count[0])
				continue;
			if (vh->interlaced)
				p = buf + vh->stride * y * 2;
			else
				p = buf + vh->stride * y;
			data[y].id = data[y].reserved = 0;
			if (low_pass_bit_slicer_Y8(vh->slicers + i, data[y].data, p)) {
				vbi->service_set |= s->service;
				vbi->service_lines[0][y + vh->start[0]] = s->service;
				data[y].id = s->service;
				data[y].field = 0;
				data[y].line = y + vh->start[0];
			}
		}

		for (y = s->first[1] - vh->start[1]; y <= s->last[1] - vh->start[1]; y++) {
			unsigned yy = y + vh->count[0];

			if (y < 0 || y >= vh->count[1])
				continue;
			if (vh->interlaced)
				p = buf + vh->stride * y * 2 + 1;
			else
				p = buf + vh->stride * yy;
			data[yy].id = data[yy].reserved = 0;
			if (low_pass_bit_slicer_Y8(vh->slicers + i, data[yy].data, p)) {
				vbi->service_set |= s->service;
				vbi->service_lines[1][y + vh->start[1] - vh->start_of_field_2] = s->service;
				data[yy].id = s->service;
				data[yy].field = 1;
				data[yy].line = y + vh->start[1] - vh->start_of_field_2;
			}
		}
	}
}
