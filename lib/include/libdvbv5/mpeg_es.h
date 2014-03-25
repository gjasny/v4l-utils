/*
 * Copyright (c) 2013 - Andre Roth <neolynx@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 */

#ifndef _MPEG_ES_H
#define _MPEG_ES_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#define DVB_MPEG_ES_PIC_START  0x00
#define DVB_MPEG_ES_USER_DATA  0xb2
#define DVB_MPEG_ES_SEQ_START  0xb3
#define DVB_MPEG_ES_SEQ_EXT    0xb5
#define DVB_MPEG_ES_GOP        0xb8
#define DVB_MPEG_ES_SLICES     0x01 ... 0xaf

struct dvb_mpeg_es_seq_start {
	union {
		uint32_t bitfield;
		struct {
			uint32_t  type:8;
			uint32_t  sync:24;
		} __attribute__((packed));
	} __attribute__((packed));
	union {
		uint32_t bitfield2;
		struct {
			uint32_t framerate:4;
			uint32_t aspect:4;
			uint32_t height:12;
			uint32_t width:12;
		} __attribute__((packed));
	} __attribute__((packed));
	union {
		uint32_t bitfield3;
		struct {
			uint32_t qm_nonintra:1;
			uint32_t qm_intra:1;
			uint32_t constrained:1;
			uint32_t vbv:10; // Size of video buffer verifier = 16*1024*vbv buf size
			uint32_t one:1;
			uint32_t bitrate:18;
		} __attribute__((packed));
	} __attribute__((packed));
} __attribute__((packed));

struct dvb_mpeg_es_pic_start {
	union {
		uint32_t bitfield;
		struct {
			uint32_t  type:8;
			uint32_t  sync:24;
		} __attribute__((packed));
	} __attribute__((packed));
	union {
		uint32_t bitfield2;
		struct {
			uint32_t dummy:3;
			uint32_t vbv_delay:16;
			uint32_t coding_type:3;
			uint32_t temporal_ref:10;
		} __attribute__((packed));
	} __attribute__((packed));
} __attribute__((packed));

enum dvb_mpeg_es_frame_t
{
	DVB_MPEG_ES_FRAME_UNKNOWN,
	DVB_MPEG_ES_FRAME_I,
	DVB_MPEG_ES_FRAME_P,
	DVB_MPEG_ES_FRAME_B,
	DVB_MPEG_ES_FRAME_D
};
extern const char *dvb_mpeg_es_frame_names[5];

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

int  dvb_mpeg_es_seq_start_init (const uint8_t *buf, ssize_t buflen, struct dvb_mpeg_es_seq_start *seq_start);
void dvb_mpeg_es_seq_start_print(struct dvb_v5_fe_parms *parms, struct dvb_mpeg_es_seq_start *seq_start);

int  dvb_mpeg_es_pic_start_init (const uint8_t *buf, ssize_t buflen, struct dvb_mpeg_es_pic_start *pic_start);
void dvb_mpeg_es_pic_start_print(struct dvb_v5_fe_parms *parms, struct dvb_mpeg_es_pic_start *pic_start);

#ifdef __cplusplus
}
#endif

#endif
