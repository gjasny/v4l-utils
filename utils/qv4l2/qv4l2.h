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

#ifndef QV4L2_H
#define QV4L2_H

#include <qmainwindow.h>
#include <qtabwidget.h>
#include <qsignalmapper.h>
#include <qgrid.h>
#include <map>
#include <vector>

#include <linux/videodev2.h>

class QComboBox;
class QSpinBox;

typedef std::vector<unsigned> ClassIDVec;
typedef std::map<unsigned, ClassIDVec> ClassMap;
typedef std::map<unsigned, struct v4l2_queryctrl> CtrlMap;
typedef std::map<unsigned, QWidget *> WidgetMap;

enum {
	CTRL_UPDATE_ON_CHANGE = 0x10,
	CTRL_DEFAULTS,
	CTRL_REFRESH,
	CTRL_UPDATE
};

class ApplicationWindow: public QMainWindow
{
    Q_OBJECT

public:
    ApplicationWindow();
    ~ApplicationWindow();

    void setDevice(const QString &device);
    bool doIoctl(QString descr, unsigned cmd, void *arg);

protected:
    void closeEvent( QCloseEvent* );

private slots:
    void selectdev(int);
    void choose();
    void ctrlAction(int);

    void about();

private:
    void add_dirVideoDevice(const char *dirname);
    void addTabs();
    void finishGrid(QWidget *vbox, QGrid *grid, unsigned ctrl_class, bool odd);
    void addCtrl(QGrid *grid, const struct v4l2_queryctrl &qctrl);
    void updateCtrl(unsigned id);
    void refresh(unsigned ctrl_class);
    void setDefaults(unsigned ctrl_class);
    int getVal(unsigned id);
    long long getVal64(unsigned id);
    void setVal(unsigned id, int v);
    void setVal64(unsigned id, long long v);
    QString getCtrlFlags(unsigned flags);
    void setWhat(QWidget *w, unsigned id, long long v);
    void updateVideoInput();
    void updateVideoOutput();
    void updateAudioInput();
    void updateAudioOutput();
    void updateStandard();
    void updateFreq();
    void updateFreqChannel();

    QString filename;
    QSignalMapper *sigMapper;
    QTabWidget *tabs;
    QPopupMenu *videoDevice;
    int fd;
    CtrlMap ctrlMap;
    WidgetMap widgetMap;
    ClassMap classMap;
};

extern ApplicationWindow *g_mw;

#endif
