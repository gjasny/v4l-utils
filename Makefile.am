AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
	lib/libv4lconvert \
	lib/libv4l2 \
	lib/libv4l1 \
	lib/libdvbv5

if WITH_V4LUTILS
SUBDIRS += \
	utils/libv4l2util \
	utils/libmedia_dev \
	utils/decode_tm6000 \
	utils/keytable \
	utils/v4l2-compliance \
	utils/v4l2-ctl \
	utils/v4l2-dbg \
	utils/v4l2-sysfs-path

if LINUX_OS
SUBDIRS += \
	utils/dvb \
	utils/rds \
	utils/xc3028-firmware
endif

if BUILD_QT
SUBDIRS += utils/qv4l2
endif
endif

EXTRA_DIST = contrib include README.lib README.lib-multi-threading

# custom targets

sync-with-kernel:
	@if [ ! -f $(KERNEL_DIR)/include/linux/videodev2.h -o \
	      ! -f $(KERNEL_DIR)/include/linux/ivtv.h -o \
	      ! -f $(KERNEL_DIR)/include/media/v4l2-chip-ident.h ]; then \
	  echo "Error you must set KERNEL_DIR to point to an extracted kernel source dir"; \
	  exit 1; \
	fi
	cp -a $(KERNEL_DIR)/include/linux/videodev2.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/include/linux/ivtv.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/include/media/v4l2-chip-ident.h $(top_srcdir)/include/media

	$(MAKE) -C utils/keytable $@
	$(MAKE) -C utils/v4l2-dbg $@
	$(MAKE) -C utils/xc3028-firmware $@
	$(MAKE) -C lib/libdvbv5 $@

