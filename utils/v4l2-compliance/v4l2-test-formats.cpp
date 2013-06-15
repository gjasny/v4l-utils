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
	V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_M2M,
	V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_M2M,
	V4L2_CAP_VIDEO_OVERLAY,
	V4L2_CAP_VBI_CAPTURE,
	V4L2_CAP_VBI_OUTPUT,
	V4L2_CAP_SLICED_VBI_CAPTURE,
	V4L2_CAP_SLICED_VBI_OUTPUT,
	V4L2_CAP_VIDEO_OUTPUT_OVERLAY,
	V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE,
	V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE,
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
		if (ret == ENOTTY)
			return ret;
		if (f == 0 && ret == EINVAL) {
			if (valid)
				warn("found framesize %dx%d, but no frame intervals\n", w, h);
			return ENOTTY;
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
			if (ret)
				return fail("invalid frameinterval %d (%d/%d)\n", f,
						frmival.discrete.numerator,
						frmival.discrete.denominator);
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
		if (ret == ENOTTY)
			return ret;
		if (f == 0 && ret == EINVAL)
			return ENOTTY;
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
			if (ret && ret != ENOTTY)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					frmsize.discrete.width + 1, frmsize.discrete.height, false);
			if (ret && ret != ENOTTY)
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
			if (ret && ret != ENOTTY)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					sw->max_width, sw->max_height, true);
			if (ret && ret != ENOTTY)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					sw->min_width - 1, sw->min_height, false);
			if (ret && ret != ENOTTY)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					sw->max_width, sw->max_height + 1, false);
			if (ret && ret != ENOTTY)
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

static int testEnumFormatsType(struct node *node, unsigned type)
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
		if (ret == ENOTTY)
			return ret;
		if (f == 0 && ret == EINVAL)
			return ENOTTY;
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
		if (ret && ret != ENOTTY)
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
	bool supported = false;
	unsigned type;
	int ret;

	for (type = 0; type <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; type++) {
		ret = testEnumFormatsType(node, type);
		if (ret && ret != ENOTTY)
			return ret;
		if (!ret)
			supported = true;
		switch (type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
			if (ret && (node->caps & buftype2cap[type]))
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
	if (ret != ENOTTY && ret != EINVAL)
		return fail("Buffer type PRIVATE allowed!\n");

	ret = testEnumFrameSizes(node, 0x20202020);
	if (ret != ENOTTY)
		return fail("Accepted framesize for invalid format\n");
	ret = testEnumFrameIntervals(node, 0x20202020, 640, 480, false);
	if (ret != ENOTTY)
		return fail("Accepted frameinterval for invalid format\n");
	return supported ? 0 : ENOTTY;
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
	fail_on_test(ret == ENOTTY && (node->caps & (V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_VIDEO_OUTPUT_OVERLAY)));
	if (ret == ENOTTY)
		return ret;
	if (ret && ret != EINVAL)
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

static void createInvalidFmt(struct v4l2_format &fmt, struct v4l2_clip &clip, unsigned type)
{
	memset(&fmt, 0xff, sizeof(fmt));
	fmt.type = type;
	fmt.fmt.pix.field = V4L2_FIELD_ANY;
	if (type == V4L2_BUF_TYPE_VIDEO_OVERLAY ||
	    type == V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY) {
		memset(&clip, 0xff, sizeof(clip));
		clip.next = NULL;
		fmt.fmt.win.clipcount = 1;
		fmt.fmt.win.clips = &clip;
		fmt.fmt.win.bitmap = NULL;
	}
}

static int testFormatsType(struct node *node, int ret,  unsigned type, struct v4l2_format &fmt)
{
	pixfmt_set &set = node->buftype_pixfmts[type];
	pixfmt_set *set_splane;
	struct v4l2_pix_format &pix = fmt.fmt.pix;
	struct v4l2_pix_format_mplane &pix_mp = fmt.fmt.pix_mp;
	struct v4l2_window &win = fmt.fmt.win;
	struct v4l2_vbi_format &vbi = fmt.fmt.vbi;
	struct v4l2_sliced_vbi_format &sliced = fmt.fmt.sliced;
	unsigned min_data_samples;
	unsigned min_sampling_rate;
	v4l2_std_id std;
	__u32 service_set = 0;
	unsigned cnt = 0;

	if (ret == ENOTTY)
		return ret;
	if (ret == EINVAL)
		return ENOTTY;
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
		fail_on_test(pix.field == V4L2_FIELD_ANY);
		if (pix.priv)
			return fail("priv is non-zero!\n");
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
		fail_on_test(pix.field == V4L2_FIELD_ANY);
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
		// Currently VBI assumes that you have G_STD as well.
		fail_on_test(doioctl(node, VIDIOC_G_STD, &std));
		if (std & V4L2_STD_625_50) {
			min_sampling_rate = 6937500;
			// the number of databits for PAL teletext is 18 (clock run in) +
			// 6 (framing code) + 42 * 8 (data).
			min_data_samples = (vbi.sampling_rate * (18 + 6 + 42 * 8)) / min_sampling_rate;
		} else {
			min_sampling_rate = 5727272;
			// the number of databits for NTSC teletext is 18 (clock run in) +
			// 6 (framing code) + 34 * 8 (data).
			min_data_samples = (vbi.sampling_rate * (18 + 6 + 34 * 8)) / min_sampling_rate;
		}
		fail_on_test(vbi.sampling_rate < min_sampling_rate);
		fail_on_test(!vbi.samples_per_line);
		fail_on_test(vbi.sample_format != V4L2_PIX_FMT_GREY);
		fail_on_test(vbi.offset > vbi.samples_per_line);
		ret = check_0(vbi.reserved, sizeof(vbi.reserved));
		if (ret)
			return fail("vbi.reserved not zeroed\n");
		// Check that offset leaves enough room for the maximum required
		// amount of data.
		fail_on_test(min_data_samples > vbi.samples_per_line - vbi.offset);
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
		fail_on_test(win.field == V4L2_FIELD_ANY);
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

int testGetFormats(struct node *node)
{
	struct v4l2_clip clip;
	struct v4l2_format fmt;
	bool supported = false;
	int type;
	int ret;

	for (type = 0; type <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; type++) {
		createInvalidFmt(fmt, clip, type);
		ret = doioctl(node, VIDIOC_G_FMT, &fmt);
		ret = testFormatsType(node, ret, type, fmt);

		if (ret && ret != ENOTTY)
			return ret;
		if (!ret) {
			supported = true;
			node->valid_buftypes |= 1 << type;
		}
		if (ret && (node->caps & buftype2cap[type]))
			return fail("%s cap set, but no %s formats defined\n",
					buftype2s(type).c_str(), buftype2s(type).c_str());
		if (!ret && !(node->caps & buftype2cap[type])) {
			switch (type) {
			case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			case V4L2_BUF_TYPE_VIDEO_OUTPUT:
			case V4L2_BUF_TYPE_VIDEO_OVERLAY:
			case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
			case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
				return fail("%s cap not set, but %s formats defined\n",
					buftype2s(type).c_str(), buftype2s(type).c_str());
			default:
				/* ENUMFMT doesn't support other buftypes */
				break;
			}
		}
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_PRIVATE;
	ret = doioctl(node, VIDIOC_G_FMT, &fmt);
	if (ret != ENOTTY && ret != EINVAL)
		return fail("Buffer type PRIVATE allowed!\n");
	return supported ? 0 : ENOTTY;
}

int testTryFormats(struct node *node)
{
	struct v4l2_clip clip;
	struct v4l2_format fmt, fmt_try;
	int type;
	int ret;
	
	for (type = 0; type <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;

		createInvalidFmt(fmt, clip, type);
		doioctl(node, VIDIOC_G_FMT, &fmt);
		fmt_try = fmt;
		ret = doioctl(node, VIDIOC_TRY_FMT, &fmt_try);
		if (ret)
			return fail("%s is valid, but no TRY_FMT was implemented\n",
					buftype2s(type).c_str());
		ret = testFormatsType(node, ret, type, fmt_try);
		if (ret)
			return ret;
		if (memcmp(&fmt, &fmt_try, sizeof(fmt)))
			return fail("%s: TRY_FMT(G_FMT) != G_FMT\n",
					buftype2s(type).c_str());
	}

	for (type = 0; type <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;

		createInvalidFmt(fmt, clip, type);
		ret = doioctl(node, VIDIOC_TRY_FMT, &fmt);
		if (ret == EINVAL) {
			__u32 pixelformat;
			bool is_mplane = false;

			/* In case of failure obtain a valid pixelformat and insert
			 * that in the next attempt to call TRY_FMT. */
			doioctl(node, VIDIOC_G_FMT, &fmt);

			switch (type) {
			case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			case V4L2_BUF_TYPE_VIDEO_OUTPUT:
				pixelformat = fmt.fmt.pix.pixelformat;
				break;
			case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
			case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
				pixelformat = fmt.fmt.pix_mp.pixelformat;
				is_mplane = true;
				break;
			default:
				/* for other formats returning EINVAL is certainly wrong */
				return fail("TRY_FMT cannot handle an invalid format\n");
			}
			warn("TRY_FMT cannot handle an invalid pixelformat.\n");
			warn("This may or may not be a problem. For more information see:\n");
			warn("http://www.mail-archive.com/linux-media@vger.kernel.org/msg56550.html\n");

			/* Now try again, but pass a valid pixelformat. */
			createInvalidFmt(fmt, clip, type);
			if (is_mplane)
				fmt.fmt.pix_mp.pixelformat = pixelformat;
			else
				fmt.fmt.pix.pixelformat = pixelformat;
			ret = doioctl(node, VIDIOC_TRY_FMT, &fmt);
			if (ret == EINVAL)
				return fail("TRY_FMT cannot handle an invalid format\n");
		}
		ret = testFormatsType(node, ret, type, fmt);
		if (ret)
			return fail("%s is valid, but TRY_FMT failed to return a format\n",
					buftype2s(type).c_str());
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_PRIVATE;
	ret = doioctl(node, VIDIOC_TRY_FMT, &fmt);
	if (ret != ENOTTY && ret != EINVAL)
		return fail("Buffer type PRIVATE allowed!\n");
	return node->valid_buftypes ? 0 : ENOTTY;
}

static int testGlobalFormat(struct node *node, int type)
{
	struct v4l2_fmtdesc fdesc;
	struct v4l2_frmsizeenum fsize;
	struct v4l2_format fmt1, fmt2;
	struct v4l2_pix_format *p1 = &fmt1.fmt.pix;
	struct v4l2_pix_format *p2 = &fmt2.fmt.pix;
	struct v4l2_pix_format_mplane *mp1 = &fmt1.fmt.pix_mp;
	struct v4l2_pix_format_mplane *mp2 = &fmt2.fmt.pix_mp;
	__u32 pixfmt1, pixfmt2;
	__u32 w1 = 0, w2 = 0, h1 = 0, h2 = 0;

	memset(&fmt1, 0, sizeof(fmt1));
	memset(&fmt2, 0, sizeof(fmt2));
	fmt1.type = fmt2.type = type;
	fdesc.index = 1;
	fdesc.type = type;
	memset(&fsize, 0, sizeof(fsize));

	if (!doioctl(node, VIDIOC_ENUM_FMT, &fdesc)) {
		// We found at least two different formats.
		pixfmt2 = fdesc.pixelformat;
		fdesc.index = 0;
		doioctl(node, VIDIOC_ENUM_FMT, &fdesc);
		pixfmt1 = fdesc.pixelformat;
	} else {
		fdesc.index = 0;
		doioctl(node, VIDIOC_ENUM_FMT, &fdesc);
		fsize.pixel_format = fdesc.pixelformat;
		if (doioctl(node, VIDIOC_ENUM_FRAMESIZES, &fsize))
			return 0;
		pixfmt1 = pixfmt2 = fdesc.pixelformat;
		switch (fsize.type) {
		case V4L2_FRMSIZE_TYPE_CONTINUOUS:
		case V4L2_FRMSIZE_TYPE_STEPWISE:
			w1 = fsize.stepwise.min_width;
			w2 = fsize.stepwise.max_width;
			h1 = fsize.stepwise.min_height;
			h2 = fsize.stepwise.max_height;
			break;
		case V4L2_FRMSIZE_TYPE_DISCRETE:
			w1 = fsize.discrete.width;
			h1 = fsize.discrete.height;
			fsize.index = 1;
			doioctl(node, VIDIOC_ENUM_FRAMESIZES, &fsize);
			w2 = fsize.discrete.width;
			h2 = fsize.discrete.height;
			break;
		}
	}
	// Check if we have found different formats, otherwise this
	// test is pointless.
	if (pixfmt1 == pixfmt2 && w1 == w2 && h1 == h2)
		return 0;

	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
	    type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		mp1->pixelformat = pixfmt1;
		mp1->width = w1;
		mp1->height = h1;
		mp2->pixelformat = pixfmt2;
		mp2->width = w2;
		mp2->height = h2;
	} else {
		p1->pixelformat = pixfmt1;
		p1->width = w1;
		p1->height = h1;
		p2->pixelformat = pixfmt2;
		p2->width = w2;
		p2->height = h2;
	}
	if (doioctl(node, VIDIOC_S_FMT, &fmt1)) {
		warn("Could not set fmt1\n");
		return 0;
	}
	if (doioctl(node->node2, VIDIOC_S_FMT, &fmt2)) {
		warn("Could not set fmt2\n");
		return 0;
	}
	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
	    type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (mp1->pixelformat == mp2->pixelformat &&
		    mp1->width == mp2->width && mp1->height == mp2->height) {
			// This compliance test only succeeds if the two formats
			// are really different after S_FMT
			warn("Could not perform global format test\n");
			return 0;
		}
	} else {
		if (p1->pixelformat == p2->pixelformat &&
		    p1->width == p2->width && p1->height == p2->height) {
			// This compliance test only succeeds if the two formats
			// are really different after S_FMT
			warn("Could not perform global format test\n");
			return 0;
		}
	}
	doioctl(node, VIDIOC_G_FMT, &fmt1);
	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
	    type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		pixfmt1 = mp1->pixelformat;
		w1 = mp1->width;
		h1 = mp1->height;
		pixfmt2 = mp2->pixelformat;
		w2 = mp2->width;
		h2 = mp2->height;
	} else {
		pixfmt1 = p1->pixelformat;
		w1 = p1->width;
		h1 = p1->height;
		pixfmt2 = p2->pixelformat;
		w2 = p2->width;
		h2 = p2->height;
	}
	if (pixfmt1 != pixfmt2 || w1 != w2 || h1 != h2)
		return fail("Global format mismatch: %08x/%dx%d vs %08x/%dx%d\n",
				pixfmt1, w1, h1, pixfmt2, w2, h2);
	info("Global format check succeeded for type %d\n", type);
	return 0;
}

int testSetFormats(struct node *node)
{
	struct v4l2_clip clip, clip_set;
	struct v4l2_format fmt, fmt_set;
	int type;
	int ret;
	
	for (type = 0; type <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;

		createInvalidFmt(fmt, clip, type);
		doioctl(node, VIDIOC_G_FMT, &fmt);
		
		createInvalidFmt(fmt_set, clip_set, type);
		ret = doioctl(node, VIDIOC_S_FMT, &fmt_set);
		if (ret == EINVAL) {
			__u32 pixelformat;
			bool is_mplane = false;

			/* In case of failure obtain a valid pixelformat and insert
			 * that in the next attempt to call TRY_FMT. */
			doioctl(node, VIDIOC_G_FMT, &fmt_set);

			switch (type) {
			case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			case V4L2_BUF_TYPE_VIDEO_OUTPUT:
				pixelformat = fmt_set.fmt.pix.pixelformat;
				break;
			case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
			case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
				pixelformat = fmt_set.fmt.pix_mp.pixelformat;
				is_mplane = true;
				break;
			default:
				/* for other formats returning EINVAL is certainly wrong */
				return fail("TRY_FMT cannot handle an invalid format\n");
			}
			warn("S_FMT cannot handle an invalid pixelformat.\n");
			warn("This may or may not be a problem. For more information see:\n");
			warn("http://www.mail-archive.com/linux-media@vger.kernel.org/msg56550.html\n");

			/* Now try again, but pass a valid pixelformat. */
			createInvalidFmt(fmt_set, clip_set, type);
			if (is_mplane)
				fmt_set.fmt.pix_mp.pixelformat = pixelformat;
			else
				fmt_set.fmt.pix.pixelformat = pixelformat;
			ret = doioctl(node, VIDIOC_S_FMT, &fmt_set);
			if (ret == EINVAL)
				return fail("S_FMT cannot handle an invalid format\n");
		}
		ret = testFormatsType(node, ret, type, fmt_set);
		if (ret)
			return fail("%s is valid, but no S_FMT was implemented\n",
					buftype2s(type).c_str());

		fmt_set = fmt;
		ret = doioctl(node, VIDIOC_S_FMT, &fmt_set);
		ret = testFormatsType(node, ret, type, fmt_set);
		if (ret)
			return ret;
		if (memcmp(&fmt, &fmt_set, sizeof(fmt)))
			return fail("%s: S_FMT(G_FMT) != G_FMT\n",
					buftype2s(type).c_str());
	}
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_PRIVATE;
	ret = doioctl(node, VIDIOC_S_FMT, &fmt);
	if (ret != ENOTTY && ret != EINVAL)
		return fail("Buffer type PRIVATE allowed!\n");
	if (!node->valid_buftypes)
		return ENOTTY;

	// Test if setting a format on one fh will set the format for all
	// filehandles.
	if (node->node2 == NULL)
		return 0;
	// m2m devices are unique in that the format is often per-filehandle.
	if (node->is_m2m)
		return 0;

	for (type = 0; type <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; type++) {
		switch (type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
			if (!(node->valid_buftypes & (1 << type)))
				continue;

			ret = testGlobalFormat(node, type);
			if (ret)
				return ret;
			break;

		default:
			break;
		}
	}

	return 0;
}

static int testSlicedVBICapType(struct node *node, unsigned type)
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
	if (ret == ENOTTY || ret == EINVAL) {
		fail_on_test(sliced_type && (node->caps & buftype2cap[type]));
		return ret == ENOTTY ? ret : 0;
	}
	fail_on_test(ret);
	fail_on_test(check_0(cap.reserved, sizeof(cap.reserved)));
	fail_on_test(cap.type != type);
	fail_on_test(!sliced_type || !(node->caps & buftype2cap[type]));

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

static int testParmStruct(struct node *node, struct v4l2_streamparm &parm)
{
	struct v4l2_captureparm *cap = &parm.parm.capture;
	struct v4l2_outputparm *out = &parm.parm.output;
	int ret;

	switch (parm.type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		ret = check_0(cap->reserved, sizeof(cap->reserved));
		if (ret)
			return fail("reserved not zeroed\n");
		fail_on_test(cap->readbuffers > VIDEO_MAX_FRAME);
		if (!(node->caps & V4L2_CAP_READWRITE))
			fail_on_test(cap->readbuffers);
		else if (node->caps & V4L2_CAP_STREAMING)
			fail_on_test(!cap->readbuffers);
		fail_on_test(cap->capability & ~V4L2_CAP_TIMEPERFRAME);
		fail_on_test(cap->capturemode & ~V4L2_MODE_HIGHQUALITY);
		if (cap->capturemode & V4L2_MODE_HIGHQUALITY)
			warn("V4L2_MODE_HIGHQUALITY is poorly defined\n");
		fail_on_test(cap->extendedmode);
		if (cap->capability & V4L2_CAP_TIMEPERFRAME)
			fail_on_test(cap->timeperframe.numerator == 0 || cap->timeperframe.denominator == 0);
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		ret = check_0(out->reserved, sizeof(out->reserved));
		if (ret)
			return fail("reserved not zeroed\n");
		fail_on_test(out->writebuffers > VIDEO_MAX_FRAME);
		if (!(node->caps & V4L2_CAP_READWRITE))
			fail_on_test(out->writebuffers);
		else if (node->caps & V4L2_CAP_STREAMING)
			fail_on_test(!out->writebuffers);
		fail_on_test(out->capability & ~V4L2_CAP_TIMEPERFRAME);
		fail_on_test(out->outputmode);
		fail_on_test(out->extendedmode);
		if (out->capability & V4L2_CAP_TIMEPERFRAME)
			fail_on_test(out->timeperframe.numerator == 0 || out->timeperframe.denominator == 0);
		break;
	default:
		break;
	}
	return 0;
}

static int testParmType(struct node *node, unsigned type)
{
	struct v4l2_streamparm parm;
	int ret;

	memset(&parm, 0, sizeof(parm));
	parm.type = type;
	ret = doioctl(node, VIDIOC_G_PARM, &parm);
	if (ret == ENOTTY)
		return ret;
	if (ret == EINVAL)
		return ENOTTY;
	if (ret)
		return fail("expected EINVAL, but got %d when getting parms for buftype %d\n", ret, type);
	fail_on_test(parm.type != type);
	ret = testParmStruct(node, parm);
	if (ret)
		return ret;

	memset(&parm, 0, sizeof(parm));
	parm.type = type;
	ret = doioctl(node, VIDIOC_S_PARM, &parm);
	if (ret == ENOTTY)
		return 0;
	if (ret)
		return fail("got error %d when setting parms for buftype %d\n", ret, type);
	fail_on_test(parm.type != type);
	return testParmStruct(node, parm);
}

int testParm(struct node *node)
{
	bool supported = false;
	int type;
	int ret;

	for (type = 0; type <= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; type++) {
		ret = testParmType(node, type);

		if (ret && ret != ENOTTY)
			return ret;
		if (!ret) {
			supported = true;
			if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE &&
			    type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
			    type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
			    type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
				return fail("G/S_PARM is only allowed for video capture/output\n");
			if (!(node->caps & buftype2cap[type]))
				return fail("%s cap not set, but G/S_PARM worked\n",
						buftype2s(type).c_str());
		}
	}

	ret = testParmType(node, V4L2_BUF_TYPE_PRIVATE);
	if (ret != ENOTTY && ret != EINVAL)
		return fail("Buffer type PRIVATE allowed!\n");
	return supported ? 0 : ENOTTY;
}
