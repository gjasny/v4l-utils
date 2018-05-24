// SPDX-License-Identifier: GPL-2.0+
//
// Copyright (C) 2018 Sean Young <sean@mess.org>

#include <linux/lirc.h>
#include <linux/bpf.h>

#include "bpf_helpers.h"

enum grundig_state {
	STATE_INACTIVE,
	STATE_HEADER_SPACE,
	STATE_LEADING_PULSE,
	STATE_BITS_SPACE,
	STATE_BITS_PULSE,
};

struct decoder_state {
	unsigned int bits;
	enum grundig_state state;
	unsigned int count;
	unsigned int last_space;
};

struct bpf_map_def SEC("maps") decoder_state_map = {
	.type = BPF_MAP_TYPE_ARRAY,
	.key_size = sizeof(unsigned int),
	.value_size = sizeof(struct decoder_state),
	.max_entries = 1,
};

static inline int eq_margin(unsigned d1, unsigned d2, unsigned margin)
{
	return ((d1 > (d2 - margin)) && (d1 < (d2 + margin)));
}

// These values can be overridden in the rc_keymap toml
//
// We abuse elf relocations. We cast the address of these variables to
// an int, so that the compiler emits a mov immediate for the address
// but uses it as an int. The bpf loader replaces the relocation with the
// actual value (either overridden or taken from the data segment).
int header_pulse = 900;
int header_space = 2900;
int leader_pulse = 1300;

#define BPF_PARAM(x) (int)(&(x))

SEC("grundig")
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

	if (s->state == STATE_INACTIVE) {
		if (LIRC_IS_PULSE(*sample) && eq_margin(BPF_PARAM(header_pulse), duration, 100)) {
			s->bits = 0;
			s->state = STATE_HEADER_SPACE;
			s->count = 0;
		}
	} else if (s->state == STATE_HEADER_SPACE) {
		if (LIRC_IS_SPACE(*sample) && eq_margin(BPF_PARAM(header_space), duration, 200))
			s->state = STATE_LEADING_PULSE;
		else
			s->state = STATE_INACTIVE;
	} else if (s->state == STATE_LEADING_PULSE) {
		if (LIRC_IS_PULSE(*sample) && eq_margin(BPF_PARAM(leader_pulse), duration, 100))
			s->state = STATE_BITS_SPACE;
		else
			s->state = STATE_INACTIVE;
	} else if (s->state == STATE_BITS_SPACE) {
		s->last_space = duration;
		s->state = STATE_BITS_PULSE;
	} else if (s->state == STATE_BITS_PULSE) {
		int t = -1;
		if (eq_margin(s->last_space, (472), (150)) &&
		    eq_margin(duration, (583), (150)))
			t = 0;
		if (eq_margin(s->last_space, (1139), (150)) &&
		    eq_margin(duration, (583), (150)))
			t = 1;
		if (eq_margin(s->last_space, (1806), (150)) &&
		    eq_margin(duration, (583), (150)))
			t = 2;
		if (eq_margin(s->last_space, (2200), (150)) &&
		    eq_margin(duration, (1139), (150)))
			t = 3;
		if (t < 0) {
			s->state = STATE_INACTIVE;
		} else {
			s->bits <<= 2;
			switch (t) {
			case 3: s->bits |= 0; break;
			case 2: s->bits |= 3; break;
			case 1: s->bits |= 2; break;
			case 0: s->bits |= 1; break;
			}
			s->count += 2;
			if (s->count == 16) {
				bpf_rc_keydown(sample, 0x40, s->bits, 0);
				s->state = STATE_INACTIVE;
			} else {
				s->state = STATE_BITS_SPACE;
			}
		}
	}

	return 0;
}

char _license[] SEC("license") = "GPL";
