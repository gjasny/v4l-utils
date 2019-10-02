bin_PROGRAMS = cec-compliance
man_MANS = cec-compliance.1

cec_compliance_SOURCES = cec-compliance.cpp cec-compliance.h cec-test.cpp cec-test-adapter.cpp cec-test-audio.cpp cec-test-power.cpp cec-test-fuzzing.cpp
cec_compliance_CPPFLAGS = -I$(top_srcdir)/utils/libcecutil -I$(top_srcdir)/utils/common
cec_compliance_LDFLAGS = -lrt ../libcecutil/libcecutil.la

cec-compliance.cpp: version.h

version.h:
	@if [ -d $(top_srcdir)/.git ]; then \
		echo -n "#define SHA " >$@ ; \
		git -C $(top_srcdir) rev-parse HEAD >>$@ ; \
	else \
		touch $@ ; \
	fi

clean-local:
	-rm -vf version.h

EXTRA_DIST = cec-compliance.1
