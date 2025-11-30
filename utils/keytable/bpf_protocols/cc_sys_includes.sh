#!/bin/sh
# Get C compiler's default includes on this system, as the BPF toolchain
# generally doesn't see the Linux headers. This fixes "missing" files on some
# architectures/distros, such as asm/byteorder.h, asm/socket.h, asm/sockios.h,
# sys/cdefs.h etc.
#
# Use '-idirafter': Don't interfere with include mechanics except where the
# build would have failed anyways.
LC_ALL=C "$@" -v -E - </dev/null 2>&1 \
	| sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }'
