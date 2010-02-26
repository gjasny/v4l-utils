TARGETS  = ioctl-test 		\
	   sliced-vbi-test 	\
	   sliced-vbi-detect 	\
	   vbi-test 		\
	   v4lgrab		\
	   v4l2grab		\
	   driver-test		\
	   pixfmt-test		\
	   stress-buffer	\
	   capture-example

all: $(TARGETS)

-include *.d

install:

../../utils/libv4l2util/libv4l2util.a: ../../utils/libv4l2util/v4l2_driver.c ../../utils/libv4l2util/frequencies.c
	make -C ../../utils/libv4l2util libv4l2util.a

../../lib/libv4l2/libv4l2.so:
	make -C ../../lib

driver-test: driver-test.o ../../utils/libv4l2util/libv4l2util.a
	$(CC) $(LDFLAGS) -o $@ $^

pixfmt-test: pixfmt-test.o
	$(CC) $(LDFLAGS) -o $@ $^ -lX11

v4l2grab: v4l2grab.o ../../lib/libv4l2/libv4l2.so
	$(CC) $(LDFLAGS) -o $@ $^ -L../../lib/libv4l2 -L../../lib/libv4lconvert -lv4l2 -lv4lconvert

ioctl-test: ioctl-test.o
	$(CC) $(LDFLAGS) -o $@ $^

sliced-vbi-test: sliced-vbi-test.o
	$(CC) $(LDFLAGS) -o $@ $^

sliced-vbi-detect: sliced-vbi-detect.o
	$(CC) $(LDFLAGS) -o $@ $^

vbi-test: vbi-test.o
	$(CC) $(LDFLAGS) -o $@ $^

v4lgrab: v4lgrab.o
	$(CC) $(LDFLAGS) -o $@ $^

stress-buffer: stress-buffer.o
	$(CC) $(LDFLAGS) -o $@ $^

capture-example: capture-example.o
	$(CC) $(LDFLAGS) -o $@ $^

include ../../Make.rules
