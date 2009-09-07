LIB_RELEASE=0
V4L2_LIB_VERSION=$(LIB_RELEASE).6.2-test

all install:
	$(MAKE) -C libv4lconvert V4L2_LIB_VERSION=$(V4L2_LIB_VERSION) $@
	$(MAKE) -C libv4l2 V4L2_LIB_VERSION=$(V4L2_LIB_VERSION) $@
	$(MAKE) -C libv4l1 V4L2_LIB_VERSION=$(V4L2_LIB_VERSION) $@

clean:
	rm -f *~ include/*~ DEADJOE include/DEADJOE
	$(MAKE) -C libv4lconvert V4L2_LIB_VERSION=$(V4L2_LIB_VERSION) $@
	$(MAKE) -C libv4l2 V4L2_LIB_VERSION=$(V4L2_LIB_VERSION) $@
	$(MAKE) -C libv4l1 V4L2_LIB_VERSION=$(V4L2_LIB_VERSION) $@

export: clean
	tar --transform s/^\./libv4l-$(V4L2_LIB_VERSION)/g -zcvf \
		/tmp/libv4l-$(V4L2_LIB_VERSION).tar.gz .
