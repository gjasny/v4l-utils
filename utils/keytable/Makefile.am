bin_PROGRAMS = ir-keytable
man_MANS = ir-keytable.1
sysconf_DATA = rc_maps.cfg
keytablesystem_DATA = $(srcdir)/rc_keymaps/*
udevrules_DATA = 70-infrared.rules

ir_keytable_SOURCES = keytable.c parse.h
ir_keytable_LDADD = @LIBINTL@
ir_keytable_LDFLAGS = $(ARGP_LIBS)

EXTRA_DIST = 70-infrared.rules rc_keymaps rc_keymaps_userspace gen_keytables.pl ir-keytable.1 rc_maps.cfg

# custom target
install-data-local:
	$(install_sh) -d "$(DESTDIR)$(keytableuserdir)"

sync-with-kernel:
	@if [ ! -f $(KERNEL_DIR)/usr/include/linux/input-event-codes.h ]; then \
	  echo "Error you must set KERNEL_DIR to point to an extracted kernel source dir"; \
	  exit 1; \
	fi
	@echo generating $(srcdir)/parse.h
	@printf "struct parse_event {\n\tchar *name;\n\tunsigned int value;\n};\n" > $(srcdir)/parse.h

	@printf "struct parse_event events_type[] = {\n" >> $(srcdir)/parse.h
	@more $(KERNEL_DIR)/usr/include/linux/input-event-codes.h | perl -n \
	-e 'if (m/^\#define\s+(EV_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) ' \
	-e '{ printf "\t{\"%s\", %s},\n",$$1,$$2 if ($$1 ne "EV_VERSION"); }' \
	>> $(srcdir)/parse.h
	@printf "\t{ NULL, 0}\n};\n" >> $(srcdir)/parse.h

	@printf "struct parse_event msc_events[] = {\n" >> $(srcdir)/parse.h
	@more $(KERNEL_DIR)/usr/include/linux/input-event-codes.h | perl -n \
	-e 'if (m/^\#define\s+(MSC_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) ' \
	-e '{ printf "\t{\"%s\", %s},\n",$$1,$$2; }' \
	>> $(srcdir)/parse.h
	@printf "\t{ NULL, 0}\n};\n" >> $(srcdir)/parse.h

	@printf "struct parse_event key_events[] = {\n" >> $(srcdir)/parse.h
	@more $(KERNEL_DIR)/usr/include/linux/input-event-codes.h | perl -n \
	-e 'if (m/^\#define\s+(KEY_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) ' \
	-e '{ printf "\t{\"%s\", %s},\n",$$1,$$2; }' \
	-e 'if (m/^\#define\s+(BTN_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) ' \
	-e '{ printf "\t{\"%s\", %s},\n",$$1,$$2; }' \
	>> $(srcdir)/parse.h
	@printf "\t{ NULL, 0}\n};\n" >> $(srcdir)/parse.h

	@printf "struct parse_event rel_events[] = {\n" >> $(srcdir)/parse.h
	@more $(KERNEL_DIR)/usr/include/linux/input-event-codes.h | perl -n \
	-e 'if (m/^\#define\s+(REL_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) ' \
	-e '{ printf "\t{\"%s\", %s},\n",$$1,$$2; }' \
	>> $(srcdir)/parse.h
	@printf "\t{ NULL, 0}\n};\n" >> $(srcdir)/parse.h

	@printf "struct parse_event abs_events[] = {\n" >> $(srcdir)/parse.h
	@more $(KERNEL_DIR)/usr/include/linux/input-event-codes.h | perl -n \
	-e 'if (m/^\#define\s+(ABS_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) ' \
	-e '{ printf "\t{\"%s\", %s},\n",$$1,$$2; }' \
	>> $(srcdir)/parse.h
	@printf "\t{ NULL, 0}\n};\n" >> $(srcdir)/parse.h

	@-mkdir -p $(srcdir)/rc_keymaps
	@-rm $(srcdir)/rc_keymaps/*
	@-cp $(srcdir)/rc_keymaps_userspace/* $(srcdir)/rc_keymaps/
	@echo storing existing keymaps at $(srcdir)/rc_keymaps/
	@cd $(srcdir) && ./gen_keytables.pl $(KERNEL_DIR);
