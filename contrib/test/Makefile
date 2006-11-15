# Makefile for linuxtv.org v4l2-apps/test

CPPFLAGS += -I../../linux/include

binaries = ioctl-test 		\
	   sliced-vbi-test 	\
	   sliced-vbi-detect 	\
	   vbi-test 		\
	   v4lgrab		\
	   driver-test

.PHONY: all clean install

all: $(binaries)

clean::
	rm -f $(binaries)

install:

../lib/libv4l2.a: ../lib/v4l2_driver.c ../lib/frequencies.c
	make -C ../lib libv4l2.a

driver-test: driver-test.o ../lib/libv4l2.a

include ../Make.rules
