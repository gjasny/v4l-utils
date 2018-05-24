// SPDX-License-Identifier: GPL-2.0+
//
// Copyright (C) 2018 Sean Young <sean@mess.org>

#include <linux/lirc.h>
#include <linux/bpf.h>

#include "bpf_helpers.h"

enum state {
	STATE_INACTIVE,
	STATE_HEADER_SPACE,
	STATE_REPEAT_SPACE,
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
int margin = 200;
int header_pulse = 2125;
int header_space = 1875;
int repeat_pulse = 0;
int repeat_space = 0;
int bit_pulse = 625;
int bit_0_space = 375;
int bit_1_space = 1625;
int trailer_pulse = 625;
int bits = 4;
int reverse = 0;
int header_optional = 0;
int rc_protocol = 64;

#define BPF_PARAM(x) (int)(&(x))

static inline int eq_margin(unsigned d1, unsigned d2)
{
	return ((d1 > (d2 - BPF_PARAM(margin))) && (d1 < (d2 + BPF_PARAM(margin))));
}

SEC("pulse_distance")
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
	case STATE_HEADER_SPACE:
		if (!pulse && eq_margin(BPF_PARAM(header_space), duration))
			s->state = STATE_BITS_PULSE;
		else
			s->state = STATE_INACTIVE;
		break;
	case STATE_REPEAT_SPACE:
		if (!pulse && eq_margin(BPF_PARAM(repeat_space), duration))
			s->state = STATE_TRAILER;
		else
			s->state = STATE_INACTIVE;
		break;
	case STATE_INACTIVE:
		if (pulse && eq_margin(BPF_PARAM(header_pulse), duration)) {
			s->bits = 0;
			s->state = STATE_HEADER_SPACE;
			s->count = 0;
			break;
		}
		if (pulse && BPF_PARAM(repeat_pulse) > 0 &&
		    eq_margin(BPF_PARAM(repeat_pulse), duration)) {
			s->state = STATE_REPEAT_SPACE;
			s->count = 0;
			break;
		}
		if (!BPF_PARAM(header_optional))
			break;
		/* pass through */
	case STATE_BITS_PULSE:
		if (pulse && eq_margin(BPF_PARAM(bit_pulse), duration))
			s->state = STATE_BITS_SPACE;
		else
			s->state = STATE_INACTIVE;
		break;
	case STATE_BITS_SPACE:
		if (pulse) {
			s->state = STATE_INACTIVE;
			break;
		}

		unsigned long set_bit = 1;

		if (BPF_PARAM(reverse))
			set_bit <<= s->count;
		else
			s->bits <<= 1;

		if (eq_margin(BPF_PARAM(bit_1_space), duration))
			s->bits |= set_bit;
		else if (!eq_margin(BPF_PARAM(bit_0_space), duration)) {
			s->state = STATE_INACTIVE;
			break;
		}

		s->count++;
		if (s->count == BPF_PARAM(bits))
			s->state = STATE_TRAILER;
		else
			s->state = STATE_BITS_PULSE;
		break;
	case STATE_TRAILER:
		if (pulse && eq_margin(BPF_PARAM(trailer_pulse), duration)) {
			if (s->count == 0)
				bpf_rc_repeat(sample);
			else
				bpf_rc_keydown(sample, BPF_PARAM(rc_protocol), s->bits, 0);
		}

		s->state = STATE_INACTIVE;
	}

	return 0;
}

char _license[] SEC("license") = "GPL";
