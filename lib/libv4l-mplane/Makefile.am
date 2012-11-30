if WITH_LIBV4L
libv4l2plugin_LTLIBRARIES = libv4l-mplane.la
else
noinst_LTLIBRARIES = libv4l-mplane.la
endif

libv4l_mplane_la_SOURCES = libv4l-mplane.c
libv4l_mplane_la_CPPFLAGS = -fvisibility=hidden $(ENFORCE_LIBV4L_STATIC) -std=c99
libv4l_mplane_la_LDFLAGS = -version-info 0 -lpthread $(ENFORCE_LIBV4L_STATIC)
