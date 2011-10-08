TARGETS = dvbv5_scan

all: $(TARGETS)

-include *.d

dvb5_scan: dvb5_scan.c

dvb-v5.h:
	./gen_dvb_structs.pl $(KERNEL_DIR)/include/

ir-keytable: keytable.o
	$(CC) $(LDFLAGS) -o $@ $^

include ../../Make.rules
