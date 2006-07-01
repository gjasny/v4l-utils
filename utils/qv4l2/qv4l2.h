/****************************************************************************
** $Id: qt/application.h   3.3.6   edited Aug 31 2005 $
**
** Copyright (C) 1992-2005 Trolltech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#ifndef APPLICATION_H
#define APPLICATION_H

#include <qmainwindow.h>
#include <qtabwidget.h>
#include <qsignalmapper.h>
#include <qgrid.h>
#include <map>
#include <vector>

#define __user
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

protected:
    void closeEvent( QCloseEvent* );

private slots:
    void choose();
    void ctrlAction(int);
    void inputChanged(int);
    void outputChanged(int);
    void inputAudioChanged(int);
    void outputAudioChanged(int);
    void standardChanged(int);
    void freqTableChanged(int);
    void freqChannelChanged(int);
    void freqChanged(int);

    void about();

private:
    void addTabs();
    void addGeneralTab();
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
    bool doIoctl(QString descr, unsigned cmd, void *arg);
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
    int fd;
    CtrlMap ctrlMap;
    WidgetMap widgetMap;
    ClassMap classMap;
    struct v4l2_tuner tuner;

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
