TARGETS = v4l-keytable

IR_FILES = "drivers/media/IR/ir-keymaps.c \
drivers/media/dvb/dvb-usb/a800.c \
drivers/media/dvb/dvb-usb/af9005-remote.c \
drivers/media/dvb/dvb-usb/af9015.c \
drivers/media/dvb/dvb-usb/af9015.h \
drivers/media/dvb/dvb-usb/anysee.c \
drivers/media/dvb/dvb-usb/cinergyT2-core.c \
drivers/media/dvb/dvb-usb/cxusb.c \
drivers/media/dvb/dvb-usb/dib0700_devices.c \
drivers/media/dvb/dvb-usb/dibusb-common.c \
drivers/media/dvb/dvb-usb/digitv.c \
drivers/media/dvb/dvb-usb/dtt200u.c \
drivers/media/dvb/dvb-usb/dvb-usb-remote.c \
drivers/media/dvb/dvb-usb/dvb-usb.h \
drivers/media/dvb/dvb-usb/dw2102.c \
drivers/media/dvb/dvb-usb/m920x.c \
drivers/media/dvb/dvb-usb/nova-t-usb2.c \
drivers/media/dvb/dvb-usb/opera1.c \
drivers/media/dvb/dvb-usb/vp702x.c \
drivers/media/dvb/dvb-usb/vp7045.c "

all: $(TARGETS)

-include *.d

v4l-keytable: keytable.o
	$(CC) $(LDFLAGS) -o $@ $^

sync-with-kernel:
	@if [ ! -f $(KERNEL_DIR)/include/linux/input.h ]; then \
	  echo "Error you must set KERNEL_DIR to point to an extracted kernel source dir"; \
	  exit 1; \
	fi
	@echo generating parse.h
	@printf "struct parse_key {\n\tchar *name;\n\tunsigned int value;\n} " >parse.h
	@printf "keynames[] = {\n" >>parse.h

	@more $(KERNEL_DIR)/include/linux/input.h |perl -n \
	-e 'if (m/^\#define\s+(KEY_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) ' \
	-e '{ printf "\t{\"%s\", %s},\n",$$1,$$2; }' \
	-e 'if (m/^\#define\s+(BTN_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) ' \
	-e '{ printf "\t{\"%s\", %s},\n",$$1,$$2; }' \
	>> parse.h
	@printf "\t{ NULL, 0}\n};\n" >>parse.h

	@-mkdir -p keycodes
	@echo storing existing keycodes at keycodes/
	@for i in `echo $(IR_FILES)`; do ./gen_keytables.pl $(KERNEL_DIR)/$$i; done

install:

include ../../Make.rules
