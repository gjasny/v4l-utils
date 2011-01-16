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

static int validTuner(struct node *node, const struct v4l2_tuner &tuner,
		unsigned t, v4l2_std_id std)
{
	bool valid_modes[5] = { true, false, false, true, false };
	bool tv = !node->is_radio;
	enum v4l2_tuner_type type = tv ? V4L2_TUNER_ANALOG_TV : V4L2_TUNER_RADIO;
	__u32 audmode;

	if (tuner.index != t)
		return fail("invalid index\n");
	if (check_ustring(tuner.name, sizeof(tuner.name)))
		return fail("invalid name\n");
	if (check_0(tuner.reserved, sizeof(tuner.reserved)))
		return fail("non-zero reserved fields\n");
	if (tuner.type != type)
		return fail("invalid tuner type %d\n", tuner.type);
	if (tv && (tuner.capability & V4L2_TUNER_CAP_RDS))
		return fail("RDS for TV tuner?\n");
	if (!tv && (tuner.capability & (V4L2_TUNER_CAP_NORM |
					V4L2_TUNER_CAP_LANG1 | V4L2_TUNER_CAP_LANG2)))
		return fail("TV capabilities for radio tuner?\n");
	if (tuner.rangelow >= tuner.rangehigh)
		return fail("rangelow >= rangehigh\n");
	if (tuner.rangelow == 0 || tuner.rangehigh == 0xffffffff)
		return fail("invalid rangelow or rangehigh\n");
	if (!(tuner.capability & V4L2_TUNER_CAP_STEREO) &&
			(tuner.rxsubchans & V4L2_TUNER_SUB_STEREO))
		return fail("stereo subchan, but no stereo caps?\n");
	if (!(tuner.capability & V4L2_TUNER_CAP_LANG1) &&
			(tuner.rxsubchans & V4L2_TUNER_SUB_LANG1))
		return fail("lang1 subchan, but no lang1 caps?\n");
	if (!(tuner.capability & V4L2_TUNER_CAP_LANG2) &&
			(tuner.rxsubchans & V4L2_TUNER_SUB_LANG2))
		return fail("lang2 subchan, but no lang2 caps?\n");
	if (!(tuner.capability & V4L2_TUNER_CAP_RDS) &&
			(tuner.rxsubchans & V4L2_TUNER_SUB_RDS))
		return fail("RDS subchan, but no RDS caps?\n");
	if (std == V4L2_STD_NTSC_M && (tuner.rxsubchans & V4L2_TUNER_SUB_LANG1))
		return fail("LANG1 subchan, but NTSC-M standard\n");
	if (tuner.audmode > V4L2_TUNER_MODE_LANG1_LANG2)
		return fail("invalid audio mode\n");
	// Ambiguous whether this is allowed or not
	//		if (!tv && tuner.audmode > V4L2_TUNER_MODE_STEREO)
	//			return -16;
	if (tuner.signal > 65535)
		return fail("signal too large\n");
	if (tuner.capability & V4L2_TUNER_CAP_STEREO)
		valid_modes[V4L2_TUNER_MODE_STEREO] = true;
	if (tuner.capability & V4L2_TUNER_CAP_LANG2) {
		valid_modes[V4L2_TUNER_MODE_LANG2] = true;
		valid_modes[V4L2_TUNER_MODE_LANG1_LANG2] = true;
	}
	for (audmode = 0; audmode < 5; audmode++) {
		struct v4l2_tuner tun = { 0 };

		tun.index = tuner.index;
		tun.audmode = audmode;
		if (doioctl(node, VIDIOC_S_TUNER, &tun))
			return fail("cannot set audmode %d\n", audmode);
		if (doioctl(node, VIDIOC_G_TUNER, &tun))
			fail("failure to get new tuner audmode\n");
		if (tun.audmode > V4L2_TUNER_MODE_LANG1_LANG2)
			return fail("invalid new audmode\n");
		// Ambiguous whether this is allowed or not
		//	if (!tv && tun.audmode > V4L2_TUNER_MODE_STEREO)
		//		return -21;
		//	if (!valid_modes[tun.audmode])
		//		return fail("accepted invalid audmode %d\n", audmode);
	}
	return 0;
}

int testTuner(struct node *node)
{
	struct v4l2_tuner tuner;
	v4l2_std_id std;
	unsigned t = 0;
	int ret;

	if (doioctl(node, VIDIOC_G_STD, &std))
		std = 0;

	for (;;) {
		memset(&tuner, 0xff, sizeof(tuner));
		memset(tuner.reserved, 0, sizeof(tuner.reserved));
		tuner.index = t;
		ret = doioctl(node, VIDIOC_G_TUNER, &tuner);
		if (ret == EINVAL && t == 0)
			return -ENOSYS;
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("couldn't get tuner %d\n", t);
		if (validTuner(node, tuner, t, std))
			return fail("invalid tuner %d\n", t);
		t++;
		node->tuners++;
	}
	memset(&tuner, 0, sizeof(tuner));
	tuner.index = t;
	if (doioctl(node, VIDIOC_S_TUNER, &tuner) != EINVAL)
		return fail("could set invalid tuner %d\n", t);
	return 0;
}

static int validInput(struct node *node, const struct v4l2_input &descr, unsigned i)
{
	__u32 mask = (1 << node->audio_inputs) - 1;

	if (descr.index != i)
		return fail("invalid index\n");
	if (check_ustring(descr.name, sizeof(descr.name)))
		return fail("invalid name\n");
	if (descr.type != V4L2_INPUT_TYPE_TUNER && descr.type != V4L2_INPUT_TYPE_CAMERA)
		return fail("invalid type\n");
	if (descr.type == V4L2_INPUT_TYPE_CAMERA && descr.tuner)
		return fail("invalid tuner\n");
	if (!(descr.capabilities & V4L2_IN_CAP_STD) && descr.std)
		return fail("invalid std\n");
	if ((descr.capabilities & V4L2_IN_CAP_STD) && !descr.std)
		return fail("std == 0\n");
	if (descr.capabilities & ~0x7)
		return fail("invalid capabilities\n");
	if (check_0(descr.reserved, sizeof(descr.reserved)))
		return fail("non-zero reserved fields\n");
	if (descr.status & ~0x07070337)
		return fail("invalid status\n");
	if (descr.status & 0x02070000)
		return fail("use of deprecated digital video status\n");
	if (descr.audioset & ~mask)
		return fail("invalid audioset\n");
	if (descr.tuner && descr.tuner >= node->tuners)
		return fail("invalid tuner\n");
	return 0;
}

int testInput(struct node *node)
{
	struct v4l2_input descr;
	struct v4l2_audio audio;
	int cur_input = MAGIC;
	int input;
	int ret = doioctl(node, VIDIOC_G_INPUT, &cur_input);
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
		ret = doioctl(node, VIDIOC_ENUMINPUT, &descr);
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("could not enumerate input %d\n", i);
		input = i;
		if (doioctl(node, VIDIOC_S_INPUT, &input))
			return fail("could not set input to %d\n", i);
		if (input != i)
			return fail("input set to %d, but becomes %d?!\n", i, input);
		if (validInput(node, descr, i))
			return fail("invalid attributes for input %d\n", i);
		for (a = 0; a <= node->audio_inputs; a++) {
			memset(&audio, 0, sizeof(audio));
			audio.index = a;
			ret = doioctl(node, VIDIOC_S_AUDIO, &audio);
			if (ret && (descr.audioset & (1 << a)))
				return fail("could not set audio input to %d for video input %d\n", a, i);
			if (ret != EINVAL && !(descr.audioset & (1 << a)))
				return fail("could set invalid audio input %d for video input %d\n", a, i);
		}
		node->inputs++;
		i++;
	}
	input = i;
	if (doioctl(node, VIDIOC_S_INPUT, &input) != EINVAL)
		return fail("could set input to invalid input %d\n", i);
	if (doioctl(node, VIDIOC_S_INPUT, &cur_input))
		return fail("couldn't set input to the original input %d\n", cur_input);
	return 0;
}

static int validInputAudio(const struct v4l2_audio &descr, unsigned i)
{
	if (descr.index != i)
		return fail("invalid index\n");
	if (check_ustring(descr.name, sizeof(descr.name)))
		return fail("invalid name\n");
	if (descr.capability & ~0x3)
		return fail("invalid capabilities\n");
	if (descr.mode != 0 && descr.mode != V4L2_AUDMODE_AVL)
		return fail("invalid mode\n");
	if (!(descr.capability & V4L2_AUDCAP_AVL) && descr.mode)
		return fail("mode != 0\n");
	if (check_0(descr.reserved, sizeof(descr.reserved)))
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

		ret = doioctl(node, VIDIOC_ENUMAUDIO, &input);
		if (i == 0 && ret == EINVAL)
			return -ENOSYS;
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("could not enumerate audio input %d\n", i);
		if (validInputAudio(input, i))
			return fail("invalid attributes for audio input %d\n", i);
		node->audio_inputs++;
		i++;
	}
	memset(&input, 0xff, sizeof(input));
	memset(input.reserved, 0, sizeof(input.reserved));
	input.index = i;
	input.mode = 0;
	if (doioctl(node, VIDIOC_S_AUDIO, &input) != EINVAL)
		return fail("can set invalid audio input\n");
	memset(&input, 0xff, sizeof(input));
	ret = doioctl(node, VIDIOC_G_AUDIO, &input);
	if (i == 0) {
	       	if (ret != EINVAL)
			return fail("can get current audio input, but no inputs enumerated\n");
		return 0;
	}
	if (ret)
		return fail("cannot get current audio input\n");
	if (input.index >= node->audio_inputs)
		return fail("invalid current audio input %d\n", input.index);
	if (validInputAudio(input, input.index))
		return fail("invalid attributes for audio input %d\n", input.index);
	return 0;
}

static int validModulator(struct node *node, const struct v4l2_modulator &mod, unsigned m)
{
	bool tv = !node->is_radio;

	if (mod.index != m)
		return fail("invalid index\n");
	if (check_ustring(mod.name, sizeof(mod.name)))
		return fail("invalid name\n");
	if (check_0(mod.reserved, sizeof(mod.reserved)))
		return fail("non-zero reserved fields\n");
	if (tv && (mod.capability & V4L2_TUNER_CAP_RDS))
		return fail("RDS for TV modulator?\n");
	if (!tv && (mod.capability & (V4L2_TUNER_CAP_NORM |
					V4L2_TUNER_CAP_LANG1 | V4L2_TUNER_CAP_LANG2)))
		return fail("TV capabilities for radio modulator?\n");
	if (mod.rangelow >= mod.rangehigh)
		return fail("rangelow >= rangehigh\n");
	if (mod.rangelow == 0 || mod.rangehigh == 0xffffffff)
		return fail("invalid rangelow or rangehigh\n");
	if (!(mod.capability & V4L2_TUNER_CAP_STEREO) &&
			(mod.txsubchans & V4L2_TUNER_SUB_STEREO))
		return fail("stereo subchan, but no stereo caps?\n");
	if (!(mod.capability & V4L2_TUNER_CAP_LANG1) &&
			(mod.txsubchans & V4L2_TUNER_SUB_LANG1))
		return fail("lang1 subchan, but no lang1 caps?\n");
	if (!(mod.capability & V4L2_TUNER_CAP_LANG2) &&
			(mod.txsubchans & V4L2_TUNER_SUB_LANG2))
		return fail("lang2 subchan, but no lang2 caps?\n");
	if (!(mod.capability & V4L2_TUNER_CAP_RDS) &&
			(mod.txsubchans & V4L2_TUNER_SUB_RDS))
		return fail("RDS subchan, but no RDS caps?\n");
	return 0;
}

int testModulator(struct node *node)
{
	struct v4l2_modulator mod;
	unsigned m = 0;
	int ret;

	for (;;) {
		memset(&mod, 0xff, sizeof(mod));
		memset(mod.reserved, 0, sizeof(mod.reserved));
		mod.index = m;
		ret = doioctl(node, VIDIOC_G_MODULATOR, &mod);
		if (ret == EINVAL && m == 0)
			return -ENOSYS;
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("couldn't get modulator %d\n", m);
		if (validModulator(node, mod, m))
			return fail("invalid modulator %d\n", m);
		if (doioctl(node, VIDIOC_S_MODULATOR, &mod))
			return fail("cannot set modulator %d\n", m);
		m++;
		node->modulators++;
	}
	memset(&mod, 0, sizeof(mod));
	mod.index = m;
	if (doioctl(node, VIDIOC_S_MODULATOR, &mod) != EINVAL)
		return fail("could set invalid modulator %d\n", m);
	return 0;
}

static int validOutput(struct node *node, const struct v4l2_output &descr, unsigned o)
{
	__u32 mask = (1 << node->audio_outputs) - 1;

	if (descr.index != o)
		return fail("invalid index\n");
	if (check_ustring(descr.name, sizeof(descr.name)))
		return fail("invalid name\n");
	if (descr.type != V4L2_OUTPUT_TYPE_MODULATOR && descr.type != V4L2_OUTPUT_TYPE_ANALOG)
		return fail("invalid type\n");
	if (descr.type == V4L2_OUTPUT_TYPE_ANALOG && descr.modulator)
		return fail("invalid modulator\n");
	if (!(descr.capabilities & V4L2_OUT_CAP_STD) && descr.std)
		return fail("invalid std\n");
	if ((descr.capabilities & V4L2_OUT_CAP_STD) && !descr.std)
		return fail("std == 0\n");
	if (descr.capabilities & ~0x7)
		return fail("invalid capabilities\n");
	if (check_0(descr.reserved, sizeof(descr.reserved)))
		return fail("non-zero reserved fields\n");
	if (descr.audioset & ~mask)
		return fail("invalid audioset\n");
	if (descr.modulator && descr.modulator >= node->modulators)
		return fail("invalid modulator\n");
	return 0;
}

int testOutput(struct node *node)
{
	struct v4l2_output descr;
	struct v4l2_audioout audio;
	int cur_output = MAGIC;
	int output;
	int ret = doioctl(node, VIDIOC_G_OUTPUT, &cur_output);
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
		ret = doioctl(node, VIDIOC_ENUMOUTPUT, &descr);
		if (ret)
			break;
		output = o;
		if (doioctl(node, VIDIOC_S_OUTPUT, &output))
			return fail("could not set output to %d\n", o);
		if (output != o)
			return fail("output set to %d, but becomes %d?!\n", o, output);
		if (validOutput(node, descr, o))
			return fail("invalid attributes for output %d\n", o);
		for (a = 0; a <= node->audio_outputs; a++) {
			memset(&audio, 0, sizeof(audio));
			audio.index = a;
			ret = doioctl(node, VIDIOC_S_AUDOUT, &audio);
			if (ret && (descr.audioset & (1 << a)))
				return fail("could not set audio output to %d for video output %d\n", a, o);
			if (ret != EINVAL && !(descr.audioset & (1 << a)))
				return fail("could set invalid audio output %d for video output %d\n", a, o);
		}
		node->outputs++;
		o++;
	}
	output = o;
	if (doioctl(node, VIDIOC_S_OUTPUT, &output) != EINVAL)
		return fail("could set output to invalid output %d\n", o);
	if (doioctl(node, VIDIOC_S_OUTPUT, &cur_output))
		return fail("couldn't set output to the original output %d\n", cur_output);
	return 0;
}

static int validOutputAudio(const struct v4l2_audioout &descr, unsigned o)
{
	if (descr.index != o)
		return fail("invalid index\n");
	if (check_ustring(descr.name, sizeof(descr.name)))
		return fail("invalid name\n");
	if (descr.capability)
		return fail("invalid capabilities\n");
	if (descr.mode)
		return fail("invalid mode\n");
	if (check_0(descr.reserved, sizeof(descr.reserved)))
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

		ret = doioctl(node, VIDIOC_ENUMAUDOUT, &output);
		if (o == 0 && ret == EINVAL)
			return -ENOSYS;
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("could not enumerate audio output %d\n", o);
		if (validOutputAudio(output, o))
			return fail("invalid attributes for audio output %d\n", o);
		node->audio_outputs++;
		o++;
	}
	memset(&output, 0xff, sizeof(output));
	memset(output.reserved, 0, sizeof(output.reserved));
	output.index = o;
	output.mode = 0;
	if (doioctl(node, VIDIOC_S_AUDOUT, &output) != EINVAL)
		return fail("can set invalid audio output\n");
	memset(&output, 0xff, sizeof(output));
	ret = doioctl(node, VIDIOC_G_AUDOUT, &output);
	if (o == 0) {
	       	if (ret != EINVAL)
			return fail("can get current audio output, but no outputs enumerated\n");
		return 0;
	}
	if (ret)
		return fail("cannot get current audio output\n");
	if (output.index >= node->audio_outputs)
		return fail("invalid current audio output %d\n", output.index);
	if (validOutputAudio(output, output.index))
		return fail("invalid attributes for audio output %d\n", output.index);
	return 0;
}
