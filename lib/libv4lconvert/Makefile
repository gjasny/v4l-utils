override CPPFLAGS += -I../include -fvisibility=hidden

LIBS_libv4lconvert = -lrt -lm

ifeq ($(LINKTYPE),static)
CONVERT_LIB   = libv4lconvert.a
else
CONVERT_LIB   = libv4lconvert.so
override CPPFLAGS += -fPIC
endif

CONVERT_OBJS  = libv4lconvert.o tinyjpeg.o sn9c10x.o sn9c20x.o pac207.o \
		mr97310a.o flip.o crop.o jidctflt.o spca561-decompress.o \
		rgbyuv.o sn9c2028-decomp.o spca501.o sq905c.o bayer.o hm12.o \
		stv0680.o cpia1.o \
		control/libv4lcontrol.o processing/libv4lprocessing.o \
		processing/whitebalance.o processing/autogain.o \
		processing/gamma.o helper.o
TARGETS       = $(CONVERT_LIB) libv4lconvert.pc ov511-decomp ov518-decomp
INCLUDES      = ../include/libv4lconvert.h

override CPPFLAGS += -DLIBDIR=\"$(LIBDIR)\" -DLIBSUBDIR=\"$(LIBSUBDIR)\"

all: $(TARGETS)

-include $(CONVERT_OBJS:.o=.d)

$(CONVERT_LIB): $(CONVERT_OBJS)

libv4lconvert.pc:
	@echo prefix=$(PREFIX) > libv4lconvert.pc
	@echo libdir=$(LIBDIR) >> libv4lconvert.pc
	@echo >> libv4lconvert.pc
	@echo 'Name: libv4lconvert' >> libv4lconvert.pc
	@echo 'Description: v4l format conversion library' >> libv4lconvert.pc
	@echo 'Version: '$(V4L_UTILS_VERSION) >> libv4lconvert.pc
	@echo 'Libs: -L$${libdir} -lv4lconvert' >> libv4lconvert.pc
	@echo 'Libs.private: -lrt -lm' >> libv4lconvert.pc
	@echo 'Cflags: -I$${prefix}/include' >> libv4lconvert.pc

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/include
	install -p -m 644 $(INCLUDES) $(DESTDIR)$(PREFIX)/include
	mkdir -p $(DESTDIR)$(LIBDIR)/$(LIBSUBDIR)
ifeq ($(LINKTYPE),static)
	install -m 644 $(CONVERT_LIB) $(DESTDIR)$(LIBDIR)
else
	install -m 755 $(CONVERT_LIB).$(LIB_RELEASE) $(DESTDIR)$(LIBDIR)
	cd $(DESTDIR)$(LIBDIR) && \
	  ln -f -s $(CONVERT_LIB).$(LIB_RELEASE) $(CONVERT_LIB)
endif
	install -m 755 *-decomp $(DESTDIR)$(LIBDIR)/$(LIBSUBDIR)
	mkdir -p $(DESTDIR)$(LIBDIR)/pkgconfig
	install -m 644 libv4lconvert.pc $(DESTDIR)$(LIBDIR)/pkgconfig

clean::
	rm -f */*~ */*.o */*.d */*.orig */*.rej */DEADJOE

ov511-decomp: ov511-decomp.o
ov518-decomp: ov518-decomp.o

include ../../Make.rules
