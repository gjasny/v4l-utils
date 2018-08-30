######################################################################
# qmake project file for qvidcap
######################################################################

TEMPLATE = app
INCLUDEPATH += . ../libv4l2util ../../lib/include ../../include
CONFIG += debug

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# opengl: to disable opengl suppport comment out the following
# line and the line '#define HAVE_QTGL 1' from ../../config.h
QT += opengl

INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/../common
INCLUDEPATH += $$PWD/../v4l2-compliance

# Input
HEADERS += capture-win-gl.h
HEADERS += qvidcap.h
HEADERS += ../../config.h

SOURCES += capture-win-gl.cpp
SOURCES += qvidcap.cpp
SOURCES += ../common/v4l-stream.c
SOURCES += ../common/codec-fwht.c
SOURCES += ../common/v4l2-tpg-core.c
SOURCES += ../common/v4l2-tpg-colors.c

LIBS += -L$$PWD/../../lib/libv4l2/.libs -lv4l2
LIBS += -L$$PWD/../../lib/libv4lconvert/.libs -lv4lconvert
LIBS += -L$$PWD/../libv4l2util/.libs -lv4l2util
LIBS += -lrt -ldl -ljpeg

RESOURCES += qvidcap.qrc
