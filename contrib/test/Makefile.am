noinst_PROGRAMS = \
	ioctl-test		\
	sliced-vbi-test		\
	sliced-vbi-detect	\
	v4l2grab		\
	driver-test		\
	mc_nextgen_test		\
	stress-buffer		\
	capture-example

if HAVE_X11
noinst_PROGRAMS += pixfmt-test
endif

if HAVE_GLU
noinst_PROGRAMS += v4l2gl
endif

driver_test_SOURCES = driver-test.c
driver_test_LDADD = ../../utils/libv4l2util/libv4l2util.la

pixfmt_test_SOURCES = pixfmt-test.c
pixfmt_test_CFLAGS = $(X11_CFLAGS)
pixfmt_test_LDFLAGS = $(X11_LIBS)

v4l2grab_SOURCES = v4l2grab.c
v4l2grab_LDFLAGS = $(ARGP_LIBS)
v4l2grab_LDADD = ../../lib/libv4l2/libv4l2.la ../../lib/libv4lconvert/libv4lconvert.la -lpthread

v4l2gl_SOURCES = v4l2gl.c
v4l2gl_LDFLAGS = $(X11_LIBS) $(GL_LIBS) $(GLU_LIBS) $(ARGP_LIBS)
v4l2gl_LDADD = ../../lib/libv4l2/libv4l2.la ../../lib/libv4lconvert/libv4lconvert.la

mc_nextgen_test_CFLAGS = $(LIBUDEV_CFLAGS)
mc_nextgen_test_LDFLAGS = $(LIBUDEV_LIBS)


ioctl_test_SOURCES = ioctl-test.c ioctl-test.h ioctl_32.h ioctl_64.h

sliced_vbi_test_SOURCES = sliced-vbi-test.c

sliced_vbi_detect_SOURCES = sliced-vbi-detect.c

stress_buffer_SOURCES = stress-buffer.c

capture_example_SOURCES = capture-example.c

ioctl-test.c: ioctl-test.h

sync-with-kernel:
	./gen_ioctl_list.pl --gen_ioctl_numbers >.tmp_ioctl.c
	gcc -I ../../include/ .tmp_ioctl.c -o tmp_ioctl32 -m32
	gcc -I ../../include/ .tmp_ioctl.c -o tmp_ioctl64 -m64
	./tmp_ioctl32 32 >ioctl_32.h
	./tmp_ioctl64 64 >ioctl_64.h
	rm .tmp_ioctl.c tmp_ioctl32 tmp_ioctl64
	./gen_ioctl_list.pl >ioctl-test.h

EXTRA_DIST = \
	gen_ioctl_list.pl
