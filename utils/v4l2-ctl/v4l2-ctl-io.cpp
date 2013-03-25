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
#include <config.h>

#include <linux/videodev2.h>
#include <libv4l2.h>
#include <string>

#include "v4l2-ctl.h"

static struct v4l2_audio vaudio;	/* list audio inputs */
static struct v4l2_audioout vaudout;   	/* audio outputs */
static int input;			/* set_input/get_input */
static int output;			/* set_output/get_output */

void io_usage(void)
{
	printf("\nInput/Output options:\n"
	       "  -I, --get-input    query the video input [VIDIOC_G_INPUT]\n"
	       "  -i, --set-input=<num>\n"
	       "                     set the video input to <num> [VIDIOC_S_INPUT]\n"
	       "  -N, --list-outputs display video outputs [VIDIOC_ENUMOUTPUT]\n"
	       "  -n, --list-inputs  display video inputs [VIDIOC_ENUMINPUT]\n"
	       "  -O, --get-output   query the video output [VIDIOC_G_OUTPUT]\n"
	       "  -o, --set-output=<num>\n"
	       "                     set the video output to <num> [VIDIOC_S_OUTPUT]\n"
	       "  --set-audio-output=<num>\n"
	       "                     set the audio output to <num> [VIDIOC_S_AUDOUT]\n"
	       "  --get-audio-input  query the audio input [VIDIOC_G_AUDIO]\n"
	       "  --set-audio-input=<num>\n"
	       "                     set the audio input to <num> [VIDIOC_S_AUDIO]\n"
	       "  --get-audio-output query the audio output [VIDIOC_G_AUDOUT]\n"
	       "  --set-audio-output=<num>\n"
	       "                     set the audio output to <num> [VIDIOC_S_AUDOUT]\n"
	       "  --list-audio-outputs\n"
	       "                     display audio outputs [VIDIOC_ENUMAUDOUT]\n"
	       "  --list-audio-inputs\n"
	       "                     display audio inputs [VIDIOC_ENUMAUDIO]\n"
	       );
}

static const flag_def in_status_def[] = {
	{ V4L2_IN_ST_NO_POWER,    "no power" },
	{ V4L2_IN_ST_NO_SIGNAL,   "no signal" },
	{ V4L2_IN_ST_NO_COLOR,    "no color" },
	{ V4L2_IN_ST_HFLIP,       "hflip" },
	{ V4L2_IN_ST_VFLIP,       "vflip" },
	{ V4L2_IN_ST_NO_H_LOCK,   "no hsync lock." },
	{ V4L2_IN_ST_COLOR_KILL,  "color kill" },
	{ V4L2_IN_ST_NO_SYNC,     "no sync lock" },
	{ V4L2_IN_ST_NO_EQU,      "no equalizer lock" },
	{ V4L2_IN_ST_NO_CARRIER,  "no carrier" },
	{ V4L2_IN_ST_MACROVISION, "macrovision" },
	{ V4L2_IN_ST_NO_ACCESS,   "no conditional access" },
	{ V4L2_IN_ST_VTR,         "VTR time constant" },
	{ 0, NULL }
};

static std::string status2s(__u32 status)
{
	return status ? flags2s(status, in_status_def) : "ok";
}


static const flag_def input_cap_def[] = {
	{V4L2_IN_CAP_DV_TIMINGS, "DV timings" },
	{V4L2_IN_CAP_STD, "SDTV standards" },
	{ 0, NULL }
};

static std::string input_cap2s(__u32 capabilities)
{
	return capabilities ? flags2s(capabilities, input_cap_def) : "not defined";
}

static const flag_def output_cap_def[] = {
	{V4L2_OUT_CAP_DV_TIMINGS, "DV timings" },
	{V4L2_OUT_CAP_STD, "SDTV standards" },
	{ 0, NULL }
};

static std::string output_cap2s(__u32 capabilities)
{
	return capabilities ? flags2s(capabilities, output_cap_def) : "not defined";
}

void io_cmd(int ch, char *optarg)
{
	switch (ch) {
		case OptSetInput:
			input = strtol(optarg, 0L, 0);
			break;
		case OptSetOutput:
			output = strtol(optarg, 0L, 0);
			break;
		case OptSetAudioInput:
			vaudio.index = strtol(optarg, 0L, 0);
			break;
		case OptSetAudioOutput:
			vaudout.index = strtol(optarg, 0L, 0);
			break;
	}
}

void io_set(int fd)
{
	if (options[OptSetInput]) {
		if (doioctl(fd, VIDIOC_S_INPUT, &input) == 0) {
			struct v4l2_input vin;

			printf("Video input set to %d", input);
			vin.index = input;
			if (test_ioctl(fd, VIDIOC_ENUMINPUT, &vin) >= 0)
				printf(" (%s: %s)", vin.name, status2s(vin.status).c_str());
			printf("\n");
		}
	}

	if (options[OptSetOutput]) {
		if (doioctl(fd, VIDIOC_S_OUTPUT, &output) == 0)
			printf("Output set to %d\n", output);
	}

	if (options[OptSetAudioInput]) {
		if (doioctl(fd, VIDIOC_S_AUDIO, &vaudio) == 0)
			printf("Audio input set to %d\n", vaudio.index);
	}

	if (options[OptSetAudioOutput]) {
		if (doioctl(fd, VIDIOC_S_AUDOUT, &vaudout) == 0)
			printf("Audio output set to %d\n", vaudout.index);
	}
}

void io_get(int fd)
{
	if (options[OptGetInput]) {
		if (doioctl(fd, VIDIOC_G_INPUT, &input) == 0) {
			struct v4l2_input vin;

			printf("Video input : %d", input);
			vin.index = input;
			if (test_ioctl(fd, VIDIOC_ENUMINPUT, &vin) >= 0)
				printf(" (%s: %s)", vin.name, status2s(vin.status).c_str());
			printf("\n");
		}
	}

	if (options[OptGetOutput]) {
		if (doioctl(fd, VIDIOC_G_OUTPUT, &output) == 0) {
			struct v4l2_output vout;

			printf("Video output: %d", output);
			vout.index = output;
			if (test_ioctl(fd, VIDIOC_ENUMOUTPUT, &vout) >= 0) {
				printf(" (%s)", vout.name);
			}
			printf("\n");
		}
	}

	if (options[OptGetAudioInput]) {
		if (doioctl(fd, VIDIOC_G_AUDIO, &vaudio) == 0)
			printf("Audio input : %d (%s)\n", vaudio.index, vaudio.name);
	}

	if (options[OptGetAudioOutput]) {
		if (doioctl(fd, VIDIOC_G_AUDOUT, &vaudout) == 0)
			printf("Audio output: %d (%s)\n", vaudout.index, vaudout.name);
	}
}

void io_list(int fd)
{
	if (options[OptListInputs]) {
		struct v4l2_input vin;

		vin.index = 0;
		printf("ioctl: VIDIOC_ENUMINPUT\n");
		while (test_ioctl(fd, VIDIOC_ENUMINPUT, &vin) >= 0) {
			if (vin.index)
				printf("\n");
			printf("\tInput       : %d\n", vin.index);
			printf("\tName        : %s\n", vin.name);
			printf("\tType        : 0x%08X\n", vin.type);
			printf("\tAudioset    : 0x%08X\n", vin.audioset);
			printf("\tTuner       : 0x%08X\n", vin.tuner);
			printf("\tStandard    : 0x%016llX (%s)\n", (unsigned long long)vin.std,
				std2s(vin.std).c_str());
			printf("\tStatus      : 0x%08X (%s)\n", vin.status, status2s(vin.status).c_str());
			printf("\tCapabilities: 0x%08X (%s)\n", vin.capabilities, input_cap2s(vin.capabilities).c_str());
                        vin.index++;
                }
	}

	if (options[OptListOutputs]) {
		struct v4l2_output vout;

		vout.index = 0;
		printf("ioctl: VIDIOC_ENUMOUTPUT\n");
		while (test_ioctl(fd, VIDIOC_ENUMOUTPUT, &vout) >= 0) {
			if (vout.index)
				printf("\n");
			printf("\tOutput      : %d\n", vout.index);
			printf("\tName        : %s\n", vout.name);
			printf("\tType        : 0x%08X\n", vout.type);
			printf("\tAudioset    : 0x%08X\n", vout.audioset);
			printf("\tStandard    : 0x%016llX (%s)\n", (unsigned long long)vout.std,
					std2s(vout.std).c_str());
			printf("\tCapabilities: 0x%08X (%s)\n", vout.capabilities, output_cap2s(vout.capabilities).c_str());
			vout.index++;
		}
	}

	if (options[OptListAudioInputs]) {
		struct v4l2_audio vaudio;	/* list audio inputs */
		vaudio.index = 0;
		printf("ioctl: VIDIOC_ENUMAUDIO\n");
		while (test_ioctl(fd, VIDIOC_ENUMAUDIO, &vaudio) >= 0) {
			if (vaudio.index)
				printf("\n");
			printf("\tInput   : %d\n", vaudio.index);
			printf("\tName    : %s\n", vaudio.name);
			vaudio.index++;
		}
	}

	if (options[OptListAudioOutputs]) {
		struct v4l2_audioout vaudio;	/* list audio outputs */
		vaudio.index = 0;
		printf("ioctl: VIDIOC_ENUMAUDOUT\n");
		while (test_ioctl(fd, VIDIOC_ENUMAUDOUT, &vaudio) >= 0) {
			if (vaudio.index)
				printf("\n");
			printf("\tOutput  : %d\n", vaudio.index);
			printf("\tName    : %s\n", vaudio.name);
			vaudio.index++;
		}
	}
}
