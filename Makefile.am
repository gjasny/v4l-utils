AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = lib

if WITH_V4LUTILS
SUBDIRS += utils contrib
endif

EXTRA_DIST = android-config.h doxygen_libdvbv5.cfg include COPYING.libv4l README.libv4l README.lib-multi-threading TODO.libdvbv5 \
        doc/libdvbv5-index.doc

include $(top_srcdir)/aminclude.am

# custom targets

sync-with-kernel:
	@if [ ! -f $(KERNEL_DIR)/usr/include/linux/videodev2.h -o \
	      ! -f $(KERNEL_DIR)/usr/include/linux/fb.h -o \
	      ! -f $(KERNEL_DIR)/usr/include/linux/v4l2-controls.h -o \
	      ! -f $(KERNEL_DIR)/usr/include/linux/v4l2-common.h -o \
	      ! -f $(KERNEL_DIR)/usr/include/linux/v4l2-subdev.h -o \
	      ! -f $(KERNEL_DIR)/usr/include/linux/v4l2-mediabus.h -o \
	      ! -f $(KERNEL_DIR)/usr/include/linux/ivtv.h -o \
	      ! -f $(KERNEL_DIR)/usr/include/linux/dvb/frontend.h -o \
	      ! -f $(KERNEL_DIR)/usr/include/linux/dvb/dmx.h -o \
	      ! -f $(KERNEL_DIR)/usr/include/linux/dvb/audio.h -o \
	      ! -f $(KERNEL_DIR)/usr/include/linux/dvb/video.h ]; then \
	  echo "Error you must set KERNEL_DIR to point to an extracted kernel source dir"; \
	  echo "and run 'make headers_install' in \$$KERNEL_DIR."; \
	  exit 1; \
	fi
	cp -a $(KERNEL_DIR)/usr/include/linux/videodev2.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/usr/include/linux/fb.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/usr/include/linux/v4l2-controls.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/usr/include/linux/v4l2-common.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/usr/include/linux/v4l2-subdev.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/usr/include/linux/v4l2-mediabus.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/usr/include/linux/ivtv.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/usr/include/linux/dvb/frontend.h $(top_srcdir)/include/linux/dvb
	cp -a $(KERNEL_DIR)/usr/include/linux/dvb/dmx.h $(top_srcdir)/include/linux/dvb
	cp -a $(KERNEL_DIR)/usr/include/linux/dvb/audio.h $(top_srcdir)/include/linux/dvb
	cp -a $(KERNEL_DIR)/usr/include/linux/dvb/video.h $(top_srcdir)/include/linux/dvb
	cp -a $(KERNEL_DIR)/drivers/media/platform/vivid/vivid-tpg.[ch] $(top_srcdir)/utils/v4l2-ctl
	cp -a $(KERNEL_DIR)/drivers/media/platform/vivid/vivid-tpg-colors.[ch] $(top_srcdir)/utils/v4l2-ctl
	patch -p1 <$(top_srcdir)/utils/v4l2-ctl/vivid-tpg.h.patch

	$(MAKE) -C utils/keytable $@
	$(MAKE) -C utils/xc3028-firmware $@
	$(MAKE) -C lib/libdvbv5 $@
	$(MAKE) -C contrib/freebsd $@
	$(MAKE) -C contrib/test $@
