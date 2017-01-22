bin_PROGRAMS = cec-ctl
man_MANS = cec-ctl.1

cec_ctl_SOURCES = cec-ctl.cpp

cec-ctl.cpp: cec-ctl-gen.h

cec-ctl-gen.h: msg2ctl.pl ../../include/linux/cec.h ../../include/linux/cec-funcs.h
	./msg2ctl.pl 0 ../../include/linux/cec.h ../../include/linux/cec-funcs.h >$@

clean-local:
	-rm -vf cec-ctl-gen.h

EXTRA_DIST = cec-ctl.1 msg2ctl.pl
