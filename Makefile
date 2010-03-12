all install:
	$(MAKE) -C lib $@
	$(MAKE) -C utils $@

sync-with-kernel:
	@if [ ! -f $(KERNEL_DIR)/include/linux/videodev2.h -o \
	      ! -f $(KERNEL_DIR)/include/linux/ivtv.h -o \
	      ! -f $(KERNEL_DIR)/include/media/v4l2-chip-ident.h ]; then \
	  echo "Error you must set KERNEL_DIR to point to an extracted kernel source dir"; \
	  exit 1; \
	fi
	cp -a $(KERNEL_DIR)/include/linux/videodev2.h include/linux
	cp -a $(KERNEL_DIR)/include/linux/ivtv.h include/linux
	cp -a $(KERNEL_DIR)/include/media/v4l2-chip-ident.h include/media
	make -C utils $@

clean::
	rm -f include/*/*~
	$(MAKE) -C lib $@
	$(MAKE) -C utils $@

tag:
	@git tag -a -m "Tag as v4l-utils-$(V4L_UTILS_VERSION)" v4l-utils-$(V4L_UTILS_VERSION)
	@echo "Tagged as v4l-utils-$(V4L_UTILS_VERSION)"

archive-no-tag:
	@git archive --format=tar --prefix=v4l-utils-$(V4L_UTILS_VERSION)/ v4l-utils-$(V4L_UTILS_VERSION) > v4l-utils-$(V4L_UTILS_VERSION).tar
	@bzip2 -f v4l-utils-$(V4L_UTILS_VERSION).tar

archive: clean tag archive-no-tag

export: clean
	tar --transform s/^\./v4l-utils-$(V4L_UTILS_VERSION)/g \
		--exclude=.git -zcvf \
		/tmp/v4l-utils-$(V4L_UTILS_VERSION).tar.gz .

include Make.rules
