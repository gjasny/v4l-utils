if WITH_LIBDVBV5
lib_LTLIBRARIES = libdvbv5.la

otherincludedir = $(includedir)/libdvbv5
otherinclude_HEADERS = \
	../include/libdvbv5/libdvb-version.h \
	../include/libdvbv5/dvb-demux.h \
	../include/libdvbv5/dvb-v5-std.h \
	../include/libdvbv5/dvb-file.h \
	../include/libdvbv5/countries.h \
	../include/libdvbv5/crc32.h \
	../include/libdvbv5/dvb-dev.h \
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
	../include/libdvbv5/desc_event_short.h \
	../include/libdvbv5/desc_event_extended.h \
	../include/libdvbv5/desc_atsc_service_location.h \
	../include/libdvbv5/desc_hierarchy.h \
	../include/libdvbv5/desc_extension.h \
	../include/libdvbv5/desc_isdbt_delivery.h \
	../include/libdvbv5/desc_logical_channel.h \
	../include/libdvbv5/desc_ts_info.h \
	../include/libdvbv5/desc_partial_reception.h \
	../include/libdvbv5/desc_ca.h \
	../include/libdvbv5/desc_ca_identifier.h \
	../include/libdvbv5/nit.h \
	../include/libdvbv5/sdt.h \
	../include/libdvbv5/vct.h \
	../include/libdvbv5/atsc_header.h \
	../include/libdvbv5/mgt.h \
	../include/libdvbv5/eit.h \
	../include/libdvbv5/cat.h \
	../include/libdvbv5/atsc_eit.h \
	../include/libdvbv5/mpeg_ts.h \
	../include/libdvbv5/mpeg_pes.h \
	../include/libdvbv5/mpeg_es.h

pkgconfig_DATA = libdvbv5.pc
LIBDVBV5_VERSION = -version-info 0
else
noinst_LTLIBRARIES = libdvbv5.la
endif

libdvbv5_la_SOURCES = \
        compat-soname.c \
	crc32.c \
	countries.c \
	dvb-legacy-channel-format.c \
	dvb-zap-format.c \
	dvb-vdr-format.c \
	dvb-v5.c	 \
	dvb-v5.h	 \
	parse_string.c	 \
	parse_string.h	 \
	dvb-demux.c	 \
	dvb-dev.c	 \
	dvb-dev-local.c	 \
	dvb-dev-priv.h   \
	dvb-fe.c	 \
	dvb-fe-priv.h    \
	dvb-log.c	 \
	dvb-file.c	 \
	dvb-v5-std.c	 \
	dvb-sat.c	 \
	dvb-scan.c	 \
	descriptors.c	 \
	tables/header.c		\
	tables/pat.c		\
	tables/pmt.c		\
	tables/nit.c		\
	tables/sdt.c		\
	tables/vct.c		\
	tables/mgt.c		\
	tables/eit.c		\
	tables/cat.c		\
	tables/atsc_eit.c	\
	tables/mpeg_ts.c	\
	tables/mpeg_pes.c	\
	tables/mpeg_es.c	\
	descriptors/desc_language.c		\
	descriptors/desc_network_name.c		\
	descriptors/desc_cable_delivery.c	\
	descriptors/desc_sat.c			\
	descriptors/desc_terrestrial_delivery.c \
	descriptors/desc_t2_delivery.c		\
	descriptors/desc_service.c		\
	descriptors/desc_frequency_list.c	\
	descriptors/desc_event_short.c		\
	descriptors/desc_event_extended.c	\
	descriptors/desc_atsc_service_location.c \
	descriptors/desc_hierarchy.c		\
	descriptors/desc_extension.c		\
	descriptors/desc_isdbt_delivery.c	\
	descriptors/desc_logical_channel.c	\
	descriptors/desc_ts_info.c		\
	descriptors/desc_partial_reception.c	\
	descriptors/desc_ca.c			\
	descriptors/desc_ca_identifier.c

if WITH_DVBV5_REMOTE
libdvbv5_la_SOURCES += \
	dvb-dev-remote.c
endif

libdvbv5_la_CPPFLAGS = -I../.. $(ENFORCE_LIBDVBV5_STATIC) $(LIBUDEV_CFLAGS)
libdvbv5_la_LDFLAGS = $(LIBDVBV5_VERSION) $(ENFORCE_LIBDVBV5_STATIC) $(LIBUDEV_LIBS) -lm -lrt
libdvbv5_la_LIBADD = $(LTLIBICONV)

EXTRA_DIST = README gen_dvb_structs.pl

sync-with-kernel:
	./gen_dvb_structs.pl $(KERNEL_DIR)/usr/include/
