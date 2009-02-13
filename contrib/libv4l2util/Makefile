# Makefile for linuxtv.org v4l2-apps/lib

CPPFLAGS += -I../include -I..

includes = libv4l2util.h

objects  = frequencies.o v4l2_driver.o

sharedlib = libv4l2util.so
staticlib = libv4l2util.a

ifeq ($(prefix),)
prefix = /usr
endif

.PHONY: all clean install

all: $(sharedlib) $(staticlib)

$(sharedlib): $(objects)

$(staticlib): $(objects)

clean::
	rm -f $(objects) $(sharedlib) $(staticlib)

install:
	mkdir -p $(prefix)/include
	cp $(includes) $(prefix)/include
	mkdir -p $(prefix)/lib
	cp $(sharedlib) $(staticlib) $(prefix)/lib

include ../Make.rules
