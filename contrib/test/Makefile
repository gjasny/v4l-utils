# Makefile for linuxtv.org v4l2-apps/test

CPPFLAGS += -I../include

binaries = ioctl-test 		\
	   sliced-vbi-test 	\
	   sliced-vbi-detect 	\
	   vbi-test 		\
	   v4lgrab		\
	   driver-test		\
	   pixfmt-test		\
	   stress-buffer	\
	   capture_example

.PHONY: all clean install

all: $(binaries)

clean::
	rm -f $(binaries)

install:

../lib/libv4l2.a: ../lib/v4l2_driver.c ../lib/frequencies.c
	make -C ../lib libv4l2.a

driver-test: driver-test.o ../lib/libv4l2.a

pixfmt-test: pixfmt-test.o
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@ -lX11

include ../Make.rules
