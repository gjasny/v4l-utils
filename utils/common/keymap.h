/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __KEYMAP_H
#define __KEYMAP_H

struct keymap {
	struct keymap *next;
	char *name;
	char *protocol;
	char *variant;
	struct protocol_param *param;
	struct scancode_entry *scancode;
	struct raw_entry *raw;
};

struct protocol_param {
	struct protocol_param *next;
	char *name;
	long int value;
};

struct scancode_entry {
	struct scancode_entry *next;
	u_int32_t scancode;
	char *keycode;
};

struct raw_entry {
	struct raw_entry *next;
	u_int32_t scancode;
	u_int32_t raw_length;
	char *keycode;
	u_int32_t raw[1];
};

void free_keymap(struct keymap *map);
error_t parse_keymap(char *fname, struct keymap **keymap, bool verbose);
int keymap_param(struct keymap *map, const char *name, int fallback);

#endif
