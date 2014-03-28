if WITH_LIBDVBV5
lib_LTLIBRARIES = libdvbv5.la

otherincludedir = $(includedir)/libdvbv5
otherinclude_HEADERS = \
	../include/libdvbv5/dvb-demux.h \
	../include/libdvbv5/dvb-v5-std.h \
	../include/libdvbv5/dvb-file.h \
	../include/libdvbv5/dvb-frontend.h \
	../include/libdvbv5/dvb-fe.h \
	../include/libdvbv5/dvb-sat.h \
	../include/libdvbv5/dvb-scan.h \
	../include/libdvbv5/dvb-log.h \
	../include/libdvbv5/descriptors.h \
	../include/libdvbv5/header.h \
	../include/libdvbv5/pat.h \
	../include/libdvbv5/pmt.h \
	../include/libdvbv5/desc_language.h \
	../include/libdvbv5/desc_network_name.h \
	../include/libdvbv5/desc_cable_delivery.h \
	../include/libdvbv5/desc_sat.h \
	../include/libdvbv5/desc_terrestrial_delivery.h \
	../include/libdvbv5/desc_t2_delivery.h \
	../include/libdvbv5/desc_service.h \
	../include/libdvbv5/desc_frequency_list.h \
	../include/libdvbv5/desc_service_list.h \
	../include/libdvbv5/desc_event_short.h \
	../include/libdvbv5/desc_event_extended.h \
	../include/libdvbv5/desc_atsc_service_location.h \
	../include/libdvbv5/desc_hierarchy.h \
	../include/libdvbv5/desc_extension.h \
	../include/libdvbv5/desc_isdbt_delivery.h \
	../include/libdvbv5/desc_logical_channel.h \
	../include/libdvbv5/desc_ts_info.h \
	../include/libdvbv5/desc_partial_reception.h \
	../include/libdvbv5/nit.h \
	../include/libdvbv5/sdt.h \
	../include/libdvbv5/vct.h \
	../include/libdvbv5/atsc_header.h \
	../include/libdvbv5/mgt.h \
	../include/libdvbv5/eit.h \
	../include/libdvbv5/atsc_eit.h \
	../include/libdvbv5/desc_service_location.h \
	../include/libdvbv5/mpeg_ts.h \
	../include/libdvbv5/mpeg_pes.h \
	../include/libdvbv5/mpeg_es.h

pkgconfig_DATA = libdvbv5.pc
LIBDVBV5_VERSION = -version-info 0
else
noinst_LTLIBRARIES = libdvbv5.la
endif

libdvbv5_la_SOURCES = \
	crc32.c crc32.h \
	../include/dvb-frontend.h \
	dvb-legacy-channel-format.c \
	dvb-zap-format.c \
	dvb-v5.c	dvb-v5.h \
	parse_string.c	parse_string.h \
	dvb-demux.c	../include/dvb-demux.h \
	dvb-fe.c	../include/dvb-fe.h \
	dvb-log.c	../include/dvb-log.h \
	dvb-file.c	../include/dvb-file.h \
	dvb-v5-std.c	../include/dvb-v5-std.h \
	dvb-sat.c	../include/dvb-sat.h \
	dvb-scan.c	../include/dvb-scan.h \
	descriptors.c	../include/descriptors.h \
	descriptors/header.c		../include/libdvbv5/header.h \
	descriptors/atsc_header.c	../include/libdvbv5/atsc_header.h \
	descriptors/pat.c		../include/libdvbv5/pat.h \
	descriptors/pmt.c		../include/libdvbv5/pmt.h \
	descriptors/nit.c		../include/libdvbv5/nit.h \
	descriptors/sdt.c		../include/libdvbv5/sdt.h \
	descriptors/vct.c		../include/libdvbv5/vct.h \
	descriptors/mgt.c		../include/libdvbv5/mgt.h \
	descriptors/eit.c		../include/libdvbv5/eit.h \
	descriptors/atsc_eit.c		../include/libdvbv5/atsc_eit.h \
	descriptors/desc_language.c		../include/libdvbv5/desc_language.h \
	descriptors/desc_network_name.c		../include/libdvbv5/desc_network_name.h \
	descriptors/desc_cable_delivery.c	../include/libdvbv5/desc_cable_delivery.h \
	descriptors/desc_sat.c			../include/libdvbv5/desc_sat.h \
	descriptors/desc_terrestrial_delivery.c  ../include/libdvbv5/desc_terrestrial_delivery.h \
	descriptors/desc_t2_delivery.c		../include/libdvbv5/desc_t2_delivery.h \
	descriptors/desc_service.c		../include/libdvbv5/desc_service.h \
	descriptors/desc_frequency_list.c	../include/libdvbv5/desc_frequency_list.h \
	descriptors/desc_service_list.c		../include/libdvbv5/desc_service_list.h \
	descriptors/desc_event_short.c		../include/libdvbv5/desc_event_short.h \
	descriptors/desc_event_extended.c	../include/libdvbv5/desc_event_extended.h \
	descriptors/desc_atsc_service_location.c ../include/libdvbv5/desc_atsc_service_location.h \
	descriptors/desc_hierarchy.c		../include/libdvbv5/desc_hierarchy.h \
	descriptors/desc_extension.c		../include/libdvbv5/desc_extension.h \
	descriptors/desc_isdbt_delivery.c	../include/libdvbv5/desc_isdbt_delivery.h \
	descriptors/desc_logical_channel.c	../include/libdvbv5/desc_logical_channel.h \
	descriptors/desc_ts_info.c		../include/libdvbv5/desc_ts_info.h \
	descriptors/desc_partial_reception.c	../include/libdvbv5/desc_partial_reception.h \
	descriptors/desc_service_location.c	../include/libdvbv5/desc_service_location.h \
	descriptors/mpeg_ts.c		../include/libdvbv5/mpeg_ts.h \
	descriptors/mpeg_pes.c		../include/libdvbv5/mpeg_pes.h \
	descriptors/mpeg_es.c		../include/libdvbv5/mpeg_es.h

libdvbv5_la_CPPFLAGS = -I../.. $(ENFORCE_LIBDVBV5_STATIC)
libdvbv5_la_LDFLAGS = $(LIBDVBV5_VERSION) $(ENFORCE_LIBDVBV5_STATIC) -lm
libdvbv5_la_LIBADD = $(LTLIBICONV)

EXTRA_DIST = README gen_dvb_structs.pl

sync-with-kernel:
	./gen_dvb_structs.pl $(KERNEL_DIR)/include/
