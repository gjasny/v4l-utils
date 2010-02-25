TARGETS = v4l2-compliance

all: $(TARGETS)

v4l2-compliance: v4l2-compliance.o
	$(CXX) $(LDFLAGS) -o $@ $^

include ../../Make.rules
