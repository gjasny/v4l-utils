bin_PROGRAMS = dvb-fe-tool dvbv5-zap dvbv5-scan dvb-format-convert
noinst_LTLIBRARIES = libdvbv5.la

libdvbv5_la_SOURCES = \
  dvb-demux.c dvb-demux.h \
  dvb-fe.c dvb-fe.h \
  dvb-file.c dvb-file.h \
  dvb_frontend.h  dvb-v5.h  dvb-v5-std.h \
  dvb-legacy-channel-format.c \
  dvb-zap-format.c \
  descriptors.c descriptors.h \
  libsat.c libsat.h \
  libscan.c libscan.h \
  parse_string.c parse_string.h
libdvbv5_la_CPPFLAGS = -static
libdvbv5_la_LDFLAGS = -static

dvb_fe_tool_SOURCES = dvb-fe-tool.c
dvb_fe_tool_LDADD = libdvbv5.la
dvb_fe_tool_LDFLAGS = $(ARGP_LIBS)

dvbv5_zap_SOURCES = dvbv5-zap.c
dvbv5_zap_LDADD = libdvbv5.la
dvbv5_zap_LDFLAGS = $(ARGP_LIBS)

dvbv5_scan_SOURCES = dvbv5-scan.c
dvbv5_scan_LDADD = libdvbv5.la
dvbv5_scan_LDFLAGS = $(ARGP_LIBS)

dvb_format_convert_SOURCES = dvb-format-convert.c
dvb_format_convert_LDADD = libdvbv5.la
dvb_format_convert_LDFLAGS = $(ARGP_LIBS)

EXTRA_DIST = README gen_dvb_structs.pl

sync-with-kernel:
	./gen_dvb_structs.pl $(KERNEL_DIR)/include/
