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

#include <sys/time.h>
#include <linux/videodev2.h>
#include <qgrid.h>

class QComboBox;
class QSpinBox;

class GeneralTab: public QGrid
{
    Q_OBJECT

public:
    GeneralTab(const char *device, int fd, int n, QWidget *parent = 0);
    virtual ~GeneralTab() {}

private slots:
    void inputChanged(int);
    void outputChanged(int);
    void inputAudioChanged(int);
    void outputAudioChanged(int);
    void standardChanged(int);
    void freqTableChanged(int);
    void freqChannelChanged(int);
    void freqChanged(int);

private:
    void updateVideoInput();
    void updateVideoOutput();
    void updateAudioInput();
    void updateAudioOutput();
    void updateStandard();
    void updateFreq();
    void updateFreqChannel();

    int fd;
    struct v4l2_tuner tuner;
    struct v4l2_capability querycap;

    // General tab
    QComboBox *videoInput;
    QComboBox *videoOutput;
    QComboBox *audioInput;
    QComboBox *audioOutput;
    QComboBox *tvStandard;
    QSpinBox  *freq;
    QComboBox *freqTable;
    QComboBox *freqChannel;
};

#endif
