TARGETS = firmware-tool

all: $(TARGETS)

-include *.d

firmware-tool: firmware-tool.o standards.o
	$(CC) $(LDFLAGS) -o $@ $^

sync-with-kernel:
	@if [ ! -f $(KERNEL_DIR)/drivers/media/common/tuners/tuner-xc2028-types.h ]; then \
	  echo "Error you must set KERNEL_DIR to point to an extracted kernel source dir"; \
	  exit 1; \
	fi
	cp -a $(KERNEL_DIR)/drivers/media/common/tuners/tuner-xc2028-types.h .

install:

include ../../Make.rules
