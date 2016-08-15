bin_PROGRAMS = cec-follower

cec_follower_SOURCES = cec-follower.cpp cec-processing.cpp cec-log.cpp

cec-log.cpp: cec-log.h

cec-log.h: ../cec-ctl/msg2ctl.pl ../../include/linux/cec.h ../../include/linux/cec-funcs.h
	../cec-ctl/msg2ctl.pl 1 ../../include/linux/cec.h ../../include/linux/cec-funcs.h >$@

cec-follower.cpp: cec-table.h

cec-table.h: ../cec-ctl/msg2ctl.pl ../../include/linux/cec.h ../../include/linux/cec-funcs.h
	../cec-ctl/msg2ctl.pl 2 ../../include/linux/cec.h ../../include/linux/cec-funcs.h >$@

clean-local:
	-rm -vf cec-log.h
