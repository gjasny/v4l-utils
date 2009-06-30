# Makefile for linuxtv.org v4l2-apps/test

CPPFLAGS += -I../include

binaries = ioctl-test 		\
	   sliced-vbi-test 	\
	   sliced-vbi-detect 	\
	   vbi-test 		\
	   v4lgrab		\
	   v4l2grab		\
	   driver-test		\
	   pixfmt-test		\
	   stress-buffer	\
	   capture-example

.PHONY: all clean install

all: $(binaries)

clean::
	rm -f $(binaries)

install:

../libv4l2util/libv4l2util.a: ../libv4l2util/v4l2_driver.c ../libv4l2util/frequencies.c
	make -C ../libv4l2util libv4l2util.a

../libv4l/libv4l2/libv4l2.so:
	make -C ../libv4l/

driver-test: driver-test.o ../libv4l2util/libv4l2util.a

pixfmt-test: pixfmt-test.o
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@ -lX11

v4l2grab: v4l2grab.o
	$(CC) $(LDFLAGS) $^ -o $@ ../libv4l/libv4l2/libv4l2.so

include ../Make.rules
