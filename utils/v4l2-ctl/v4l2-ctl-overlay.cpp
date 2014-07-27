#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <dirent.h>
#include <math.h>

#include <linux/fb.h>
#include <vector>

#include "v4l2-ctl.h"

static unsigned int set_fbuf;
static unsigned int set_overlay_fmt;
static struct v4l2_format overlay_fmt;	/* set_format/get_format video overlay */
static struct v4l2_framebuffer fbuf;	/* fbuf */
static int overlay;			/* overlay */
static const char *fb_device;
static std::vector<struct v4l2_clip> clips;
static std::vector<struct v4l2_rect> bitmap_rects;

void overlay_usage(void)
{
	printf("\nVideo Overlay options:\n"
	       "  --list-formats-overlay\n"
	       "                     display supported overlay formats [VIDIOC_ENUM_FMT]\n"
	       "  --find-fb          find the fb device corresponding with the overlay\n"
	       "  --overlay=<on>     turn overlay on (1) or off (0) (VIDIOC_OVERLAY)\n"
	       "  --get-fmt-overlay  query the video or video output overlay format [VIDIOC_G_FMT]\n"
	       "  --set-fmt-overlay\n"
	       "  --try-fmt-overlay=chromakey=<key>,global_alpha=<alpha>,\n"
	       "                           top=<t>,left=<l>,width=<w>,height=<h>,field=<f>\n"
	       "     		     set/try the video or video output overlay format [VIDIOC_TRY/S_FMT]\n"
	       "                     <f> can be one of:\n"
	       "                     any, none, top, bottom, interlaced, seq_tb, seq_bt,\n"
	       "                     alternate, interlaced_tb, interlaced_bt\n"
	       "                     If the width or height changed then the old clip list and bitmap will\n"
	       "                     be invalidated.\n"
	       "  --clear-clips      clear any old clips, to be used in combination with --try/set-fmt-overlay\n"
	       "  --clear-bitmap     clear any old bitmap, to be used in combination with --try/set-fmt-overlay\n"
	       "  --add-clip=top=<t>,left=<l>,width=<w>,height=<h>\n"
	       "                     Add an entry to the clip list. May be used multiple times.\n"
	       "                     This clip list will be passed to --try/set-fmt-overlay\n"
	       "  --add-bitmap=top=<t>,left=<l>,width=<w>,height=<h>\n"
	       "                     Set the bits in the given rectangle in the bitmap to 1. May be\n"
	       "                     used multiple times.\n"
	       "                     The bitmap will be passed to --try/set-fmt-overlay\n"
	       "  --get-fbuf         query the overlay framebuffer data [VIDIOC_G_FBUF]\n"
	       "  --set-fbuf=chromakey=<b>,src_chromakey=<b>,global_alpha=<b>,local_alpha=<b>,local_inv_alpha=<b>,fb=<fb>\n"
	       "		     set the overlay framebuffer [VIDIOC_S_FBUF]\n"
	       "                     <b> is 0 or 1\n"
	       "                     <fb> is the framebuffer device (/dev/fbX)\n"
	       "                     if <fb> starts with a digit, then /dev/fb<fb> is used\n"
	       );
}

static std::string fbufcap2s(unsigned cap)
{
	std::string s;

	if (cap & V4L2_FBUF_CAP_EXTERNOVERLAY)
		s += "\t\t\tExtern Overlay\n";
	if (cap & V4L2_FBUF_CAP_CHROMAKEY)
		s += "\t\t\tChromakey\n";
	if (cap & V4L2_FBUF_CAP_SRC_CHROMAKEY)
		s += "\t\t\tSource Chromakey\n";
	if (cap & V4L2_FBUF_CAP_GLOBAL_ALPHA)
		s += "\t\t\tGlobal Alpha\n";
	if (cap & V4L2_FBUF_CAP_LOCAL_ALPHA)
		s += "\t\t\tLocal Alpha\n";
	if (cap & V4L2_FBUF_CAP_LOCAL_INV_ALPHA)
		s += "\t\t\tLocal Inverted Alpha\n";
	if (cap & V4L2_FBUF_CAP_LIST_CLIPPING)
		s += "\t\t\tClipping List\n";
	if (cap & V4L2_FBUF_CAP_BITMAP_CLIPPING)
		s += "\t\t\tClipping Bitmap\n";
	if (s.empty()) s += "\t\t\t\n";
	return s;
}

static std::string fbufflags2s(unsigned fl)
{
	std::string s;

	if (fl & V4L2_FBUF_FLAG_PRIMARY)
		s += "\t\t\tPrimary Graphics Surface\n";
	if (fl & V4L2_FBUF_FLAG_OVERLAY)
		s += "\t\t\tOverlay Matches Capture/Output Size\n";
	if (fl & V4L2_FBUF_FLAG_CHROMAKEY)
		s += "\t\t\tChromakey\n";
	if (fl & V4L2_FBUF_FLAG_SRC_CHROMAKEY)
		s += "\t\t\tSource Chromakey\n";
	if (fl & V4L2_FBUF_FLAG_GLOBAL_ALPHA)
		s += "\t\t\tGlobal Alpha\n";
	if (fl & V4L2_FBUF_FLAG_LOCAL_ALPHA)
		s += "\t\t\tLocal Alpha\n";
	if (fl & V4L2_FBUF_FLAG_LOCAL_INV_ALPHA)
		s += "\t\t\tLocal Inverted Alpha\n";
	if (s.empty()) s += "\t\t\t\n";
	return s;
}

static void printfbuf(const struct v4l2_framebuffer &fb)
{
	int is_ext = fb.capability & V4L2_FBUF_CAP_EXTERNOVERLAY;

	printf("Framebuffer Format:\n");
	printf("\tCapability    : %s", fbufcap2s(fb.capability).c_str() + 3);
	printf("\tFlags         : %s", fbufflags2s(fb.flags).c_str() + 3);
	if (fb.base)
		printf("\tBase          : %p\n", fb.base);
	printf("\tWidth         : %d\n", fb.fmt.width);
	printf("\tHeight        : %d\n", fb.fmt.height);
	printf("\tPixel Format  : '%s'\n", fcc2s(fb.fmt.pixelformat).c_str());
	if (!is_ext) {
		printf("\tBytes per Line: %d\n", fb.fmt.bytesperline);
		printf("\tSize image    : %d\n", fb.fmt.sizeimage);
		printf("\tColorspace    : %s\n", colorspace2s(fb.fmt.colorspace).c_str());
		if (fb.fmt.priv)
			printf("\tCustom Info   : %08x\n", fb.fmt.priv);
	}
}

static void find_fb(int fd)
{
	struct v4l2_framebuffer fbuf;
	unsigned int i;
	int fb_fd;

	if (doioctl(fd, VIDIOC_G_FBUF, &fbuf))
		return;
	if (fbuf.base == 0) {
		printf("No framebuffer base address was defined\n");
		return;
	}

	for (i = 0; i < 30; i++) {
		struct fb_fix_screeninfo si;
		char dev_name[16];

		snprintf(dev_name, sizeof(dev_name), "/dev/fb%u", i);

		fb_fd = open(dev_name, O_RDWR);
		if (fb_fd == -1) {
			switch (errno) {
			case ENOENT: /* no such file */
			case ENXIO:  /* no driver */
				continue;

			default:
				return;
			}
		}

		if (!ioctl(fb_fd, FBIOGET_FSCREENINFO, &si))
			if (si.smem_start == (unsigned long)fbuf.base) {
				printf("%s is the framebuffer associated with base address %p\n",
					dev_name, fbuf.base);
				close(fb_fd);
				return;
			}
		close(fb_fd);
		fb_fd = -1;
	}
	printf("No matching framebuffer found for base address %p\n", fbuf.base);
}

struct bitfield2fmt {
	unsigned red_off, red_len;
	unsigned green_off, green_len;
	unsigned blue_off, blue_len;
	unsigned transp_off, transp_len;
	__u32 pixfmt;
};

static const struct bitfield2fmt fb_formats[] = {
	{ 10, 5,  5, 5,  0, 5, 15, 1, V4L2_PIX_FMT_ARGB555 },
	{ 11, 5,  5, 6,  0, 5,  0, 0, V4L2_PIX_FMT_RGB565 },
	{  1, 5,  6, 5, 11, 5,  0, 1, V4L2_PIX_FMT_RGB555X },
	{  0, 5,  5, 6, 11, 5,  0, 0, V4L2_PIX_FMT_RGB565X },
	{ 16, 8,  8, 8,  0, 8,  0, 0, V4L2_PIX_FMT_BGR24 },
	{  0, 8,  8, 8, 16, 8,  0, 0, V4L2_PIX_FMT_RGB24 },
	{ 16, 8,  8, 8,  0, 8, 24, 8, V4L2_PIX_FMT_ABGR32 },
	{  8, 8, 16, 8, 24, 8,  0, 8, V4L2_PIX_FMT_ARGB32 },
	{ }
};

static bool match_bitfield(const struct fb_bitfield &bf, unsigned off, unsigned len)
{
	if (bf.msb_right || bf.length != len)
		return false;
	return !len || bf.offset == off;
}

static int fbuf_fill_from_fb(struct v4l2_framebuffer &fb, const char *fb_device)
{
	struct fb_fix_screeninfo si;
	struct fb_var_screeninfo vi;
	int fb_fd;

	if (fb_device == NULL)
		return 0;
	fb_fd = open(fb_device, O_RDWR);
	if (fb_fd == -1) {
		fprintf(stderr, "cannot open %s\n", fb_device);
		return -1;
	}
	if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &si)) {
		fprintf(stderr, "could not obtain fscreeninfo from %s\n", fb_device);
		close(fb_fd);
		return -1;
	}
	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vi)) {
		fprintf(stderr, "could not obtain vscreeninfo from %s\n", fb_device);
		close(fb_fd);
		return -1;
	}
	fb.base = (void *)si.smem_start;
	fb.fmt.sizeimage = si.smem_len;
	fb.fmt.width = vi.xres;
	fb.fmt.height = vi.yres;
	fb.fmt.bytesperline = si.line_length;
	if (fb.fmt.height * fb.fmt.bytesperline > fb.fmt.sizeimage) {
		fprintf(stderr, "height * bytesperline > sizeimage?!\n");
		close(fb_fd);
		return -1;
	}
	fb.fmt.pixelformat = 0;
	if ((si.capabilities & FB_CAP_FOURCC) && vi.grayscale > 1) {
		fb.fmt.pixelformat = vi.grayscale;
		fb.fmt.colorspace = vi.colorspace;
	} else {
		if (vi.grayscale == 1) {
			if (vi.bits_per_pixel == 8)
				fb.fmt.pixelformat = V4L2_PIX_FMT_GREY;
		} else {
			for (int i = 0; fb_formats[i].pixfmt; i++) {
				const struct bitfield2fmt &p = fb_formats[i];

				if (match_bitfield(vi.blue, p.blue_off, p.blue_len) &&
				    match_bitfield(vi.green, p.green_off, p.green_len) &&
				    match_bitfield(vi.red, p.red_off, p.red_len) &&
				    match_bitfield(vi.transp, p.transp_off, p.transp_len)) {
					fb.fmt.pixelformat = p.pixfmt;
					break;
				}
			}
		}
		fb.fmt.colorspace = V4L2_COLORSPACE_SRGB;
	}
	printfbuf(fb);
	close(fb_fd);
	return 0;
}

void overlay_cmd(int ch, char *optarg)
{
	struct v4l2_rect r;
	char *value, *subs;

	switch (ch) {
	case OptSetOverlayFormat:
	case OptTryOverlayFormat:
		subs = optarg;
		while (subs && *subs != '\0') {
			static const char *const subopts[] = {
				"chromakey",
				"global_alpha",
				"left",
				"top",
				"width",
				"height",
				"field",
				NULL
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				overlay_fmt.fmt.win.chromakey = strtoul(value, 0L, 0);
				set_overlay_fmt |= FmtChromaKey;
				break;
			case 1:
				overlay_fmt.fmt.win.global_alpha = strtoul(value, 0L, 0);
				set_overlay_fmt |= FmtGlobalAlpha;
				break;
			case 2:
				overlay_fmt.fmt.win.w.left = strtol(value, 0L, 0);
				set_overlay_fmt |= FmtLeft;
				break;
			case 3:
				overlay_fmt.fmt.win.w.top = strtol(value, 0L, 0);
				set_overlay_fmt |= FmtTop;
				break;
			case 4:
				overlay_fmt.fmt.win.w.width = strtoul(value, 0L, 0);
				set_overlay_fmt |= FmtWidth;
				break;
			case 5:
				overlay_fmt.fmt.win.w.height = strtoul(value, 0L, 0);
				set_overlay_fmt |= FmtHeight;
				break;
			case 6:
				overlay_fmt.fmt.win.field = parse_field(value);
				set_overlay_fmt |= FmtField;
				break;
			default:
				overlay_usage();
				break;
			}
		}
		break;
	case OptAddClip:
	case OptAddBitmap:
		subs = optarg;
		memset(&r, 0, sizeof(r));
		while (*subs != '\0') {
			static const char *const subopts[] = {
				"left",
				"top",
				"width",
				"height",
				NULL
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				r.left = strtoul(value, 0L, 0);
				break;
			case 1:
				r.top = strtoul(value, 0L, 0);
				break;
			case 2:
				r.width = strtoul(value, 0L, 0);
				break;
			case 3:
				r.height = strtoul(value, 0L, 0);
				break;
			default:
				overlay_usage();
				return;
			}
		}
		if (r.width == 0 || r.height == 0) {
			overlay_usage();
			return;
		}
		if (ch == OptAddBitmap) {
			bitmap_rects.push_back(r);
		} else {
			struct v4l2_clip c;

			c.c = r;
			c.next = NULL;
			clips.push_back(c);
		}
		break;
	case OptSetFBuf:
		subs = optarg;
		while (*subs != '\0') {
			const unsigned chroma_flags = V4L2_FBUF_FLAG_CHROMAKEY |
						      V4L2_FBUF_FLAG_SRC_CHROMAKEY;
			const unsigned alpha_flags = V4L2_FBUF_FLAG_GLOBAL_ALPHA |
						     V4L2_FBUF_FLAG_LOCAL_ALPHA |
						     V4L2_FBUF_FLAG_LOCAL_INV_ALPHA;
			static const char *const subopts[] = {
				"chromakey",
				"src_chromakey",
				"global_alpha",
				"local_alpha",
				"local_inv_alpha",
				"fb",
				NULL
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				fbuf.flags &= ~chroma_flags;
				fbuf.flags |= strtol(value, 0L, 0) ? V4L2_FBUF_FLAG_CHROMAKEY : 0;
				set_fbuf |= chroma_flags;
				break;
			case 1:
				fbuf.flags &= ~chroma_flags;
				fbuf.flags |= strtol(value, 0L, 0) ? V4L2_FBUF_FLAG_SRC_CHROMAKEY : 0;
				set_fbuf |= chroma_flags;
				break;
			case 2:
				fbuf.flags &= ~alpha_flags;
				fbuf.flags |= strtol(value, 0L, 0) ? V4L2_FBUF_FLAG_GLOBAL_ALPHA : 0;
				set_fbuf |= alpha_flags;
				break;
			case 3:
				fbuf.flags &= ~alpha_flags;
				fbuf.flags |= strtol(value, 0L, 0) ? V4L2_FBUF_FLAG_LOCAL_ALPHA : 0;
				set_fbuf |= alpha_flags;
				break;
			case 4:
				fbuf.flags &= ~alpha_flags;
				fbuf.flags |= strtol(value, 0L, 0) ? V4L2_FBUF_FLAG_LOCAL_INV_ALPHA : 0;
				set_fbuf |= alpha_flags;
				break;
			case 5:
				fb_device = value;
				if (fb_device[0] >= '0' && fb_device[0] <= '9' && strlen(fb_device) <= 3) {
					static char newdev[20];

					sprintf(newdev, "/dev/fb%s", fb_device);
					fb_device = newdev;
				}
				break;
			default:
				overlay_usage();
				break;
			}
		}
		break;
	case OptOverlay:
		overlay = strtol(optarg, 0L, 0);
		break;
	}
}

static void do_try_set_overlay(struct v4l2_format &fmt, int fd)
{
	struct v4l2_window &win = fmt.fmt.win;
	bool keep_old_clip = true;
	bool keep_old_bitmap = true;
	struct v4l2_clip *cliplist = NULL;
	unsigned stride = (win.w.width + 7) / 8;
	unsigned char *bitmap = NULL;
	int ret;

	if (((set_overlay_fmt & FmtWidth) && win.w.width != overlay_fmt.fmt.win.w.width) ||
	    ((set_overlay_fmt & FmtHeight) && win.w.height != overlay_fmt.fmt.win.w.height))
		keep_old_bitmap = keep_old_clip = false;
	if (options[OptClearBitmap] || bitmap_rects.size())
		keep_old_bitmap = false;
	if (options[OptClearClips] || clips.size())
		keep_old_clip = false;

	win.bitmap = NULL;
	if (keep_old_bitmap) {
		bitmap = (unsigned char *)calloc(1, stride * win.w.height);
		win.bitmap = bitmap;
	}
	if (keep_old_clip) {
		if (win.clipcount)
			cliplist = (struct v4l2_clip *)malloc(win.clipcount * sizeof(*cliplist));
		win.clips = cliplist;
	}
	if (keep_old_clip || keep_old_bitmap)
		if (doioctl(fd, VIDIOC_G_FMT, &fmt))
			goto free;

	if (set_overlay_fmt & FmtChromaKey)
		win.chromakey = overlay_fmt.fmt.win.chromakey;
	if (set_overlay_fmt & FmtGlobalAlpha)
		win.global_alpha = overlay_fmt.fmt.win.global_alpha;
	if (set_overlay_fmt & FmtLeft)
		win.w.left = overlay_fmt.fmt.win.w.left;
	if (set_overlay_fmt & FmtTop)
		win.w.top = overlay_fmt.fmt.win.w.top;
	if (set_overlay_fmt & FmtWidth)
		win.w.width = overlay_fmt.fmt.win.w.width;
	if (set_overlay_fmt & FmtHeight)
		win.w.height = overlay_fmt.fmt.win.w.height;
	if (set_overlay_fmt & FmtField)
		win.field = overlay_fmt.fmt.win.field;
	if (clips.size()) {
		win.clipcount = clips.size();
		win.clips = &clips[0];
	}
	if (bitmap_rects.size()) {
		free(bitmap);
		stride = (win.w.width + 7) / 8;
		bitmap = (unsigned char *)calloc(1, stride * win.w.height);
		win.bitmap = bitmap;
		for (unsigned i = 0; i < bitmap_rects.size(); i++) {
			const v4l2_rect &r = bitmap_rects[i];

			if (r.left + r.width > win.w.width ||
			    r.top + r.height > win.w.height) {
				fprintf(stderr, "rectangle is out of range\n");
				return;
			}
			for (unsigned y = r.top; y < r.top + r.height; y++)
				for (unsigned x = r.left; x < r.left + r.width; x++)
					bitmap[y * stride + x / 8] |= 1 << (x & 7);
		}
		win.bitmap = bitmap;
	}
	if (options[OptSetOverlayFormat])
		ret = doioctl(fd, VIDIOC_S_FMT, &fmt);
	else
		ret = doioctl(fd, VIDIOC_TRY_FMT, &fmt);
	if (ret == 0 && (verbose || options[OptTryOverlayFormat]))
		printfmt(fmt);

free:
	if (bitmap)
		free(bitmap);
	if (cliplist)
		free(cliplist);
}

void overlay_set(int fd)
{
	if ((options[OptSetOverlayFormat] || options[OptTryOverlayFormat]) &&
			(set_overlay_fmt || options[OptClearClips] || options[OptClearBitmap] ||
			 bitmap_rects.size() || clips.size())) {
		struct v4l2_format fmt;

		memset(&fmt, 0, sizeof(fmt));
		// You can never have both VIDEO_OVERLAY and VIDEO_OUTPUT_OVERLAY
		if (capabilities & V4L2_CAP_VIDEO_OVERLAY)
			fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
		else
			fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
		if (doioctl(fd, VIDIOC_G_FMT, &fmt) == 0)
			do_try_set_overlay(fmt, fd);
	}

	if (options[OptSetFBuf]) {
		struct v4l2_framebuffer fb;

		if (doioctl(fd, VIDIOC_G_FBUF, &fb) == 0) {
			fb.flags &= ~set_fbuf;
			fb.flags |= fbuf.flags;
			if (!fbuf_fill_from_fb(fb, fb_device))
				doioctl(fd, VIDIOC_S_FBUF, &fb);
		}
	}

	if (options[OptOverlay]) {
		doioctl(fd, VIDIOC_OVERLAY, &overlay);
	}
}

void overlay_get(int fd)
{
	if (options[OptGetOverlayFormat]) {
		struct v4l2_format fmt;
		unsigned char *bitmap = NULL;

		memset(&fmt, 0, sizeof(fmt));
		// You can never have both VIDEO_OVERLAY and VIDEO_OUTPUT_OVERLAY
		if (capabilities & V4L2_CAP_VIDEO_OVERLAY)
			fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
		else
			fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
		if (doioctl(fd, VIDIOC_G_FMT, &fmt) == 0) {
			unsigned stride = (fmt.fmt.win.w.width + 7) / 8;

			if (fmt.fmt.win.clipcount)
				fmt.fmt.win.clips = (struct v4l2_clip *)malloc(fmt.fmt.win.clipcount * sizeof(clips[0]));
			bitmap = (unsigned char *)calloc(1, stride * fmt.fmt.win.w.height);
			fmt.fmt.win.bitmap = bitmap;
			doioctl(fd, VIDIOC_G_FMT, &fmt);
			printfmt(fmt);
			if (fmt.fmt.win.clips)
				free(fmt.fmt.win.clips);
			if (bitmap)
				free(bitmap);
		}
	}

	if (options[OptGetFBuf]) {
		struct v4l2_framebuffer fb;
		if (doioctl(fd, VIDIOC_G_FBUF, &fb) == 0)
			printfbuf(fb);
	}
}

void overlay_list(int fd)
{
	if (options[OptListOverlayFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_VIDEO_OVERLAY);
	}
	if (options[OptFindFb])
		find_fb(fd);
}
