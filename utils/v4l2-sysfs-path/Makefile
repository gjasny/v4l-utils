TARGETS = v4l2-sysfs-path

all: $(TARGETS)

-include *.d

v4l2-sysfs-path: v4l2-sysfs-path.o ../libv4l2util/libv4l2util.a
	$(CC) $(LDFLAGS) -o $@ $^ -lsysfs

install: $(TARGETS)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGETS) $(DESTDIR)$(PREFIX)/bin

include ../../Make.rules
