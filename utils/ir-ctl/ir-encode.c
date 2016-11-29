/*
 * ir-encode.c - encodes IR scancodes in different protocols
 *
 * Copyright (C) 2016 Sean Young <sean@mess.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * TODO: XMP protocol and MCE keyboard
 */

#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

#include "ir-encode.h"

#define NS_TO_US(x) (((x)+500)/1000)

static int nec_encode(enum rc_proto proto, unsigned scancode, unsigned *buf)
{
	const int nec_unit = 562500;
	int n = 0;

	void add_byte(unsigned bits)
	{
		int i;
		for (i=0; i<8; i++) {
			buf[n++] = NS_TO_US(nec_unit);
			if (bits & (1 << i))
				buf[n++] = NS_TO_US(nec_unit * 3);
			else
				buf[n++] = NS_TO_US(nec_unit);
		}
	}

	buf[n++] = NS_TO_US(nec_unit * 16);
	buf[n++] = NS_TO_US(nec_unit * 8);

	switch (proto) {
	default:
		return 0;
	case RC_PROTO_NEC:
		add_byte(scancode >> 8);
		add_byte(~(scancode >> 8));
		add_byte(scancode);
		add_byte(~scancode);
		break;
	case RC_PROTO_NECX:
		add_byte(scancode >> 16);
		add_byte(scancode >> 8);
		add_byte(scancode);
		add_byte(~scancode);
		break;
	case RC_PROTO_NEC32:
		/*
		 * At the time of writing kernel software nec decoder
		 * reverses the bit order so it will not match. Hardware
		 * decoders do not have this issue.
		 */
		add_byte(scancode >> 24);
		add_byte(scancode >> 16);
		add_byte(scancode >> 8);
		add_byte(scancode);
		break;
	}

	buf[n++] = NS_TO_US(nec_unit);

	return n;
}

static int jvc_encode(enum rc_proto proto, unsigned scancode, unsigned *buf)
{
	const int jvc_unit = 525000;
	int i;

	/* swap bytes so address comes first */
	scancode = ((scancode << 8) & 0xff00) | ((scancode >> 8) & 0x00ff);

	*buf++ = NS_TO_US(jvc_unit * 16);
	*buf++ = NS_TO_US(jvc_unit * 8);

	for (i=0; i<16; i++) {
		*buf++ = NS_TO_US(jvc_unit);

		if (scancode & 1)
			*buf++ = NS_TO_US(jvc_unit * 3);
		else
			*buf++ = NS_TO_US(jvc_unit);

		scancode >>= 1;
	}

	*buf = NS_TO_US(jvc_unit);

	return 35;
}

static int sanyo_encode(enum rc_proto proto, unsigned scancode, unsigned *buf)
{
	const int sanyo_unit = 562500;

	void add_bits(int bits, int count)
	{
		int i;
		for (i=0; i<count; i++) {
			*buf++ = NS_TO_US(sanyo_unit);

			if (bits & (1 << i))
				*buf++ = NS_TO_US(sanyo_unit * 3);
			else
				*buf++ = NS_TO_US(sanyo_unit);
		}
	}

	*buf++ = NS_TO_US(sanyo_unit * 16);
	*buf++ = NS_TO_US(sanyo_unit * 8);

	add_bits(scancode >> 8, 13);
	add_bits(~(scancode >> 8), 13);
	add_bits(scancode, 8);
	add_bits(~scancode, 8);

	*buf = NS_TO_US(sanyo_unit);

	return 87;
}

static int sharp_encode(enum rc_proto proto, unsigned scancode, unsigned *buf)
{
	const int sharp_unit = 40000;

	void add_bits(int bits, int count)
	{
		int i;
		for (i=0; i<count; i++) {
			*buf++ = NS_TO_US(sharp_unit * 8);

			if (bits & (1 << i))
				*buf++ = NS_TO_US(sharp_unit * 50);
			else
				*buf++ = NS_TO_US(sharp_unit * 25);
		}
	}

	add_bits(scancode >> 8, 5);
	add_bits(scancode, 8);
	add_bits(1, 2);

	*buf++ = NS_TO_US(sharp_unit * 8);
	*buf++ = NS_TO_US(sharp_unit * 1000);

	add_bits(scancode >> 8, 5);
	add_bits(~scancode, 8);
	add_bits(~1, 2);
	*buf++ = NS_TO_US(sharp_unit * 8);

	return (13 + 2) * 4 + 3;
}

static int sony_encode(enum rc_proto proto, unsigned scancode, unsigned *buf)
{
	const int sony_unit = 600000;
	int n = 0;

	void add_bits(int bits, int count)
	{
		int i;
		for (i=0; i<count; i++) {
			if (bits & (1 << i))
				buf[n++] = NS_TO_US(sony_unit * 2);
			else
				buf[n++] = NS_TO_US(sony_unit);

			buf[n++] = NS_TO_US(sony_unit);
		}
	}

	buf[n++] = NS_TO_US(sony_unit * 4);
	buf[n++] = NS_TO_US(sony_unit);

	switch (proto) {
	case RC_PROTO_SONY12:
		add_bits(scancode, 7);
		add_bits(scancode >> 16, 5);
		break;
	case RC_PROTO_SONY15:
		add_bits(scancode, 7);
		add_bits(scancode >> 16, 8);
		break;
	case RC_PROTO_SONY20:
		add_bits(scancode, 7);
		add_bits(scancode >> 16, 5);
		add_bits(scancode >> 8, 8);
		break;
	default:
		return 0;
	}

	/* ignore last space */
	return n - 1;
}

static int rc5_encode(enum rc_proto proto, unsigned scancode, unsigned *buf)
{
	const unsigned int rc5_unit = 888888;
	unsigned n = 0;

	void advance_space(unsigned length)
	{
		if (n % 2)
			buf[n] += length;
		else
			buf[++n] = length;
	}

	void advance_pulse(unsigned length)
	{
		if (n % 2)
			buf[++n] = length;
		else
			buf[n] += length;
	}

	void add_bits(int bits, int count)
	{
		while (count--) {
			if (bits & (1 << count)) {
				advance_space(NS_TO_US(rc5_unit));
				advance_pulse(NS_TO_US(rc5_unit));
			} else {
				advance_pulse(NS_TO_US(rc5_unit));
				advance_space(NS_TO_US(rc5_unit));
			}
		}
	}

	buf[n] = NS_TO_US(rc5_unit);

	switch (proto) {
	default:
		return 0;
	case RC_PROTO_RC5:
		add_bits(!(scancode & 0x40), 1);
		add_bits(0, 1);
		add_bits(scancode >> 8, 5);
		add_bits(scancode, 6);
		break;
	case RC_PROTO_RC5_SZ:
		add_bits(!!(scancode & 0x2000), 1);
		add_bits(0, 1);
		add_bits(scancode >> 6, 6);
		add_bits(scancode, 6);
		break;
	case RC_PROTO_RC5X_20:
		add_bits(!(scancode & 0x4000), 1);
		add_bits(0, 1);
		add_bits(scancode >> 16, 5);
		advance_space(NS_TO_US(rc5_unit * 4));
		add_bits(scancode >> 8, 6);
		add_bits(scancode, 6);
		break;
	}

	/* drop any trailing pulse */
	return (n % 2) ? n : n + 1;
}

static int rc6_encode(enum rc_proto proto, unsigned scancode, unsigned *buf)
{
	const unsigned int rc6_unit = 444444;
	unsigned n = 0;

	void advance_space(unsigned length)
	{
		if (n % 2)
			buf[n] += length;
		else
			buf[++n] = length;
	}

	void advance_pulse(unsigned length)
	{
		if (n % 2)
			buf[++n] = length;
		else
			buf[n] += length;
	}

	void add_bits(unsigned bits, unsigned count, unsigned length)
	{
		while (count--) {
			if (bits & (1 << count)) {
				advance_pulse(length);
				advance_space(length);
			} else {
				advance_space(length);
				advance_pulse(length);
			}
		}
	}

	buf[n++] = NS_TO_US(rc6_unit * 6);
	buf[n++] = NS_TO_US(rc6_unit * 2);
	buf[n] = 0;

	switch (proto) {
	default:
		return 0;
	case RC_PROTO_RC6_0:
		add_bits(8, 4, NS_TO_US(rc6_unit));
		add_bits(0, 1, NS_TO_US(rc6_unit * 2));
		add_bits(scancode, 16, NS_TO_US(rc6_unit));
		break;
	case RC_PROTO_RC6_6A_20:
		add_bits(14, 4, NS_TO_US(rc6_unit));
		add_bits(0, 1, NS_TO_US(rc6_unit * 2));
		add_bits(scancode, 20, NS_TO_US(rc6_unit));
		break;
	case RC_PROTO_RC6_6A_24:
		add_bits(14, 4, NS_TO_US(rc6_unit));
		add_bits(0, 1, NS_TO_US(rc6_unit * 2));
		add_bits(scancode, 24, NS_TO_US(rc6_unit));
		break;
	case RC_PROTO_RC6_6A_32:
	case RC_PROTO_RC6_MCE:
		add_bits(14, 4, NS_TO_US(rc6_unit));
		add_bits(0, 1, NS_TO_US(rc6_unit * 2));
		add_bits(scancode, 32, NS_TO_US(rc6_unit));
		break;
	}

	/* drop any trailing pulse */
	return (n % 2) ? n : n + 1;
}

static const struct {
	char name[10];
	unsigned scancode_mask;
	unsigned max_edges;
	unsigned carrier;
	int (*encode)(enum rc_proto proto, unsigned scancode, unsigned *buf);
} encoders[RC_PROTO_COUNT] = {
	[RC_PROTO_RC5] = { "rc5", 0x1f7f, 24, 36000, rc5_encode },
	[RC_PROTO_RC5X_20] = { "rc5x_20", 0x1f7f3f, 40, 36000, rc5_encode },
	[RC_PROTO_RC5_SZ] = { "rc5_sz", 0x2fff, 26, 36000, rc5_encode },
	[RC_PROTO_SONY12] = { "sony12", 0x1f007f, 25, 40000, sony_encode },
	[RC_PROTO_SONY15] = { "sony15", 0xff007f, 31, 40000, sony_encode },
	[RC_PROTO_SONY20] = { "sony20", 0x1fff7f, 41, 40000, sony_encode },
	[RC_PROTO_JVC] = { "jvc", 0xffff, 35, 38000, jvc_encode },
	[RC_PROTO_NEC] = { "nec", 0xffff, 67, 38000, nec_encode },
	[RC_PROTO_NECX] = { "necx", 0xffffff, 67, 38000, nec_encode },
	[RC_PROTO_NEC32] = { "nec32", 0xffffffff, 67, 38000, nec_encode },
	[RC_PROTO_SANYO] = { "sanyo", 0x1fffff, 87, 38000, sanyo_encode },
	[RC_PROTO_RC6_0] = { "rc6_0", 0xffff, 24, 36000, rc6_encode },
	[RC_PROTO_RC6_6A_20] = { "rc6_6a_20", 0xfffff, 52, 36000, rc6_encode },
	[RC_PROTO_RC6_6A_24] = { "rc6_6a_24", 0xffffff, 60, 36000, rc6_encode },
	[RC_PROTO_RC6_6A_32] = { "rc6_6a_32", 0xffffffff, 76, 36000, rc6_encode },
	[RC_PROTO_RC6_MCE] = { "rc6_mce", 0xffff7fff, 76, 36000, rc6_encode },
	[RC_PROTO_SHARP] = { "sharp", 0x1fff, 63, 38000, sharp_encode },
};

static bool str_like(const char *a, const char *b)
{
	while (*a && *b) {
		while (*a == ' ' || *a == '-' || *a == '_')
			a++;
		while (*b == ' ' || *b == '-' || *b == '_')
			b++;

		if (*a >= 0x7f || *b >= 0x7f)
			return false;

		if (tolower(*a) != tolower(*b))
			return false;

		a++; b++;
	}

	return !*a && !*b;
}

bool protocol_match(const char *name, enum rc_proto *proto)
{
	enum rc_proto p;

	for (p=0; p<RC_PROTO_COUNT; p++) {
		if (str_like(encoders[p].name, name)) {
			*proto = p;
			return true;
		}
	}

	return false;
}

unsigned protocol_carrier(enum rc_proto proto)
{
	return encoders[proto].carrier;
}

unsigned protocol_max_size(enum rc_proto proto)
{
	return encoders[proto].max_edges;
}

unsigned protocol_scancode_mask(enum rc_proto proto)
{
	return encoders[proto].scancode_mask;
}

bool protocol_scancode_valid(enum rc_proto p, unsigned s)
{
	if (s & ~encoders[p].scancode_mask)
		return false;

	if (p == RC_PROTO_NECX) {
		return (((s >> 16) ^ ~(s >> 8)) & 0xff) != 0;
	} else if (p == RC_PROTO_NEC32) {
		return (((s >> 24) ^ ~(s >> 16)) & 0xff) != 0;
	} else if (p == RC_PROTO_RC6_MCE) {
		return (s & 0xffff0000) == 0x800f0000;
	} else if (p == RC_PROTO_RC6_6A_32) {
		return (s & 0xffff0000) != 0x800f0000;
	}

	return true;
}

unsigned protocol_encode(enum rc_proto proto, unsigned scancode, unsigned *buf)
{
	return encoders[proto].encode(proto, scancode, buf);
}

const char* protocol_name(enum rc_proto proto)
{
	return encoders[proto].name;
}
