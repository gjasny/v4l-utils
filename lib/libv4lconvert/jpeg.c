/*
#             (C) 2008-2011 Hans de Goede <hdegoede@redhat.com>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <errno.h>
#include "libv4lconvert-priv.h"

int v4lconvert_decode_jpeg_tinyjpeg(struct v4lconvert_data *data,
	unsigned char *src, int src_size, unsigned char *dest,
	struct v4l2_format *fmt, unsigned int dest_pix_fmt, int flags)
{
	int result = 0;
	unsigned char *components[3];
	unsigned int header_width, header_height;
	unsigned int width  = fmt->fmt.pix.width;
	unsigned int height = fmt->fmt.pix.height;

	if (!data->tinyjpeg) {
		data->tinyjpeg = tinyjpeg_init();
		if (!data->tinyjpeg)
			return v4lconvert_oom_error(data);
	}
	flags |= TINYJPEG_FLAGS_MJPEG_TABLE;
	tinyjpeg_set_flags(data->tinyjpeg, flags);
	if (tinyjpeg_parse_header(data->tinyjpeg, src, src_size)) {
		V4LCONVERT_ERR("parsing JPEG header: %s",
				tinyjpeg_get_errorstring(data->tinyjpeg));
		errno = EIO;
		return -1;
	}
	tinyjpeg_get_size(data->tinyjpeg, &header_width, &header_height);

	if (data->control_flags & V4LCONTROL_ROTATED_90_JPEG) {
		unsigned int tmp = width;
		width = height;
		height = tmp;
	}

	if (header_width != width || header_height != height) {
		V4LCONVERT_ERR("unexpected width / height in JPEG header"
			       "expected: %ux%u, header: %ux%u\n",
			       width, height, header_width, header_height);
		errno = EIO;
		return -1;
	}
	fmt->fmt.pix.width = header_width;
	fmt->fmt.pix.height = header_height;

	components[0] = dest;

	switch (dest_pix_fmt) {
	case V4L2_PIX_FMT_RGB24:
		tinyjpeg_set_components(data->tinyjpeg, components, 1);
		result = tinyjpeg_decode(data->tinyjpeg, TINYJPEG_FMT_RGB24);
		break;
	case V4L2_PIX_FMT_BGR24:
		tinyjpeg_set_components(data->tinyjpeg, components, 1);
		result = tinyjpeg_decode(data->tinyjpeg, TINYJPEG_FMT_BGR24);
		break;
	case V4L2_PIX_FMT_YUV420:
		components[1] = components[0] + width * height;
		components[2] = components[1] + width * height / 4;
		tinyjpeg_set_components(data->tinyjpeg, components, 3);
		result = tinyjpeg_decode(data->tinyjpeg, TINYJPEG_FMT_YUV420P);
		break;
	case V4L2_PIX_FMT_YVU420:
		components[2] = components[0] + width * height;
		components[1] = components[2] + width * height / 4;
		tinyjpeg_set_components(data->tinyjpeg, components, 3);
		result = tinyjpeg_decode(data->tinyjpeg, TINYJPEG_FMT_YUV420P);
		break;
	}

	if (result) {
		/* The JPEG header checked out ok but we got an error
		   during decompression. Some webcams, esp pixart and
		   sn9c20x based ones regulary generate corrupt frames,
		   which are best thrown away to avoid flashes in the
		   video stream. We use EPIPE to signal the upper layer
		   we have some video data, but it is incomplete.

		   The upper layer (usually libv4l2) should respond to
		   this by trying a number of times to get a new frame
		   and if that fails just passing up whatever we did
		   manage to decompress. */
		V4LCONVERT_ERR("decompressing JPEG: %s",
				tinyjpeg_get_errorstring(data->tinyjpeg));
		errno = EPIPE;
		return -1;
	}
	return 0;
}
