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
class QSpinBox;

class GeneralTab: public QGridLayout, public v4l2
{
	Q_OBJECT

public:
	GeneralTab(const QString &device, v4l2 &fd, int n, QWidget *parent = 0);
	virtual ~GeneralTab() {}

	CapMethod capMethod();
	int width() const { return m_width; }
	int height() const { return m_height; }

private slots:
	void inputChanged(int);
	void outputChanged(int);
	void inputAudioChanged(int);
	void outputAudioChanged(int);
	void standardChanged(int);
	void presetChanged(int);
	void freqTableChanged(int);
	void freqChannelChanged(int);
	void freqChanged(int);
	void vidCapFormatChanged(int);
	void frameWidthChanged();
	void frameHeightChanged();
	void frameSizeChanged(int);
	void frameIntervalChanged(int);
	void vidOutFormatChanged(int);

private:
	void updateVideoInput();
	void updateVideoOutput();
	void updateAudioInput();
	void updateAudioOutput();
	void updateStandard();
	void updatePreset();
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
	struct v4l2_tuner m_tuner;
	struct v4l2_capability m_querycap;
	__u32 m_pixelformat;
	__u32 m_width, m_height;

	// General tab
	QComboBox *m_videoInput;
	QComboBox *m_videoOutput;
	QComboBox *m_audioInput;
	QComboBox *m_audioOutput;
	QComboBox *m_tvStandard;
	QComboBox *m_videoPreset;
	QSpinBox  *m_freq;
	QComboBox *m_freqTable;
	QComboBox *m_freqChannel;
	QComboBox *m_vidCapFormats;
	QComboBox *m_frameSize;
	QSpinBox *m_frameWidth;
	QSpinBox *m_frameHeight;
	QComboBox *m_frameInterval;
	QComboBox *m_vidOutFormats;
	QComboBox *m_capMethods;
	QGridLayout *m_layout;
};

#endif
