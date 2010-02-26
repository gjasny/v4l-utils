OBJS    = frequencies.o v4l2_driver.o
TARGETS = libv4l2util.a

all: $(TARGETS)

-include $(OBJS:.o=.d)

libv4l2util.a: $(OBJS)

install:

include ../../Make.rules
