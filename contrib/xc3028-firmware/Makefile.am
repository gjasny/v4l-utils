noinst_PROGRAMS = firmware-tool

firmware_tool_SOURCES = firmware-tool.c standards.c extract_head.h standards.h tuner-xc2028-types.h

EXTRA_DIST = README

# custom targets

sync-with-kernel:
	@if [ ! -f $(KERNEL_DIR)/drivers/media/tuners/tuner-xc2028-types.h ]; then \
	  echo "Error you must set KERNEL_DIR to point to an extracted kernel source dir"; \
	  exit 1; \
	fi
	cp -a $(KERNEL_DIR)/drivers/media/tuners/tuner-xc2028-types.h $(srcdir)/
