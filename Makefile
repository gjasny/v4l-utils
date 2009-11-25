# Makefile for linuxtv.org v4l2-apps

.PHONY: all distclean clean install

all:: prepare-includes

all clean install::
	$(MAKE) -C libv4l $@
	$(MAKE) -C libv4l2util $@
	$(MAKE) -C util $@
	$(MAKE) -C test $@

%:
	make -C .. $(MAKECMDGOALS)

distclean:: clean
	$(MAKE) -C util $@
	-$(RM) -rf include

prepare-includes:
	-if [ ! -d include ]; then \
		cp -r ../linux/include include ; \
		../v4l/scripts/headers_convert.pl `find include -type f` ; \
	fi
