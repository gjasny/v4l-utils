CC            = gcc
LD            = gcc

CPPFLAGS      = -fPIC -I../include -I../../../../linux/include

CFLAGS := -g -O1
CFLAGS += -Wall -W -Wno-unused -Wpointer-arith -Wstrict-prototypes

LDFLAGS       = -shared

V4L2_OBJS     = libv4l2.o log.o ../libv4lconvert/libv4lconvert.so
V4L2_LIB      = libv4l2.so
V4L2CONVERT   = v4l2convert.so
V4L2CONVERT_O = v4l2convert.o libv4l2.so
TARGETS       = $(V4L2_LIB) $(V4L2CONVERT)
INCLUDES      = ../include/libv4l2.h

ifeq ($(LIB_RELEASE),)
LIB_RELEASE = 0
endif

ifeq ($(PREFIX),)
PREFIX = /usr/local
endif

ifeq ($(LIBDIR),)
LIBDIR = $(PREFIX)/lib
endif

all: $(TARGETS)

$(V4L2_LIB): $(V4L2_OBJS)

$(V4L2CONVERT): $(V4L2CONVERT_O) $(V4L2_LIB)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/include
	install -p -m 644 $(INCLUDES) $(DESTDIR)$(PREFIX)/include
	mkdir -p $(DESTDIR)$(LIBDIR)/libv4l
	install -m 755 $(V4L2_LIB).$(LIB_RELEASE) $(DESTDIR)$(LIBDIR)
	cd $(DESTDIR)$(LIBDIR) && \
	  ln -f -s $(V4L2_LIB).$(LIB_RELEASE) $(V4L2_LIB)
	install -m 755 $(V4L2CONVERT).$(LIB_RELEASE) \
	  $(DESTDIR)$(LIBDIR)/libv4l/$(V4L2CONVERT)

clean::
	rm -f *.so* *.o log *~
	rm -f *.d

%.o: %.c
	$(CC) -c -MMD $(CPPFLAGS) $(CFLAGS) -o $@ $<

%.so:
	$(CC) $(LDFLAGS) -Wl,-soname,$@.$(LIB_RELEASE) -o $@.$(LIB_RELEASE) $^
	ln -f -s $@.$(LIB_RELEASE) $@
