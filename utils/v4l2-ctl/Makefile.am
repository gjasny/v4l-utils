bin_PROGRAMS = v4l2-ctl
man_MANS = v4l2-ctl.1
DEFS :=

v4l2_ctl_SOURCES = v4l2-ctl.cpp v4l2-ctl.h v4l2-ctl-common.cpp v4l2-ctl-tuner.cpp \
	v4l2-ctl-io.cpp v4l2-ctl-stds.cpp v4l2-ctl-vidcap.cpp v4l2-ctl-vidout.cpp \
	v4l2-ctl-overlay.cpp v4l2-ctl-vbi.cpp v4l2-ctl-selection.cpp v4l2-ctl-misc.cpp \
	v4l2-ctl-streaming.cpp v4l2-ctl-sdr.cpp v4l2-ctl-edid.cpp v4l2-ctl-modes.cpp \
	v4l2-tpg-colors.c v4l2-tpg-core.c v4l-stream.c
v4l2_ctl_CPPFLAGS = -I../common

if WITH_V4L2_CTL_LIBV4L
v4l2_ctl_LDADD = ../../lib/libv4l2/libv4l2.la ../../lib/libv4lconvert/libv4lconvert.la -lrt -lpthread
else
DEFS += -DNO_LIBV4L2
endif

EXTRA_DIST = Android.mk v4l2-ctl.1
