noinst_LTLIBRARIES = libmediactl.la libv4l2subdev.la

libmediactl_la_SOURCES = libmediactl.c mediactl-priv.h
libmediactl_la_CFLAGS = -static $(LIBUDEV_CFLAGS)
libmediactl_la_LDFLAGS = -static $(LIBUDEV_LIBS)

libv4l2subdev_la_SOURCES = libv4l2subdev.c
libv4l2subdev_la_LIBADD = libmediactl.la
libv4l2subdev_la_CFLAGS = -static
libv4l2subdev_la_LDFLAGS = -static

mediactl_includedir=$(includedir)/mediactl
noinst_HEADERS = mediactl.h v4l2subdev.h

bin_PROGRAMS = media-ctl
media_ctl_SOURCES = media-ctl.c options.c options.h tools.h
media_ctl_LDADD = libmediactl.la libv4l2subdev.la
