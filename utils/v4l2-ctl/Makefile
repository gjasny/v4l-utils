override CPPFLAGS += -DV4L_UTILS_VERSION=\"$(V4L_UTILS_VERSION)\"

TARGETS = cx18-ctl ivtv-ctl v4l2-ctl

all: $(TARGETS)

-include *.d

cx18-ctl: cx18-ctl.o
	$(CC) $(LDFLAGS) -o $@ $^

ivtv-ctl: ivtv-ctl.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

v4l2-ctl: v4l2-ctl.o
	$(CXX) $(LDFLAGS) -o $@ $^ -lv4l2 -lv4lconvert -lrt

install: $(TARGETS)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGETS) $(DESTDIR)$(PREFIX)/bin

include ../../Make.rules
