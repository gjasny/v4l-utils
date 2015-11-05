noinst_LTLIBRARIES = libmediactl.la libv4l2subdev.la

libmediactl_la_SOURCES = libmediactl.c mediactl-priv.h
libmediactl_la_CFLAGS = -static $(LIBUDEV_CFLAGS)
libmediactl_la_LDFLAGS = -static $(LIBUDEV_LIBS)

media-bus-format-names.h: ../../include/linux/media-bus-format.h
	sed -e '/#define MEDIA_BUS_FMT/ ! d; s/.*FMT_//; /FIXED/ d; s/\t.*//; s/.*/{ \"&\", MEDIA_BUS_FMT_& },/;' \
	< $< > $@

media-bus-format-codes.h: ../../include/linux/media-bus-format.h
	sed -e '/#define MEDIA_BUS_FMT/ ! d; s/.*#define //; /FIXED/ d; s/\t.*//; s/.*/ &,/;' \
	< $< > $@

BUILT_SOURCES = media-bus-format-names.h media-bus-format-codes.h
CLEANFILES = $(BUILT_SOURCES)

nodist_libv4l2subdev_la_SOURCES = $(BUILT_SOURCES)
libv4l2subdev_la_SOURCES = libv4l2subdev.c
libv4l2subdev_la_LIBADD = libmediactl.la
libv4l2subdev_la_CFLAGS = -static
libv4l2subdev_la_LDFLAGS = -static

mediactl_includedir=$(includedir)/mediactl
noinst_HEADERS = mediactl.h v4l2subdev.h

bin_PROGRAMS = media-ctl
media_ctl_SOURCES = media-ctl.c options.c options.h tools.h
media_ctl_LDADD = libmediactl.la libv4l2subdev.la
