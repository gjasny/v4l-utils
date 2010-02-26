TARGETS = decode_tm6000

all: $(TARGETS)

-include *.d

decode_tm6000: decode_tm6000.o ../libv4l2util/libv4l2util.a
	$(CC) $(LDFLAGS) -o $@ $^

install: $(TARGETS)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGETS) $(DESTDIR)$(PREFIX)/bin

include ../../Make.rules
