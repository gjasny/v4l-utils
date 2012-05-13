lib_LTLIBRARIES = libdvbv5.la

libdvbv5_la_SOURCES = \
  dvb-demux.c dvb-demux.h \
  dvb-fe.c dvb-fe.h \
  dvb-file.c dvb-file.h \
  dvb-frontend.h  dvb-v5.h  dvb-v5-std.h \
  dvb-legacy-channel-format.c \
  dvb-zap-format.c \
  descriptors.c descriptors.h \
  libsat.c libsat.h \
  libscan.c libscan.h \
  parse_string.c parse_string.h
#libdvbv5_la_CPPFLAGS = -fvisibility=hidden
#libdvbv5_la_LDFLAGS = -version-info 0 -lpthread

EXTRA_DIST = README gen_dvb_structs.pl

sync-with-kernel:
	./gen_dvb_structs.pl $(KERNEL_DIR)/include/
