bin_PROGRAMS = dvb-fe-tool dvbv5-zap dvbv5-scan dvb-format-convert

if WITH_DVBV5_REMOTE
bin_PROGRAMS += \
	dvbv5-daemon
endif

man_MANS = dvb-fe-tool.1 dvbv5-zap.1 dvbv5-scan.1 dvb-format-convert.1

dvb_fe_tool_SOURCES = dvb-fe-tool.c
dvb_fe_tool_LDADD = ../../lib/libdvbv5/libdvbv5.la @LIBINTL@ $(LIBUDEV_LIBS) $(XMLRPC_LDADD)
dvb_fe_tool_LDFLAGS = $(ARGP_LIBS) -lm $(LIBUDEV_CFLAGS) $(XMLRPC_LDFLAGS)

dvbv5_zap_SOURCES = dvbv5-zap.c
dvbv5_zap_LDADD = ../../lib/libdvbv5/libdvbv5.la @LIBINTL@ $(LIBUDEV_LIBS) $(XMLRPC_LDADD)
dvbv5_zap_LDFLAGS = $(ARGP_LIBS) -lm $(LIBUDEV_CFLAGS) $(XMLRPC_LDFLAGS)

dvbv5_scan_SOURCES = dvbv5-scan.c
dvbv5_scan_LDADD = ../../lib/libdvbv5/libdvbv5.la @LIBINTL@ $(LIBUDEV_LIBS) $(XMLRPC_LDADD)
dvbv5_scan_LDFLAGS = $(ARGP_LIBS) -lm $(LIBUDEV_CFLAGS) $(XMLRPC_LDFLAGS)

dvb_format_convert_SOURCES = dvb-format-convert.c
dvb_format_convert_LDADD = ../../lib/libdvbv5/libdvbv5.la @LIBINTL@ $(LIBUDEV_LIBS) $(XMLRPC_LDADD)
dvb_format_convert_LDFLAGS = $(ARGP_LIBS) -lm $(LIBUDEV_CFLAGS) $(XMLRPC_LDFLAGS)

dvbv5_daemon_SOURCES = dvbv5-daemon.c
dvbv5_daemon_LDADD = ../../lib/libdvbv5/libdvbv5.la @LIBINTL@ $(LIBUDEV_LIBS) $(XMLRPC_LDADD) $(PTHREAD_LDADD)
dvbv5_daemon_LDFLAGS = $(ARGP_LIBS) -lm $(XMLRPC_LDFLAGS) $(PTHREAD_LDFLAGS)
dvbv5_daemon_CFLAGS =  $(XMLRPC_CFLAGS) $(LIBUDEV_CFLAGS) $(PTHREAD_CFLAGS)

EXTRA_DIST = README
