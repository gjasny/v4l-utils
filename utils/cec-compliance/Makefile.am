bin_PROGRAMS = cec-compliance

cec_compliance_SOURCES = cec-compliance.cpp cec-test.cpp cec-test-adapter.cpp cec-test-audio.cpp cec-test-power.cpp

cec-compliance.cpp: cec-table.h

cec-table.h: ../cec-ctl/msg2ctl.pl ../../include/linux/cec.h ../../include/linux/cec-funcs.h
	../cec-ctl/msg2ctl.pl 2 ../../include/linux/cec.h ../../include/linux/cec-funcs.h >$@

clean-local:
	-rm -vf cec-table.h
