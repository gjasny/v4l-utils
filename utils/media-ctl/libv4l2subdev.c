/*
 * V4L2 subdev interface library
 *
 * Copyright (C) 2010-2014 Ideas on board SPRL
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/v4l2-subdev.h>

#include "mediactl.h"
#include "mediactl-priv.h"
#include "tools.h"
#include "v4l2subdev.h"

int v4l2_subdev_open(struct media_entity *entity)
{
	if (entity->fd != -1)
		return 0;

	entity->fd = open(entity->devname, O_RDWR);
	if (entity->fd == -1) {
		int ret = -errno;
		media_dbg(entity->media,
			  "%s: Failed to open subdev device node %s\n", __func__,
			  entity->devname);
		return ret;
	}

	return 0;
}

void v4l2_subdev_close(struct media_entity *entity)
{
	close(entity->fd);
	entity->fd = -1;
}

int v4l2_subdev_get_format(struct media_entity *entity,
	struct v4l2_mbus_framefmt *format, unsigned int pad,
	enum v4l2_subdev_format_whence which)
{
	struct v4l2_subdev_format fmt;
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(&fmt, 0, sizeof(fmt));
	fmt.pad = pad;
	fmt.which = which;

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_G_FMT, &fmt);
	if (ret < 0)
		return -errno;

	*format = fmt.format;
	return 0;
}

int v4l2_subdev_set_format(struct media_entity *entity,
	struct v4l2_mbus_framefmt *format, unsigned int pad,
	enum v4l2_subdev_format_whence which)
{
	struct v4l2_subdev_format fmt;
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(&fmt, 0, sizeof(fmt));
	fmt.pad = pad;
	fmt.which = which;
	fmt.format = *format;

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_S_FMT, &fmt);
	if (ret < 0)
		return -errno;

	*format = fmt.format;
	return 0;
}

int v4l2_subdev_get_selection(struct media_entity *entity,
	struct v4l2_rect *rect, unsigned int pad, unsigned int target,
	enum v4l2_subdev_format_whence which)
{
	union {
		struct v4l2_subdev_selection sel;
		struct v4l2_subdev_crop crop;
	} u;
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(&u.sel, 0, sizeof(u.sel));
	u.sel.pad = pad;
	u.sel.target = target;
	u.sel.which = which;

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_G_SELECTION, &u.sel);
	if (ret >= 0) {
		*rect = u.sel.r;
		return 0;
	}
	if (errno != ENOTTY || target != V4L2_SEL_TGT_CROP)
		return -errno;

	memset(&u.crop, 0, sizeof(u.crop));
	u.crop.pad = pad;
	u.crop.which = which;

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_G_CROP, &u.crop);
	if (ret < 0)
		return -errno;

	*rect = u.crop.rect;
	return 0;
}

int v4l2_subdev_set_selection(struct media_entity *entity,
	struct v4l2_rect *rect, unsigned int pad, unsigned int target,
	enum v4l2_subdev_format_whence which)
{
	union {
		struct v4l2_subdev_selection sel;
		struct v4l2_subdev_crop crop;
	} u;
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(&u.sel, 0, sizeof(u.sel));
	u.sel.pad = pad;
	u.sel.target = target;
	u.sel.which = which;
	u.sel.r = *rect;

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_S_SELECTION, &u.sel);
	if (ret >= 0) {
		*rect = u.sel.r;
		return 0;
	}
	if (errno != ENOTTY || target != V4L2_SEL_TGT_CROP)
		return -errno;

	memset(&u.crop, 0, sizeof(u.crop));
	u.crop.pad = pad;
	u.crop.which = which;
	u.crop.rect = *rect;

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_S_CROP, &u.crop);
	if (ret < 0)
		return -errno;

	*rect = u.crop.rect;
	return 0;
}

int v4l2_subdev_get_dv_timings_caps(struct media_entity *entity,
	struct v4l2_dv_timings_cap *caps)
{
	unsigned int pad = caps->pad;
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(caps, 0, sizeof(*caps));
	caps->pad = pad;

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_DV_TIMINGS_CAP, caps);
	if (ret < 0)
		return -errno;

	return 0;
}

int v4l2_subdev_query_dv_timings(struct media_entity *entity,
	struct v4l2_dv_timings *timings)
{
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(timings, 0, sizeof(*timings));

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_QUERY_DV_TIMINGS, timings);
	if (ret < 0)
		return -errno;

	return 0;
}

int v4l2_subdev_get_dv_timings(struct media_entity *entity,
	struct v4l2_dv_timings *timings)
{
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(timings, 0, sizeof(*timings));

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_G_DV_TIMINGS, timings);
	if (ret < 0)
		return -errno;

	return 0;
}

int v4l2_subdev_set_dv_timings(struct media_entity *entity,
	struct v4l2_dv_timings *timings)
{
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_S_DV_TIMINGS, timings);
	if (ret < 0)
		return -errno;

	return 0;
}

int v4l2_subdev_get_frame_interval(struct media_entity *entity,
				   struct v4l2_fract *interval)
{
	struct v4l2_subdev_frame_interval ival;
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(&ival, 0, sizeof(ival));

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_G_FRAME_INTERVAL, &ival);
	if (ret < 0)
		return -errno;

	*interval = ival.interval;
	return 0;
}

int v4l2_subdev_set_frame_interval(struct media_entity *entity,
				   struct v4l2_fract *interval)
{
	struct v4l2_subdev_frame_interval ival;
	int ret;

	ret = v4l2_subdev_open(entity);
	if (ret < 0)
		return ret;

	memset(&ival, 0, sizeof(ival));
	ival.interval = *interval;

	ret = ioctl(entity->fd, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &ival);
	if (ret < 0)
		return -errno;

	*interval = ival.interval;
	return 0;
}

static int v4l2_subdev_parse_format(struct media_device *media,
				    struct v4l2_mbus_framefmt *format,
				    const char *p, char **endp)
{
	enum v4l2_mbus_pixelcode code;
	unsigned int width, height;
	char *fmt;
	char *end;

	/*
	 * Compatibility with the old syntax: consider space as valid
	 * separator between the media bus pixel code and the size.
	 */
	for (; isspace(*p); ++p);
	for (end = (char *)p;
	     *end != '/' && *end != ' ' && *end != '\0'; ++end);

	fmt = strndup(p, end - p);
	if (!fmt)
		return -ENOMEM;

	code = v4l2_subdev_string_to_pixelcode(fmt);
	free(fmt);
	if (code == (enum v4l2_mbus_pixelcode)-1) {
		media_dbg(media, "Invalid pixel code '%.*s'\n", end - p, p);
		return -EINVAL;
	}

	p = end + 1;
	width = strtoul(p, &end, 10);
	if (*end != 'x') {
		media_dbg(media, "Expected 'x'\n");
		return -EINVAL;
	}

	p = end + 1;
	height = strtoul(p, &end, 10);
	*endp = end;

	memset(format, 0, sizeof(*format));
	format->width = width;
	format->height = height;
	format->code = code;

	return 0;
}

static int v4l2_subdev_parse_rectangle(struct media_device *media,
				       struct v4l2_rect *r, const char *p,
				       char **endp)
{
	char *end;

	if (*p++ != '(') {
		media_dbg(media, "Expected '('\n");
		*endp = (char *)p - 1;
		return -EINVAL;
	}

	r->left = strtoul(p, &end, 10);
	if (*end != ',') {
		media_dbg(media, "Expected ','\n");
		*endp = end;
		return -EINVAL;
	}

	p = end + 1;
	r->top = strtoul(p, &end, 10);
	if (*end++ != ')') {
		media_dbg(media, "Expected ')'\n");
		*endp = end - 1;
		return -EINVAL;
	}
	if (*end != '/') {
		media_dbg(media, "Expected '/'\n");
		*endp = end;
		return -EINVAL;
	}

	p = end + 1;
	r->width = strtoul(p, &end, 10);
	if (*end != 'x') {
		media_dbg(media, "Expected 'x'\n");
		*endp = end;
		return -EINVAL;
	}

	p = end + 1;
	r->height = strtoul(p, &end, 10);
	*endp = end;

	return 0;
}

static int v4l2_subdev_parse_frame_interval(struct media_device *media,
					    struct v4l2_fract *interval,
					    const char *p, char **endp)
{
	char *end;

	for (; isspace(*p); ++p);

	interval->numerator = strtoul(p, &end, 10);

	for (p = end; isspace(*p); ++p);
	if (*p++ != '/') {
		media_dbg(media, "Expected '/'\n");
		*endp = (char *)p - 1;
		return -EINVAL;
	}

	for (; isspace(*p); ++p);
	interval->denominator = strtoul(p, &end, 10);

	*endp = end;
	return 0;
}

/*
 * The debate over whether this function should be named icanhasstr() instead
 * has been strong and heated. If you feel like this would be an important
 * change, patches are welcome (or not).
 */
static bool strhazit(const char *str, const char **p)
{
	int len = strlen(str);

	if (strncmp(str, *p, len))
		return false;

	for (*p += len; isspace(**p); ++*p);
	return true;
}

static struct media_pad *v4l2_subdev_parse_pad_format(
	struct media_device *media, struct v4l2_mbus_framefmt *format,
	struct v4l2_rect *crop, struct v4l2_rect *compose,
	struct v4l2_fract *interval, const char *p, char **endp)
{
	struct media_pad *pad;
	bool first;
	char *end;
	int ret;

	for (; isspace(*p); ++p);

	pad = media_parse_pad(media, p, &end);
	if (pad == NULL) {
		*endp = end;
		return NULL;
	}

	for (p = end; isspace(*p); ++p);
	if (*p++ != '[') {
		media_dbg(media, "Expected '['\n");
		*endp = (char *)p - 1;
		return NULL;
	}

	for (first = true; ; first = false) {
		for (; isspace(*p); p++);

		/*
		 * Backward compatibility: if the first property starts with an
		 * uppercase later, process it as a format description.
		 */
		if (strhazit("fmt:", &p) || (first && isupper(*p))) {
			ret = v4l2_subdev_parse_format(media, format, p, &end);
			if (ret < 0) {
				*endp = end;
				return NULL;
			}

			p = end;
			continue;
		}

		if (strhazit("field:", &p)) {
			enum v4l2_field field;
			char *strfield;

			for (end = (char *)p; isalpha(*end) || *end == '-';
			     ++end);

			strfield = strndup(p, end - p);
			if (!strfield) {
				*endp = (char *)p;
				return NULL;
			}

			field = v4l2_subdev_string_to_field(strfield);
			free(strfield);
			if (field == (enum v4l2_field)-1) {
				media_dbg(media, "Invalid field value '%*s'\n",
					  end - p, p);
				*endp = (char *)p;
				return NULL;
			}

			format->field = field;

			p = end;
			continue;
		}

		/*
		 * Backward compatibility: crop rectangles can be specified
		 * implicitly without the 'crop:' property name.
		 */
		if (strhazit("crop:", &p) || *p == '(') {
			ret = v4l2_subdev_parse_rectangle(media, crop, p, &end);
			if (ret < 0) {
				*endp = end;
				return NULL;
			}

			p = end;
			continue;
		}

		if (strhazit("compose:", &p)) {
			ret = v4l2_subdev_parse_rectangle(media, compose, p, &end);
			if (ret < 0) {
				*endp = end;
				return NULL;
			}

			for (p = end; isspace(*p); p++);
			continue;
		}

		if (*p == '@') {
			ret = v4l2_subdev_parse_frame_interval(media, interval, ++p, &end);
			if (ret < 0) {
				*endp = end;
				return NULL;
			}

			p = end;
			continue;
		}

		break;
	}

	if (*p != ']') {
		media_dbg(media, "Expected ']'\n");
		*endp = (char *)p;
		return NULL;
	}

	*endp = (char *)p + 1;
	return pad;
}

static int set_format(struct media_pad *pad,
		      struct v4l2_mbus_framefmt *format)
{
	int ret;

	if (format->width == 0 || format->height == 0)
		return 0;

	media_dbg(pad->entity->media,
		  "Setting up format %s %ux%u on pad %s/%u\n",
		  v4l2_subdev_pixelcode_to_string(format->code),
		  format->width, format->height,
		  pad->entity->info.name, pad->index);

	ret = v4l2_subdev_set_format(pad->entity, format, pad->index,
				     V4L2_SUBDEV_FORMAT_ACTIVE);
	if (ret < 0) {
		media_dbg(pad->entity->media,
			  "Unable to set format: %s (%d)\n",
			  strerror(-ret), ret);
		return ret;
	}

	media_dbg(pad->entity->media,
		  "Format set: %s %ux%u\n",
		  v4l2_subdev_pixelcode_to_string(format->code),
		  format->width, format->height);

	return 0;
}

static int set_selection(struct media_pad *pad, unsigned int target,
			 struct v4l2_rect *rect)
{
	int ret;

	if (rect->left == -1 || rect->top == -1)
		return 0;

	media_dbg(pad->entity->media,
		  "Setting up selection target %u rectangle (%u,%u)/%ux%u on pad %s/%u\n",
		  target, rect->left, rect->top, rect->width, rect->height,
		  pad->entity->info.name, pad->index);

	ret = v4l2_subdev_set_selection(pad->entity, rect, pad->index,
					target, V4L2_SUBDEV_FORMAT_ACTIVE);
	if (ret < 0) {
		media_dbg(pad->entity->media,
			  "Unable to set selection rectangle: %s (%d)\n",
			  strerror(-ret), ret);
		return ret;
	}

	media_dbg(pad->entity->media,
		  "Selection rectangle set: (%u,%u)/%ux%u\n",
		  rect->left, rect->top, rect->width, rect->height);

	return 0;
}

static int set_frame_interval(struct media_entity *entity,
			      struct v4l2_fract *interval)
{
	int ret;

	if (interval->numerator == 0)
		return 0;

	media_dbg(entity->media,
		  "Setting up frame interval %u/%u on entity %s\n",
		  interval->numerator, interval->denominator,
		  entity->info.name);

	ret = v4l2_subdev_set_frame_interval(entity, interval);
	if (ret < 0) {
		media_dbg(entity->media,
			  "Unable to set frame interval: %s (%d)",
			  strerror(-ret), ret);
		return ret;
	}

	media_dbg(entity->media, "Frame interval set: %u/%u\n",
		  interval->numerator, interval->denominator);

	return 0;
}


static int v4l2_subdev_parse_setup_format(struct media_device *media,
					  const char *p, char **endp)
{
	struct v4l2_mbus_framefmt format = { 0, 0, 0 };
	struct media_pad *pad;
	struct v4l2_rect crop = { -1, -1, -1, -1 };
	struct v4l2_rect compose = crop;
	struct v4l2_fract interval = { 0, 0 };
	unsigned int i;
	char *end;
	int ret;

	pad = v4l2_subdev_parse_pad_format(media, &format, &crop, &compose,
					   &interval, p, &end);
	if (pad == NULL) {
		media_print_streampos(media, p, end);
		media_dbg(media, "Unable to parse format\n");
		return -EINVAL;
	}

	if (pad->flags & MEDIA_PAD_FL_SINK) {
		ret = set_format(pad, &format);
		if (ret < 0)
			return ret;
	}

	ret = set_selection(pad, V4L2_SEL_TGT_CROP, &crop);
	if (ret < 0)
		return ret;

	ret = set_selection(pad, V4L2_SEL_TGT_COMPOSE, &compose);
	if (ret < 0)
		return ret;

	if (pad->flags & MEDIA_PAD_FL_SOURCE) {
		ret = set_format(pad, &format);
		if (ret < 0)
			return ret;
	}

	ret = set_frame_interval(pad->entity, &interval);
	if (ret < 0)
		return ret;


	/* If the pad is an output pad, automatically set the same format on
	 * the remote subdev input pads, if any.
	 */
	if (pad->flags & MEDIA_PAD_FL_SOURCE) {
		for (i = 0; i < pad->entity->num_links; ++i) {
			struct media_link *link = &pad->entity->links[i];
			struct v4l2_mbus_framefmt remote_format;

			if (!(link->flags & MEDIA_LNK_FL_ENABLED))
				continue;

			if (link->source == pad &&
			    link->sink->entity->info.type == MEDIA_ENT_T_V4L2_SUBDEV) {
				remote_format = format;
				set_format(link->sink, &remote_format);
			}
		}
	}

	*endp = end;
	return 0;
}

int v4l2_subdev_parse_setup_formats(struct media_device *media, const char *p)
{
	char *end;
	int ret;

	do {
		ret = v4l2_subdev_parse_setup_format(media, p, &end);
		if (ret < 0)
			return ret;

		for (; isspace(*end); end++);
		p = end + 1;
	} while (*end == ',');

	return *end ? -EINVAL : 0;
}

static const struct {
	const char *name;
	enum v4l2_mbus_pixelcode code;
} mbus_formats[] = {
#include "media-bus-format-names.h"
	{ "Y8", MEDIA_BUS_FMT_Y8_1X8},
	{ "Y10", MEDIA_BUS_FMT_Y10_1X10 },
	{ "Y12", MEDIA_BUS_FMT_Y12_1X12 },
	{ "YUYV", MEDIA_BUS_FMT_YUYV8_1X16 },
	{ "YUYV1_5X8", MEDIA_BUS_FMT_YUYV8_1_5X8 },
	{ "YUYV2X8", MEDIA_BUS_FMT_YUYV8_2X8 },
	{ "UYVY", MEDIA_BUS_FMT_UYVY8_1X16 },
	{ "UYVY1_5X8", MEDIA_BUS_FMT_UYVY8_1_5X8 },
	{ "UYVY2X8", MEDIA_BUS_FMT_UYVY8_2X8 },
	{ "VUY24", MEDIA_BUS_FMT_VUY8_1X24 },
	{ "SBGGR8", MEDIA_BUS_FMT_SBGGR8_1X8 },
	{ "SGBRG8", MEDIA_BUS_FMT_SGBRG8_1X8 },
	{ "SGRBG8", MEDIA_BUS_FMT_SGRBG8_1X8 },
	{ "SRGGB8", MEDIA_BUS_FMT_SRGGB8_1X8 },
	{ "SBGGR10", MEDIA_BUS_FMT_SBGGR10_1X10 },
	{ "SGBRG10", MEDIA_BUS_FMT_SGBRG10_1X10 },
	{ "SGRBG10", MEDIA_BUS_FMT_SGRBG10_1X10 },
	{ "SRGGB10", MEDIA_BUS_FMT_SRGGB10_1X10 },
	{ "SBGGR10_DPCM8", MEDIA_BUS_FMT_SBGGR10_DPCM8_1X8 },
	{ "SGBRG10_DPCM8", MEDIA_BUS_FMT_SGBRG10_DPCM8_1X8 },
	{ "SGRBG10_DPCM8", MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8 },
	{ "SRGGB10_DPCM8", MEDIA_BUS_FMT_SRGGB10_DPCM8_1X8 },
	{ "SBGGR12", MEDIA_BUS_FMT_SBGGR12_1X12 },
	{ "SGBRG12", MEDIA_BUS_FMT_SGBRG12_1X12 },
	{ "SGRBG12", MEDIA_BUS_FMT_SGRBG12_1X12 },
	{ "SRGGB12", MEDIA_BUS_FMT_SRGGB12_1X12 },
	{ "AYUV32", MEDIA_BUS_FMT_AYUV8_1X32 },
	{ "RBG24", MEDIA_BUS_FMT_RBG888_1X24 },
	{ "RGB32", MEDIA_BUS_FMT_RGB888_1X32_PADHI },
	{ "ARGB32", MEDIA_BUS_FMT_ARGB8888_1X32 },
};

const char *v4l2_subdev_pixelcode_to_string(enum v4l2_mbus_pixelcode code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mbus_formats); ++i) {
		if (mbus_formats[i].code == code)
			return mbus_formats[i].name;
	}

	return "unknown";
}

enum v4l2_mbus_pixelcode v4l2_subdev_string_to_pixelcode(const char *string)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mbus_formats); ++i) {
		if (strcmp(mbus_formats[i].name, string) == 0)
			return mbus_formats[i].code;
	}

	return (enum v4l2_mbus_pixelcode)-1;
}

static struct {
	const char *name;
	enum v4l2_field field;
} fields[] = {
	{ "any", V4L2_FIELD_ANY },
	{ "none", V4L2_FIELD_NONE },
	{ "top", V4L2_FIELD_TOP },
	{ "bottom", V4L2_FIELD_BOTTOM },
	{ "interlaced", V4L2_FIELD_INTERLACED },
	{ "seq-tb", V4L2_FIELD_SEQ_TB },
	{ "seq-bt", V4L2_FIELD_SEQ_BT },
	{ "alternate", V4L2_FIELD_ALTERNATE },
	{ "interlaced-tb", V4L2_FIELD_INTERLACED_TB },
	{ "interlaced-bt", V4L2_FIELD_INTERLACED_BT },
};

const char *v4l2_subdev_field_to_string(enum v4l2_field field)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(fields); ++i) {
		if (fields[i].field == field)
			return fields[i].name;
	}

	return "unknown";
}

enum v4l2_field v4l2_subdev_string_to_field(const char *string)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(fields); ++i) {
		if (strcasecmp(fields[i].name, string) == 0)
			return fields[i].field;
	}

	return (enum v4l2_field)-1;
}

static const enum v4l2_mbus_pixelcode mbus_codes[] = {
#include "media-bus-format-codes.h"
};

const enum v4l2_mbus_pixelcode *v4l2_subdev_pixelcode_list(unsigned int *length)
{
	*length = ARRAY_SIZE(mbus_codes);

	return mbus_codes;
}
