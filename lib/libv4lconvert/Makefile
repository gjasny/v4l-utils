CC            = gcc
LD            = gcc

CPPFLAGS      = -fPIC -I../include -I../../../../linux/include

CFLAGS := -g -O1
CFLAGS += -Wall -W -Wno-unused -Wpointer-arith -Wstrict-prototypes

LDFLAGS       = -shared

CONVERT_LIB   = libv4lconvert.so
CONVERT_OBJS  = libv4lconvert.o tinyjpeg.o sn9c10x.o pac207.o \
		jidctflt.o spca561-decompress.o rgbyuv.o spca501.o bayer.o
TARGETS       = $(CONVERT_LIB)
INCLUDES      = ../include/libv4lconvert.h

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

$(CONVERT_LIB): $(CONVERT_OBJS)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/include
	install -p -m 644 $(INCLUDES) $(DESTDIR)$(PREFIX)/include
	mkdir -p $(DESTDIR)$(LIBDIR)
	install -m 755 $(CONVERT_LIB).$(LIB_RELEASE) $(DESTDIR)$(LIBDIR)
	cd $(DESTDIR)$(LIBDIR) && \
	  ln -f -s $(CONVERT_LIB).$(LIB_RELEASE) $(CONVERT_LIB)

clean::
	rm -f *.so* *.o log *~
	rm -f *.d

%.o: %.c
	$(CC) -c -MMD $(CPPFLAGS) $(CFLAGS) -o $@ $<

%.so:
	$(CC) $(LDFLAGS) -Wl,-soname,$@.$(LIB_RELEASE) -o $@.$(LIB_RELEASE) $^
	ln -f -s $@.$(LIB_RELEASE) $@
