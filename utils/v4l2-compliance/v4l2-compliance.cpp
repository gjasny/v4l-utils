/*
    V4L2 API compliance test tool.

    Copyright (C) 2008, 2010  Hans Verkuil <hverkuil@xs4all.nl>

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

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
#include <math.h>
#include <sys/klog.h>

#include "v4l2-compliance.h"

/* Short option list

   Please keep in alphabetical order.
   That makes it easier to see which short options are still free.

   In general the lower case is used to set something and the upper
   case is used to retrieve a setting. */
enum Option {
	OptSetDevice = 'd',
	OptSetRadioDevice = 'r',
	OptSetVbiDevice = 'V',
	OptHelp = 'h',
	OptTest = 't',
	OptVerbose = 'v',
	OptLast = 256
};

enum Test {
	TestCap = 0,
	TestChipIdent,
	TestRegister,
	TestLogStatus,
	TestMax
};

static int test[TestMax];

static char options[OptLast];

static int app_result;
static int tests_total, tests_ok;

// Globals
int verbose;
unsigned caps;

static struct option long_options[] = {
	{"device", required_argument, 0, OptSetDevice},
	{"radio-device", required_argument, 0, OptSetRadioDevice},
	{"vbi-device", required_argument, 0, OptSetVbiDevice},
	{"help", no_argument, 0, OptHelp},
	{"verbose", no_argument, 0, OptVerbose},
	{"test", required_argument, 0, OptTest},
	{0, 0, 0, 0}
};

static void usage(void)
{
	printf("Usage:\n");
	printf("Common options:\n");
	printf("  -D, --info         show driver info [VIDIOC_QUERYCAP]\n");
	printf("  -d, --device=<dev> use device <dev> as the video device\n");
	printf("                     if <dev> is a single digit, then /dev/video<dev> is used\n");
	printf("  -r, --radio-device=<dev> use device <dev> as the radio device\n");
	printf("                     if <dev> is a single digit, then /dev/radio<dev> is used\n");
	printf("  -V, --vbi-device=<dev> use device <dev> as the vbi device\n");
	printf("                     if <dev> is a single digit, then /dev/vbi<dev> is used\n");
	printf("  -h, --help         display this help message\n");
	printf("  -t, --test=<num>   run specified test.\n");
	printf("                     By default all tests are run.\n");
	printf("                     0 = test VIDIOC_QUERYCAP\n");
	printf("  -v, --verbose      turn on verbose ioctl error reporting.\n");
	exit(0);
}

int doioctl(int fd, unsigned long int request, void *parm, const char *name)
{
	int retVal;
	int e;

	errno = 0;
	retVal = ioctl(fd, request, parm);
	e = errno;
	if (verbose)
		printf("\t\t%s returned %d (%s)\n", name, retVal, strerror(e));
	if (retVal == 0) return retVal;
	if (retVal != -1) {
		return -1;
	}
	retVal = e;
	return retVal;
}

std::string cap2s(unsigned cap)
{
	std::string s;

	if (cap & V4L2_CAP_VIDEO_CAPTURE)
		s += "\t\tVideo Capture\n";
	if (cap & V4L2_CAP_VIDEO_OUTPUT)
		s += "\t\tVideo Output\n";
	if (cap & V4L2_CAP_VIDEO_OVERLAY)
		s += "\t\tVideo Overlay\n";
	if (cap & V4L2_CAP_VIDEO_OUTPUT_OVERLAY)
		s += "\t\tVideo Output Overlay\n";
	if (cap & V4L2_CAP_VBI_CAPTURE)
		s += "\t\tVBI Capture\n";
	if (cap & V4L2_CAP_VBI_OUTPUT)
		s += "\t\tVBI Output\n";
	if (cap & V4L2_CAP_SLICED_VBI_CAPTURE)
		s += "\t\tSliced VBI Capture\n";
	if (cap & V4L2_CAP_SLICED_VBI_OUTPUT)
		s += "\t\tSliced VBI Output\n";
	if (cap & V4L2_CAP_RDS_CAPTURE)
		s += "\t\tRDS Capture\n";
	if (cap & V4L2_CAP_RDS_OUTPUT)
		s += "\t\tRDS Output\n";
	if (cap & V4L2_CAP_TUNER)
		s += "\t\tTuner\n";
	if (cap & V4L2_CAP_MODULATOR)
		s += "\t\tModulator\n";
	if (cap & V4L2_CAP_AUDIO)
		s += "\t\tAudio\n";
	if (cap & V4L2_CAP_RADIO)
		s += "\t\tRadio\n";
	if (cap & V4L2_CAP_READWRITE)
		s += "\t\tRead/Write\n";
	if (cap & V4L2_CAP_ASYNCIO)
		s += "\t\tAsync I/O\n";
	if (cap & V4L2_CAP_STREAMING)
		s += "\t\tStreaming\n";
	return s;
}

const char *ok(int res)
{
	static char buf[100];

	if (res == ENOSYS) {
		strcpy(buf, "Not Supported");
		res = 0;
	} else {
		strcpy(buf, "OK");
	}
	tests_total++;
	if (res) {
		app_result = res;
		sprintf(buf, "FAIL (%d)\n", res);
	} else {
		tests_ok++;
	}
	return buf;
}

int check_string(const char *s, size_t len, const char *fld)
{
	if (strlen(s) == 0) {
		if (verbose)
			printf("%s field empty\n", fld);
		return -1;
	}
	if (strlen(s) >= len) {
		if (verbose)
			printf("%s field not 0-terminated\n", fld);
		return -2;
	}
	return 0;
}

int check_ustring(const __u8 *s, int len, const char *fld)
{
	return check_string((const char *)s, len, fld);
}

int check_0(void *p, int len)
{
	__u8 *q = (__u8 *)p;

	while (len--)
		if (*q++) {
			if (verbose)
				printf("array not zeroed by driver\n");
			return -1;
		}
	return 0;
}

static int testCap(int fd)
{
	struct v4l2_capability vcap;
	__u32 caps;

	if (doioctl(fd, VIDIOC_QUERYCAP, &vcap, "VIDIOC_QUERYCAP"))
		return -1;
	if (check_ustring(vcap.driver, sizeof(vcap.driver), "driver"))
		return -2;
	if (check_ustring(vcap.card, sizeof(vcap.card), "card"))
		return -3;
	if (check_ustring(vcap.bus_info, sizeof(vcap.bus_info), "bus_info"))
		return -4;
	if (check_0(vcap.reserved, sizeof(vcap.reserved)))
		return -5;
	caps = vcap.capabilities;
	if (caps == 0) {
		if (verbose) printf("no capabilities set\n");
		return -6;
	}
	return 0;
}

static int check_prio(int fd, int fd2, enum v4l2_priority match)
{
	enum v4l2_priority prio;

	if (doioctl(fd, VIDIOC_G_PRIORITY, &prio, "VIDIOC_G_PRIORITY"))
		return -1;
	if (prio != match) {
		if (verbose) printf("wrong priority returned (%d, expected %d)\n", prio, match);
		return -2;
	}
	if (doioctl(fd2, VIDIOC_G_PRIORITY, &prio, "VIDIOC_G_PRIORITY"))
		return -3;
	if (prio != match) {
		if (verbose) printf("wrong priority returned on second fh (%d, expected %d)\n", prio, match);
		return -4;
	}
	return 0;
}

static int testPrio(int fd, int fd2)
{
	enum v4l2_priority prio;

	if (check_prio(fd, fd2, V4L2_PRIORITY_DEFAULT))
		return -1;

	prio = V4L2_PRIORITY_RECORD;
	if (doioctl(fd, VIDIOC_S_PRIORITY, &prio, "VIDIOC_S_PRIORITY"))
		return -2;
	if (check_prio(fd, fd2, V4L2_PRIORITY_RECORD))
		return -3;

	prio = V4L2_PRIORITY_INTERACTIVE;
	if (!doioctl(fd2, VIDIOC_S_PRIORITY, &prio, "VIDIOC_S_PRIORITY")) {
		if (verbose) printf("Can lower prio on second filehandle\n");
		return -4;
	}
	prio = V4L2_PRIORITY_INTERACTIVE;
	if (doioctl(fd, VIDIOC_S_PRIORITY, &prio, "VIDIOC_S_PRIORITY")) {
		if (verbose) printf("Could not lower prio\n");
		return -5;
	}
	
	if (check_prio(fd, fd2, V4L2_PRIORITY_INTERACTIVE))
		return -3;
	return 0;
}

int main(int argc, char **argv)
{
	int i;
	unsigned t;
	int fd = -1;
	int video_fd = -1;
	int video_fd2 = -1;
	int radio_fd = -1;
	int radio_fd2 = -1;
	int vbi_fd = -1;
	int vbi_fd2 = -1;

	/* command args */
	int ch;
	const char *device = NULL;
	const char *video_device = NULL;		/* -d device */
	const char *radio_device = NULL;	/* -r device */
	const char *vbi_device = NULL;		/* -V device */
	struct v4l2_capability vcap;	/* list_cap */
	char short_options[26 * 2 * 2 + 1];
	int idx = 0;
	int tests = 0;

	for (i = 0; long_options[i].name; i++) {
		if (!isalpha(long_options[i].val))
			continue;
		short_options[idx++] = long_options[i].val;
		if (long_options[i].has_arg == required_argument)
			short_options[idx++] = ':';
	}
	while (1) {
		int option_index = 0;

		short_options[idx] = 0;
		ch = getopt_long(argc, argv, short_options,
				 long_options, &option_index);
		if (ch == -1)
			break;

		options[(int)ch] = 1;
		switch (ch) {
		case OptHelp:
			usage();
			return 0;
		case OptTest:
			t = strtoul(optarg, NULL, 0);

			if (t >= TestMax)
				usage();
			test[t] = 1;
			tests++;
			break;
		case OptSetDevice:
			video_device = optarg;
			if (video_device[0] >= '0' && video_device[0] <= '9' && video_device[1] == 0) {
				static char newdev[20];
				char dev = video_device[0];

				sprintf(newdev, "/dev/video%c", dev);
				video_device = newdev;
			}
			break;
		case OptSetRadioDevice:
			radio_device = optarg;
			if (radio_device[0] >= '0' && radio_device[0] <= '9' && radio_device[1] == 0) {
				static char newdev[20];
				char dev = radio_device[0];

				sprintf(newdev, "/dev/radio%c", dev);
				radio_device = newdev;
			}
			break;
		case OptSetVbiDevice:
			vbi_device = optarg;
			if (vbi_device[0] >= '0' && vbi_device[0] <= '9' && vbi_device[1] == 0) {
				static char newdev[20];
				char dev = vbi_device[0];

				sprintf(newdev, "/dev/vbi%c", dev);
				vbi_device = newdev;
			}
			break;
		case ':':
			fprintf(stderr, "Option `%s' requires a value\n",
				argv[optind]);
			usage();
			return 1;
		case '?':
			fprintf(stderr, "Unknown argument `%s'\n",
				argv[optind]);
			usage();
			return 1;
		}
	}
	if (optind < argc) {
		printf("unknown arguments: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
		usage();
		return 1;
	}
	verbose = options[OptVerbose];
	if (!tests) {
		for (t = 0; t < TestMax; t++)
			test[t] = 1;
	}

	if (!video_device && !radio_device && !vbi_device) {
		fprintf(stderr, "No device selected\n");
		usage();
		exit(1);
	}

	if (video_device && (video_fd = open(video_device, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", video_device,
			strerror(errno));
		exit(1);
	}

	if (radio_device && (radio_fd = open(radio_device, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", radio_device,
			strerror(errno));
		exit(1);
	}

	if (vbi_device && (vbi_fd = open(vbi_device, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", vbi_device,
			strerror(errno));
		exit(1);
	}

	if (video_fd >= 0) {
		fd = video_fd;
		device = video_device;
	} else if (radio_fd >= 0) {
		fd = radio_fd;
		device = radio_device;
	} else if (vbi_fd >= 0) {
		fd = vbi_fd;
		device = vbi_device;
	}

	ioctl(fd, VIDIOC_QUERYCAP, &vcap, "VIDIOC_QUERYCAP");
	caps = vcap.capabilities;

	/* Information Opts */

	printf("Driver Info:\n");
	printf("\tDriver name   : %s\n", vcap.driver);
	printf("\tCard type     : %s\n", vcap.card);
	printf("\tBus info      : %s\n", vcap.bus_info);
	printf("\tDriver version: %d\n", vcap.version);
	printf("\tCapabilities  : 0x%08X\n", vcap.capabilities);
	printf("%s", cap2s(vcap.capabilities).c_str());

	printf("\nCompliance test for device %s:\n\n", device);

	printf("Required ioctls:\n");
	if (test[TestCap]) {
		if (video_device)
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(video_fd)));
		if (radio_device)
			printf("\ttest VIDIOC_QUERYCAP for radio: %s\n", ok(testCap(radio_fd)));
		if (vbi_device)
			printf("\ttest VIDIOC_QUERYCAP for vbi: %s\n", ok(testCap(vbi_fd)));
	}

	printf("Allow for multiple opens:\n");
	if (video_device) {
		printf("\ttest second video open: %s\n",
				ok((video_fd2 = open(video_device, O_RDWR)) < 0));
		if (video_fd2 >= 0) {
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(video_fd2)));
			printf("\ttest VIDIOC_S_PRIORITY: %s\n",
					ok(testPrio(video_fd, video_fd2)));
			close(video_fd2);
		}
	}
	if (radio_device) {
		printf("\ttest second radio open: %s\n",
				ok((radio_fd2 = open(radio_device, O_RDWR)) < 0));
		if (radio_fd2 >= 0) {
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(radio_fd2)));
			printf("\ttest VIDIOC_S_PRIORITY: %s\n",
					ok(testPrio(radio_fd, radio_fd2)));
			close(radio_fd2);
		}
	}
	if (vbi_device) {
		printf("\ttest second vbi open: %s\n",
				ok((vbi_fd2 = open(vbi_device, O_RDWR)) < 0));
		if (vbi_fd2 >= 0) {
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(vbi_fd2)));
			printf("\ttest VIDIOC_S_PRIORITY: %s\n",
					ok(testPrio(vbi_fd, vbi_fd2)));
			close(vbi_fd2);
		}
	}

	printf("Debug ioctls:\n");
	if (test[TestChipIdent])
		printf("\ttest VIDIOC_DBG_G_CHIP_IDENT: %s\n", ok(testChipIdent(fd)));
	if (test[TestRegister])
		printf("\ttest VIDIOC_DBG_G/S_REGISTER: %s\n", ok(testRegister(fd)));
	if (test[TestLogStatus])
		printf("\ttest VIDIOC_LOG_STATUS: %s\n", ok(testLogStatus(fd)));

	close(fd);
	printf("Total: %d Succeeded: %d Failed: %d\n",
			tests_total, tests_ok, tests_total - tests_ok);
	exit(app_result);
}
