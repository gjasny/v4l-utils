if LINUX_OS
SUBDIRS = test xc3028-firmware

if HAVE_LINUX_I2C_DEV
SUBDIRS += \
	rds-saa6588
endif

else
SUBDIRS = freebsd
endif

if WITH_GCONV
SUBDIRS += gconv
endif

SUBDIRS += decode_tm6000


EXTRA_DIST = \
	cobalt-ctl \
	parsers \
	pci_traffic \
	v4l_rec.pl \
	lircd2toml.py
