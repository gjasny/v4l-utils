bin_PROGRAMS = dvb-fe-tool dvbv5-zap dvbv5-scan dvb-format-convert
man_MANS = dvb-fe-tool.1 dvbv5-zap.1 dvbv5-scan.1 dvb-format-convert.1

dvb_fe_tool_SOURCES = dvb-fe-tool.c
dvb_fe_tool_LDADD = ../../lib/libdvbv5/libdvbv5.la @LIBINTL@
dvb_fe_tool_LDFLAGS = $(ARGP_LIBS) -lm

dvbv5_zap_SOURCES = dvbv5-zap.c
dvbv5_zap_LDADD = ../../lib/libdvbv5/libdvbv5.la @LIBINTL@
dvbv5_zap_LDFLAGS = $(ARGP_LIBS) -lm

dvbv5_scan_SOURCES = dvbv5-scan.c
dvbv5_scan_LDADD = ../../lib/libdvbv5/libdvbv5.la @LIBINTL@
dvbv5_scan_LDFLAGS = $(ARGP_LIBS) -lm

dvb_format_convert_SOURCES = dvb-format-convert.c
dvb_format_convert_LDADD = ../../lib/libdvbv5/libdvbv5.la @LIBINTL@
dvb_format_convert_LDFLAGS = $(ARGP_LIBS) -lm

EXTRA_DIST = README

