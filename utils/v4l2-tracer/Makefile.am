if HAVE_JSONC

lib_LTLIBRARIES = libv4l2tracer.la
libv4l2tracer_la_SOURCES = libv4l2tracer.cpp trace-helper.cpp trace.cpp trace.h \
v4l2-tracer-common.cpp v4l2-tracer-common.h $(top_srcdir)/utils/common/v4l2-info.cpp \
$(top_srcdir)/utils/common/media-info.cpp
nodist_libv4l2tracer_la_SOURCES = trace-gen.cpp
libv4l2tracer_la_CPPFLAGS = -I$(top_srcdir)/utils/common $(JSONC_CFLAGS)
libv4l2tracer_la_LDFLAGS = -avoid-version -module -shared -export-dynamic -ldl $(JSONC_LIBS)
libv4l2tracer_la_LIBTOOLFLAGS = --tag=disable-static

bin_PROGRAMS = v4l2-tracer
man_MANS = v4l2-tracer.1

v4l2_tracer_SOURCES = v4l2-tracer.cpp retrace-helper.cpp retrace.cpp retrace.h \
v4l2-tracer-common.cpp v4l2-tracer-common.h $(top_srcdir)/utils/common/v4l2-info.cpp \
$(top_srcdir)/utils/common/media-info.cpp
nodist_v4l2_tracer_SOURCES = retrace-gen.cpp
v4l2_tracer_CPPFLAGS = -I$(top_srcdir)/utils/common -DLIBTRACER_PATH=$(libdir) $(JSONC_CFLAGS) \
$(GIT_SHA) $(GIT_COMMIT_CNT) $(GIT_COMMIT_DATE)
v4l2_tracer_LDFLAGS = -lrt -lpthread $(JSONC_LIBS)

V4L2_TRACER_GEN_PREREQS = $(top_srcdir)/include/linux/v4l2-controls.h \
$(top_srcdir)/include/linux/videodev2.h $(top_srcdir)/include/linux/media.h \
$(top_srcdir)/include/linux/v4l2-common.h
V4L2_TRACER_GEN = trace-gen.cpp trace-gen.h retrace-gen.cpp retrace-gen.h v4l2-tracer-info-gen.h
v4l2-tracer.cpp: $(V4L2_TRACER_GEN)
$(V4L2_TRACER_GEN): gen.intermediate;
.INTERMEDIATE: gen.intermediate
gen.intermediate: v4l2-tracer-gen.pl $(V4L2_TRACER_GEN_PREREQS)
	$(AM_V_GEN) $(top_srcdir)/utils/v4l2-tracer/v4l2-tracer-gen.pl $(V4L2_TRACER_GEN_PREREQS)

clean-local:
	-rm -vf $(V4L2_TRACER_GEN)

EXTRA_DIST = v4l2-tracer.1.in v4l2-tracer-gen.pl

endif
