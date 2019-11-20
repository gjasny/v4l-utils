bin_PROGRAMS = cec-ctl
man_MANS = cec-ctl.1

cec_ctl_SOURCES = cec-ctl.cpp cec-pin.cpp cec-ctl.h
cec_ctl_CPPFLAGS = -I$(top_srcdir)/utils/libcecutil
cec_ctl_LDADD = -lrt ../libcecutil/libcecutil.la

EXTRA_DIST = cec-ctl.1
