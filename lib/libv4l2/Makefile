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

ifeq ($(DESTDIR),)
DESTDIR = /usr/local
endif

all: $(TARGETS)

$(V4L2_LIB): $(V4L2_OBJS)

$(V4L2CONVERT): $(V4L2CONVERT_O) $(V4L2_LIB)

install: all
	mkdir -p $(DESTDIR)/include
	cp $(INCLUDES) $(DESTDIR)/include
	mkdir -p $(DESTDIR)/lib
	cp $(V4L2_LIB).$(LIB_RELEASE) $(DESTDIR)/lib
	cd $(DESTDIR)/lib && \
	  ln -f -s $(V4L2_LIB).$(LIB_RELEASE) $(V4L2_LIB)
	cp $(V4L2CONVERT).$(LIB_RELEASE) $(DESTDIR)/lib
	cd $(DESTDIR)/lib && \
	  ln -f -s $(V4L2CONVERT).$(LIB_RELEASE) $(V4L2CONVERT)

clean::
	rm -f *.so* *.o log *~
	rm -f *.d

%.o: %.c
	$(CC) -c -MMD $(CPPFLAGS) $(CFLAGS) -o $@ $<

%.so:
	$(CC) $(LDFLAGS) -Wl,-soname,$@.$(LIB_RELEASE) -o $@.$(LIB_RELEASE) $^
	ln -f -s $@.$(LIB_RELEASE) $@
