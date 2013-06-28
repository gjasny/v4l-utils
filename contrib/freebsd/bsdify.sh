#!/bin/bash

set -o errexit
set -x

KERNEL_DIR="${1?'Need kernel directory'}"

SED=sed

function replace_types {
    $SED \
	-e 's/__u8/uint8_t/g' \
	-e 's/__u16/uint16_t/g' \
	-e 's/__u32/uint32_t/g' \
	-e 's/__u64/uint64_t/g' \
	-e 's/__s8/int8_t/g' \
	-e 's/__s16/int16_t/g' \
	-e 's/__s32/int32_t/g' \
	-e 's/__s64/int64_t/g' \
	-e 's/__le32/uint32_t/g' \
	-e 's/__user//g' \
	-i "${1?'Missing file name'}"
}

# clean

rm -rf include .pc

# copy

for i in input.h ivtv.h uinput.h videodev2.h v4l2-controls.h v4l2-common.h dvb/{audio.h,ca.h,dmx.h,frontend.h,net.h,osd.h,version.h,video.h}; do
    mkdir -p include/linux/$(dirname $i)
    cp $KERNEL_DIR/include/uapi/linux/$i include/linux/$i
done

# replace kernel types

for i in ivtv.h uinput.h videodev2.h dvb/{audio.h,ca.h,dmx.h,frontend.h,net.h,osd.h,version.h,video.h}; do
    replace_types include/linux/$i
done


quilt push -a
