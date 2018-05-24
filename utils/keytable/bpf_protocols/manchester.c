// SPDX-License-Identifier: GPL-2.0+
//
// Copyright (C) 2018 Sean Young <sean@mess.org>

#include <linux/lirc.h>
#include <linux/bpf.h>

#include "bpf_helpers.h"

struct decoder_state {
	unsigned int state;
	unsigned int count;
	unsigned long bits;
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
// See:
// http://clearwater.com.au/code/rc5
int margin = 200;
int header_pulse = 0;
int header_space = 0;
int zero_pulse = 888;
int zero_space = 888;
int one_pulse = 888;
int one_space = 888;
int toggle_bit = 100;
int bits = 14;
int scancode_mask = 0;
int rc_protocol = 66;

#define BPF_PARAM(x) (int)(&(x))

static inline int eq_margin(unsigned d1, unsigned d2)
{
	return ((d1 > (d2 - BPF_PARAM(margin))) && (d1 < (d2 + BPF_PARAM(margin))));
}

#define STATE_INACTIVE 0
#define STATE_HEADER   1
#define STATE_START1   2
#define STATE_MID1     3
#define STATE_MID0     4
#define STATE_START0   5

static int emitBit(unsigned int *sample, struct decoder_state *s, int bit, int state)
{
	s->bits <<= 1;
	s->bits |= bit;
	s->count++;

	if (s->count == BPF_PARAM(bits)) {
		unsigned int toggle = 0;
		unsigned long mask = BPF_PARAM(scancode_mask);
		if (BPF_PARAM(toggle_bit) < BPF_PARAM(bits)) {
			unsigned int tmask = 1 << BPF_PARAM(toggle_bit);
			if (s->bits & tmask)
				toggle = 1;
			mask |= tmask;
		}

		bpf_rc_keydown(sample, BPF_PARAM(rc_protocol), s->bits & ~mask,
			       toggle);
		state = STATE_INACTIVE;
	}

	return state;
}

SEC("manchester")
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

	unsigned int newState = s->state;

	switch (s->state) {
	case STATE_INACTIVE:
		if (BPF_PARAM(header_pulse)) {
			if (pulse &&
			    eq_margin(BPF_PARAM(header_pulse), duration)) {
				s->state = STATE_HEADER;
			}
			break;
		}
		/* pass through */
	case STATE_HEADER:
		if (BPF_PARAM(header_space)) {
			if (!pulse &&
			    eq_margin(BPF_PARAM(header_space), duration)) {
				s->state = STATE_MID1;
			}
			break;
		}
		s->bits = 0;
		s->count = 0;
		/* pass through */
	case STATE_MID1:
		if (!pulse)
			break;

		if (eq_margin(BPF_PARAM(one_pulse), duration))
			newState = emitBit(sample, s, 1, STATE_START1);
		else if (eq_margin(BPF_PARAM(one_pulse) +
				   BPF_PARAM(zero_pulse), duration))
			newState = emitBit(sample, s, 1, STATE_MID0);
		break;
	case STATE_MID0:
		if (pulse)
			break;

		if (eq_margin(BPF_PARAM(zero_space), duration))
			newState = emitBit(sample, s, 0, STATE_START0);
		else if (eq_margin(BPF_PARAM(zero_space) +
				   BPF_PARAM(one_space), duration))
			newState = emitBit(sample, s, 0, STATE_MID1);
		else
			newState = emitBit(sample, s, 0, STATE_INACTIVE);
		break;
	case STATE_START1:
		if (!pulse && eq_margin(BPF_PARAM(zero_space), duration))
			newState = STATE_MID1;
		break;
	case STATE_START0:
		if (pulse && eq_margin(BPF_PARAM(one_pulse), duration))
			newState = STATE_MID0;
		break;
	}

	//char fmt1[] = "bits:%d state:%d newState:%d\n";
	//bpf_trace_printk(fmt1, sizeof(fmt1), pulse, duration, s->bits);
	//char fmt2[] = "count:%d state:%d newState:%d\n";
	//bpf_trace_printk(fmt2, sizeof(fmt2), s->count, s->state, newState);

	if (newState == s->state)
		s->state = STATE_INACTIVE;
	else
		s->state = newState;

	return 0;
}

char _license[] SEC("license") = "GPL";
