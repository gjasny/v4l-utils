bin_PROGRAMS = cec-ctl
man_MANS = cec-ctl.1

cec_ctl_SOURCES = cec-ctl.cpp cec-pin.cpp cec-ctl.h cec-info.cpp
cec_ctl_CPPFLAGS = -I$(top_srcdir)/utils/common
cec_ctl_LDFLAGS = -lrt

cec-ctl.cpp: cec-ctl-gen.h

cec-pin.cpp: cec-pin-gen.h

cec-ctl-gen.h: msg2ctl.pl $(top_srcdir)/include/linux/cec.h $(top_srcdir)/include/linux/cec-funcs.h \
		$(top_srcdir)/utils/common/cec-htng.h $(top_srcdir)/utils/common/cec-htng-funcs.h
	$(top_srcdir)/utils/cec-ctl/msg2ctl.pl 0 $(top_srcdir)/include/linux/cec.h \
		$(top_srcdir)/utils/common/cec-htng.h $(top_srcdir)/include/linux/cec-funcs.h \
		$(top_srcdir)/utils/common/cec-htng-funcs.h >$@

cec-pin-gen.h: msg2ctl.pl $(top_srcdir)/include/linux/cec.h $(top_srcdir)/include/linux/cec-funcs.h \
		$(top_srcdir)/utils/common/cec-htng.h $(top_srcdir)/utils/common/cec-htng-funcs.h
	$(top_srcdir)/utils/cec-ctl/msg2ctl.pl 2 $(top_srcdir)/include/linux/cec.h \
		$(top_srcdir)/utils/common/cec-htng.h $(top_srcdir)/include/linux/cec-funcs.h \
		$(top_srcdir)/utils/common/cec-htng-funcs.h >$@

clean-local:
	-rm -vf cec-ctl-gen.h cec-pin-gen.h

EXTRA_DIST = cec-ctl.1 msg2ctl.pl
