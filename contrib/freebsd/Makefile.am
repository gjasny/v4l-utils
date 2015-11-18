EXTRA_DIST = \
	include \
	patches

sync-with-kernel:
	rm -rf .pc

	for i in input.h input-event-codes.h ivtv.h uinput.h videodev2.h v4l2-controls.h v4l2-common.h; do \
	    mkdir -p include/linux/$$(dirname $$i); \
	    cp $(KERNEL_DIR)/usr/include/linux/$$i include/linux/$$i; \
	done

	for i in ivtv.h uinput.h videodev2.h v4l2-common.h; do \
	    sed -e 's/__u8/uint8_t/g' -e 's/__u16/uint16_t/g' -e 's/__u32/uint32_t/g' -e 's/__u64/uint64_t/g' -e 's/__s8/int8_t/g' -e 's/__s16/int16_t/g' -e 's/__s32/int32_t/g' -e 's/__s64/int64_t/g' -e 's/__le32/uint32_t/g' -e 's/__user//g' -i include/linux/$$i; \
	done

	quilt push -a
