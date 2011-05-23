OBJS    = frequencies.o v4l2_driver.o get_media_devices.o
TARGETS = libv4l2util.a
INCLUDES = get_media_devices.h libv4l2util.h v4l2_driver.h

all: $(TARGETS)

-include $(OBJS:.o=.d)

libv4l2util.a: $(OBJS)

install: $(TARGETS)
	mkdir -p $(DESTDIR)$(PREFIX)/include
	install -p -m 644 $(INCLUDES) $(DESTDIR)$(PREFIX)/include
	mkdir -p $(DESTDIR)$(LIBDIR)
	install -m 644 $(TARGETS) $(DESTDIR)$(LIBDIR)

include ../../Make.rules
