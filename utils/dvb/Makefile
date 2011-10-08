TARGETS = dvb-v5.h

all: $(TARGETS)

-include *.d

dvb-v5.h:
	./gen_dvb_structs.pl $(KERNEL_DIR)/include/

ir-keytable: keytable.o
	$(CC) $(LDFLAGS) -o $@ $^

include ../../Make.rules
