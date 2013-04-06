if WITH_LIBV4L
lib_LTLIBRARIES = libv4l2rds.la
include_HEADERS = ../include/libv4l2rds.h
pkgconfig_DATA = libv4l2rds.pc
else
noinst_LTLIBRARIES = libv4l2rds.la
endif

libv4l2rds_la_SOURCES = libv4l2rds.c
libv4l2rds_la_CPPFLAGS = -fvisibility=hidden $(ENFORCE_LIBV4L_STATIC) -std=gnu99
libv4l2rds_la_LDFLAGS = -version-info 0 -lpthread $(ENFORCE_LIBV4L_STATIC)
