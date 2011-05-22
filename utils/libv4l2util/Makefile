OBJS    = frequencies.o v4l2_driver.o get_media_devices.o
TARGETS = libv4l2util.a

all: $(TARGETS)

-include $(OBJS:.o=.d)

libv4l2util.a: $(OBJS)

install: $(TARGETS)

include ../../Make.rules
