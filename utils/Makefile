# Makefile for linuxtv.org v4l2-apps/util

CPPFLAGS += -I../../linux/include

binaries = v4l2-ctl

.PHONY: all clean install qv4l2

all: $(binaries) qv4l2 keytable

clean::
	rm -f $(binaries)
	-if [ -f qv4l2/Makefile ]; then make -C qv4l2 $@; fi
	-rm -f qv4l2/qv4l2 qv4l2/Makefile

qv4l2:
	if [ ! -f qv4l2/Makefile ]; then (cd qv4l2; qmake); fi
	make -C qv4l2

install:

include ../Make.rules
