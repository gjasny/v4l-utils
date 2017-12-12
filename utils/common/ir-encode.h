
#ifndef __IR_ENCODE_H__
#define __IR_ENCODE_H__

#define ARRAY_SIZE(x)     (sizeof(x)/sizeof((x)[0]))

bool protocol_match(const char *name, enum rc_proto *proto);
unsigned protocol_carrier(enum rc_proto proto);
unsigned protocol_max_size(enum rc_proto proto);
bool protocol_scancode_valid(enum rc_proto proto, unsigned scancode);
unsigned protocol_scancode_mask(enum rc_proto proto);
bool protocol_encoder_available(enum rc_proto proto);
unsigned protocol_encode(enum rc_proto proto, unsigned scancode, unsigned *buf);
const char *protocol_name(enum rc_proto proto);

#endif
