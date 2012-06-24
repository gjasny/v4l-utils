if WITH_LIBV4L
lib_LTLIBRARIES = libv4lconvert.la
libv4lconvertpriv_PROGRAMS = ov511-decomp ov518-decomp
include_HEADERS = ../include/libv4lconvert.h
pkgconfig_DATA = libv4lconvert.pc
else
noinst_LTLIBRARIES = libv4lconvert.la
endif

libv4lconvert_la_SOURCES = \
  libv4lconvert.c tinyjpeg.c sn9c10x.c sn9c20x.c pac207.c  mr97310a.c \
  flip.c crop.c jidctflt.c spca561-decompress.c \
  rgbyuv.c sn9c2028-decomp.c spca501.c sq905c.c bayer.c hm12.c \
  stv0680.c cpia1.c se401.c jpgl.c jpeg.c jl2005bcd.c \
  control/libv4lcontrol.c control/libv4lcontrol.h control/libv4lcontrol-priv.h \
  processing/libv4lprocessing.c processing/whitebalance.c processing/autogain.c \
  processing/gamma.c processing/libv4lprocessing.h processing/libv4lprocessing-priv.h \
  helper.c helper-funcs.h libv4lconvert-priv.h libv4lsyscall-priv.h \
  tinyjpeg.h tinyjpeg-internal.h
if HAVE_JPEG
libv4lconvert_la_SOURCES += jpeg_memsrcdest.c jpeg_memsrcdest.h
endif
libv4lconvert_la_CPPFLAGS = $(CFLAG_VISIBILITY) $(ENFORCE_LIBV4L_STATIC)
libv4lconvert_la_LDFLAGS = -version-info 0 -lrt -lm $(JPEG_LIBS) $(ENFORCE_LIBV4L_STATIC)

ov511_decomp_SOURCES = ov511-decomp.c

ov518_decomp_SOURCES = ov518-decomp.c

