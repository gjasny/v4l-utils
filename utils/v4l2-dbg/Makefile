TARGETS = v4l2-dbg

all: $(TARGETS)

-include *.d

v4l2-dbg: v4l2-dbg.o v4l2-chipids.o
	$(CXX) $(LDFLAGS) -o $@ $^

sync-with-kernel:
	@if [ ! -f $(KERNEL_DIR)/include/media/v4l2-chip-ident.h ]; then \
	  echo "Error you must set KERNEL_DIR to point to an extracted kernel source dir"; \
	  exit 1; \
	fi
	@echo "struct chipid { const char *name; unsigned id; } chipids[] = {" > v4l2-chipids.cpp
	@grep V4L2_IDENT_ $(KERNEL_DIR)/include/media/v4l2-chip-ident.h | sed -e 's/.*V4L2_IDENT_\([0-9A-Z_]*\)[^=]*=[^0-9]*\([0-9]*\).*/{ "\1", \2 },/' | tr A-Z a-z >> v4l2-chipids.cpp
	@echo "{ 0, 0 }};" >> v4l2-chipids.cpp

install: $(TARGETS)
	mkdir -p $(DESTDIR)$(PREFIX)/sbin
	install -m 755 $(TARGETS) $(DESTDIR)$(PREFIX)/sbin

include ../../Make.rules
