if WITH_LIBV4L
lib_LTLIBRARIES = libv4l1.la
include_HEADERS = ../include/libv4l1.h ../include/libv4l1-videodev.h
pkgconfig_DATA = libv4l1.pc
if WITH_V4L_WRAPPERS
libv4l1priv_LTLIBRARIES = v4l1compat.la

install-exec-hook:
	$(MKDIR_P) $(DESTDIR)/$(libdir)
	(cd $(DESTDIR)/$(libdir) && rm -f v4l1compat.so && $(LN_S) $(libv4l1subdir)/v4l1compat.so v4l1compat.so)

endif
else
noinst_LTLIBRARIES = libv4l1.la
endif

libv4l1_la_SOURCES = libv4l1.c log.c libv4l1-priv.h
libv4l1_la_CPPFLAGS = $(CFLAG_VISIBILITY) $(ENFORCE_LIBV4L_STATIC)
libv4l1_la_LDFLAGS = -version-info 0 -lpthread $(ENFORCE_LIBV4L_STATIC)
libv4l1_la_LIBADD = ../libv4l2/libv4l2.la

v4l1compat_la_SOURCES = v4l1compat.c

v4l1compat_la_LIBADD = libv4l1.la
v4l1compat_la_LDFLAGS = -avoid-version -module -shared -export-dynamic
v4l1compat_la_LIBTOOLFLAGS = --tag=disable-static

EXTRA_DIST = libv4l1-kernelcode-license.txt
