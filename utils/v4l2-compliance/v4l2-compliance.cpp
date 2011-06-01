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
    Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
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
#include <sys/utsname.h>

#include "v4l2-compliance.h"

/* Short option list

   Please keep in alphabetical order.
   That makes it easier to see which short options are still free.

   In general the lower case is used to set something and the upper
   case is used to retrieve a setting. */
enum Option {
	OptSetDevice = 'd',
	OptHelp = 'h',
	OptSetRadioDevice = 'r',
	OptTest = 't',
	OptTrace = 'T',
	OptVerbose = 'v',
	OptSetVbiDevice = 'V',
	OptUseWrapper = 'w',
	OptLast = 256
};

static char options[OptLast];

static int app_result;
static int tests_total, tests_ok;

// Globals
int verbose;
int wrapper;
int kernel_version;
unsigned caps;
unsigned warnings;

static struct option long_options[] = {
	{"device", required_argument, 0, OptSetDevice},
	{"radio-device", required_argument, 0, OptSetRadioDevice},
	{"vbi-device", required_argument, 0, OptSetVbiDevice},
	{"help", no_argument, 0, OptHelp},
	{"verbose", required_argument, 0, OptVerbose},
	{"trace", no_argument, 0, OptTrace},
	{"wrapper", no_argument, 0, OptUseWrapper},
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
	printf("  -v, --verbose=<level> turn on verbose reporting.\n");
	printf("                     level 1: show warnings\n");
	printf("                     level 2: show warnings and info messages\n");
	printf("  -T, --trace        trace all called ioctls.\n");
	printf("  -w, --wrapper      use the libv4l2 wrapper library.\n");
	exit(0);
}

int doioctl_name(struct node *node, unsigned long int request, void *parm, const char *name)
{
	int retval;
	int e;

	errno = 0;
	retval = test_ioctl(node->fd, request, parm);
	e = errno;
	if (options[OptTrace])
		printf("\t\t%s returned %d (%s)\n", name, retval, strerror(e));
	if (retval == 0)
		return 0;
	if (retval != -1) {
		fail("%s returned %d instead of 0 or -1\n", name, retval);
		return -1;
	}
	return e;
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

	if (res == -ENOSYS) {
		strcpy(buf, "Not Supported");
		res = 0;
	} else {
		strcpy(buf, "OK");
	}
	tests_total++;
	if (res) {
		app_result = res;
		sprintf(buf, "FAIL");
	} else {
		tests_ok++;
	}
	return buf;
}

int check_string(const char *s, size_t len)
{
	size_t sz = strnlen(s, len);

	if (sz == 0)
		return fail("string empty\n");
	if (sz == len)
		return fail("string not 0-terminated\n");
	return 0;
}

int check_ustring(const __u8 *s, int len)
{
	return check_string((const char *)s, len);
}

int check_0(const void *p, int len)
{
	const __u8 *q = (const __u8 *)p;

	while (len--)
		if (*q++)
			return 1;
	return 0;
}

static int testCap(struct node *node)
{
	struct v4l2_capability vcap;
	__u32 caps;

	if (doioctl(node, VIDIOC_QUERYCAP, &vcap))
		return fail("VIDIOC_QUERYCAP not implemented\n");
	if (check_ustring(vcap.driver, sizeof(vcap.driver)))
		return fail("invalid driver name\n");
	if (check_ustring(vcap.card, sizeof(vcap.card)))
		return fail("invalid card name\n");
	if (check_ustring(vcap.bus_info, sizeof(vcap.bus_info))) {
		if (vcap.bus_info[0])
			return fail("invalid bus_info\n");
		else
			warn("VIDIOC_QUERYCAP: empty bus_info\n");
	}
	if (check_0(vcap.reserved, sizeof(vcap.reserved)))
		return fail("non-zero reserved fields\n");
	caps = vcap.capabilities;
	if (caps == 0)
		return fail("no capabilities set\n");
	if (node->is_video && !(caps & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OUTPUT |
				        V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_VIDEO_OUTPUT_OVERLAY)))
		return fail("video node without the relevant capabilities\n");
	if (node->is_radio && !(caps & (V4L2_CAP_RADIO | V4L2_CAP_MODULATOR)))
		return fail("radio node without the relevant capabilities\n");
	if (node->is_radio && (caps & V4L2_CAP_AUDIO))
		return fail("radio node cannot have CAP_AUDIO set\n");
	if (node->is_vbi && !(caps & (V4L2_CAP_VBI_CAPTURE | V4L2_CAP_SLICED_VBI_CAPTURE |
				      V4L2_CAP_VBI_OUTPUT | V4L2_CAP_SLICED_VBI_OUTPUT)))
		return fail("vbi node without the relevant capabilities\n");
	return 0;
}

static int check_prio(struct node *node, struct node *node2, enum v4l2_priority match)
{
	enum v4l2_priority prio;
	int err;

	err = doioctl(node, VIDIOC_G_PRIORITY, &prio);
	if (err)
		return fail("VIDIOC_G_PRIORITY failed\n");
	if (prio != match)
		return fail("wrong priority returned (%d, expected %d)\n", prio, match);
	if (doioctl(node2, VIDIOC_G_PRIORITY, &prio))
		return fail("second VIDIOC_G_PRIORITY failed\n");
	if (prio != match)
		return fail("wrong priority returned on second fh (%d, expected %d)\n", prio, match);
	return 0;
}

static int testPrio(struct node *node, struct node *node2)
{
	enum v4l2_priority prio;
	int err;

	err = check_prio(node, node2, V4L2_PRIORITY_DEFAULT);
	if (err)
		return err;

	prio = V4L2_PRIORITY_RECORD;
	if (doioctl(node, VIDIOC_S_PRIORITY, &prio))
		return fail("VIDIOC_S_PRIORITY RECORD failed\n");
	if (check_prio(node, node2, V4L2_PRIORITY_RECORD))
		return fail("expected priority RECORD");

	prio = V4L2_PRIORITY_INTERACTIVE;
	if (!doioctl(node2, VIDIOC_S_PRIORITY, &prio))
		return fail("Can lower prio on second filehandle\n");
	prio = V4L2_PRIORITY_INTERACTIVE;
	if (doioctl(node, VIDIOC_S_PRIORITY, &prio))
		return fail("Could not lower prio\n");
	if (check_prio(node, node2, V4L2_PRIORITY_INTERACTIVE))
		return fail("expected priority INTERACTIVE");
	return 0;
}

int main(int argc, char **argv)
{
	int i;
	struct node node = { -1 };
	struct node video_node = { -1 };
	struct node video_node2 = { -1 };
	struct node radio_node = { -1, true };
	struct node radio_node2 = { -1, true };
	struct node vbi_node = { -1 };
	struct node vbi_node2 = { -1 };

	/* command args */
	int ch;
	const char *device = NULL;
	const char *video_device = NULL;		/* -d device */
	const char *radio_device = NULL;	/* -r device */
	const char *vbi_device = NULL;		/* -V device */
	struct v4l2_capability vcap;	/* list_cap */
	char short_options[26 * 2 * 2 + 1];
	int idx = 0;

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
		case OptVerbose:
			verbose = atoi(optarg);
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
	wrapper = options[OptUseWrapper];

	struct utsname uts;
	int v1, v2, v3;

	uname(&uts);
	sscanf(uts.release, "%d.%d.%d", &v1, &v2, &v3);
	if (v1 == 2 && v2 == 6)
		kernel_version = v3;

	if (!video_device && !radio_device && !vbi_device)
		video_device = "/dev/video0";

	if (video_device && (video_node.fd = test_open(video_device, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", video_device,
			strerror(errno));
		exit(1);
	}

	if (radio_device && (radio_node.fd = test_open(radio_device, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", radio_device,
			strerror(errno));
		exit(1);
	}

	if (vbi_device && (vbi_node.fd = test_open(vbi_device, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", vbi_device,
			strerror(errno));
		exit(1);
	}

	if (video_node.fd >= 0) {
		node.fd = video_node.fd;
		device = video_device;
		node.is_video = true;
	} else if (radio_node.fd >= 0) {
		node.fd = radio_node.fd;
		device = radio_device;
		node.is_radio = true;
	} else if (vbi_node.fd >= 0) {
		node.fd = vbi_node.fd;
		device = vbi_device;
		node.is_vbi = true;
	}

	doioctl(&node, VIDIOC_QUERYCAP, &vcap);
	node.caps = vcap.capabilities;
	if (node.caps & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VBI_CAPTURE |
			 V4L2_CAP_SLICED_VBI_CAPTURE | V4L2_CAP_RDS_CAPTURE |
			 V4L2_CAP_RADIO | V4L2_CAP_TUNER))
		node.has_inputs = true;
	if (node.caps & (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VBI_OUTPUT |
			 V4L2_CAP_SLICED_VBI_OUTPUT | V4L2_CAP_RDS_OUTPUT |
			 V4L2_CAP_MODULATOR))
		node.has_outputs = true;

	/* Information Opts */

	if (kernel_version)
		printf("Running on 2.6.%d\n\n", kernel_version);

	printf("Driver Info:\n");
	printf("\tDriver name   : %s\n", vcap.driver);
	printf("\tCard type     : %s\n", vcap.card);
	printf("\tBus info      : %s\n", vcap.bus_info);
	printf("\tDriver version: %d.%d.%d\n",
			vcap.version >> 16,
			(vcap.version >> 8) & 0xff,
			vcap.version & 0xff);
	printf("\tCapabilities  : 0x%08X\n", vcap.capabilities);
	printf("%s", cap2s(vcap.capabilities).c_str());

	printf("\nCompliance test for device %s (%susing libv4l2):\n\n",
			device, wrapper ? "" : "not ");

	/* Required ioctls */

	printf("Required ioctls:\n");
	printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(&node)));
	printf("\n");

	/* Multiple opens */

	printf("Allow for multiple opens:\n");
	if (video_device) {
		video_node2 = node;
		printf("\ttest second video open: %s\n",
				ok((video_node2.fd = test_open(video_device, O_RDWR)) < 0));
		if (video_node2.fd >= 0) {
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(&video_node2)));
			printf("\ttest VIDIOC_G/S_PRIORITY: %s\n",
					ok(testPrio(&video_node, &video_node2)));
			test_close(video_node2.fd);
		}
	}
	if (radio_device) {
		radio_node2 = node;
		printf("\ttest second radio open: %s\n",
				ok((radio_node2.fd = test_open(radio_device, O_RDWR)) < 0));
		if (radio_node2.fd >= 0) {
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(&radio_node2)));
			printf("\ttest VIDIOC_G/S_PRIORITY: %s\n",
					ok(testPrio(&radio_node, &radio_node2)));
			test_close(radio_node2.fd);
		}
	}
	if (vbi_device) {
		vbi_node2 = node;
		printf("\ttest second vbi open: %s\n",
				ok((vbi_node2.fd = test_open(vbi_device, O_RDWR)) < 0));
		if (vbi_node2.fd >= 0) {
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(&vbi_node2)));
			printf("\ttest VIDIOC_G/S_PRIORITY: %s\n",
					ok(testPrio(&vbi_node, &vbi_node2)));
			test_close(vbi_node2.fd);
		}
	}
	printf("\n");

	/* Debug ioctls */

	printf("Debug ioctls:\n");
	printf("\ttest VIDIOC_DBG_G_CHIP_IDENT: %s\n", ok(testChipIdent(&node)));
	printf("\ttest VIDIOC_DBG_G/S_REGISTER: %s\n", ok(testRegister(&node)));
	printf("\ttest VIDIOC_LOG_STATUS: %s\n", ok(testLogStatus(&node)));
	printf("\n");

	/* Input ioctls */

	printf("Input ioctls:\n");
	printf("\ttest VIDIOC_G/S_TUNER: %s\n", ok(testTuner(&node)));
	printf("\ttest VIDIOC_G/S_FREQUENCY: %s\n", ok(testTunerFreq(&node)));
	printf("\ttest VIDIOC_ENUMAUDIO: %s\n", ok(testEnumInputAudio(&node)));
	printf("\ttest VIDIOC_G/S/ENUMINPUT: %s\n", ok(testInput(&node)));
	printf("\ttest VIDIOC_G/S_AUDIO: %s\n", ok(testInputAudio(&node)));
	printf("\tInputs: %d Audio Inputs: %d Tuners: %d\n",
			node.inputs, node.audio_inputs, node.tuners);
	printf("\n");

	/* Output ioctls */

	printf("Output ioctls:\n");
	printf("\ttest VIDIOC_G/S_MODULATOR: %s\n", ok(testModulator(&node)));
	printf("\ttest VIDIOC_G/S_FREQUENCY: %s\n", ok(testModulatorFreq(&node)));
	printf("\ttest VIDIOC_ENUMAUDOUT: %s\n", ok(testEnumOutputAudio(&node)));
	printf("\ttest VIDIOC_G/S/ENUMOUTPUT: %s\n", ok(testOutput(&node)));
	printf("\ttest VIDIOC_G/S_AUDOUT: %s\n", ok(testOutputAudio(&node)));
	printf("\tOutputs: %d Audio Outputs: %d Modulators: %d\n",
			node.outputs, node.audio_outputs, node.modulators);
	printf("\n");

	/* Control ioctls */

	printf("Control ioctls:\n");
	printf("\ttest VIDIOC_QUERYCTRL/MENU: %s\n", ok(testQueryControls(&node)));
	printf("\ttest VIDIOC_G/S_CTRL: %s\n", ok(testSimpleControls(&node)));
	printf("\ttest VIDIOC_G/S/TRY_EXT_CTRLS: %s\n", ok(testExtendedControls(&node)));
	printf("\tStandard Controls: %d Private Controls: %d\n",
			node.std_controls, node.priv_controls);
	printf("\n");

	/* I/O configuration ioctls */

	printf("Input/Output configuration ioctls:\n");
	printf("\ttest VIDIOC_ENUM/G/S/QUERY_STD: %s\n", ok(testStd(&node)));
	printf("\ttest VIDIOC_ENUM/G/S/QUERY_DV_PRESETS: %s\n", ok(testPresets(&node)));
	printf("\ttest VIDIOC_G/S_DV_TIMINGS: %s\n", ok(testCustomTimings(&node)));
	printf("\n");

	/* TODO:

	   VIDIOC_CROPCAP, VIDIOC_G/S_CROP
	   VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS
	   VIDIOC_G/S_FBUF/OVERLAY
	   VIDIOC_G/S/TRY_FMT
	   VIDIOC_G/S_PARM
	   VIDIOC_G/S_JPEGCOMP
	   VIDIOC_SLICED_VBI_CAP
	   VIDIOC_S_HW_FREQ_SEEK
	   VIDIOC_SUBSCRIBE_EVENT/UNSUBSCRIBE_EVENT/DQEVENT
	   VIDIOC_(TRY_)ENCODER_CMD
	   VIDIOC_G_ENC_INDEX
	   VIDIOC_REQBUFS/QBUF/DQBUF/QUERYBUF
	   VIDIOC_STREAMON/OFF

	   */

	/* Final test report */

	test_close(node.fd);
	printf("Total: %d Succeeded: %d Failed: %d Warnings: %d\n",
			tests_total, tests_ok, tests_total - tests_ok, warnings);
	exit(app_result);
}
