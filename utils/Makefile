# Makefile for linuxtv.org v4l2-apps/util

CPPFLAGS += -I../../linux/include

binaries = v4l2-ctl

.PHONY: all clean install qv4l2

all: $(binaries) qv4l2 keytable

clean::
	rm -f $(binaries)
	-if [ -f qv4l2/Makefile ]; then make -C qv4l2 $@; fi
	-rm -f qv4l2/qv4l2 qv4l2/Makefile

qv4l2:
	if [ ! -f qv4l2/Makefile ]; then (cd qv4l2; qmake); fi
	make -C qv4l2

install:

include ../Make.rules

parse.h: /usr/include/linux/input.h
	@echo generating parse.h
	@echo -en "struct parse_key {\n\tchar *name;\n\tunsigned int value;\n} " >parse.h
	@echo -en "keynames[] = {\n" >>parse.h

	@more /usr/include/linux/input.h |perl -n \
	-e 'if (m/^\#define\s+(KEY_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) ' \
	-e '{ printf "\t{\"%s\", %s},\n",$$1,$$2; }' \
	-e 'if (m/^\#define\s+(BTN_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) ' \
	-e '{ printf "\t{\"%s\", %s},\n",$$1,$$2; }' \
	>> parse.h
	@echo -en "\t{ NULL, 0}\n};\n" >>parse.h

keytables:
	-mkdir keycodes
	./gen_keytables.pl ../../linux/drivers/media/common/ir-keymaps.c

keytable: keytable.c parse.h keytables
