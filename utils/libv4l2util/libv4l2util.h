/* Copyright (C) 2006 Hans Verkuil <hverkuil@xs4all.nl>

   The libv4l2util Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The libv4l2util Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the libv4l2util Library; if not, write to the Free
   Software Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA
   02110-1335 USA.  */

#ifndef _V4L2UTIL_H_
#define _V4L2UTIL_H_

/* --------------------------------------------------------------------- */

struct v4l2_channel_list {
    const char * const name; /* channel name */
    unsigned freq; 	     /* channel frequency in kHz */
};

struct v4l2_channel_lists {
    const char * const name; /* channel list name */
    const struct v4l2_channel_list * const list;
    unsigned count;	     /* number of channels in channel list */
};

extern const struct v4l2_channel_lists v4l2_channel_lists[];

/* This list is sorted alphabetically on ISO code. The last item is
   denoted by a NULL pointer for the iso_code. */
struct v4l2_country_std_map {
    const char * const iso_code; /* The 2-character upper case ISO-3166 country code */
    v4l2_std_id std; 		 /* The TV standard(s) in use */
};

extern const struct v4l2_country_std_map v4l2_country_std_map[];

#endif
