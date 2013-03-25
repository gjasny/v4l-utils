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
#include "../libv4l2util/libv4l2util.h"

#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLineEdit>
#include <QDoubleValidator>

#include <stdio.h>
#include <errno.h>

GeneralTab::GeneralTab(const QString &device, v4l2 &fd, int n, QWidget *parent) :
	QGridLayout(parent),
	v4l2(fd),
	m_row(0),
	m_col(0),
	m_cols(n),
	m_isRadio(false),
	m_isVbi(false),
	m_audioInput(NULL),
	m_tvStandard(NULL),
	m_qryStandard(NULL),
	m_videoTimings(NULL),
	m_qryTimings(NULL),
	m_freq(NULL),
	m_vidCapFormats(NULL),
	m_frameSize(NULL),
	m_vidOutFormats(NULL),
	m_vbiMethods(NULL)
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
	g_modulator(m_modulator);

	v4l2_input vin;
	bool needsStd = false;
	bool needsTimings = false;

	if (m_tuner.capability && m_tuner.capability & V4L2_TUNER_CAP_LOW)
		m_isRadio = true;
	if (m_modulator.capability && m_modulator.capability & V4L2_TUNER_CAP_LOW)
		m_isRadio = true;
	if (m_querycap.capabilities & V4L2_CAP_DEVICE_CAPS)
		m_isVbi = caps() & (V4L2_CAP_VBI_CAPTURE | V4L2_CAP_SLICED_VBI_CAPTURE);

	if (!isRadio() && enum_input(vin, true)) {
		addLabel("Input");
		m_videoInput = new QComboBox(parent);
		do {
			m_videoInput->addItem((char *)vin.name);
			if (vin.capabilities & V4L2_IN_CAP_STD)
				needsStd = true;
			if (vin.capabilities & V4L2_IN_CAP_DV_TIMINGS)
				needsTimings = true;
		} while (enum_input(vin));
		addWidget(m_videoInput);
		connect(m_videoInput, SIGNAL(activated(int)), SLOT(inputChanged(int)));
	}

	v4l2_output vout;
	if (!isRadio() && enum_output(vout, true)) {
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
	if (!isRadio() && enum_audio(vaudio, true)) {
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
	if (!isRadio() && enum_audout(vaudout, true)) {
		addLabel("Output Audio");
		m_audioOutput = new QComboBox(parent);
		do {
			m_audioOutput->addItem((char *)vaudout.name);
		} while (enum_audout(vaudout));
		addWidget(m_audioOutput);
		connect(m_audioOutput, SIGNAL(activated(int)), SLOT(outputAudioChanged(int)));
		updateAudioOutput();
	}

	if (needsStd) {
		v4l2_std_id tmp;

		addLabel("TV Standard");
		m_tvStandard = new QComboBox(parent);
		addWidget(m_tvStandard);
		connect(m_tvStandard, SIGNAL(activated(int)), SLOT(standardChanged(int)));
		refreshStandards();
		if (ioctl_exists(VIDIOC_QUERYSTD, &tmp)) {
			addLabel("");
			m_qryStandard = new QPushButton("Query Standard", parent);
			addWidget(m_qryStandard);
			connect(m_qryStandard, SIGNAL(clicked()), SLOT(qryStdClicked()));
		}
	}

	if (needsTimings) {
		addLabel("Video Timings");
		m_videoTimings = new QComboBox(parent);
		addWidget(m_videoTimings);
		connect(m_videoTimings, SIGNAL(activated(int)), SLOT(timingsChanged(int)));
		refreshTimings();
		addLabel("");
		m_qryTimings = new QPushButton("Query Timings", parent);
		addWidget(m_qryTimings);
		connect(m_qryTimings, SIGNAL(clicked()), SLOT(qryTimingsClicked()));
	}

	if (m_tuner.capability) {
		QDoubleValidator *val;
		double factor = (m_tuner.capability & V4L2_TUNER_CAP_LOW) ? 16 : 16000;

		val = new QDoubleValidator(m_tuner.rangelow / factor, m_tuner.rangehigh / factor, 3, parent);
		m_freq = new QLineEdit(parent);
		m_freq->setValidator(val);
		m_freq->setWhatsThis(QString("Frequency\nLow: %1\nHigh: %2")
				.arg(m_tuner.rangelow / factor).arg(m_tuner.rangehigh / factor));
		connect(m_freq, SIGNAL(lostFocus()), SLOT(freqChanged()));
		connect(m_freq, SIGNAL(returnPressed()), SLOT(freqChanged()));
		updateFreq();
		if (m_tuner.capability & V4L2_TUNER_CAP_LOW)
			addLabel("Frequency (kHz)");
		else
			addLabel("Frequency (MHz)");
		addWidget(m_freq);

		if (!m_tuner.capability & V4L2_TUNER_CAP_LOW) {
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
		addLabel("Audio Mode");
		m_audioMode = new QComboBox(parent);
		m_audioMode->setSizeAdjustPolicy(QComboBox::AdjustToContents);
		m_audioMode->addItem("Mono");
		int audIdx = 0;
		m_audioModes[audIdx++] = V4L2_TUNER_MODE_MONO;
		if (m_tuner.capability & V4L2_TUNER_CAP_STEREO) {
			m_audioMode->addItem("Stereo");
			m_audioModes[audIdx++] = V4L2_TUNER_MODE_STEREO;
		}
		if (m_tuner.capability & V4L2_TUNER_CAP_LANG1) {
			m_audioMode->addItem("Language 1");
			m_audioModes[audIdx++] = V4L2_TUNER_MODE_LANG1;
		}
		if (m_tuner.capability & V4L2_TUNER_CAP_LANG2) {
			m_audioMode->addItem("Language 2");
			m_audioModes[audIdx++] = V4L2_TUNER_MODE_LANG2;
		}
		if ((m_tuner.capability & (V4L2_TUNER_CAP_LANG1 | V4L2_TUNER_CAP_LANG2)) ==
				(V4L2_TUNER_CAP_LANG1 | V4L2_TUNER_CAP_LANG2)) {
			m_audioMode->addItem("Language 1+2");
			m_audioModes[audIdx++] = V4L2_TUNER_MODE_LANG1_LANG2;
		}
		addWidget(m_audioMode);
		for (int i = 0; i < audIdx; i++)
			if (m_audioModes[i] == m_tuner.audmode)
				m_audioMode->setCurrentIndex(i);
		connect(m_audioMode, SIGNAL(activated(int)), SLOT(audioModeChanged(int)));
		m_subchannels = new QLabel("", parent);
		addWidget(m_subchannels, Qt::AlignRight);
		m_detectSubchans = new QPushButton("Refresh Tuner Status", parent);
		addWidget(m_detectSubchans);
		connect(m_detectSubchans, SIGNAL(clicked()), SLOT(detectSubchansClicked()));
		detectSubchansClicked();
	}

	if (m_modulator.capability) {
		QDoubleValidator *val;
		double factor = (m_modulator.capability & V4L2_TUNER_CAP_LOW) ? 16 : 16000;

		val = new QDoubleValidator(m_modulator.rangelow / factor, m_modulator.rangehigh / factor, 3, parent);
		m_freq = new QLineEdit(parent);
		m_freq->setValidator(val);
		m_freq->setWhatsThis(QString("Frequency\nLow: %1\nHigh: %2")
				.arg(m_modulator.rangelow / factor).arg(m_modulator.rangehigh / factor));
		connect(m_freq, SIGNAL(lostFocus()), SLOT(freqChanged()));
		connect(m_freq, SIGNAL(returnPressed()), SLOT(freqChanged()));
		updateFreq();
		if (m_modulator.capability & V4L2_TUNER_CAP_LOW)
			addLabel("Frequency (kHz)");
		else
			addLabel("Frequency (MHz)");
		addWidget(m_freq);
		if (m_modulator.capability & V4L2_TUNER_CAP_STEREO) {
			addLabel("Stereo");
			m_stereoMode = new QCheckBox(parent);
			m_stereoMode->setCheckState((m_modulator.txsubchans & V4L2_TUNER_SUB_STEREO) ?
					Qt::Checked : Qt::Unchecked);
			addWidget(m_stereoMode);
			connect(m_stereoMode, SIGNAL(clicked()), SLOT(stereoModeChanged()));
		}
		if (m_modulator.capability & V4L2_TUNER_CAP_RDS) {
			addLabel("RDS");
			m_rdsMode = new QCheckBox(parent);
			m_rdsMode->setCheckState((m_modulator.txsubchans & V4L2_TUNER_SUB_RDS) ?
					Qt::Checked : Qt::Unchecked);
			addWidget(m_rdsMode);
			connect(m_rdsMode, SIGNAL(clicked()), SLOT(rdsModeChanged()));
		}
	}

	if (isRadio())
		goto done;

	if (isVbi()) {
		addLabel("VBI Capture Method");
		m_vbiMethods = new QComboBox(parent);
		if (caps() & V4L2_CAP_VBI_CAPTURE)
			m_vbiMethods->addItem("Raw");
		if (caps() & V4L2_CAP_SLICED_VBI_CAPTURE)
			m_vbiMethods->addItem("Sliced");
		addWidget(m_vbiMethods);
		connect(m_vbiMethods, SIGNAL(activated(int)), SLOT(vbiMethodsChanged(int)));
		updateVideoInput();
		goto capture_method;
	}

	v4l2_fmtdesc fmt;
	addLabel("Capture Image Formats");
	m_vidCapFormats = new QComboBox(parent);
	if (enum_fmt_cap(fmt, true)) {
		do {
			QString s(pixfmt2s(fmt.pixelformat) + " (");

			if (fmt.flags & V4L2_FMT_FLAG_EMULATED)
				m_vidCapFormats->addItem(s + "Emulated)");
			else
				m_vidCapFormats->addItem(s + (const char *)fmt.description + ")");
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

	updateVideoInput();
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

capture_method:
	addLabel("Capture Method");
	m_capMethods = new QComboBox(parent);
	m_buftype = isSlicedVbi() ? V4L2_BUF_TYPE_SLICED_VBI_CAPTURE :
		(isVbi() ? V4L2_BUF_TYPE_VBI_CAPTURE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (caps() & V4L2_CAP_STREAMING) {
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
		if (reqbufs_user(reqbuf, 1)) {
			m_capMethods->addItem("User pointer I/O", QVariant(methodUser));
			reqbufs_user(reqbuf, 0);
		}
		if (reqbufs_mmap(reqbuf, 1)) {
			m_capMethods->addItem("Memory mapped I/O", QVariant(methodMmap));
			reqbufs_mmap(reqbuf, 0);
		}
	}
	if (caps() & V4L2_CAP_READWRITE) {
		m_capMethods->addItem("read()", QVariant(methodRead));
	}
	addWidget(m_capMethods);

done:
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

bool GeneralTab::isSlicedVbi() const
{
	return m_vbiMethods && m_vbiMethods->currentText() == "Sliced";
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

void GeneralTab::timingsChanged(int index)
{
	v4l2_enum_dv_timings timings;

	enum_dv_timings(timings, true, index);
	s_dv_timings(timings.timings);
	updateTimings();
}

void GeneralTab::freqTableChanged(int)
{
	updateFreqChannel();
	freqChannelChanged(0);
}

void GeneralTab::freqChannelChanged(int idx)
{
	double f = v4l2_channel_lists[m_freqTable->currentIndex()].list[idx].freq;

	m_freq->setText(QString::number(f / 1000.0));
	freqChanged();
}

void GeneralTab::freqChanged()
{
	double f = m_freq->text().toDouble();

	s_frequency(f * 16, m_isRadio);
}

void GeneralTab::audioModeChanged(int)
{
	m_tuner.audmode = m_audioModes[m_audioMode->currentIndex()];
	s_tuner(m_tuner);
}

void GeneralTab::detectSubchansClicked()
{
	QString chans;

	g_tuner(m_tuner);
	if (m_tuner.rxsubchans & V4L2_TUNER_SUB_MONO)
		chans += "Mono ";
	if (m_tuner.rxsubchans & V4L2_TUNER_SUB_STEREO)
		chans += "Stereo ";
	if (m_tuner.rxsubchans & V4L2_TUNER_SUB_LANG1)
		chans += "Lang1 ";
	if (m_tuner.rxsubchans & V4L2_TUNER_SUB_LANG2)
		chans += "Lang2 ";
	if (m_tuner.rxsubchans & V4L2_TUNER_SUB_RDS)
		chans += "RDS ";
	chans += "(" + QString::number((int)(m_tuner.signal / 655.35 + 0.5)) + "%";
	if (m_tuner.signal && m_tuner.afc)
		chans += m_tuner.afc < 0 ? " too low" : " too high";
	chans += ")";
	m_subchannels->setText(chans);
}

void GeneralTab::stereoModeChanged()
{
	v4l2_modulator mod;
	bool val = m_stereoMode->checkState() == Qt::Checked;

	g_modulator(mod);
	mod.txsubchans &= ~(V4L2_TUNER_SUB_MONO | V4L2_TUNER_SUB_STEREO);
	mod.txsubchans |= val ? V4L2_TUNER_SUB_STEREO : V4L2_TUNER_SUB_MONO;
	s_modulator(mod);
}

void GeneralTab::rdsModeChanged()
{
	v4l2_modulator mod;
	bool val = m_rdsMode->checkState() == Qt::Checked;

	g_modulator(mod);
	mod.txsubchans &= ~V4L2_TUNER_SUB_RDS;
	mod.txsubchans |= val ? V4L2_TUNER_SUB_RDS : 0;
	s_modulator(mod);
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
		if (set_interval(frmival.discrete))
			m_interval = frmival.discrete;
	}
	updateVidCapFormat();
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

void GeneralTab::vbiMethodsChanged(int idx)
{
	m_buftype = isSlicedVbi() ? V4L2_BUF_TYPE_SLICED_VBI_CAPTURE :
		(isVbi() ? V4L2_BUF_TYPE_VBI_CAPTURE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
}

void GeneralTab::updateVideoInput()
{
	int input;
	v4l2_input in;

	if (!g_input(input))
		return;
	enum_input(in, true, input);
	m_videoInput->setCurrentIndex(input);
	if (m_tvStandard) {
		refreshStandards();
		updateStandard();
		m_tvStandard->setEnabled(in.capabilities & V4L2_IN_CAP_STD);
		if (m_qryStandard)
			m_qryStandard->setEnabled(in.capabilities & V4L2_IN_CAP_STD);
	}
	if (m_videoTimings) {
		refreshTimings();
		updateTimings();
		m_videoTimings->setEnabled(in.capabilities & V4L2_IN_CAP_DV_TIMINGS);
		m_qryTimings->setEnabled(in.capabilities & V4L2_IN_CAP_DV_TIMINGS);
	}
}

void GeneralTab::updateVideoOutput()
{
	int output;
	v4l2_output out;

	if (!g_output(output))
		return;
	enum_output(out, true, output);
	m_videoOutput->setCurrentIndex(output);
	if (m_tvStandard) {
		m_tvStandard->setEnabled(out.capabilities & V4L2_OUT_CAP_STD);
		if (m_qryStandard)
			m_qryStandard->setEnabled(out.capabilities & V4L2_OUT_CAP_STD);
	}
	if (m_videoTimings) {
		m_videoTimings->setEnabled(out.capabilities & V4L2_OUT_CAP_DV_TIMINGS);
		m_qryTimings->setEnabled(out.capabilities & V4L2_OUT_CAP_DV_TIMINGS);
	}
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

void GeneralTab::refreshStandards()
{
	v4l2_standard vs;
	m_tvStandard->clear();
	if (enum_std(vs, true)) {
		do {
			m_tvStandard->addItem((char *)vs.name);
		} while (enum_std(vs));
	}
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
	updateVidCapFormat();
}

void GeneralTab::qryStdClicked()
{
	v4l2_std_id std;

	if (!query_std(std))
		return;

	if (std == V4L2_STD_ALL) {
		info("No standard detected\n");
	} else {
		s_std(std);
		updateStandard();
	}
}

void GeneralTab::refreshTimings()
{
	v4l2_enum_dv_timings timings;
	m_videoTimings->clear();
	if (enum_dv_timings(timings, true)) {
		do {
			v4l2_bt_timings &bt = timings.timings.bt;
			double tot_height = bt.height +
				bt.vfrontporch + bt.vsync + bt.vbackporch +
				bt.il_vfrontporch + bt.il_vsync + bt.il_vbackporch;
			double tot_width = bt.width +
				bt.hfrontporch + bt.hsync + bt.hbackporch;
			char buf[100];

			if (bt.interlaced)
				sprintf(buf, "%dx%di%.2f", bt.width, bt.height,
					(double)bt.pixelclock / (tot_width * (tot_height / 2)));
			else
				sprintf(buf, "%dx%dp%.2f", bt.width, bt.height,
					(double)bt.pixelclock / (tot_width * tot_height));
			m_videoTimings->addItem(buf);
		} while (enum_dv_timings(timings));
	}
}

void GeneralTab::updateTimings()
{
	v4l2_dv_timings timings;
	v4l2_enum_dv_timings p;
	QString what;

	g_dv_timings(timings);
	if (enum_dv_timings(p, true)) {
		do {
			if (!memcmp(&timings, &p.timings, sizeof(timings)))
				break;
		} while (enum_dv_timings(p));
	}
	if (memcmp(&timings, &p.timings, sizeof(timings)))
		return;
	m_videoTimings->setCurrentIndex(p.index);
	what.sprintf("Video Timings (%u)\n"
		"Frame %ux%u\n",
		p.index, p.timings.bt.width, p.timings.bt.height);
	m_videoTimings->setWhatsThis(what);
	updateVidCapFormat();
}

void GeneralTab::qryTimingsClicked()
{
	v4l2_dv_timings timings;

	if (query_dv_timings(timings)) {
		s_dv_timings(timings);
		updateTimings();
	}
}

void GeneralTab::updateFreq()
{
	v4l2_frequency f;

	g_frequency(f);
	/* m_freq listens to valueChanged block it to avoid recursion */
	m_freq->blockSignals(true);
	m_freq->setText(QString::number(f.frequency / 16.0));
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

	if (isVbi())
		return;
	g_fmt_cap(fmt);
	m_pixelformat = fmt.fmt.pix.pixelformat;
	m_width       = fmt.fmt.pix.width;
	m_height      = fmt.fmt.pix.height;
	updateFrameSize();
	updateFrameInterval();
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
		m_frameWidth->setMinimum(m_width);
		m_frameWidth->setMaximum(m_width);
		m_frameWidth->setValue(m_width);
		m_frameHeight->setMinimum(m_height);
		m_frameHeight->setMaximum(m_height);
		m_frameHeight->setValue(m_height);
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
	m_has_interval = ok && frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE;
	m_frameInterval->setEnabled(m_has_interval);
	if (m_has_interval) {
	        m_interval = frmival.discrete;
        	curr_ok = v4l2::get_interval(curr);
		do {
			m_frameInterval->addItem(QString("%1 fps")
				.arg((double)frmival.discrete.denominator / frmival.discrete.numerator));
			if (curr_ok &&
			    frmival.discrete.numerator == curr.numerator &&
			    frmival.discrete.denominator == curr.denominator) {
				m_frameInterval->setCurrentIndex(frmival.index);
				m_interval = frmival.discrete;
                        }
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

bool GeneralTab::get_interval(struct v4l2_fract &interval)
{
	if (m_has_interval)
		interval = m_interval;

	return m_has_interval;
}
