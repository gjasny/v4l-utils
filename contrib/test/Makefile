# Makefile for linuxtv.org v4l2-apps/test

CPPFLAGS += -I../../linux/include

binaries = ioctl-test 		\
	   sliced-vbi-test 	\
	   sliced-vbi-detect 	\
	   vbi-test 		\
	   v4lgrab

.PHONY: all clean install

all: $(binaries)

clean::
	rm -f $(binaries)

install:

include ../Make.rules
