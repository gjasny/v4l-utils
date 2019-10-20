// SPDX-License-Identifier: GPL-2.0+
//
// Copyright (C) 2018 Sean Young <sean@mess.org>

#include <linux/lirc.h>
#include <linux/bpf.h>

#include "bpf_helpers.h"

enum state {
	STATE_INACTIVE,
	STATE_BITS_SPACE,
	STATE_BITS_PULSE,
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
int margin = 325;

#define BPF_PARAM(x) (int)(&(x))

static inline int eq_margin(unsigned d1, unsigned d2)
{
	return ((d1 > (d2 - BPF_PARAM(margin))) && (d1 < (d2 + BPF_PARAM(margin))));
}

SEC("imon_rsc")
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
		if (pulse && (eq_margin(duration, 2000) ||
			      eq_margin(duration, 3250))) {
			s->state = STATE_BITS_SPACE;
			s->bits = 0;
			s->count = 0;
		}
		break;

	case STATE_BITS_PULSE:
		if (pulse && eq_margin(duration, 625))
			s->state = STATE_BITS_SPACE;
		else
			s->state = STATE_INACTIVE;
		break;

	case STATE_BITS_SPACE:
		if (pulse) {
			s->state = STATE_INACTIVE;
			break;
		}

		if (duration > 2400) {
			int x = 0, y = 0;

			if (!(s->count == 5 || s->count == 4)) {
				s->state = STATE_INACTIVE;
				break;
			}

			switch (s->bits & 0x0f) {
			case 0x0: x =  0; y = -4; break;
			case 0x1: x =  0; y =  4; break;
			case 0x2: x =  4; y =  0; break;
			case 0x3: x = -4; y =  0; break;

			case 0x4: x =  4; y = -4; break;
			case 0x5: x = -4; y =  4; break;
			case 0x6: x =  4; y =  4; break;
			case 0x7: x = -4; y = -4; break;

			case 0xc: x =  4; y = -2; break;
			case 0xd: x = -4; y =  2; break;
			case 0xe: x =  2; y =  4; break;
			case 0xf: x = -2; y = -4; break;

			case 0x8: x =  2; y = -4; break;
			case 0x9: x = -2; y =  4; break;
			case 0xa: x =  4; y =  2; break;
			case 0xb: x = -4; y = -2; break;
			}

			bpf_rc_pointer_rel(sample, x, y);

			s->state = STATE_INACTIVE;
			break;
		}

		s->bits <<= 1;

		if (eq_margin(duration, 1800))
			s->bits |= 1;
		else if (!eq_margin(duration, 625)) {
			s->state = STATE_INACTIVE;
			break;
		}
		s->count++;
		s->state = STATE_BITS_PULSE;
		break;
	}

	return 0;
}

char _license[] SEC("license") = "GPL";
