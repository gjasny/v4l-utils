#!/bin/bash

TOPSRCDIR="$( cd "$( dirname "$0" )" && pwd )"

KERNEL_DIR=$1

if [ -z "${KERNEL_DIR}" -o \
     ! -f ${KERNEL_DIR}/usr/include/linux/videodev2.h -o \
     ! -f ${KERNEL_DIR}/usr/include/linux/fb.h -o \
     ! -f ${KERNEL_DIR}/usr/include/linux/v4l2-controls.h -o \
     ! -f ${KERNEL_DIR}/usr/include/linux/v4l2-common.h -o \
     ! -f ${KERNEL_DIR}/usr/include/linux/v4l2-subdev.h -o \
     ! -f ${KERNEL_DIR}/usr/include/linux/v4l2-mediabus.h -o \
     ! -f ${KERNEL_DIR}/usr/include/linux/ivtv.h -o \
     ! -f ${KERNEL_DIR}/usr/include/linux/dvb/frontend.h -o \
     ! -f ${KERNEL_DIR}/usr/include/linux/dvb/dmx.h -o \
     ! -f ${KERNEL_DIR}/usr/include/linux/lirc.h -o \
     ! -f ${KERNEL_DIR}/usr/include/linux/bpf.h -o \
     ! -f ${KERNEL_DIR}/usr/include/linux/bpf_common.h -o \
     ! -f ${KERNEL_DIR}/drivers/media/tuners/tuner-xc2028-types.h -o \
     ! -f ${KERNEL_DIR}/usr/include/linux/input-event-codes.h ]; then
	echo "Usage: $0 KERNEL_DIR"
	echo
	echo "KERNEL_DIR must be the path to an extracted kernel source dir"
	echo "and run 'make headers_install' in KERNEL_DIR."
	exit 1
fi

cp -a ${KERNEL_DIR}/usr/include/linux/videodev2.h ${TOPSRCDIR}/include/linux
cp -a ${KERNEL_DIR}/usr/include/linux/fb.h ${TOPSRCDIR}/include/linux
cp -a ${KERNEL_DIR}/usr/include/linux/v4l2-controls.h ${TOPSRCDIR}/include/linux
cp -a ${KERNEL_DIR}/usr/include/linux/v4l2-common.h ${TOPSRCDIR}/include/linux
cp -a ${KERNEL_DIR}/usr/include/linux/v4l2-subdev.h ${TOPSRCDIR}/include/linux
cp -a ${KERNEL_DIR}/usr/include/linux/v4l2-mediabus.h ${TOPSRCDIR}/include/linux
cp -a ${KERNEL_DIR}/usr/include/linux/media-bus-format.h ${TOPSRCDIR}/include/linux
cp -a ${KERNEL_DIR}/usr/include/linux/media.h ${TOPSRCDIR}/include/linux
cp -a ${KERNEL_DIR}/usr/include/linux/ivtv.h ${TOPSRCDIR}/include/linux
cp -a ${KERNEL_DIR}/usr/include/linux/dvb/frontend.h ${TOPSRCDIR}/include/linux/dvb
cp ${TOPSRCDIR}/include/linux/dvb/frontend.h ${TOPSRCDIR}/lib/include/libdvbv5/dvb-frontend.h
cp -a ${KERNEL_DIR}/usr/include/linux/dvb/dmx.h ${TOPSRCDIR}/include/linux/dvb
cp -a ${KERNEL_DIR}/usr/include/linux/lirc.h ${TOPSRCDIR}/include/linux
cp -a ${KERNEL_DIR}/usr/include/linux/bpf.h ${TOPSRCDIR}/include/linux
cp -a ${KERNEL_DIR}/usr/include/linux/bpf_common.h ${TOPSRCDIR}/include/linux
cp -a ${KERNEL_DIR}/usr/include/linux/cec.h ${TOPSRCDIR}/include/linux
cp -a ${KERNEL_DIR}/usr/include/linux/cec-funcs.h ${TOPSRCDIR}/include/linux
cp -a ${KERNEL_DIR}/drivers/media/common/v4l2-tpg/v4l2-tpg-core.c ${TOPSRCDIR}/utils/common
cp -a ${KERNEL_DIR}/drivers/media/common/v4l2-tpg/v4l2-tpg-colors.c ${TOPSRCDIR}/utils/common
cp -a ${KERNEL_DIR}/include/media/tpg/v4l2-tpg.h ${TOPSRCDIR}/utils/common
cp -a ${KERNEL_DIR}/include/media/fwht-ctrls.h ${TOPSRCDIR}/utils/common
patch -d ${TOPSRCDIR} --no-backup-if-mismatch -p1 <${TOPSRCDIR}/utils/common/v4l2-tpg.patch
cp -a ${KERNEL_DIR}/drivers/media/test-drivers/vicodec/codec-fwht.[ch] ${TOPSRCDIR}/utils/common/
cp -a ${KERNEL_DIR}/drivers/media/test-drivers/vicodec/codec-v4l2-fwht.[ch] ${TOPSRCDIR}/utils/common/
patch -d ${TOPSRCDIR} --no-backup-if-mismatch -p1 <${TOPSRCDIR}/utils/common/codec-fwht.patch
grep V4L2_.*_FMT.*descr ${KERNEL_DIR}/drivers/media/v4l2-core/v4l2-ioctl.c | grep -v V4L2_PIX_FMT_H264_SLICE | grep -v V4L2_PIX_FMT_VP8_FRAME | grep -v V4L2_PIX_FMT_HEVC_SLICE | perl -pe 's/.*V4L2_(.*)_FMT/\tcase V4L2_\1_FMT/; s/:.*descr = /: return /; s/;.*/;/;' >${TOPSRCDIR}/utils/common/v4l2-pix-formats.h

function keytable {
	SRCDIR=${TOPSRCDIR}/utils/keytable

	cd ${SRCDIR}
	echo generating ${SRCDIR}/parse.h
	./gen_input_events.pl < ${KERNEL_DIR}/usr/include/linux/input-event-codes.h  > ${SRCDIR}/parse.h
	mkdir -p ${SRCDIR}/rc_keymaps
	rm -f ${SRCDIR}/rc_keymaps/*
	echo storing existing keymaps at ${SRCDIR}/rc_keymaps/
	./gen_keytables.pl ${KERNEL_DIR};
	cp ${SRCDIR}/rc_keymaps_userspace/* ${SRCDIR}/rc_keymaps/
}

function libdvbv5 {
	SRCDIR=${TOPSRCDIR}/lib/libdvbv5

	cd ${SRCDIR}
	./gen_dvb_structs.pl ${KERNEL_DIR}/usr/include/
}

function xc3028-firmware {
	SRCDIR=${TOPSRCDIR}/contrib/xc3028-firmware

	cp -a ${KERNEL_DIR}/drivers/media/tuners/tuner-xc2028-types.h ${SRCDIR}/
}

function ioctl-test {
	SRCDIR=${TOPSRCDIR}/contrib/test

	cd ${SRCDIR}

	./gen_ioctl_list.pl --gen_ioctl_numbers >.tmp_ioctl.c
	gcc -I ../../include/ .tmp_ioctl.c -o tmp_ioctl32 -m32
	gcc -I ../../include/ .tmp_ioctl.c -o tmp_ioctl64 -m64
	./tmp_ioctl32 32 >ioctl_32.h
	./tmp_ioctl64 64 >ioctl_64.h
	rm .tmp_ioctl.c tmp_ioctl32 tmp_ioctl64
	./gen_ioctl_list.pl >ioctl-test.h
}

function freebsd {
	SRCDIR=${TOPSRCDIR}/contrib/freebsd

	cd ${SRCDIR}

	rm -rf .pc

	for i in input.h input-event-codes.h ivtv.h uinput.h videodev2.h v4l2-controls.h v4l2-common.h; do
		mkdir -p include/linux/$(dirname $i)
		cp ${KERNEL_DIR}/usr/include/linux/$i include/linux/$i
	done

	for i in ivtv.h uinput.h videodev2.h v4l2-common.h; do
		sed -e 's/__u8/uint8_t/g' -e 's/__u16/uint16_t/g' -e 's/__u32/uint32_t/g' -e 's/__u64/uint64_t/g' -e 's/__s8/int8_t/g' -e 's/__s16/int16_t/g' -e 's/__s32/int32_t/g' -e 's/__s64/int64_t/g' -e 's/__le32/uint32_t/g' -e 's/__user//g' -i include/linux/$i
	done

	quilt push -a
}

keytable
libdvbv5
freebsd
ioctl-test
xc3028-firmware
