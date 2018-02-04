bin_PROGRAMS = cec-follower
man_MANS = cec-follower.1

cec_follower_SOURCES = cec-follower.cpp cec-follower.h cec-processing.cpp cec-log.cpp cec-info.cpp
cec_follower_CPPFLAGS = -I$(top_srcdir)/utils/common
cec_follower_LDFLAGS = -lrt

cec-log.cpp: cec-log.h

cec-log.h: $(top_srcdir)/utils/cec-ctl/msg2ctl.pl $(top_srcdir)/include/linux/cec.h \
		$(top_srcdir)/utils/common/cec-htng.h $(top_srcdir)/include/linux/cec-funcs.h \
		$(top_srcdir)/utils/common/cec-htng-funcs.h
	$(top_srcdir)/utils/cec-ctl/msg2ctl.pl 1 $(top_srcdir)/include/linux/cec.h \
		$(top_srcdir)/utils/common/cec-htng.h $(top_srcdir)/include/linux/cec-funcs.h \
		$(top_srcdir)/utils/common/cec-htng-funcs.h >$@

cec-follower.cpp: cec-table.h version.h

cec-table.h: $(top_srcdir)/utils/cec-ctl/msg2ctl.pl $(top_srcdir)/include/linux/cec.h \
		$(top_srcdir)/utils/common/cec-htng.h $(top_srcdir)/include/linux/cec-funcs.h \
		$(top_srcdir)/utils/common/cec-htng-funcs.h
	$(top_srcdir)/utils/cec-ctl/msg2ctl.pl 2 $(top_srcdir)/include/linux/cec.h \
		$(top_srcdir)/utils/common/cec-htng.h $(top_srcdir)/include/linux/cec-funcs.h \
		$(top_srcdir)/utils/common/cec-htng-funcs.h >$@

version.h:
	@if git -C $(srcdir) rev-parse HEAD >/dev/null 2>&1; then \
		echo -n "#define SHA " >$@ ; \
		git -C $(srcdir) rev-parse HEAD >>$@ ; \
	else \
		touch $@ ; \
	fi

clean-local:
	-rm -vf cec-log.h cec-table.h version.h

EXTRA_DIST = cec-follower.1
