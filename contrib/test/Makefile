FILES   = ioctl-test sliced-vbi-test sliced-vbi-detect vbi-test v4lgrab v4l2-ctl
CC      = gcc
LIBS    =
CFLAGS  = -O3 -Wall -fomit-frame-pointer -funroll-loops -g -I ../linux/include
CXXFLAGS = $(CFLAGS)
LDFLAGS =

all: $(FILES) qv4l2

clean:
	-rm -f core core.[0123456789]* *~ *.o $(FILES)
	-if [ -f qv4l2/Makefile ]; then make -C qv4l2 $@; fi
	-rm -f qv4l2/qv4l2 qv4l2/Makefile

qv4l2:
	if [ ! -f qv4l2/Makefile ]; then (cd qv4l2; qmake); fi
	make -C qv4l2

.PHONY: qv4l2
