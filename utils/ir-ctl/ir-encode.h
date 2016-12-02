
#ifndef __IR_ENCODE_H__
#define __IR_ENCODE_H__

enum rc_proto {
	RC_PROTO_RC5,
	RC_PROTO_RC5X_20,
	RC_PROTO_RC5_SZ,
	RC_PROTO_JVC,
	RC_PROTO_SONY12,
	RC_PROTO_SONY15,
	RC_PROTO_SONY20,
	RC_PROTO_NEC,
	RC_PROTO_NECX,
	RC_PROTO_NEC32,
	RC_PROTO_SANYO,
	RC_PROTO_MCE_KBD,
	RC_PROTO_RC6_0,
	RC_PROTO_RC6_6A_20,
	RC_PROTO_RC6_6A_24,
	RC_PROTO_RC6_6A_32,
	RC_PROTO_RC6_MCE,
	RC_PROTO_SHARP,
	RC_PROTO_XMP,
	RC_PROTO_COUNT
};

bool protocol_match(const char *name, enum rc_proto *proto);
unsigned protocol_carrier(enum rc_proto proto);
unsigned protocol_max_size(enum rc_proto proto);
bool protocol_scancode_valid(enum rc_proto proto, unsigned scancode);
unsigned protocol_scancode_mask(enum rc_proto proto);
unsigned protocol_encode(enum rc_proto proto, unsigned scancode, unsigned *buf);
const char *protocol_name(enum rc_proto proto);

#endif
