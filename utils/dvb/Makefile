TARGETS = dvbv5_scan

all: $(TARGETS)

-include *.d

dvbv5_scan: dvbv5_scan.o dvb-fe.o
	$(CC) $(LDFLAGS) -o $@ $^

dvb-v5.h:
	./gen_dvb_structs.pl $(KERNEL_DIR)/include/

include ../../Make.rules
