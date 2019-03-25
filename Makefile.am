AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = v4l-utils-po libdvbv5-po lib

if WITH_V4LUTILS
SUBDIRS += utils contrib
endif

EXTRA_DIST = android-config.h bootstrap.sh doxygen_libdvbv5.cfg include COPYING.libv4l \
        COPYING.libdvbv5 README.libv4l README.lib-multi-threading TODO.libdvbv5 \
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
	      ! -f $(KERNEL_DIR)/usr/include/linux/lirc.h -o \
	      ! -f $(KERNEL_DIR)/usr/include/linux/bpf.h -o \
	      ! -f $(KERNEL_DIR)/usr/include/linux/bpf_common.h ]; then \
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
	cp -a $(KERNEL_DIR)/usr/include/linux/media-bus-format.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/usr/include/linux/media.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/usr/include/linux/ivtv.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/usr/include/linux/dvb/frontend.h $(top_srcdir)/include/linux/dvb
	cp $(top_srcdir)/include/linux/dvb/frontend.h $(top_srcdir)/lib/include/libdvbv5/dvb-frontend.h
	cp -a $(KERNEL_DIR)/usr/include/linux/dvb/dmx.h $(top_srcdir)/include/linux/dvb
	cp -a $(KERNEL_DIR)/usr/include/linux/lirc.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/usr/include/linux/bpf.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/usr/include/linux/bpf_common.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/usr/include/linux/cec.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/usr/include/linux/cec-funcs.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/drivers/media/common/v4l2-tpg/v4l2-tpg-core.c $(top_srcdir)/utils/common
	cp -a $(KERNEL_DIR)/drivers/media/common/v4l2-tpg/v4l2-tpg-colors.c $(top_srcdir)/utils/common
	cp -a $(KERNEL_DIR)/include/media/tpg/v4l2-tpg* $(top_srcdir)/utils/common
	cp -a $(KERNEL_DIR)/include/media/fwht-ctrls.h $(top_srcdir)/utils/common
	patch -d $(top_srcdir) --no-backup-if-mismatch -p0 <$(top_srcdir)/utils/common/v4l2-tpg.patch
	cp -a $(KERNEL_DIR)/drivers/media/platform/vicodec/codec-fwht.[ch] $(top_srcdir)/utils/common/
	cp -a $(KERNEL_DIR)/drivers/media/platform/vicodec/codec-v4l2-fwht.[ch] $(top_srcdir)/utils/common/
	patch -d $(top_srcdir) --no-backup-if-mismatch -p1 <$(top_srcdir)/utils/common/codec-fwht.patch
	grep V4L2_PIX_FMT.*descr $(KERNEL_DIR)/drivers/media/v4l2-core/v4l2-ioctl.c | perl -pe 's/.*V4L2_PIX_FMT/\tcase V4L2_PIX_FMT/; s/:.*descr = /: return /; s/;.*/;/;' >$(top_srcdir)/utils/common/v4l2-pix-formats.h

	$(MAKE) -C utils/keytable $@
	$(MAKE) -C lib/libdvbv5 $@
	$(MAKE) -C contrib/freebsd $@
	$(MAKE) -C contrib/test $@
	$(MAKE) -C contrib/xc3028-firmware $@
