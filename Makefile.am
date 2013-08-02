AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = lib

if WITH_V4LUTILS
SUBDIRS += utils contrib
endif

EXTRA_DIST = include COPYING.libv4l README.libv4l README.lib-multi-threading

# custom targets

sync-with-kernel:
	@if [ ! -f $(KERNEL_DIR)/include/uapi/linux/videodev2.h -o \
	      ! -f $(KERNEL_DIR)/include/uapi/linux/v4l2-controls.h -o \
	      ! -f $(KERNEL_DIR)/include/uapi/linux/v4l2-common.h -o \
	      ! -f $(KERNEL_DIR)/include/uapi/linux/ivtv.h -o \
	      ! -f $(KERNEL_DIR)/include/uapi/linux/dvb/frontend.h -o \
	      ! -f $(KERNEL_DIR)/include/uapi/linux/dvb/dmx.h -o \
	      ! -f $(KERNEL_DIR)/include/uapi/linux/dvb/audio.h -o \
	      ! -f $(KERNEL_DIR)/include/uapi/linux/dvb/video.h ]; then \
	  echo "Error you must set KERNEL_DIR to point to an extracted kernel source dir"; \
	  exit 1; \
	fi
	cp -a $(KERNEL_DIR)/include/uapi/linux/videodev2.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/include/uapi/linux/v4l2-controls.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/include/uapi/linux/v4l2-common.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/include/uapi/linux/ivtv.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/include/uapi/linux/dvb/frontend.h $(top_srcdir)/include/linux/dvb
	cp -a $(KERNEL_DIR)/include/uapi/linux/dvb/dmx.h $(top_srcdir)/include/linux/dvb
	cp -a $(KERNEL_DIR)/include/uapi/linux/dvb/audio.h $(top_srcdir)/include/linux/dvb
	cp -a $(KERNEL_DIR)/include/uapi/linux/dvb/video.h $(top_srcdir)/include/linux/dvb

	$(MAKE) -C utils/keytable $@
	$(MAKE) -C utils/xc3028-firmware $@
	$(MAKE) -C lib/libdvbv5 $@
	$(MAKE) -C contrib/freebsd $@
	$(MAKE) -C contrib/test $@
