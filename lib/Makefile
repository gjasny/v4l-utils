LIB_RELEASE=0
V4L2_LIB_VERSION=$(LIB_RELEASE).5.4

all clean install:
	$(MAKE) -C libv4lconvert V4L2_LIB_VERSION=$(V4L2_LIB_VERSION) $@
	$(MAKE) -C libv4l2 V4L2_LIB_VERSION=$(V4L2_LIB_VERSION) $@
	$(MAKE) -C libv4l1 V4L2_LIB_VERSION=$(V4L2_LIB_VERSION) $@

export: clean
	mkdir /tmp/libv4l-$(V4L2_LIB_VERSION)
	cp -a . /tmp/libv4l-$(V4L2_LIB_VERSION)/
	cd /tmp/ && \
		tar cvf /tmp/libv4l-$(V4L2_LIB_VERSION).tar\
		libv4l-$(V4L2_LIB_VERSION)
	gzip /tmp/libv4l-$(V4L2_LIB_VERSION).tar
	rm -rf /tmp/libv4l-$(V4L2_LIB_VERSION)
