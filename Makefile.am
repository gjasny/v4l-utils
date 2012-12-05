AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = lib

if WITH_V4LUTILS
SUBDIRS += utils contrib
endif

EXTRA_DIST = include README.lib README.lib-multi-threading

# custom targets

sync-with-kernel:
	@if [ ! -f $(KERNEL_DIR)/include/uapi/linux/videodev2.h -o \
	      ! -f $(KERNEL_DIR)/include/uapi/linux/v4l2-controls.h -o \
	      ! -f $(KERNEL_DIR)/include/uapi/linux/v4l2-common.h -o \
	      ! -f $(KERNEL_DIR)/include/uapi/linux/ivtv.h -o \
	      ! -f $(KERNEL_DIR)/include/media/v4l2-chip-ident.h ]; then \
	  echo "Error you must set KERNEL_DIR to point to an extracted kernel source dir"; \
	  exit 1; \
	fi
	cp -a $(KERNEL_DIR)/include/uapi/linux/videodev2.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/include/uapi/linux/v4l2-controls.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/include/uapi/linux/v4l2-common.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/include/uapi/linux/ivtv.h $(top_srcdir)/include/linux
	cp -a $(KERNEL_DIR)/include/media/v4l2-chip-ident.h $(top_srcdir)/include/media

	$(MAKE) -C utils/keytable $@
	$(MAKE) -C utils/v4l2-dbg $@
	$(MAKE) -C utils/xc3028-firmware $@
	$(MAKE) -C lib/libdvbv5 $@
	$(MAKE) -C contrib/freebsd $@

