/*
    V4L2 API format ioctl tests.

    Copyright (C) 2011  Hans Verkuil <hans.verkuil@cisco.com>

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
    Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <assert.h>
#include "v4l2-compliance.h"

static const __u32 buftype2cap[] = {
	0,
	V4L2_CAP_VIDEO_CAPTURE,
	V4L2_CAP_VIDEO_OUTPUT,
	V4L2_CAP_VIDEO_OVERLAY,
	V4L2_CAP_VBI_CAPTURE,
	V4L2_CAP_VBI_OUTPUT,
	V4L2_CAP_SLICED_VBI_CAPTURE,
	V4L2_CAP_SLICED_VBI_OUTPUT,
	V4L2_CAP_VIDEO_OUTPUT_OVERLAY,
	// To be discussed
	V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_CAPTURE,
	V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_OUTPUT,
};

static int testEnumFrameIntervals(struct node *node, __u32 pixfmt, __u32 w, __u32 h, bool valid)
{
	struct v4l2_frmivalenum frmival;
	struct v4l2_frmival_stepwise *sw = &frmival.stepwise;
	bool found_stepwise = false;
	unsigned f = 0;
	int ret;

	for (;;) {
		memset(&frmival, 0xff, sizeof(frmival));
		frmival.index = f;
		frmival.pixel_format = pixfmt;
		frmival.width = w;
		frmival.height = h;

		ret = doioctl(node, VIDIOC_ENUM_FRAMEINTERVALS, &frmival);
		if (f == 0 && ret == EINVAL) {
			if (valid)
				warn("found framesize %dx%d, but no frame intervals\n", w, h);
			return -ENOSYS;
		}
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("expected EINVAL, but got %d when enumerating frameinterval %d\n", ret, f);
		ret = check_0(frmival.reserved, sizeof(frmival.reserved));
		if (ret)
			return fail("frmival.reserved not zeroed\n");
		if (frmival.pixel_format != pixfmt || frmival.index != f ||
				frmival.width != w || frmival.height != h)
			return fail("frmival.pixel_format, index, width or height changed\n");
		switch (frmival.type) {
		case V4L2_FRMIVAL_TYPE_DISCRETE:
			ret = check_fract(&frmival.discrete);
			if (found_stepwise)
				return fail("mixing discrete and stepwise is not allowed\n");
			break;
		case V4L2_FRMIVAL_TYPE_CONTINUOUS:
			if (sw->step.numerator != 1 || sw->step.denominator != 1)
				return fail("invalid step for continuous frameinterval\n");
			/* fallthrough */
		case V4L2_FRMIVAL_TYPE_STEPWISE:
			if (frmival.index)
				return fail("index must be 0 for stepwise/continuous frameintervals\n");
			found_stepwise = true;
			ret = check_fract(&sw->min);
			if (ret == 0)
				ret = check_fract(&sw->max);
			if (ret == 0)
				ret = check_fract(&sw->step);
			if (ret)
				return fail("invalid min, max or step for frameinterval %d\n", f);
			if (fract2f(&sw->min) > fract2f(&sw->max))
				return fail("min > max\n");
			if (fract2f(&sw->step) > fract2f(&sw->max) - fract2f(&sw->min))
				return fail("step > (max - min)\n");
			break;
		default:
			return fail("frmival.type is invalid\n");
		}
		
		f++;
	}
	if (!valid)
		return fail("found frame intervals for invalid size %dx%d\n", w, h);
	info("found %d frameintervals for pixel format %08x and size %dx%d\n", f, pixfmt, w, h);
	return 0;
}

static int testEnumFrameSizes(struct node *node, __u32 pixfmt)
{
	struct v4l2_frmsizeenum frmsize;
	struct v4l2_frmsize_stepwise *sw = &frmsize.stepwise;
	bool found_stepwise = false;
	unsigned f = 0;
	int ret;

	for (;;) {
		memset(&frmsize, 0xff, sizeof(frmsize));
		frmsize.index = f;
		frmsize.pixel_format = pixfmt;

		ret = doioctl(node, VIDIOC_ENUM_FRAMESIZES, &frmsize);
		if (f == 0 && ret == EINVAL)
			return -ENOSYS;
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("expected EINVAL, but got %d when enumerating framesize %d\n", ret, f);
		ret = check_0(frmsize.reserved, sizeof(frmsize.reserved));
		if (ret)
			return fail("frmsize.reserved not zeroed\n");
		if (frmsize.pixel_format != pixfmt || frmsize.index != f)
			return fail("frmsize.pixel_format or index changed\n");
		switch (frmsize.type) {
		case V4L2_FRMSIZE_TYPE_DISCRETE:
			if (frmsize.discrete.width == 0 || frmsize.discrete.height == 0)
				return fail("invalid width/height for discrete framesize\n");
			if (found_stepwise)
				return fail("mixing discrete and stepwise is not allowed\n");
			ret = testEnumFrameIntervals(node, pixfmt,
					frmsize.discrete.width, frmsize.discrete.height, true);
			if (ret > 0)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					frmsize.discrete.width + 1, frmsize.discrete.height, false);
			if (ret > 0)
				return ret;
			break;
		case V4L2_FRMSIZE_TYPE_CONTINUOUS:
			if (frmsize.stepwise.step_width != 1 || frmsize.stepwise.step_height != 1)
				return fail("invalid step_width/height for continuous framesize\n");
			/* fallthrough */
		case V4L2_FRMSIZE_TYPE_STEPWISE:
			if (frmsize.index)
				return fail("index must be 0 for stepwise/continuous framesizes\n");
			found_stepwise = true;
			if (!sw->min_width || !sw->min_height || !sw->step_width || !sw->step_height)
				return fail("0 for min_width/height or step_width/height\n");
			if (sw->min_width > sw->max_width || sw->min_height > sw->max_height)
				return fail("min_width/height > max_width/height\n");
			if (sw->step_width > sw->max_width - sw->min_width ||
			    sw->step_height > sw->max_height - sw->min_height)
				return fail("step > max - min for width or height\n");
			ret = testEnumFrameIntervals(node, pixfmt,
					sw->min_width, sw->min_height, true);
			if (ret > 0)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					sw->max_width, sw->max_height, true);
			if (ret > 0)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					sw->min_width - 1, sw->min_height, false);
			if (ret > 0)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					sw->max_width, sw->max_height + 1, false);
			if (ret > 0)
				return ret;
			break;
		default:
			return fail("frmsize.type is invalid\n");
		}
		
		f++;
	}
	info("found %d framesizes for pixel format %08x\n", f, pixfmt);
	return 0;
}

static int testEnumFormatsType(struct node *node, enum v4l2_buf_type type)
{
	pixfmt_set &set = node->buftype_pixfmts[type];
	struct v4l2_fmtdesc fmtdesc;
	unsigned f = 0;
	int ret;

	for (;;) {
		memset(&fmtdesc, 0xff, sizeof(fmtdesc));
		fmtdesc.type = type;
		fmtdesc.index = f;

		ret = doioctl(node, VIDIOC_ENUM_FMT, &fmtdesc);
		if (f == 0 && ret == EINVAL)
			return -ENOSYS;
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("expected EINVAL, but got %d when enumerating buftype %d\n", ret, type);
		ret = check_0(fmtdesc.reserved, sizeof(fmtdesc.reserved));
		if (ret)
			return fail("fmtdesc.reserved not zeroed\n");
		if (fmtdesc.index != f)
			return fail("fmtdesc.index was modified\n");
		if (fmtdesc.type != type)
			return fail("fmtdesc.type was modified\n");
		ret = check_ustring(fmtdesc.description, sizeof(fmtdesc.description));
		if (ret)
			return fail("fmtdesc.description not set\n");
		if (!fmtdesc.pixelformat)
			return fail("fmtdesc.pixelformat not set\n");
		if (!wrapper && (fmtdesc.flags & V4L2_FMT_FLAG_EMULATED))
			return fail("drivers must never set the emulated flag\n");
		if (fmtdesc.flags & ~(V4L2_FMT_FLAG_COMPRESSED | V4L2_FMT_FLAG_EMULATED))
			return fail("unknown flag %08x returned\n", fmtdesc.flags);
		ret = testEnumFrameSizes(node, fmtdesc.pixelformat);
		if (ret > 0)
			return ret;
		if (ret == 0 && !(node->caps & (V4L2_CAP_VIDEO_CAPTURE | V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)))
			return fail("found framesizes when no video capture is supported\n");
		f++;
		if (type == V4L2_BUF_TYPE_PRIVATE)
			continue;
		// Update array in v4l2-compliance.h if new buffer types are added
		assert(type <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		if (set.find(fmtdesc.pixelformat) != set.end())
			return fail("duplicate format %08x\n", fmtdesc.pixelformat);
		set.insert(fmtdesc.pixelformat);
	}
	info("found %d formats for buftype %d\n", f, type);
	return 0;
}

int testEnumFormats(struct node *node)
{
	static const __u32 buftype2cap[] = {
		0,
		V4L2_CAP_VIDEO_CAPTURE,
		V4L2_CAP_VIDEO_OUTPUT,
		V4L2_CAP_VIDEO_OVERLAY,
		V4L2_CAP_VBI_CAPTURE,
		V4L2_CAP_VBI_OUTPUT,
		V4L2_CAP_SLICED_VBI_CAPTURE,
		V4L2_CAP_SLICED_VBI_OUTPUT,
		V4L2_CAP_VIDEO_OUTPUT_OVERLAY,
		V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		V4L2_CAP_VIDEO_OUTPUT_MPLANE,
	};
	int type;
	int ret;

	for (type = 0; type <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; type++) {
		ret = testEnumFormatsType(node, (enum v4l2_buf_type)type);
		if (ret > 0)
			return ret;
		switch (type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
			if (ret < 0 && (node->caps & buftype2cap[type]))
				return fail("%s cap set, but no %s formats defined\n",
						buftype2s(type).c_str(), buftype2s(type).c_str());
			if (!ret && !(node->caps & buftype2cap[type]))
				return fail("%s cap not set, but %s formats defined\n",
						buftype2s(type).c_str(), buftype2s(type).c_str());
			break;
		default:
			if (!ret)
				return fail("Buffer type %s not allowed!\n", buftype2s(type).c_str());
			break;
		}
	}

	ret = testEnumFormatsType(node, V4L2_BUF_TYPE_PRIVATE);
	if (ret > 0)
		return ret;
	if (!ret)
		warn("Buffer type PRIVATE allowed!\n");
		
	ret = testEnumFrameSizes(node, 0x20202020);
	if (ret >= 0)
		return fail("Accepted framesize for invalid format\n");
	ret = testEnumFrameIntervals(node, 0x20202020, 640, 480, false);
	if (ret >= 0)
		return fail("Accepted frameinterval for invalid format\n");
	return 0;
}

int testFBuf(struct node *node)
{
	struct v4l2_framebuffer fbuf;
	struct v4l2_pix_format &fmt = fbuf.fmt;
	__u32 caps;
	__u32 flags;
	int ret;

	memset(&fbuf, 0xff, sizeof(fbuf));
	fbuf.fmt.priv = 0;
	ret = doioctl(node, VIDIOC_G_FBUF, &fbuf);
	fail_on_test(ret == 0 && !(node->caps & (V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_VIDEO_OUTPUT_OVERLAY)));
	fail_on_test(ret == EINVAL && (node->caps & (V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_VIDEO_OUTPUT_OVERLAY)));
	if (ret == EINVAL)
		return -ENOSYS;
	if (ret)
		return fail("expected EINVAL, but got %d when getting framebuffer format\n", ret);
	node->fbuf_caps = caps = fbuf.capability;
	flags = fbuf.flags;
	if (node->caps & V4L2_CAP_VIDEO_OUTPUT_OVERLAY)
		fail_on_test(!fbuf.base);
	if (flags & V4L2_FBUF_FLAG_CHROMAKEY)
		fail_on_test(!(caps & V4L2_FBUF_CAP_CHROMAKEY));
	if (flags & V4L2_FBUF_FLAG_LOCAL_ALPHA)
		fail_on_test(!(caps & V4L2_FBUF_CAP_LOCAL_ALPHA));
	if (flags & V4L2_FBUF_FLAG_GLOBAL_ALPHA)
		fail_on_test(!(caps & V4L2_FBUF_CAP_GLOBAL_ALPHA));
	if (flags & V4L2_FBUF_FLAG_LOCAL_INV_ALPHA)
		fail_on_test(!(caps & V4L2_FBUF_CAP_LOCAL_INV_ALPHA));
	if (flags & V4L2_FBUF_FLAG_SRC_CHROMAKEY)
		fail_on_test(!(caps & V4L2_FBUF_CAP_SRC_CHROMAKEY));
	fail_on_test(!fmt.width || !fmt.height);
	if (fmt.priv)
		warn("fbuf.fmt.priv is non-zero\n");
	/* Not yet: unclear what EXTERNOVERLAY means in a output overlay context
	if (caps & V4L2_FBUF_CAP_EXTERNOVERLAY) {
		fail_on_test(fmt.bytesperline);
		fail_on_test(fmt.sizeimage);
		fail_on_test(fbuf.base);
	}*/
	fail_on_test(fmt.bytesperline && fmt.bytesperline < fmt.width);
	fail_on_test(fmt.sizeimage && fmt.sizeimage < fmt.bytesperline * fmt.height);
	fail_on_test(!fmt.colorspace);
	return 0;
}

static int testFormatsType(struct node *node, enum v4l2_buf_type type)
{
	pixfmt_set &set = node->buftype_pixfmts[type];
	pixfmt_set *set_splane;
	struct v4l2_format fmt;
	struct v4l2_pix_format &pix = fmt.fmt.pix;
	struct v4l2_pix_format_mplane &pix_mp = fmt.fmt.pix_mp;
	struct v4l2_window &win = fmt.fmt.win;
	struct v4l2_vbi_format &vbi = fmt.fmt.vbi;
	struct v4l2_sliced_vbi_format &sliced = fmt.fmt.sliced;
	__u32 service_set = 0;
	unsigned cnt = 0;
	int ret;
	
	memset(&fmt, 0xff, sizeof(fmt));
	fmt.type = type;
	ret = doioctl(node, VIDIOC_G_FMT, &fmt);
	if (ret == EINVAL)
		return -ENOSYS;
	if (ret)
		return fail("expected EINVAL, but got %d when getting format for buftype %d\n", ret, type);
	fail_on_test(fmt.type != type);
	
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		fail_on_test(!pix.width || !pix.height);
		if (set.find(pix.pixelformat) == set.end())
			return fail("unknown pixelformat %08x for buftype %d\n",
					pix.pixelformat, type);
		fail_on_test(pix.bytesperline && pix.bytesperline < pix.width);
		fail_on_test(!pix.sizeimage);
		fail_on_test(!pix.colorspace);
		if (pix.priv)
			warn("priv is non-zero!\n");
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fail_on_test(!pix_mp.width || !pix_mp.height);
		set_splane = &node->buftype_pixfmts[type - 8];
		if (set.find(pix_mp.pixelformat) == set.end() &&
		    set_splane->find(pix_mp.pixelformat) == set_splane->end())
			return fail("unknown pixelformat %08x for buftype %d\n",
					pix_mp.pixelformat, type);
		fail_on_test(!pix_mp.colorspace);
		ret = check_0(pix_mp.reserved, sizeof(pix_mp.reserved));
		if (ret)
			return fail("pix_mp.reserved not zeroed\n");
		fail_on_test(pix_mp.num_planes == 0 || pix_mp.num_planes >= VIDEO_MAX_PLANES);
		for (int i = 0; i < pix_mp.num_planes; i++) {
			struct v4l2_plane_pix_format &pfmt = pix_mp.plane_fmt[i];

			ret = check_0(pfmt.reserved, sizeof(pfmt.reserved));
			if (ret)
				return fail("pix_mp.plane_fmt[%d].reserved not zeroed\n", i);
			fail_on_test(!pfmt.sizeimage);
			fail_on_test(pfmt.bytesperline && pfmt.bytesperline < pix_mp.width);
		}
		break;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		fail_on_test(!vbi.sampling_rate);
		fail_on_test(!vbi.samples_per_line);
		fail_on_test(vbi.sample_format != V4L2_PIX_FMT_GREY);
		fail_on_test(vbi.offset > vbi.samples_per_line);
		ret = check_0(vbi.reserved, sizeof(vbi.reserved));
		if (ret)
			return fail("vbi.reserved not zeroed\n");
		fail_on_test(!vbi.count[0] || !vbi.count[1]);
		fail_on_test(vbi.flags & ~(V4L2_VBI_UNSYNC | V4L2_VBI_INTERLACED));
		if (vbi.flags & V4L2_VBI_INTERLACED)
			fail_on_test(vbi.count[0] != vbi.count[1]);
		break;
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		ret = check_0(sliced.reserved, sizeof(sliced.reserved));
		if (ret)
			return fail("sliced.reserved not zeroed\n");
		fail_on_test(sliced.service_lines[0][0] || sliced.service_lines[1][0]);
		for (int f = 0; f < 2; f++) {
			for (int i = 0; i < 24; i++) {
				if (sliced.service_lines[f][i])
					cnt++;
				service_set |= sliced.service_lines[f][i];
			}
		}
		fail_on_test(sliced.io_size < sizeof(struct v4l2_sliced_vbi_data) * cnt);
		fail_on_test(sliced.service_set != service_set);
		break;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		fail_on_test(win.field != V4L2_FIELD_ANY &&
			     win.field != V4L2_FIELD_TOP &&
			     win.field != V4L2_FIELD_BOTTOM &&
			     win.field != V4L2_FIELD_INTERLACED);
		fail_on_test(win.clipcount && !(node->fbuf_caps & V4L2_FBUF_CAP_LIST_CLIPPING));
		for (struct v4l2_clip *clip = win.clips; clip; win.clipcount--) {
			fail_on_test(clip == NULL);
			clip = clip->next;
		}
		fail_on_test(win.clipcount);
		fail_on_test(win.chromakey && !(node->fbuf_caps & (V4L2_FBUF_CAP_CHROMAKEY | V4L2_FBUF_CAP_SRC_CHROMAKEY)));
		if (!(node->fbuf_caps & V4L2_FBUF_CAP_BITMAP_CLIPPING))
			fail_on_test(win.bitmap);
		fail_on_test(win.global_alpha && !(node->fbuf_caps & V4L2_FBUF_CAP_GLOBAL_ALPHA));
		break;
	case V4L2_BUF_TYPE_PRIVATE:
		break;
	}
	return 0;
}

int testFormats(struct node *node)
{
	int type;
	int ret;

	for (type = 0; type <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; type++) {
		ret = testFormatsType(node, (enum v4l2_buf_type)type);

		if (ret > 0)
			return ret;
		if (ret && (node->caps & buftype2cap[type]))
			return fail("%s cap set, but no %s formats defined\n",
					buftype2s(type).c_str(), buftype2s(type).c_str());
		if (!ret && !(node->caps & buftype2cap[type]))
			return fail("%s cap not set, but %s formats defined\n",
					buftype2s(type).c_str(), buftype2s(type).c_str());
	}

	ret = testFormatsType(node, V4L2_BUF_TYPE_PRIVATE);
	if (ret > 0)
		return ret;
	if (!ret)
		warn("Buffer type PRIVATE allowed!\n");
	return 0;
}

static int testSlicedVBICapType(struct node *node, enum v4l2_buf_type type)
{
	struct v4l2_sliced_vbi_cap cap;
	bool sliced_type = (type == V4L2_BUF_TYPE_SLICED_VBI_CAPTURE ||
			    type == V4L2_BUF_TYPE_SLICED_VBI_OUTPUT);
	__u32 service_set = 0;
	int ret;

	memset(&cap, 0xff, sizeof(cap));
	memset(&cap.reserved, 0, sizeof(cap.reserved));
	cap.type = type;
	ret = doioctl(node, VIDIOC_G_SLICED_VBI_CAP, &cap);
	fail_on_test(check_0(cap.reserved, sizeof(cap.reserved)));
	fail_on_test(cap.type != type);
	fail_on_test(ret && ret != EINVAL && sliced_type);
	if (ret == EINVAL) {
		fail_on_test(sliced_type && (node->caps & buftype2cap[type]));
		if (node->caps & (V4L2_CAP_SLICED_VBI_CAPTURE | V4L2_CAP_SLICED_VBI_OUTPUT))
			return 0;
		return -ENOSYS;
	}
	if (ret)
		return fail("expected EINVAL, but got %d when getting sliced VBI caps buftype %d\n", ret, type);
	fail_on_test(!(node->caps & buftype2cap[type]));

	for (int f = 0; f < 2; f++)
		for (int i = 0; i < 24; i++)
			service_set |= cap.service_lines[f][i];
	fail_on_test(cap.service_set != service_set);
	fail_on_test(cap.service_lines[0][0] || cap.service_lines[1][0]);
	return 0;
}

int testSlicedVBICap(struct node *node)
{
	int ret;

	ret = testSlicedVBICapType(node, V4L2_BUF_TYPE_SLICED_VBI_CAPTURE);
	if (ret)
		return ret;
	ret = testSlicedVBICapType(node, V4L2_BUF_TYPE_SLICED_VBI_OUTPUT);
	if (ret)
		return ret;
	return testSlicedVBICapType(node, V4L2_BUF_TYPE_VIDEO_CAPTURE);
}
