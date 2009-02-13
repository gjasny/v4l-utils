/* qv4l2: a control panel controlling v4l2 devices.
 *
 * Copyright (C) 2006 Hans Verkuil <hverkuil@xs4all.nl>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include "qv4l2.h"
#include "general-tab.h"
#include "libv4l2util.h"

#include <qlabel.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qwhatsthis.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

GeneralTab::GeneralTab(const char *device, int _fd, int n, QWidget *parent) :
	QGrid(n, parent),
	fd(_fd)
{
	int cnt = 0;

	setSpacing(3);

	memset(&querycap, 0, sizeof(querycap));
	if (ioctl(fd, VIDIOC_QUERYCAP, &querycap) >=0) {
		QLabel *l1 = new QLabel("Device:", this);
		new QLabel(device, this);
		l1->setAlignment(Qt::AlignRight);

		QLabel *l2 = new QLabel("Driver:", this);
		l2->setAlignment(Qt::AlignRight);

		new QLabel((char *)querycap.driver, this);

		QLabel *l3 = new QLabel("Card:", this);
		l3->setAlignment(Qt::AlignRight);

		new QLabel((char *)querycap.card, this);

		QLabel *l4 = new QLabel("Bus:", this);
		l4->setAlignment(Qt::AlignRight);

		new QLabel((char *)querycap.bus_info, this);
	}

	memset(&tuner, 0, sizeof(tuner));
	ioctl(fd, VIDIOC_G_TUNER, &tuner);
	if (tuner.rangehigh>INT_MAX)
		tuner.rangehigh=INT_MAX;

	struct v4l2_input vin;
	memset(&vin, 0, sizeof(vin));
	if (ioctl(fd, VIDIOC_ENUMINPUT, &vin) >= 0) {
		QLabel *label = new QLabel("Input", this);
		label->setAlignment(Qt::AlignRight);
		videoInput = new QComboBox(this);
		while (ioctl(fd, VIDIOC_ENUMINPUT, &vin) >= 0) {
			videoInput->insertItem((char *)vin.name);
			vin.index++;
		}
		connect(videoInput, SIGNAL(activated(int)), SLOT(inputChanged(int)));
		updateVideoInput();
		cnt++;
	}

	struct v4l2_output vout;
	memset(&vout, 0, sizeof(vout));
	if (ioctl(fd, VIDIOC_ENUMOUTPUT, &vout) >= 0) {
		QLabel *label = new QLabel("Output", this);
		label->setAlignment(Qt::AlignRight);
		videoOutput = new QComboBox(this);
		while (ioctl(fd, VIDIOC_ENUMOUTPUT, &vout) >= 0) {
			videoOutput->insertItem((char *)vout.name);
			vout.index++;
		}
		connect(videoOutput, SIGNAL(activated(int)), SLOT(outputChanged(int)));
		updateVideoOutput();
		cnt++;
	}

	struct v4l2_audio vaudio;
	memset(&vaudio, 0, sizeof(vaudio));
	if (ioctl(fd, VIDIOC_ENUMAUDIO, &vaudio) >= 0) {
		QLabel *label = new QLabel("Input Audio", this);
		label->setAlignment(Qt::AlignRight);
		audioInput = new QComboBox(this);
		vaudio.index = 0;
		while (ioctl(fd, VIDIOC_ENUMAUDIO, &vaudio) >= 0) {
			audioInput->insertItem((char *)vaudio.name);
			vaudio.index++;
		}
		connect(audioInput, SIGNAL(activated(int)), SLOT(inputAudioChanged(int)));
		updateAudioInput();
		cnt++;
	}

	struct v4l2_audioout vaudioout;
	memset(&vaudioout, 0, sizeof(vaudioout));
	if (ioctl(fd, VIDIOC_ENUMAUDOUT, &vaudioout) >= 0) {
		QLabel *label = new QLabel("Output Audio", this);
		label->setAlignment(Qt::AlignRight);
		audioOutput = new QComboBox(this);
		while (ioctl(fd, VIDIOC_ENUMAUDOUT, &vaudioout) >= 0) {
			audioOutput->insertItem((char *)vaudioout.name);
			vaudioout.index++;
		}
		connect(audioOutput, SIGNAL(activated(int)), SLOT(outputAudioChanged(int)));
		updateAudioOutput();
		cnt++;
	}

	struct v4l2_standard vs;
	memset(&vs, 0, sizeof(vs));
	if (ioctl(fd, VIDIOC_ENUMSTD, &vs) >= 0) {
		QLabel *label = new QLabel("TV Standard", this);
		label->setAlignment(Qt::AlignRight);
		tvStandard = new QComboBox(this);
		while (ioctl(fd, VIDIOC_ENUMSTD, &vs) >= 0) {
			tvStandard->insertItem((char *)vs.name);
			vs.index++;
		}
		connect(tvStandard, SIGNAL(activated(int)), SLOT(standardChanged(int)));
		updateStandard();
		cnt++;
	}

	bool first = cnt & 1;

	if (first) {
		QString what;
		QLabel *label = new QLabel("Frequency", this);
		label->setAlignment(Qt::AlignRight);
		freq = new QSpinBox(tuner.rangelow, tuner.rangehigh, 1, this);
		QWhatsThis::add(freq, what.sprintf("Frequency\n"
					"Low: %d\n"
					"High: %d\n",
					tuner.rangelow, tuner.rangehigh));
		connect(freq, SIGNAL(valueChanged(int)), SLOT(freqChanged(int)));
		updateFreq();
		cnt++;
	}

	{
		QLabel *label = new QLabel("Frequency Tables", this);
		label->setAlignment(Qt::AlignRight);
		freqTable = new QComboBox(this);
		for (int i = 0; v4l2_channel_lists[i].name; i++) {
			freqTable->insertItem(v4l2_channel_lists[i].name);
		}
		connect(freqTable, SIGNAL(activated(int)), SLOT(freqTableChanged(int)));

		label = new QLabel("Channels", this);
		label->setAlignment(Qt::AlignRight);
		freqChannel = new QComboBox(this);
		connect(freqChannel, SIGNAL(activated(int)), SLOT(freqChannelChanged(int)));
		updateFreqChannel();
	}

	if (!first) {
		QString what;
		QLabel *label = new QLabel("Frequency", this);
		label->setAlignment(Qt::AlignRight);
		freq = new QSpinBox(tuner.rangelow, tuner.rangehigh, 1, this);
		QWhatsThis::add(freq, what.sprintf("Frequency\n"
					"Low: %d\n"
					"High: %d\n",
					tuner.rangelow, tuner.rangehigh));
		connect(freq, SIGNAL(valueChanged(int)), SLOT(freqChanged(int)));
		updateFreq();
		cnt++;
	}

	if (cnt & 1) {
		new QWidget(this);
		new QWidget(this);
	}
	QWidget *stretch = new QWidget(this);
	stretch->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
}

void GeneralTab::inputChanged(int input)
{
	g_mw->doIoctl("Set Input", VIDIOC_S_INPUT, &input);
	struct v4l2_audio vaudio;
	memset(&vaudio, 0, sizeof(vaudio));
	if (audioInput && ioctl(fd, VIDIOC_G_AUDIO, &vaudio) >= 0) {
		audioInput->setCurrentItem(vaudio.index);
		updateAudioInput();
	}
	updateVideoInput();
}

void GeneralTab::outputChanged(int output)
{
	g_mw->doIoctl("Set Output", VIDIOC_S_OUTPUT, &output);
	updateVideoOutput();
}

void GeneralTab::inputAudioChanged(int input)
{
	struct v4l2_audio vaudio;
	memset(&vaudio, 0, sizeof(vaudio));
	vaudio.index = input;
	g_mw->doIoctl("Set Audio Input", VIDIOC_S_AUDIO, &vaudio);
	updateAudioInput();
}

void GeneralTab::outputAudioChanged(int output)
{
	struct v4l2_audioout vaudioout;
	memset(&vaudioout, 0, sizeof(vaudioout));
	vaudioout.index = output;
	g_mw->doIoctl("Set Audio Output", VIDIOC_S_AUDOUT, &vaudioout);
	updateAudioOutput();
}

void GeneralTab::standardChanged(int std)
{
	struct v4l2_standard vs;
	memset(&vs, 0, sizeof(vs));
	vs.index = std;
	ioctl(fd, VIDIOC_ENUMSTD, &vs);
	g_mw->doIoctl("Set TV Standard", VIDIOC_S_STD, &vs.id);
	updateStandard();
}

void GeneralTab::freqTableChanged(int)
{
	updateFreqChannel();
	freqChannelChanged(0);
}

void GeneralTab::freqChannelChanged(int idx)
{
	freq->setValue((int)(v4l2_channel_lists[freqTable->currentItem()].list[idx].freq / 62.5));
}

void GeneralTab::freqChanged(int val)
{
	struct v4l2_frequency f;

	memset(&f, 0, sizeof(f));
	f.type = V4L2_TUNER_ANALOG_TV;
	f.frequency = val;
	g_mw->doIoctl("Set frequency", VIDIOC_S_FREQUENCY, &f);
}

void GeneralTab::updateVideoInput()
{
	int input;

	ioctl(fd, VIDIOC_G_INPUT, &input);
	videoInput->setCurrentItem(input);
}

void GeneralTab::updateVideoOutput()
{
	int output;

	ioctl(fd, VIDIOC_G_OUTPUT, &output);
	videoOutput->setCurrentItem(output);
}

void GeneralTab::updateAudioInput()
{
	struct v4l2_audio audio;
	QString what;

	memset(&audio, 0, sizeof(audio));
	ioctl(fd, VIDIOC_G_AUDIO, &audio);
	audioInput->setCurrentItem(audio.index);
	if (audio.capability & V4L2_AUDCAP_STEREO)
		what = "stereo input";
	else
		what = "mono input";
	if (audio.capability & V4L2_AUDCAP_AVL)
		what += ", has AVL";
	if (audio.mode & V4L2_AUDMODE_AVL)
		what += ", AVL is on";
	QWhatsThis::add(audioInput, what);
}

void GeneralTab::updateAudioOutput()
{
	struct v4l2_audioout audio;

	memset(&audio, 0, sizeof(audio));
	ioctl(fd, VIDIOC_G_AUDOUT, &audio);
	audioOutput->setCurrentItem(audio.index);
}

void GeneralTab::updateStandard()
{
	v4l2_std_id std;
	struct v4l2_standard vs;
	QString what;
	ioctl(fd, VIDIOC_G_STD, &std);
	memset(&vs, 0, sizeof(vs));
	while (ioctl(fd, VIDIOC_ENUMSTD, &vs) != -1) {
		if (vs.id & std) {
			tvStandard->setCurrentItem(vs.index);
			what.sprintf("TV Standard (0x%llX)\n"
				"Frame period: %f (%d/%d)\n"
				"Frame lines: %d\n", (long long int)std,
				(double)vs.frameperiod.numerator / vs.frameperiod.denominator,
				vs.frameperiod.numerator, vs.frameperiod.denominator,
				vs.framelines);
			QWhatsThis::add(tvStandard, what);
			return;
		}
		vs.index++;
	}
}

void GeneralTab::updateFreq()
{
	struct v4l2_frequency f;

	memset(&f, 0, sizeof(f));
	ioctl(fd, VIDIOC_G_FREQUENCY, &f);
	freq->setValue(f.frequency);
}

void GeneralTab::updateFreqChannel()
{
	freqChannel->clear();
	int tbl = freqTable->currentItem();
	const struct v4l2_channel_list *list = v4l2_channel_lists[tbl].list;
	for (unsigned i = 0; i < v4l2_channel_lists[tbl].count; i++)
		freqChannel->insertItem(list[i].name);
}

