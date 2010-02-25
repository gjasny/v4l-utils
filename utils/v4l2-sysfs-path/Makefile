TARGETS = v4l2-sysfs-path

all: $(TARGETS)

v4l2-sysfs-path: v4l2-sysfs-path.o ../libv4l2util/libv4l2util.a
	$(CC) $(LDFLAGS) -o $@ $^ -lsysfs

include ../../Make.rules
