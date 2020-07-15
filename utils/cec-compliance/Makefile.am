bin_PROGRAMS = cec-compliance
man_MANS = cec-compliance.1

cec_compliance_SOURCES = cec-compliance.cpp cec-compliance.h cec-test.cpp cec-test-adapter.cpp cec-test-audio.cpp cec-test-power.cpp cec-test-fuzzing.cpp
cec_compliance_CPPFLAGS = -I$(top_srcdir)/utils/libcecutil $(GIT_SHA) $(GIT_COMMIT_CNT)
cec_compliance_LDADD = -lrt ../libcecutil/libcecutil.la

EXTRA_DIST = cec-compliance.1
