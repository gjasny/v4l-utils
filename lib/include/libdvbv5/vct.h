/*
 * Copyright (c) 2013 - Mauro Carvalho Chehab <m.chehab@samsung.com>
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

#ifndef _VCT_H
#define _VCT_H

#include <stdint.h>
#include <unistd.h> /* ssize_t */

#include <libdvbv5/atsc_header.h>

#define ATSC_TABLE_TVCT     0xc8
#define ATSC_TABLE_CVCT     0xc9
#define ATSC_TABLE_VCT_PID  0x1ffb

struct atsc_table_vct_channel {
	uint16_t	__short_name[7];

	union {
		uint32_t bitfield1;
		struct {
			uint32_t	modulation_mode:8;
			uint32_t	minor_channel_number:10;
			uint32_t	major_channel_number:10;
			uint32_t	reserved1:4;
		} __attribute__((packed));
	} __attribute__((packed));

	uint32_t	carrier_frequency;
	uint16_t	channel_tsid;
	uint16_t	program_number;
	union {
		uint16_t bitfield2;
		struct {
			uint16_t	service_type:6;
			uint16_t	reserved2:3;
			uint16_t	hide_guide:1;
			uint16_t	out_of_band:1;	/* CVCT only */
			uint16_t	path_select:1;	/* CVCT only */
			uint16_t	hidden:1;
			uint16_t	access_controlled:1;
			uint16_t	ETM_location:2;

		} __attribute__((packed));
	} __attribute__((packed));

	uint16_t source_id;
	union {
		uint16_t bitfield3;
		struct {
			uint16_t descriptors_length:10;
			uint16_t reserved3:6;
		} __attribute__((packed));
	} __attribute__((packed));

	/*
	 * Everything after descriptor (including it) won't be bit-mapped
	 * to the data parsed from the MPEG TS. So, metadata are added there
	 */
	struct dvb_desc *descriptor;
	struct atsc_table_vct_channel *next;

	/* The channel_short_name is converted to locale charset by vct.c */

	char short_name[32];
} __attribute__((packed));

struct atsc_table_vct {
	ATSC_HEADER();

	uint8_t num_channels_in_section;

	/*
	 * Everything after descriptor (including it) won't be bit-mapped
	 * to the data parsed from the MPEG TS. So, metadata are added there
	 */
	struct atsc_table_vct_channel *channel;
	struct dvb_desc *descriptor;
} __attribute__((packed));


union atsc_table_vct_descriptor_length {
	uint16_t bitfield;
	struct {
		uint16_t descriptor_length:10;
		uint16_t reserved:6;
	} __attribute__((packed));
} __attribute__((packed));

#define atsc_vct_channel_foreach(_channel, _vct) \
	for (struct atsc_table_vct_channel *_channel = _vct->channel; _channel; _channel = _channel->next) \

struct dvb_v5_fe_parms;

#ifdef __cplusplus
extern "C" {
#endif

ssize_t atsc_table_vct_init (struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, struct atsc_table_vct **table);
void atsc_table_vct_free(struct atsc_table_vct *vct);
void atsc_table_vct_print(struct dvb_v5_fe_parms *parms, struct atsc_table_vct *vct);

#ifdef __cplusplus
}
#endif

#endif
