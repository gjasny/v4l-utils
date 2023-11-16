######################################################################
# qmake project file for qvidcap
######################################################################

TEMPLATE = app
INCLUDEPATH += . ../libv4l2util ../../lib/include ../../include
CONFIG += debug

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
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
INCLUDEPATH += $$PWD/../v4l2-compliance

v4l2_convert_hook.depends = $$PWD/v4l2-convert.pl $$PWD/v4l2-convert.glsl
v4l2_convert_hook.commands = perl $$PWD/v4l2-convert.pl < $$PWD/v4l2-convert.glsl > v4l2-convert.h
QMAKE_EXTRA_TARGETS += v4l2_convert_hook
PRE_TARGETDEPS += v4l2_convert_hook

# Input
HEADERS += capture.h
HEADERS += qvidcap.h
HEADERS += $$MESON_BUILD_PATH/config.h

SOURCES += capture.cpp paint.cpp
SOURCES += qvidcap.cpp
SOURCES += ../common/v4l-stream.c
SOURCES += ../common/codec-fwht.c
SOURCES += ../common/codec-v4l2-fwht.c
SOURCES += ../common/v4l2-info.cpp
SOURCES += ../common/v4l2-tpg-core.c
SOURCES += ../common/v4l2-tpg-colors.c

LIBS += -L$$MESON_BUILD_PATH/lib/libv4l2 -lv4l2
LIBS += -L$$MESON_BUILD_PATH/lib/libv4lconvert -lv4lconvert
LIBS += -L$$MESON_BUILD_PATH/utils/libv4l2util -lv4l2util
LIBS += -lrt -ldl -ljpeg

RESOURCES += qvidcap.qrc
