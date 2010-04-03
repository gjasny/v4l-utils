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

#include <QMainWindow>
#include <QTabWidget>
#include <QSignalMapper>
#include <QLabel>
#include <QGridLayout>
#include <QSocketNotifier>
#include <QImage>
#include <map>
#include <vector>

#include "v4l2-api.h"

class QComboBox;
class QSpinBox;
class GeneralTab;
class QCloseEvent;
class CaptureWin;

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

enum CapMethod {
	methodRead,
	methodMmap,
	methodUser
};

struct buffer {
	void   *start;
	size_t  length;
};


class ApplicationWindow: public QMainWindow, public v4l2
{
	Q_OBJECT

public:
	ApplicationWindow();
	virtual ~ApplicationWindow();

private slots:
	void closeDevice();
	void closeCaptureWin();

public:
	void setDevice(const QString &device, bool rawOpen);

	// capturing
private:
	CaptureWin *m_capture;

	bool startCapture(unsigned buffer_size);
	void stopCapture();
	void startOutput(unsigned buffer_size);
	void stopOutput();
	struct buffer *m_buffers;
	struct v4l2_format m_capSrcFormat;
	struct v4l2_format m_capDestFormat;
	unsigned char *m_frameData;
	unsigned m_nbuffers;
	struct v4lconvert_data *m_convertData;
	CapMethod m_capMethod;

private slots:
	void capStart(bool);
	void capFrame();

	// gui
private slots:
	void opendev();
	void openrawdev();
	void ctrlAction(int);

	void about();

public:
	virtual void error(const QString &text);
	void error(int err);
	void errorCtrl(unsigned id, int err);
	void errorCtrl(unsigned id, int err, long long v);
	void info(const QString &info);
	virtual void closeEvent(QCloseEvent *event);

private:
	void addWidget(QGridLayout *grid, QWidget *w, Qt::Alignment align = Qt::AlignLeft);
	void addLabel(QGridLayout *grid, const QString &text, Qt::Alignment align = Qt::AlignRight)
	{
		addWidget(grid, new QLabel(text, parentWidget()), align);
	}
	void addTabs();
	void finishGrid(QGridLayout *grid, unsigned ctrl_class);
	void addCtrl(QGridLayout *grid, const struct v4l2_queryctrl &qctrl);
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

	GeneralTab *m_genTab;
	QAction *m_capStartAct;
	QString m_filename;
	QSignalMapper *m_sigMapper;
	QTabWidget *m_tabs;
	QSocketNotifier *m_capNotifier;
	QImage *m_capImage;
	int m_row, m_col, m_cols;
	CtrlMap m_ctrlMap;
	WidgetMap m_widgetMap;
	ClassMap m_classMap;
};

extern ApplicationWindow *g_mw;

#endif
