TARGETS = decode_tm6000

all: $(TARGETS)

decode_tm6000: decode_tm6000.o ../libv4l2util/libv4l2util.a
	$(CC) $(LDFLAGS) -o $@ $^

include ../../Make.rules
