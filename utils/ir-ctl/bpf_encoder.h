/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __BPF_ENCODER_H
#define __BPF_ENCODER_H

bool encode_bpf_protocol(struct keymap *map, uint32_t scancode, int *buf, int *length);

#endif
