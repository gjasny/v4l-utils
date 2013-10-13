if WITH_V4L_PLUGINS
libv4l2plugin_LTLIBRARIES = libv4l-mplane.la
endif

libv4l_mplane_la_SOURCES = libv4l-mplane.c
libv4l_mplane_la_CPPFLAGS = $(CFLAG_VISIBILITY)
libv4l_mplane_la_LDFLAGS = -avoid-version -module -shared -export-dynamic -lpthread
