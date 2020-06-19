AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = v4l-utils-po libdvbv5-po lib

if WITH_V4LUTILS
SUBDIRS += utils contrib
endif

EXTRA_DIST = android-config.h sync-with-kernel.sh bootstrap.sh \
	doxygen_libdvbv5.cfg include COPYING.libv4l \
	COPYING.libdvbv5 README.libv4l README.lib-multi-threading \
	TODO.libdvbv5 doc/libdvbv5-index.doc

include $(top_srcdir)/aminclude.am

