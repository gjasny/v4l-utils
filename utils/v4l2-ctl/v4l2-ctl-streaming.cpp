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
#include <sys/mman.h>
#include <dirent.h>
#include <math.h>
#include <config.h>

#include <linux/videodev2.h>
#include <libv4l2.h>
#include <string>

#include "v4l2-ctl.h"

static unsigned stream_count;
static unsigned stream_skip;
static unsigned reqbufs_count = 3;
static char *file;

void streaming_usage(void)
{
	printf("\nVideo Streaming options:\n"
	       "  --stream-count=<count>\n"
	       "                     stream <count> buffers. The default is to keep streaming\n"
	       "                     forever. This count does not include the number of initial\n"
	       "                     skipped buffers as is passed by --stream-skip.\n"
	       "  --stream-skip=<count>\n"
	       "                     skip the first <count> buffers. The default is 0.\n"
	       "  --stream-to=<file> stream to this file. The default is to discard the\n"
	       "                     data. If <file> is '-', then the data is written to stdout\n"
	       "                     and the --silent option is turned on automatically.\n"
	       "  --stream-mmap=<count>\n"
	       "                     capture video using mmap() [VIDIOC_(D)QBUF]\n"
	       "                     count: the number of buffers to allocate. The default is 3.\n"
	       "  --stream-user=<count>\n"
	       "                     capture video using user pointers [VIDIOC_(D)QBUF]\n"
	       "                     count: the number of buffers to allocate. The default is 3.\n"
	       "  --list-buffers     list all video buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-mplane\n"
	       "                     list all multi-planar video buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-out list all video output buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-out-mplane\n"
	       "                     list all multi-planar video output buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-vbi list all VBI buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-vbi-out\n"
	       "                     list all VBI output buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-sliced-vbi\n"
	       "                     list all sliced VBI buffers [VIDIOC_QUERYBUF]\n"
	       "  --list-buffers-sliced-vbi-out\n"
	       "                     list all sliced VBI output buffers [VIDIOC_QUERYBUF]\n"
	       );
}

static const flag_def flags_def[] = {
	{ V4L2_BUF_FLAG_MAPPED, "mapped" },
	{ V4L2_BUF_FLAG_QUEUED, "queued" },
	{ V4L2_BUF_FLAG_DONE, "done" },
	{ V4L2_BUF_FLAG_KEYFRAME, "keyframe" },
	{ V4L2_BUF_FLAG_PFRAME, "P-frame" },
	{ V4L2_BUF_FLAG_BFRAME, "B-frame" },
	{ V4L2_BUF_FLAG_ERROR, "error" },
	{ V4L2_BUF_FLAG_TIMECODE, "timecode" },
	{ V4L2_BUF_FLAG_INPUT, "input" },
	{ V4L2_BUF_FLAG_PREPARED, "prepared" },
	{ V4L2_BUF_FLAG_NO_CACHE_INVALIDATE, "no-cache-invalidate" },
	{ V4L2_BUF_FLAG_NO_CACHE_CLEAN, "no-cache-clean" },
	{ 0, NULL }
};

static const flag_def tc_flags_def[] = {
	{ V4L2_TC_FLAG_DROPFRAME, "dropframe" },
	{ V4L2_TC_FLAG_COLORFRAME, "colorframe" },
	{ V4L2_TC_USERBITS_field, "userbits-field" },
	{ V4L2_TC_USERBITS_USERDEFINED, "userbits-userdefined" },
	{ V4L2_TC_USERBITS_8BITCHARS, "userbits-8bitchars" },
	{ 0, NULL }
};

static void print_buffer(struct v4l2_buffer &buf)
{
	printf("\tIndex    : %d\n", buf.index);
	printf("\tType     : %s\n", buftype2s(buf.type).c_str());
	printf("\tFlags    : %s\n", flags2s(buf.flags, flags_def).c_str());
	printf("\tField    : %s\n", field2s(buf.field).c_str());
	printf("\tSequence : %u\n", buf.sequence);
	printf("\tLength   : %u\n", buf.length);
	printf("\tBytesused: %u\n", buf.bytesused);
	printf("\tTimestamp: %lu.%06lus\n", buf.timestamp.tv_sec, buf.timestamp.tv_usec);
	if (buf.flags & V4L2_BUF_FLAG_TIMECODE) {
		static const int fps_types[] = { 0, 24, 25, 30, 50, 60 };
		int fps = buf.timecode.type;

		if (fps > 5)
			fps = 0;
		printf("\tTimecode : %dfps %s %dh %dm %ds %df (0x%02x 0x%02x 0x%02x 0x%02x)\n",
			fps_types[fps],
			flags2s(buf.timecode.flags, tc_flags_def).c_str(),
			buf.timecode.hours, buf.timecode.minutes,
			buf.timecode.seconds, buf.timecode.frames,
			buf.timecode.userbits[0], buf.timecode.userbits[1],
			buf.timecode.userbits[2], buf.timecode.userbits[3]);
	}
	if (buf.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
	    buf.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		for (unsigned i = 0; i < buf.length; i++) {
			struct v4l2_plane *p = buf.m.planes + i;

			printf("\tPlane    : %d\n", i);
			printf("\t\tLength     : %u\n", p->length);
			printf("\t\tBytesused  : %u\n", p->bytesused);
			printf("\t\tData Offset: %u\n", p->data_offset);
		}
	}
			
	printf("\n");
}

static void list_buffers(int fd, unsigned buftype)
{
	int i;

	for (i = 0; i < VIDEO_MAX_FRAME; i++) {
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		struct v4l2_buffer buf;

		buf.type = buftype;
		buf.index = i;
		buf.reserved = 0;
		if (buftype == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
		    buftype == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			buf.m.planes = planes;
			memset(planes, 0, sizeof(planes));
		}
		if (test_ioctl(fd, VIDIOC_QUERYBUF, &buf))
			break;
		if (i == 0)
			printf("VIDIOC_QUERYBUF:\n");
		print_buffer(buf);
	}
}

void streaming_cmd(int ch, char *optarg)
{
	switch (ch) {
	case OptStreamCount:
		stream_count = strtoul(optarg, 0L, 0);
		break;
	case OptStreamSkip:
		stream_skip = strtoul(optarg, 0L, 0);
		break;
	case OptStreamTo:
		file = optarg;
		if (strcmp(file, "-"))
			options[OptSilent] = true;
		break;
	case OptStreamMmap:
	case OptStreamUser:
		if (optarg) {
			reqbufs_count = strtoul(optarg, 0L, 0);
			if (reqbufs_count == 0)
				reqbufs_count = 3;
		}
		break;
	}
}

void streaming_set(int fd)
{
	if (options[OptStreamMmap] || options[OptStreamUser]) {
		struct v4l2_requestbuffers reqbufs;
		bool is_mmap = options[OptStreamMmap];
		FILE *fout = NULL;

		memset(&reqbufs, 0, sizeof(reqbufs));
		reqbufs.count = reqbufs_count;
		reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		reqbufs.memory = is_mmap ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;

		if (file) {
			if (!strcmp(file, "-"))
				fout = stdout;
			else
				fout = fopen(file, "w+");
		}

		if (doioctl(fd, VIDIOC_REQBUFS, &reqbufs))
			return;

		void *buffers[reqbufs.count];
		
		for (unsigned i = 0; i < reqbufs.count; i++) {
			struct v4l2_buffer buf;

			memset(&buf, 0, sizeof(buf));
			buf.type = reqbufs.type;
			buf.memory = reqbufs.memory;
			buf.index = i;
			if (doioctl(fd, VIDIOC_QUERYBUF, &buf))
				return;
			if (is_mmap) {
				buffers[i] = mmap(NULL, buf.length,
					PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

				if (buffers[i] == MAP_FAILED) {
					fprintf(stderr, "mmap failed\n");
					return;
				}
			} else {
				buffers[i] = calloc(1, buf.length);
				buf.m.userptr = (unsigned long)buffers[i];
			}
			if (doioctl(fd, VIDIOC_QBUF, &buf))
				return;
		}

		int type = reqbufs.type;
		if (doioctl(fd, VIDIOC_STREAMON, &type))
			return;

		for (;;) {
			struct v4l2_buffer buf;
			int ret;

			memset(&buf, 0, sizeof(buf));
			buf.type = reqbufs.type;
			buf.memory = reqbufs.memory;

			ret = test_ioctl(fd, VIDIOC_DQBUF, &buf);
			if (ret < 0 && errno == EAGAIN)
				continue;
			if (ret < 0) {
				if (!options[OptSilent])
					printf("%s: failed: %s\n", "VIDIOC_DQBUF", strerror(errno));
				return;
			}
			if (fout == NULL) {
				printf(".");
				fflush(stdout);
			} else if (!stream_skip) {
				fwrite(buffers[buf.index], 1, buf.length, fout);
			}
			if (doioctl(fd, VIDIOC_QBUF, &buf))
				return;
			if (stream_skip) {
				stream_skip--;
				continue;
			}
			if (stream_count == 0)
				continue;
			if (--stream_count == 0)
				break;
		}
		doioctl(fd, VIDIOC_STREAMOFF, &type);

		for (unsigned i = 0; i < reqbufs.count; i++) {
			struct v4l2_buffer buf;

			memset(&buf, 0, sizeof(buf));
			buf.type = reqbufs.type;
			buf.memory = reqbufs.memory;
			buf.index = i;
			if (doioctl(fd, VIDIOC_QUERYBUF, &buf))
				return;
			if (is_mmap)
				munmap(buffers[i], buf.length);
			else
				free(buffers[i]);
		}
		if (fout)
			fclose(fout);
	}
}

void streaming_list(int fd)
{
	if (options[OptListBuffers]) {
		list_buffers(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	}

	if (options[OptListBuffersMplane]) {
		list_buffers(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	}

	if (options[OptListBuffersOut]) {
		list_buffers(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	}

	if (options[OptListBuffersMplaneOut]) {
		list_buffers(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	}

	if (options[OptListBuffersVbi]) {
		list_buffers(fd, V4L2_BUF_TYPE_VBI_CAPTURE);
	}

	if (options[OptListBuffersSlicedVbi]) {
		list_buffers(fd, V4L2_BUF_TYPE_SLICED_VBI_CAPTURE);
	}

	if (options[OptListBuffersVbiOut]) {
		list_buffers(fd, V4L2_BUF_TYPE_VBI_OUTPUT);
	}

	if (options[OptListBuffersSlicedVbiOut]) {
		list_buffers(fd, V4L2_BUF_TYPE_SLICED_VBI_OUTPUT);
	}
}
