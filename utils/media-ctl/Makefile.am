lib_LTLIBRARIES = libmediactl.la libv4l2subdev.la
libmediactl_la_SOURCES = mediactl.c
libmediactl_la_CFLAGS = $(LIBUDEV_CFLAGS)
libmediactl_la_LDFLAGS = $(LIBUDEV_LIBS)
libv4l2subdev_la_SOURCES = v4l2subdev.c
libv4l2subdev_la_LIBADD = libmediactl.la
mediactl_includedir=$(includedir)/mediactl
mediactl_include_HEADERS = mediactl.h v4l2subdev.h

bin_PROGRAMS = media-ctl
media_ctl_SOURCES = main.c options.c options.h tools.h
media_ctl_LDADD = libmediactl.la libv4l2subdev.la
