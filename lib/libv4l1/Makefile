override CPPFLAGS += -I../include -fvisibility=hidden

LIBS_libv4l1  = -lpthread

V4L1_OBJS     = libv4l1.o log.o
V4L1COMPAT    = v4l1compat.so
V4L1COMPAT_O  = v4l1compat.o libv4l1.so
TARGETS       = $(V4L1_LIB) libv4l1.pc
INCLUDES      = ../include/libv4l1.h ../include/libv4l1-videodev.h

ifeq ($(LINKTYPE),static)
V4L1_LIB      = libv4l1.a
V4L1_DEPS     = $(V4L1_OBJS)
else
V4L1_LIB      = libv4l1.so
V4L1_DEPS    += $(V4L1_OBJS) ../libv4l2/libv4l2.so
TARGETS      += $(V4L1COMPAT)
override CPPFLAGS += -fPIC
endif

all: $(TARGETS)

-include $(V4L1_OBJS:.o=.d)

$(V4L1_LIB): $(V4L1_DEPS)

$(V4L1COMPAT): $(V4L1COMPAT_O) $(V4L1_LIB)

libv4l1.pc:
	@echo prefix=$(PREFIX) > libv4l1.pc
	@echo libdir=$(LIBDIR) >> libv4l1.pc
	@echo >> libv4l1.pc
	@echo 'Name: libv4l1' >> libv4l1.pc
	@echo 'Description: v4l1 compatibility library' >> libv4l1.pc
	@echo 'Version: '$(V4L_UTILS_VERSION) >> libv4l1.pc
	@echo 'Requires.private: libv4l2' >> libv4l1.pc
	@echo 'Libs: -L$${libdir} -lv4l1' >> libv4l1.pc
	@echo 'Libs.private: '$(LIBS_libv4l1) >> libv4l1.pc
	@echo 'Cflags: -I$${prefix}/include' >> libv4l1.pc

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/include
	install -p -m 644 $(INCLUDES) $(DESTDIR)$(PREFIX)/include
ifeq ($(LINKTYPE),static)
	mkdir -p $(DESTDIR)$(LIBDIR)
	install -m 644 $(V4L1_LIB) $(DESTDIR)$(LIBDIR)
else
	mkdir -p $(DESTDIR)$(LIBDIR)/$(LIBSUBDIR)
	install -m 755 $(V4L1_LIB).$(LIB_RELEASE) $(DESTDIR)$(LIBDIR)
	cd $(DESTDIR)$(LIBDIR) && \
	  ln -f -s $(V4L1_LIB).$(LIB_RELEASE) $(V4L1_LIB)
	install -m 755 $(V4L1COMPAT).$(LIB_RELEASE) \
	  $(DESTDIR)$(LIBDIR)/$(LIBSUBDIR)/$(V4L1COMPAT)
endif
	mkdir -p $(DESTDIR)$(LIBDIR)/pkgconfig
	install -m 644 libv4l1.pc $(DESTDIR)$(LIBDIR)/pkgconfig

include ../../Make.rules
