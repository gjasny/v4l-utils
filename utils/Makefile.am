SUBDIRS = \
	libv4l2util \
	libmedia_dev \
	decode_tm6000 \
	ivtv-ctl \
	ir-ctl \
	cx18-ctl \
	keytable \
	media-ctl \
	v4l2-compliance \
	v4l2-ctl \
	v4l2-dbg \
	v4l2-sysfs-path \
	cec-ctl \
	cec-compliance \
	cec-follower \
	rds-ctl

if WITH_LIBDVBV5
SUBDIRS += \
	dvb
endif

if LINUX_OS
SUBDIRS += \
	xc3028-firmware

if HAVE_LINUX_I2C_DEV
SUBDIRS += \
	rds
endif
endif

if WITH_QV4L2
SUBDIRS += qv4l2
endif

EXTRA_DIST = common
