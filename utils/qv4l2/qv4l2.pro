######################################################################
# qmake project file for qv4l2
######################################################################

TEMPLATE = app
INCLUDEPATH += . ../libv4l2util ../../lib/include ../../include
CONFIG += debug

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# opengl: to disable opengl suppport comment out the following
# line and the line '#define HAVE_QTGL 1' from ../../config.h
QT += opengl

INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/../v4l2-ctl/
INCLUDEPATH += $$PWD/../v4l2-compliance

# Input
HEADERS += alsa_stream.h
HEADERS += capture-win-gl.h
HEADERS += capture-win.h
HEADERS += capture-win-qt.h
HEADERS += general-tab.h
HEADERS += qv4l2.h
HEADERS += raw2sliced.h
HEADERS += vbi-tab.h
HEADERS += ../v4l2-ctl/v4l2-tpg.h
HEADERS += ../v4l2-ctl/v4l2-tpg-colors.h
HEADERS += ../../config.h

SOURCES += capture-win.cpp
SOURCES += capture-win-gl.cpp
SOURCES += capture-win-qt.cpp
SOURCES += ctrl-tab.cpp
SOURCES += general-tab.cpp
SOURCES += qv4l2.cpp
SOURCES += raw2sliced.cpp
SOURCES += tpg-tab.cpp
SOURCES += vbi-tab.cpp
SOURCES += ../v4l2-ctl/v4l2-tpg-core.c
SOURCES += ../v4l2-ctl/v4l2-tpg-colors.c

LIBS += -L$$PWD/../../lib/libv4l2/.libs -lv4l2
LIBS += -L$$PWD/../../lib/libv4lconvert/.libs -lv4lconvert
LIBS += -L$$PWD/../libv4l2util/.libs -lv4l2util 
LIBS += -lrt -ldl -ljpeg

RESOURCES += qv4l2.qrc
