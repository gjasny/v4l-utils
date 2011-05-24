OBJS    = get_media_devices.o
TARGETS = libmedia_dev.a
INCLUDES = get_media_devices.h

all: $(TARGETS)

-include $(OBJS:.o=.d)

libmedia_dev.a: $(OBJS)

install: $(TARGETS)
#	mkdir -p $(DESTDIR)$(PREFIX)/include
#	install -p -m 644 $(INCLUDES) $(DESTDIR)$(PREFIX)/include
#	mkdir -p $(DESTDIR)$(LIBDIR)
#	install -m 644 $(TARGETS) $(DESTDIR)$(LIBDIR)

include ../../Make.rules
