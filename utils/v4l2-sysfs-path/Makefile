TARGETS = v4l2-sysfs-path

all: $(TARGETS)

-include *.d

v4l2-sysfs-path: v4l2-sysfs-path.o ../libmedia_dev/libmedia_dev.a
	$(CC) $(LDFLAGS) -o $@ $^

install: $(TARGETS)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGETS) $(DESTDIR)$(PREFIX)/bin

include ../../Make.rules
