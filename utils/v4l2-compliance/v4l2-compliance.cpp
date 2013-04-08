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
	OptNoWarnings = 'n',
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
bool show_info;
bool show_warnings = true;
bool wrapper;
int kernel_version;
unsigned warnings;

static struct option long_options[] = {
	{"device", required_argument, 0, OptSetDevice},
	{"radio-device", required_argument, 0, OptSetRadioDevice},
	{"vbi-device", required_argument, 0, OptSetVbiDevice},
	{"help", no_argument, 0, OptHelp},
	{"verbose", no_argument, 0, OptVerbose},
	{"no-warnings", no_argument, 0, OptNoWarnings},
	{"trace", no_argument, 0, OptTrace},
	{"wrapper", no_argument, 0, OptUseWrapper},
	{0, 0, 0, 0}
};

static void usage(void)
{
	printf("Usage:\n");
	printf("Common options:\n");
	printf("  -d, --device=<dev> use device <dev> as the video device\n");
	printf("                     if <dev> starts with a digit, then /dev/video<dev> is used\n");
	printf("  -r, --radio-device=<dev> use device <dev> as the radio device\n");
	printf("                     if <dev> starts with a digit, then /dev/radio<dev> is used\n");
	printf("  -V, --vbi-device=<dev> use device <dev> as the vbi device\n");
	printf("                     if <dev> starts with a digit, then /dev/vbi<dev> is used\n");
	printf("  -h, --help         display this help message\n");
	printf("  -n, --no-warnings  turn off warning messages.\n");
	printf("  -T, --trace        trace all called ioctls.\n");
	printf("  -v, --verbose      turn on verbose reporting.\n");
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
	if (cap & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		s += "\t\tVideo Capture Multiplanar\n";
	if (cap & V4L2_CAP_VIDEO_OUTPUT)
		s += "\t\tVideo Output\n";
	if (cap & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
		s += "\t\tVideo Output Multiplanar\n";
	if (cap & V4L2_CAP_VIDEO_M2M)
		s += "\t\tVideo Memory-to-Memory\n";
	if (cap & V4L2_CAP_VIDEO_M2M_MPLANE)
		s += "\t\tVideo Memory-to-Memory Multiplanar\n";
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
	if (cap & V4L2_CAP_DEVICE_CAPS)
		s += "\t\tDevice Capabilities\n";
	return s;
}

std::string buftype2s(int type)
{
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		return "Video Capture";
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		return "Video Capture Multiplanar";
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return "Video Output";
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return "Video Output Multiplanar";
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		return "Video Overlay";
	case V4L2_BUF_TYPE_VBI_CAPTURE:
		return "VBI Capture";
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		return "VBI Output";
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
		return "Sliced VBI Capture";
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		return "Sliced VBI Output";
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		return "Video Output Overlay";
	case V4L2_BUF_TYPE_PRIVATE:
		return "Private";
	default:
		return std::string("Unknown");
	}
}

const char *ok(int res)
{
	static char buf[100];

	if (res == ENOTTY) {
		strcpy(buf, "OK (Not Supported)");
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
	__u32 caps, dcaps;
	const __u32 video_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OUTPUT |
			V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE |
			V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_VIDEO_OUTPUT_OVERLAY;
	const __u32 vbi_caps = V4L2_CAP_VBI_CAPTURE | V4L2_CAP_SLICED_VBI_CAPTURE |
			V4L2_CAP_VBI_OUTPUT | V4L2_CAP_SLICED_VBI_OUTPUT;
	const __u32 radio_caps = V4L2_CAP_RADIO | V4L2_CAP_MODULATOR;
	const __u32 input_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OVERLAY |
			V4L2_CAP_VBI_CAPTURE | V4L2_CAP_SLICED_VBI_CAPTURE |
			V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_HW_FREQ_SEEK |
			V4L2_CAP_TUNER;
	const __u32 output_caps = V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			V4L2_CAP_VIDEO_OUTPUT_OVERLAY | V4L2_CAP_VBI_OUTPUT |
			V4L2_CAP_SLICED_VBI_OUTPUT | V4L2_CAP_MODULATOR;
	const __u32 m2m_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE;
	const __u32 io_caps = V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;
	const __u32 mplane_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
		V4L2_CAP_VIDEO_M2M_MPLANE;
	const __u32 splane_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OUTPUT |
		V4L2_CAP_VIDEO_M2M;

	memset(&vcap, 0xff, sizeof(vcap));
	// Must always be there
	fail_on_test(doioctl(node, VIDIOC_QUERYCAP, &vcap));
	fail_on_test(check_ustring(vcap.driver, sizeof(vcap.driver)));
	fail_on_test(check_ustring(vcap.card, sizeof(vcap.card)));
	fail_on_test(check_ustring(vcap.bus_info, sizeof(vcap.bus_info)));
	// Check for valid prefixes
	if (memcmp(vcap.bus_info, "usb-", 4) &&
	    memcmp(vcap.bus_info, "PCI:", 4) &&
	    memcmp(vcap.bus_info, "PCIe:", 5) &&
	    memcmp(vcap.bus_info, "ISA:", 4) &&
	    memcmp(vcap.bus_info, "I2C:", 4) &&
	    memcmp(vcap.bus_info, "parport", 7) &&
	    memcmp(vcap.bus_info, "platform:", 9))
		return fail("missing bus_info prefix ('%s')\n", vcap.bus_info);
	fail_on_test((vcap.version >> 16) < 3);
	fail_on_test(check_0(vcap.reserved, sizeof(vcap.reserved)));
	caps = vcap.capabilities;
	dcaps = vcap.device_caps;
	node->is_m2m = dcaps & m2m_caps;
	fail_on_test(caps == 0);
	fail_on_test(caps & V4L2_CAP_ASYNCIO);
	fail_on_test(!(caps & V4L2_CAP_DEVICE_CAPS));
	fail_on_test(dcaps & V4L2_CAP_DEVICE_CAPS);
	fail_on_test(dcaps & ~caps);
	fail_on_test(!(dcaps & caps));
	fail_on_test(node->is_video && !(dcaps & video_caps));
	fail_on_test(node->is_radio && !(dcaps & radio_caps));
	// V4L2_CAP_AUDIO is invalid for radio
	fail_on_test(node->is_radio && (dcaps & V4L2_CAP_AUDIO));
	fail_on_test(node->is_vbi && !(dcaps & vbi_caps));
	// You can't have both set due to missing buffer type in VIDIOC_G/S_FBUF
	fail_on_test((dcaps & (V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_VIDEO_OUTPUT_OVERLAY)) ==
			(V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_VIDEO_OUTPUT_OVERLAY));
	fail_on_test(node->is_video && (dcaps & (vbi_caps | radio_caps)));
	fail_on_test(node->is_radio && (dcaps & (vbi_caps | video_caps)));
	fail_on_test(node->is_vbi && (dcaps & (video_caps | radio_caps)));
	if (node->is_m2m) {
		// This will become an error as this combination of caps
		// is on the feature removal list.
		if ((dcaps & input_caps) && (dcaps & output_caps))
			warn("VIDIOC_QUERYCAP: m2m with video input and output caps\n");
	} else {
		if (dcaps & input_caps)
			fail_on_test(dcaps & output_caps);
		if (dcaps & output_caps)
			fail_on_test(dcaps & input_caps);
	}
	if (node->can_capture || node->can_output) {
		// whether io_caps need to be set for RDS capture/output is
		// checked elsewhere as that depends on the tuner/modulator
		// capabilities.
		if (!(dcaps & (V4L2_CAP_RDS_CAPTURE | V4L2_CAP_RDS_OUTPUT)))
			fail_on_test(!(dcaps & io_caps));
	} else {
		fail_on_test(dcaps & io_caps);
	}
	// having both mplane and splane caps is not allowed (at least for now)
	fail_on_test((dcaps & mplane_caps) && (dcaps & splane_caps));

	return 0;
}

static int check_prio(struct node *node, struct node *node2, enum v4l2_priority match)
{
	enum v4l2_priority prio;

	// Must be able to get priority
	fail_on_test(doioctl(node, VIDIOC_G_PRIORITY, &prio));
	// Must match the expected prio
	fail_on_test(prio != match);
	fail_on_test(doioctl(node2, VIDIOC_G_PRIORITY, &prio));
	fail_on_test(prio != match);
	return 0;
}

static int testPrio(struct node *node, struct node *node2)
{
	enum v4l2_priority prio;
	int err;

	if (node->is_m2m) {
		fail_on_test(doioctl(node, VIDIOC_G_PRIORITY, &prio) != ENOTTY);
		return 0;
	}
	err = check_prio(node, node2, V4L2_PRIORITY_DEFAULT);
	if (err)
		return err;

	prio = V4L2_PRIORITY_RECORD;
	// Must be able to change priority
	fail_on_test(doioctl(node, VIDIOC_S_PRIORITY, &prio));
	// Must match the new prio
	fail_on_test(check_prio(node, node2, V4L2_PRIORITY_RECORD));

	prio = V4L2_PRIORITY_INTERACTIVE;
	// Going back to interactive on the other node must fail
	fail_on_test(!doioctl(node2, VIDIOC_S_PRIORITY, &prio));
	prio = V4L2_PRIORITY_INTERACTIVE;
	// Changing it on the first node must work.
	fail_on_test(doioctl(node, VIDIOC_S_PRIORITY, &prio));
	fail_on_test(check_prio(node, node2, V4L2_PRIORITY_INTERACTIVE));
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
			if (video_device[0] >= '0' && video_device[0] <= '9' && strlen(video_device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/video%s", video_device);
				video_device = newdev;
			}
			break;
		case OptSetRadioDevice:
			radio_device = optarg;
			if (radio_device[0] >= '0' && radio_device[0] <= '9' && strlen(radio_device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/radio%s", radio_device);
				radio_device = newdev;
			}
			break;
		case OptSetVbiDevice:
			vbi_device = optarg;
			if (vbi_device[0] >= '0' && vbi_device[0] <= '9' && strlen(vbi_device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/vbi%s", vbi_device);
				vbi_device = newdev;
			}
			break;
		case OptNoWarnings:
			show_warnings = false;
			break;
		case OptVerbose:
			show_info = true;
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
		printf("is radio\n");
	} else if (vbi_node.fd >= 0) {
		node.fd = vbi_node.fd;
		device = vbi_device;
		node.is_vbi = true;
	}
	node.device = device;

	doioctl(&node, VIDIOC_QUERYCAP, &vcap);
	if (vcap.capabilities & V4L2_CAP_DEVICE_CAPS)
		node.caps = vcap.device_caps;
	else
		node.caps = vcap.capabilities;
	if (node.caps & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VBI_CAPTURE |
			 V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_SLICED_VBI_CAPTURE |
			 V4L2_CAP_RDS_CAPTURE | V4L2_CAP_RADIO | V4L2_CAP_TUNER))
		node.has_inputs = true;
	if (node.caps & (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VBI_OUTPUT |
			 V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_SLICED_VBI_OUTPUT |
			 V4L2_CAP_RDS_OUTPUT | V4L2_CAP_MODULATOR))
		node.has_outputs = true;
	if (node.caps & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VBI_CAPTURE |
			 V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE |
			 V4L2_CAP_VIDEO_M2M | V4L2_CAP_SLICED_VBI_CAPTURE |
			 V4L2_CAP_RDS_CAPTURE))
		node.can_capture = true;
	if (node.caps & (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VBI_OUTPUT |
			 V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE |
			 V4L2_CAP_VIDEO_M2M | V4L2_CAP_SLICED_VBI_OUTPUT |
			 V4L2_CAP_RDS_OUTPUT))
		node.can_output = true;

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
	if (vcap.capabilities & V4L2_CAP_DEVICE_CAPS) {
		printf("\tDevice Caps   : 0x%08X\n", vcap.device_caps);
		printf("%s", cap2s(vcap.device_caps).c_str());
	}

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
					ok(testPrio(&node, &video_node2)));
			node.node2 = &video_node2;
		}
	}
	if (radio_device) {
		radio_node2 = node;
		printf("\ttest second radio open: %s\n",
				ok((radio_node2.fd = test_open(radio_device, O_RDWR)) < 0));
		if (radio_node2.fd >= 0) {
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(&radio_node2)));
			printf("\ttest VIDIOC_G/S_PRIORITY: %s\n",
					ok(testPrio(&node, &radio_node2)));
			node.node2 = &video_node2;
		}
	}
	if (vbi_device) {
		vbi_node2 = node;
		printf("\ttest second vbi open: %s\n",
				ok((vbi_node2.fd = test_open(vbi_device, O_RDWR)) < 0));
		if (vbi_node2.fd >= 0) {
			printf("\ttest VIDIOC_QUERYCAP: %s\n", ok(testCap(&vbi_node2)));
			printf("\ttest VIDIOC_G/S_PRIORITY: %s\n",
					ok(testPrio(&node, &vbi_node2)));
			node.node2 = &video_node2;
		}
	}
	printf("\n");

	/* Debug ioctls */

	printf("Debug ioctls:\n");
	printf("\ttest VIDIOC_DBG_G/S_REGISTER: %s\n", ok(testRegister(&node)));
	printf("\ttest VIDIOC_LOG_STATUS: %s\n", ok(testLogStatus(&node)));
	printf("\n");

	/* Input ioctls */

	printf("Input ioctls:\n");
	printf("\ttest VIDIOC_G/S_TUNER: %s\n", ok(testTuner(&node)));
	printf("\ttest VIDIOC_G/S_FREQUENCY: %s\n", ok(testTunerFreq(&node)));
	printf("\ttest VIDIOC_S_HW_FREQ_SEEK: %s\n", ok(testTunerHwSeek(&node)));
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
	printf("\ttest VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: %s\n", ok(testControlEvents(&node)));
	printf("\ttest VIDIOC_G/S_JPEGCOMP: %s\n", ok(testJpegComp(&node)));
	printf("\tStandard Controls: %d Private Controls: %d\n",
			node.std_controls, node.priv_controls);
	printf("\n");

	/* I/O configuration ioctls */

	printf("Input/Output configuration ioctls:\n");
	printf("\ttest VIDIOC_ENUM/G/S/QUERY_STD: %s\n", ok(testStd(&node)));
	printf("\ttest VIDIOC_ENUM/G/S/QUERY_DV_TIMINGS: %s\n", ok(testTimings(&node)));
	printf("\ttest VIDIOC_DV_TIMINGS_CAP: %s\n", ok(testTimingsCap(&node)));
	printf("\n");

	/* Format ioctls */

	printf("Format ioctls:\n");
	printf("\ttest VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: %s\n", ok(testEnumFormats(&node)));
	printf("\ttest VIDIOC_G/S_PARM: %s\n", ok(testParm(&node)));
	printf("\ttest VIDIOC_G_FBUF: %s\n", ok(testFBuf(&node)));
	printf("\ttest VIDIOC_G_FMT: %s\n", ok(testGetFormats(&node)));
	printf("\ttest VIDIOC_TRY_FMT: %s\n", ok(testTryFormats(&node)));
	printf("\ttest VIDIOC_S_FMT: %s\n", ok(testSetFormats(&node)));
	printf("\ttest VIDIOC_G_SLICED_VBI_CAP: %s\n", ok(testSlicedVBICap(&node)));
	printf("\n");

	/* Codec ioctls */

	printf("Codec ioctls:\n");
	printf("\ttest VIDIOC_(TRY_)ENCODER_CMD: %s\n", ok(testEncoder(&node)));
	printf("\ttest VIDIOC_G_ENC_INDEX: %s\n", ok(testEncIndex(&node)));
	printf("\ttest VIDIOC_(TRY_)DECODER_CMD: %s\n", ok(testDecoder(&node)));
	printf("\n");

	/* Buffer ioctls */

	printf("Buffer ioctls:\n");
	printf("\ttest VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: %s\n", ok(testReqBufs(&node)));
	//printf("\ttest read/write: %s\n", ok(testReadWrite(&node)));
	printf("\n");

	/* TODO:

	   VIDIOC_CROPCAP, VIDIOC_G/S_CROP, VIDIOC_G/S_SELECTION
	   VIDIOC_S_FBUF/OVERLAY
	   VIDIOC_QBUF/DQBUF/QUERYBUF/PREPARE_BUFS/EXPBUF
	   VIDIOC_STREAMON/OFF
	   */

	/* Final test report */

	test_close(node.fd);
	if (node.node2)
		test_close(node.node2->fd);
	printf("Total: %d, Succeeded: %d, Failed: %d, Warnings: %d\n",
			tests_total, tests_ok, tests_total - tests_ok, warnings);
	exit(app_result);
}
