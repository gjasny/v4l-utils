sbin_PROGRAMS = v4l2-dbg
v4l2_dbg_SOURCES = v4l2-dbg.cpp v4l2-chipids.cpp v4l2-dbg-ac97.h  v4l2-dbg-bttv.h  v4l2-dbg-em28xx.h  v4l2-dbg.h  v4l2-dbg-micron.h  v4l2-dbg-saa7134.h	v4l2-dbg-tvp5150.h

# custom targets

sync-with-kernel:
	@if [ ! -f $(KERNEL_DIR)/include/media/v4l2-chip-ident.h ]; then \
	  echo "Error you must set KERNEL_DIR to point to an extracted kernel source dir"; \
	  exit 1; \
	fi
	@echo "struct chipid { const char *name; unsigned id; } chipids[] = {" > $(srcdir)/v4l2-chipids.cpp
	@grep V4L2_IDENT_ $(KERNEL_DIR)/include/media/v4l2-chip-ident.h | sed -e 's/.*V4L2_IDENT_\([0-9A-Z_]*\)[^=]*=[^0-9]*\([0-9]*\).*/{ "\1", \2 },/' | tr A-Z a-z >> $(srcdir)/v4l2-chipids.cpp
	@echo "{ 0, 0 }};" >> $(srcdir)/v4l2-chipids.cpp
