TARGETS = dvb-fe-tool dvbv5-zap dvbv5-scan

all: $(TARGETS)

-include *.d

dvb-fe-tool: dvb-fe-tool.o dvb-fe.o
dvbv5-zap: dvbv5-zap.o dvb-fe.o dvb-demux.o dvb-file.o \
		dvb-zap-format.o dvb-legacy-channel-format.o

dvbv5-scan: dvbv5-scan.o dvb-fe.o dvb-demux.o dvb-file.o \
		dvb-zap-format.o dvb-legacy-channel-format.o \
		libscan.o

	$(CC) $(LDFLAGS) -o $@ $^

dvb-v5.h:
	./gen_dvb_structs.pl $(KERNEL_DIR)/include/

include ../../Make.rules
