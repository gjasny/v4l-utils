#include <cstring>

#include "compiler.h"
#include "v4l2-ctl.h"

static struct v4l2_format sliced_fmt;	  /* set_format/get_format for sliced VBI */
static struct v4l2_format sliced_fmt_out; /* set_format/get_format for sliced VBI output */
static struct v4l2_format raw_fmt;	  /* set_format/get_format for VBI */
static struct v4l2_format raw_fmt_out;	  /* set_format/get_format for VBI output */

void vbi_usage()
{
	printf("\nVBI Formats options:\n"
	       "  --get-sliced-vbi-cap\n"
	       "		     query the sliced VBI capture capabilities\n"
	       "                     [VIDIOC_G_SLICED_VBI_CAP]\n"
	       "  --get-sliced-vbi-out-cap\n"
	       "		     query the sliced VBI output capabilities\n"
	       "                     [VIDIOC_G_SLICED_VBI_CAP]\n"
	       "  -B, --get-fmt-sliced-vbi\n"
	       "		     query the sliced VBI capture format [VIDIOC_G_FMT]\n"
	       "  --get-fmt-sliced-vbi-out\n"
	       "		     query the sliced VBI output format [VIDIOC_G_FMT]\n"
	       "  -b, --set-fmt-sliced-vbi\n"
	       "  --try-fmt-sliced-vbi\n"
	       "  --set-fmt-sliced-vbi-out\n"
	       "  --try-fmt-sliced-vbi-out <mode>\n"
	       "                     set/try the sliced VBI capture/output format to <mode>\n"
	       "                     [VIDIOC_S/TRY_FMT], <mode> is a comma separated list of:\n"
	       "                     off:      turn off sliced VBI (cannot be combined with\n"
	       "                               other modes)\n"
	       "                     teletext: teletext (PAL/SECAM)\n"
	       "                     cc:       closed caption (NTSC)\n"
	       "                     wss:      widescreen signal (PAL/SECAM)\n"
	       "                     vps:      VPS (PAL/SECAM)\n"
	       "  --get-fmt-vbi      query the VBI capture format [VIDIOC_G_FMT]\n"
	       "  --get-fmt-vbi-out  query the VBI output format [VIDIOC_G_FMT]\n"
	       "  --set-fmt-vbi\n"
	       "  --try-fmt-vbi\n"
	       "  --set-fmt-vbi-out\n"
	       "  --try-fmt-vbi-out samplingrate=<r>,offset=<o>,samplesperline=<spl>,\n"
	       "                     start0=<s0>,count0=<c0>,start1=<s1>,count1=<c1>\n"
	       "                     set/try the raw VBI capture/output format [VIDIOC_S/TRY_FMT]\n"
	       "                     samplingrate: samples per second\n"
	       "                     offset: horizontal offset in samples\n"
	       "                     samplesperline: samples per line\n"
	       "                     start0: start line number of the first field\n"
	       "                     count0: number of lines in the first field\n"
	       "                     start1: start line number of the second field\n"
	       "                     count1: number of lines in the second field\n"
	       );
}

static void print_sliced_vbi_cap(struct v4l2_sliced_vbi_cap &cap)
{
	printf("\tType           : %s\n", buftype2s(cap.type).c_str());
	printf("\tService Set    : %s\n",
			service2s(cap.service_set).c_str());
	for (int i = 0; i < 24; i++) {
		printf("\tService Line %2d: %8s / %-8s\n", i,
				service2s(cap.service_lines[0][i]).c_str(),
				service2s(cap.service_lines[1][i]).c_str());
	}
}

void vbi_cmd(int ch, char *optarg)
{
	char *value, *subs;
	bool found_off = false;
	v4l2_format *sliced = &sliced_fmt;
	v4l2_format *raw = &raw_fmt;

	switch (ch) {
	case OptSetSlicedVbiOutFormat:
	case OptTrySlicedVbiOutFormat:
		sliced = &sliced_fmt_out;
		fallthrough;
	case OptSetSlicedVbiFormat:
	case OptTrySlicedVbiFormat:
		sliced->fmt.sliced.service_set = 0;
		if (optarg[0] == 0) {
			fprintf(stderr, "empty string\n");
			vbi_usage();
			std::exit(EXIT_FAILURE);
		}
		while (*optarg) {
			subs = std::strchr(optarg, ',');
			if (subs)
				*subs = 0;

			if (!strcmp(optarg, "off"))
				found_off = true;
			else if (!strcmp(optarg, "teletext"))
				sliced->fmt.sliced.service_set |=
					V4L2_SLICED_TELETEXT_B;
			else if (!strcmp(optarg, "cc"))
				sliced->fmt.sliced.service_set |=
					V4L2_SLICED_CAPTION_525;
			else if (!strcmp(optarg, "wss"))
				sliced->fmt.sliced.service_set |=
					V4L2_SLICED_WSS_625;
			else if (!strcmp(optarg, "vps"))
				sliced->fmt.sliced.service_set |=
					V4L2_SLICED_VPS;
			else
				vbi_usage();
			if (subs == nullptr)
				break;
			optarg = subs + 1;
		}
		if (found_off && sliced->fmt.sliced.service_set) {
			fprintf(stderr, "Sliced VBI mode 'off' cannot be combined with other modes\n");
			vbi_usage();
			std::exit(EXIT_FAILURE);
		}
		break;
	case OptSetVbiOutFormat:
	case OptTryVbiOutFormat:
		raw = &raw_fmt_out;
		fallthrough;
	case OptSetVbiFormat:
	case OptTryVbiFormat:
		subs = optarg;
		memset(&raw->fmt.vbi, 0, sizeof(raw->fmt.vbi));
		while (*subs != '\0') {
			static constexpr const char *subopts[] = {
				"samplingrate",
				"offset",
				"samplesperline",
				"start0",
				"start1",
				"count0",
				"count1",
				nullptr
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				raw->fmt.vbi.sampling_rate = strtoul(value, nullptr, 0);
				break;
			case 1:
				raw->fmt.vbi.offset = strtoul(value, nullptr, 0);
				break;
			case 2:
				raw->fmt.vbi.samples_per_line = strtoul(value, nullptr, 0);
				break;
			case 3:
				raw->fmt.vbi.start[0] = strtoul(value, nullptr, 0);
				break;
			case 4:
				raw->fmt.vbi.start[1] = strtoul(value, nullptr, 0);
				break;
			case 5:
				raw->fmt.vbi.count[0] = strtoul(value, nullptr, 0);
				break;
			case 6:
				raw->fmt.vbi.count[1] = strtoul(value, nullptr, 0);
				break;
			default:
				vbi_usage();
				break;
			}
		}
		break;
	}
}

static void fill_raw_vbi(v4l2_vbi_format &dst, const v4l2_vbi_format &src)
{
	if (src.sampling_rate)
		dst.sampling_rate = src.sampling_rate;
	if (src.offset)
		dst.offset = src.offset;
	if (src.samples_per_line)
		dst.samples_per_line = src.samples_per_line;
	if (src.start[0])
		dst.start[0] = src.start[0];
	if (src.start[1])
		dst.start[1] = src.start[1];
	if (src.count[0])
		dst.count[0] = src.count[0];
	if (src.count[1])
		dst.count[1] = src.count[1];
}

void vbi_set(cv4l_fd &_fd)
{
	int fd = _fd.g_fd();
	int ret;

	if (options[OptSetSlicedVbiFormat] || options[OptTrySlicedVbiFormat]) {
		sliced_fmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
		if (options[OptSetSlicedVbiFormat])
			ret = doioctl(fd, VIDIOC_S_FMT, &sliced_fmt);
		else
			ret = doioctl(fd, VIDIOC_TRY_FMT, &sliced_fmt);
		if (ret == 0 && (verbose || options[OptTrySlicedVbiFormat]))
			printfmt(fd, sliced_fmt);
	}

	if (options[OptSetSlicedVbiOutFormat] || options[OptTrySlicedVbiOutFormat]) {
		sliced_fmt_out.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
		if (options[OptSetSlicedVbiOutFormat])
			ret = doioctl(fd, VIDIOC_S_FMT, &sliced_fmt_out);
		else
			ret = doioctl(fd, VIDIOC_TRY_FMT, &sliced_fmt_out);
		if (ret == 0 && (verbose || options[OptTrySlicedVbiOutFormat]))
			printfmt(fd, sliced_fmt_out);
	}

	if (options[OptSetVbiFormat] || options[OptTryVbiFormat]) {
		v4l2_format fmt;

		fmt.type = V4L2_BUF_TYPE_VBI_CAPTURE;
		doioctl(fd, VIDIOC_G_FMT, &fmt);
		fill_raw_vbi(fmt.fmt.vbi, raw_fmt.fmt.vbi);
		if (options[OptSetVbiFormat])
			ret = doioctl(fd, VIDIOC_S_FMT, &fmt);
		else
			ret = doioctl(fd, VIDIOC_TRY_FMT, &fmt);
		if (ret == 0 && (verbose || options[OptTryVbiFormat]))
			printfmt(fd, fmt);
	}

	if (options[OptSetVbiOutFormat] || options[OptTryVbiOutFormat]) {
		v4l2_format fmt;

		fmt.type = V4L2_BUF_TYPE_VBI_OUTPUT;
		doioctl(fd, VIDIOC_G_FMT, &fmt);
		fill_raw_vbi(fmt.fmt.vbi, raw_fmt.fmt.vbi);
		if (options[OptSetVbiOutFormat])
			ret = doioctl(fd, VIDIOC_S_FMT, &fmt);
		else
			ret = doioctl(fd, VIDIOC_TRY_FMT, &fmt);
		if (ret == 0 && (verbose || options[OptTryVbiOutFormat]))
			printfmt(fd, fmt);
	}
}

void vbi_get(cv4l_fd &_fd)
{
	int fd = _fd.g_fd();

	if (options[OptGetSlicedVbiFormat]) {
		sliced_fmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
		if (doioctl(fd, VIDIOC_G_FMT, &sliced_fmt) == 0)
			printfmt(fd, sliced_fmt);
	}

	if (options[OptGetSlicedVbiOutFormat]) {
		sliced_fmt_out.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
		if (doioctl(fd, VIDIOC_G_FMT, &sliced_fmt_out) == 0)
			printfmt(fd, sliced_fmt_out);
	}

	if (options[OptGetVbiFormat]) {
		raw_fmt.type = V4L2_BUF_TYPE_VBI_CAPTURE;
		if (doioctl(fd, VIDIOC_G_FMT, &raw_fmt) == 0)
			printfmt(fd, raw_fmt);
	}

	if (options[OptGetVbiOutFormat]) {
		raw_fmt_out.type = V4L2_BUF_TYPE_VBI_OUTPUT;
		if (doioctl(fd, VIDIOC_G_FMT, &raw_fmt_out) == 0)
			printfmt(fd, raw_fmt_out);
	}
}

void vbi_list(cv4l_fd &fd)
{
	if (options[OptGetSlicedVbiCap]) {
		struct v4l2_sliced_vbi_cap cap;

		cap.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
		if (doioctl(fd.g_fd(), VIDIOC_G_SLICED_VBI_CAP, &cap) == 0) {
			print_sliced_vbi_cap(cap);
		}
	}

	if (options[OptGetSlicedVbiOutCap]) {
		struct v4l2_sliced_vbi_cap cap;

		cap.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
		if (doioctl(fd.g_fd(), VIDIOC_G_SLICED_VBI_CAP, &cap) == 0) {
			print_sliced_vbi_cap(cap);
		}
	}
}
