# Makefile for linuxtv.org v4l2-apps

.PHONY: all clean install

all clean install:
	$(MAKE) -C lib $@
	$(MAKE) -C util $@
	$(MAKE) -C test $@

%:
	make -C .. $(MAKECMDGOALS)
