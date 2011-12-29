TARGETS = dvb-fe-tool dvbv5-zap

all: $(TARGETS)

-include *.d

dvb-fe-tool: dvb-fe-tool.o dvb-fe.o
dvbv5-zap: dvbv5-zap.o dvb-fe.o util.o dvb-file.o \
		dvb-zap-format.o dvb-legacy-channel-format.o

	$(CC) $(LDFLAGS) -o $@ $^

dvb-v5.h:
	./gen_dvb_structs.pl $(KERNEL_DIR)/include/

include ../../Make.rules
