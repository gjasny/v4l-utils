/*
    V4L2 API compliance time32/time64 ioctl tests.

    Copyright (C) 2021  Hans Verkuil <hverkuil-cisco@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 */

#include <sys/types.h>

#include "compiler.h"
#include "v4l2-compliance.h"

typedef __s32		old_time32_t;

struct old_timespec32 {
	old_time32_t	tv_sec;
	__s32		tv_nsec;
};

struct old_timeval32 {
	old_time32_t	tv_sec;
	__s32		tv_usec;
};

/*
 * The user space interpretation of the 'v4l2_event' differs
 * based on the 'time_t' definition on 32-bit architectures, so
 * the kernel has to handle both.
 * This is the old version for 32-bit architectures.
 */
struct v4l2_event_time32 {
	__u32				type;
	union {
		struct v4l2_event_vsync		vsync;
		struct v4l2_event_ctrl		ctrl;
		struct v4l2_event_frame_sync	frame_sync;
		struct v4l2_event_src_change	src_change;
		struct v4l2_event_motion_det	motion_det;
		__u8				data[64];
	} u;
	__u32				pending;
	__u32				sequence;
	struct old_timespec32		timestamp;
	__u32				id;
	__u32				reserved[8];
};

#define	VIDIOC_DQEVENT_TIME32	 _IOR('V', 89, struct v4l2_event_time32)

typedef __s64		new_time64_t;

struct new_timespec64 {
	new_time64_t	tv_sec;
	__s64		tv_nsec;
};

struct new_timeval64 {
	new_time64_t	tv_sec;
	__s64		tv_usec;
};

struct v4l2_event_time64 {
	__u32				type;
	union {
		struct v4l2_event_vsync		vsync;
		struct v4l2_event_ctrl		ctrl;
		struct v4l2_event_frame_sync	frame_sync;
		struct v4l2_event_src_change	src_change;
		struct v4l2_event_motion_det	motion_det;
		__u8				data[64];
	} u;
	__u32				pending;
	__u32				sequence;
	struct new_timespec64		timestamp;
	__u32				id;
	__u32				reserved[8];
};

#define	VIDIOC_DQEVENT_TIME64	 _IOR('V', 89, struct v4l2_event_time64)

struct v4l2_buffer_time32 {
	__u32			index;
	__u32			type;
	__u32			bytesused;
	__u32			flags;
	__u32			field;
	struct old_timeval32	timestamp;
	struct v4l2_timecode	timecode;
	__u32			sequence;

	/* memory location */
	__u32			memory;
	union {
		__u32           offset;
		unsigned long   userptr;
		struct v4l2_plane *planes;
		__s32		fd;
	} m;
	__u32			length;
	__u32			reserved2;
	union {
		__s32		request_fd;
		__u32		reserved;
	};
};
#define VIDIOC_QUERYBUF_TIME32	_IOWR('V',  9, struct v4l2_buffer_time32)
#define VIDIOC_QBUF_TIME32	_IOWR('V', 15, struct v4l2_buffer_time32)
#define VIDIOC_DQBUF_TIME32	_IOWR('V', 17, struct v4l2_buffer_time32)
#define VIDIOC_PREPARE_BUF_TIME32 _IOWR('V', 93, struct v4l2_buffer_time32)

int testTime32_64(struct node *node)
{
	struct v4l2_event_subscription sub = { 0 };
	struct v4l2_event_time32 ev32;
	struct v4l2_event_time64 ev64;
	struct v4l2_event ev;

	if (node->controls.empty())
		return 0;

	fail_on_test(VIDIOC_DQEVENT != VIDIOC_DQEVENT_TIME32 &&
		     VIDIOC_DQEVENT != VIDIOC_DQEVENT_TIME64);

	for (const auto &control : node->controls) {
		const test_query_ext_ctrl &qctrl = control.second;
		if (qctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS)
			continue;

		info("checking control event '%s' (0x%08x)\n", qctrl.name, qctrl.id);
		sub.type = V4L2_EVENT_CTRL;
		sub.id = qctrl.id;
		sub.flags = V4L2_EVENT_SUB_FL_SEND_INITIAL;
		fail_on_test(doioctl(node, VIDIOC_SUBSCRIBE_EVENT, &sub));
		fail_on_test(doioctl(node, VIDIOC_DQEVENT, &ev));
		fail_on_test(doioctl(node, VIDIOC_UNSUBSCRIBE_EVENT, &sub));

		fail_on_test(doioctl(node, VIDIOC_SUBSCRIBE_EVENT, &sub));
		int ret32 = doioctl(node, VIDIOC_DQEVENT_TIME32, &ev32);
		fail_on_test(doioctl(node, VIDIOC_UNSUBSCRIBE_EVENT, &sub));

		fail_on_test(doioctl(node, VIDIOC_SUBSCRIBE_EVENT, &sub));
		int ret64 = doioctl(node, VIDIOC_DQEVENT_TIME64, &ev64);
		fail_on_test(doioctl(node, VIDIOC_UNSUBSCRIBE_EVENT, &sub));

		__u64 ev_ts = ev.timestamp.tv_sec * 1000000000ULL + ev.timestamp.tv_nsec;

		if (ret32 != ENOTTY) {
			fail_on_test(ret32);
			fail_on_test(ev.type != ev32.type);
			fail_on_test(ev.id != ev32.id);
			fail_on_test(check_0(ev32.reserved, sizeof(ev32.reserved)));
			__u64 ev32_ts = ev32.timestamp.tv_sec * 1000000000ULL + ev32.timestamp.tv_nsec;
			__s64 delta_ms = (ev32_ts - ev_ts) / 1000000;
			fail_on_test_val(delta_ms > 500, (int)delta_ms);
			info("VIDIOC_DQEVENT 32-bit timespec: %lld ms\n", delta_ms);
		}

		if (ret64 != ENOTTY) {
			fail_on_test(ret64);
			fail_on_test(ev.type != ev64.type);
			fail_on_test(ev.id != ev64.id);
			fail_on_test(check_0(ev64.reserved, sizeof(ev64.reserved)));
			__u64 ev64_ts = ev64.timestamp.tv_sec * 1000000000ULL + ev64.timestamp.tv_nsec;
			__s64 delta_ms = (ev64_ts - ev_ts) / 1000000;
			fail_on_test_val(delta_ms > 500, (int)delta_ms);
			info("VIDIOC_DQEVENT 64-bit timespec: %lld ms\n", delta_ms);
		}

		break;
	}
	return 0;
}
