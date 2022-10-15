AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4
AM_MAKEFLAGS = $(word 1, $(subst 1, -w, $(filter 1, $(V))) --no-print-directory)

SUBDIRS = lib

if USE_NLS
SUBDIRS += v4l-utils-po libdvbv5-po
endif

if WITH_V4LUTILS
SUBDIRS += utils contrib
endif

EXTRA_DIST = android-config.h sync-with-kernel.sh bootstrap.sh \
	doxygen_libdvbv5.cfg include COPYING.libv4l \
	COPYING.libdvbv5 README.libv4l README.lib-multi-threading \
	TODO.libdvbv5 doc/libdvbv5-index.doc

include $(top_srcdir)/aminclude.am
