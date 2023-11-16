######################################################################
# qmake project file for qv4l2
######################################################################

TEMPLATE = app
INCLUDEPATH += . ../libv4l2util ../../lib/include ../../include
CONFIG += debug

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
greaterThan(QT_MAJOR_VERSION, 5): QT += core5compat
# opengl: to disable opengl suppport on Qt6 comment out the
# following line
greaterThan(QT_MAJOR_VERSION, 5): QT += openglwidgets

# adjust to your local meson build path
MESON_BUILD_PATH = $$PWD/build-meson

# opengl: to disable opengl suppport comment out the following
# line and the line '#define HAVE_QTGL 1' from ../../config.h
QT += opengl

QMAKE_CFLAGS += -include $$MESON_BUILD_PATH/config.h
QMAKE_CXXFLAGS += -include $$MESON_BUILD_PATH/config.h

INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/../common
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
HEADERS += ../common/v4l2-tpg.h
HEADERS += ../common/v4l2-tpg-colors.h
HEADERS += $$MESON_BUILD_PATH/config.h

SOURCES += alsa_stream.c
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

LIBS += -L$$MESON_BUILD_PATH/lib/libv4l2 -lv4l2
LIBS += -L$$MESON_BUILD_PATH/lib/libv4lconvert -lv4lconvert
LIBS += -L$$MESON_BUILD_PATH/utils/libv4l2util -lv4l2util
LIBS += -L$$MESON_BUILD_PATH/utils/libmedia_dev -lmedia_dev
LIBS += -lasound  # comment out in case alsa sound support is disabled/not available
LIBS += -lrt -ldl -ljpeg

RESOURCES += qv4l2.qrc
