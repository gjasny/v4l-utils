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


#ifndef GENERAL_TAB_H
#define GENERAL_TAB_H

#include <QSpinBox>
#include <sys/time.h>
#include <linux/videodev2.h>
#include "qv4l2.h"
#include "v4l2-api.h"

class QComboBox;
class QCheckBox;
class QSpinBox;
class QPushButton;

class GeneralTab: public QGridLayout, public v4l2
{
	Q_OBJECT

public:
	GeneralTab(const QString &device, v4l2 &fd, int n, QWidget *parent = 0);
	virtual ~GeneralTab() {}

	CapMethod capMethod();
	bool get_interval(struct v4l2_fract &interval);
	int width() const { return m_width; }
	int height() const { return m_height; }
	bool isRadio() const { return m_isRadio; }
	bool isVbi() const { return m_isVbi; }
	bool isSlicedVbi() const;
	__u32 bufType() const { return m_buftype; }
	inline bool reqbufs_mmap(v4l2_requestbuffers &reqbuf, int count = 0) {
		return v4l2::reqbufs_mmap(reqbuf, m_buftype, count);
	}
	inline bool reqbufs_user(v4l2_requestbuffers &reqbuf, int count = 0) {
		return v4l2::reqbufs_user(reqbuf, m_buftype, count);
	}
	inline bool dqbuf_mmap(v4l2_buffer &buf, bool &again) {
		return v4l2::dqbuf_mmap(buf, m_buftype, again);
	}
	inline bool dqbuf_user(v4l2_buffer &buf, bool &again) {
		return v4l2::dqbuf_user(buf, m_buftype, again);
	}
	inline bool qbuf_mmap(int index) {
		return v4l2::qbuf_mmap(index, m_buftype);
	}
	inline bool qbuf_user(int index, void *ptr, int length) {
		return v4l2::qbuf_user(index, m_buftype, ptr, length);
	}
	inline bool streamon() { return v4l2::streamon(m_buftype); }
	inline bool streamoff() { return v4l2::streamoff(m_buftype); }

private slots:
	void inputChanged(int);
	void outputChanged(int);
	void inputAudioChanged(int);
	void outputAudioChanged(int);
	void standardChanged(int);
	void qryStdClicked();
	void timingsChanged(int);
	void qryTimingsClicked();
	void freqTableChanged(int);
	void freqChannelChanged(int);
	void freqChanged();
	void audioModeChanged(int);
	void detectSubchansClicked();
	void stereoModeChanged();
	void rdsModeChanged();
	void vidCapFormatChanged(int);
	void frameWidthChanged();
	void frameHeightChanged();
	void frameSizeChanged(int);
	void frameIntervalChanged(int);
	void vidOutFormatChanged(int);
	void vbiMethodsChanged(int);

private:
	void updateVideoInput();
	void updateVideoOutput();
	void updateAudioInput();
	void updateAudioOutput();
	void refreshStandards();
	void updateStandard();
	void refreshTimings();
	void updateTimings();
	void updateFreq();
	void updateFreqChannel();
	void updateVidCapFormat();
	void updateFrameSize();
	void updateFrameInterval();
	void updateVidOutFormat();

	void addWidget(QWidget *w, Qt::Alignment align = Qt::AlignLeft);
	void addLabel(const QString &text, Qt::Alignment align = Qt::AlignRight)
	{
		addWidget(new QLabel(text, parentWidget()), align);
	}
	void info(const QString &info)
	{
		g_mw->info(info);
	}
	virtual void error(const QString &error)
	{
		g_mw->error(error);
	}

	int m_row;
	int m_col;
	int m_cols;
	bool m_isRadio;
	bool m_isVbi;
	__u32 m_buftype;
	__u32 m_audioModes[5];
	struct v4l2_tuner m_tuner;
	struct v4l2_modulator m_modulator;
	struct v4l2_capability m_querycap;
	__u32 m_pixelformat;
	__u32 m_width, m_height;
	struct v4l2_fract m_interval;
	bool m_has_interval;

	// General tab
	QComboBox *m_videoInput;
	QComboBox *m_videoOutput;
	QComboBox *m_audioInput;
	QComboBox *m_audioOutput;
	QComboBox *m_tvStandard;
	QPushButton *m_qryStandard;
	QComboBox *m_videoTimings;
	QPushButton *m_qryTimings;
	QLineEdit *m_freq;
	QComboBox *m_freqTable;
	QComboBox *m_freqChannel;
	QComboBox *m_audioMode;
	QLabel *m_subchannels;
	QCheckBox *m_stereoMode;
	QCheckBox *m_rdsMode;
	QPushButton *m_detectSubchans;
	QComboBox *m_vidCapFormats;
	QComboBox *m_frameSize;
	QSpinBox *m_frameWidth;
	QSpinBox *m_frameHeight;
	QComboBox *m_frameInterval;
	QComboBox *m_vidOutFormats;
	QComboBox *m_capMethods;
	QComboBox *m_vbiMethods;
};

#endif
