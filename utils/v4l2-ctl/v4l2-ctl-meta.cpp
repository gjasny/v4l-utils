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

#include "v4l2-ctl.h"

static struct v4l2_format vfmt;	/* set_format/get_format */

void meta_usage(void)
{
	printf("\nMetadata Formats options:\n"
	       "  --list-formats-meta display supported metadata formats [VIDIOC_ENUM_FMT]\n"
	       "  --get-fmt-meta      query the metadata format [VIDIOC_G_FMT]\n"
	       "  --set-fmt-meta <f>  set the metadata format [VIDIOC_S_FMT]\n"
	       "                     parameter is either the format index as reported by\n"
	       "                     --list-formats-meta, or the fourcc value as a string\n"
	       "  --try-fmt-meta <f>  try the metadata format [VIDIOC_TRY_FMT]\n"
	       "                     parameter is either the format index as reported by\n"
	       "                     --list-formats-meta, or the fourcc value as a string\n"
	       );
}

void meta_cmd(int ch, char *optarg)
{
	switch (ch) {
	case OptSetMetaFormat:
	case OptTryMetaFormat:
		if (strlen(optarg) == 0) {
			meta_usage();
			exit(1);
		} else if (strlen(optarg) == 4) {
			vfmt.fmt.meta.dataformat = v4l2_fourcc(optarg[0],
					optarg[1], optarg[2], optarg[3]);
		} else {
			vfmt.fmt.meta.dataformat = strtol(optarg, 0L, 0);
		}
		break;
	}
}

void meta_set(cv4l_fd &_fd)
{
	int fd = _fd.g_fd();
	int ret;

	if ((options[OptSetMetaFormat] || options[OptTryMetaFormat]) &&
	    v4l_type_is_meta(_fd.g_type())) {
		struct v4l2_format in_vfmt;

		in_vfmt.type = _fd.g_type();
		in_vfmt.fmt.meta.dataformat = vfmt.fmt.meta.dataformat;

		if (in_vfmt.fmt.meta.dataformat < 256) {
			struct v4l2_fmtdesc fmt;

			fmt.index = in_vfmt.fmt.meta.dataformat;
			fmt.type = in_vfmt.type;

			if (doioctl(fd, VIDIOC_ENUM_FMT, &fmt))
				fmt.pixelformat = 0;

			in_vfmt.fmt.meta.dataformat = fmt.pixelformat;
		}

		if (options[OptSetMetaFormat])
			ret = doioctl(fd, VIDIOC_S_FMT, &in_vfmt);
		else
			ret = doioctl(fd, VIDIOC_TRY_FMT, &in_vfmt);
		if (ret == 0 && (verbose || options[OptTryMetaFormat]))
			printfmt(fd, in_vfmt);
	}
}

void meta_get(cv4l_fd &fd)
{
	if (options[OptGetMetaFormat] && v4l_type_is_meta(fd.g_type())) {
		vfmt.type = fd.g_type();
		if (doioctl(fd.g_fd(), VIDIOC_G_FMT, &vfmt) == 0)
			printfmt(fd.g_fd(), vfmt);
	}
}

void meta_list(cv4l_fd &fd)
{
	if (options[OptListMetaFormats] && v4l_type_is_meta(fd.g_type())) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, fd.g_type());
	}
}
