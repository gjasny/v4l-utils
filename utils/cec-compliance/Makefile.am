bin_PROGRAMS = cec-compliance
man_MANS = cec-compliance.1

cec_compliance_SOURCES = cec-compliance.cpp cec-compliance.h cec-test.cpp cec-test-adapter.cpp cec-test-audio.cpp cec-test-power.cpp cec-info.cpp
cec_compliance_CPPFLAGS = -I$(top_srcdir)/utils/common
cec_compliance_LDFLAGS = -lrt

cec-compliance.cpp: cec-table.h

cec-table.h: $(top_srcdir)/utils/cec-ctl/msg2ctl.pl $(top_srcdir)/include/linux/cec.h \
		$(top_srcdir)/utils/common/cec-htng.h $(top_srcdir)/include/linux/cec-funcs.h \
		$(top_srcdir)/utils/common/cec-htng-funcs.h
	$(top_srcdir)/utils/cec-ctl/msg2ctl.pl 2 $(top_srcdir)/include/linux/cec.h \
		$(top_srcdir)/utils/common/cec-htng.h $(top_srcdir)/include/linux/cec-funcs.h \
		$(top_srcdir)/utils/common/cec-htng-funcs.h >$@

cec-compliance.cpp: version.h

version.h:
	@if git -C $(srcdir) rev-parse HEAD >/dev/null 2>&1; then \
		echo -n "#define SHA " >$@ ; \
		git -C $(srcdir) rev-parse HEAD >>$@ ; \
	else \
		touch $@ ; \
	fi

clean-local:
	-rm -vf cec-table.h version.h

EXTRA_DIST = cec-compliance.1
