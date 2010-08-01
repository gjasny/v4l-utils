TARGETS = ir-keytable

IR_FILES = "drivers/media/dvb/dvb-usb/a800.c \
drivers/media/dvb/dvb-usb/af9005-remote.c \
drivers/media/dvb/dvb-usb/af9015.c \
drivers/media/dvb/dvb-usb/af9015.h \
drivers/media/dvb/dvb-usb/anysee.c \
drivers/media/dvb/dvb-usb/cinergyT2-core.c \
drivers/media/dvb/dvb-usb/cxusb.c \
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

ir-keytable: keytable.o
	$(CC) $(LDFLAGS) -o $@ $^

keytable.o: keytable.c parse.h

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

	@-mkdir -p rc_keymaps
	@echo storing existing keymaps at rc_keymaps/
	@for i in $(KERNEL_DIR)/drivers/media/IR/keymaps/*.c; do echo $$i;./gen_keytables.pl $$i; done
	@for i in `echo $(IR_FILES)`; do echo $$i;./gen_keytables.pl $(KERNEL_DIR)/$$i; done

install: $(TARGETS)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGETS) $(DESTDIR)$(PREFIX)/bin
	install -m 644 rc_maps.cfg.example /etc
	install -m 755 -d /etc/rc_keymaps
	install -m 644 rc_keymaps/* /etc/rc_keymaps

include ../../Make.rules
