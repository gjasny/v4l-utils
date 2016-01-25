if WITH_LIBV4L
lib_LTLIBRARIES = libv4l2.la
include_HEADERS = ../include/libv4l2.h ../include/libv4l-plugin.h
pkgconfig_DATA = libv4l2.pc
LIBV4L2_VERSION = -version-info 0
if WITH_V4L_WRAPPERS
libv4l2priv_LTLIBRARIES = v4l2convert.la

install-exec-hook:
	$(mkdir_p) $(DESTDIR)/$(libdir)
	(cd $(DESTDIR)/$(libdir) && rm -f v4l2convert.so && $(LN_S) $(libv4l2subdir)/v4l2convert.so v4l2convert.so)

endif
else
noinst_LTLIBRARIES = libv4l2.la
endif

libv4l2_la_SOURCES = libv4l2.c v4l2-plugin.c log.c libv4l2-priv.h
libv4l2_la_CPPFLAGS = $(CFLAG_VISIBILITY) $(ENFORCE_LIBV4L_STATIC)
libv4l2_la_LDFLAGS = $(LIBV4L2_VERSION) -lpthread $(DLOPEN_LIBS) $(ENFORCE_LIBV4L_STATIC)
libv4l2_la_LIBADD = ../libv4lconvert/libv4lconvert.la

v4l2convert_la_SOURCES = v4l2convert.c
v4l2convert_la_LIBADD = libv4l2.la
v4l2convert_la_LDFLAGS = -avoid-version -module -shared -export-dynamic
v4l2convert_la_LIBTOOLFLAGS = --tag=disable-static

EXTRA_DIST = Android.mk v4l2-plugin-android.c
