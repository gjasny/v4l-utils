TARGETS = ir-keytable

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
	@-rm rc_keymaps/*
	@echo storing existing keymaps at rc_keymaps/
	@./gen_keytables.pl $(KERNEL_DIR);

install: $(TARGETS)
	mkdir -p $(DESTDIR)$(PREFIX)/bin $(DESTDIR)/etc
	install -m 755 $(TARGETS) $(DESTDIR)$(PREFIX)/bin
	install -m 644 -p rc_maps.cfg $(DESTDIR)/etc
	install -m 755 -d $(DESTDIR)/etc/rc_keymaps
	install -m 644 -p rc_keymaps/* $(DESTDIR)/etc/rc_keymaps
	install -m 755 -d $(DESTDIR)/lib/udev/rules.d
	install -m 644 -p 70-infrared.rules $(DESTDIR)/lib/udev/rules.d
	install -m 755 -d $(DESTDIR)$(MANDIR)/man1
	install -m 644 -p ir-keytable.1 $(DESTDIR)$(MANDIR)/man1

include ../../Make.rules
