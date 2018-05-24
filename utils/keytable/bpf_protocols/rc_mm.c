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
	STATE_TRAILER,
};

struct decoder_state {
	unsigned int bits;
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
//
// This is why they should be accessed through the BPF_PARAM() macro.

#define BPF_PARAM(x) (int)(&(x))

int margin = 100;
int header_pulse = 417;
int header_space = 278;
int bit_pulse = 167;
int bit_0_space = 278;
int bit_1_space = 444;
int bit_2_space = 611;
int bit_3_space = 778;
int trailer = 167;
int bits = 8;
int rc_protocol = 65;

static inline int eq_margin(unsigned d1, unsigned d2)
{
	return ((d1 > (d2 - BPF_PARAM(margin))) && (d1 < (d2 + BPF_PARAM(margin))));
}

SEC("rc_mm")
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

	switch (s->state) {
	case STATE_INACTIVE:
		if (LIRC_IS_PULSE(*sample) && eq_margin(BPF_PARAM(header_pulse), duration)) {
			s->state = STATE_HEADER_SPACE;
			s->bits = 0;
			s->count = 0;
		}
		break;
	case STATE_HEADER_SPACE:
		if (LIRC_IS_SPACE(*sample) && eq_margin(BPF_PARAM(header_space), duration))
			s->state = STATE_BITS_PULSE;
		else
			s->state = STATE_INACTIVE;
		break;
	case STATE_BITS_PULSE:
		if (LIRC_IS_PULSE(*sample) && eq_margin(BPF_PARAM(bit_pulse), duration)) {
			s->state = STATE_BITS_SPACE;
		} else {
			s->state = STATE_INACTIVE;
		}
		break;
	case STATE_BITS_SPACE:
		if (!LIRC_IS_SPACE(*sample)) {
			s->state = STATE_INACTIVE;
			break;
		}

		s->bits <<= 2;

		if (eq_margin(BPF_PARAM(bit_0_space), duration)) {
			s->bits |= 0;
		} else if (eq_margin(BPF_PARAM(bit_1_space), duration)) {
			s->bits |= 1;
		} else if (eq_margin(BPF_PARAM(bit_2_space), duration)) {
			s->bits |= 2;
		} else if (eq_margin(BPF_PARAM(bit_3_space), duration)) {
			s->bits |= 3;
		} else {
			s->state = STATE_INACTIVE;
			break;
		}

		s->count += 2;
		if (s->count >= BPF_PARAM(bits)) {
			s->state = STATE_TRAILER;
		} else {
			s->state = STATE_BITS_PULSE;
		}
		break;
	case STATE_TRAILER:
		if (LIRC_IS_PULSE(*sample) && eq_margin(BPF_PARAM(trailer), duration))
			bpf_rc_keydown(sample, BPF_PARAM(rc_protocol), s->bits, 0);

		s->state = STATE_INACTIVE;
	}

	return 0;
}

char _license[] SEC("license") = "GPL";

