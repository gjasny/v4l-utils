/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __KEYMAP_H
#define __KEYMAP_H

struct raw_entry {
	struct raw_entry *next;
	u_int32_t scancode;
	u_int32_t raw_length;
	u_int32_t raw[1];
};

#endif
