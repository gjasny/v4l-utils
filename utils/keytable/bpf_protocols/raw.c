// SPDX-License-Identifier: GPL-2.0+
//
// Copyright (C) 2019 Sean Young <sean@mess.org>
//
// This decoder matches pre-defined pulse-space sequences. It does so by
// iterating through the list There are many optimisation possible, if
// performance is an issue.
//
// First of all iterating through the list of patterns is one-by-one, even
// though after the first few pulse space sequences, most patterns will be
// will not be a match. The bitmap datastructure could use a "next clear bit"
// function.
//
// Secondly this can be transformed into a much more efficient state machine,
// where we pre-compile the patterns much like a regex.

#include <linux/lirc.h>
#include <linux/bpf.h>

#include "bpf_helpers.h"
#include "bitmap.h"

#define MAX_PATTERNS 1024

struct decoder_state {
	int pos;
	DECLARE_BITMAP(nomatch, MAX_PATTERNS);
};

struct bpf_map_def SEC("maps") decoder_state_map = {
	.type = BPF_MAP_TYPE_ARRAY,
	.key_size = sizeof(unsigned int),
	.value_size = sizeof(struct decoder_state),
	.max_entries = 1,
};

struct raw_pattern {
	unsigned int scancode;
	unsigned short raw[0];
};

// ir-keytable will load the raw patterns here
struct bpf_map_def SEC("maps") raw_map = {
	.type = BPF_MAP_TYPE_ARRAY,
	.key_size = sizeof(unsigned int),
	.value_size = sizeof(struct raw_pattern), // this is not used
	.max_entries = MAX_PATTERNS,
};


// These values can be overridden in the rc_keymap toml
//
// We abuse elf relocations. We cast the address of these variables to
// an int, so that the compiler emits a mov immediate for the address
// but uses it as an int. The bpf loader replaces the relocation with the
// actual value (either overridden or taken from the data segment).
int margin = 200;
int rc_protocol = 68;
// The following two values are calculated by ir-keytable
int trail_space = 1000;
int max_length = 1;

#define BPF_PARAM(x) (int)(&(x))

static inline int eq_margin(unsigned d1, unsigned d2)
{
	return ((d1 > (d2 - BPF_PARAM(margin))) && (d1 < (d2 + BPF_PARAM(margin))));
}

SEC("raw")
int bpf_decoder(unsigned int *sample)
{
	unsigned int key = 0;
	struct decoder_state *s = bpf_map_lookup_elem(&decoder_state_map, &key);
	struct raw_pattern *p;
	unsigned int i;

	// Make verifier happy. Should never come to pass
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
	int pos = s->pos;

	if (pos < 0 || pos >= BPF_PARAM(max_length)) {
		if (!pulse && duration >= BPF_PARAM(trail_space)) {
			bitmap_zero(s->nomatch, MAX_PATTERNS);
			s->pos = 0;
		}

		return 0;
	}

	if (!pulse && duration >= BPF_PARAM(trail_space)) {
		for (i = 0; i < MAX_PATTERNS; i++) {
			key = i;
			p = bpf_map_lookup_elem(&raw_map, &key);
			// Make verifier happy. Should never come to pass
			if (!p)
				break;

			// Has this pattern already mismatched?
			if (bitmap_test(s->nomatch, i))
				continue;

			// Are we at the end of the pattern?
			if (p->raw[pos] == 0)
				bpf_rc_keydown(sample, BPF_PARAM(rc_protocol),
					       p->scancode, 0);
		}

		bitmap_zero(s->nomatch, MAX_PATTERNS);
		s->pos = 0;
	} else {
		for (i = 0; i < MAX_PATTERNS; i++) {
			key = i;
			p = bpf_map_lookup_elem(&raw_map, &key);
			// Make verifier happy. Should never come to pass
			if (!p)
				break;

			// Has this pattern already mismatched?
			if (bitmap_test(s->nomatch, i))
				continue;

			// If the pattern ended, or does not match
			if (p->raw[pos] == 0 ||
				!eq_margin(duration,  p->raw[pos]))
				bitmap_set(s->nomatch, i);
		}

		s->pos++;
	}

	return 0;
}

char _license[] SEC("license") = "GPL";
