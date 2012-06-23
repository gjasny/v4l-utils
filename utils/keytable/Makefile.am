rootetcdir="/etc"

bin_PROGRAMS = ir-keytable
man_MANS = ir-keytable.1
nobase_rootetc_DATA = rc_maps.cfg $(srcdir)/rc_keymaps/*
udevrules_DATA = 70-infrared.rules

ir_keytable_SOURCES = keytable.c parse.h
ir_keytable_LDFLAGS = $(ARGP_LIBS)

EXTRA_DIST = 70-infrared.rules rc_keymaps gen_keytables.pl ir-keytable.1 rc_maps.cfg

# custom target

sync-with-kernel:
	@if [ ! -f $(KERNEL_DIR)/include/linux/input.h ]; then \
	  echo "Error you must set KERNEL_DIR to point to an extracted kernel source dir"; \
	  exit 1; \
	fi
	@echo generating $(srcdir)/parse.h
	@printf "struct parse_key {\n\tchar *name;\n\tunsigned int value;\n} " > $(srcdir)/parse.h
	@printf "keynames[] = {\n" >> $(srcdir)/parse.h

	@more $(KERNEL_DIR)/include/linux/input.h | perl -n \
	-e 'if (m/^\#define\s+(KEY_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) ' \
	-e '{ printf "\t{\"%s\", %s},\n",$$1,$$2; }' \
	-e 'if (m/^\#define\s+(BTN_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) ' \
	-e '{ printf "\t{\"%s\", %s},\n",$$1,$$2; }' \
	>> $(srcdir)/parse.h
	@printf "\t{ NULL, 0}\n};\n" >> $(srcdir)/parse.h

	@-mkdir -p $(srcdir)/rc_keymaps
	@-rm $(srcdir)/rc_keymaps/*
	@echo storing existing keymaps at $(srcdir)/rc_keymaps/
	@cd $(srcdir) && ./gen_keytables.pl $(KERNEL_DIR);
