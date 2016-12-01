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
	V4L2_CAP_SDR_CAPTURE,
	V4L2_CAP_SDR_OUTPUT,
};

static int testEnumFrameIntervals(struct node *node, __u32 pixfmt,
				  __u32 w, __u32 h, __u32 type)
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
			if (type == V4L2_FRMSIZE_TYPE_DISCRETE)
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
			if (frmival.type == V4L2_FRMIVAL_TYPE_STEPWISE &&
			    fract2f(&sw->step) > fract2f(&sw->max) - fract2f(&sw->min))
				return fail("step > (max - min)\n");
			break;
		default:
			return fail("frmival.type is invalid\n");
		}

		f++;
		node->has_frmintervals = true;
	}
	if (type == 0)
		return fail("found frame intervals for invalid size %dx%d\n", w, h);
	info("found %d frameintervals for pixel format %08x (%s) and size %dx%d\n",
	     f, pixfmt, fcc2s(pixfmt).c_str(), w, h);
	return 0;
}

static int testEnumFrameSizes(struct node *node, __u32 pixfmt)
{
	struct v4l2_frmsizeenum frmsize;
	struct v4l2_frmsize_stepwise *sw = &frmsize.stepwise;
	bool found_stepwise = false;
	__u64 cookie;
	unsigned f = 0;
	unsigned count = 0;
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
					frmsize.discrete.width, frmsize.discrete.height, frmsize.type);
			if (ret && ret != ENOTTY)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					frmsize.discrete.width + 1, frmsize.discrete.height, 0);
			if (ret && ret != ENOTTY)
				return ret;
			if (ret == 0 && !(node->g_caps() & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE)))
				return fail("found discrete framesizes when no video capture is supported\n");
			cookie = ((__u64)pixfmt << 32) |
				 (frmsize.discrete.width << 16) |
				 frmsize.discrete.height;
			node->frmsizes.insert(cookie);
			count++;
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
					sw->min_width, sw->min_height, frmsize.type);
			if (ret && ret != ENOTTY)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					sw->max_width, sw->max_height, frmsize.type);
			if (ret && ret != ENOTTY)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					sw->min_width - 1, sw->min_height, 0);
			if (ret && ret != ENOTTY)
				return ret;
			ret = testEnumFrameIntervals(node, pixfmt,
					sw->max_width, sw->max_height + 1, 0);
			if (ret && ret != ENOTTY)
				return ret;
			break;
		default:
			return fail("frmsize.type is invalid\n");
		}

		f++;
	}
	node->frmsizes_count[pixfmt] = count;
	info("found %d framesizes for pixel format %08x (%s)\n",
	     f, pixfmt, fcc2s(pixfmt).c_str());
	return 0;
}

static int testEnumFormatsType(struct node *node, unsigned type)
{
	pixfmt_map &map = node->buftype_pixfmts[type];
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
		if (node->g_direct() && (fmtdesc.flags & V4L2_FMT_FLAG_EMULATED))
			return fail("drivers must never set the emulated flag\n");
		if (fmtdesc.flags & ~(V4L2_FMT_FLAG_COMPRESSED | V4L2_FMT_FLAG_EMULATED))
			return fail("unknown flag %08x returned\n", fmtdesc.flags);
		ret = testEnumFrameSizes(node, fmtdesc.pixelformat);
		if (ret && ret != ENOTTY)
			return ret;
		f++;
		if (type == V4L2_BUF_TYPE_PRIVATE)
			continue;
		// Update define in v4l2-compliance.h if new buffer types are added
		assert(type <= V4L2_BUF_TYPE_LAST);
		if (map.find(fmtdesc.pixelformat) != map.end())
			return fail("duplicate format %08x (%s)\n",
				    fmtdesc.pixelformat, fcc2s(fmtdesc.pixelformat).c_str());
		map[fmtdesc.pixelformat] = fmtdesc.flags;
	}
	info("found %d formats for buftype %d\n", f, type);
	return 0;
}

int testEnumFormats(struct node *node)
{
	bool supported = false;
	unsigned type;
	int ret;

	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
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
		case V4L2_BUF_TYPE_SDR_CAPTURE:
		case V4L2_BUF_TYPE_SDR_OUTPUT:
			if (ret && (node->g_caps() & buftype2cap[type]))
				return fail("%s cap set, but no %s formats defined\n",
						buftype2s(type).c_str(), buftype2s(type).c_str());
			if (!ret && !(node->g_caps() & buftype2cap[type]))
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
	ret = testEnumFrameIntervals(node, 0x20202020, 640, 480, 0);
	if (ret != ENOTTY)
		return fail("Accepted frameinterval for invalid format\n");
	return supported ? 0 : ENOTTY;
}

static int testColorspace(__u32 pixelformat, __u32 colorspace, __u32 ycbcr_enc, __u32 quantization)
{
	fail_on_test(!colorspace);
	fail_on_test(colorspace == V4L2_COLORSPACE_BT878);
	fail_on_test(pixelformat == V4L2_PIX_FMT_JPEG &&
		     colorspace != V4L2_COLORSPACE_JPEG);
	fail_on_test(pixelformat != V4L2_PIX_FMT_JPEG &&
		     colorspace == V4L2_COLORSPACE_JPEG);
	fail_on_test(colorspace >= 0xff);
	fail_on_test(ycbcr_enc >= 0xff);
	fail_on_test(quantization >= 0xff);
	return 0;
}

int testFBuf(struct node *node)
{
	struct v4l2_framebuffer fbuf;
	__u32 caps;
	__u32 flags;
	int ret;

	memset(&fbuf, 0xff, sizeof(fbuf));
	fbuf.fmt.priv = 0;
	ret = doioctl(node, VIDIOC_G_FBUF, &fbuf);
	fail_on_test(ret == 0 && !(node->g_caps() & (V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_VIDEO_OUTPUT_OVERLAY)));
	fail_on_test(ret == ENOTTY && (node->g_caps() & (V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_VIDEO_OUTPUT_OVERLAY)));
	if (ret == ENOTTY)
		return ret;
	if (ret && ret != EINVAL)
		return fail("expected EINVAL, but got %d when getting framebuffer format\n", ret);
	node->fbuf_caps = caps = fbuf.capability;
	flags = fbuf.flags;
	if (node->g_caps() & V4L2_CAP_VIDEO_OUTPUT_OVERLAY)
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
	fail_on_test(!fbuf.fmt.width || !fbuf.fmt.height);
	if (fbuf.fmt.priv)
		warn("fbuf.fmt.priv is non-zero\n");
	/* Not yet: unclear what EXTERNOVERLAY means in a output overlay context
	if (caps & V4L2_FBUF_CAP_EXTERNOVERLAY) {
		fail_on_test(fbuf.fmt.bytesperline);
		fail_on_test(fbuf.fmt.sizeimage);
		fail_on_test(fbuf.base);
	}*/
	fail_on_test(fbuf.fmt.bytesperline && fbuf.fmt.bytesperline < fbuf.fmt.width);
	fail_on_test(fbuf.fmt.sizeimage && fbuf.fmt.sizeimage < fbuf.fmt.bytesperline * fbuf.fmt.height);
	fail_on_test(testColorspace(fbuf.fmt.pixelformat, fbuf.fmt.colorspace, 0, 0));
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
		clip.next = (struct  v4l2_clip *)0xdeadbeef;
		fmt.fmt.win.clipcount = 1;
		fmt.fmt.win.clips = &clip;
		fmt.fmt.win.bitmap = NULL;
	}
}

static int testFormatsType(struct node *node, int ret,  unsigned type, struct v4l2_format &fmt, bool have_clip = false)
{
	pixfmt_map &map = node->buftype_pixfmts[type];
	pixfmt_map *map_splane;
	struct v4l2_pix_format &pix = fmt.fmt.pix;
	struct v4l2_pix_format_mplane &pix_mp = fmt.fmt.pix_mp;
	struct v4l2_window &win = fmt.fmt.win;
	struct v4l2_vbi_format &vbi = fmt.fmt.vbi;
	struct v4l2_sliced_vbi_format &sliced = fmt.fmt.sliced;
	struct v4l2_sdr_format &sdr = fmt.fmt.sdr;
	unsigned min_data_samples;
	unsigned min_sampling_rate;
	v4l2_std_id std;
	__u32 service_set = 0;
	unsigned tot_bytesperline = 0;
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
		if (map.find(pix.pixelformat) == map.end())
			return fail("pixelformat %08x (%s) for buftype %d not reported by ENUM_FMT\n",
					pix.pixelformat, fcc2s(pix.pixelformat).c_str(), type);
		fail_on_test(pix.bytesperline && pix.bytesperline < pix.width);
		fail_on_test(!pix.sizeimage);
		if (!node->is_m2m)
			fail_on_test(testColorspace(pix.pixelformat, pix.colorspace,
					    pix.ycbcr_enc, pix.quantization));
		fail_on_test(pix.field == V4L2_FIELD_ANY);
		if (pix.priv && pix.priv != V4L2_PIX_FMT_PRIV_MAGIC)
			return fail("priv is non-zero and non-magic!\n");
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		fail_on_test(!pix_mp.width || !pix_mp.height);
		map_splane = &node->buftype_pixfmts[type - 8];
		if (map.find(pix_mp.pixelformat) == map.end() &&
		    map_splane->find(pix_mp.pixelformat) == map_splane->end())
			return fail("pixelformat %08x (%s) for buftype %d not reported by ENUM_FMT\n",
					pix_mp.pixelformat, fcc2s(pix_mp.pixelformat).c_str(), type);
		if (!node->is_m2m)
			fail_on_test(testColorspace(pix_mp.pixelformat, pix_mp.colorspace, 
                                            pix_mp.ycbcr_enc, pix_mp.quantization));
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
			tot_bytesperline += pfmt.bytesperline;
		}
		fail_on_test(tot_bytesperline && tot_bytesperline < pix_mp.width);
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
		fail_on_test(win.clipcount && !(node->fbuf_caps & V4L2_FBUF_CAP_LIST_CLIPPING));
		if (have_clip)
			fail_on_test(!win.clipcount && (node->fbuf_caps & V4L2_FBUF_CAP_LIST_CLIPPING));
		if (win.clipcount) {
			struct v4l2_rect *r = &win.clips->c;
			struct v4l2_framebuffer fb;

			fail_on_test(doioctl(node, VIDIOC_G_FBUF, &fb));
			fail_on_test(!win.clips);
			fail_on_test(win.clips->next != (void *)0xdeadbeef);
			fail_on_test(win.clipcount != 1);
			fail_on_test(r->left < 0 || r->top < 0);
			fail_on_test((unsigned)r->left >= fb.fmt.width || (unsigned)r->top >= fb.fmt.height);
			fail_on_test(r->width == 0 || r->height == 0);
			fail_on_test(r->left + r->width > fb.fmt.width || r->top + r->height > fb.fmt.height);
		}
		fail_on_test(win.chromakey && !(node->fbuf_caps & (V4L2_FBUF_CAP_CHROMAKEY | V4L2_FBUF_CAP_SRC_CHROMAKEY)));
		if (!(node->fbuf_caps & V4L2_FBUF_CAP_BITMAP_CLIPPING))
			fail_on_test(win.bitmap);
		fail_on_test(win.global_alpha && !(node->fbuf_caps & V4L2_FBUF_CAP_GLOBAL_ALPHA));
		break;
	case V4L2_BUF_TYPE_SDR_CAPTURE:
	case V4L2_BUF_TYPE_SDR_OUTPUT:
		if (map.find(sdr.pixelformat) == map.end())
			return fail("pixelformat %08x (%s) for buftype %d not reported by ENUM_FMT\n",
					pix.pixelformat, fcc2s(pix.pixelformat).c_str(), type);
		fail_on_test(sdr.buffersize == 0);
		fail_on_test(check_0(sdr.reserved, sizeof(sdr.reserved)));
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

	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
		createInvalidFmt(fmt, clip, type);
		ret = doioctl(node, VIDIOC_G_FMT, &fmt);
		ret = testFormatsType(node, ret, type, fmt);

		if (ret && ret != ENOTTY)
			return ret;
		if (!ret) {
			supported = true;
			node->valid_buftypes |= 1 << type;
		}
		switch (type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		case V4L2_BUF_TYPE_SDR_CAPTURE:
		case V4L2_BUF_TYPE_SDR_OUTPUT:
			if (ret && (node->g_caps() & buftype2cap[type]))
				return fail("%s cap set, but no %s formats defined\n",
					buftype2s(type).c_str(), buftype2s(type).c_str());
			if (!ret && !(node->g_caps() & buftype2cap[type]))
				return fail("%s cap not set, but %s formats defined\n",
					buftype2s(type).c_str(), buftype2s(type).c_str());
			break;
		default:
			/* ENUMFMT doesn't support other buftypes */
			break;
		}
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_PRIVATE;
	ret = doioctl(node, VIDIOC_G_FMT, &fmt);
	if (ret != ENOTTY && ret != EINVAL)
		return fail("Buffer type PRIVATE allowed!\n");
	return supported ? 0 : ENOTTY;
}

static bool matchFormats(const struct v4l2_format &f1, const struct v4l2_format &f2)
{
	const struct v4l2_pix_format &pix1 = f1.fmt.pix;
	const struct v4l2_pix_format &pix2 = f2.fmt.pix;

	if (f1.type != f2.type)
		return false;
	switch (f1.type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		if (!memcmp(&f1.fmt.pix, &f2.fmt.pix, sizeof(f1.fmt.pix)))
			return true;
		printf("\t\tG_FMT:     %dx%d, %x, %d, %d, %d, %d, %d, %d, %x\n",
			pix1.width, pix1.height, pix1.pixelformat, pix1.field, pix1.bytesperline,
			pix1.sizeimage, pix1.colorspace, pix1.ycbcr_enc, pix1.quantization, pix1.priv);
		printf("\t\tTRY/S_FMT: %dx%d, %x, %d, %d, %d, %d, %d, %d, %x\n",
			pix2.width, pix2.height, pix2.pixelformat, pix2.field, pix2.bytesperline,
			pix2.sizeimage, pix2.colorspace, pix2.ycbcr_enc, pix2.quantization, pix2.priv);
		return false;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		return !memcmp(&f1.fmt.win, &f2.fmt.win, sizeof(f1.fmt.win));
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		return !memcmp(&f1.fmt.vbi, &f2.fmt.vbi, sizeof(f1.fmt.vbi));
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		return !memcmp(&f1.fmt.sliced, &f2.fmt.sliced, sizeof(f1.fmt.sliced));
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return !memcmp(&f1.fmt.pix_mp, &f2.fmt.pix_mp, sizeof(f1.fmt.pix_mp));
	case V4L2_BUF_TYPE_SDR_CAPTURE:
	case V4L2_BUF_TYPE_SDR_OUTPUT:
		return !memcmp(&f1.fmt.sdr, &f2.fmt.sdr, sizeof(f1.fmt.sdr));

	}
	return false;
}

int testTryFormats(struct node *node)
{
	struct v4l2_clip clip;
	struct v4l2_format fmt, fmt_try;
	int result = 0;
	int type;
	int ret;
	
	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;

		switch (type) {
		case V4L2_BUF_TYPE_VBI_CAPTURE:
		case V4L2_BUF_TYPE_VBI_OUTPUT:
		case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
		case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
			if (!(node->cur_io_caps & V4L2_IN_CAP_STD))
				continue;
			break;
		}

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
		if (!matchFormats(fmt, fmt_try))
			result = fail("%s: TRY_FMT(G_FMT) != G_FMT\n",
					buftype2s(type).c_str());
	}

	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;

		switch (type) {
		case V4L2_BUF_TYPE_VBI_CAPTURE:
		case V4L2_BUF_TYPE_VBI_OUTPUT:
		case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
		case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
			if (!(node->cur_io_caps & V4L2_IN_CAP_STD))
				continue;
			break;
		}

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
			case V4L2_BUF_TYPE_SDR_CAPTURE:
			case V4L2_BUF_TYPE_SDR_OUTPUT:
				pixelformat = fmt.fmt.sdr.pixelformat;
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
			warn_once("TRY_FMT cannot handle an invalid pixelformat.\n");
			warn_once("This may or may not be a problem. For more information see:\n");
			warn_once("http://www.mail-archive.com/linux-media@vger.kernel.org/msg56550.html\n");

			/* Now try again, but pass a valid pixelformat. */
			createInvalidFmt(fmt, clip, type);
			if (node->is_sdr)
				fmt.fmt.sdr.pixelformat = pixelformat;
			else if (is_mplane)
				fmt.fmt.pix_mp.pixelformat = pixelformat;
			else
				fmt.fmt.pix.pixelformat = pixelformat;
			ret = doioctl(node, VIDIOC_TRY_FMT, &fmt);
			if (ret == EINVAL)
				return fail("TRY_FMT cannot handle an invalid format\n");
		}
		ret = testFormatsType(node, ret, type, fmt, true);
		if (ret)
			return fail("%s is valid, but TRY_FMT failed to return a format\n",
					buftype2s(type).c_str());
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_PRIVATE;
	ret = doioctl(node, VIDIOC_TRY_FMT, &fmt);
	if (ret != ENOTTY && ret != EINVAL)
		return fail("Buffer type PRIVATE allowed!\n");
	return node->valid_buftypes ? result : ENOTTY;
}

static int testM2MFormats(struct node *node)
{
	cv4l_fmt fmt_out;
	cv4l_fmt fmt_cap;
	__u32 cap_type = node->g_type();
	__u32 out_type = v4l_type_invert(cap_type);
	__u32 col, ycbcr_enc, quant, xfer_func;

	fail_on_test(node->g_fmt(fmt_out, out_type));
	node->g_fmt(fmt_cap, cap_type);
	fail_on_test(fmt_cap.g_colorspace() != fmt_out.g_colorspace());
	fail_on_test(fmt_cap.g_ycbcr_enc() != fmt_out.g_ycbcr_enc());
	fail_on_test(fmt_cap.g_quantization() != fmt_out.g_quantization());
	fail_on_test(fmt_cap.g_xfer_func() != fmt_out.g_xfer_func());
	col = fmt_out.g_colorspace() == V4L2_COLORSPACE_SMPTE170M ?
		V4L2_COLORSPACE_REC709 : V4L2_COLORSPACE_SMPTE170M;
	ycbcr_enc = fmt_out.g_ycbcr_enc() == V4L2_YCBCR_ENC_601 ?
		V4L2_YCBCR_ENC_709 : V4L2_YCBCR_ENC_601;
	quant = fmt_out.g_quantization() == V4L2_QUANTIZATION_LIM_RANGE ?
		V4L2_QUANTIZATION_FULL_RANGE : V4L2_QUANTIZATION_LIM_RANGE;
	xfer_func = fmt_out.g_xfer_func() == V4L2_XFER_FUNC_SRGB ?
		V4L2_XFER_FUNC_709 : V4L2_XFER_FUNC_SRGB;
	fmt_out.s_colorspace(col);
	fmt_out.s_xfer_func(xfer_func);
	fmt_out.s_ycbcr_enc(ycbcr_enc);
	fmt_out.s_quantization(quant);
	node->s_fmt(fmt_out);
	fail_on_test(fmt_out.g_colorspace() != col);
	fail_on_test(fmt_out.g_xfer_func() != xfer_func);
	fail_on_test(fmt_out.g_ycbcr_enc() != ycbcr_enc);
	fail_on_test(fmt_out.g_quantization() != quant);
	node->g_fmt(fmt_cap);
	fail_on_test(fmt_cap.g_colorspace() != col);
	fail_on_test(fmt_cap.g_xfer_func() != xfer_func);
	fail_on_test(fmt_cap.g_ycbcr_enc() != ycbcr_enc);
	fail_on_test(fmt_cap.g_quantization() != quant);
	return 0;
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
	struct v4l2_sdr_format *sdr1 = &fmt1.fmt.sdr;
	struct v4l2_sdr_format *sdr2 = &fmt2.fmt.sdr;
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
	// This test will also never succeed if we are using the libv4l2
	// wrapper.
	if (!node->g_direct() || (pixfmt1 == pixfmt2 && w1 == w2 && h1 == h2))
		return 0;

	if (type == V4L2_BUF_TYPE_SDR_CAPTURE ||
	    type == V4L2_BUF_TYPE_SDR_OUTPUT) {
		sdr1->pixelformat = pixfmt1;
		sdr2->pixelformat = pixfmt2;
	} else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
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
	if (type == V4L2_BUF_TYPE_SDR_CAPTURE ||
	    type == V4L2_BUF_TYPE_SDR_OUTPUT) {
		if (sdr1->pixelformat == sdr2->pixelformat) {
			// This compliance test only succeeds if the two formats
			// are really different after S_FMT
			info("Could not perform global format test\n");
			return 0;
		}
	} else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
		   type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (mp1->pixelformat == mp2->pixelformat &&
		    mp1->width == mp2->width && mp1->height == mp2->height) {
			// This compliance test only succeeds if the two formats
			// are really different after S_FMT
			info("Could not perform global format test\n");
			return 0;
		}
	} else {
		if (p1->pixelformat == p2->pixelformat &&
		    p1->width == p2->width && p1->height == p2->height) {
			// This compliance test only succeeds if the two formats
			// are really different after S_FMT
			info("Could not perform global format test\n");
			return 0;
		}
	}
	doioctl(node, VIDIOC_G_FMT, &fmt1);
	if (type == V4L2_BUF_TYPE_SDR_CAPTURE ||
	    type == V4L2_BUF_TYPE_SDR_OUTPUT) {
		pixfmt1 = sdr1->pixelformat;
		pixfmt2 = sdr2->pixelformat;
	} else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
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
		return fail("Global format mismatch: %08x(%s)/%dx%d vs %08x(%s)/%dx%d\n",
			    pixfmt1, fcc2s(pixfmt1).c_str(), w1, h1,
			    pixfmt2, fcc2s(pixfmt2).c_str(), w2, h2);
	info("Global format check succeeded for type %d\n", type);
	return 0;
}

int testSetFormats(struct node *node)
{
	struct v4l2_clip clip, clip_set;
	struct v4l2_format fmt, fmt_set;
	struct v4l2_format initial_fmts[V4L2_BUF_TYPE_LAST + 1];
	int type;
	int ret;
	
	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;

		createInvalidFmt(fmt, clip, type);
		doioctl(node, VIDIOC_G_FMT, &fmt);
		
		initial_fmts[type] = fmt;
		createInvalidFmt(fmt_set, clip_set, type);
		ret = doioctl(node, VIDIOC_S_FMT, &fmt_set);
		if (ret == EINVAL) {
			__u32 pixelformat;
			bool is_mplane = false;

			/* In case of failure obtain a valid pixelformat and insert
			 * that in the next attempt to call TRY_FMT. */
			doioctl(node, VIDIOC_G_FMT, &fmt_set);

			switch (type) {
			case V4L2_BUF_TYPE_SDR_CAPTURE:
			case V4L2_BUF_TYPE_SDR_OUTPUT:
				pixelformat = fmt_set.fmt.sdr.pixelformat;
				break;
			case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			case V4L2_BUF_TYPE_VIDEO_OUTPUT:
				pixelformat = fmt_set.fmt.pix.pixelformat;
				break;
			case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
			case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
				pixelformat = fmt_set.fmt.pix_mp.pixelformat;
				is_mplane = true;
				break;
			case V4L2_BUF_TYPE_VBI_CAPTURE:
			case V4L2_BUF_TYPE_VBI_OUTPUT:
			case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
			case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
				continue;
			default:
				/* for other formats returning EINVAL is certainly wrong */
				return fail("TRY_FMT cannot handle an invalid format\n");
			}
			warn_once("S_FMT cannot handle an invalid pixelformat.\n");
			warn_once("This may or may not be a problem. For more information see:\n");
			warn_once("http://www.mail-archive.com/linux-media@vger.kernel.org/msg56550.html\n");

			/* Now try again, but pass a valid pixelformat. */
			createInvalidFmt(fmt_set, clip_set, type);
			if (node->is_sdr)
				fmt_set.fmt.sdr.pixelformat = pixelformat;
			else if (is_mplane)
				fmt_set.fmt.pix_mp.pixelformat = pixelformat;
			else
				fmt_set.fmt.pix.pixelformat = pixelformat;
			ret = doioctl(node, VIDIOC_S_FMT, &fmt_set);
			if (ret == EINVAL)
				return fail("S_FMT cannot handle an invalid format\n");
		}
		ret = testFormatsType(node, ret, type, fmt_set, true);
		if (ret)
			return fail("%s is valid, but no S_FMT was implemented\n",
					buftype2s(type).c_str());

		fmt_set = fmt;
		ret = doioctl(node, VIDIOC_S_FMT, &fmt_set);
		ret = testFormatsType(node, ret, type, fmt_set);
		if (ret)
			return ret;
		if (!matchFormats(fmt, fmt_set))
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

	// m2m devices are special in that the format is often per-filehandle.
	// But colorspace information should be passed from output to capture,
	// so test that.
	if (node->is_m2m)
		return testM2MFormats(node);

	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
		switch (type) {
		case V4L2_BUF_TYPE_SDR_CAPTURE:
		case V4L2_BUF_TYPE_SDR_OUTPUT:
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

	/* Restore initial format */
	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
		if (!(node->valid_buftypes & (1 << type)))
			continue;

		doioctl(node, VIDIOC_S_FMT, &initial_fmts[type]);
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
		if (node->cur_io_caps & V4L2_IN_CAP_STD)
			fail_on_test(sliced_type && (node->g_caps() & buftype2cap[type]));
		return ret == ENOTTY ? ret : 0;
	}
	fail_on_test(ret);
	fail_on_test(check_0(cap.reserved, sizeof(cap.reserved)));
	fail_on_test(cap.type != type);
	fail_on_test(!sliced_type || !(node->g_caps() & buftype2cap[type]));

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
		if (!(node->g_caps() & V4L2_CAP_READWRITE))
			fail_on_test(cap->readbuffers);
		else if (node->g_caps() & V4L2_CAP_STREAMING)
			fail_on_test(!cap->readbuffers);
		fail_on_test(cap->capability & ~V4L2_CAP_TIMEPERFRAME);
		fail_on_test(node->has_frmintervals && !cap->capability);
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
		if (!(node->g_caps() & V4L2_CAP_READWRITE))
			fail_on_test(out->writebuffers);
		else if (node->g_caps() & V4L2_CAP_STREAMING)
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
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		if (type && (node->g_caps() & buftype2cap[type]))
			fail_on_test(ret && node->has_frmintervals);
		break;
	default:
		fail_on_test(ret == 0);
		memset(&parm, 0, sizeof(parm));
		parm.type = type;
		fail_on_test(!doioctl(node, VIDIOC_S_PARM, &parm));
		break;
	}
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
	fail_on_test(ret && node->has_frmintervals);
	if (!ret && !node->has_frmintervals)
		warn("S_PARM is supported for buftype %d, but not ENUM_FRAMEINTERVALS\n", type);
	if (ret == ENOTTY)
		return 0;
	if (ret)
		return fail("got error %d when setting parms for buftype %d\n", ret, type);
	fail_on_test(parm.type != type);
	if (!(parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME))
		warn("S_PARM is supported but doesn't report V4L2_CAP_TIMEPERFRAME\n");
	return testParmStruct(node, parm);
}

int testParm(struct node *node)
{
	bool supported = false;
	int type;
	int ret;

	for (type = 0; type <= V4L2_BUF_TYPE_LAST; type++) {
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
			if (!(node->g_caps() & buftype2cap[type]))
				return fail("%s cap not set, but G/S_PARM worked\n",
						buftype2s(type).c_str());
		}
	}

	ret = testParmType(node, V4L2_BUF_TYPE_PRIVATE);
	if (ret != ENOTTY && ret != EINVAL)
		return fail("Buffer type PRIVATE allowed!\n");
	return supported ? 0 : ENOTTY;
}

static bool rect_is_inside(const struct v4l2_rect *r1, const struct v4l2_rect *r2)
{
	return r1->left >= r2->left && r1->top >= r2->top &&
	       r1->left + r1->width <= r2->left + r2->width &&
	       r1->top + r1->height <= r2->top + r2->height;
}

static int testBasicSelection(struct node *node, unsigned type, unsigned target)
{
	struct v4l2_selection sel = {
		type,
		target,
	};
	int ret;
	v4l2_format fmt;

	memset(sel.reserved, 0xff, sizeof(sel.reserved));
	ret = doioctl(node, VIDIOC_G_SELECTION, &sel);
	if (ret == ENOTTY || ret == EINVAL || ret == ENODATA) {
		fail_on_test(!doioctl(node, VIDIOC_S_SELECTION, &sel));
		return ENOTTY;
	}
	fail_on_test(ret);
	fail_on_test(check_0(sel.reserved, sizeof(sel.reserved)));

	// selection is not supported (for now) if there is more than one
	// discrete frame size.
	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		v4l_format_init(&fmt, node->is_planar ?
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
			V4L2_BUF_TYPE_VIDEO_CAPTURE);
	else
		v4l_format_init(&fmt, node->is_planar ?
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE :
			V4L2_BUF_TYPE_VIDEO_OUTPUT);
	fail_on_test(doioctl(node, VIDIOC_G_FMT, &fmt));
	__u32 pixfmt = v4l_format_g_pixelformat(&fmt);
	if (node->frmsizes_count.find(pixfmt) != node->frmsizes_count.end())
		fail_on_test(node->frmsizes_count[pixfmt] > 1);

	// _MPLANE types are not allowed
	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else
		sel.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel) != EINVAL);
	// Check handling of invalid type.
	sel.type = 0xff;
	fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel) != EINVAL);
	// Check handling of invalid target.
	sel.type = type;
	sel.target = 0xffff;
	fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel) != EINVAL);
	return 0;
}

static int testBasicCrop(struct node *node, unsigned type)
{
	struct v4l2_selection sel_crop = {
		type,
		V4L2_SEL_TGT_CROP,
	};
	struct v4l2_selection sel_def;
	struct v4l2_selection sel_bounds;
	int s_sel_ret = EINVAL;
	int ret;

	fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel_crop));
	fail_on_test(!sel_crop.r.width || !sel_crop.r.height);
	sel_def = sel_crop;
	sel_def.target = V4L2_SEL_TGT_CROP_DEFAULT;
	fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel_def));
	fail_on_test(!sel_def.r.width || !sel_def.r.height);
	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		fail_on_test(sel_def.r.left || sel_def.r.top);
	sel_bounds = sel_crop;
	sel_bounds.target = V4L2_SEL_TGT_CROP_BOUNDS;
	fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel_bounds));
	fail_on_test(!sel_bounds.r.width || !sel_bounds.r.height);
	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		fail_on_test(sel_bounds.r.left || sel_bounds.r.top);
	fail_on_test(!rect_is_inside(&sel_crop.r, &sel_bounds.r));
	fail_on_test(!rect_is_inside(&sel_def.r, &sel_bounds.r));

	sel_crop.type = type;
	sel_crop.target = V4L2_SEL_TGT_CROP;
	fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel_crop));

	// _MPLANE types are not allowed
	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		sel_crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else
		sel_crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ret = doioctl(node, VIDIOC_S_SELECTION, &sel_crop);
	if (ret == ENOTTY)
		s_sel_ret = ret;
	fail_on_test(ret != s_sel_ret);
	// Check handling of invalid type.
	sel_crop.type = 0xff;
	fail_on_test(doioctl(node, VIDIOC_S_SELECTION, &sel_crop) != s_sel_ret);
	// Check handling of invalid target.
	sel_crop.type = type;
	sel_crop.target = 0xffff;
	fail_on_test(doioctl(node, VIDIOC_S_SELECTION, &sel_crop) != s_sel_ret);
	// Check handling of read-only targets.
	sel_crop.target = V4L2_SEL_TGT_CROP_DEFAULT;
	fail_on_test(doioctl(node, VIDIOC_S_SELECTION, &sel_crop) != s_sel_ret);
	sel_crop.target = V4L2_SEL_TGT_CROP_BOUNDS;
	fail_on_test(doioctl(node, VIDIOC_S_SELECTION, &sel_crop) != s_sel_ret);
	return 0;
}

static int testLegacyCrop(struct node *node)
{
	struct v4l2_cropcap cap = {
		node->g_selection_type()
	};
	struct v4l2_crop crop = {
		node->g_selection_type()
	};
	struct v4l2_selection sel = {
		node->g_selection_type()
	};

	sel.target = node->can_capture ? V4L2_SEL_TGT_CROP_DEFAULT :
					 V4L2_SEL_TGT_COMPOSE_DEFAULT;
	/*
	 * If either CROPCAP or G_CROP works, then G_SELECTION should
	 * work as well.
	 * If neither CROPCAP nor G_CROP work, then G_SELECTION shouldn't
	 * work either.
	 */
	if (!doioctl(node, VIDIOC_CROPCAP, &cap)) {
		fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel));

		// Checks for invalid types
		if (cap.type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
			cap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		else
			cap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		fail_on_test(doioctl(node, VIDIOC_CROPCAP, &cap) != EINVAL);
		cap.type = 0xff;
		fail_on_test(doioctl(node, VIDIOC_CROPCAP, &cap) != EINVAL);
	} else {
		fail_on_test(!doioctl(node, VIDIOC_G_SELECTION, &sel));
	}
	sel.target = node->can_capture ? V4L2_SEL_TGT_CROP :
					 V4L2_SEL_TGT_COMPOSE;
	if (!doioctl(node, VIDIOC_G_CROP, &crop))
		fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel));
	else
		fail_on_test(!doioctl(node, VIDIOC_G_SELECTION, &sel));
	return 0;
}

int testCropping(struct node *node)
{
	int ret_cap, ret_out;

	ret_cap = ENOTTY;
	ret_out = ENOTTY;

	fail_on_test(testLegacyCrop(node));
	if (node->can_capture && node->is_video)
		ret_cap = testBasicSelection(node, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_SEL_TGT_CROP);
	if (node->can_output && node->is_video)
		ret_out = testBasicSelection(node, V4L2_BUF_TYPE_VIDEO_OUTPUT, V4L2_SEL_TGT_CROP);
	if ((!node->can_capture && !node->can_output) || !node->is_video) {
		struct v4l2_selection sel = {
			V4L2_BUF_TYPE_VIDEO_CAPTURE,
			V4L2_SEL_TGT_CROP
		};

		if (node->can_output)
			sel.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel) != ENOTTY);
		fail_on_test(doioctl(node, VIDIOC_S_SELECTION, &sel) != ENOTTY);
	}
	if (ret_cap && ret_out)
		return ret_cap;

	if (!ret_cap)
		fail_on_test(testBasicCrop(node, V4L2_BUF_TYPE_VIDEO_CAPTURE));
	if (!ret_out)
		fail_on_test(testBasicCrop(node, V4L2_BUF_TYPE_VIDEO_OUTPUT));

	return 0;
}

static int testBasicCompose(struct node *node, unsigned type)
{
	struct v4l2_selection sel_compose = {
		type,
		V4L2_SEL_TGT_COMPOSE,
	};
	struct v4l2_selection sel_def;
	struct v4l2_selection sel_bounds;
	struct v4l2_selection sel_padded;
	int s_sel_ret = EINVAL;
	int ret;

	fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel_compose));
	fail_on_test(!sel_compose.r.width || !sel_compose.r.height);
	sel_def = sel_compose;
	sel_def.target = V4L2_SEL_TGT_COMPOSE_DEFAULT;
	fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel_def));
	fail_on_test(!sel_def.r.width || !sel_def.r.height);
	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		fail_on_test(sel_def.r.left || sel_def.r.top);
	sel_bounds = sel_compose;
	sel_bounds.target = V4L2_SEL_TGT_COMPOSE_BOUNDS;
	fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel_bounds));
	fail_on_test(!sel_bounds.r.width || !sel_bounds.r.height);
	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		fail_on_test(sel_bounds.r.left || sel_bounds.r.top);
	fail_on_test(!rect_is_inside(&sel_compose.r, &sel_bounds.r));
	fail_on_test(!rect_is_inside(&sel_def.r, &sel_bounds.r));
	sel_padded = sel_compose;
	sel_padded.target = V4L2_SEL_TGT_COMPOSE_PADDED;
	ret = doioctl(node, VIDIOC_G_SELECTION, &sel_padded);
	fail_on_test(ret && ret != EINVAL);
	if (!ret) {
		fail_on_test(!rect_is_inside(&sel_padded.r, &sel_bounds.r));
		fail_on_test(!sel_padded.r.width || !sel_padded.r.height);
	}

	sel_compose.type = type;
	sel_compose.target = V4L2_SEL_TGT_COMPOSE;
	fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel_compose));

	// _MPLANE types are not allowed
	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		sel_compose.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else
		sel_compose.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ret = doioctl(node, VIDIOC_S_SELECTION, &sel_compose);
	if (ret == ENOTTY)
		s_sel_ret = ret;
	fail_on_test(ret != s_sel_ret);
	// Check handling of invalid type.
	sel_compose.type = 0xff;
	fail_on_test(doioctl(node, VIDIOC_S_SELECTION, &sel_compose) != s_sel_ret);
	// Check handling of invalid target.
	sel_compose.type = type;
	sel_compose.target = 0xffff;
	fail_on_test(doioctl(node, VIDIOC_S_SELECTION, &sel_compose) != s_sel_ret);
	// Check handling of read-only targets.
	sel_compose.target = V4L2_SEL_TGT_COMPOSE_DEFAULT;
	fail_on_test(doioctl(node, VIDIOC_S_SELECTION, &sel_compose) != s_sel_ret);
	sel_compose.target = V4L2_SEL_TGT_COMPOSE_BOUNDS;
	fail_on_test(doioctl(node, VIDIOC_S_SELECTION, &sel_compose) != s_sel_ret);
	sel_compose.target = V4L2_SEL_TGT_COMPOSE_PADDED;
	fail_on_test(doioctl(node, VIDIOC_S_SELECTION, &sel_compose) != s_sel_ret);
	return 0;
}

int testComposing(struct node *node)
{
	int ret_cap, ret_out;

	ret_cap = ENOTTY;
	ret_out = ENOTTY;

	if (node->can_capture && node->is_video)
		ret_cap = testBasicSelection(node, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_SEL_TGT_COMPOSE);
	if (node->can_output && node->is_video)
		ret_out = testBasicSelection(node, V4L2_BUF_TYPE_VIDEO_OUTPUT, V4L2_SEL_TGT_COMPOSE);
	if ((!node->can_capture && !node->can_output) || !node->is_video) {
		struct v4l2_selection sel = {
			V4L2_BUF_TYPE_VIDEO_OUTPUT,
			V4L2_SEL_TGT_COMPOSE
		};

		if (node->can_output)
			sel.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel) != ENOTTY);
		fail_on_test(doioctl(node, VIDIOC_S_SELECTION, &sel) != ENOTTY);
	}
	if (ret_cap && ret_out)
		return ret_cap;

	if (!ret_cap)
		fail_on_test(testBasicCompose(node, V4L2_BUF_TYPE_VIDEO_CAPTURE));
	if (!ret_out)
		fail_on_test(testBasicCompose(node, V4L2_BUF_TYPE_VIDEO_OUTPUT));

	return 0;
}

static int testBasicScaling(struct node *node, const struct v4l2_format &cur)
{
	struct v4l2_selection sel_crop = {
		V4L2_BUF_TYPE_VIDEO_CAPTURE,
		V4L2_SEL_TGT_CROP,
		0,
		{ 1, 1, 0, 0 }
	};
	struct v4l2_selection sel_compose = {
		V4L2_BUF_TYPE_VIDEO_CAPTURE,
		V4L2_SEL_TGT_COMPOSE,
		0,
		{ 1, 1, 0, 0 }
	};
	unsigned compose_w = 0, compose_h = 0;
	unsigned crop_w = 0, crop_h = 0;
	struct v4l2_format fmt = cur;
	bool have_crop = false;
	bool crop_is_fmt = false;
	bool crop_is_const = false;
	bool have_compose = false;
	bool compose_is_fmt = false;
	bool compose_is_const = false;
	bool compose_is_crop = false;
	__u64 cookie;
	int ret;

	if (node->can_output) {
		sel_crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		sel_compose.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	}
	doioctl(node, VIDIOC_S_SELECTION, &sel_crop);
	doioctl(node, VIDIOC_S_SELECTION, &sel_compose);
	v4l_format_s_width(&fmt, 1);
	v4l_format_s_height(&fmt, 1);
	v4l_format_s_field(&fmt, V4L2_FIELD_ANY);
	fail_on_test(doioctl(node, VIDIOC_S_FMT, &fmt));
	ret = doioctl(node, VIDIOC_G_SELECTION, &sel_crop);
	have_crop = ret == 0;
	if (ret == 0) {
		crop_is_fmt = sel_crop.r.width == v4l_format_g_width(&fmt) &&
			      sel_crop.r.height == v4l_format_g_height(&fmt);
		crop_w = sel_crop.r.width;
		crop_h = sel_crop.r.height;
	}
	ret = doioctl(node, VIDIOC_G_SELECTION, &sel_compose);
	have_compose = ret == 0;
	if (ret == 0) {
		compose_is_fmt = sel_compose.r.width == v4l_format_g_width(&fmt) &&
				 sel_compose.r.height == v4l_format_g_height(&fmt);
		compose_w = sel_compose.r.width;
		compose_h = sel_compose.r.height;
	}
	if (have_crop && have_compose)
		compose_is_crop = compose_w == crop_w &&
				  compose_h == crop_h;

	cookie = ((__u64)v4l_format_g_pixelformat(&fmt) << 32) |
		  (v4l_format_g_width(&fmt) << 16) |
		  v4l_format_g_height(&fmt);
	if (node->can_output) {
		if (node->frmsizes.find(cookie) == node->frmsizes.end() && !compose_is_fmt &&
		    (v4l_format_g_width(&fmt) != v4l_format_g_width(&cur) ||
		     v4l_format_g_height(&fmt) != v4l_format_g_height(&cur)))
			node->can_scale = true;
	} else {
		if (node->frmsizes.find(cookie) == node->frmsizes.end() && !crop_is_fmt &&
		    (v4l_format_g_width(&fmt) != v4l_format_g_width(&cur) ||
		     v4l_format_g_height(&fmt) != v4l_format_g_height(&cur)))
			node->can_scale = true;
	}
	sel_crop.r.width = sel_compose.r.width = 0x4000;
	sel_crop.r.height = sel_compose.r.height = 0x4000;
	v4l_format_s_width(&fmt, 0x4000);
	v4l_format_s_height(&fmt, 0x4000);
	v4l_format_s_field(&fmt, V4L2_FIELD_ANY);
	fail_on_test(doioctl(node, VIDIOC_S_FMT, &fmt));
	doioctl(node, VIDIOC_S_SELECTION, &sel_crop);
	doioctl(node, VIDIOC_S_SELECTION, &sel_compose);
	crop_is_fmt = false;
	ret = doioctl(node, VIDIOC_G_SELECTION, &sel_crop);
	if (ret == 0) {
		crop_is_fmt = sel_crop.r.width == v4l_format_g_width(&fmt) &&
			      sel_crop.r.height == v4l_format_g_height(&fmt);
		crop_is_const = sel_crop.r.width == crop_w &&
				sel_crop.r.height == crop_h;
	}
	ret = doioctl(node, VIDIOC_G_SELECTION, &sel_compose);
	if (ret == 0) {
		compose_is_fmt = sel_compose.r.width == v4l_format_g_width(&fmt) &&
				 sel_compose.r.height == v4l_format_g_height(&fmt);
		compose_is_const = sel_compose.r.width == compose_w &&
				   sel_compose.r.height == compose_h;
	}
	if (compose_is_crop)
		compose_is_crop = sel_compose.r.width == sel_crop.r.width &&
				  sel_compose.r.height == sel_crop.r.height;
	cookie = ((__u64)v4l_format_g_pixelformat(&fmt) << 32) |
		  (v4l_format_g_width(&fmt) << 16) |
		  v4l_format_g_height(&fmt);
	if (node->can_output) {
		if (node->frmsizes.find(cookie) == node->frmsizes.end() && !compose_is_fmt &&
		    (v4l_format_g_width(&fmt) != v4l_format_g_width(&cur) ||
		     v4l_format_g_height(&fmt) != v4l_format_g_height(&cur)))
			node->can_scale = true;
		if (crop_is_const || compose_is_crop)
			node->can_scale = false;
	} else {
		if (node->frmsizes.find(cookie) == node->frmsizes.end() && !crop_is_fmt &&
		    (v4l_format_g_width(&fmt) != v4l_format_g_width(&cur) ||
		     v4l_format_g_height(&fmt) != v4l_format_g_height(&cur)))
			node->can_scale = true;
		if (compose_is_const || compose_is_crop)
			node->can_scale = false;
	}
	fail_on_test(node->can_scale &&
		     node->frmsizes_count[v4l_format_g_pixelformat(&cur)]);
	return 0;
}

int testScaling(struct node *node)
{
	struct v4l2_format fmt;

	if (!node->is_video)
		return ENOTTY;
	node->can_scale = false;
	if (node->can_capture) {
		v4l_format_init(&fmt, node->is_planar ?
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
			V4L2_BUF_TYPE_VIDEO_CAPTURE);
		fail_on_test(doioctl(node, VIDIOC_G_FMT, &fmt));
		testBasicScaling(node, fmt);
		fail_on_test(doioctl(node, VIDIOC_S_FMT, &fmt));
	}
	if (node->can_output) {
		v4l_format_init(&fmt, node->is_planar ?
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE :
			V4L2_BUF_TYPE_VIDEO_OUTPUT);
		fail_on_test(doioctl(node, VIDIOC_G_FMT, &fmt));
		testBasicScaling(node, fmt);
		fail_on_test(doioctl(node, VIDIOC_S_FMT, &fmt));
	}
	return node->can_scale ? 0 : ENOTTY;
}
