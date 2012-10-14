SUBDIRS = \
	dvb \
	libv4l2util \
	libmedia_dev \
	decode_tm6000 \
	keytable \
	v4l2-compliance \
	v4l2-ctl \
	v4l2-dbg \
	v4l2-sysfs-path \
	rds-ctl

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

