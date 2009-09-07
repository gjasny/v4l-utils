override CPPFLAGS += -I../include -I../../../include -fvisibility=hidden

CFLAGS := -g -O1
CFLAGS += -Wall -Wno-unused -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes

LIBS_libv4l2  = -lpthread

V4L2_OBJS     = libv4l2.o log.o
V4L2CONVERT   = v4l2convert.so
V4L2CONVERT_O = v4l2convert.o libv4l2.so
TARGETS       = $(V4L2_LIB) libv4l2.pc
INCLUDES      = ../include/libv4l2.h

ifeq ($(LINKTYPE),static)
V4L2_LIB      = libv4l2.a
V4L2_DEPS     = $(V4L2_OBJS)
else
V4L2_LIB      = libv4l2.so
V4L2_DEPS    += $(V4L2_OBJS) ../libv4lconvert/libv4lconvert.so
TARGETS      += $(V4L2CONVERT)
override CPPFLAGS += -fPIC
endif

ifeq ($(LIB_RELEASE),)
LIB_RELEASE = 0
endif

ifeq ($(PREFIX),)
PREFIX = /usr/local
endif

ifeq ($(LIBDIR),)
LIBDIR = $(PREFIX)/lib
endif

ifeq ($(LIBSUBDIR),)
LIBSUBDIR = libv4l
endif

all: $(TARGETS)

-include $(V4L2_OBJS:.o=.d)

$(V4L2_LIB): $(V4L2_DEPS)

$(V4L2CONVERT): $(V4L2CONVERT_O) $(V4L2_LIB)

libv4l2.pc:
	@echo prefix=$(PREFIX) > libv4l2.pc
	@echo libdir=$(LIBDIR) >> libv4l2.pc
	@echo >> libv4l2.pc
	@echo 'Name: libv4l2' >> libv4l2.pc
	@echo 'Description: v4l2 device access library' >> libv4l2.pc
	@echo 'Version: '$(V4L2_LIB_VERSION) >> libv4l2.pc
	@echo 'Requires.private: libv4lconvert' >> libv4l2.pc
	@echo 'Libs: -L$${libdir} -lv4l2' >> libv4l2.pc
	@echo 'Libs.private: -lpthread' >> libv4l2.pc
	@echo 'Cflags: -I$${prefix}/include' >> libv4l2.pc

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/include
	install -p -m 644 $(INCLUDES) $(DESTDIR)$(PREFIX)/include
ifeq ($(LINKTYPE),static)
	mkdir -p $(DESTDIR)$(LIBDIR)
	install -m 644 $(V4L2_LIB) $(DESTDIR)$(LIBDIR)
else
	mkdir -p $(DESTDIR)$(LIBDIR)/$(LIBSUBDIR)
	install -m 755 $(V4L2_LIB).$(LIB_RELEASE) $(DESTDIR)$(LIBDIR)
	cd $(DESTDIR)$(LIBDIR) && \
	  ln -f -s $(V4L2_LIB).$(LIB_RELEASE) $(V4L2_LIB)
	install -m 755 $(V4L2CONVERT).$(LIB_RELEASE) \
	  $(DESTDIR)$(LIBDIR)/$(LIBSUBDIR)/$(V4L2CONVERT)
endif
	mkdir -p $(DESTDIR)$(LIBDIR)/pkgconfig
	install -m 644 libv4l2.pc $(DESTDIR)$(LIBDIR)/pkgconfig

clean::
	rm -f *.a *.so* *.o *.d libv4l2.pc log *~ *.orig *.rej DEADJOE

%.o: %.c
	$(CC) -Wp,-MMD,"$*.d",-MQ,"$@",-MP -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

%.so:
	$(CC) -shared $(LDFLAGS) -Wl,-soname,$@.$(LIB_RELEASE) -o $@.$(LIB_RELEASE) $^ $(LIBS_$*)
	ln -f -s $@.$(LIB_RELEASE) $@

%.a:
	$(AR) cqs $@ $^
