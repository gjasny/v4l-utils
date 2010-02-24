all install:
	$(MAKE) -C libv4lconvert $@
	$(MAKE) -C libv4l2 $@
	$(MAKE) -C libv4l1 $@

clean::
	rm -f include/*~
	$(MAKE) -C libv4lconvert $@
	$(MAKE) -C libv4l2 $@
	$(MAKE) -C libv4l1 $@

include ../Make.rules
