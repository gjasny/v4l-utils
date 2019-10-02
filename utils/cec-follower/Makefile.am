bin_PROGRAMS = cec-follower
man_MANS = cec-follower.1

cec_follower_SOURCES = cec-follower.cpp cec-follower.h cec-processing.cpp cec-tuner.cpp
cec_follower_CPPFLAGS = -I$(top_srcdir)/utils/libcecutil -I$(top_srcdir)/utils/common
cec_follower_LDFLAGS = -lrt ../libcecutil/libcecutil.la

cec-follower.cpp: version.h

version.h:
	@if [ -d $(top_srcdir)/.git ]; then \
		echo -n "#define SHA " >$@ ; \
		git -C $(top_srcdir) rev-parse HEAD >>$@ ; \
	else \
		touch $@ ; \
	fi

clean-local:
	-rm -vf version.h

EXTRA_DIST = cec-follower.1
