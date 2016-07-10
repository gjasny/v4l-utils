bin_PROGRAMS = cec-ctl

cec_ctl_SOURCES = cec-ctl.cpp

cec-ctl.cpp: cec-ctl-gen.h

cec-ctl-gen.h: msg2ctl.pl ../../include/linux/cec.h ../../include/linux/cec-funcs.h
	./msg2ctl.pl 0 ../../include/linux/cec.h ../../include/linux/cec-funcs.h >$@

clean-local:
	-rm -vf cec-ctl-gen.h
