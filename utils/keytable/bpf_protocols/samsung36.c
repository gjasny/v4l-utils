// SPDX-License-Identifier: GPL-2.0+
//
// Remote protocol used by some Samsung remotes. It has 36 bits and the
// 16th bit is not really a bit, but a marker to distinguish it from
// shorter samsung protocols.
//
// http://www.hifi-remote.com/wiki/index.php/DecodeIR#Samsung36
// Copyright (C) 2020 Sean Young <sean@mess.org>

#include <linux/lirc.h>
#include <linux/bpf.h>

#include "bpf_helpers.h"

enum state {
	STATE_INACTIVE,
	STATE_HEADER_SPACE,
	STATE_BITS_SPACE,
	STATE_BITS_PULSE,
	STATE_TRAILER,
};

struct decoder_state {
	unsigned long bits;
	enum state state;
	unsigned int count;
};

struct bpf_map_def SEC("maps") decoder_state_map = {
	.type = BPF_MAP_TYPE_ARRAY,
	.key_size = sizeof(unsigned int),
	.value_size = sizeof(struct decoder_state),
	.max_entries = 1,
};

// These values can be overridden in the rc_keymap toml
//
// We abuse elf relocations. We cast the address of these variables to
// an int, so that the compiler emits a mov immediate for the address
// but uses it as an int. The bpf loader replaces the relocation with the
// actual value (either overridden or taken from the data segment).
int margin = 300;
int rc_protocol = 69;

#define BPF_PARAM(x) (int)(&(x))

static inline int eq_margin(unsigned d1, unsigned d2)
{
	return ((d1 > (d2 - BPF_PARAM(margin))) && (d1 < (d2 + BPF_PARAM(margin))));
}

SEC("samsung36")
int bpf_decoder(unsigned int *sample)
{
	unsigned int key = 0;
	struct decoder_state *s = bpf_map_lookup_elem(&decoder_state_map, &key);

	if (!s)
		return 0;

	switch (*sample & LIRC_MODE2_MASK) {
	case LIRC_MODE2_SPACE:
	case LIRC_MODE2_PULSE:
	case LIRC_MODE2_TIMEOUT:
		break;
	default:
		// not a timing events
		return 0;
	}

	int duration = LIRC_VALUE(*sample);
	int pulse = LIRC_IS_PULSE(*sample);

	switch (s->state) {
	case STATE_INACTIVE:
		if (pulse && eq_margin(duration, 4500)) {
			s->bits = 0;
			s->state = STATE_HEADER_SPACE;
			s->count = 0;
		}
		break;
	case STATE_HEADER_SPACE:
		if (!pulse && eq_margin(duration, 4500))
			s->state = STATE_BITS_PULSE;
		else
			s->state = STATE_INACTIVE;
		break;
	case STATE_BITS_PULSE:
		if (pulse && eq_margin(duration, 500))
			s->state = STATE_BITS_SPACE;
		else
			s->state = STATE_INACTIVE;
		break;
	case STATE_BITS_SPACE:
		if (pulse) {
			s->state = STATE_INACTIVE;
			break;
		}

		s->count++;

		if (s->count == 17) {
			if (eq_margin(duration, 4450)) {
				s->state = STATE_BITS_PULSE;
			} else {
				s->state = STATE_INACTIVE;
			}
			break;
		}

		s->bits <<= 1;

		if (eq_margin(duration, 1600)) {
			s->bits |= 1;
		} else if (!eq_margin(duration, 500)) {
			s->state = STATE_INACTIVE;
			break;
		}

		if (s->count == 37)
			s->state = STATE_TRAILER;
		else
			s->state = STATE_BITS_PULSE;
		break;
	case STATE_TRAILER:
		if (pulse && eq_margin(duration, 500)) {
			bpf_rc_keydown(sample, BPF_PARAM(rc_protocol), s->bits, 0);
		}

		s->state = STATE_INACTIVE;
	}

	return 0;
}

char _license[] SEC("license") = "GPL";
