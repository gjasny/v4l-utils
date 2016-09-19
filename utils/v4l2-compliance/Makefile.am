bin_PROGRAMS = v4l2-compliance
man_MANS = v4l2-compliance.1
DEFS :=

v4l2_compliance_SOURCES = v4l2-compliance.cpp v4l2-test-debug.cpp v4l2-test-input-output.cpp \
	v4l2-test-controls.cpp v4l2-test-io-config.cpp v4l2-test-formats.cpp v4l2-test-buffers.cpp \
	v4l2-test-codecs.cpp v4l2-test-colors.cpp v4l2-compliance.h
v4l2_compliance_CPPFLAGS = -I../common

if WITH_V4L2_COMPLIANCE_LIBV4L
v4l2_compliance_LDADD = ../../lib/libv4l2/libv4l2.la ../../lib/libv4lconvert/libv4lconvert.la -lrt -lpthread
else
v4l2_compliance_LDADD = -lrt -lpthread
DEFS += -DNO_LIBV4L2
endif

EXTRA_DIST = Android.mk fixme.txt v4l2-compliance.1

v4l2-compliance.cpp: version.h

version.h:
	@if git show-ref -s HEAD >/dev/null 2>&1; then \
		echo -n "#define SHA " >$@ ; \
		git show-ref -s HEAD >>$@ ; \
	else \
		touch $@ ; \
	fi

clean-local:
	-rm -vf version.h

