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
#include <QRegExp>

bool GeneralTab::m_fullAudioName = false;

enum audioDeviceAdd {
	AUDIO_ADD_NO,
	AUDIO_ADD_READ,
	AUDIO_ADD_WRITE,
	AUDIO_ADD_READWRITE
};

GeneralTab::GeneralTab(const QString &device, v4l2 &fd, int n, QWidget *parent) :
	QGridLayout(parent),
	v4l2(fd),
	m_row(0),
	m_col(0),
	m_cols(n),
	m_isRadio(false),
	m_isSDR(false),
	m_isVbi(false),
	m_freqFac(16),
	m_freqRfFac(16),
	m_isPlanar(false),
	m_videoInput(NULL),
	m_videoOutput(NULL),
	m_audioInput(NULL),
	m_audioOutput(NULL),
	m_tvStandard(NULL),
	m_qryStandard(NULL),
	m_videoTimings(NULL),
	m_pixelAspectRatio(NULL),
	m_crop(NULL),
	m_qryTimings(NULL),
	m_freq(NULL),
	m_freqTable(NULL),
	m_freqChannel(NULL),
	m_audioMode(NULL),
	m_subchannels(NULL),
	m_freqRf(NULL),
	m_stereoMode(NULL),
	m_rdsMode(NULL),
	m_detectSubchans(NULL),
	m_vidCapFormats(NULL),
	m_vidCapFields(NULL),
	m_frameSize(NULL),
	m_frameWidth(NULL),
	m_frameHeight(NULL),
	m_frameInterval(NULL),
	m_vidOutFormats(NULL),
	m_capMethods(NULL),
	m_vbiMethods(NULL),
	m_audioInDevice(NULL),
	m_audioOutDevice(NULL)
{
	m_device.append(device);
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
	g_tuner(m_tuner_rf, 1);
	g_modulator(m_modulator);

	v4l2_input vin;
	bool needsStd = false;
	bool needsTimings = false;

	if (m_tuner.capability &&
	    (m_tuner.capability & (V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_1HZ)))
		m_isRadio = true;
	if (m_modulator.capability &&
	    (m_modulator.capability & (V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_1HZ)))
		m_isRadio = true;
	if (m_querycap.capabilities & V4L2_CAP_DEVICE_CAPS) {
		m_isVbi = caps() & (V4L2_CAP_VBI_CAPTURE | V4L2_CAP_SLICED_VBI_CAPTURE);
		m_isSDR = caps() & V4L2_CAP_SDR_CAPTURE;
		if (m_isSDR)
			m_isRadio = true;
	}
	if (m_querycap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		m_isPlanar = true;
	m_buftype = (isPlanar() ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
		  V4L2_BUF_TYPE_VIDEO_CAPTURE);

	if (hasAlsaAudio()) {
		m_audioInDevice = new QComboBox(parent);
		m_audioOutDevice = new QComboBox(parent);
		m_audioInDevice->setSizeAdjustPolicy(QComboBox::AdjustToContents);
		m_audioOutDevice->setSizeAdjustPolicy(QComboBox::AdjustToContents);

		if (createAudioDeviceList()) {
			addLabel("Audio Input Device");
			connect(m_audioInDevice, SIGNAL(activated(int)), SLOT(changeAudioDevice()));
			addWidget(m_audioInDevice);

			addLabel("Audio Output Device");
			connect(m_audioOutDevice, SIGNAL(activated(int)), SLOT(changeAudioDevice()));
			addWidget(m_audioOutDevice);

			if (isRadio()) {
				setAudioDeviceBufferSize(75);
			} else {
				v4l2_fract fract;
				if (!v4l2::get_interval(m_buftype, fract)) {
					// Default values are for 30 FPS
					fract.numerator = 33;
					fract.denominator = 1000;
				}
				// Standard capacity is two frames
				setAudioDeviceBufferSize((fract.numerator * 2000) / fract.denominator);
			}
		} else {
			delete m_audioInDevice;
			delete m_audioOutDevice;
			m_audioInDevice = NULL;
			m_audioOutDevice = NULL;
		}
	}

	if (!isRadio() && !isVbi()) {
		m_pixelAspectRatio = new QComboBox(parent);
		m_pixelAspectRatio->addItem("Autodetect");
		m_pixelAspectRatio->addItem("Square");
		m_pixelAspectRatio->addItem("NTSC/PAL-M/PAL-60");
		m_pixelAspectRatio->addItem("NTSC/PAL-M/PAL-60, Anamorphic");
		m_pixelAspectRatio->addItem("PAL/SECAM");
		m_pixelAspectRatio->addItem("PAL/SECAM, Anamorphic");

		// Update hints by calling a get
		getPixelAspectRatio();

		addLabel("Pixel Aspect Ratio");
		addWidget(m_pixelAspectRatio);
		connect(m_pixelAspectRatio, SIGNAL(activated(int)), SLOT(changePixelAspectRatio()));

		m_crop = new QComboBox(parent);
		m_crop->addItem("None");
		m_crop->addItem("Top and Bottom Line");
		m_crop->addItem("Widescreen 14:9 (Letterbox)");
		m_crop->addItem("Widescreen 16:9 (Letterbox)");
		m_crop->addItem("Cinema 1.85:1 (Letterbox)");
		m_crop->addItem("Cinema 2.39:1 (Letterbox)");
		m_crop->addItem("Traditional 4:3 (Pillarbox)");

		addLabel("Cropping");
		addWidget(m_crop);
		connect(m_crop, SIGNAL(activated(int)), SIGNAL(cropChanged()));
	}

	if (!isRadio() && enum_input(vin, true)) {
		addLabel("Input");
		m_videoInput = new QComboBox(parent);
		do {
			m_videoInput->addItem((char *)vin.name);
			if (vin.capabilities & V4L2_IN_CAP_STD)
				needsStd = true;
			if (vin.capabilities & V4L2_IN_CAP_DV_TIMINGS)
				needsTimings = true;

			struct v4l2_event_subscription sub = {
				V4L2_EVENT_SOURCE_CHANGE, vin.index
			};

			subscribe_event(sub);
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
		m_audioInput->setMinimumContentsLength(10);
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
		m_audioOutput->setMinimumContentsLength(10);
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
		m_tvStandard->setMinimumContentsLength(10);
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
		m_videoTimings->setMinimumContentsLength(15);
		addWidget(m_videoTimings);
		connect(m_videoTimings, SIGNAL(activated(int)), SLOT(timingsChanged(int)));
		refreshTimings();
		addLabel("");
		m_qryTimings = new QPushButton("Query Timings", parent);
		addWidget(m_qryTimings);
		connect(m_qryTimings, SIGNAL(clicked()), SLOT(qryTimingsClicked()));
	}

	if (m_tuner.capability) {
		const char *unit = (m_tuner.capability & V4L2_TUNER_CAP_LOW) ? " kHz" :
			(m_tuner.capability & V4L2_TUNER_CAP_1HZ ? " Hz" : " MHz");

		m_freqFac = (m_tuner.capability & V4L2_TUNER_CAP_1HZ) ? 1 : 16;
		m_freq = new QDoubleSpinBox(parent);
		m_freq->setMinimum(m_tuner.rangelow / m_freqFac);
		m_freq->setMaximum(m_tuner.rangehigh / m_freqFac);
		m_freq->setSingleStep(1.0 / m_freqFac);
		m_freq->setSuffix(unit);
		m_freq->setDecimals((m_tuner.capability & V4L2_TUNER_CAP_1HZ) ? 0 : 4);
		m_freq->setWhatsThis(QString("Frequency\nLow: %1 %3\nHigh: %2 %3")
				     .arg((double)m_tuner.rangelow / m_freqFac, 0, 'f', 2)
				     .arg((double)m_tuner.rangehigh / m_freqFac, 0, 'f', 2)
				     .arg(unit));
		m_freq->setStatusTip(m_freq->whatsThis());
		connect(m_freq, SIGNAL(valueChanged(double)), SLOT(freqChanged(double)));
		updateFreq();
		addLabel("Frequency");
		addWidget(m_freq);
	}

	if (m_tuner.capability && !isRadio()) {
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

	if (m_tuner.capability && !isSDR()) {
		addLabel("Audio Mode");
		m_audioMode = new QComboBox(parent);
		m_audioMode->setMinimumContentsLength(12);
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

	if (m_tuner_rf.capability) {
		const char *unit = (m_tuner_rf.capability & V4L2_TUNER_CAP_LOW) ? " kHz" :
			(m_tuner_rf.capability & V4L2_TUNER_CAP_1HZ ? " Hz" : " MHz");

		m_freqRfFac = (m_tuner_rf.capability & V4L2_TUNER_CAP_1HZ) ? 1 : 16;
		m_freqRf = new QDoubleSpinBox(parent);
		m_freqRf->setMinimum(m_tuner_rf.rangelow / m_freqRfFac);
		m_freqRf->setMaximum(m_tuner_rf.rangehigh / m_freqRfFac);
		m_freqRf->setSingleStep(1.0 / m_freqRfFac);
		m_freqRf->setSuffix(unit);
		m_freqRf->setDecimals((m_tuner_rf.capability & V4L2_TUNER_CAP_1HZ) ? 0 : 4);
		m_freqRf->setWhatsThis(QString("RF Frequency\nLow: %1 %3\nHigh: %2 %3")
				     .arg((double)m_tuner_rf.rangelow / m_freqRfFac, 0, 'f', 2)
				     .arg((double)m_tuner_rf.rangehigh / m_freqRfFac, 0, 'f', 2)
				     .arg(unit));
		m_freqRf->setStatusTip(m_freqRf->whatsThis());
		connect(m_freqRf, SIGNAL(valueChanged(double)), SLOT(freqRfChanged(double)));
		updateFreqRf();
		addLabel("RF Frequency");
		addWidget(m_freqRf);
	}

	if (m_modulator.capability) {
		const char *unit = (m_modulator.capability & V4L2_TUNER_CAP_LOW) ? " kHz" :
			(m_modulator.capability & V4L2_TUNER_CAP_1HZ ? " Hz" : " MHz");

		m_freqFac = (m_modulator.capability & V4L2_TUNER_CAP_1HZ) ? 1 : 16;
		m_freq = new QDoubleSpinBox(parent);
		m_freq->setMinimum(m_modulator.rangelow / m_freqFac);
		m_freq->setMaximum(m_modulator.rangehigh / m_freqFac);
		m_freq->setSingleStep(1.0 / m_freqFac);
		m_freq->setSuffix(unit);
		m_freq->setDecimals((m_modulator.capability & V4L2_TUNER_CAP_1HZ) ? 0 : 4);
		m_freq->setWhatsThis(QString("Frequency\nLow: %1 %3\nHigh: %2 %3")
				     .arg((double)m_modulator.rangelow / m_freqFac, 0, 'f', 2)
				     .arg((double)m_modulator.rangehigh / m_freqFac, 0, 'f', 2)
				     .arg(unit));
		m_freq->setStatusTip(m_freq->whatsThis());
		connect(m_freq, SIGNAL(valueChanged(double)), SLOT(freqChanged(double)));
		updateFreq();
		addLabel("Frequency");
		addWidget(m_freq);
	}
	if (m_modulator.capability && !isSDR()) {
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
	m_vidCapFormats->setMinimumContentsLength(20);
	if (enum_fmt_cap(fmt, m_buftype, true)) {
		do {
			QString s(pixfmt2s(fmt.pixelformat) + " (");

			if (fmt.flags & V4L2_FMT_FLAG_EMULATED)
				m_vidCapFormats->addItem(s + "Emulated)");
			else
				m_vidCapFormats->addItem(s + (const char *)fmt.description + ")");
		} while (enum_fmt_cap(fmt, m_buftype));
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
	m_frameSize->setMinimumContentsLength(10);
	m_frameSize->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	addWidget(m_frameSize);
	connect(m_frameSize, SIGNAL(activated(int)), SLOT(frameSizeChanged(int)));

	addLabel("Frame Interval");
	m_frameInterval = new QComboBox(parent);
	m_frameInterval->setMinimumContentsLength(6);
	m_frameInterval->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	addWidget(m_frameInterval);
	connect(m_frameInterval, SIGNAL(activated(int)), SLOT(frameIntervalChanged(int)));

	addLabel("Field");
	m_vidCapFields = new QComboBox(parent);
	m_vidCapFields->setMinimumContentsLength(21);
	addWidget(m_vidCapFields);
	connect(m_vidCapFields, SIGNAL(activated(int)), SLOT(vidCapFieldChanged(int)));

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
		(isVbi() ? V4L2_BUF_TYPE_VBI_CAPTURE : 
		 (isPlanar() ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
		  V4L2_BUF_TYPE_VIDEO_CAPTURE));
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

void GeneralTab::setHaveBuffers(bool haveBuffers)
{
	if (m_videoInput)
		m_videoInput->setDisabled(haveBuffers);
	if (m_videoOutput)
		m_videoOutput->setDisabled(haveBuffers);
	if (m_tvStandard)
		m_tvStandard->setDisabled(haveBuffers);
	if (m_videoTimings)
		m_videoTimings->setDisabled(haveBuffers);
	if (m_vidCapFormats)
		m_vidCapFormats->setDisabled(haveBuffers);
	if (m_vidCapFields)
		m_vidCapFields->setDisabled(haveBuffers);
	if (m_frameSize)
		m_frameSize->setDisabled(haveBuffers);
	if (m_frameWidth)
		m_frameWidth->setDisabled(haveBuffers);
	if (m_frameHeight)
		m_frameHeight->setDisabled(haveBuffers);
	if (m_vidOutFormats)
		m_vidOutFormats->setDisabled(haveBuffers);
	if (m_capMethods)
		m_capMethods->setDisabled(haveBuffers);
	if (m_vbiMethods)
		m_vbiMethods->setDisabled(haveBuffers);
}

void GeneralTab::showAllAudioDevices(bool use)
{
	QString oldIn(m_audioInDevice->currentText());
	QString oldOut(m_audioOutDevice->currentText());

	m_fullAudioName = use;
	if (oldIn == NULL || oldOut == NULL || !createAudioDeviceList())
		return;

	// Select a similar device as before the listings method change
	// check by comparing old selection with any matching in the new list
	bool setIn = false, setOut = false;
	int listSize = std::max(m_audioInDevice->count(), m_audioOutDevice->count());

	for (int i = 0; i < listSize; i++) {
		QString oldInCmp(oldIn.left(std::min(m_audioInDevice->itemText(i).length(), oldIn.length())));
		QString oldOutCmp(oldOut.left(std::min(m_audioOutDevice->itemText(i).length(), oldOut.length())));

		if (!setIn && i < m_audioInDevice->count()
		    && m_audioInDevice->itemText(i).startsWith(oldInCmp)) {
			setIn = true;
			m_audioInDevice->setCurrentIndex(i);
		}

		if (!setOut && i < m_audioOutDevice->count()
		    && m_audioOutDevice->itemText(i).startsWith(oldOutCmp)) {
			setOut = true;
			m_audioOutDevice->setCurrentIndex(i);
		}
	}
}

bool GeneralTab::filterAudioInDevice(QString &deviceName)
{
	// Removes S/PDIF, front speakers and surround from input devices
	// as they are output devices, not input
	if (deviceName.contains("surround")
	    || deviceName.contains("front")
	    || deviceName.contains("iec958"))
		return false;

	// Removes sysdefault too if not full audio mode listings
	if (!m_fullAudioName && deviceName.contains("sysdefault"))
		return false;

	return true;
}

bool GeneralTab::filterAudioOutDevice(QString &deviceName)
{
	// Removes advanced options if not full audio mode listings
	if (!m_fullAudioName && (deviceName.contains("surround")
				 || deviceName.contains("front")
				 || deviceName.contains("iec958")
				 || deviceName.contains("sysdefault"))) {
		return false;
	}

	return true;
}

int GeneralTab::addAudioDevice(void *hint, int deviceNum)
{
	int added = 0;
#ifdef HAVE_ALSA
	char *name;
	char *iotype;
	QString deviceName;
	QString listName;
	QStringList deviceType;
	iotype = snd_device_name_get_hint(hint, "IOID");
	name = snd_device_name_get_hint(hint, "NAME");
	deviceName.append(name);

	snd_card_get_name(deviceNum, &name);
	listName.append(name);

	deviceType = deviceName.split(":");

	// Add device io capability to list name
	if (m_fullAudioName) {
		listName.append(" ");

		// Makes the surround name more readable
		if (deviceName.contains("surround"))
			listName.append(QString("surround %1.%2")
					.arg(deviceType.value(0)[8]).arg(deviceType.value(0)[9]));
		else
			listName.append(deviceType.value(0));

	} else if (!deviceType.value(0).contains("default")) {
		listName.append(" ").append(deviceType.value(0));
	}

	// Add device number if it is not 0
	if (deviceName.contains("DEV=")) {
		int devNo;
		QStringList deviceNo = deviceName.split("DEV=");
		devNo = deviceNo.value(1).toInt();
		if (devNo)
			listName.append(QString(" %1").arg(devNo));
	}

	if ((iotype == NULL || strncmp(iotype, "Input", 5) == 0) && filterAudioInDevice(deviceName)) {
		m_audioInDevice->addItem(listName);
		m_audioInDeviceMap[listName] = snd_device_name_get_hint(hint, "NAME");
		added += AUDIO_ADD_READ;
	}

	if ((iotype == NULL || strncmp(iotype, "Output", 6) == 0)  && filterAudioOutDevice(deviceName)) {
		m_audioOutDevice->addItem(listName);
		m_audioOutDeviceMap[listName] = snd_device_name_get_hint(hint, "NAME");
		added += AUDIO_ADD_WRITE;
	}
#endif
	return added;
}

bool GeneralTab::createAudioDeviceList()
{
#ifdef HAVE_ALSA
	if (m_audioInDevice == NULL || m_audioOutDevice == NULL)
		return false;

	m_audioInDevice->clear();
	m_audioOutDevice->clear();
	m_audioInDeviceMap.clear();
	m_audioOutDeviceMap.clear();

	m_audioInDevice->addItem("None");
	m_audioOutDevice->addItem("Default");
	m_audioInDeviceMap["None"] = "None";
	m_audioOutDeviceMap["Default"] = "default";

	int deviceNum = -1;
	int audioDevices = 0;
	int matchDevice = matchAudioDevice();
	int indexDevice = -1;
	int indexCount = 0;

	while (snd_card_next(&deviceNum) >= 0) {
		if (deviceNum == -1)
			break;

		audioDevices++;
		if (deviceNum == matchDevice && indexDevice == -1)
			indexDevice = indexCount;

		void **hint;

		snd_device_name_hint(deviceNum, "pcm", &hint);
		for (int i = 0; hint[i] != NULL; i++) {
			int addAs = addAudioDevice(hint[i], deviceNum);
			if (addAs == AUDIO_ADD_READ || addAs == AUDIO_ADD_READWRITE)
				indexCount++;
		}
		snd_device_name_free_hint(hint);
	}

	snd_config_update_free_global();
	m_audioInDevice->setCurrentIndex(indexDevice + 1);
	changeAudioDevice();
	return m_audioInDeviceMap.size() > 1 && m_audioOutDeviceMap.size() > 1 && audioDevices > 1;
#else
	return false;
#endif
}

void GeneralTab::changeAudioDevice()
{
	m_audioOutDevice->setEnabled(getAudioInDevice() != NULL ? getAudioInDevice().compare("None") : false);
	emit audioDeviceChanged();
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
	updateVidCapFormat();
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

	m_freq->setValue(f / 1000.0);
	freqChanged(m_freq->value());
}

void GeneralTab::freqChanged(double f)
{
	v4l2_frequency freq;

	if (!m_freq->isEnabled())
		return;

	g_frequency(freq);
	freq.frequency = f * m_freqFac;
	s_frequency(freq);
	updateFreq();
}

void GeneralTab::freqRfChanged(double f)
{
	v4l2_frequency freq;

	if (!m_freqRf->isEnabled())
		return;

	g_frequency(freq, 1);
	freq.frequency = f * m_freqRfFac;
	s_frequency(freq);
	updateFreqRf();
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

	enum_fmt_cap(desc, m_buftype, true, idx);

	v4l2_format fmt;

	g_fmt_cap(m_buftype, fmt);
	if (isPlanar())
		fmt.fmt.pix_mp.pixelformat = desc.pixelformat;
	else
		fmt.fmt.pix.pixelformat = desc.pixelformat;
	if (try_fmt(fmt))
		s_fmt(fmt);

	updateVidCapFormat();
}

static const char *field2s(int val)
{
	switch (val) {
	case V4L2_FIELD_ANY:
		return "Any";
	case V4L2_FIELD_NONE:
		return "None";
	case V4L2_FIELD_TOP:
		return "Top";
	case V4L2_FIELD_BOTTOM:
		return "Bottom";
	case V4L2_FIELD_INTERLACED:
		return "Interlaced";
	case V4L2_FIELD_SEQ_TB:
		return "Sequential Top-Bottom";
	case V4L2_FIELD_SEQ_BT:
		return "Sequential Bottom-Top";
	case V4L2_FIELD_ALTERNATE:
		return "Alternating";
	case V4L2_FIELD_INTERLACED_TB:
		return "Interlaced Top-Bottom";
	case V4L2_FIELD_INTERLACED_BT:
		return "Interlaced Bottom-Top";
	default:
		return "";
	}
}

void GeneralTab::vidCapFieldChanged(int idx)
{
	v4l2_format fmt;

	g_fmt_cap(m_buftype, fmt);
	for (__u32 f = V4L2_FIELD_NONE; f <= V4L2_FIELD_INTERLACED_BT; f++) {
		if (m_vidCapFields->currentText() == QString(field2s(f))) {
			if (isPlanar())
				fmt.fmt.pix_mp.field = f;
			else
				fmt.fmt.pix.field = f;
			s_fmt(fmt);
			break;
		}
	}
	updateVidCapFormat();
}

void GeneralTab::frameWidthChanged()
{
	v4l2_format fmt;
	int val = m_frameWidth->value();

	if (!m_frameWidth->isEnabled())
		return;
	g_fmt_cap(m_buftype, fmt);
	if (isPlanar())
		fmt.fmt.pix_mp.width = val;
	else
		fmt.fmt.pix.width = val;
	if (try_fmt(fmt))
		s_fmt(fmt);

	updateVidCapFormat();
}

void GeneralTab::frameHeightChanged()
{
	v4l2_format fmt;
	int val = m_frameHeight->value();

	if (!m_frameHeight->isEnabled())
		return;
	g_fmt_cap(m_buftype, fmt);
	if (isPlanar())
		fmt.fmt.pix_mp.height = val;
	else
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

		g_fmt_cap(m_buftype, fmt);
		if (isPlanar()) {
			fmt.fmt.pix_mp.width = frmsize.discrete.width;
			fmt.fmt.pix_mp.height = frmsize.discrete.height;
		} else {
			fmt.fmt.pix.width = frmsize.discrete.width;
			fmt.fmt.pix.height = frmsize.discrete.height;
		}
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
		if (set_interval(m_buftype, frmival.discrete))
			m_interval = frmival.discrete;
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

void GeneralTab::vbiMethodsChanged(int idx)
{
	m_buftype = isSlicedVbi() ? V4L2_BUF_TYPE_SLICED_VBI_CAPTURE :
		(isVbi() ? V4L2_BUF_TYPE_VBI_CAPTURE :
		 (isPlanar() ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
		  V4L2_BUF_TYPE_VIDEO_CAPTURE));
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
		bool enableFreq = in.type == V4L2_INPUT_TYPE_TUNER;
		if (m_freq)
			m_freq->setEnabled(enableFreq);
		if (m_freqTable)
			m_freqTable->setEnabled(enableFreq);
		if (m_freqChannel)
			m_freqChannel->setEnabled(enableFreq);
		if (m_detectSubchans) {
			m_detectSubchans->setEnabled(enableFreq);
			if (!enableFreq)
				m_subchannels->setText("");
			else
				detectSubchansClicked();
		}
	}
	if (m_videoTimings) {
		refreshTimings();
		updateTimings();
		m_videoTimings->setEnabled(in.capabilities & V4L2_IN_CAP_DV_TIMINGS);
		m_qryTimings->setEnabled(in.capabilities & V4L2_IN_CAP_DV_TIMINGS);
	}
	if (m_audioInput)
		m_audioInput->setEnabled(in.audioset);
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
	m_audioInput->setStatusTip(what);
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
	m_tvStandard->setStatusTip(what);
	m_tvStandard->setWhatsThis(what);
	updateVidCapFormat();
	if (!isVbi())
		changePixelAspectRatio();
}

void GeneralTab::qryStdClicked()
{
	v4l2_std_id std;

	if (!query_std(std))
		return;

	if (std == V4L2_STD_UNKNOWN) {
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
	m_videoTimings->setStatusTip(what);
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

void GeneralTab::sourceChange(const v4l2_event &ev)
{
	if (!m_videoInput)
		return;
	if ((int)ev.id != m_videoInput->currentIndex())
		return;
	if (m_qryStandard && m_qryStandard->isEnabled())
		m_qryStandard->click();
	else if (m_qryTimings && m_qryTimings->isEnabled())
		m_qryTimings->click();
}

void GeneralTab::updateFreq()
{
	v4l2_frequency f;

	g_frequency(f);
	/* m_freq listens to valueChanged block it to avoid recursion */
	m_freq->blockSignals(true);
	m_freq->setValue((double)f.frequency / m_freqFac);
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

void GeneralTab::updateFreqRf()
{
	v4l2_frequency f;

	g_frequency(f, 1);
	/* m_freqRf listens to valueChanged block it to avoid recursion */
	m_freqRf->blockSignals(true);
	m_freqRf->setValue((double)f.frequency / m_freqRfFac);
	m_freqRf->blockSignals(false);
}

void GeneralTab::updateVidCapFormat()
{
	v4l2_fmtdesc desc;
	v4l2_format fmt;

	if (isVbi())
		return;
	g_fmt_cap(m_buftype, fmt);
	if (isPlanar()) {
		m_pixelformat = fmt.fmt.pix_mp.pixelformat;
		m_width       = fmt.fmt.pix_mp.width;
		m_height      = fmt.fmt.pix_mp.height;
	} else {
		m_pixelformat = fmt.fmt.pix.pixelformat;
		m_width       = fmt.fmt.pix.width;
		m_height      = fmt.fmt.pix.height;
	}
	updateFrameSize();
	updateFrameInterval();
	if (enum_fmt_cap(desc, m_buftype, true)) {
		do {
			if (isPlanar()) {
				if (desc.pixelformat == fmt.fmt.pix_mp.pixelformat)
					break;
			} else {
				if (desc.pixelformat == fmt.fmt.pix.pixelformat)
					break;
			}
		} while (enum_fmt_cap(desc, m_buftype));
	}
	if (isPlanar()) {
		if (desc.pixelformat != fmt.fmt.pix_mp.pixelformat)
			return;
	} else {
		if (desc.pixelformat != fmt.fmt.pix.pixelformat)
			return;
	}
	m_vidCapFormats->setCurrentIndex(desc.index);
	updateVidCapFields();
}

void GeneralTab::updateVidCapFields()
{
	v4l2_format fmt;
	v4l2_format tmp;
	bool first = true;

	g_fmt_cap(m_buftype, fmt);

	for (__u32 f = V4L2_FIELD_NONE; f <= V4L2_FIELD_INTERLACED_BT; f++) {
		tmp = fmt;
		if (isPlanar()) {
			tmp.fmt.pix_mp.field = f;
			if (!try_fmt(tmp) || tmp.fmt.pix_mp.field != f)
				continue;
			if (first) {
				m_vidCapFields->clear();
				first = false;
			}
			m_vidCapFields->addItem(field2s(f));
			if (fmt.fmt.pix_mp.field == f)
				m_vidCapFields->setCurrentIndex(m_vidCapFields->count() - 1);
		} else {
			tmp.fmt.pix.field = f;
			if (!try_fmt(tmp) || tmp.fmt.pix.field != f)
				continue;
			if (first) {
				m_vidCapFields->clear();
				first = false;
			}
			m_vidCapFields->addItem(field2s(f));
			if (fmt.fmt.pix.field == f)
				m_vidCapFields->setCurrentIndex(m_vidCapFields->count() - 1);
		}
	}
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
		frmsize.stepwise.max_width = 4096;
		frmsize.stepwise.step_width = 1;
		frmsize.stepwise.min_height = 8;
		frmsize.stepwise.max_height = 2160;
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

CropMethod GeneralTab::getCropMethod()
{
	switch (m_crop->currentIndex()) {
	case 1:
		return QV4L2_CROP_TB;
	case 2:
		return QV4L2_CROP_W149;
	case 3:
		return QV4L2_CROP_W169;
	case 4:
		return QV4L2_CROP_C185;
	case 5:
		return QV4L2_CROP_C239;
	case 6:
		return QV4L2_CROP_P43;
	default:
		return QV4L2_CROP_NONE;
	}
}

void GeneralTab::changePixelAspectRatio()
{
	// Update hints by calling a get
	getPixelAspectRatio();
	info("");
	emit pixelAspectRatioChanged();
}

double GeneralTab::getPixelAspectRatio()
{
	v4l2_fract ratio = { 1, 1 };

	switch (m_pixelAspectRatio->currentIndex()) {
	case 0:
		ratio = g_pixel_aspect(m_buftype);
		break;
	case 2:
		ratio.numerator = 11;
		ratio.denominator = 10;
		break;
	case 3:
		ratio.numerator = 33;
		ratio.denominator = 40;
		break;
	case 4:
		ratio.numerator = 11;
		ratio.denominator = 12;
		break;
	case 5:
		ratio.numerator = 11;
		ratio.denominator = 16;
		break;
	default:
		break;
	}

	m_pixelAspectRatio->setWhatsThis(QString("Pixel Aspect Ratio %1:%2")
			 .arg(ratio.denominator).arg(ratio.numerator));
	m_pixelAspectRatio->setStatusTip(m_pixelAspectRatio->whatsThis());
	// Note: ratio is y / x, whereas we want x / y, so we return
	// denominator / numerator.
	return (double)ratio.denominator / ratio.numerator;
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
        	curr_ok = v4l2::get_interval(m_buftype, curr);
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

QString GeneralTab::getAudioInDevice()
{
	if (m_audioInDevice == NULL)
		return NULL;

	return m_audioInDeviceMap[m_audioInDevice->currentText()];
}

QString GeneralTab::getAudioOutDevice()
{
	if (m_audioOutDevice == NULL)
		return NULL;

	return m_audioOutDeviceMap[m_audioOutDevice->currentText()];
}

void GeneralTab::setAudioDeviceBufferSize(int size)
{
	m_audioDeviceBufferSize = size;
}

int GeneralTab::getAudioDeviceBufferSize()
{
	return m_audioDeviceBufferSize;
}

#ifdef HAVE_ALSA
int GeneralTab::checkMatchAudioDevice(void *md, const char *vid, const enum device_type type)
{
	const char *devname = NULL;

	while ((devname = get_associated_device(md, devname, type, vid, MEDIA_V4L_VIDEO)) != NULL) {
		if (type == MEDIA_SND_CAP) {
			QStringList devAddr = QString(devname).split(QRegExp("[:,]"));
			return devAddr.value(1).toInt();
		}
	}
	return -1;
}

int GeneralTab::matchAudioDevice()
{
	QStringList devPath = m_device.split("/");
	QString curDev = devPath.value(devPath.count() - 1);

	void *media;
	const char *video = NULL;
	int match;

	media = discover_media_devices();

	while ((video = get_associated_device(media, video, MEDIA_V4L_VIDEO, NULL, NONE)) != NULL)
		if (curDev.compare(video) == 0)
			for (int i = 0; i <= MEDIA_SND_HW; i++)
				if ((match = checkMatchAudioDevice(media, video, static_cast<device_type>(i))) != -1)
					return match;

	return -1;
}
#endif

bool GeneralTab::hasAlsaAudio()
{
#ifdef HAVE_ALSA
	return !isVbi();
#else
	return false;
#endif
}
