/* SPDX-License-Identifier: GPL-2.0 */

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "keymap.h"

// Some keymaps use BPF decoders, so the kernel has no idea how to encode
// them. We need user-space encoders for these.
//
// This encoders should match what the BPF decoders in
// utils/keytable/bpf_protocols/*.c decode.

static void encode_pulse_distance(struct keymap *map, uint32_t scancode, int *buf, int *length)
{
	int len = 0, bits, i;

	buf[len++] = keymap_param(map, "header_pulse", 2125);
	buf[len++] = keymap_param(map, "header_space", 1875);

	bits = keymap_param(map, "bits", 4);

	if (keymap_param(map, "reverse", 0)) {
		for (i = 0; i < bits; i++) {
			buf[len++] = keymap_param(map, "bit_pulse", 625);

			if (scancode & (1 << i))
				buf[len++] = keymap_param(map, "bit_1_space", 1625);
			else
				buf[len++] = keymap_param(map, "bit_0_space", 375);
		}
	} else {
		for (i = bits - 1; i >= 0; i--) {
			buf[len++] = keymap_param(map, "bit_pulse", 625);

			if (scancode & (1 << i))
				buf[len++] = keymap_param(map, "bit_1_space", 1625);
			else
				buf[len++] = keymap_param(map, "bit_0_space", 375);
		}
	}

	buf[len++] = keymap_param(map, "trailer_pulse", 625);

	*length = len;
}

static void encode_pulse_length(struct keymap *map, uint32_t scancode, int *buf, int *length)
{
	int len = 0, bits, i;

	buf[len++] = keymap_param(map, "header_pulse", 2125);
	buf[len++] = keymap_param(map, "header_space", 1875);

	bits = keymap_param(map, "bits", 4);

	if (keymap_param(map, "reverse", 0)) {
		for (i = 0; i < bits; i++) {
			if (scancode & (1 << i))
				buf[len++] = keymap_param(map, "bit_1_space", 1625);
			else
				buf[len++] = keymap_param(map, "bit_0_space", 375);

			buf[len++] = keymap_param(map, "bit_pulse", 625);
		}
	} else {
		for (i = bits - 1; i >= 0; i--) {
			if (scancode & (1 << i))
				buf[len++] = keymap_param(map, "bit_1_space", 1625);
			else
				buf[len++] = keymap_param(map, "bit_0_space", 375);

			buf[len++] = keymap_param(map, "bit_pulse", 625);
		}
	}

	*length = len;
}

static void encode_manchester(struct keymap *map, uint32_t scancode, int *buf, int *length)
{
	int len = 0, bits, i;

	void advance_space(unsigned length)
	{
		if (len % 2)
			buf[len] += length;
		else
			buf[++len] = length;
	}

	void advance_pulse(unsigned length)
	{
		if (len % 2)
			buf[++len] = length;
		else
			buf[len] += length;
	}

	bits = keymap_param(map, "bits", 14);

	for (i = bits - 1; i >= 0; i--) {
		if (scancode & (1 << i)) {
			advance_pulse(keymap_param(map, "one_pulse", 888));
			advance_space(keymap_param(map, "one_space", 888));
		} else {
			advance_space(keymap_param(map, "zero_space", 888));
			advance_pulse(keymap_param(map, "zero_pulse", 888));
		}
	}

	/* drop any trailing pulse */
        *length = (len % 2) ? len : len + 1;
}

bool encode_bpf_protocol(struct keymap *map, uint32_t scancode, int *buf, int *length)
{
	if (!strcmp(map->protocol, "pulse_distance")) {
		encode_pulse_distance(map, scancode, buf, length);
		return true;
	}

	if (!strcmp(map->protocol, "pulse_length")) {
		encode_pulse_length(map, scancode, buf, length);
		return true;
	}

	if (!strcmp(map->protocol, "manchester")) {
		encode_manchester(map, scancode, buf, length);
		return true;
	}

	return false;
}
