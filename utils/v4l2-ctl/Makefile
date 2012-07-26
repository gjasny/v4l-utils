override CPPFLAGS += -DV4L_UTILS_VERSION=\"$(V4L_UTILS_VERSION)\"

TARGETS = cx18-ctl ivtv-ctl v4l2-ctl

all: $(TARGETS)

-include *.d

cx18-ctl: cx18-ctl.o
	$(CC) $(LDFLAGS) -o $@ $^

ivtv-ctl: ivtv-ctl.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

v4l2-ctl: v4l2-ctl.o v4l2-ctl-common.o v4l2-ctl-tuner.o v4l2-ctl-io.o v4l2-ctl-stds.o v4l2-ctl-vidcap.o v4l2-ctl-vidout.o v4l2-ctl-overlay.o v4l2-ctl-vbi.o v4l2-ctl-selection.o v4l2-ctl-misc.o v4l2-ctl-streaming.o
	$(CXX) $(LDFLAGS) -o $@ $^ -lv4l2 -lv4lconvert -lrt

install: $(TARGETS)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGETS) $(DESTDIR)$(PREFIX)/bin

include ../../Make.rules
