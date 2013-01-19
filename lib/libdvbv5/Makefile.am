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
  dvb-log.c ../include/dvb-log.h \
  dvb-file.c ../include/dvb-file.h \
  ../include/dvb-frontend.h \
  dvb-v5.h dvb-v5.c \
  ../include/dvb-v5-std.h dvb-v5-std.c \
  dvb-legacy-channel-format.c \
  dvb-zap-format.c \
  dvb-sat.c ../include/dvb-sat.h \
  dvb-scan.c ../include/dvb-scan.h \
  parse_string.c parse_string.h \
  crc32.c crc32.h \
  descriptors.c ../include/descriptors.h \
  descriptors/header.c ../include/descriptors/header.h \
  descriptors/pat.c  ../include/descriptors/pat.h \
  descriptors/pmt.c  ../include/descriptors/pmt.h \
  descriptors/desc_language.c  ../include/descriptors/desc_language.h \
  descriptors/desc_network_name.c  ../include/descriptors/desc_network_name.h \
  descriptors/desc_cable_delivery.c  ../include/descriptors/desc_cable_delivery.h \
  descriptors/desc_sat.c  ../include/descriptors/desc_sat.h \
  descriptors/desc_terrestrial_delivery.c  ../include/descriptors/desc_terrestrial_delivery.h \
  descriptors/desc_service.c  ../include/descriptors/desc_service.h \
  descriptors/desc_frequency_list.c  ../include/descriptors/desc_frequency_list.h \
  descriptors/desc_service_list.c  ../include/descriptors/desc_service_list.h \
  descriptors/desc_event_short.c  ../include/descriptors/desc_event_short.h \
  descriptors/desc_event_extended.c  ../include/descriptors/desc_event_extended.h \
  descriptors/desc_hierarchy.c  ../include/descriptors/desc_hierarchy.h \
  descriptors/nit.c  ../include/descriptors/nit.h \
  descriptors/sdt.c  ../include/descriptors/sdt.h \
  descriptors/eit.c  ../include/descriptors/eit.h

libdvbv5_la_CPPFLAGS = $(ENFORCE_LIBDVBV5_STATIC)
libdvbv5_la_LDFLAGS = -version-info 0 $(ENFORCE_LIBDVBV5_STATIC) -lm
libdvbv5_la_LIBADD = $(LTLIBICONV)

EXTRA_DIST = README gen_dvb_structs.pl

sync-with-kernel:
	./gen_dvb_structs.pl $(KERNEL_DIR)/include/
