all install:
	$(MAKE) -C lib $@

clean::
	rm -f include/*~
	$(MAKE) -C lib $@

tag:
	@git tag -a -m "Tag as v4l-utils-$(V4L_UTILS_VERSION)" v4l-utils-$(V4L_UTILS_VERSION)
	@echo "Tagged as v4l-utils-$(V4L_UTILS_VERSION)"

archive-no-tag:
	@git archive --format=tar --prefix=v4l-utils-$(V4L_UTILS_VERSION)/ v4l-utils-$(V4L_UTILS_VERSION) > v4l-utils-$(V4L_UTILS_VERSION).tar
	@bzip2 -f v4l-utils-$(V4L_UTILS_VERSION).tar

archive: clean tag archive-no-tag

include Make.rules
