LIB_RELEASE=0
V4L2_LIB_VERSION=$(LIB_RELEASE).3

all clean install:
	$(MAKE) -C libv4lconvert $@
	$(MAKE) -C libv4l2 $@
	$(MAKE) -C libv4l1 $@

export: clean
	mkdir /tmp/libv4l-$(V4L2_LIB_VERSION)
	cp -a . /tmp/libv4l-$(V4L2_LIB_VERSION)/
	cd /tmp/ && \
		tar cvf /tmp/libv4l-$(V4L2_LIB_VERSION).tar\
		libv4l-$(V4L2_LIB_VERSION)
	gzip /tmp/libv4l-$(V4L2_LIB_VERSION).tar
	rm -rf /tmp/libv4l-$(V4L2_LIB_VERSION)
