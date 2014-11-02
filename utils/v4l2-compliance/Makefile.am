bin_PROGRAMS = v4l2-compliance
v4l2_compliance_SOURCES = v4l2-compliance.cpp v4l2-test-debug.cpp v4l2-test-input-output.cpp \
	v4l2-test-controls.cpp v4l2-test-io-config.cpp v4l2-test-formats.cpp v4l2-test-buffers.cpp \
	v4l2-test-codecs.cpp v4l2-compliance.h cv4l-helpers.h v4l-helpers.h
v4l2_compliance_LDADD = ../../lib/libv4l2/libv4l2.la ../../lib/libv4lconvert/libv4lconvert.la
v4l2_compliance_LDFLAGS = -lrt

EXTRA_DIST = Android.mk fixme.txt
