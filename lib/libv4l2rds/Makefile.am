if WITH_LIBV4L
lib_LTLIBRARIES = libv4l2rds.la
include_HEADERS = ../include/libv4l2rds.h
pkgconfig_DATA = libv4l2rds.pc
LIBV4L2RDS_VERSION = -version-info 0
else
noinst_LTLIBRARIES = libv4l2rds.la
endif

libv4l2rds_la_SOURCES = libv4l2rds.c
libv4l2rds_la_CPPFLAGS = $(CFLAG_VISIBILITY) $(ENFORCE_LIBV4L_STATIC)
libv4l2rds_la_LDFLAGS = $(LIBV4L2RDS_VERSION) -lpthread $(ENFORCE_LIBV4L_STATIC)
