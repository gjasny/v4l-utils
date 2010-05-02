TARGETS = v4l2-compliance

all: $(TARGETS)

-include *.d

v4l2-compliance: v4l2-compliance.o v4l2-test-debug.o
	$(CXX) $(LDFLAGS) -o $@ $^

install: $(TARGETS)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGETS) $(DESTDIR)$(PREFIX)/bin

include ../../Make.rules
