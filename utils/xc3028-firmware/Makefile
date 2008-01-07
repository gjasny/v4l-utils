# Makefile for linuxtv.org v4l2-apps/util/xc3028-firmware

CPPFLAGS += -I../../../linux/include

binaries = firmware-tool

.PHONY: all clean install qv4l2

all: $(binaries)

clean::
	rm -f $(binaries)

firmware-tool: firmware-tool.o standards.o
	$(CC) $^ -o $@

install:

include ../../Make.rules
