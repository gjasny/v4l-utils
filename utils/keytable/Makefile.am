bin_PROGRAMS = ir-keytable
man_MANS = ir-keytable.1 rc_keymap.5
sysconf_DATA = rc_maps.cfg
keytablesystem_DATA = $(srcdir)/rc_keymaps/*
udevrules_DATA = 70-infrared.rules

ir_keytable_SOURCES = keytable.c parse.h ir-encode.c ir-encode.h toml.c toml.h keymap.c keymap.h

if WITH_BPF
ir_keytable_SOURCES += bpf.c bpf_load.c bpf.h bpf_load.h
endif

ir_keytable_LDADD = @LIBINTL@
ir_keytable_LDFLAGS = $(ARGP_LIBS)

if WITH_BPF
ir_keytable_LDFLAGS += $(LIBELF_LIBS)
SUBDIRS = bpf_protocols
endif

EXTRA_DIST = 70-infrared.rules rc_keymaps rc_keymaps_userspace gen_input_events.pl gen_keytables.pl ir-keytable.1 rc_maps.cfg rc_keymap.5

check:
	@$(foreach keymap,$(wildcard $(keytablesystem_DATA)),./ir-keytable --test-keymap=$(keymap);)

# custom target
install-data-local:
	$(install_sh) -d "$(DESTDIR)$(keytableuserdir)"

sync-with-kernel:
	@if [ ! -f $(KERNEL_DIR)/usr/include/linux/input-event-codes.h ]; then \
	  echo "Error you must set KERNEL_DIR to point to an extracted kernel source dir"; \
	  exit 1; \
	fi
	@echo generating $(srcdir)/parse.h
	@./gen_input_events.pl < $(KERNEL_DIR)/usr/include/linux/input-event-codes.h  > $(srcdir)/parse.h
	@-mkdir -p $(srcdir)/rc_keymaps
	@-rm $(srcdir)/rc_keymaps/*
	@echo storing existing keymaps at $(srcdir)/rc_keymaps/
	@cd $(srcdir) && ./gen_keytables.pl $(KERNEL_DIR);
	@-cp $(srcdir)/rc_keymaps_userspace/* $(srcdir)/rc_keymaps/
