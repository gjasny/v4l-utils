TARGETS = dvb-fe-tool dvbv5-zap dvbv5-scan dvb-format-convert

all: $(TARGETS)

-include *.d

dvb-fe-tool: dvb-fe-tool.o dvb-fe.o libsat.o dvb-file.o dvb-legacy-channel-format.o dvb-zap-format.o
dvbv5-zap: dvbv5-zap.o dvb-fe.o dvb-demux.o dvb-file.o \
		dvb-zap-format.o dvb-legacy-channel-format.o \
		libsat.o
dvbv5-scan: dvbv5-scan.o dvb-fe.o dvb-demux.o dvb-file.o \
		dvb-zap-format.o dvb-legacy-channel-format.o \
		parse_string.o descriptors.o libscan.o \
		libsat.o

dvb-format-convert: dvb-format-convert.o dvb-file.o \
		dvb-zap-format.o dvb-legacy-channel-format.o \
		dvb-fe.o libsat.o


dvb-v5.h:
	./gen_dvb_structs.pl $(KERNEL_DIR)/include/

include ../../Make.rules

install: $(TARGETS)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGETS) $(DESTDIR)$(PREFIX)/bin
