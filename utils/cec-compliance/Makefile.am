bin_PROGRAMS = cec-compliance
man_MANS = cec-compliance.1

cec_compliance_SOURCES = cec-compliance.cpp cec-compliance.h cec-test.cpp cec-test-adapter.cpp cec-test-audio.cpp cec-test-power.cpp
cec_compliance_LDFLAGS = -lrt

cec-compliance.cpp: cec-table.h

cec-table.h: ../cec-ctl/msg2ctl.pl ../../include/linux/cec.h ../../include/linux/cec-funcs.h
	../cec-ctl/msg2ctl.pl 2 ../../include/linux/cec.h ../../include/linux/cec-funcs.h >$@

cec-compliance.cpp: version.h

version.h:
	@if git show-ref -s HEAD >/dev/null 2>&1; then \
		echo -n "#define SHA " >$@ ; \
		git show-ref -s HEAD >>$@ ; \
	else \
		touch $@ ; \
	fi

clean-local:
	-rm -vf cec-table.h version.h

EXTRA_DIST = cec-compliance.1
