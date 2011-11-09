TARGETS = dvb-fe-tool

all: $(TARGETS)

-include *.d

dvb-fe-tool: dvb-fe-tool.o dvb-fe.o
	$(CC) $(LDFLAGS) -o $@ $^

dvb-v5.h:
	./gen_dvb_structs.pl $(KERNEL_DIR)/include/

include ../../Make.rules
