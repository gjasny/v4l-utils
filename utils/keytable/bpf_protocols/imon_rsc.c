// SPDX-License-Identifier: GPL-2.0+
//
// Copyright (C) 2018 Sean Young <sean@mess.org>

#include <linux/lirc.h>
#include <linux/bpf.h>

#include "bpf_helpers.h"

enum state {
	STATE_INACTIVE,
	STATE_HEADER_SPACE,
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
int margin = 250;

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
			s->state = STATE_HEADER_SPACE;
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
	case STATE_HEADER_SPACE:
		if (!pulse && eq_margin(duration, 1875)) {
			s->state = STATE_BITS_PULSE;
			break;
		}
	case STATE_BITS_SPACE:
		if (pulse) {
			s->state = STATE_INACTIVE;
			break;
		}

		if (s->count == 4) {
			int x = 0, y = 0;
			switch (s->bits) {
			case 0:  x = 0;  y = -4; break;
			case 8:  x = 0;  y =  4; break;
			case 4:  x = 4;  y =  0; break;
			case 12: x = -4; y =  0; break;

			case 2:  x = 4;  y = -4; break;
			case 10: x = -4; y =  4; break;
			case 6:  x = 4;  y =  4; break;
			case 14: x = -4; y = -4; break;

			case  1: x = 4;  y = -2; break;
			case  9: x = -4; y =  2; break;
			case  5: x = 2;  y =  4; break;
			case 13: x = -2; y = -4; break;

			case 3:  x = 2;  y = -4; break;
			case 11: x = -2; y =  4; break;
			case 7:  x = 4;  y =  2; break;
			case 15: x = -4; y = -2; break;
			}
			bpf_rc_pointer_rel(sample, x, y);

			s->state = STATE_INACTIVE;
			break;
		}

		if (eq_margin(duration, 1700))
			s->bits |= 1 << s->count;
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
