TARGETS = dvb-fe-tool dvbzap

all: $(TARGETS)

-include *.d

dvb-fe-tool: dvb-fe-tool.o dvb-fe.o
dvbzap: dvbzap.o dvb-fe.o util.o

	$(CC) $(LDFLAGS) -o $@ $^

dvb-v5.h:
	./gen_dvb_structs.pl $(KERNEL_DIR)/include/

include ../../Make.rules
