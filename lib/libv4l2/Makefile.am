if WITH_LIBV4L
lib_LTLIBRARIES = libv4l2.la
include_HEADERS = ../include/libv4l2.h ../include/libv4l-plugin.h
libv4l2priv_LTLIBRARIES = v4l2convert.la
pkgconfig_DATA = libv4l2.pc
else
noinst_LTLIBRARIES = libv4l2.la
endif

libv4l2_la_SOURCES = libv4l2.c v4l2-plugin.c log.c libv4l2-priv.h
libv4l2_la_CPPFLAGS = -fvisibility=hidden $(ENFORCE_LIBV4L_STATIC)
libv4l2_la_LDFLAGS = -version-info 0 -lpthread $(DLOPEN_LIBS) $(ENFORCE_LIBV4L_STATIC)
libv4l2_la_LIBADD = ../libv4lconvert/libv4lconvert.la

v4l2convert_la_SOURCES = v4l2convert.c
v4l2convert_la_LIBADD = libv4l2.la
v4l2convert_la_LDFLAGS = -avoid-version -module -shared -export-dynamic
v4l2convert_la_LIBTOOLFLAGS = --tag=disable-static
