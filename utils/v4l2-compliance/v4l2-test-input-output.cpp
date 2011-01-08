/*
    V4L2 API compliance input/output ioctl tests.

    Copyright (C) 2011  Hans Verkuil <hverkuil@xs4all.nl>

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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "v4l2-compliance.h"

#define MAGIC 0x1eadbeef

static int validInput(const struct v4l2_input *descr, unsigned i, unsigned max_audio)
{
	__u32 mask = (1 << max_audio) - 1;

	if (descr->index != i)
		return fail("invalid index\n");
	if (check_ustring(descr->name, sizeof(descr->name)))
		return fail("invalid name\n");
	if (descr->type != V4L2_INPUT_TYPE_TUNER && descr->type != V4L2_INPUT_TYPE_CAMERA)
		return fail("invalid type\n");
	if (descr->type == V4L2_INPUT_TYPE_CAMERA && descr->tuner)
		return fail("invalid tuner\n");
	if (!(descr->capabilities & V4L2_IN_CAP_STD) && descr->std)
		return fail("invalid std\n");
	if ((descr->capabilities & V4L2_IN_CAP_STD) && !descr->std)
		return fail("std == 0\n");
	if (descr->capabilities & ~0x7)
		return fail("invalid capabilities\n");
	if (check_0(descr->reserved, sizeof(descr->reserved)))
		return fail("non-zero reserved fields\n");
	if (descr->status & ~0x07070337)
		return fail("invalid status\n");
	if (descr->status & 0x02070000)
		return fail("use of deprecated digital video status\n");
	if (descr->audioset & ~mask)
		return fail("invalid audioset\n");
	return 0;
}

int testInput(struct node *node)
{
	struct v4l2_input descr;
	struct v4l2_audio audio;
	int cur_input = MAGIC;
	int input;
	int ret = doioctl(node, VIDIOC_G_INPUT, &cur_input, "VIDIOC_G_INPUT");
	int i = 0;
	unsigned a;

	if (ret == EINVAL)
		return -ENOSYS;
	if (ret)
		return ret;
	if (cur_input == MAGIC)
		return fail("VIDIOC_G_INPUT didn't fill in the input\n");
	for (;;) {
		memset(&descr, 0xff, sizeof(descr));
		descr.index = i;
		ret = doioctl(node, VIDIOC_ENUMINPUT, &descr, "VIDIOC_ENUMINPUT");
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("could not enumerate input %d\n", i);
		input = i;
		if (doioctl(node, VIDIOC_S_INPUT, &input, "VIDIOC_S_INPUT"))
			return fail("could not set input to %d\n", i);
		if (input != i)
			return fail("input set to %d, but becomes %d?!\n", i, input);
		if (validInput(&descr, i, node->audio_inputs))
			return fail("invalid attributes for input %d\n", i);
		for (a = 0; a <= node->audio_inputs; a++) {
			memset(&audio, 0, sizeof(audio));
			audio.index = a;
			ret = doioctl(node, VIDIOC_S_AUDIO, &audio, "VIDIOC_S_AUDIO");
			if (ret && (descr.audioset & (1 << a)))
				return fail("could not set audio input to %d for video input %d\n", a, i);
			if (ret != EINVAL && !(descr.audioset & (1 << a)))
				return fail("could set invalid audio input %d for video input %d\n", a, i);
		}
		i++;
	}
	input = i;
	if (doioctl(node, VIDIOC_S_INPUT, &input, "VIDIOC_S_INPUT") != EINVAL)
		return fail("could set input to invalid input %d\n", i);
	if (doioctl(node, VIDIOC_S_INPUT, &cur_input, "VIDIOC_S_INPUT"))
		return fail("couldn't set input to the original input %d\n", cur_input);
	return 0;
}

static int validInputAudio(const struct v4l2_audio *descr, unsigned i)
{
	if (descr->index != i)
		return fail("invalid index\n");
	if (check_ustring(descr->name, sizeof(descr->name)))
		return fail("invalid name\n");
	if (descr->capability & ~0x3)
		return fail("invalid capabilities\n");
	if (descr->mode != 0 && descr->mode != V4L2_AUDMODE_AVL)
		return fail("invalid mode\n");
	if (!(descr->capability & V4L2_AUDCAP_AVL) && descr->mode)
		return fail("mode != 0\n");
	if (check_0(descr->reserved, sizeof(descr->reserved)))
		return fail("non-zero reserved fields\n");
	return 0;
}

int testInputAudio(struct node *node)
{
	struct v4l2_audio input;
	int i = 0;
	int ret;

	for (;;) {
		memset(&input, 0xff, sizeof(input));
		input.index = i;

		ret = doioctl(node, VIDIOC_ENUMAUDIO, &input, "VIDIOC_ENUMAUDIO");
		if (i == 0 && ret == EINVAL)
			return -ENOSYS;
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("could not enumerate audio input %d\n", i);
		if (validInputAudio(&input, i))
			return fail("invalid attributes for audio input %d\n", i);
		node->audio_inputs++;
		i++;
	}
	memset(&input, 0xff, sizeof(input));
	memset(input.reserved, 0, sizeof(input.reserved));
	input.index = i;
	input.mode = 0;
	if (doioctl(node, VIDIOC_S_AUDIO, &input, "VIDIOC_S_AUDIO") != EINVAL)
		return fail("can set invalid audio input\n");
	memset(&input, 0xff, sizeof(input));
	ret = doioctl(node, VIDIOC_G_AUDIO, &input, "VIDIOC_G_AUDIO");
	if (i == 0) {
	       	if (ret != EINVAL)
			return fail("can get current audio input, but no inputs enumerated\n");
		return 0;
	}
	if (ret)
		return fail("cannot get current audio input\n");
	if (input.index >= node->audio_inputs)
		return fail("invalid current audio input %d\n", input.index);
	if (validInputAudio(&input, input.index))
		return fail("invalid attributes for audio input %d\n", input.index);
	return 0;
}

static int validOutput(const struct v4l2_output *descr, unsigned o, unsigned max_audio)
{
	__u32 mask = (1 << max_audio) - 1;

	if (descr->index != o)
		return fail("invalid index\n");
	if (check_ustring(descr->name, sizeof(descr->name)))
		return fail("invalid name\n");
	if (descr->type != V4L2_OUTPUT_TYPE_MODULATOR && descr->type != V4L2_OUTPUT_TYPE_ANALOG)
		return fail("invalid type\n");
	if (descr->type == V4L2_OUTPUT_TYPE_ANALOG && descr->modulator)
		return fail("invalid modulator\n");
	if (!(descr->capabilities & V4L2_OUT_CAP_STD) && descr->std)
		return fail("invalid std\n");
	if ((descr->capabilities & V4L2_OUT_CAP_STD) && !descr->std)
		return fail("std == 0\n");
	if (descr->capabilities & ~0x7)
		return fail("invalid capabilities\n");
	if (check_0(descr->reserved, sizeof(descr->reserved)))
		return fail("non-zero reserved fields\n");
	if (descr->audioset & ~mask)
		return fail("invalid audioset\n");
	return 0;
}

int testOutput(struct node *node)
{
	struct v4l2_output descr;
	struct v4l2_audioout audio;
	int cur_output = MAGIC;
	int output;
	int ret = doioctl(node, VIDIOC_G_OUTPUT, &cur_output, "VIDIOC_G_OUTPUT");
	int o = 0;
	unsigned a;

	if (ret == EINVAL)
		return -ENOSYS;
	if (ret)
		return ret;
	if (cur_output == MAGIC)
		return fail("VIDIOC_G_OUTPUT didn't fill in the output\n");
	for (;;) {
		memset(&descr, 0xff, sizeof(descr));
		descr.index = o;
		ret = doioctl(node, VIDIOC_ENUMOUTPUT, &descr, "VIDIOC_ENUMOUTPUT");
		if (ret)
			break;
		output = o;
		if (doioctl(node, VIDIOC_S_OUTPUT, &output, "VIDIOC_S_OUTPUT"))
			return fail("could not set output to %d\n", o);
		if (output != o)
			return fail("output set to %d, but becomes %d?!\n", o, output);
		if (validOutput(&descr, o, node->audio_outputs))
			return fail("invalid attributes for output %d\n", o);
		for (a = 0; a <= node->audio_outputs; a++) {
			memset(&audio, 0, sizeof(audio));
			audio.index = a;
			ret = doioctl(node, VIDIOC_S_AUDOUT, &audio, "VIDIOC_S_AUDOUT");
			if (ret && (descr.audioset & (1 << a)))
				return fail("could not set audio output to %d for video output %d\n", a, o);
			if (ret != EINVAL && !(descr.audioset & (1 << a)))
				return fail("could set invalid audio output %d for video output %d\n", a, o);
		}
		o++;
	}
	output = o;
	if (doioctl(node, VIDIOC_S_OUTPUT, &output, "VIDIOC_S_OUTPUT") != EINVAL)
		return fail("could set output to invalid output %d\n", o);
	if (doioctl(node, VIDIOC_S_OUTPUT, &cur_output, "VIDIOC_S_OUTPUT"))
		return fail("couldn't set output to the original output %d\n", cur_output);
	return 0;
}

static int validOutputAudio(const struct v4l2_audioout *descr, unsigned o)
{
	if (descr->index != o)
		return fail("invalid index\n");
	if (check_ustring(descr->name, sizeof(descr->name)))
		return fail("invalid name\n");
	if (descr->capability)
		return fail("invalid capabilities\n");
	if (descr->mode)
		return fail("invalid mode\n");
	if (check_0(descr->reserved, sizeof(descr->reserved)))
		return fail("non-zero reserved fields\n");
	return 0;
}

int testOutputAudio(struct node *node)
{
	struct v4l2_audioout output;
	int o = 0;
	int ret;

	for (;;) {
		memset(&output, 0xff, sizeof(output));
		output.index = o;

		ret = doioctl(node, VIDIOC_ENUMAUDOUT, &output, "VIDIOC_ENUMAUDOUT");
		if (o == 0 && ret == EINVAL)
			return -ENOSYS;
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("could not enumerate audio output %d\n", o);
		if (validOutputAudio(&output, o))
			return fail("invalid attributes for audio output %d\n", o);
		node->audio_outputs++;
		o++;
	}
	memset(&output, 0xff, sizeof(output));
	memset(output.reserved, 0, sizeof(output.reserved));
	output.index = o;
	output.mode = 0;
	if (doioctl(node, VIDIOC_S_AUDOUT, &output, "VIDIOC_S_AUDOUT") != EINVAL)
		return fail("can set invalid audio output\n");
	memset(&output, 0xff, sizeof(output));
	ret = doioctl(node, VIDIOC_G_AUDOUT, &output, "VIDIOC_G_AUDOUT");
	if (o == 0) {
	       	if (ret != EINVAL)
			return fail("can get current audio output, but no outputs enumerated\n");
		return 0;
	}
	if (ret)
		return fail("cannot get current audio output\n");
	if (output.index >= node->audio_outputs)
		return fail("invalid current audio output %d\n", output.index);
	if (validOutputAudio(&output, output.index))
		return fail("invalid attributes for audio output %d\n", output.index);
	return 0;
}
