# Makefile for linuxtv.org v4l2-apps/util

IR_FILES = "linux/drivers/media/common/ir-keymaps.c \
linux/drivers/media/dvb/dvb-usb/a800.c \
linux/drivers/media/dvb/dvb-usb/af9005-remote.c \
linux/drivers/media/dvb/dvb-usb/af9015.c \
linux/drivers/media/dvb/dvb-usb/af9015.h \
linux/drivers/media/dvb/dvb-usb/anysee.c \
linux/drivers/media/dvb/dvb-usb/cinergyT2-core.c \
linux/drivers/media/dvb/dvb-usb/cxusb.c \
linux/drivers/media/dvb/dvb-usb/dib0700_devices.c \
linux/drivers/media/dvb/dvb-usb/dibusb-common.c \
linux/drivers/media/dvb/dvb-usb/digitv.c \
linux/drivers/media/dvb/dvb-usb/dtt200u.c \
linux/drivers/media/dvb/dvb-usb/dvb-usb-remote.c \
linux/drivers/media/dvb/dvb-usb/dvb-usb.h \
linux/drivers/media/dvb/dvb-usb/dw2102.c \
linux/drivers/media/dvb/dvb-usb/m920x.c \
linux/drivers/media/dvb/dvb-usb/nova-t-usb2.c \
linux/drivers/media/dvb/dvb-usb/opera1.c \
linux/drivers/media/dvb/dvb-usb/vp702x.c \
linux/drivers/media/dvb/dvb-usb/vp7045.c "

ifeq ($(KERNEL_DIR),)
	KERNEL_DIR = /usr
endif

CPPFLAGS += -I../include -D_GNU_SOURCE
LDFLAGS += -lm

binaries = v4l2-ctl v4l2-dbg v4l2-compliance ivtv-ctl cx18-ctl v4l2-sysfs-path

ifeq ($(prefix),)
prefix = /usr
endif

.PHONY: all clean install qv4l2

all: $(binaries) qv4l2 keytable

clean::
	rm -f $(binaries) v4l2-chipids.cpp
	-if [ -f qv4l2/Makefile ]; then make -C qv4l2 $@; fi
	-rm -f qv4l2/qv4l2 qv4l2/Makefile
	make -C xc3028-firmware $@
	rm -f v4l2-chipids.cpp
	rm -rf keycodes parse.h keytable

qv4l2:
	-if [ ! -f qv4l2/Makefile ]; then (cd qv4l2; qmake); fi
	$(MAKE) -C qv4l2

v4l2-dbg: v4l2-dbg.o v4l2-chipids.o
	$(CXX) $^ -o $@

v4l2-dbg.o: v4l2-dbg.h v4l2-dbg-bttv.h v4l2-dbg-em28xx.h v4l2-dbg-saa7134.h

v4l2-sysfs-path.o: v4l2-sysfs-path.c ../libv4l2util/v4l2_driver.h

v4l2-sysfs-path: v4l2-sysfs-path.o ../libv4l2util/libv4l2util.a
	$(CC) $^ -o $@ -lsysfs

../libv4l2util/libv4l2util.a:
	$(MAKE) -C ../libv4l2util libv4l2util.a

install:
	mkdir -p $(prefix)/bin
	cp $(binaries) $(prefix)/bin

include ../Make.rules

parse.h: $(KERNEL_DIR)/include/linux/input.h
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

keytables:
	@-mkdir -p keycodes
	@echo storing existing keycodes at keycodes/
	@for i in `echo $(IR_FILES)`; do ./gen_keytables.pl ../../$$i; done

keytable: keytable.c parse.h keytables

v4l2-chipids.cpp: ../include/media/v4l2-chip-ident.h
	@echo "struct chipid { const char *name; unsigned id; } chipids[] = {" >$@
	@grep V4L2_IDENT_ $^ | sed -e 's/.*V4L2_IDENT_\([0-9A-Z_]*\)[^=]*=[^0-9]*\([0-9]*\).*/{ "\1", \2 },/' | tr A-Z a-z >>$@
	@echo "{ 0, 0 }};" >>$@


