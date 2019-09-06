bin_PROGRAMS = ir-ctl
man_MANS = ir-ctl.1

ir_ctl_SOURCES = ir-ctl.c ir-encode.c ir-encode.h toml.c toml.h keymap.c keymap.h bpf_encoder.c bpf_encoder.h
ir_ctl_LDADD = @LIBINTL@
ir_ctl_LDFLAGS = $(ARGP_LIBS)
