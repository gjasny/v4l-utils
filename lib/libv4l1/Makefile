CC            = gcc
LD            = gcc

CPPFLAGS      = -fPIC -I../include -I../../../../linux/include

CFLAGS := -g -O1
CFLAGS += -Wall -W -Wno-unused -Wpointer-arith -Wstrict-prototypes

LDFLAGS       = -shared

V4L1_OBJS     = libv4l1.o log.o ../libv4l2/libv4l2.so
V4L1_LIB      = libv4l1.so
V4L1COMPAT    = v4l1compat.so
V4L1COMPAT_O  = v4l1compat.o libv4l1.so
TARGETS       = $(V4L1_LIB) $(V4L1COMPAT)
INCLUDES      = ../include/libv4l1.h

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


$(V4L1_LIB): $(V4L1_OBJS)

$(V4L1COMPAT): $(V4L1COMPAT_O) $(V4L1_LIB)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/include
	install -p -m 644 $(INCLUDES) $(DESTDIR)$(PREFIX)/include
	mkdir -p $(DESTDIR)$(LIBDIR)/libv4l
	install -m 755 $(V4L1_LIB).$(LIB_RELEASE) $(DESTDIR)$(LIBDIR)
	cd $(DESTDIR)$(LIBDIR) && \
	  ln -f -s $(V4L1_LIB).$(LIB_RELEASE) $(V4L1_LIB)
	install -m 755 $(V4L1COMPAT).$(LIB_RELEASE) \
	  $(DESTDIR)$(LIBDIR)/libv4l/$(V4L1COMPAT)

clean::
	rm -f *.so* *.o log *~
	rm -f *.d

%.o: %.c
	$(CC) -c -MMD $(CPPFLAGS) $(CFLAGS) -o $@ $<

%.so:
	$(CC) $(LDFLAGS) -Wl,-soname,$@.$(LIB_RELEASE) -o $@.$(LIB_RELEASE) $^
	ln -f -s $@.$(LIB_RELEASE) $@
