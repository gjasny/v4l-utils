noinst_LTLIBRARIES = libcecutil.la

libcecutil_la_SOURCES = cec-info.cpp cec-log.cpp cec-parse.cpp cec-info.h cec-log.h cec-parse.h \
			cec-htng.h cec-htng-funcs.h
libcecutil_la_CPPFLAGS = -static -I$(top_srcdir)/utils/common
libcecutil_la_LDFLAGS = -static

CEC_GEN_PREREQS = $(top_srcdir)/include/linux/cec.h $(top_srcdir)/utils/libcecutil/cec-htng.h \
		$(top_srcdir)/include/linux/cec-funcs.h $(top_srcdir)/utils/libcecutil/cec-htng-funcs.h

cec-info.cpp: cec-msgs-gen.h

cec-log.cpp: cec-log-gen.h

cec-parse.cpp: cec-parse-gen.h cec-parse-src-gen.h

cec-msgs-gen.h cec-log-gen.h cec-parse-src-gen.h cec-parse-gen.h: cec-gen.pl $(CEC_GEN_PREREQS)
	$(top_srcdir)/utils/libcecutil/cec-gen.pl $@ $(CEC_GEN_PREREQS) >$@ || rm $@

clean-local:
	-rm -vf cec-log-gen.h cec-parse-gen.h cec-parse-src-gen.h cec-table-gen.h

EXTRA_DIST = TODO
