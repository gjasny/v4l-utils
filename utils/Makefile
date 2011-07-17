SUBDIRS=libv4l2util libmedia_dev decode_tm6000 keytable rds v4l2-compliance v4l2-ctl v4l2-dbg v4l2-sysfs-path xc3028-firmware

all install: 
	@for i in $(SUBDIRS); do \
		$(MAKE) -C $$i $@ || exit 1; \
	done

	# Test whether qmake is installed, and whether it is for qt4.
	@if which qmake-qt4 >/dev/null 2>&1; then \
		QMAKE=qmake-qt4; \
	else \
		QMAKE=qmake; \
	fi; \
	if which $$QMAKE >/dev/null 2>&1; then \
		if $$QMAKE --version 2>&1 | grep '4\.[0-9][0-9]*\.[0-9][0-9]*' >/dev/null; then \
			if [ ! -f qv4l2/Makefile ]; then \
				(cd qv4l2 && $$QMAKE) || exit 1; \
			fi; \
			$(MAKE) -C qv4l2 -f Makefile.install $@; \
		fi \
	fi

sync-with-kernel:
	$(MAKE) -C keytable $@
	$(MAKE) -C v4l2-dbg $@
	$(MAKE) -C xc3028-firmware $@

clean::
	@for i in $(SUBDIRS); do \
		$(MAKE) -C $$i $@; \
	done
	$(MAKE) -C v4l2-sysfs-path $@
	if [ -f qv4l2/Makefile ]; then $(MAKE) -C qv4l2 $@; fi
	rm -f qv4l2/Makefile qv4l2/qv4l2

include ../Make.rules
