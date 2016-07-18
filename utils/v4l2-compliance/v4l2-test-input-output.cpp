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
    Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
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

static int checkEnumFreqBands(struct node *node, __u32 tuner, __u32 type, __u32 caps,
			      __u32 rangelow, __u32 rangehigh)
{
	const __u32 band_caps = V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_1HZ;
	__u32 caps_union = 0;
	unsigned low = 0xffffffff;
	unsigned high = 0;
	unsigned i;

	for (i = 0; ; i++) {
		struct v4l2_frequency_band band;
		int ret;

		memset(band.reserved, 0, sizeof(band.reserved));
		band.tuner = tuner;
		band.type = type;
		band.index = i;
		ret = doioctl(node, VIDIOC_ENUM_FREQ_BANDS, &band);
		if (ret == EINVAL && i)
			return 0;
		if (ret)
			return fail("couldn't get freq band\n");
		caps_union |= band.capability;
		if ((caps & band_caps) != (band.capability & band_caps))
			return fail("Inconsistent CAP_LOW/CAP_1HZ usage\n");
		fail_on_test(band.rangehigh < band.rangelow);
		fail_on_test(band.index != i);
		fail_on_test(band.type != type);
		fail_on_test(band.tuner != tuner);
		fail_on_test((band.capability & V4L2_TUNER_CAP_FREQ_BANDS) == 0);
		fail_on_test(check_0(band.reserved, sizeof(band.reserved)));
		if (band.rangelow < low)
			low = band.rangelow;
		if (band.rangehigh > high)
			high = band.rangehigh;
	}
	fail_on_test(caps_union != caps);
	fail_on_test(low != rangelow);
	fail_on_test(high != rangehigh);
	return 0;
}

static int checkTuner(struct node *node, const struct v4l2_tuner &tuner,
		unsigned t, v4l2_std_id std)
{
	bool valid_modes[5] = { true, false, false, false, false };
	bool tv = node->is_video || node->is_vbi;
	bool hwseek_caps = tuner.capability & (V4L2_TUNER_CAP_HWSEEK_BOUNDED |
			V4L2_TUNER_CAP_HWSEEK_WRAP | V4L2_TUNER_CAP_HWSEEK_PROG_LIM);
	unsigned type = tv ? V4L2_TUNER_ANALOG_TV : V4L2_TUNER_RADIO;
	__u32 audmode;

	if (tuner.index != t)
		return fail("invalid index\n");
	if (check_ustring(tuner.name, sizeof(tuner.name)))
		return fail("invalid name\n");
	if (check_0(tuner.reserved, sizeof(tuner.reserved)))
		return fail("non-zero reserved fields\n");
	if (node->is_sdr) {
		fail_on_test(tuner.type != V4L2_TUNER_SDR && tuner.type != V4L2_TUNER_RF);
	} else if (tuner.type != type) {
		return fail("invalid tuner type %d\n", tuner.type);
	}
	if (tv && (tuner.capability & V4L2_TUNER_CAP_RDS))
		return fail("RDS for TV tuner?\n");
	if (!tv && (tuner.capability & (V4L2_TUNER_CAP_NORM |
					V4L2_TUNER_CAP_LANG1 | V4L2_TUNER_CAP_LANG2)))
		return fail("TV capabilities for radio tuner?\n");
	if (tv && (tuner.capability & (V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_1HZ)))
		return fail("did not expect to see V4L2_TUNER_CAP_LOW/1HZ set for a tv tuner\n");
	if (node->is_radio && !(tuner.capability & V4L2_TUNER_CAP_LOW))
		return fail("V4L2_TUNER_CAP_LOW was not set for a radio tuner\n");
	fail_on_test((tuner.capability & V4L2_TUNER_CAP_LOW) &&
		     (tuner.capability & V4L2_TUNER_CAP_1HZ));
	if (node->is_sdr)
		fail_on_test(!(V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_1HZ));
	fail_on_test(!(tuner.capability & V4L2_TUNER_CAP_FREQ_BANDS));
	fail_on_test(!(node->g_caps() & V4L2_CAP_HW_FREQ_SEEK) && hwseek_caps);
	fail_on_test((node->g_caps() & V4L2_CAP_HW_FREQ_SEEK) &&
		!(tuner.capability & (V4L2_TUNER_CAP_HWSEEK_BOUNDED | V4L2_TUNER_CAP_HWSEEK_WRAP)));
	if (tuner.rangelow > tuner.rangehigh)
		return fail("rangelow > rangehigh\n");
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
	bool have_rds = tuner.capability & V4L2_TUNER_CAP_RDS;
	bool have_rds_method = tuner.capability &
                        (V4L2_TUNER_CAP_RDS_BLOCK_IO | V4L2_TUNER_CAP_RDS_CONTROLS);
	if (have_rds ^ have_rds_method)
		return fail("V4L2_TUNER_CAP_RDS is set, but not V4L2_TUNER_CAP_RDS_* or vice versa\n");
	fail_on_test(node->is_sdr && have_rds);
	if ((tuner.capability & V4L2_TUNER_CAP_RDS_BLOCK_IO) &&
			!(node->g_caps() & V4L2_CAP_READWRITE))
		return fail("V4L2_TUNER_CAP_RDS_BLOCK_IO is set, but not V4L2_CAP_READWRITE\n");
	if (node->is_radio && !(tuner.capability & V4L2_TUNER_CAP_RDS_BLOCK_IO) &&
			(node->g_caps() & V4L2_CAP_READWRITE))
		return fail("V4L2_TUNER_CAP_RDS_BLOCK_IO is not set, but V4L2_CAP_READWRITE is\n");
	if (std == V4L2_STD_NTSC_M && (tuner.rxsubchans & V4L2_TUNER_SUB_LANG1))
		return fail("LANG1 subchan, but NTSC-M standard\n");
	if (tuner.audmode > V4L2_TUNER_MODE_LANG1_LANG2)
		return fail("invalid audio mode\n");
	if (!tv && tuner.audmode > V4L2_TUNER_MODE_STEREO)
		return fail("invalid audio mode for radio device\n");
	if (tuner.signal > 65535)
		return fail("signal too large\n");
	if (tuner.capability & V4L2_TUNER_CAP_STEREO)
		valid_modes[V4L2_TUNER_MODE_STEREO] = true;
	if (tuner.capability & V4L2_TUNER_CAP_LANG1)
		valid_modes[V4L2_TUNER_MODE_LANG1] = true;
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
		if (!valid_modes[tun.audmode])
			return fail("accepted invalid audmode %d\n", audmode);
	}
	return checkEnumFreqBands(node, tuner.index, tuner.type, tuner.capability,
			tuner.rangelow, tuner.rangehigh);
}

int testTuner(struct node *node)
{
	struct v4l2_tuner tuner;
	v4l2_std_id std;
	unsigned t = 0;
	bool has_rds = false;
	int ret;

	if (doioctl(node, VIDIOC_G_STD, &std))
		std = 0;

	for (;;) {
		memset(&tuner, 0xff, sizeof(tuner));
		memset(tuner.reserved, 0, sizeof(tuner.reserved));
		tuner.index = t;
		ret = doioctl(node, VIDIOC_G_TUNER, &tuner);
		if (ret == ENOTTY)
			return ret;
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("couldn't get tuner %d\n", t);
		if (checkTuner(node, tuner, t, std))
			return fail("invalid tuner %d\n", t);
		t++;
		node->tuners++;
		if (tuner.capability & V4L2_TUNER_CAP_RDS)
			has_rds = true;
	}
	memset(&tuner, 0, sizeof(tuner));
	tuner.index = t;
	if (doioctl(node, VIDIOC_S_TUNER, &tuner) != EINVAL)
		return fail("could set invalid tuner %d\n", t);
	if (node->tuners && !(node->g_caps() & V4L2_CAP_TUNER))
		return fail("tuners found, but no tuner capability set\n");
	if (!node->tuners && (node->g_caps() & V4L2_CAP_TUNER))
		return fail("no tuners found, but tuner capability set\n");
	if (has_rds && !(node->g_caps() & V4L2_CAP_RDS_CAPTURE))
		return fail("RDS tuner capability, but no RDS capture capability?\n");
	if (!has_rds && (node->g_caps() & V4L2_CAP_RDS_CAPTURE))
		return fail("No RDS tuner capability, but RDS capture capability?\n");
	return 0;
}

int testTunerFreq(struct node *node)
{
	struct v4l2_frequency freq = { 0 };
	enum v4l2_tuner_type last_type = V4L2_TUNER_ANALOG_TV;
	unsigned t;
	int ret;

	for (t = 0; t < node->tuners; t++) {
		struct v4l2_tuner tuner = { 0 };
		
		tuner.index = t;
		ret = doioctl(node, VIDIOC_G_TUNER, &tuner);
		if (ret)
			return fail("could not get tuner %d\n", t);
		last_type = (enum v4l2_tuner_type)tuner.type;
		memset(&freq, 0, sizeof(freq));
		freq.tuner = t;
		ret = doioctl(node, VIDIOC_G_FREQUENCY, &freq);
		if (ret)
			return fail("could not get frequency for tuner %d\n", t);
		if (check_0(freq.reserved, sizeof(freq.reserved)))
			return fail("reserved was not zeroed\n");
		if (freq.type != V4L2_TUNER_RADIO && freq.type != V4L2_TUNER_ANALOG_TV &&
		    freq.type != V4L2_TUNER_SDR && freq.type != V4L2_TUNER_RF)
			return fail("returned invalid tuner type %d\n", freq.type);
		if (freq.type == V4L2_TUNER_RADIO && !(node->g_caps() & V4L2_CAP_RADIO))
			return fail("radio tuner found but no radio capability set\n");
		if ((freq.type == V4L2_TUNER_SDR || freq.type == V4L2_TUNER_RF) &&
		    !(node->g_caps() & V4L2_CAP_SDR_CAPTURE))
			return fail("sdr tuner found but no sdr capture capability set\n");
		if (freq.type != tuner.type)
			return fail("frequency tuner type and tuner type mismatch\n");
		if (freq.tuner != t)
			return fail("frequency tuner field changed!\n");
		if (freq.frequency == 0)
			return fail("frequency not set\n");
		if (freq.frequency < tuner.rangelow || freq.frequency > tuner.rangehigh)
			warn("returned tuner %d frequency out of range (%d not in [%d...%d])\n",
					t, freq.frequency, tuner.rangelow, tuner.rangehigh);

		freq.type = (enum v4l2_tuner_type)0;
		ret = doioctl(node, VIDIOC_S_FREQUENCY, &freq);
		if (ret != EINVAL)
			return fail("did not return EINVAL when passed tuner type 0\n");
		freq.type = tuner.type;
		ret = doioctl(node, VIDIOC_S_FREQUENCY, &freq);
		if (ret)
			return fail("could not set current frequency\n");
		freq.frequency = tuner.rangelow;
		ret = doioctl(node, VIDIOC_S_FREQUENCY, &freq);
		if (ret)
			return fail("could not set rangelow frequency\n");
		freq.frequency = tuner.rangehigh;
		ret = doioctl(node, VIDIOC_S_FREQUENCY, &freq);
		if (ret)
			return fail("could not set rangehigh frequency\n");
		freq.frequency = tuner.rangelow - 1;
		ret = doioctl(node, VIDIOC_S_FREQUENCY, &freq);
		if (ret)
			return fail("could not set rangelow-1 frequency\n");
		ret = doioctl(node, VIDIOC_G_FREQUENCY, &freq);
		if (ret || freq.frequency != tuner.rangelow)
			return fail("frequency rangelow-1 wasn't mapped to rangelow\n");
		freq.frequency = tuner.rangehigh + 1;
		ret = doioctl(node, VIDIOC_S_FREQUENCY, &freq);
		if (ret)
			return fail("could not set rangehigh+1 frequency\n");
		ret = doioctl(node, VIDIOC_G_FREQUENCY, &freq);
		if (ret || freq.frequency != tuner.rangehigh)
			return fail("frequency rangehigh+1 wasn't mapped to rangehigh\n");
	}

	/* If this is a modulator device, then skip the remaining tests */
	if (node->g_caps() & V4L2_CAP_MODULATOR)
		return 0;

	freq.tuner = t;
	ret = doioctl(node, VIDIOC_G_FREQUENCY, &freq);
	if (ret != EINVAL && ret != ENOTTY)
		return fail("could get frequency for invalid tuner %d\n", t);
	freq.tuner = t;
	freq.type = last_type;
	// TV: 400 Mhz Radio: 100 MHz
	freq.frequency = last_type == V4L2_TUNER_ANALOG_TV ? 6400 : 1600000;
	ret = doioctl(node, VIDIOC_S_FREQUENCY, &freq);
	if (ret != EINVAL && ret != ENOTTY)
		return fail("could set frequency for invalid tuner %d\n", t);
	return node->tuners ? 0 : ENOTTY;
}

int testTunerHwSeek(struct node *node)
{
	struct v4l2_hw_freq_seek seek;
	unsigned t;
	int ret;

	for (t = 0; t < node->tuners; t++) {
		struct v4l2_tuner tuner = { 0 };
		
		tuner.index = t;
		ret = doioctl(node, VIDIOC_G_TUNER, &tuner);
		if (ret)
			return fail("could not get tuner %d\n", t);

		memset(&seek, 0, sizeof(seek));
		seek.tuner = t;
		seek.type = V4L2_TUNER_RADIO;
		ret = doioctl(node, VIDIOC_S_HW_FREQ_SEEK, &seek);
		if (!(node->g_caps() & V4L2_CAP_HW_FREQ_SEEK) && ret != ENOTTY)
			return fail("hw seek supported but capability not set\n");
		if (!node->is_radio && ret != ENOTTY)
			return fail("hw seek supported on a non-radio node?!\n");
		if (!node->is_radio || !(node->g_caps() & V4L2_CAP_HW_FREQ_SEEK))
			return ENOTTY;
		seek.type = V4L2_TUNER_ANALOG_TV;
		ret = doioctl(node, VIDIOC_S_HW_FREQ_SEEK, &seek);
		if (ret != EINVAL)
			return fail("hw seek accepted TV tuner\n");
		seek.type = V4L2_TUNER_RADIO;
		seek.seek_upward = 1;
		ret = doioctl(node, VIDIOC_S_HW_FREQ_SEEK, &seek);
		if (ret == EINVAL && (tuner.capability & V4L2_TUNER_CAP_HWSEEK_BOUNDED))
			return fail("hw bounded seek failed\n");
		if (ret && ret != EINVAL && ret != ENODATA)
			return fail("hw bounded seek failed with error %d\n", ret);
		seek.wrap_around = 1;
		ret = doioctl(node, VIDIOC_S_HW_FREQ_SEEK, &seek);
		if (ret == EINVAL && (tuner.capability & V4L2_TUNER_CAP_HWSEEK_WRAP))
			return fail("hw wrapped seek failed\n");
		if (ret && ret != EINVAL && ret != ENODATA)
			return fail("hw wrapped seek failed with error %d\n", ret);
		if (check_0(seek.reserved, sizeof(seek.reserved)))
			return fail("non-zero reserved fields\n");
	}
	memset(&seek, 0, sizeof(seek));
	seek.tuner = node->tuners;
	seek.type = V4L2_TUNER_RADIO;
	ret = doioctl(node, VIDIOC_S_HW_FREQ_SEEK, &seek);
	if (ret != EINVAL && ret != ENOTTY)
		return fail("hw seek for invalid tuner didn't return EINVAL or ENOTTY\n");
	return ret == ENOTTY ? ret : 0;
}

static int checkInput(struct node *node, const struct v4l2_input &descr, unsigned i)
{
	__u32 mask = (1 << node->audio_inputs) - 1;
	struct v4l2_selection sel;

	if (descr.index != i)
		return fail("invalid index\n");
	if (check_ustring(descr.name, sizeof(descr.name)))
		return fail("invalid name\n");
	if (descr.type != V4L2_INPUT_TYPE_TUNER &&
      descr.type != V4L2_INPUT_TYPE_CAMERA &&
      descr.type != V4L2_INPUT_TYPE_TOUCH)
		return fail("invalid type\n");
	if (descr.type == V4L2_INPUT_TYPE_CAMERA && descr.tuner)
		return fail("invalid tuner\n");
	if (descr.type == V4L2_INPUT_TYPE_TUNER && node->tuners == 0)
		return fail("no tuners found for tuner input\n");
	if (!(descr.capabilities & V4L2_IN_CAP_STD) && descr.std)
		return fail("invalid std\n");
	if ((descr.capabilities & V4L2_IN_CAP_STD) && !descr.std)
		return fail("std == 0\n");
	memset(&sel, 0, sizeof(sel));
	sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	sel.target = V4L2_SEL_TGT_NATIVE_SIZE;
	if (descr.capabilities & V4L2_IN_CAP_NATIVE_SIZE) {
		fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel));
		fail_on_test(doioctl(node, VIDIOC_S_SELECTION, &sel));
	} else if (!doioctl(node, VIDIOC_G_SELECTION, &sel)) {
		fail_on_test(!doioctl(node, VIDIOC_S_SELECTION, &sel));
	}
	if (descr.capabilities & ~0x7)
		return fail("invalid capabilities\n");
	if (check_0(descr.reserved, sizeof(descr.reserved)))
		return fail("non-zero reserved fields\n");
	if (descr.status & ~0x07070337)
		return fail("invalid status\n");
	if (descr.status & 0x02060000)
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
	int cur_input = MAGIC;
	int input;
	int ret = doioctl(node, VIDIOC_G_INPUT, &cur_input);
	int i = 0;

	if (ret == ENOTTY) {
		if (node->has_inputs)
			return fail("G_INPUT not supported for a capture device\n");
		descr.index = 0;
		ret = doioctl(node, VIDIOC_ENUMINPUT, &descr);
		if (ret != ENOTTY)
			return fail("G_INPUT not supported, but ENUMINPUT is\n");
		cur_input = 0;
		ret = doioctl(node, VIDIOC_S_INPUT, &cur_input);
		if (ret != ENOTTY)
			return fail("G_INPUT not supported, but S_INPUT is\n");
		return ENOTTY;
	}
	if (ret)
		return fail("could not get current input\n");
	if (cur_input == MAGIC)
		return fail("VIDIOC_G_INPUT didn't fill in the input\n");
	if (node->is_radio)
		return fail("radio can't have input support\n");
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
		if (checkInput(node, descr, i))
			return fail("invalid attributes for input %d\n", i);
		node->inputs++;
		i++;
	}
	input = i;
	if (doioctl(node, VIDIOC_S_INPUT, &input) != EINVAL)
		return fail("could set input to invalid input %d\n", i);
	if (doioctl(node, VIDIOC_S_INPUT, &cur_input))
		return fail("couldn't set input to the original input %d\n", cur_input);
	if (node->inputs && !node->has_inputs)
		return fail("inputs found, but no input capabilities set\n");
	if (!node->inputs && node->has_inputs)
		return fail("no inputs found, but input capabilities set\n");
	fail_on_test(node->is_m2m && node->inputs > 1);
	return 0;
}

static int checkInputAudio(const struct v4l2_audio &descr, unsigned i)
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

int testEnumInputAudio(struct node *node)
{
	struct v4l2_audio input;
	unsigned i = 0;
	int ret;

	for (;;) {
		memset(&input, 0xff, sizeof(input));
		input.index = i;

		ret = doioctl(node, VIDIOC_ENUMAUDIO, &input);
		if (ret == ENOTTY)
			return ret;
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("could not enumerate audio input %d\n", i);
		if (checkInputAudio(input, i))
			return fail("invalid attributes for audio input %d\n", i);
		node->audio_inputs++;
		i++;
	}
	if (node->audio_inputs && !(node->g_caps() & V4L2_CAP_AUDIO))
		return fail("audio inputs reported, but no CAP_AUDIO set\n");
	return 0;
}

static int checkInputAudioSet(struct node *node, __u32 audioset)
{
	struct v4l2_audio input = { 0 };
	unsigned i;
	int ret;

	ret = doioctl(node, VIDIOC_G_AUDIO, &input);
	if (audioset == 0 && ret != ENOTTY && ret != EINVAL)
		return fail("No audio inputs, but G_AUDIO did not return ENOTTY or EINVAL\n");
	if (audioset) {
		if (ret)
			return fail("Audio inputs, but G_AUDIO returned an error\n");
		if (input.index >= node->audio_inputs)
			return fail("invalid current audio input %d\n", input.index);
		if (checkInputAudio(input, input.index))
			return fail("invalid attributes for audio input %d\n", input.index);
	}

	for (i = 0; i <= node->audio_inputs; i++) {
		int valid = audioset & (1 << i);

		memset(&input, 0xff, sizeof(input));
		memset(input.reserved, 0, sizeof(input.reserved));
		input.index = i;
		input.mode = 0;
		ret = doioctl(node, VIDIOC_S_AUDIO, &input);
		if (!valid && ret != EINVAL && ret != ENOTTY)
			return fail("can set invalid audio input %d\n", i);
		if (valid && ret)
			return fail("can't set valid audio input %d\n", i);
	}
	return 0;
}

int testInputAudio(struct node *node)
{
	struct v4l2_input vinput = { 0 };
	unsigned i = 0;
	int ret;

	if (node->audio_inputs && node->inputs == 0)
		return fail("audio inputs found but no video inputs?!\n");

	for (i = 0; i < node->inputs; i++) {
		ret = doioctl(node, VIDIOC_S_INPUT, &i);
		if (ret)
			return fail("could not select input %d\n", i);
		vinput.index = i;
		ret = doioctl(node, VIDIOC_ENUMINPUT, &vinput);
		if (ret)
			return fail("could not enumerate input %d\n", i);
		if (checkInputAudioSet(node, vinput.audioset))
			return fail("invalid audioset for input %d\n", i);
	}
	return node->audio_inputs ? 0 : ENOTTY;
}

static int checkModulator(struct node *node, const struct v4l2_modulator &mod, unsigned m)
{
	bool tv = !node->is_radio && !node->is_sdr;

	if (mod.index != m)
		return fail("invalid index\n");
	if (check_ustring(mod.name, sizeof(mod.name)))
		return fail("invalid name\n");
	if (check_0(mod.reserved, sizeof(mod.reserved)))
		return fail("non-zero reserved fields\n");
	if (tv)
		return fail("currently only radio/sdr modulators are supported\n");
	if (node->is_sdr)
		fail_on_test(mod.type != V4L2_TUNER_SDR && mod.type != V4L2_TUNER_RF);
	else if (mod.type != V4L2_TUNER_RADIO)
		return fail("invalid modulator type %d\n", mod.type);

	if (!(mod.capability & (V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_1HZ)))
		return fail("V4L2_TUNER_CAP_LOW/1HZ was not set for a radio modulator\n");
	if (mod.capability & (V4L2_TUNER_CAP_NORM |
					V4L2_TUNER_CAP_LANG1 | V4L2_TUNER_CAP_LANG2))
		return fail("TV capabilities for radio modulator?\n");
	fail_on_test(!(mod.capability & V4L2_TUNER_CAP_FREQ_BANDS));
	if (mod.rangelow > mod.rangehigh)
		return fail("rangelow > rangehigh\n");
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
	bool have_rds = mod.capability & V4L2_TUNER_CAP_RDS;
	bool have_rds_method = mod.capability &
                        (V4L2_TUNER_CAP_RDS_BLOCK_IO | V4L2_TUNER_CAP_RDS_CONTROLS);
	if (have_rds ^ have_rds_method)
		return fail("V4L2_TUNER_CAP_RDS is set, but not V4L2_TUNER_CAP_RDS_* or vice versa\n");
	if ((mod.capability & V4L2_TUNER_CAP_RDS_BLOCK_IO) &&
			!(node->g_caps() & V4L2_CAP_READWRITE))
		return fail("V4L2_TUNER_CAP_RDS_BLOCK_IO is set, but not V4L2_CAP_READWRITE\n");
	if (!node->is_sdr && !(mod.capability & V4L2_TUNER_CAP_RDS_BLOCK_IO) &&
			(node->g_caps() & V4L2_CAP_READWRITE))
		return fail("V4L2_TUNER_CAP_RDS_BLOCK_IO is not set, but V4L2_CAP_READWRITE is\n");
	return checkEnumFreqBands(node, mod.index, mod.type, mod.capability,
			mod.rangelow, mod.rangehigh);
}

int testModulator(struct node *node)
{
	struct v4l2_modulator mod;
	unsigned m = 0;
	bool has_rds = false;
	int ret;

	for (;;) {
		memset(&mod, 0xff, sizeof(mod));
		memset(mod.reserved, 0, sizeof(mod.reserved));
		mod.index = m;
		ret = doioctl(node, VIDIOC_G_MODULATOR, &mod);
		if (ret == ENOTTY)
			return ret;
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("couldn't get modulator %d\n", m);
		if (checkModulator(node, mod, m))
			return fail("invalid modulator %d\n", m);
		if (doioctl(node, VIDIOC_S_MODULATOR, &mod))
			return fail("cannot set modulator %d\n", m);
		m++;
		node->modulators++;
		if (mod.capability & V4L2_TUNER_CAP_RDS)
			has_rds = true;
	}
	memset(&mod, 0, sizeof(mod));
	mod.index = m;
	if (doioctl(node, VIDIOC_S_MODULATOR, &mod) != EINVAL)
		return fail("could set invalid modulator %d\n", m);
	if (node->modulators && !(node->g_caps() & V4L2_CAP_MODULATOR))
		return fail("modulators found, but no modulator capability set\n");
	if (!node->modulators && (node->g_caps() & V4L2_CAP_MODULATOR))
		return fail("no modulators found, but modulator capability set\n");
	if (has_rds && !(node->g_caps() & V4L2_CAP_RDS_OUTPUT))
		return fail("RDS modulator capability, but no RDS output capability?\n");
	if (!has_rds && (node->g_caps() & V4L2_CAP_RDS_OUTPUT))
		return fail("No RDS modulator capability, but RDS output capability?\n");
	return 0;
}

int testModulatorFreq(struct node *node)
{
	struct v4l2_frequency freq = { 0 };
	unsigned m;
	int ret;

	for (m = 0; m < node->modulators; m++) {
		struct v4l2_modulator modulator;
		
		modulator.index = m;
		memset(modulator.reserved, 0, sizeof(modulator.reserved));
		ret = doioctl(node, VIDIOC_G_MODULATOR, &modulator);
		if (ret)
			return fail("could not get modulator %d\n", m);
		memset(&freq, 0, sizeof(freq));
		freq.tuner = m;
		ret = doioctl(node, VIDIOC_G_FREQUENCY, &freq);
		if (ret)
			return fail("could not get frequency for modulator %d\n", m);
		if (check_0(freq.reserved, sizeof(freq.reserved)))
			return fail("reserved was not zeroed\n");
		if (freq.tuner != m)
			return fail("frequency modulator field changed!\n");
		if ((freq.type == V4L2_TUNER_SDR || freq.type == V4L2_TUNER_RF) &&
		    !(node->g_caps() & V4L2_CAP_SDR_OUTPUT))
			return fail("sdr tuner found but no sdr output capability set\n");
		if (freq.frequency == 0)
			return fail("frequency not set\n");
		if (freq.frequency < modulator.rangelow || freq.frequency > modulator.rangehigh)
			warn("returned modulator %d frequency out of range (%d not in [%d...%d])\n",
					m, freq.frequency, modulator.rangelow, modulator.rangehigh);

		ret = doioctl(node, VIDIOC_S_FREQUENCY, &freq);
		if (ret)
			return fail("could not set current frequency\n");
		freq.frequency = modulator.rangelow;
		ret = doioctl(node, VIDIOC_S_FREQUENCY, &freq);
		if (ret)
			return fail("could not set rangelow frequency\n");
		freq.frequency = modulator.rangehigh;
		ret = doioctl(node, VIDIOC_S_FREQUENCY, &freq);
		if (ret)
			return fail("could not set rangehigh frequency\n");
		freq.frequency = modulator.rangelow - 1;
		ret = doioctl(node, VIDIOC_S_FREQUENCY, &freq);
		if (ret)
			return fail("could not set rangelow-1 frequency\n");
		ret = doioctl(node, VIDIOC_G_FREQUENCY, &freq);
		if (ret || freq.frequency != modulator.rangelow)
			return fail("frequency rangelow-1 wasn't mapped to rangelow\n");
		freq.frequency = modulator.rangehigh + 1;
		ret = doioctl(node, VIDIOC_S_FREQUENCY, &freq);
		if (ret)
			return fail("could not set rangehigh+1 frequency\n");
		ret = doioctl(node, VIDIOC_G_FREQUENCY, &freq);
		if (ret || freq.frequency != modulator.rangehigh)
			return fail("frequency rangehigh+1 wasn't mapped to rangehigh\n");
	}

	/* If this is a tuner device, then skip the remaining tests */
	if (node->g_caps() & V4L2_CAP_TUNER)
		return 0;

	freq.tuner = m;
	ret = doioctl(node, VIDIOC_G_FREQUENCY, &freq);
	if (ret != EINVAL && ret != ENOTTY)
		return fail("could get frequency for invalid modulator %d\n", m);
	freq.tuner = m;
	// Radio: 100 MHz
	freq.frequency = 1600000;
	ret = doioctl(node, VIDIOC_S_FREQUENCY, &freq);
	if (ret != EINVAL && ret != ENOTTY)
		return fail("could set frequency for invalid modulator %d\n", m);
	return node->modulators ? 0 : ENOTTY;
}

static int checkOutput(struct node *node, const struct v4l2_output &descr, unsigned o)
{
	__u32 mask = (1 << node->audio_outputs) - 1;
	struct v4l2_selection sel;

	if (descr.index != o)
		return fail("invalid index\n");
	if (check_ustring(descr.name, sizeof(descr.name)))
		return fail("invalid name\n");
	if (descr.type != V4L2_OUTPUT_TYPE_MODULATOR && descr.type != V4L2_OUTPUT_TYPE_ANALOG)
		return fail("invalid type\n");
	if (descr.type == V4L2_OUTPUT_TYPE_ANALOG && descr.modulator)
		return fail("invalid modulator\n");
	if (descr.type == V4L2_OUTPUT_TYPE_MODULATOR && node->modulators == 0)
		return fail("no modulators found for modulator output\n");
	if (!(descr.capabilities & V4L2_OUT_CAP_STD) && descr.std)
		return fail("invalid std\n");
	if ((descr.capabilities & V4L2_OUT_CAP_STD) && !descr.std)
		return fail("std == 0\n");
	memset(&sel, 0, sizeof(sel));
	sel.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	sel.target = V4L2_SEL_TGT_NATIVE_SIZE;
	if (descr.capabilities & V4L2_OUT_CAP_NATIVE_SIZE) {
		fail_on_test(doioctl(node, VIDIOC_G_SELECTION, &sel));
		fail_on_test(doioctl(node, VIDIOC_S_SELECTION, &sel));
	} else if (!doioctl(node, VIDIOC_G_SELECTION, &sel)) {
		fail_on_test(!doioctl(node, VIDIOC_S_SELECTION, &sel));
	}
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
	int cur_output = MAGIC;
	int output;
	int ret = doioctl(node, VIDIOC_G_OUTPUT, &cur_output);
	int o = 0;

	if (ret == ENOTTY) {
		if (node->has_outputs)
			return fail("G_OUTPUT not supported for an output device\n");
		descr.index = 0;
		ret = doioctl(node, VIDIOC_ENUMOUTPUT, &descr);
		if (ret != ENOTTY)
			return fail("G_OUTPUT not supported, but ENUMOUTPUT is\n");
		output = 0;
		ret = doioctl(node, VIDIOC_S_OUTPUT, &output);
		if (ret != ENOTTY)
			return fail("G_OUTPUT not supported, but S_OUTPUT is\n");
	}
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
		if (checkOutput(node, descr, o))
			return fail("invalid attributes for output %d\n", o);
		node->outputs++;
		o++;
	}
	output = o;
	if (doioctl(node, VIDIOC_S_OUTPUT, &output) != EINVAL)
		return fail("could set output to invalid output %d\n", o);
	if (doioctl(node, VIDIOC_S_OUTPUT, &cur_output))
		return fail("couldn't set output to the original output %d\n", cur_output);
	if (node->outputs && !node->has_outputs)
		return fail("outputs found, but no output capabilities set\n");
	if (!node->outputs && node->has_outputs)
		return fail("no outputs found, but output capabilities set\n");
	fail_on_test(node->is_m2m && node->outputs > 1);
	return 0;
}

static int checkOutputAudio(const struct v4l2_audioout &descr, unsigned o)
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

int testEnumOutputAudio(struct node *node)
{
	struct v4l2_audioout output;
	unsigned o = 0;
	int ret;

	for (;;) {
		memset(&output, 0xff, sizeof(output));
		output.index = o;

		ret = doioctl(node, VIDIOC_ENUMAUDOUT, &output);
		if (ret == ENOTTY)
			return ENOTTY;
		if (ret == EINVAL)
			break;
		if (ret)
			return fail("could not enumerate audio output %d\n", o);
		if (checkOutputAudio(output, o))
			return fail("invalid attributes for audio output %d\n", o);
		node->audio_outputs++;
		o++;
	}

	if (node->audio_outputs && !(node->g_caps() & V4L2_CAP_AUDIO))
		return fail("audio outputs reported, but no CAP_AUDIO set\n");
	return 0;
}

static int checkOutputAudioSet(struct node *node, __u32 audioset)
{
	struct v4l2_audioout output;
	unsigned i;
	int ret;

	memset(output.reserved, 0, sizeof(output.reserved));
	ret = doioctl(node, VIDIOC_G_AUDOUT, &output);
	if (audioset == 0 && ret != EINVAL && ret != ENOTTY)
		return fail("No audio outputs, but G_AUDOUT did not return EINVAL or ENOTTY\n");
	if (audioset) {
		if (ret)
			return fail("Audio outputs, but G_AUDOUT returned an error\n");
		if (output.index >= node->audio_outputs)
			return fail("invalid current audio output %d\n", output.index);
		if (checkOutputAudio(output, output.index))
			return fail("invalid attributes for audio output %d\n", output.index);
	}

	for (i = 0; i <= node->audio_outputs; i++) {
		int valid = audioset & (1 << i);

		memset(&output, 0xff, sizeof(output));
		memset(output.reserved, 0, sizeof(output.reserved));
		output.index = i;
		output.mode = 0;
		ret = doioctl(node, VIDIOC_S_AUDOUT, &output);
		if (!valid && ret != EINVAL && ret != ENOTTY)
			return fail("can set invalid audio output %d\n", i);
		if (valid && ret)
			return fail("can't set valid audio output %d\n", i);
	}
	return 0;
}

int testOutputAudio(struct node *node)
{
	struct v4l2_output voutput;
	unsigned o = 0;
	int ret;

	if (node->audio_outputs && node->outputs == 0)
		return fail("audio outputs found but no video outputs?!\n");

	for (o = 0; o < node->outputs; o++) {
		ret = doioctl(node, VIDIOC_S_OUTPUT, &o);
		if (ret)
			return fail("could not select output %d\n", o);
		voutput.index = o;
		ret = doioctl(node, VIDIOC_ENUMOUTPUT, &voutput);
		if (ret)
			return fail("could not enumerate output %d\n", o);
		if (checkOutputAudioSet(node, voutput.audioset))
			return fail("invalid audioset for output %d\n", o);
	}

	if (node->audio_outputs == 0 && node->audio_inputs == 0 && (node->g_caps() & V4L2_CAP_AUDIO))
		return fail("no audio inputs or outputs reported, but CAP_AUDIO set\n");
	return node->audio_outputs ? 0 : ENOTTY;
}
