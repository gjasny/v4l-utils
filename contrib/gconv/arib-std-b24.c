/* Conversion module for ARIB-STD-B24.
   Copyright (C) 1998-2014 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

/*
 * Conversion module for the character encoding
 * defined in ARIB STD-B24 Volume 1, Part 2, Chapter 7.
 *    http://www.arib.or.jp/english/html/overview/doc/6-STD-B24v5_2-1p3-E1.pdf
 *    http://www.arib.or.jp/english/html/overview/sb_ej.html
 *    https://sites.google.com/site/unicodesymbols/Home/japanese-tv-symbols/
 * It is based on ISO-2022, and used in Japanese digital televsion.
 *
 * Note 1: "mosaic" characters are not supported in this module.
 * Note 2: Control characters (for subtitles) are discarded.
 */

#include <assert.h>
#include <dlfcn.h>
#include <gconv.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "jis0201.h"
#include "jis0208.h"
#include "jisx0213.h"

/* Definitions used in the body of the `gconv' function.  */
#define CHARSET_NAME		"ARIB-STD-B24//"
#define DEFINE_INIT		1
#define DEFINE_FINI		1
#define ONE_DIRECTION		0
#define FROM_LOOP		from_aribb24_loop
#define TO_LOOP			to_aribb24_loop
#define FROM_LOOP_MIN_NEEDED_FROM 1
#define FROM_LOOP_MAX_NEEDED_FROM 1
#define FROM_LOOP_MIN_NEEDED_TO 4
#define FROM_LOOP_MAX_NEEDED_TO (4 * 4)
#define TO_LOOP_MIN_NEEDED_FROM 4
#define TO_LOOP_MAX_NEEDED_FROM 4
#define TO_LOOP_MIN_NEEDED_TO 1
#define TO_LOOP_MAX_NEEDED_TO 7

#define PREPARE_LOOP \
  __mbstate_t saved_state;						      \
  __mbstate_t *statep = data->__statep;					      \
  status = __GCONV_OK;

/* Since we might have to reset input pointer we must be able to save
   and retore the state.  */
#define SAVE_RESET_STATE(Save) \
  {									      \
    if (Save)								      \
      saved_state = *statep;						      \
    else								      \
      *statep = saved_state;						      \
  }

/* During UCS-4 to ARIB-STD-B24 conversion, the state contains the last
   two bytes to be output, in .prev member. */

/* Since this is a stateful encoding we have to provide code which resets
   the output state to the initial state.  This has to be done during the
   flushing.  */
#define EMIT_SHIFT_TO_INIT \
  {									      \
    if (!FROM_DIRECTION)						      \
      status = out_buffered((struct state_to *) data->__statep,		      \
			    &outbuf, outend);				      \
    /* we don't have to emit anything, just reset the state.  */	      \
    memset (data->__statep, '\0', sizeof (*data->__statep));		      \
  }


/* This makes obvious what everybody knows: 0x1b is the Esc character.  */
#define ESC 0x1b
/* other control characters */
#define SS2 0x19
#define SS3 0x1d
#define LS0 0x0f
#define LS1 0x0e

#define LS2 0x6e
#define LS3 0x6f
#define LS1R 0x7e
#define LS2R 0x7d
#define LS3R 0x7c

#define LF 0x0a
#define CR 0x0d
#define BEL 0x07
#define BS 0x08
#define COL 0x90
#define CDC 0x92
#define MACRO_CTRL 0x95
#define CSI 0x9b
#define TIME 0x9d

/* code sets */
enum g_set
{
  KANJI_set = '\x42',         /* 2Byte set */
  ASCII_set = '\x40',
  ASCII_x_set = '\x4a',
  HIRAGANA_set = '\x30',
  KATAKANA_set = '\x31',
  MOSAIC_A_set = '\x32',
  MOSAIC_B_set = '\x33',
  MOSAIC_C_set = '\x34',
  MOSAIC_D_set = '\x35',
  PROP_ASCII_set = '\x36',
  PROP_HIRA_set = '\x37',
  PROP_KATA_set = '\x38',
  JIS0201_KATA_set = '\x49',
  JISX0213_1_set = '\x39',    /* 2Byte set */
  JISX0213_2_set = '\x3a',    /* 2Byte set */
  EXTRA_SYMBOLS_set = '\x3b', /* 2Byte set */

  DRCS0_set = 0x40 | 0x80,    /* 2Byte set */
  DRCS1_set = 0x41 | 0x80,
  DRCS15_set = 0x4f | 0x80,
  MACRO_set = 0x70 | 0x80,
};


/* First define the conversion function from ARIB-STD-B24 to UCS-4.  */

enum mode_e
{
  NORMAL,
  ESCAPE,
  G_SEL_1B,
  G_SEL_MB,
  CTRL_SEQ,
  DESIGNATE_MB,
  DRCS_SEL_1B,
  DRCS_SEL_MB,
  MB_2ND,
};

/*
 * __GCONV_INPUT_INCOMPLETE is never used in this conversion, thus
 * we can re-use mbstate_t.__value and .__count:3 for the other purpose.
 */
struct state_from {
  /* __count */
  uint8_t cnt:3;	/* for use in skelton.c. always 0 */
  uint8_t pad0:1;
  uint8_t gl:2;		/* idx of the G-set invoked into GL */
  uint8_t gr:2;		/*  ... to GR */
  uint8_t ss:2;		/* SS state. 0: no shift, 2:SS2, 3:SS3 */
  uint8_t gidx:2;	/* currently designated G-set */
  uint8_t mode:4;	/* current input mode. see below. */
  uint8_t skip;		/* [CTRL_SEQ] # of char to skip */
  uint8_t prev;		/* previously input char [in MB_2ND] or,*/
			/* input char to wait for. [CTRL_SEQ (.skip == 0)] */

  /* __value */
  uint8_t g[4];		/* code set for G0..G3 */
} __attribute__((packed));

static const struct state_from def_state_from = {
  .cnt = 0,
  .gl = 0,
  .gr = 2,
  .ss = 0,
  .gidx = 0,
  .mode = NORMAL,
  .skip = 0,
  .prev = '\0',
  .g[0] = KANJI_set,
  .g[1] = ASCII_set,
  .g[2] = HIRAGANA_set,
  .g[3] = KATAKANA_set,
};

#define EXTRA_LOOP_DECLS	, __mbstate_t *statep
#define EXTRA_LOOP_ARGS		, statep

#define INIT_PARAMS \
  struct state_from st = *((struct state_from *)statep);		      \
  if (st.g[0] == 0)							      \
    st = def_state_from;

#define UPDATE_PARAMS		*statep = *((__mbstate_t *)&st)

#define LOOP_NEED_FLAGS

#define MIN_NEEDED_INPUT	FROM_LOOP_MIN_NEEDED_FROM
#define MAX_NEEDED_INPUT	FROM_LOOP_MAX_NEEDED_FROM
#define MIN_NEEDED_OUTPUT	FROM_LOOP_MIN_NEEDED_TO
#define MAX_NEEDED_OUTPUT	FROM_LOOP_MAX_NEEDED_TO
#define LOOPFCT			FROM_LOOP

/* tables and functions used in BODY */

static const uint16_t kata_punc[] = {
  0x30fd, 0x30fe, 0x30fc, 0x3002, 0x300c, 0x300d, 0x3001, 0x30fb
};

static const uint16_t hira_punc[] = {
  0x309d, 0x309e
};

static const uint16_t nonspacing_symbol[] = {
  0x0301, 0x0300, 0x0308, 0x0302, 0x0304, 0x0332
};

static const uint32_t extra_kanji[] = {
  /* row 85 */
  /* col 0..15 */
  0, 0x3402, 0x20158, 0x4efd, 0x4eff, 0x4f9a, 0x4fc9, 0x509c,
  0x511e, 0x51bc, 0x351f, 0x5307, 0x5361, 0x536c, 0x8a79, 0x20bb7,
  /* col. 16..31 */
  0x544d, 0x5496, 0x549c, 0x54a9, 0x550e, 0x554a, 0x5672, 0x56e4,
  0x5733, 0x5734, 0xfa10, 0x5880, 0x59e4, 0x5a23, 0x5a55, 0x5bec,
  /* col. 32..47 */
  0xfa11, 0x37e2, 0x5eac, 0x5f34, 0x5f45, 0x5fb7, 0x6017, 0xfa6b,
  0x6130, 0x6624, 0x66c8, 0x66d9, 0x66fa, 0x66fb, 0x6852, 0x9fc4,
  /* col. 48..63 */
  0x6911, 0x693b, 0x6a45, 0x6a91, 0x6adb, 0x233cc, 0x233fe, 0x235c4,
  0x6bf1, 0x6ce0, 0x6d2e, 0xfa45, 0x6dbf, 0x6dca, 0x6df8, 0xfa46,
  /* col. 64..79 */
  0x6f5e, 0x6ff9, 0x7064, 0xfa6c, 0x242ee, 0x7147, 0x71c1, 0x7200,
  0x739f, 0x73a8, 0x73c9, 0x73d6, 0x741b, 0x7421, 0xfa4a, 0x7426,
  /* col. 80..96 */
  0x742a, 0x742c, 0x7439, 0x744b, 0x3eda, 0x7575, 0x7581, 0x7772,
  0x4093, 0x78c8, 0x78e0, 0x7947, 0x79ae, 0x9fc6, 0x4103, 0,

  /* row 86 */
  /* col 0..15 */
  0, 0x9fc5, 0x79da, 0x7a1e, 0x7b7f, 0x7c31, 0x4264, 0x7d8b,
  0x7fa1, 0x8118, 0x813a, 0xfa6d, 0x82ae, 0x845b, 0x84dc, 0x84ec,
  /* col. 16..31 */
  0x8559, 0x85ce, 0x8755, 0x87ec, 0x880b, 0x88f5, 0x89d2, 0x8af6,
  0x8dce, 0x8fbb, 0x8ff6, 0x90dd, 0x9127, 0x912d, 0x91b2, 0x9233,
  /* col. 32..43 */
  0x9288, 0x9321, 0x9348, 0x9592, 0x96de, 0x9903, 0x9940, 0x9ad9,
  0x9bd6, 0x9dd7, 0x9eb4, 0x9eb5
};

static const uint32_t extra_symbols[5][96] = {
  /* row 90 */
  {
    /* col 0..15 */
    0, 0x26cc, 0x26cd, 0x2762, 0x26cf, 0x26d0, 0x26d1, 0,
    0x26d2, 0x26d5, 0x26d3, 0x26d4, 0, 0, 0, 0,
    /* col 16..31 */
    0x1f17f, 0x1f18a, 0, 0, 0x26d6, 0x26d7, 0x26d8, 0x26d9,
    0x26da, 0x26db, 0x26dc, 0x26dd, 0x26de, 0x26df, 0x26e0, 0x26e1,
    /* col 32..47 */
    0x2b55, 0x3248, 0x3249, 0x324a, 0x324b, 0x324c, 0x324d, 0x324e,
    0x324f, 0, 0, 0, 0, 0x2491, 0x2492, 0x2493,
    /* col 48..63 */
    0x1f14a, 0x1f14c, 0x1f13F, 0x1f146, 0x1f14b, 0x1f210, 0x1f211, 0x1f212,
    0x1f213, 0x1f142, 0x1f214, 0x1f215, 0x1f216, 0x1f14d, 0x1f131, 0x1f13d,
    /* col 64..79 */
    0x2b1b, 0x2b24, 0x1f217, 0x1f218, 0x1f219, 0x1f21a, 0x1f21b, 0x26bf,
    0x1f21c, 0x1f21d, 0x1f21e, 0x1f21f, 0x1f220, 0x1f221, 0x1f222, 0x1f223,
    /* col 80..95 */
    0x1f224, 0x1f225, 0x1f14e, 0x3299, 0x1f200, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
  },
  /* row 91 */
  {
    /* col 0..15 */
    0, 0x26e3, 0x2b56, 0x2b57, 0x2b58, 0x2b59, 0x2613, 0x328b,
    0x3012, 0x26e8, 0x3246, 0x3245, 0x26e9, 0x0fd6, 0x26ea, 0x26eb,
    /* col 16..31 */
    0x26ec, 0x2668, 0x26ed, 0x26ee, 0x26ef, 0x2693, 0x1f6e7, 0x26f0,
    0x26f1, 0x26f2, 0x26f3, 0x26f4, 0x26f5, 0x1f157, 0x24b9, 0x24c8,
    /* col 32..47 */
    0x26f6, 0x1f15f, 0x1f18b, 0x1f18d, 0x1f18c, 0x1f179, 0x26f7, 0x26f8,
    0x26f9, 0x26fa, 0x1f17b, 0x260e, 0x26fb, 0x26fc, 0x26fd, 0x26fe,
    /* col 48..63 */
    0x1f17c, 0x26ff,
  },
  /* row 92 */
  {
    /* col 0..15 */
    0, 0x27a1, 0x2b05, 0x2b06, 0x2b07, 0x2b2f, 0x2b2e, 0x5e74,
    0x6708, 0x65e5, 0x5186, 0x33a1, 0x33a5, 0x339d, 0x33a0, 0x33a4,
    /* col 16..31 */
    0x1f100, 0x2488, 0x2489, 0x248a, 0x248b, 0x248c, 0x248d, 0x248e,
    0x248f, 0x2490, 0, 0, 0, 0, 0, 0,
    /* col 32..47 */
    0x1f101, 0x1f102, 0x1f103, 0x1f104, 0x1f105, 0x1f106, 0x1f107, 0x1f108,
    0x1f109, 0x1f10a, 0x3233, 0x3236, 0x3232, 0x3231, 0x3239, 0x3244,
    /* col 48..63 */
    0x25b6, 0x25c0, 0x3016, 0x3017, 0x27d0, 0x00b2, 0x00b3, 0x1f12d,
    0, 0, 0, 0, 0, 0, 0, 0,
    /* col 64..79 */
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    /* col 80..95 */
    0, 0, 0, 0, 0, 0, 0x1f12c, 0x1f12b,
    0x3247, 0x1f190, 0x1f226, 0x213b, 0, 0, 0, 0
  },
  /* row 93 */
  {
    /* col 0..15 */
    0, 0x322a, 0x322b, 0x322c, 0x322d, 0x322e, 0x322f, 0x3230,
    0x3237, 0x337e, 0x337d, 0x337c, 0x337b, 0x2116, 0x2121, 0x3036,
    /* col 16..31 */
    0x26be, 0x1f240, 0x1f241, 0x1f242, 0x1f243, 0x1f244, 0x1f245, 0x1f246,
    0x1f247, 0x1f248, 0x1f12a, 0x1f227, 0x1f228, 0x1f229, 0x1f214, 0x1f22a,
    /* col 32..47 */
    0x1f22b, 0x1f22c, 0x1f22d, 0x1f22e, 0x1f22f, 0x1f230, 0x1f231, 0x2113,
    0x338f, 0x3390, 0x33ca, 0x339e, 0x33a2, 0x3371, 0, 0,
    /* col 48..63 */
    0x00bd, 0x2189, 0x2153, 0x2154, 0x00bc, 0x00be, 0x2155, 0x2156,
    0x2157, 0x2158, 0x2159, 0x215a, 0x2150, 0x215b, 0x2151, 0x2152,
    /* col 64..79 */
    0x2600, 0x2601, 0x2602, 0x26c4, 0x2616, 0x2617, 0x26c9, 0x26ca,
    0x2666, 0x2665, 0x2663, 0x2660, 0x26cb, 0x2a00, 0x203c, 0x2049,
    /* col 80..95 */
    0x26c5, 0x2614, 0x26c6, 0x2603, 0x26c7, 0x26a1, 0x26c8, 0,
    0x269e, 0x269f, 0x266c, 0x260e, 0, 0, 0, 0
  },
  /* row 94 */
  {
    /* col 0..15 */
    0, 0x2160, 0x2161, 0x2162, 0x2163, 0x2164, 0x2165, 0x2166,
    0x2167, 0x2168, 0x2169, 0x216a, 0x216b, 0x2470, 0x2471, 0x2472,
    /* col 16..31 */
    0x2473, 0x2474, 0x2475, 0x2476, 0x2477, 0x2478, 0x2479, 0x247a,
    0x247b, 0x247c, 0x247d, 0x247e, 0x247f, 0x3251, 0x3252, 0x3253,
    /* col 32..47 */
    0x3254, 0x1f110, 0x1f111, 0x1f112, 0x1f113, 0x1f114, 0x1f115, 0x1f116,
    0x1f117, 0x1f118, 0x1f119, 0x1f11a, 0x1f11b, 0x1f11c, 0x1f11d, 0x1f11e,
    /* col 48..63 */
    0x1f11f, 0x1f120, 0x1f121, 0x1f122, 0x1f123, 0x1f124, 0x1f125, 0x1f126,
    0x1f127, 0x1f128, 0x1f129, 0x3255, 0x3256, 0x3257, 0x3258, 0x3259,
    /* col 64..79 */
    0x325a, 0x2460, 0x2461, 0x2462, 0x2463, 0x2464, 0x2465, 0x2466,
    0x2467, 0x2468, 0x2469, 0x246a, 0x246b, 0x246c, 0x246d, 0x246e,
    /* col 80..95 */
    0x246f, 0x2776, 0x2777, 0x2778, 0x2779, 0x277a, 0x277b, 0x277c,
    0x277d, 0x277e, 0x277f, 0x24eb, 0x24ec, 0x325b, 0, 0
  },
};

struct mchar_entry {
  uint32_t len;
  uint32_t to[4];
};

/* list of transliterations. */

/* small/subscript-ish KANJI. map to the normal sized version */
static const struct mchar_entry ext_sym_smallk[] = {
  {.len = 1, .to = { 0x6c0f }},
  {.len = 1, .to = { 0x526f }},
  {.len = 1, .to = { 0x5143 }},
  {.len = 1, .to = { 0x6545 }},
  {.len = 1, .to = { 0x52ed }},
  {.len = 1, .to = { 0x65b0 }},
};

/* symbols of music instruments */
static const struct mchar_entry ext_sym_music[] = {
  {.len = 4, .to = { 0x0028, 0x0076, 0x006e, 0x0029 }},
  {.len = 4, .to = { 0x0028, 0x006f, 0x0062, 0x0029 }},
  {.len = 4, .to = { 0x0028, 0x0063, 0x0062, 0x0029 }},
  {.len = 3, .to = { 0x0028, 0x0063, 0x0065 }},
  {.len = 3, .to = { 0x006d, 0x0062, 0x0029 }},
  {.len = 4, .to = { 0x0028, 0x0068, 0x0070, 0x0029 }},
  {.len = 4, .to = { 0x0028, 0x0062, 0x0072, 0x0029 }},
  {.len = 3, .to = { 0x0028, 0x0070, 0x0029 }},

  {.len = 3, .to = { 0x0028, 0x0073, 0x0029 }},
  {.len = 4, .to = { 0x0028, 0x006d, 0x0073, 0x0029 }},
  {.len = 3, .to = { 0x0028, 0x0074, 0x0029 }},
  {.len = 4, .to = { 0x0028, 0x0062, 0x0073, 0x0029 }},
  {.len = 3, .to = { 0x0028, 0x0062, 0x0029 }},
  {.len = 4, .to = { 0x0028, 0x0074, 0x0062, 0x0029 }},
  {.len = 4, .to = { 0x0028, 0x0076, 0x0070, 0x0029 }},
  {.len = 4, .to = { 0x0028, 0x0064, 0x0073, 0x0029 }},

  {.len = 4, .to = { 0x0028, 0x0061, 0x0067, 0x0029 }},
  {.len = 4, .to = { 0x0028, 0x0065, 0x0067, 0x0029 }},
  {.len = 4, .to = { 0x0028, 0x0076, 0x006f, 0x0029 }},
  {.len = 4, .to = { 0x0028, 0x0066, 0x006c, 0x0029 }},
  {.len = 3, .to = { 0x0028, 0x006b, 0x0065 }},
  {.len = 2, .to = { 0x0079, 0x0029 }},
  {.len = 3, .to = { 0x0028, 0x0073, 0x0061 }},
  {.len = 2, .to = { 0x0078, 0x0029 }},

  {.len = 3, .to = { 0x0028, 0x0073, 0x0079 }},
  {.len = 2, .to = { 0x006e, 0x0029 }},
  {.len = 3, .to = { 0x0028, 0x006f, 0x0072 }},
  {.len = 2, .to = { 0x0067, 0x0029 }},
  {.len = 3, .to = { 0x0028, 0x0070, 0x0065 }},
  {.len = 2, .to = { 0x0072, 0x0029 }},
};


int
b24_char_conv (int set, unsigned char c1, unsigned char c2, uint32_t *out)
{
  int len;
  uint32_t ch;

  if (set > DRCS0_set && set <= DRCS15_set)
    set = DRCS0_set;

  switch (set)
    {
      case ASCII_set:
      case ASCII_x_set:
      case PROP_ASCII_set:
	if (c1 == 0x7e)
	  *out = 0x203e;
	else if (c1 == 0x5c)
	  *out = 0xa5;
	else
	  *out = c1;
	return 1;

      case KATAKANA_set:
      case PROP_KATA_set:
	if (c1 <= 0x76)
	  *out = 0x3080 + c1;
	else
	  *out = kata_punc[c1 - 0x77];
	return 1;

      case HIRAGANA_set:
      case PROP_HIRA_set:
	if (c1 <= 0x73)
	  *out = 0x3020 + c1;
	else if (c1 == 0x77 || c1 == 0x78)
	  *out = hira_punc[c1 - 0x77];
	else if (c1 >= 0x79)
	  *out = kata_punc[c1 - 0x77];
	else
	  return 0;
	return 1;

      case JIS0201_KATA_set:
	if (c1 > 0x5f)
	  return 0;
	*out = 0xff40 + c1;
	return 1;

      case EXTRA_SYMBOLS_set:
	if (c1 == 0x75 || (c1 == 0x76 && (c2 - 0x20) <=43))
	  {
	    *out = extra_kanji[(c1 - 0x75) * 96 + (c2 - 0x20)];
	    return 1;
	  }
	/* fall through */
      case KANJI_set:
	/* check extra symbols */
	if (c1 >= 0x7a && c1 <= 0x7e)
	  {
	    const struct mchar_entry *entry;

	    c1 -= 0x20;
	    c2 -= 0x20;
	    if (c1 == 0x5c && c2 >= 0x1a && c2 <= 0x1f)
	      entry = &ext_sym_smallk[c2 - 0x1a];
	    else if (c1 == 0x5c && c2 >= 0x38 && c2 <= 0x55)
	      entry = &ext_sym_music[c2 - 0x38];
	    else
	      entry = NULL;

	    if (entry)
	      {
		int i;

		for (i = 0; i < entry->len; i++)
		  out[i] = entry->to[i];
		return i;
	      }

	    *out = extra_symbols[c1 - 0x5a][c2];
	    if (*out == 0)
	      return 0;

	    return 1;
	  }
	if (set == EXTRA_SYMBOLS_set)
	  return 0;

	/* non-JISX0213 modification. (combining chars) */
	if (c1 == 0x22 && c2 == 0x7e)
	  {
	    *out = 0x20dd;
	    return 1;
	  }
	else if (c1 == 0x21 && c2 >= 0x2d && c2 <= 0x32)
	  {
	    *out = nonspacing_symbol[c2 - 0x2d];
	    return 1;
	  }
	/* fall through */
      case JISX0213_1_set:
      case JISX0213_2_set:
	len = 1;
	ch = jisx0213_to_ucs4(c1 | (set == JISX0213_2_set ? 0x0200 : 0x0100),
			      c2);
	if (ch == 0)
	  return 0;
	if (ch < 0x80)
	  {
	    len = 2;
	    out[0] = __jisx0213_to_ucs_combining[ch - 1][0];
	    out[1] = __jisx0213_to_ucs_combining[ch - 1][1];
	  }
	else
	  *out = ch;
	return len;

      case MOSAIC_A_set:
      case MOSAIC_B_set:
      case MOSAIC_C_set:
      case MOSAIC_D_set:
      case DRCS0_set:
      case MACRO_set:
	*out = __UNKNOWN_10646_CHAR;
	return 1;

      default:
	break;
    }

  return 0;
}

#define BODY \
  {									      \
    uint32_t ch = *inptr;						      \
									      \
    if (ch == 0)							      \
      {									      \
	st.mode = NORMAL;						      \
	++ inptr;							      \
	continue;							      \
      }									      \
    if (__glibc_unlikely (st.mode == CTRL_SEQ))				      \
      {									      \
	if (st.skip)							      \
	  {								      \
	    --st.skip;							      \
	    if (st.skip == 0)						      \
	      st.mode = NORMAL;						      \
	    if (ch < 0x40 || ch > 0x7f)					      \
	      STANDARD_FROM_LOOP_ERR_HANDLER (1);			      \
	  }								      \
	else if (st.prev == MACRO_CTRL)					      \
	  {								      \
	    if (ch == MACRO_CTRL)					      \
	      st.skip = 1;						      \
	    else if (ch == LF || ch == CR) {				      \
	      st = def_state_from;					      \
	      put32(outptr, ch);					      \
	      outptr += 4;						      \
	    }								      \
	  }								      \
	else if (st.prev == CSI && (ch == 0x5b || ch == 0x5c || ch == 0x6f))  \
	  st.mode = NORMAL;						      \
	else if (st.prev == TIME || st.prev == CSI)			      \
	  {								      \
	    if (ch == 0x20 || (st.prev == TIME && ch == 0x28))		      \
	      st.skip = 1;						      \
	    else if (!((st.prev == TIME && ch == 0x29)			      \
		       || ch == 0x3b || (ch >= 0x30 && ch <= 0x39)))	      \
	      {								      \
		st.mode = NORMAL;					      \
		STANDARD_FROM_LOOP_ERR_HANDLER (1);			      \
	      }								      \
	  }								      \
	else if (st.prev == COL || st.prev == CDC)			      \
	  {								      \
	    if (ch == 0x20)						      \
	      st.skip = 1;						      \
	    else							      \
	      {								      \
		st.mode = NORMAL;					      \
		if (ch < 0x40 || ch > 0x7f)				      \
		  STANDARD_FROM_LOOP_ERR_HANDLER (1);			      \
	      }								      \
	  }								      \
	++ inptr;							      \
	continue;							      \
      }									      \
									      \
    if (__glibc_unlikely (ch == LF))					      \
      {									      \
	st = def_state_from;						      \
	put32 (outptr, ch);						      \
	outptr += 4;							      \
	++ inptr;							      \
	continue;							      \
      }									      \
									      \
    if (__glibc_unlikely (st.mode == ESCAPE))				      \
      {									      \
	if (ch == LS2 || ch == LS3)					      \
	  {								      \
	    st.mode = NORMAL;						      \
	    st.gl = (ch == LS2) ? 2 : 3;				      \
	    st.ss = 0;							      \
	  }								      \
	else if (ch == LS1R || ch == LS2R || ch == LS3R)		      \
	  {								      \
	    st.mode = NORMAL;						      \
	    st.gr = (ch == LS1R) ? 1 : (ch == LS2R) ? 2 : 3;		      \
	    st.ss = 0;							      \
	  }								      \
	else if (ch == 0x24) 						      \
	  st.mode = DESIGNATE_MB;					      \
	else if (ch >= 0x28 && ch <= 0x2b)				      \
	  {								      \
	    st.mode = G_SEL_1B;						      \
	    st.gidx = ch - 0x28;					      \
	  }								      \
	else								      \
	  {								      \
	    st.mode = NORMAL;						      \
	    STANDARD_FROM_LOOP_ERR_HANDLER (1);				      \
	  }								      \
	++ inptr;							      \
	continue;							      \
      }									      \
									      \
    if (__glibc_unlikely (st.mode == DESIGNATE_MB))			      \
      {									      \
	if (ch == KANJI_set || ch == JISX0213_1_set || ch == JISX0213_2_set   \
	    || ch == EXTRA_SYMBOLS_set)					      \
	  {								      \
	    st.mode = NORMAL;						      \
	    st.g[0] = ch;						      \
	  }								      \
	else if (ch >= 0x28 && ch <= 0x2b)				      \
	  {								      \
	  st.mode = G_SEL_MB;						      \
	  st.gidx = ch - 0x28;						      \
	  }								      \
	else								      \
	  {								      \
	    st.mode = NORMAL;						      \
	    STANDARD_FROM_LOOP_ERR_HANDLER (1);				      \
	  }								      \
	++ inptr;							      \
	continue;							      \
      }									      \
									      \
    if (__glibc_unlikely (st.mode == G_SEL_1B))				      \
      {									      \
	if (ch == ASCII_set || ch == ASCII_x_set || ch == JIS0201_KATA_set    \
	    || (ch >= 0x30 && ch <= 0x38))				      \
	  {								      \
	    st.g[st.gidx] = ch;						      \
	    st.mode = NORMAL;						      \
	  }								      \
	else if (ch == 0x20)						      \
	    st.mode = DRCS_SEL_1B;					      \
	else								      \
	  {								      \
	    st.mode = NORMAL;						      \
	    STANDARD_FROM_LOOP_ERR_HANDLER (1);				      \
	  }								      \
	++ inptr;							      \
	continue;							      \
      }									      \
									      \
    if (__glibc_unlikely (st.mode == G_SEL_MB))				      \
      {									      \
	if (ch == KANJI_set || ch == JISX0213_1_set || ch == JISX0213_2_set   \
	    || ch == EXTRA_SYMBOLS_set)					      \
	  {								      \
	    st.g[st.gidx] = ch;						      \
	    st.mode = NORMAL;						      \
	  }								      \
	else if (ch == 0x20)						      \
	  st.mode = DRCS_SEL_MB;					      \
	else								      \
	  {								      \
	    st.mode = NORMAL;						      \
	    STANDARD_FROM_LOOP_ERR_HANDLER (1);				      \
	  }								      \
	++ inptr;							      \
	continue;							      \
      }									      \
									      \
    if (__glibc_unlikely (st.mode == DRCS_SEL_1B))			      \
      {									      \
	st.mode = NORMAL;						      \
	if (ch == 0x70 || (ch >= 0x41 && ch <= 0x4f))			      \
	  st.g[st.gidx] = ch | 0x80;					      \
	else								      \
	  STANDARD_FROM_LOOP_ERR_HANDLER (1);				      \
	++ inptr;							      \
	continue;							      \
      }									      \
									      \
    if (__glibc_unlikely (st.mode == DRCS_SEL_MB))			      \
      {									      \
	st.mode = NORMAL;						      \
	if (ch == 0x40)							      \
	  st.g[st.gidx] = ch | 0x80;					      \
	else								      \
	  STANDARD_FROM_LOOP_ERR_HANDLER (1);				      \
	++ inptr;							      \
	continue;							      \
      }									      \
									      \
    if (st.mode == MB_2ND)						      \
      {									      \
	int gidx;							      \
	int i, len;							      \
	uint32_t out[MAX_NEEDED_OUTPUT];				      \
									      \
	gidx = (st.ss) ? st.ss : (ch & 0x80) ? st.gr : st.gl;		      \
	st.mode = NORMAL;						      \
	st.ss = 0;							      \
	if (__glibc_unlikely (!(ch & 0x60))) /* C0/C1 */		      \
	  STANDARD_FROM_LOOP_ERR_HANDLER (1);				      \
	if (__glibc_unlikely (st.ss > 0 && (ch & 0x80)))		      \
	  STANDARD_FROM_LOOP_ERR_HANDLER (1);				      \
	if (__glibc_unlikely ((st.prev & 0x80) != (ch & 0x80)))		      \
	  STANDARD_FROM_LOOP_ERR_HANDLER (1);				      \
	len = b24_char_conv(st.g[gidx], (st.prev & 0x7f), (ch & 0x7f), out);  \
	if (len == 0)							      \
	  STANDARD_FROM_LOOP_ERR_HANDLER (1);				      \
	if (outptr + 4 * len > outend)					      \
	  {								      \
	    result = __GCONV_FULL_OUTPUT;				      \
	    break;							      \
	  }								      \
	for (i = 0; i < len; i++)					      \
	  {								      \
	    if (irreversible						      \
		&& __builtin_expect (out[i] == __UNKNOWN_10646_CHAR, 0))      \
	      ++ *irreversible;						      \
	    put32 (outptr, out[i]);					      \
	    outptr += 4;						      \
	  }								      \
	++ inptr;							      \
	continue;							      \
      }									      \
									      \
    if (st.mode == NORMAL)						      \
      {									      \
	int gidx, set;							      \
									      \
	if (__glibc_unlikely (!(ch & 0x60))) /* C0/C1 */		      \
	  {								      \
	    if (ch == ESC)						      \
	      st.mode = ESCAPE;						      \
	    else if (ch == SS2)						      \
	      st.ss = 2;						      \
	    else if (ch == SS3)						      \
	      st.ss = 3;						      \
	    else if (ch == LS0)						      \
	      {								      \
		st.ss = 0;						      \
		st.gl = 0;						      \
	      }								      \
	    else if (ch == LS1)						      \
	      {								      \
		st.ss = 0;						      \
		st.gl = 1;						      \
	      }								      \
	    else if (ch == BEL || ch == BS || ch == CR)			      \
	      {								      \
		st.ss = 0;						      \
		put32 (outptr, ch);					      \
		outptr += 4;						      \
	      }								      \
	    else if (ch == 0x09 || ch == 0x0b || ch == 0x0c || ch == 0x18     \
		     || ch == 0x1e || ch == 0x1f || (ch >= 0x80 && ch <= 0x8a)\
		     || ch == 0x99 || ch == 0x9a)			      \
	      {								      \
		/* do nothing. just skip */				      \
	      }								      \
	    else if (ch == 0x16 || ch == 0x8b || ch == 0x91 || ch == 0x93     \
		     || ch == 0x94 || ch == 0x97 || ch == 0x98)		      \
	      {								      \
		st.mode = CTRL_SEQ;					      \
		st.skip = 1;						      \
	      }								      \
	    else if (ch == 0x1c)					      \
	      {								      \
		st.mode = CTRL_SEQ;					      \
		st.skip = 2;						      \
	      }								      \
	    else if (ch == COL || ch == CDC || ch == MACRO_CTRL		      \
		     || ch == CSI ||ch == TIME)				      \
	      {								      \
		st.mode = CTRL_SEQ;					      \
		st.skip = 0;						      \
		st.prev = ch;						      \
	      }								      \
	    else							      \
	      STANDARD_FROM_LOOP_ERR_HANDLER (1);			      \
									      \
	    ++ inptr;							      \
	    continue;							      \
	  }								      \
									      \
	if (__glibc_unlikely ((ch & 0x7f) == 0x20 || ch == 0x7f))	      \
	  {								      \
	    st.ss = 0;							      \
	    put32 (outptr, ch);						      \
	    outptr += 4;						      \
	    ++ inptr;							      \
	    continue;							      \
	  }								      \
	if (__glibc_unlikely (ch == 0xff))				      \
	  {								      \
	    st.ss = 0;							      \
	    put32 (outptr, __UNKNOWN_10646_CHAR);			      \
	    if (irreversible)						      \
	      ++ *irreversible;						      \
	    outptr += 4;						      \
	    ++ inptr;							      \
	    continue;							      \
	  }								      \
									      \
	if (__glibc_unlikely (st.ss > 0 && (ch & 0x80)))		      \
	  STANDARD_FROM_LOOP_ERR_HANDLER (1);				      \
									      \
	gidx = (st.ss) ? st.ss : (ch & 0x80) ? st.gr : st.gl;		      \
	set = st.g[gidx];						      \
	if (set == DRCS0_set || set == KANJI_set || set == JISX0213_1_set     \
	    || set == JISX0213_2_set || set == EXTRA_SYMBOLS_set)	      \
	  {								      \
	    st.mode = MB_2ND;						      \
	    st.prev = ch;						      \
	  }								      \
	else								      \
	  {								      \
	    uint32_t out;						      \
									      \
	    st.ss = 0;							      \
	    if (b24_char_conv(set, (ch & 0x7f), 0, &out) == 0)		      \
	      STANDARD_FROM_LOOP_ERR_HANDLER (1);			      \
	    if (out == __UNKNOWN_10646_CHAR && irreversible)		      \
	      ++ *irreversible;						      \
	    put32 (outptr, out);					      \
	    outptr += 4;						      \
	  }								      \
	++ inptr;							      \
	continue;							      \
      }									      \
  }
#include <iconv/loop.c>


/* Next, define the other direction, from UCS-4 to ARIB-STD-B24.  */

/* As MIN_INPUT is 4 (> 1), .cnt & .value must be put aside for skeleton.c.
 * To reduce the size of the state and fit into mbstate_t,
 * put constraints on G-set that can be locking-shift'ed to GL/GR.
 * GL is limited to invoke G0/G1, GR to G2/G3. i.e. LS2,LS3, LS1R are not used.
 * G0 is fixed to KANJI, G1 to ASCII.
 * G2 can be either HIRAGANA/JISX0213_{1,2},
 * G3 can be either KATAKANA/JISX0201_KATA/EXTRA_SYMBOLS.
 * JISX0213_{1,2},EXTRA_SYMBOLS are invoked into GR by SS2/SS3
 * if it is not already invoked to GR.
 * plus, charset is referenced by an index instead of its designation char.
 */
enum gset_idx {
  KANJI_idx,
  ASCII_idx,
  HIRAGANA_idx,
  KATAKANA_idx,
  JIS0201_KATA_idx,
  JISX0213_1_idx,
  JISX0213_2_idx,
  EXTRA_SYMBOLS_idx,
};

struct state_to {
  /* __count */
  uint32_t cnt:3;	/* for use in skelton.c.*/
  uint32_t gl:1;	/* 0: GL<-G0, 1: GL<-G1 */
  uint32_t gr:1;	/* 0: GR<-G2, 1: GR<-G3 */
  uint32_t g2:3;	/* Gset idx which is designated to G0 */
  uint32_t g3:3;	/* same to G1 */
  uint32_t prev:21;	/* previously input, combining char (for JISX0213) */

  /* __value */
  uint32_t __value;	/* used in skeleton.c */
} __attribute__((packed));

static const struct state_to def_state_to = {
  .cnt = 0,
  .gl = 0,
  .gr = 0,
  .g2 = HIRAGANA_idx,
  .g3 = KATAKANA_idx,
  .prev = 0,
  .__value = 0
};

#define EXTRA_LOOP_DECLS	, __mbstate_t *statep
#define EXTRA_LOOP_ARGS		, statep

#define INIT_PARAMS \
  struct state_to st = *((struct state_to *) statep);			      \
  if (st.g2 == 0)							      \
    st = def_state_to;							      \

#define UPDATE_PARAMS		*statep = *((__mbstate_t * )&st)
#define REINIT_PARAMS \
  do									      \
    {									      \
      st = *((struct state_to *) statep);				      \
      if (st.g2 == 0)							      \
	st = def_state_to;						      \
    }									      \
  while (0)

#define LOOP_NEED_FLAGS

#define MIN_NEEDED_INPUT	TO_LOOP_MIN_NEEDED_FROM
#define MAX_NEEDED_INPUT	TO_LOOP_MAX_NEEDED_FROM
#define MIN_NEEDED_OUTPUT	TO_LOOP_MIN_NEEDED_TO
#define MAX_NEEDED_OUTPUT	TO_LOOP_MAX_NEEDED_TO
#define LOOPFCT			TO_LOOP

/* tables and functions used in BODY */

/* Composition tables for each of the relevant combining characters.  */
static const struct
{
  uint16_t base;
  uint16_t composed;
} comp_table_data[] =
{
#define COMP_TABLE_IDX_02E5 0
#define COMP_TABLE_LEN_02E5 1
  { 0x2b64, 0x2b65 }, /* 0x12B65 = 0x12B64 U+02E5 */
#define COMP_TABLE_IDX_02E9 (COMP_TABLE_IDX_02E5 + COMP_TABLE_LEN_02E5)
#define COMP_TABLE_LEN_02E9 1
  { 0x2b60, 0x2b66 }, /* 0x12B66 = 0x12B60 U+02E9 */
#define COMP_TABLE_IDX_0300 (COMP_TABLE_IDX_02E9 + COMP_TABLE_LEN_02E9)
#define COMP_TABLE_LEN_0300 5
  { 0x295c, 0x2b44 }, /* 0x12B44 = 0x1295C U+0300 */
  { 0x2b38, 0x2b48 }, /* 0x12B48 = 0x12B38 U+0300 */
  { 0x2b37, 0x2b4a }, /* 0x12B4A = 0x12B37 U+0300 */
  { 0x2b30, 0x2b4c }, /* 0x12B4C = 0x12B30 U+0300 */
  { 0x2b43, 0x2b4e }, /* 0x12B4E = 0x12B43 U+0300 */
#define COMP_TABLE_IDX_0301 (COMP_TABLE_IDX_0300 + COMP_TABLE_LEN_0300)
#define COMP_TABLE_LEN_0301 4
  { 0x2b38, 0x2b49 }, /* 0x12B49 = 0x12B38 U+0301 */
  { 0x2b37, 0x2b4b }, /* 0x12B4B = 0x12B37 U+0301 */
  { 0x2b30, 0x2b4d }, /* 0x12B4D = 0x12B30 U+0301 */
  { 0x2b43, 0x2b4f }, /* 0x12B4F = 0x12B43 U+0301 */
#define COMP_TABLE_IDX_309A (COMP_TABLE_IDX_0301 + COMP_TABLE_LEN_0301)
#define COMP_TABLE_LEN_309A 14
  { 0x242b, 0x2477 }, /* 0x12477 = 0x1242B U+309A */
  { 0x242d, 0x2478 }, /* 0x12478 = 0x1242D U+309A */
  { 0x242f, 0x2479 }, /* 0x12479 = 0x1242F U+309A */
  { 0x2431, 0x247a }, /* 0x1247A = 0x12431 U+309A */
  { 0x2433, 0x247b }, /* 0x1247B = 0x12433 U+309A */
  { 0x252b, 0x2577 }, /* 0x12577 = 0x1252B U+309A */
  { 0x252d, 0x2578 }, /* 0x12578 = 0x1252D U+309A */
  { 0x252f, 0x2579 }, /* 0x12579 = 0x1252F U+309A */
  { 0x2531, 0x257a }, /* 0x1257A = 0x12531 U+309A */
  { 0x2533, 0x257b }, /* 0x1257B = 0x12533 U+309A */
  { 0x253b, 0x257c }, /* 0x1257C = 0x1253B U+309A */
  { 0x2544, 0x257d }, /* 0x1257D = 0x12544 U+309A */
  { 0x2548, 0x257e }, /* 0x1257E = 0x12548 U+309A */
  { 0x2675, 0x2678 }, /* 0x12678 = 0x12675 U+309A */
};

static const uint32_t ucs4_to_nonsp_kanji[][2] = {
  {0x20dd, 0x227e}, {0x0300, 0x212e}, {0x0301, 0x212d}, {0x0302, 0x2130},
  {0x0304, 0x2131}, {0x0308, 0x212f}, {0x0332, 0x2132}
};

static const uint32_t ucs4_to_extsym[][2] = {
  {0x00b2, 0x7c55}, {0x00b3, 0x7c56}, {0x00bc, 0x7d54}, {0x00bd, 0x7d50},
  {0x00be, 0x7d55}, {0x0fd6, 0x7b2d}, {0x203c, 0x7d6e}, {0x2049, 0x7d6f},
  {0x2113, 0x7d47}, {0x2116, 0x7d2d}, {0x2121, 0x7d2e}, {0x213b, 0x7c7b},
  {0x2150, 0x7d5c}, {0x2151, 0x7d5e}, {0x2152, 0x7d5f}, {0x2153, 0x7d52},
  {0x2154, 0x7d53}, {0x2155, 0x7d56}, {0x2156, 0x7d57}, {0x2157, 0x7d58},
  {0x2158, 0x7d59}, {0x2159, 0x7d5a}, {0x215a, 0x7d5b}, {0x215b, 0x7d5d},
  {0x2160, 0x7e21}, {0x2161, 0x7e22}, {0x2162, 0x7e23}, {0x2163, 0x7e24},
  {0x2164, 0x7e25}, {0x2165, 0x7e26}, {0x2166, 0x7e27}, {0x2167, 0x7e28},
  {0x2168, 0x7e29}, {0x2169, 0x7e2a}, {0x216a, 0x7e2b}, {0x216b, 0x7e2c},
  {0x2189, 0x7d51}, {0x2460, 0x7e61}, {0x2461, 0x7e62}, {0x2462, 0x7e63},
  {0x2463, 0x7e64}, {0x2464, 0x7e65}, {0x2465, 0x7e66}, {0x2466, 0x7e67},
  {0x2467, 0x7e68}, {0x2468, 0x7e69}, {0x2469, 0x7e6a}, {0x246a, 0x7e6b},
  {0x246b, 0x7e6c}, {0x246c, 0x7e6d}, {0x246d, 0x7e6e}, {0x246e, 0x7e6f},
  {0x246f, 0x7e70}, {0x2470, 0x7e2d}, {0x2471, 0x7e2e}, {0x2472, 0x7e2f},
  {0x2473, 0x7e30}, {0x2474, 0x7e31}, {0x2475, 0x7e32}, {0x2476, 0x7e33},
  {0x2477, 0x7e34}, {0x2478, 0x7e35}, {0x2479, 0x7e36}, {0x247a, 0x7e37},
  {0x247b, 0x7e38}, {0x247c, 0x7e39}, {0x247d, 0x7e3a}, {0x247e, 0x7e3b},
  {0x247f, 0x7e3c}, {0x2488, 0x7c31}, {0x2489, 0x7c32}, {0x248a, 0x7c33},
  {0x248b, 0x7c34}, {0x248c, 0x7c35}, {0x248d, 0x7c36}, {0x248e, 0x7c37},
  {0x248f, 0x7c38}, {0x2490, 0x7c39}, {0x2491, 0x7a4d}, {0x2492, 0x7a4e},
  {0x2493, 0x7a4f}, {0x24b9, 0x7b3e}, {0x24c8, 0x7b3f}, {0x24eb, 0x7e7b},
  {0x24ec, 0x7e7c}, {0x25b6, 0x7c50}, {0x25c0, 0x7c51}, {0x2600, 0x7d60},
  {0x2601, 0x7d61}, {0x2602, 0x7d62}, {0x2603, 0x7d73}, {0x260e, 0x7b4b},
  {0x260e, 0x7d7b}, {0x2613, 0x7b26}, {0x2614, 0x7d71}, {0x2616, 0x7d64},
  {0x2617, 0x7d65}, {0x2660, 0x7d6b}, {0x2663, 0x7d6a}, {0x2665, 0x7d69},
  {0x2666, 0x7d68}, {0x2668, 0x7b31}, {0x266c, 0x7d7a}, {0x2693, 0x7b35},
  {0x269e, 0x7d78}, {0x269f, 0x7d79}, {0x26a1, 0x7d75}, {0x26be, 0x7d30},
  {0x26bf, 0x7a67}, {0x26c4, 0x7d63}, {0x26c5, 0x7d70}, {0x26c6, 0x7d72},
  {0x26c7, 0x7d74}, {0x26c8, 0x7d76}, {0x26c9, 0x7d66}, {0x26ca, 0x7d67},
  {0x26cb, 0x7d6c}, {0x26cc, 0x7a21}, {0x26cd, 0x7a22}, {0x26cf, 0x7a24},
  {0x26d0, 0x7a25}, {0x26d1, 0x7a26}, {0x26d2, 0x7a28}, {0x26d3, 0x7a2a},
  {0x26d4, 0x7a2b}, {0x26d5, 0x7a29}, {0x26d6, 0x7a34}, {0x26d7, 0x7a35},
  {0x26d8, 0x7a36}, {0x26d9, 0x7a37}, {0x26da, 0x7a38}, {0x26db, 0x7a39},
  {0x26dc, 0x7a3a}, {0x26dd, 0x7a3b}, {0x26de, 0x7a3c}, {0x26df, 0x7a3d},
  {0x26e0, 0x7a3e}, {0x26e1, 0x7a3f}, {0x26e3, 0x7b21}, {0x26e8, 0x7b29},
  {0x26e9, 0x7b2c}, {0x26ea, 0x7b2e}, {0x26eb, 0x7b2f}, {0x26ec, 0x7b30},
  {0x26ed, 0x7b32}, {0x26ee, 0x7b33}, {0x26ef, 0x7b34}, {0x26f0, 0x7b37},
  {0x26f1, 0x7b38}, {0x26f2, 0x7b39}, {0x26f3, 0x7b3a}, {0x26f4, 0x7b3b},
  {0x26f5, 0x7b3c}, {0x26f6, 0x7b40}, {0x26f7, 0x7b46}, {0x26f8, 0x7b47},
  {0x26f9, 0x7b48}, {0x26fa, 0x7b49}, {0x26fb, 0x7b4c}, {0x26fc, 0x7b4d},
  {0x26fd, 0x7b4e}, {0x26fe, 0x7b4f}, {0x26ff, 0x7b51}, {0x2762, 0x7a23},
  {0x2776, 0x7e71}, {0x2777, 0x7e72}, {0x2778, 0x7e73}, {0x2779, 0x7e74},
  {0x277a, 0x7e75}, {0x277b, 0x7e76}, {0x277c, 0x7e77}, {0x277d, 0x7e78},
  {0x277e, 0x7e79}, {0x277f, 0x7e7a}, {0x27a1, 0x7c21}, {0x27d0, 0x7c54},
  {0x2a00, 0x7d6d}, {0x2b05, 0x7c22}, {0x2b06, 0x7c23}, {0x2b07, 0x7c24},
  {0x2b1b, 0x7a60}, {0x2b24, 0x7a61}, {0x2b2e, 0x7c26}, {0x2b2f, 0x7c25},
  {0x2b55, 0x7a40}, {0x2b56, 0x7b22}, {0x2b57, 0x7b23}, {0x2b58, 0x7b24},
  {0x2b59, 0x7b25}, {0x3012, 0x7b28}, {0x3016, 0x7c52}, {0x3017, 0x7c53},
  {0x3036, 0x7d2f}, {0x322a, 0x7d21}, {0x322b, 0x7d22}, {0x322c, 0x7d23},
  {0x322d, 0x7d24}, {0x322e, 0x7d25}, {0x322f, 0x7d26}, {0x3230, 0x7d27},
  {0x3231, 0x7c4d}, {0x3232, 0x7c4c}, {0x3233, 0x7c4a}, {0x3236, 0x7c4b},
  {0x3237, 0x7d28}, {0x3239, 0x7c4e}, {0x3244, 0x7c4f}, {0x3245, 0x7b2b},
  {0x3246, 0x7b2a}, {0x3247, 0x7c78}, {0x3248, 0x7a41}, {0x3249, 0x7a42},
  {0x324a, 0x7a43}, {0x324b, 0x7a44}, {0x324c, 0x7a45}, {0x324d, 0x7a46},
  {0x324e, 0x7a47}, {0x324f, 0x7a48}, {0x3251, 0x7e3d}, {0x3252, 0x7e3e},
  {0x3253, 0x7e3f}, {0x3254, 0x7e40}, {0x3255, 0x7e5b}, {0x3256, 0x7e5c},
  {0x3257, 0x7e5d}, {0x3258, 0x7e5e}, {0x3259, 0x7e5f}, {0x325a, 0x7e60},
  {0x325b, 0x7e7d}, {0x328b, 0x7b27}, {0x3299, 0x7a73}, {0x3371, 0x7d4d},
  {0x337b, 0x7d2c}, {0x337c, 0x7d2b}, {0x337d, 0x7d2a}, {0x337e, 0x7d29},
  {0x338f, 0x7d48}, {0x3390, 0x7d49}, {0x339d, 0x7c2d}, {0x339e, 0x7d4b},
  {0x33a0, 0x7c2e}, {0x33a1, 0x7c2b}, {0x33a2, 0x7d4c}, {0x33a4, 0x7c2f},
  {0x33a5, 0x7c2c}, {0x33ca, 0x7d4a}, {0x3402, 0x7521}, {0x351f, 0x752a},
  {0x37e2, 0x7541}, {0x3eda, 0x7574}, {0x4093, 0x7578}, {0x4103, 0x757e},
  {0x4264, 0x7626}, {0x4efd, 0x7523}, {0x4eff, 0x7524}, {0x4f9a, 0x7525},
  {0x4fc9, 0x7526}, {0x509c, 0x7527}, {0x511e, 0x7528}, {0x5186, 0x7c2a},
  {0x51bc, 0x7529}, {0x5307, 0x752b}, {0x5361, 0x752c}, {0x536c, 0x752d},
  {0x544d, 0x7530}, {0x5496, 0x7531}, {0x549c, 0x7532}, {0x54a9, 0x7533},
  {0x550e, 0x7534}, {0x554a, 0x7535}, {0x5672, 0x7536}, {0x56e4, 0x7537},
  {0x5733, 0x7538}, {0x5734, 0x7539}, {0x5880, 0x753b}, {0x59e4, 0x753c},
  {0x5a23, 0x753d}, {0x5a55, 0x753e}, {0x5bec, 0x753f}, {0x5e74, 0x7c27},
  {0x5eac, 0x7542}, {0x5f34, 0x7543}, {0x5f45, 0x7544}, {0x5fb7, 0x7545},
  {0x6017, 0x7546}, {0x6130, 0x7548}, {0x65e5, 0x7c29}, {0x6624, 0x7549},
  {0x66c8, 0x754a}, {0x66d9, 0x754b}, {0x66fa, 0x754c}, {0x66fb, 0x754d},
  {0x6708, 0x7c28}, {0x6852, 0x754e}, {0x6911, 0x7550}, {0x693b, 0x7551},
  {0x6a45, 0x7552}, {0x6a91, 0x7553}, {0x6adb, 0x7554}, {0x6bf1, 0x7558},
  {0x6ce0, 0x7559}, {0x6d2e, 0x755a}, {0x6dbf, 0x755c}, {0x6dca, 0x755d},
  {0x6df8, 0x755e}, {0x6f5e, 0x7560}, {0x6ff9, 0x7561}, {0x7064, 0x7562},
  {0x7147, 0x7565}, {0x71c1, 0x7566}, {0x7200, 0x7567}, {0x739f, 0x7568},
  {0x73a8, 0x7569}, {0x73c9, 0x756a}, {0x73d6, 0x756b}, {0x741b, 0x756c},
  {0x7421, 0x756d}, {0x7426, 0x756f}, {0x742a, 0x7570}, {0x742c, 0x7571},
  {0x7439, 0x7572}, {0x744b, 0x7573}, {0x7575, 0x7575}, {0x7581, 0x7576},
  {0x7772, 0x7577}, {0x78c8, 0x7579}, {0x78e0, 0x757a}, {0x7947, 0x757b},
  {0x79ae, 0x757c}, {0x79da, 0x7622}, {0x7a1e, 0x7623}, {0x7b7f, 0x7624},
  {0x7c31, 0x7625}, {0x7d8b, 0x7627}, {0x7fa1, 0x7628}, {0x8118, 0x7629},
  {0x813a, 0x762a}, {0x82ae, 0x762c}, {0x845b, 0x762d}, {0x84dc, 0x762e},
  {0x84ec, 0x762f}, {0x8559, 0x7630}, {0x85ce, 0x7631}, {0x8755, 0x7632},
  {0x87ec, 0x7633}, {0x880b, 0x7634}, {0x88f5, 0x7635}, {0x89d2, 0x7636},
  {0x8a79, 0x752e}, {0x8af6, 0x7637}, {0x8dce, 0x7638}, {0x8fbb, 0x7639},
  {0x8ff6, 0x763a}, {0x90dd, 0x763b}, {0x9127, 0x763c}, {0x912d, 0x763d},
  {0x91b2, 0x763e}, {0x9233, 0x763f}, {0x9288, 0x7640}, {0x9321, 0x7641},
  {0x9348, 0x7642}, {0x9592, 0x7643}, {0x96de, 0x7644}, {0x9903, 0x7645},
  {0x9940, 0x7646}, {0x9ad9, 0x7647}, {0x9bd6, 0x7648}, {0x9dd7, 0x7649},
  {0x9eb4, 0x764a}, {0x9eb5, 0x764b}, {0x9fc4, 0x754f}, {0x9fc5, 0x7621},
  {0x9fc6, 0x757d}, {0xfa10, 0x753a}, {0xfa11, 0x7540}, {0xfa45, 0x755b},
  {0xfa46, 0x755f}, {0xfa4a, 0x756e}, {0xfa6b, 0x7547}, {0xfa6c, 0x7563},
  {0xfa6d, 0x762b}, {0x1f100, 0x7c30}, {0x1f101, 0x7c40}, {0x1f102, 0x7c41},
  {0x1f103, 0x7c42}, {0x1f104, 0x7c43}, {0x1f105, 0x7c44}, {0x1f106, 0x7c45},
  {0x1f107, 0x7c46}, {0x1f108, 0x7c47}, {0x1f109, 0x7c48}, {0x1f10a, 0x7c49},
  {0x1f110, 0x7e41}, {0x1f111, 0x7e42}, {0x1f112, 0x7e43}, {0x1f113, 0x7e44},
  {0x1f114, 0x7e45}, {0x1f115, 0x7e46}, {0x1f116, 0x7e47}, {0x1f117, 0x7e48},
  {0x1f118, 0x7e49}, {0x1f119, 0x7e4a}, {0x1f11a, 0x7e4b}, {0x1f11b, 0x7e4c},
  {0x1f11c, 0x7e4d}, {0x1f11d, 0x7e4e}, {0x1f11e, 0x7e4f}, {0x1f11f, 0x7e50},
  {0x1f120, 0x7e51}, {0x1f121, 0x7e52}, {0x1f122, 0x7e53}, {0x1f123, 0x7e54},
  {0x1f124, 0x7e55}, {0x1f125, 0x7e56}, {0x1f126, 0x7e57}, {0x1f127, 0x7e58},
  {0x1f128, 0x7e59}, {0x1f129, 0x7e5a}, {0x1f12a, 0x7d3a}, {0x1f12b, 0x7c77},
  {0x1f12c, 0x7c76}, {0x1f12d, 0x7c57}, {0x1f131, 0x7a5e}, {0x1f13d, 0x7a5f},
  {0x1f13f, 0x7a52}, {0x1f142, 0x7a59}, {0x1f146, 0x7a53}, {0x1f14a, 0x7a50},
  {0x1f14b, 0x7a54}, {0x1f14c, 0x7a51}, {0x1f14d, 0x7a5d}, {0x1f14e, 0x7a72},
  {0x1f157, 0x7b3d}, {0x1f15f, 0x7b41}, {0x1f179, 0x7b45}, {0x1f17b, 0x7b4a},
  {0x1f17c, 0x7b50}, {0x1f17f, 0x7a30}, {0x1f18a, 0x7a31}, {0x1f18b, 0x7b42},
  {0x1f18c, 0x7b44}, {0x1f18d, 0x7b43}, {0x1f190, 0x7c79}, {0x1f200, 0x7a74},
  {0x1f210, 0x7a55}, {0x1f211, 0x7a56}, {0x1f212, 0x7a57}, {0x1f213, 0x7a58},
  {0x1f214, 0x7a5a}, {0x1f214, 0x7d3e}, {0x1f215, 0x7a5b}, {0x1f216, 0x7a5c},
  {0x1f217, 0x7a62}, {0x1f218, 0x7a63}, {0x1f219, 0x7a64}, {0x1f21a, 0x7a65},
  {0x1f21b, 0x7a66}, {0x1f21c, 0x7a68}, {0x1f21d, 0x7a69}, {0x1f21e, 0x7a6a},
  {0x1f21f, 0x7a6b}, {0x1f220, 0x7a6c}, {0x1f221, 0x7a6d}, {0x1f222, 0x7a6e},
  {0x1f223, 0x7a6f}, {0x1f224, 0x7a70}, {0x1f225, 0x7a71}, {0x1f226, 0x7c7a},
  {0x1f227, 0x7d3b}, {0x1f228, 0x7d3c}, {0x1f229, 0x7d3d}, {0x1f22a, 0x7d3f},
  {0x1f22b, 0x7d40}, {0x1f22c, 0x7d41}, {0x1f22d, 0x7d42}, {0x1f22e, 0x7d43},
  {0x1f22f, 0x7d44}, {0x1f230, 0x7d45}, {0x1f231, 0x7d46}, {0x1f240, 0x7d31},
  {0x1f241, 0x7d32}, {0x1f242, 0x7d33}, {0x1f243, 0x7d34}, {0x1f244, 0x7d35},
  {0x1f245, 0x7d36}, {0x1f246, 0x7d37}, {0x1f247, 0x7d38}, {0x1f248, 0x7d39},
  {0x1f6e7, 0x7b36}, {0x20158, 0x7522}, {0x20bb7, 0x752f}, {0x233cc, 0x7555},
  {0x233fe, 0x7556}, {0x235c4, 0x7557}, {0x242ee, 0x7564}
};

static int
out_ascii (struct state_to *st, uint32_t ch,
	   unsigned char **outptr, const unsigned char *outend)
{
  size_t esc_seqs;
  unsigned char *op = *outptr;

  esc_seqs = 0;
  if ((ch & 0x60) && st->gl == 0 && ch != 0x20 && ch != 0x7f && ch != 0xa0)
    ++ esc_seqs;

  if (__glibc_unlikely (op + esc_seqs + 1 > outend))
    return __GCONV_FULL_OUTPUT;

  if (esc_seqs > 0)
    {
      *op++ = LS1;
      st->gl = 1;
    }
  *op++ = ch & 0xff;
  if (ch == 0 || ch == LF)
    *st = def_state_to;
  *outptr = op;
  return __GCONV_OK;
}

static int
out_jisx0201 (struct state_to *st, uint32_t ch,
	      unsigned char **outptr, const unsigned char *outend)
{
  size_t esc_seqs;
  unsigned char *op = *outptr;

  esc_seqs = 0;
  if (st->g3 != JIS0201_KATA_idx)
    esc_seqs += 3;
  if (st->gr == 0) /* need LS3R */
    esc_seqs += 2;

  if (__glibc_unlikely (op + esc_seqs + 1 > outend))
    return __GCONV_FULL_OUTPUT;

  if (esc_seqs >= 3)
    {
      /* need charset designation */
      *op++ = ESC;
      *op++ = '\x2b'; /* designate single byte charset to G3 */
      *op++ = JIS0201_KATA_set;
      st->g3 = JIS0201_KATA_idx;
    }
  if (esc_seqs == 2 || esc_seqs == 5)
    {
      *op++ = ESC;
      *op++ = LS3R;
      st->gr = 1;
    }
  *op++ = ch & 0xff;
  *outptr = op;
  return __GCONV_OK;
}

static int
out_katakana (struct state_to *st, unsigned char ch,
	      unsigned char **outptr, const unsigned char *outend)
{
  size_t esc_seqs;
  unsigned char *op = *outptr;

  esc_seqs = 0;
  if (st->g3 != KATAKANA_idx)
    esc_seqs += 3;
  if (st->gr == 0) /* need LS3R */
    esc_seqs += 2;

  if (__glibc_unlikely (op + esc_seqs + 1 > outend))
    return __GCONV_FULL_OUTPUT;

  if (esc_seqs >= 3)
    {
      /* need charset designation */
      *op++ = ESC;
      *op++ = '\x2b'; /* designate single byte charset to G3 */
      *op++ = KATAKANA_set;
      st->g3 = KATAKANA_idx;
    }
  if (esc_seqs == 2 || esc_seqs == 5)
    {
      *op++ = ESC;
      *op++ = LS3R;
      st->gr = 1;
    }
  *op++ = ch | 0x80;
  *outptr = op;
  return __GCONV_OK;
}

static int
out_hiragana (struct state_to *st, unsigned char ch,
	      unsigned char **outptr, const unsigned char *outend)
{
  size_t esc_seqs;
  unsigned char *op = *outptr;

  esc_seqs = 0;
  if (st->g2 != HIRAGANA_idx)
    esc_seqs += 3;
  if (st->gr == 1) /* need LS2R */
    esc_seqs += 2;

  if (__glibc_unlikely (op + esc_seqs + 1 > outend))
    return __GCONV_FULL_OUTPUT;

  if (esc_seqs >= 3)
    {
      /* need charset designation */
      *op++ = ESC;
      *op++ = '\x2a'; /* designate single byte charset to G2 */
      *op++ = HIRAGANA_set;
      st->g2 = HIRAGANA_idx;
    }
  if (esc_seqs == 2 || esc_seqs == 5)
    {
      *op++ = ESC;
      *op++ = LS2R;
      st->gr = 0;
    }
  *op++ = ch | 0x80;
  *outptr = op;
  return __GCONV_OK;
}

static int
is_kana_punc (uint32_t ch)
{
  int i;
  size_t len;

  len = NELEMS (hira_punc);
  for (i = 0; i < len; i++)
    if (ch == hira_punc[i])
      return i;

  len = NELEMS (kata_punc);
  for (i = 0; i < len; i++)
    if (ch == kata_punc[i])
      return i + NELEMS (hira_punc);
  return -1;
}

static int
out_kana_punc (struct state_to *st, int idx,
	       unsigned char **outptr, const unsigned char *outend)
{
  size_t len = NELEMS (hira_punc);

  if (idx < len)
    return out_hiragana (st, 0x77 + idx, outptr, outend);
  idx -= len;
  if (idx >= 2)
    {
      /* common punc. symbols shared by katakana/hiragana */
      /* guess which is used currently */
      if (st->gr == 0 && st->g2 == HIRAGANA_idx)
	return out_hiragana (st, 0x77 + idx, outptr, outend);
      else if (st->gr == 1 && st->g3 == KATAKANA_idx)
	return out_katakana (st, 0x77 + idx, outptr, outend);
      else if (st->g2 == HIRAGANA_idx && st->g3 != KATAKANA_idx)
	return out_hiragana (st, 0x77 + idx, outptr, outend);
      /* fall through */
    }
  return out_katakana (st, 0x77 + idx, outptr, outend);
}

static int
out_kanji (struct state_to *st, uint32_t ch,
	   unsigned char **outptr, const unsigned char *outend)
{
  size_t esc_seqs;
  unsigned char *op = *outptr;

  esc_seqs = 0;
  if (st->gl)
    ++ esc_seqs;

  if (__glibc_unlikely (op + esc_seqs + 2 > outend))
    return __GCONV_FULL_OUTPUT;

  if (st->gl)
    {
      *op++ = LS0;
      st->gl = 0;
    }
  *op++ = (ch >> 8) & 0x7f;
  *op++ = ch & 0x7f;
  *outptr = op;
  return __GCONV_OK;
}

/* convert JISX0213_{1,2} to ARIB-STD-B24 */
/* assert(set_idx == JISX0213_1_idx || set_idx == JISX0213_2_idx); */
static int
out_jisx0213 (struct state_to *st, uint32_t ch, int set_idx,
	      unsigned char **outptr, const unsigned char *outend)
{
  size_t esc_seqs;
  unsigned char *op = *outptr;

  esc_seqs = 0;
  if (st->g2 != set_idx)
    esc_seqs += 4; /* designate to G2 */
  if (st->gr) /* if GR does not designate G2 */
    esc_seqs ++; /* SS3 */

  if (__glibc_unlikely (op + esc_seqs + 2 > outend))
    return __GCONV_FULL_OUTPUT;

  if (esc_seqs >= 4)
    {
      /* need charset designation */
      *op++ = ESC;
      *op++ = '\x24'; /* designate multibyte charset */
      *op++ = '\x2a'; /* to G2 */
      *op++ = (set_idx == JISX0213_1_idx) ? JISX0213_1_set : JISX0213_2_set;
      st->g2 = JISX0213_1_idx;
    }
  if (st->gr)
    *op++ = SS2; /* GR designates G3 now. insert SS2 */
  else
    ch |= 0x8080; /* use GR(G2) */
  *op++ = (ch >> 8) & 0xff;
  *op++ = ch & 0xff;
  *outptr = op;
  return __GCONV_OK;
}

static int
out_extsym (struct state_to *st, uint32_t ch,
	    unsigned char **outptr, const unsigned char *outend)
{
  size_t esc_seqs;
  unsigned char *op = *outptr;

  esc_seqs = 0;
  if (st->g3 != EXTRA_SYMBOLS_idx)
    esc_seqs += 4;
  if (st->gr == 0) /* if GR designates G2, use SS3 */
    ++ esc_seqs;

  if (__glibc_unlikely (op + esc_seqs + 2 > outend))
    return __GCONV_FULL_OUTPUT;

  if (esc_seqs >= 4)
    {
      /* need charset designation */
      *op++ = ESC;
      *op++ = '\x24'; /* designate multibyte charset */
      *op++ = '\x2b'; /* to G3 */
      *op++ = EXTRA_SYMBOLS_set;
      st->g3 = EXTRA_SYMBOLS_idx;
    }
  if (st->gr == 0)
    *op++ = SS3;
  else
    ch |= 0x8080;
  *op++ = (ch >> 8) & 0xff;
  *op++ = ch & 0xff;
  *outptr = op;
  return __GCONV_OK;
}

static int
out_buffered (struct state_to *st,
	      unsigned char **outptr, const unsigned char *outend)
{
  int r;

  if (st->prev == 0)
    return __GCONV_OK;

  if (st->prev >> 16)
    r = out_jisx0213 (st, st->prev & 0x7f7f, JISX0213_1_idx, outptr, outend);
  else if ((st->prev & 0x7f00) == 0x2400)
    r = out_hiragana (st, st->prev, outptr, outend);
  else if ((st->prev & 0x7f00) == 0x2500)
    r = out_katakana (st, st->prev, outptr, outend);
  else /* should not be reached */
    r = out_kanji (st, st->prev, outptr, outend);

  st->prev = 0;
  return r;
}

static int
cmp_u32 (const void *a, const void *b)
{
  return *(const uint32_t *)a - *(const uint32_t *)b;
}

static int
find_extsym_idx (uint32_t ch)
{
  const uint32_t (*p)[2];

  p = bsearch (&ch, ucs4_to_extsym,
	       NELEMS (ucs4_to_extsym), sizeof (ucs4_to_extsym[0]), cmp_u32);
  return p ? (p - ucs4_to_extsym) : -1;
}

#define BODY \
  {									      \
    uint32_t ch, jch;							      \
    unsigned char buf[2];						      \
    int r;								      \
									      \
    ch = get32 (inptr);							      \
    if (st.prev != 0)							      \
      {									      \
	/* Attempt to combine the last character with this one.  */	      \
	unsigned int idx;						      \
	unsigned int len;						      \
									      \
	if (ch == 0x02e5)						      \
	  idx = COMP_TABLE_IDX_02E5, len = COMP_TABLE_LEN_02E5;		      \
	else if (ch == 0x02e9)						      \
	  idx = COMP_TABLE_IDX_02E9, len = COMP_TABLE_LEN_02E9;		      \
	else if (ch == 0x0300)						      \
	  idx = COMP_TABLE_IDX_0300, len = COMP_TABLE_LEN_0300;		      \
	else if (ch == 0x0301)						      \
	  idx = COMP_TABLE_IDX_0301, len = COMP_TABLE_LEN_0301;		      \
	else if (ch == 0x309a)						      \
	  idx = COMP_TABLE_IDX_309A, len = COMP_TABLE_LEN_309A;		      \
	else								      \
	  idx = 0, len = 0;						      \
									      \
	for (;len > 0; ++idx, --len)					      \
	  if (comp_table_data[idx].base == (st.prev & 0x7f7f))		      \
	    break;							      \
									      \
	if (len > 0)							      \
	  {								      \
	    /* Output the combined character.  */			      \
	    /* We know the combined character is in JISX0213 plane 1 */	      \
	    r = out_jisx0213 (&st, comp_table_data[idx].composed,	      \
				JISX0213_1_idx, &outptr, outend);	      \
	    st.prev = 0;						      \
	    goto next;							      \
	  }								      \
									      \
	/* not a combining character */					      \
	/* Output the buffered character. */				      \
	/* We know it is in JISX0208(HIRA/KATA) or in JISX0213 plane 1. */    \
	r = out_buffered (&st, &outptr, outend);			      \
	if (r != __GCONV_OK)						      \
	  {								      \
	    result = r;							      \
	    break;							      \
	  }								      \
	/* fall through & output the current character (ch). */		      \
     }									      \
									      \
    /* ASCII or C0/C1 or NBSP */					      \
    if (ch <= 0xa0)							      \
      {									      \
	if ((ch & 0x60) || ch == 0 || ch == LF || ch == CR || ch == BS)	      \
	  r = out_ascii (&st, ch, &outptr, outend);			      \
	else								      \
	  STANDARD_TO_LOOP_ERR_HANDLER (4);				      \
	goto next;							      \
      }									      \
									      \
    /* half-width KATAKANA */						      \
    if (ucs4_to_jisx0201 (ch, buf) != __UNKNOWN_10646_CHAR)		      \
      {									      \
	if (__glibc_unlikely (buf[0] < 0x80)) /* yen sign or overline */      \
	  r = out_ascii (&st, buf[0], &outptr, outend);			      \
	else								      \
	  r = out_jisx0201 (&st, buf[0], &outptr, outend);		      \
	goto next;							      \
      }									      \
									      \
    /* check kana punct. symbols (prefer 1-Byte charset over KANJI_set) */    \
    r = is_kana_punc (ch);						      \
    if (r >= 0)								      \
      {									      \
	r = out_kana_punc (&st, r, &outptr, outend);			      \
	goto next;							      \
      }									      \
									      \
    if (ch >= ucs4_to_nonsp_kanji[0][0] &&				      \
	ch <= ucs4_to_nonsp_kanji[NELEMS (ucs4_to_nonsp_kanji) - 1][0])	      \
      {									      \
	int i;								      \
									      \
	for (i = 0; i < NELEMS (ucs4_to_nonsp_kanji); i++)		      \
	  {								      \
	    if (ch < ucs4_to_nonsp_kanji[i][0])				      \
	      break;							      \
	    else if (ch == ucs4_to_nonsp_kanji[i][0])			      \
	      {								      \
		r = out_kanji (&st, ucs4_to_nonsp_kanji[i][1],		      \
			       &outptr, outend);			      \
		goto next;						      \
	      }								      \
	  }								      \
      }									      \
									      \
    jch = ucs4_to_jisx0213 (ch);					      \
									      \
    if (ucs4_to_jisx0208 (ch, buf, 2) != __UNKNOWN_10646_CHAR)		      \
      {									      \
	if (jch & 0x0080)						      \
	  {								      \
	    /* A possible match in comp_table_data.  Buffer it.  */	      \
									      \
	    /* We know it's a JISX 0213 plane 1 character.  */		      \
	    assert ((jch & 0x8000) == 0);				      \
									      \
	    st.prev = jch & 0x7f7f;					      \
	    r = __GCONV_OK;						      \
	    goto next;							      \
	  }								      \
	/* check HIRAGANA/KATAKANA (prefer 1-Byte charset over KANJI_set) */  \
	if (buf[0] == 0x24)						      \
	  r = out_hiragana (&st, buf[1], &outptr, outend);		      \
	else if (buf[0] == 0x25)					      \
	  r = out_katakana (&st, buf[1], &outptr, outend);		      \
	else if (jch == 0x227e || (jch >= 0x212d && jch <= 0x2132))	      \
	  r = out_jisx0213 (&st, jch, JISX0213_1_idx, &outptr, outend);	      \
	else								      \
	  r = out_kanji (&st, jch, &outptr, outend);			      \
	goto next;							      \
      }									      \
									      \
    if (jch & 0x0080)							      \
      {									      \
	st.prev = (jch & 0x7f7f) | 0x10000;				      \
	r = __GCONV_OK;							      \
	goto next;							      \
      }									      \
									      \
    /* KANJI shares some chars with EXTRA_SYMBOLS, but prefer extra symbols*/ \
    r = find_extsym_idx (ch);						      \
    if (r >= 0)								      \
      {									      \
	ch = ucs4_to_extsym[r][1];					      \
	r = out_extsym (&st, ch, &outptr, outend);			      \
	goto next;							      \
      }									      \
									      \
    if (jch != 0)							      \
      {									      \
	r = out_jisx0213 (&st, jch & 0x7f7f,				      \
			  (jch & 0x8000) ? JISX0213_2_idx : JISX0213_1_idx,   \
			  &outptr, outend);				      \
	goto next;							      \
      }									      \
									      \
    UNICODE_TAG_HANDLER (ch, 4);					      \
    STANDARD_TO_LOOP_ERR_HANDLER (4);					      \
									      \
next:									      \
    if (r != __GCONV_OK)						      \
      {									      \
	result = r;							      \
	break;								      \
      }									      \
    inptr += 4;								      \
  }
#include <iconv/loop.c>

/* Now define the toplevel functions.  */
#include <iconv/skeleton.c>
