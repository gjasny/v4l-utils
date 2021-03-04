bin_PROGRAMS = ir-keytable
man_MANS = ir-keytable.1 rc_keymap.5
sysconf_DATA = rc_maps.cfg
keytablesystem_DATA = $(srcdir)/rc_keymaps/*
udevrules_DATA = 70-infrared.rules
if WITH_BPF
if HAVE_SYSTEMD
if HAVE_UDEVDSYSCALLFILTER
systemdsystemunit_DATA = 50-rc_keymap.conf
endif
endif
endif

ir_keytable_SOURCES = keytable.c parse.h ir-encode.c ir-encode.h toml.c toml.h keymap.c keymap.h

if WITH_BPF
ir_keytable_SOURCES += bpf_load.c bpf_load.h
endif

ir_keytable_LDADD = @LIBINTL@
ir_keytable_LDFLAGS = $(ARGP_LIBS)

if WITH_BPF
ir_keytable_LDFLAGS += $(LIBELF_LIBS) $(LIBBPF_LIBS)
ir_keytable_CFLAGS = $(LIBBPF_CFLAGS)
SUBDIRS = bpf_protocols
endif

EXTRA_DIST = 70-infrared.rules 50-rc_keymap.conf rc_keymaps rc_keymaps_userspace gen_input_events.pl gen_keytables.pl ir-keytable.1 rc_maps.cfg rc_keymap.5

check:
	@$(foreach keymap,$(wildcard $(keytablesystem_DATA)),./ir-keytable --test-keymap=$(keymap);)

# custom target
install-data-local:
	$(install_sh) -d "$(DESTDIR)$(keytableuserdir)"
