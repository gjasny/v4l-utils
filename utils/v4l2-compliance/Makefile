TARGETS = v4l2-compliance

all: $(TARGETS)

-include *.d

v4l2-compliance: v4l2-compliance.o v4l2-test-debug.o v4l2-test-input-output.o \
	v4l2-test-controls.o
	$(CXX) $(LDFLAGS) -lv4l2 -o $@ $^

install: $(TARGETS)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGETS) $(DESTDIR)$(PREFIX)/bin

include ../../Make.rules
