bin_PROGRAMS = cx18-ctl ivtv-ctl v4l2-ctl

cx18_ctl_SOURCES = cx18-ctl.c

ivtv_ctl_SOURCES = ivtv-ctl.c
ivtv_ctl_LDFLAGS = -lm

v4l2_ctl_SOURCES = v4l2-ctl.cpp v4l2-ctl.h v4l2-ctl-common.cpp v4l2-ctl-tuner.cpp \
	v4l2-ctl-io.cpp v4l2-ctl-stds.cpp v4l2-ctl-vidcap.cpp v4l2-ctl-vidout.cpp \
	v4l2-ctl-overlay.cpp v4l2-ctl-vbi.cpp v4l2-ctl-selection.cpp v4l2-ctl-misc.cpp \
	v4l2-ctl-streaming.cpp v4l2-ctl-test-patterns.cpp
v4l2_ctl_LDADD = ../../lib/libv4l2/libv4l2.la ../../lib/libv4lconvert/libv4lconvert.la
