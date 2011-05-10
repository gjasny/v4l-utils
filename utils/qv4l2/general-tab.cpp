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


#include "general-tab.h"
#include <libv4l2util.h>

#include <QSpinBox>
#include <QComboBox>

#include <errno.h>

GeneralTab::GeneralTab(const QString &device, v4l2 &fd, int n, QWidget *parent) :
	QGridLayout(parent),
	v4l2(fd),
	m_row(0),
	m_col(0),
	m_cols(n),
	m_audioInput(NULL),
	m_tvStandard(NULL),
	m_videoPreset(NULL),
	m_freq(NULL),
	m_vidCapFormats(NULL),
	m_frameSize(NULL),
	m_vidOutFormats(NULL)
{
	setSpacing(3);

	setSizeConstraint(QLayout::SetMinimumSize);

	if (querycap(m_querycap)) {
		addLabel("Device:");
		addLabel(device + (useWrapper() ? " (wrapped)" : ""), Qt::AlignLeft);

		addLabel("Driver:");
		addLabel((char *)m_querycap.driver, Qt::AlignLeft);

		addLabel("Card:");
		addLabel((char *)m_querycap.card, Qt::AlignLeft);

		addLabel("Bus:");
		addLabel((char *)m_querycap.bus_info, Qt::AlignLeft);
	}

	g_tuner(m_tuner);

	v4l2_standard vs;
	if (enum_std(vs, true)) {
		addLabel("TV Standard");
		m_tvStandard = new QComboBox(parent);
		do {
			m_tvStandard->addItem((char *)vs.name);
		} while (enum_std(vs));
		addWidget(m_tvStandard);
		connect(m_tvStandard, SIGNAL(activated(int)), SLOT(standardChanged(int)));
		updateStandard();
	}

	v4l2_dv_enum_preset preset;
	if (enum_dv_preset(preset, true)) {
		addLabel("Video Preset");
		m_videoPreset = new QComboBox(parent);
		do {
			m_videoPreset->addItem((char *)preset.name);
		} while (enum_dv_preset(preset));
		addWidget(m_videoPreset);
		connect(m_videoPreset, SIGNAL(activated(int)), SLOT(presetChanged(int)));
		updatePreset();
	}

	v4l2_input vin;
	if (enum_input(vin, true)) {
		addLabel("Input");
		m_videoInput = new QComboBox(parent);
		do {
			m_videoInput->addItem((char *)vin.name);
		} while (enum_input(vin));
		addWidget(m_videoInput);
		connect(m_videoInput, SIGNAL(activated(int)), SLOT(inputChanged(int)));
		updateVideoInput();
	}

	v4l2_output vout;
	if (enum_output(vout, true)) {
		addLabel("Output");
		m_videoOutput = new QComboBox(parent);
		do {
			m_videoOutput->addItem((char *)vout.name);
		} while (enum_output(vout));
		addWidget(m_videoOutput);
		connect(m_videoOutput, SIGNAL(activated(int)), SLOT(outputChanged(int)));
		updateVideoOutput();
	}

	v4l2_audio vaudio;
	if (enum_audio(vaudio, true)) {
		addLabel("Input Audio");
		m_audioInput = new QComboBox(parent);
		do {
			m_audioInput->addItem((char *)vaudio.name);
		} while (enum_audio(vaudio));
		addWidget(m_audioInput);
		connect(m_audioInput, SIGNAL(activated(int)), SLOT(inputAudioChanged(int)));
		updateAudioInput();
	}

	v4l2_audioout vaudout;
	if (enum_audout(vaudout, true)) {
		addLabel("Output Audio");
		m_audioOutput = new QComboBox(parent);
		do {
			m_audioOutput->addItem((char *)vaudout.name);
		} while (enum_audout(vaudout));
		addWidget(m_audioOutput);
		connect(m_audioOutput, SIGNAL(activated(int)), SLOT(outputAudioChanged(int)));
		updateAudioOutput();
	}

	if (m_tuner.type) {
		m_freq = new QSpinBox(parent);
		m_freq->setMinimum(m_tuner.rangelow);
		m_freq->setMaximum(m_tuner.rangehigh);
		m_freq->setWhatsThis(QString("Frequency\nLow: %1\nHigh: %2")
				.arg(m_tuner.rangelow).arg(m_tuner.rangehigh));
		connect(m_freq, SIGNAL(valueChanged(int)), SLOT(freqChanged(int)));
		updateFreq();
		addLabel("Frequency");
		addWidget(m_freq);

		addLabel("Frequency Table");
		m_freqTable = new QComboBox(parent);
		for (int i = 0; v4l2_channel_lists[i].name; i++) {
			m_freqTable->addItem(v4l2_channel_lists[i].name);
		}
		addWidget(m_freqTable);
		connect(m_freqTable, SIGNAL(activated(int)), SLOT(freqTableChanged(int)));

		addLabel("Channels");
		m_freqChannel = new QComboBox(parent);
		m_freqChannel->setSizeAdjustPolicy(QComboBox::AdjustToContents);
		addWidget(m_freqChannel);
		connect(m_freqChannel, SIGNAL(activated(int)), SLOT(freqChannelChanged(int)));
		updateFreqChannel();
	}

	v4l2_fmtdesc fmt;
	addLabel("Capture Image Formats");
	m_vidCapFormats = new QComboBox(parent);
	if (enum_fmt_cap(fmt, true)) {
		do {
			m_vidCapFormats->addItem(pixfmt2s(fmt.pixelformat) +
					" - " + (const char *)fmt.description);
		} while (enum_fmt_cap(fmt));
	}
	addWidget(m_vidCapFormats);
	connect(m_vidCapFormats, SIGNAL(activated(int)), SLOT(vidCapFormatChanged(int)));

	addLabel("Frame Width");
	m_frameWidth = new QSpinBox(parent);
	addWidget(m_frameWidth);
	connect(m_frameWidth, SIGNAL(editingFinished()), SLOT(frameWidthChanged()));
	addLabel("Frame Height");
	m_frameHeight = new QSpinBox(parent);
	addWidget(m_frameHeight);
	connect(m_frameHeight, SIGNAL(editingFinished()), SLOT(frameHeightChanged()));

	addLabel("Frame Size");
	m_frameSize = new QComboBox(parent);
	m_frameSize->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	addWidget(m_frameSize);
	connect(m_frameSize, SIGNAL(activated(int)), SLOT(frameSizeChanged(int)));

	addLabel("Frame Interval");
	m_frameInterval = new QComboBox(parent);
	m_frameInterval->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	addWidget(m_frameInterval);
	connect(m_frameInterval, SIGNAL(activated(int)), SLOT(frameIntervalChanged(int)));

	updateVidCapFormat();

	if (caps() & V4L2_CAP_VIDEO_OUTPUT) {
		addLabel("Output Image Formats");
		m_vidOutFormats = new QComboBox(parent);
		if (enum_fmt_out(fmt, true)) {
			do {
				m_vidOutFormats->addItem(pixfmt2s(fmt.pixelformat) +
						" - " + (const char *)fmt.description);
			} while (enum_fmt_out(fmt));
		}
		addWidget(m_vidOutFormats);
		connect(m_vidOutFormats, SIGNAL(activated(int)), SLOT(vidOutFormatChanged(int)));
	}

	addLabel("Capture Method");
	m_capMethods = new QComboBox(parent);
	if (m_querycap.capabilities & V4L2_CAP_STREAMING) {
		v4l2_requestbuffers reqbuf;

		// Yuck. The videobuf framework does not accept a count of 0.
		// This is out-of-spec, but it means that the only way to test which
		// method is supported is to give it a non-zero count. But non-videobuf
		// drivers like uvc do not allow e.g. S_FMT calls after a REQBUFS call
		// with non-zero counts unless there is a REQBUFS call with count == 0
		// in between. This is actual proper behavior, although somewhat
		// unexpected. So the only way at the moment to do this that works
		// everywhere is to call REQBUFS with a count of 1, and then again with
		// a count of 0.
		if (reqbufs_user_cap(reqbuf, 1)) {
			m_capMethods->addItem("User pointer I/O", QVariant(methodUser));
			reqbufs_user_cap(reqbuf, 0);
		}
		if (reqbufs_mmap_cap(reqbuf, 1)) {
			m_capMethods->addItem("Memory mapped I/O", QVariant(methodMmap));
			reqbufs_mmap_cap(reqbuf, 0);
		}
	}
	if (m_querycap.capabilities & V4L2_CAP_READWRITE) {
		m_capMethods->addItem("read()", QVariant(methodRead));
	}
	addWidget(m_capMethods);

	QGridLayout::addWidget(new QWidget(parent), rowCount(), 0, 1, n);
	setRowStretch(rowCount() - 1, 1);
}

void GeneralTab::addWidget(QWidget *w, Qt::Alignment align)
{
	QGridLayout::addWidget(w, m_row, m_col, align | Qt::AlignVCenter);
	m_col++;
	if (m_col == m_cols) {
		m_col = 0;
		m_row++;
	}
}

CapMethod GeneralTab::capMethod()
{
	return (CapMethod)m_capMethods->itemData(m_capMethods->currentIndex()).toInt();
}

void GeneralTab::inputChanged(int input)
{
	s_input(input);

	if (m_audioInput)
		updateAudioInput();

	updateVideoInput();
}

void GeneralTab::outputChanged(int output)
{
	s_output(output);
	updateVideoOutput();
}

void GeneralTab::inputAudioChanged(int input)
{
	s_audio(input);
	updateAudioInput();
}

void GeneralTab::outputAudioChanged(int output)
{
	s_audout(output);
	updateAudioOutput();
}

void GeneralTab::standardChanged(int std)
{
	v4l2_standard vs;

	enum_std(vs, true, std);
	s_std(vs.id);
	updateStandard();
}

void GeneralTab::presetChanged(int index)
{
	v4l2_dv_enum_preset preset;

	enum_dv_preset(preset, true, index);
	s_dv_preset(preset.preset);
	updatePreset();
}

void GeneralTab::freqTableChanged(int)
{
	updateFreqChannel();
	freqChannelChanged(0);
}

void GeneralTab::freqChannelChanged(int idx)
{
	m_freq->setValue((int)(v4l2_channel_lists[m_freqTable->currentIndex()].list[idx].freq / 62.5));
}

void GeneralTab::freqChanged(int val)
{
	s_frequency(val);
}

void GeneralTab::vidCapFormatChanged(int idx)
{
	v4l2_fmtdesc desc;

	enum_fmt_cap(desc, true, idx);

	v4l2_format fmt;

	g_fmt_cap(fmt);
	fmt.fmt.pix.pixelformat = desc.pixelformat;
	if (try_fmt(fmt))
		s_fmt(fmt);

	updateVidCapFormat();
}

void GeneralTab::frameWidthChanged()
{
	v4l2_format fmt;
	int val = m_frameWidth->value();

	g_fmt_cap(fmt);
	fmt.fmt.pix.width = val;
	if (try_fmt(fmt))
		s_fmt(fmt);

	updateVidCapFormat();
}

void GeneralTab::frameHeightChanged()
{
	v4l2_format fmt;
	int val = m_frameHeight->value();

	g_fmt_cap(fmt);
	fmt.fmt.pix.height = val;
	if (try_fmt(fmt))
		s_fmt(fmt);

	updateVidCapFormat();
}

void GeneralTab::frameSizeChanged(int idx)
{
	v4l2_frmsizeenum frmsize;

	if (enum_framesizes(frmsize, m_pixelformat, idx)) {
		v4l2_format fmt;

		g_fmt_cap(fmt);
		fmt.fmt.pix.width = frmsize.discrete.width;
		fmt.fmt.pix.height = frmsize.discrete.height;
		if (try_fmt(fmt))
			s_fmt(fmt);
	}
	updateVidCapFormat();
}

void GeneralTab::frameIntervalChanged(int idx)
{
	v4l2_frmivalenum frmival;

	if (enum_frameintervals(frmival, m_pixelformat, m_width, m_height, idx)
	    && frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
		set_interval(frmival.discrete);
	}
}

void GeneralTab::vidOutFormatChanged(int idx)
{
	v4l2_fmtdesc desc;

	enum_fmt_out(desc, true, idx);

	v4l2_format fmt;

	g_fmt_out(fmt);
	fmt.fmt.pix.pixelformat = desc.pixelformat;
	if (try_fmt(fmt))
		s_fmt(fmt);
	updateVidOutFormat();
}

void GeneralTab::updateVideoInput()
{
	int input;
	v4l2_input in;

	g_input(input);
	enum_input(in, true, input);
	m_videoInput->setCurrentIndex(input);
	if (m_tvStandard)
		m_tvStandard->setEnabled(in.capabilities & V4L2_IN_CAP_STD);
	if (m_videoPreset)
		m_videoPreset->setEnabled(in.capabilities & V4L2_IN_CAP_PRESETS);
}

void GeneralTab::updateVideoOutput()
{
	int output;
	v4l2_output out;

	g_output(output);
	enum_output(out, true, output);
	m_videoOutput->setCurrentIndex(output);
	if (m_tvStandard)
		m_tvStandard->setEnabled(out.capabilities & V4L2_OUT_CAP_STD);
	if (m_videoPreset)
		m_videoPreset->setEnabled(out.capabilities & V4L2_OUT_CAP_PRESETS);
}

void GeneralTab::updateAudioInput()
{
	v4l2_audio audio;
	QString what;

	g_audio(audio);
	m_audioInput->setCurrentIndex(audio.index);
	if (audio.capability & V4L2_AUDCAP_STEREO)
		what = "stereo input";
	else
		what = "mono input";
	if (audio.capability & V4L2_AUDCAP_AVL)
		what += ", has AVL";
	if (audio.mode & V4L2_AUDMODE_AVL)
		what += ", AVL is on";
	m_audioInput->setWhatsThis(what);
}

void GeneralTab::updateAudioOutput()
{
	v4l2_audioout audio;

	g_audout(audio);
	m_audioOutput->setCurrentIndex(audio.index);
}

void GeneralTab::updateStandard()
{
	v4l2_std_id std;
	v4l2_standard vs;
	QString what;

	g_std(std);
	if (enum_std(vs, true)) {
		do {
			if (vs.id == std)
				break;
		} while (enum_std(vs));
	}
	if (vs.id != std) {
		if (enum_std(vs, true)) {
			do {
				if (vs.id & std)
					break;
			} while (enum_std(vs));
		}
	}
	if ((vs.id & std) == 0)
		return;
	m_tvStandard->setCurrentIndex(vs.index);
	what.sprintf("TV Standard (0x%llX)\n"
		"Frame period: %f (%d/%d)\n"
		"Frame lines: %d", (long long int)std,
		(double)vs.frameperiod.numerator / vs.frameperiod.denominator,
		vs.frameperiod.numerator, vs.frameperiod.denominator,
		vs.framelines);
	m_tvStandard->setWhatsThis(what);
}

void GeneralTab::updatePreset()
{
	__u32 preset;
	v4l2_dv_enum_preset p;
	QString what;

	g_dv_preset(preset);
	if (enum_dv_preset(p, true)) {
		do {
			if (p.preset == preset)
				break;
		} while (enum_dv_preset(p));
	}
	if (p.preset != preset)
		return;
	m_videoPreset->setCurrentIndex(p.index);
	what.sprintf("Video Preset (%u)\n"
		"Frame %ux%u\n",
		p.preset, p.width, p.height);
	m_videoPreset->setWhatsThis(what);
}

void GeneralTab::updateFreq()
{
	v4l2_frequency f;

	g_frequency(f);
	/* m_freq listens to valueChanged block it to avoid recursion */
	m_freq->blockSignals(true);
	m_freq->setValue(f.frequency);
	m_freq->blockSignals(false);
}

void GeneralTab::updateFreqChannel()
{
	m_freqChannel->clear();
	int tbl = m_freqTable->currentIndex();
	const struct v4l2_channel_list *list = v4l2_channel_lists[tbl].list;
	for (unsigned i = 0; i < v4l2_channel_lists[tbl].count; i++)
		m_freqChannel->addItem(list[i].name);
}

void GeneralTab::updateVidCapFormat()
{
	v4l2_fmtdesc desc;
	v4l2_format fmt;

	g_fmt_cap(fmt);
	m_pixelformat = fmt.fmt.pix.pixelformat;
	m_width       = fmt.fmt.pix.width;
	m_height      = fmt.fmt.pix.height;
	updateFrameSize();
	if (enum_fmt_cap(desc, true)) {
		do {
			if (desc.pixelformat == fmt.fmt.pix.pixelformat)
				break;
		} while (enum_fmt_cap(desc));
	}
	if (desc.pixelformat != fmt.fmt.pix.pixelformat)
		return;
	m_vidCapFormats->setCurrentIndex(desc.index);
}

void GeneralTab::updateFrameSize()
{
	v4l2_frmsizeenum frmsize;
	bool ok = false;

	m_frameSize->clear();

	ok = enum_framesizes(frmsize, m_pixelformat);
	if (ok && frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
		do {
			m_frameSize->addItem(QString("%1x%2")
				.arg(frmsize.discrete.width).arg(frmsize.discrete.height));
			if (frmsize.discrete.width == m_width &&
			    frmsize.discrete.height == m_height)
				m_frameSize->setCurrentIndex(frmsize.index);
		} while (enum_framesizes(frmsize));

		m_frameWidth->setEnabled(false);
		m_frameHeight->setEnabled(false);
		m_frameSize->setEnabled(true);
		updateFrameInterval();
		return;
	}
	if (!ok) {
		frmsize.stepwise.min_width = 8;
		frmsize.stepwise.max_width = 1920;
		frmsize.stepwise.step_width = 1;
		frmsize.stepwise.min_height = 8;
		frmsize.stepwise.max_height = 1200;
		frmsize.stepwise.step_height = 1;
	}
	m_frameWidth->setEnabled(true);
	m_frameHeight->setEnabled(true);
	m_frameSize->setEnabled(false);
	m_frameWidth->setMinimum(frmsize.stepwise.min_width);
	m_frameWidth->setMaximum(frmsize.stepwise.max_width);
	m_frameWidth->setSingleStep(frmsize.stepwise.step_width);
	m_frameWidth->setValue(m_width);
	m_frameHeight->setMinimum(frmsize.stepwise.min_height);
	m_frameHeight->setMaximum(frmsize.stepwise.max_height);
	m_frameHeight->setSingleStep(frmsize.stepwise.step_height);
	m_frameHeight->setValue(m_height);
	updateFrameInterval();
}

void GeneralTab::updateFrameInterval()
{
	v4l2_frmivalenum frmival;
	v4l2_fract curr;
	bool curr_ok, ok;

	m_frameInterval->clear();

	ok = enum_frameintervals(frmival, m_pixelformat, m_width, m_height);
	curr_ok = get_interval(curr);
	if (ok && frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
		do {
			m_frameInterval->addItem(QString("%1 fps")
				.arg((double)frmival.discrete.denominator / frmival.discrete.numerator));
			if (curr_ok &&
			    frmival.discrete.numerator == curr.numerator &&
			    frmival.discrete.denominator == curr.denominator)
				m_frameInterval->setCurrentIndex(frmival.index);
		} while (enum_frameintervals(frmival));
	}
}

void GeneralTab::updateVidOutFormat()
{
	v4l2_fmtdesc desc;
	v4l2_format fmt;

	g_fmt_out(fmt);
	if (enum_fmt_out(desc, true)) {
		do {
			if (desc.pixelformat == fmt.fmt.pix.pixelformat)
				break;
		} while (enum_fmt_out(desc));
	}
	if (desc.pixelformat != fmt.fmt.pix.pixelformat)
		return;
	m_vidCapFormats->setCurrentIndex(desc.index);
}
