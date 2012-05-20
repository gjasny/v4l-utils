if WITH_LIBDVBV5
lib_LTLIBRARIES = libdvbv5.la
include_HEADERS = ../include/dvb-demux.h ../include/dvb-v5-std.h \
  ../include/dvb-file.h ../include/dvb-frontend.h ../include/dvb-fe.h \
  ../include/dvb-sat.h ../include/dvb-scan.h
pkgconfig_DATA = libdvbv5.pc
else
noinst_LTLIBRARIES = libdvbv5.la
endif

libdvbv5_la_SOURCES = \
  dvb-demux.c ../include/dvb-demux.h \
  dvb-fe.c ../include/dvb-fe.h \
  dvb-file.c ../include/dvb-file.h \
  ../include/dvb-frontend.h \
  dvb-v5.h dvb-v5.c \
  ../include/dvb-v5-std.h dvb-v5-std.c \
  dvb-legacy-channel-format.c \
  dvb-zap-format.c \
  descriptors.c descriptors.h \
  dvb-sat.c ../include/dvb-sat.h \
  dvb-scan.c ../include/dvb-scan.h \
  parse_string.c parse_string.h
libdvbv5_la_CPPFLAGS = $(ENFORCE_LIBDVBV5_STATIC)
libdvbv5_la_LDFLAGS = -version-info 0 $(ENFORCE_LIBDVBV5_STATIC)

EXTRA_DIST = README gen_dvb_structs.pl

sync-with-kernel:
	./gen_dvb_structs.pl $(KERNEL_DIR)/include/
