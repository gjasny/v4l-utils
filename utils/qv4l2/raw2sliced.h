/*
 * Raw VBI to sliced VBI parser.
 *
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

#ifndef _RAW2SLICED_H
#define _RAW2SLICED_H

#include <linux/videodev2.h>

#define VBI_MAX_SERVICES (3)

// Bit slicer internal struct
struct vbi_bit_slicer {
	unsigned int		service;
	unsigned int		cri;
	unsigned int		cri_mask;
	unsigned int		thresh;
	unsigned int		thresh_frac;
	unsigned int		cri_samples;
	unsigned int		cri_rate;
	unsigned int		oversampling_rate;
	unsigned int		phase_shift;
	unsigned int		step;
	unsigned int		frc;
	unsigned int		frc_bits;
	unsigned int		payload;
	unsigned int		endian;
};

struct vbi_handle {
	unsigned services;
	unsigned start_of_field_2;
	unsigned stride;
	bool interlaced;
	int start[2];
	int count[2];
	struct vbi_bit_slicer slicers[VBI_MAX_SERVICES];
};

// Fills in vbi_handle based on the standard and VBI format
// Returns true if one or more services are valid for the fmt/std combination.
bool vbi_prepare(struct vbi_handle *vh,
		const struct v4l2_vbi_format *fmt, v4l2_std_id std);

// Parses the raw buffer and fills in sliced_vbi_format and _data.
// data must be an array of count[0] + count[1] v4l2_sliced_vbi_data structs.
void vbi_parse(struct vbi_handle *vh, const unsigned char *buf,
		struct v4l2_sliced_vbi_format *vbi,
		struct v4l2_sliced_vbi_data *data);

#endif
