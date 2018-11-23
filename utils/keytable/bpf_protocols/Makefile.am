# Get Clang's default includes on this system, as opposed to those seen by
# '-target bpf'. This fixes "missing" files on some architectures/distros,
# such as asm/byteorder.h, asm/socket.h, asm/sockios.h, sys/cdefs.h etc.
#
# Use '-idirafter': Don't interfere with include mechanics except where the
# build would have failed anyways.
CLANG_SYS_INCLUDES := $(shell $(CLANG) -v -E - </dev/null 2>&1 \
        | sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }')

%.o: %.c bpf_helpers.h
	$(CLANG) $(CLANG_SYS_INCLUDES) -D__linux__ -I$(top_srcdir)/include -target bpf -O2 -c $<

PROTOCOLS = grundig.o pulse_distance.o pulse_length.o rc_mm.o manchester.o

all: $(PROTOCOLS)

CLEANFILES = $(PROTOCOLS)
EXTRA_DIST = $(PROTOCOLS:%.o=%.c) bpf_helpers.h

# custom target
install-data-local:
	$(install_sh) -d "$(DESTDIR)$(keytableuserdir)/protocols"
	$(install_sh) -d "$(DESTDIR)$(keytablesystemdir)/protocols"
	$(install_sh) -m 0644 $(PROTOCOLS) "$(DESTDIR)$(keytablesystemdir)/protocols"
