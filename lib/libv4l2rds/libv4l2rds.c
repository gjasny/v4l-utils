/*
 * Copyright 2012 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 * Author: Konke Radlow <koradlow@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <config.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <linux/videodev2.h>

#include "../include/libv4l2rds.h"

/* struct to encapsulate the private state information of the decoding process */
/* the fields (except for handle) are for internal use only - new information
 * is decoded and stored in them until it can be verified and copied to the
 * public part of the  rds structure (handle) */
/* for meaning of abbreviations check the library header libv4l2rds.h */
struct rds_private_state {
	/* v4l2_rds has to be in first position, to allow typecasting between
	 * v4l2_rds and rds_private_state pointers */
	struct v4l2_rds handle;

	/* current state of rds group decoding */
	uint8_t decode_state;

	/* temporal storage locations for rds fields */
	uint16_t new_pi;
	uint8_t new_ps[8];
	uint8_t new_ps_valid[8];
	uint8_t new_pty;
	uint8_t new_ptyn[2][4];
	bool new_ptyn_valid[2];
	uint8_t new_rt[64];
	uint8_t next_rt_segment;
	uint8_t new_di;
	uint8_t next_di_segment;
	uint8_t new_ecc;
	uint8_t new_lc;
	/* RDS date / time representation */
	uint32_t new_mjd;	/* modified Julian Day code */
	uint8_t utc_hour;
	uint8_t utc_minute;
	uint8_t utc_offset;

	/* TMC decoding buffers, to store data before it can be verified,
	 * and before all parts of a multi-group message have been received */
	uint8_t continuity_id;	/* continuity index of current TMC multigroup */
	uint8_t grp_seq_id; 	/* group sequence identifier */
	bool optional_tmc[112];	/* buffer for up to 112 bits of optional
				 * additional data in multi-group
				 * messages (112 is the maximal possible length
				 * specified by the standard) */

	/* TMC groups are only accepted if the same data was received twice,
	 * these structs are used as receive buffers to validate TMC groups */
	struct v4l2_rds_group prev_tmc_group;
	struct v4l2_rds_group prev_tmc_sys_group;
	struct v4l2_rds_tmc_msg new_tmc_msg;

	/* buffers for rds data, before group type specific decoding can
	 * be done */
	struct v4l2_rds_group rds_group;
	struct v4l2_rds_data rds_data_raw[4];
};

/* states of the RDS block into group decoding state machine */
enum rds_state {
	RDS_EMPTY,
	RDS_A_RECEIVED,
	RDS_B_RECEIVED,
	RDS_C_RECEIVED,
};

static inline uint8_t set_bit(uint8_t input, uint8_t bitmask, bool bitvalue)
{
	return bitvalue ? input | bitmask : input & ~bitmask;
}

/* rds_decode_a-d(..): group of functions to decode different RDS blocks
 * into the RDS group that's currently being received
 *
 * block A of RDS group always contains PI code of program */
static uint32_t rds_decode_a(struct rds_private_state *priv_state, struct v4l2_rds_data *rds_data)
{
	struct v4l2_rds *handle = &priv_state->handle;
	uint32_t updated_fields = 0;
	uint16_t pi = (rds_data->msb << 8) | rds_data->lsb;

	/* data in RDS group is uninterpreted */
	priv_state->rds_group.pi = pi;

	/* compare PI values to detect PI update (Channel Switch)
	 * --> new PI is only accepted, if the same PI is received
	 * at least 2 times in a row */
	if (pi != handle->pi && pi == priv_state->new_pi) {
		handle->pi = pi;
		handle->valid_fields |= V4L2_RDS_PI;
		updated_fields |= V4L2_RDS_PI;
	} else if (pi != handle->pi && pi != priv_state->new_pi) {
		priv_state->new_pi = pi;
	}

	return updated_fields;
}

/* block B of RDS group always contains Group Type Code, Group Type information
 * Traffic Program Code and Program Type Code as well as 5 bits of Group Type
 * depending information */
static uint32_t rds_decode_b(struct rds_private_state *priv_state, struct v4l2_rds_data *rds_data)
{
	struct v4l2_rds *handle = &priv_state->handle;
	struct v4l2_rds_group *grp = &priv_state->rds_group;
	bool traffic_prog;
	uint8_t pty;
	uint32_t updated_fields = 0;

	/* bits 12-15 (4-7 of msb) contain the Group Type Code */
	grp->group_id = rds_data->msb >> 4 ;

	/* bit 11 (3 of msb) defines Group Type info: 0 = A, 1 = B */
	grp->group_version = (rds_data->msb & 0x08) ? 'B' : 'A';

	/* bit 10 (2 of msb) defines Traffic program Code */
	traffic_prog = rds_data->msb & 0x04;
	if (handle->tp != traffic_prog) {
		handle->tp = traffic_prog;
		updated_fields |= V4L2_RDS_TP;
	}
	handle->valid_fields |= V4L2_RDS_TP;

	/* bits 0-4 contains Group Type depending information */
	grp->data_b_lsb = rds_data->lsb & 0x1f;

	/* bits 5-9 contain the PTY code */
	pty = (rds_data->msb << 3) | (rds_data->lsb >> 5);
	pty &= 0x1f; /* mask out 3 irrelevant bits */
	/* only accept new PTY if same PTY is received twice in a row
	 * and filter out cases where the PTY is already known */
	if (handle->pty == pty) {
		priv_state->new_pty = pty;
		return updated_fields;
	}

	if (priv_state->new_pty == pty) {
		handle->pty = priv_state->new_pty;
		updated_fields |= V4L2_RDS_PTY;
		handle->valid_fields |= V4L2_RDS_PTY;
	} else {
		priv_state->new_pty = pty;
	}

	return updated_fields;
}

/* block C of RDS group contains either data or the PI code, depending
 * on the Group Type - store the raw data for later decoding */
static void rds_decode_c(struct rds_private_state *priv_state, struct v4l2_rds_data *rds_data)
{
	struct v4l2_rds_group *grp = &priv_state->rds_group;

	grp->data_c_msb = rds_data->msb;
	grp->data_c_lsb = rds_data->lsb;
	/* we could decode the PI code here, because we already know if the
	 * group is of type A or B, but it doesn't give any advantage because
	 * we only get here after the PI code has been decoded in the first
	 * state of the state machine */
}

/* block D of RDS group contains data - store the raw data for later decoding */
static void rds_decode_d(struct rds_private_state *priv_state, struct v4l2_rds_data *rds_data)
{
	struct v4l2_rds_group *grp = &priv_state->rds_group;

	grp->data_d_msb = rds_data->msb;
	grp->data_d_lsb = rds_data->lsb;
}

/* decodes the RDS radio frequency representation into Hz
 * @af: 8-bit AF value as transmitted in RDS groups
 * @is_vhf: boolean value defining  which conversion table to use
 * @return: frequency in Hz, 0 in case of wrong input values */
static uint32_t rds_decode_af(uint8_t af, bool is_vhf)
{
	uint32_t freq = 0;

	/* AF = 0 => "not to be used"
	 * AF >= 205 => special meanings */
	if (af == 0 || af >= 205)
		return 0;

	/* calculate the AF values in HZ */
	if (is_vhf)
		freq = 87500000 + af * 100000;
	else if (af <= 15)
		freq = 152000 + af * 9000;
	else
		freq = 531000 + af * 9000;

	return freq;
}

/* compare two rds-groups for equality */
/* used for decoding RDS-TMC, which has the requirement that the same group
 * is at least received twice before it is accepted */
static bool rds_compare_group(const struct v4l2_rds_group *a,
				const struct v4l2_rds_group *b)
{
	if (a->pi != b->pi)
		return false;
	if (a->group_version != b->group_version)
		return false;
	if (a->group_id != b->group_id)
		return false;

	if (a->data_b_lsb != b->data_b_lsb)
		return false;
	if (a->data_c_lsb != b->data_c_lsb || a->data_c_msb != b->data_c_msb)
		return false;
	if (a->data_d_lsb != b->data_d_lsb || a->data_d_msb != b->data_d_msb)
		return false;
	/* all values are equal */
	return true;
}

/* checks if an entry for the given PI already exists and returns the index
 * of that entry if so. Else it adds a new entry to the TMC-Tuning table and returns
 * the index of the new field */
static int rds_add_tmc_station(struct rds_private_state *priv_state, uint16_t pi)
{
	struct v4l2_tmc_tuning *tuning = &priv_state->handle.tmc.tuning;
	uint8_t index = tuning->index;
	uint8_t size = tuning->station_cnt;

	/* check if there's an entry for the given PI key */
	for (int i = 0; i < tuning->station_cnt; i++) {
		if (tuning->station[i].pi == pi) {
			return i;
		}
	}
	/* if the the maximum table size is reached, overwrite old
	 * entries, starting at the oldest one = 0 */
	tuning->station[index].pi = pi;
	tuning->index = (index+1 < MAX_TMC_ALT_STATIONS) ? (index+1) : 0;
	tuning->station_cnt = (size+1 <= MAX_TMC_ALT_STATIONS) ? (size+1) : MAX_TMC_ALT_STATIONS;
	return index;
}

/* tries to add new AFs to the relevant entry in the list of RDS-TMC providers */
static bool rds_add_tmc_af(struct rds_private_state *priv_state)
{
	struct v4l2_rds_group *grp = &priv_state->rds_group;
	struct v4l2_tmc_alt_freq *afi;
	uint16_t pi_on = grp->data_d_msb << 8 | grp->data_d_lsb;
	uint8_t variant = grp->data_b_lsb & 0x0f;
	uint8_t station_index = rds_add_tmc_station(priv_state, pi_on);
	uint8_t af_index;
	uint8_t mapped_af_index;
	uint32_t freq_a = rds_decode_af(grp->data_c_msb, true);
	uint32_t freq_b = rds_decode_af(grp->data_c_lsb, true);

	afi = &priv_state->handle.tmc.tuning.station[station_index].afi;
	af_index = afi->af_index;
	mapped_af_index = afi->mapped_af_index;

	/* specific frequencies */
	if (variant == 6) {
		/* compare the new AFs to the stored ones, reset them to 0 if the AFs are
		 * already known */
		for (int i = 0; i < afi->af_size; i++) {
			freq_a = (freq_a == afi->af[i]) ? 0 : freq_a;
			freq_b = (freq_b == afi->af[i]) ? 0 : freq_b;
		}
		/* return early if there is nothing to do */
		if (freq_a == 0 && freq_b == 0)
			return false;

		/* add the new AFs if they were previously unknown */
		if (freq_a != 0) {
			afi->af[af_index] = freq_a;
			af_index = (af_index+1 < MAX_TMC_AF_CNT) ? af_index+1 : 0;
			afi->af_size++;
		}
		if (freq_b != 0) {
			afi->af[af_index] = freq_b;
			af_index = (af_index+1 < MAX_TMC_AF_CNT) ? af_index+1 : 0;
			afi->af_size++;
		}
		/* update the information in the handle */
		afi->af_index = af_index;
		if (afi->af_size >= MAX_TMC_AF_CNT)
			afi->af_size = MAX_TMC_AF_CNT;

		return true;
	}

	/* mapped frequency pair */
	else if (variant == 7) {
		/* check if there's already a frequency mapped to the new tuning
		 * frequency, update the mapped frequency in this case */
		for (int i = 0; i < afi->mapped_af_size; i++) {
			if (freq_a == afi->mapped_af_tuning[i]) {
				afi->mapped_af[i] = freq_b;
				return true;
			}
		}
		/* new pair is unknown, add it to the list */
		if (freq_a != 0 && freq_b != 0) {
			mapped_af_index = (mapped_af_index+1 >= MAX_TMC_AF_CNT) ? 0 : mapped_af_index + 1;
			afi->mapped_af[mapped_af_index] = freq_b;
			afi->mapped_af_tuning[mapped_af_index] = freq_a;
			afi->mapped_af_size++;
		}
		/* update the information in the handle */
		afi->mapped_af_index = mapped_af_index;
		if (afi->mapped_af_size >= MAX_TMC_AF_CNT)
			afi->mapped_af_size = MAX_TMC_AF_CNT;

		return true;
	}
	return false;
}

/* decode additional information of a TMC message into handy representation */
/* the additional information of TMC messages is submitted in (up to) 4 blocks of
 * 28 bits each, which are to be treated as a consecutive bit-array. This data
 * is represented by the optional_tmc array in the private handle, where each
 * value represents 1 bit. Each additional information set is defined by a 4-bit
 * label, and an associated data field for which the length is known */ 
void rds_tmc_decode_additional(struct rds_private_state *priv_state)
{
	struct v4l2_rds_tmc_msg *msg = &priv_state->handle.tmc.tmc_msg;
	struct v4l2_tmc_additional *fields = &msg->additional.fields[0];
	const uint8_t label_len = 4;	/* fixed length of a label */
	uint8_t len; 		/* length of next data field to be extracted */
	uint8_t label;		/* buffer for extracted label */
	uint16_t data;		/* buffer for extracted data */
	uint8_t array_idx = 0;	/* index for optional_tmc array */
	uint8_t *field_idx = &msg->additional.size;	/* index for
				 * additional field array */
	/* LUT for the length of additional data blocks as defined in
	 * ISO 14819-1 sect. 5.5.1 */
	static const uint8_t additional_lut[16] = {
		3, 3, 5, 5, 5, 8, 8, 8, 8, 11, 16, 16, 16, 16, 0, 0
	};

	/* reset the additional information from previous messages */
	*field_idx = 0;
	memset(fields, 0, sizeof(*fields));

	/* decode the optional TMC data */
	while (array_idx < (msg->length * 28)) {
		/* extract the next label */
		label = 0;
		for (int i = 0; i < label_len; i++) {
			if (priv_state->optional_tmc[array_idx++])
				label |= 1 << (label_len - 1 - i); 
		}

		/* extract the associated data block */
		data = 0;
		len = additional_lut[label];	/* length of data block */
		for (int i = 0; i < len; i++) {
			if (priv_state->optional_tmc[array_idx++])
				data |= 1 << (len - 1 - i);
		}

		/* if  the label is not "reserved for future use", or both
		 * fields are 0, store the extracted additional information */
		if (label == 15)
			continue;
		if (label == 0 && data == 0)
			continue;
		fields[*field_idx].label = label;
		fields[*field_idx].data = data;
		*field_idx += 1;
	}
}

/* decode the TMC system information that is contained in type 3A groups
 * that announce the presence of TMC */
static uint32_t rds_decode_tmc_system(struct rds_private_state *priv_state)
{
	struct v4l2_rds_group *group = &priv_state->rds_group;
	struct v4l2_rds_tmc *tmc = &priv_state->handle.tmc;
	uint8_t variant_code;

	/* check if the same group was received twice. If not, store new
	 * group and return early */
	if (!rds_compare_group(&priv_state->prev_tmc_sys_group, &priv_state->rds_group)) {
		priv_state->prev_tmc_sys_group = priv_state->rds_group;
		return 0;
	}
	/* bits 14-15 of block 3 contain the variant code */
	variant_code = priv_state->rds_group.data_c_msb >> 6;
	switch (variant_code) {
	case 0x00:
		/* bits 11-16 of block 3 contain the LTN */
		tmc->ltn = (((group->data_c_msb & 0x0f) << 2)) |
			(group->data_c_lsb >> 6);
		/* bit 5 of block 3 contains the AFI */
		tmc->afi = group->data_c_lsb & 0x20;
		/* bit 4 of block 3 contains the Mode */
		tmc->enhanced_mode = group->data_c_lsb & 0x10;
		/* bits 0-3 of block 3 contain the MGS */
		tmc->mgs = group->data_c_lsb & 0x0f;
		break;
	case 0x01:
		/* bits 12-13 of block 3 contain the Gap parameters */
		tmc->gap = (group->data_c_msb & 0x30) >> 4;
		/* bits 11-16 of block 3 contain the SID */
		tmc->sid = (((group->data_c_msb & 0x0f) << 2)) |
			(group->data_c_lsb >> 6);
		/* timing information is only valid in enhanced mode */
		if (!tmc->enhanced_mode)
			break;
		/* bits 4-5 of block 3 contain the activity time */
		tmc->t_a = (group->data_c_lsb & 0x30) >> 4;
		/* bits 2-3 of block 3 contain the window time */
		tmc->t_w = (group->data_c_lsb & 0x0c) >> 2;
		/* bits 0-1 of block 3 contain the delay time */
		tmc->t_d = group->data_c_lsb & 0x03;
		break;
	}
	return V4L2_RDS_TMC_SYS;
}

/* decode a single group TMC message */
static uint32_t rds_decode_tmc_single_group(struct rds_private_state *priv_state)
{
	struct v4l2_rds_group *grp = &priv_state->rds_group;
	struct v4l2_rds_tmc_msg msg;

	/* bits 0-2 of group 2 contain the duration value */
	msg.dp = grp->data_b_lsb & 0x07;
	/* bit 15 of block 3 indicates follow diversion advice */
	msg.follow_diversion = grp->data_c_msb & 0x80;
	/* bit 14 of block 3 indicates the direction */
	msg.neg_direction = grp->data_c_msb & 0x40;
	/* bits 11-13 of block 3 contain the extend of the event */
	msg.extent = (grp->data_c_msb & 0x38) >> 3;
	/* bits 0-10 of block 3 contain the event */
	msg.event = ((grp->data_c_msb & 0x07) << 8) | grp->data_c_lsb;
	/* bits 0-15 of block 4 contain the location */
	msg.location = (grp->data_d_msb << 8) | grp->data_c_lsb;
	/* there is no service ID in a single group TMC message, so
	 * just set it to 0. */
	msg.sid = 0;

	/* decoding done, store the new message */
	priv_state->handle.tmc.tmc_msg = msg;
	priv_state->handle.valid_fields |= V4L2_RDS_TMC_SG;
	priv_state->handle.valid_fields &= ~V4L2_RDS_TMC_MG;

	return V4L2_RDS_TMC_SG;
}

/* decode a multi group TMC message and decode the additional fields once
 * a complete group was decoded */
static uint32_t rds_decode_tmc_multi_group(struct rds_private_state *priv_state)
{
	struct v4l2_rds_group *grp = &priv_state->rds_group;
	struct v4l2_rds_tmc_msg *msg = &priv_state->new_tmc_msg;
	bool message_completed = false;
	uint8_t grp_seq_id;
	uint64_t buffer;

	/* bits 12-13 of block 3 contain the group sequence id, for all
	 * multi groups except the first group */
	grp_seq_id = (grp->data_c_msb & 0x30) >> 4;

	/* beginning of a new multigroup ? */
	/* bit 15 of block 3 is the first group indicator */
	if (grp->data_c_msb & 0x80) {
		/* begine decoding of new message */
		memset(msg, 0, sizeof(*msg));
		memset(priv_state->optional_tmc, 0, 112*sizeof(bool)); 
		/* bits 0-3 of block 2 contain continuity index */
		priv_state->continuity_id = grp->data_b_lsb & 0x07;
		/* bit 15 of block 3 indicates follow diversion advice */
		msg->follow_diversion = grp->data_c_msb & 0x80;
		/* bit 14 of block 3 indicates the direction */
		msg->neg_direction = grp->data_c_msb & 0x40;
		/* bits 11-13 of block 3 contain the extend of the event */
		msg->extent = (grp->data_c_msb & 0x38) >> 3;
		/* bits 0-10 of block 3 contain the event */
		msg->event = ((grp->data_c_msb & 0x07) << 8) | grp->data_c_lsb;
		/* bits 0-15 of block 4 contain the location */
		msg->location = (grp->data_d_msb << 8) | grp->data_c_lsb;
	}
	/* second group of multigroup ? */
	/* bit 14 of block 3 ist the second group indicator, and the
	 * group continuity id has to match */
	else if (grp->data_c_msb & 0x40 &&
		(grp->data_b_lsb & 0x07) == priv_state->continuity_id) {
		priv_state->grp_seq_id = grp_seq_id;
		/* store group for later decoding by transforming the bit values
		 * into boolean values and storing them in an array, to ease
		 * further handling */
		msg->length = 1;
		buffer = grp->data_c_msb << 24 | grp->data_c_lsb << 16 |
			grp->data_d_msb << 8 | grp->data_d_lsb;
		/* the buffer contains 28 bits of additional information */
		for (int i = 27; i >= 0; i--) {
			if (buffer & (1 << i))
				priv_state->optional_tmc[27-i] = true;
		}
		if (grp_seq_id == 0)
			message_completed = true;
	}
	/* subsequent groups of multigroup ? */
	/* group continuity id has to match, and group sequence number has
	 * to be smaller by one than the group sequence id */
	else if ((grp->data_b_lsb & 0x07) == priv_state->continuity_id &&
		(grp_seq_id == priv_state->grp_seq_id-1)) {
		priv_state->grp_seq_id = grp_seq_id;
		/* store group for later decoding */
		msg->length += 1;
		buffer = grp->data_c_msb << 24 | grp->data_c_lsb << 16|
			grp->data_d_msb << 8 | grp->data_d_lsb;
		/* the buffer contains 28 bits of additional information */
		for (int i = 27; i >= 0; i--) {
			if (buffer & (1 << i))
				priv_state->optional_tmc[msg->length*28 + 27 - i] = true;
		}
		if (grp_seq_id == 0)
			message_completed = true;
	}

	/* complete message received -> decode additional fields and store
	 * the new message */
	if (message_completed) {
		priv_state->handle.tmc.tmc_msg = *msg;
		rds_tmc_decode_additional(priv_state);
		priv_state->handle.valid_fields |= V4L2_RDS_TMC_MG;
		priv_state->handle.valid_fields &= ~V4L2_RDS_TMC_SG;
	}

	return V4L2_RDS_TMC_MG;
}

/* decode the RDS-TMC tuning information that is contained in type 8A groups
 * (variants 4 to 9) that announce the presence alternative transmitters 
 * providing the same RDS-TMC service */
static uint32_t rds_decode_tmc_tuning(struct rds_private_state *priv_state)
{
	struct v4l2_rds_group *group = &priv_state->rds_group;
	struct v4l2_rds_tmc *tmc = &priv_state->handle.tmc;
	uint8_t variant_code = group->data_b_lsb & 0x0f;
	uint16_t pi_on = (group->data_d_msb << 8) | group->data_d_lsb;
	uint8_t index;

	/* variants 4 and 5 carry the service provider name */
	if (variant_code >= 4 && variant_code <= 5) {
		int offset = 4 * (variant_code - 4);
		tmc->spn[0 + offset] = group->data_c_msb;
		tmc->spn[1 + offset] = group->data_c_lsb;
		tmc->spn[2 + offset] = group->data_d_msb;
		tmc->spn[3 + offset] = group->data_d_lsb;

	/* variant 6 provides specific frequencies for the same RDS-TMC service
	 * on a network with a different PI code */
	/* variant 7 provides mapped frequency pair information which should only
	 * be used if the terminal is tuned to the tuning frequency */
	} else if (variant_code == 6 || variant_code == 7) {
		rds_add_tmc_af(priv_state);

	/* variant 8 indicates up to 2 PI codes of adjacent networks carrying 
	 * the same RDS-TMC service on all transmitters of the network */ 
	} else if (variant_code == 8) {
		uint16_t pi_on_2 = (group->data_c_msb << 8) | group->data_c_lsb;

		/* try to add both transmitted PI codes to the table */
		rds_add_tmc_station(priv_state, pi_on);
		/* PI = 0 is used as a filler code */
		if (pi_on_2 != 0)
			rds_add_tmc_station(priv_state, pi_on_2);

	/* variant 9 provides PI codes of other networks with different system 
	 * parameters */
	} else if (variant_code == 9) {
		index = rds_add_tmc_station(priv_state, pi_on);

		/* bits 0 - 5 contain the service-ID of the ON */
		tmc->tuning.station[index].sid = group->data_c_lsb & 0x3F;
		/* bits 6-10 contain the msg parameters of the ON */
		tmc->tuning.station[index].msg = (group->data_c_msb & 0x03) << 2;
		tmc->tuning.station[index].msg |= (group->data_c_lsb >> 6) & 0x03;
		/* bits 11-15 contain the database-ID of the ON */
		tmc->tuning.station[index].ltn = group->data_c_msb >> 2;
	}

	return V4L2_RDS_TMC_TUNING;
}

static bool rds_add_oda(struct rds_private_state *priv_state, struct v4l2_rds_oda oda)
{
	struct v4l2_rds *handle = &priv_state->handle;

	/* check if there was already an ODA announced for this group type */
	for (int i = 0; i < handle->rds_oda.size; i++) {
		if (handle->rds_oda.oda[i].group_id == oda.group_id) {
			/* update the AID for this ODA */
			handle->rds_oda.oda[i].aid = oda.aid;
			return false;
		}
	}
	/* add the new ODA */
	if (handle->rds_oda.size >= MAX_ODA_CNT)
		return false;
	handle->rds_oda.oda[handle->rds_oda.size++] = oda;
	return true;
}

/* add a new AF to the list, if it doesn't exist yet */
static bool rds_add_af_to_list(struct v4l2_rds_af_set *af_set, uint8_t af, bool is_vhf)
{
	/* convert the frequency to Hz, skip on errors */
	uint32_t freq = rds_decode_af(af, is_vhf);
	if (freq == 0) 
		return false;

	/* prevent buffer overflows */
	if (af_set->size >= MAX_AF_CNT || af_set->size >= af_set->announced_af)
		return false;
	/* check if AF already exists */
	for (int i = 0; i < af_set->size; i++) {
		if (af_set->af[i] == freq)
			return false;
	}
	/* it's a new AF, add it to the list */
	af_set->af[(af_set->size)++] = freq;
	return true;
}

/* extracts the AF information from Block 3 of type 0A groups, and tries
 * to add them to the AF list with a helper function */
static bool rds_add_af(struct rds_private_state *priv_state)
{
	struct v4l2_rds *handle = &priv_state->handle;

	/* AFs are submitted in Block 3 of type 0A groups */
	uint8_t c_msb = priv_state->rds_group.data_c_msb;
	uint8_t c_lsb = priv_state->rds_group.data_c_lsb;
	bool updated_af = false;
	struct v4l2_rds_af_set *af_set = &handle->rds_af;

	/* the 4 8-bit values in the block's data fields (c_msb/c_lsb,
	 * d_msb/d_lsb) represent either a carrier frequency (1..204)
	 * or a special meaning (205..255).
	 * Translation tables can be found in IEC 62106 section 6.2.1.6 */

	/* 250: LF / MF frequency follows */
	if (c_msb == 250) {
		if (rds_add_af_to_list(af_set, c_lsb, false))
			updated_af = true;
		c_lsb = 0; /* invalidate */
	}
	/* 224..249: announcement of AF count (224=0, 249=25) */
	if (c_msb >= 224 && c_msb <= 249)
		af_set->announced_af = c_msb - 224;
	/* check if the data represents an AF (for 1 =< val <= 204 the
	 * value represents an AF) */
	if (c_msb < 205)
		if (rds_add_af_to_list(af_set, c_msb, true))
			updated_af = true;
	if (c_lsb < 205)
		if (rds_add_af_to_list(af_set, c_lsb, true))
			updated_af = true;
	/* did we receive all announced AFs? */
	if (af_set->size >= af_set->announced_af && af_set->announced_af != 0)
		handle->valid_fields |= V4L2_RDS_AF;
	return updated_af;
}



/* adds one char of the ps name to temporal storage, the value is validated
 * if it is received twice in a row
 * @pos:	position of the char within the PS name (0..7)
 * @ps_char:	the new character to be added
 * @return:	true, if all 8 temporal ps chars have been validated */
static bool rds_add_ps(struct rds_private_state *priv_state, uint8_t pos, uint8_t ps_char)
{
	if (ps_char == priv_state->new_ps[pos]) {
		priv_state->new_ps_valid[pos] = 1;
	} else {
		priv_state->new_ps[pos] = ps_char;
		memset(priv_state->new_ps_valid, 0, 8);
	}

	/* check if all ps positions have been validated */
	for (int i = 0; i < 8; i++)
		if (priv_state->new_ps_valid[i] != 1)
			return false;
	return true;
}

/* checks if an entry for the given PI already exists and returns the index
 * of that entry if so. Else it adds a new entry to the EON table and returns
 * the index of the new field */
static uint8_t rds_add_eon_entry(struct rds_private_state *priv_state, uint16_t pi)
{
	struct v4l2_rds *handle = &priv_state->handle;
	uint8_t index = handle->rds_eon.index;
	uint8_t size = handle->rds_eon.size;

	/* check if there's an entry for the given PI key */
	for (int i = 0; i < handle->rds_eon.size; i++) {
		if (handle->rds_eon.eon[i].pi == pi) {
			return i;
		}
	}
	/* if the the maximum table size is reached, overwrite old
	 * entries, starting at the oldest one = 0 */
	handle->rds_eon.eon[index].pi = pi;
	handle->rds_eon.eon[index].valid_fields |= V4L2_RDS_PI;
	handle->rds_eon.index = (index+1 < MAX_EON_CNT) ? (index+1) : 0;
	handle->rds_eon.size = (size+1 <= MAX_EON_CNT) ? (size+1) : MAX_EON_CNT;
	return index;
}

/* checks if an entry for the given PI already exists */
static bool rds_check_eon_entry(struct rds_private_state *priv_state, uint16_t pi)
{
	struct v4l2_rds *handle = &priv_state->handle;

	/* check if there's an entry for the given PI key */
	for (int i = 0; i <= handle->rds_eon.size; i++) {
		if (handle->rds_eon.eon[i].pi == pi) {
			return true;
		}
	}
	return false;
}

/* group of functions to decode successfully received RDS groups into
 * easily accessible data fields
 *
 * group 0: basic tuning and switching */
static uint32_t rds_decode_group0(struct rds_private_state *priv_state)
{
	struct v4l2_rds *handle = &priv_state->handle;
	struct v4l2_rds_group *grp = &priv_state->rds_group;
	bool new_ps = false;
	bool tmp;
	uint32_t updated_fields = 0;

	/* bit 4 of block B contains the TA flag */
	tmp = grp->data_b_lsb & 0x10;
	if (handle->ta != tmp) {
		handle->ta = tmp;
		updated_fields |= V4L2_RDS_TA;
	}
	handle->valid_fields |= V4L2_RDS_TA;

	/* bit 3 of block B contains the Music/Speech flag */
	tmp = grp->data_b_lsb & 0x08;
	if (handle->ms != tmp) {
		handle->ms = tmp;
		updated_fields |= V4L2_RDS_MS;
	}
	handle->valid_fields |= V4L2_RDS_MS;

	/* bit 0-1 of block b contain program service name and decoder
	 * control segment address */
	uint8_t segment = grp->data_b_lsb & 0x03;

	/* put the received station-name characters into the correct position
	 * of the station name, and check if the new PS is validated */
	rds_add_ps(priv_state, segment * 2, grp->data_d_msb);
	new_ps = rds_add_ps(priv_state, segment * 2 + 1, grp->data_d_lsb);
	if (new_ps) {
		/* check if new PS is the same as the old one */
		if (memcmp(priv_state->new_ps, handle->ps, 8) != 0) {
			memcpy(handle->ps, priv_state->new_ps, 8);
			updated_fields |= V4L2_RDS_PS;
		}
		handle->valid_fields |= V4L2_RDS_PS;
	}

	/* bit 2 of block B contains 1 bit of the Decoder Control Information (DI)
	 * the segment number defines the bit position
	 * New bits are only accepted the segments arrive in the correct order */
	bool bit2 = grp->data_b_lsb & 0x04;
	if (segment == 0 || segment == priv_state->next_di_segment) {
		switch (segment) {
		case 0:
			priv_state->new_di = set_bit(priv_state->new_di,
				V4L2_RDS_FLAG_STEREO, bit2);
			priv_state->next_di_segment = 1;
			break;
		case 1:
			priv_state->new_di = set_bit(priv_state->new_di,
				V4L2_RDS_FLAG_ARTIFICIAL_HEAD, bit2);
			priv_state->next_di_segment = 2;
			break;
		case 2:
			priv_state->new_di = set_bit(priv_state->new_di,
				V4L2_RDS_FLAG_COMPRESSED, bit2);
			priv_state->next_di_segment = 3;
			break;
		case 3:
			priv_state->new_di = set_bit(priv_state->new_di,
				V4L2_RDS_FLAG_STATIC_PTY, bit2);
			/* check if the value of DI has changed, and store
			 * and signal DI update in case */
			if (handle->di != priv_state->new_di) {
				handle->di = priv_state->new_di;
				updated_fields |= V4L2_RDS_DI;
			}
			priv_state->next_di_segment = 0;
			handle->valid_fields |= V4L2_RDS_DI;
			break;
		}
	} else {
		/* wrong order of DI segments -> restart */
		priv_state->next_di_segment = 0;
		priv_state->new_di = 0;
	}

	/* version A groups contain AFs in block C */
	if (grp->group_version == 'A')
		if (rds_add_af(priv_state))
			updated_fields |= V4L2_RDS_AF;

	return updated_fields;
}

/* group 1: slow labeling codes & program item number */
static uint32_t rds_decode_group1(struct rds_private_state *priv_state)
{
	struct v4l2_rds *handle = &priv_state->handle;
	struct v4l2_rds_group *grp = &priv_state->rds_group;
	uint32_t updated_fields = 0;
	uint8_t variant_code = 0;

	/* version A groups contain slow labeling codes,
	 * version B groups only contain program item number which is a
	 * very uncommonly used feature */
	if (grp->group_version != 'A')
		return 0;

	/* bit 14-12 of block c contain the variant code */
	variant_code = (grp->data_c_msb >> 4) & 0x07;
	if (variant_code == 0) {
		/* var 0 -> ECC, only accept if same lc is
		 * received twice */
		if (grp->data_c_lsb == priv_state->new_ecc) {
			handle->valid_fields |= V4L2_RDS_ECC;
			if (handle->ecc != grp->data_c_lsb)
				updated_fields |= V4L2_RDS_ECC;
			handle->ecc = grp->data_c_lsb;
		} else {
			priv_state->new_ecc = grp->data_c_lsb;
		}
	} else if (variant_code == 0x03) {
		/* var 0x03 -> Language Code, only accept if same lc is
		 * received twice */
		if (grp->data_c_lsb == priv_state->new_lc) {
			handle->valid_fields |= V4L2_RDS_LC;
			updated_fields |= V4L2_RDS_LC;
			handle->lc = grp->data_c_lsb;
		} else {
			priv_state->new_lc = grp->data_c_lsb;
		}
	}
	return updated_fields;
}

/* group 2: radio text */
static uint32_t rds_decode_group2(struct rds_private_state *priv_state)
{
	struct v4l2_rds *handle = &priv_state->handle;
	struct v4l2_rds_group *grp = &priv_state->rds_group;
	uint32_t updated_fields = 0;

	/* bit 0-3 of block B contain the segment code */
	uint8_t segment = grp->data_b_lsb & 0x0f;
	/* bit 4 of block b contains the A/B text flag (new radio text
	 * will be transmitted) */
	bool rt_ab_flag_n = grp->data_b_lsb & 0x10;

	/* new Radio Text will be transmitted */
	if (rt_ab_flag_n != handle->rt_ab_flag) {
		handle->rt_ab_flag = rt_ab_flag_n;
		memset(handle->rt, 0, 64);
		handle->valid_fields &= ~V4L2_RDS_RT;
		updated_fields |= V4L2_RDS_RT;
		priv_state->next_rt_segment = 0;
	}

	/* further decoding of data depends on type of message (A or B)
	 * Type A allows RTs with a max length of 64 chars
	 * Type B allows RTs with a max length of 32 chars */
	if (grp->group_version == 'A') {
		if (segment == 0 || segment == priv_state->next_rt_segment) {
			priv_state->new_rt[segment * 4] = grp->data_c_msb;
			priv_state->new_rt[segment * 4 + 1] = grp->data_c_lsb;
			priv_state->new_rt[segment * 4 + 2] = grp->data_d_msb;
			priv_state->new_rt[segment * 4 + 3] = grp->data_d_lsb;
			priv_state->next_rt_segment = segment + 1;
			if (segment == 0x0f) {
				handle->rt_length = 64;
				handle->valid_fields |= V4L2_RDS_RT;
				if (memcmp(handle->rt, priv_state->new_rt, 64)) {
					memcpy(handle->rt, priv_state->new_rt, 64);
					updated_fields |= V4L2_RDS_RT;
				}
				priv_state->next_rt_segment = 0;
			}
		}
	} else {
		if (segment == 0 || segment == priv_state->next_rt_segment) {
			priv_state->new_rt[segment * 2] = grp->data_d_msb;
			priv_state->new_rt[segment * 2 + 1] = grp->data_d_lsb;
			/* PI code in block C will be ignored */
			priv_state->next_rt_segment = segment + 1;
			if (segment == 0x0f) {
				handle->rt_length = 32;
				handle->valid_fields |= V4L2_RDS_RT;
				updated_fields |= V4L2_RDS_RT;
				if (memcmp(handle->rt, priv_state->new_rt, 32)) {
					memcpy(handle->rt, priv_state->new_rt, 32);
					updated_fields |= V4L2_RDS_RT;
				}
				priv_state->next_rt_segment = 0;
			}
		}
	}

	/* determine if complete rt was received
	 * a carriage return (0x0d) can end a message early */
	for (int i = 0; i < 64; i++) {
		if (priv_state->new_rt[i] == 0x0d) {
			/* replace CR with terminating character */
			priv_state->new_rt[i] = '\0';
			handle->rt_length = i;
			handle->valid_fields |= V4L2_RDS_RT;
			if (memcmp(handle->rt, priv_state->new_rt, handle->rt_length)) {
					memcpy(handle->rt, priv_state->new_rt,
						handle->rt_length);
					updated_fields |= V4L2_RDS_RT;
				}
			priv_state->next_rt_segment = 0;
		}
	}
	return updated_fields;
}

/* group 3: Open Data Announcements */
static uint32_t rds_decode_group3(struct rds_private_state *priv_state)
{
	struct v4l2_rds *handle = &priv_state->handle;
	struct v4l2_rds_group *grp = &priv_state->rds_group;
	struct v4l2_rds_oda new_oda;
	uint32_t updated_fields = 0;

	if (grp->group_version != 'A')
		return 0;

	/* 0th bit of block b contains Group Type Info version of announced ODA
	 * Group Type info: 0 = A, 1 = B */
	new_oda.group_version = (grp->data_b_lsb & 0x01) ? 'B' : 'A';
	/* 1st to 4th bit contain Group ID of announced ODA */
	new_oda.group_id = (grp->data_b_lsb & 0x1e) >> 1;
	/* block D contains the 16bit Application Identification Code */
	new_oda.aid = (grp->data_d_msb << 8) | grp->data_d_lsb;

	/* try to add the new ODA to the set of defined ODAs */
	if (rds_add_oda(priv_state, new_oda)) {
		handle->decode_information |= V4L2_RDS_ODA;
		updated_fields |= V4L2_RDS_ODA;
	}

	/* if it's a TMC announcement decode the contained information */
	if (new_oda.aid == 0xcd46 || new_oda.aid == 0xcd47) {
		rds_decode_tmc_system(priv_state);
	}

	return updated_fields;
}

/* decodes the RDS date/time representation into a standard c representation
 * that can be used with c-library functions */
static time_t rds_decode_mjd(const struct rds_private_state *priv_state)
{
	struct tm new_time;
	int y, m, d, k = 0;
	/* offset is given in multiples of half hrs */
	uint32_t offset = priv_state->utc_offset & 0x1f;
	uint32_t local_mjd = priv_state->new_mjd;
	uint8_t local_hour = priv_state->utc_hour;
	uint8_t local_minute = priv_state->utc_minute;

	/* add / subtract the local offset to get the local time.
	 * The offset is expressed in multiples of half hours */
	if (priv_state->utc_offset & 0x20) { /* bit 5 indicates -/+ */
		local_hour -= (offset * 2);
		local_minute -= (offset % 2) * 30;
	} else {
		local_hour += (offset * 2);
		local_minute += (offset % 2) * 30;
	}

	/* the formulas for the conversion are taken from Annex G of the
	 * IEC 62106 RDS standard */
	y = (int)((local_mjd - 15078.2) / 365.25);
	m = (int)((local_mjd - 14956.1 - (int)(y * 365.25)) / 30.6001);
	d = (int)(local_mjd - 14956 - (int)(y * 365.25) - (int)(m * 30.6001));
	if (m == 14 || m == 15)
		k = 1;
	y = y + k;
	m = m - 1 - k*12;

	/* put the values into a tm struct for conversion into time_t value */
	new_time.tm_sec = 0;
	new_time.tm_min = local_minute;
	new_time.tm_hour = local_hour;
	new_time.tm_mday = d;
	new_time.tm_mon = m;
	new_time.tm_year = y;
	/* offset (submitted by RDS) that was used to compute the local time,
	 * expressed in multiples of half hours, bit 5 indicates -/+ */
	if (priv_state->utc_offset & 0x20)
		new_time.tm_gmtoff = -2 * offset * 3600;
	else
		new_time.tm_gmtoff = 2 * offset * 3600;

	/* convert tm struct to time_t value and return it */
	return mktime(&new_time);
}

/* group 4: Date and Time */
static uint32_t rds_decode_group4(struct rds_private_state *priv_state)
{
	struct v4l2_rds *handle = &priv_state->handle;
	struct v4l2_rds_group *grp = &priv_state->rds_group;
	uint32_t mjd;
	uint32_t updated_fields = 0;

	if (grp->group_version != 'A')
		return 0;

	/* bits 0-1 of block b lsb contain bits 15 and 16 of Julian day code
	 * bits 0-7 of block c msb contain bits 7 to 14 of Julian day code
	 * bits 1-7 of block c lsb contain bits 0 to 6 of Julian day code */
	mjd = ((grp->data_b_lsb & 0x03) << 15) |
		(grp->data_c_msb << 7) | (grp->data_c_lsb >> 1);
	/* the same mjd has to be received twice in order to accept the data */
	if (priv_state->new_mjd != mjd) {
		priv_state->new_mjd = mjd;
		return 0;
	}
	/* same mjd received at least twice --> decode time & date */

	/* bit 0 of block c lsb contains bit 4 of utc_hour
	 * bits 4-7 of block d contains bits 0 to 3 of utc_hour */
	priv_state->utc_hour = ((grp->data_c_lsb & 0x01) << 4) |
		(grp->data_d_msb >> 4);

	/* bits 0-3 of block d msb contain bits 2 to 5 of utc_minute
	 * bits 6-7 of block d lsb contain bits 0 and 1 utc_minute */
	priv_state->utc_minute = ((grp->data_d_msb & 0x0f) << 2) |
		(grp->data_d_lsb >> 6);

	/* bits 0-5 of block d lsb contain bits 0 to 5 of local time offset */
	priv_state->utc_offset = grp->data_d_lsb & 0x3f;

	/* decode RDS time representation into commonly used c representation */
	handle->time = rds_decode_mjd(priv_state);
	updated_fields |= V4L2_RDS_TIME;
	handle->valid_fields |= V4L2_RDS_TIME;
	return updated_fields;
}

/* group 8A: TMC */
static uint32_t rds_decode_group8(struct rds_private_state *priv_state)
{
	struct v4l2_rds_group *grp = &priv_state->rds_group;
	uint8_t tuning_variant;

	/* TMC uses version A exclusively */
	if (grp->group_version != 'A')
		return 0;

	/* check if the same group was received twice, store new rds group
	 * and return early if the old group doesn't match the new one */
	if (!rds_compare_group(&priv_state->prev_tmc_group, &priv_state->rds_group)) {
		priv_state->prev_tmc_group = priv_state->rds_group;
		return 0;
	}
	/* modify the old group, to prevent that the same TMC message is decoded
	 * again in the next iteration (the default number of repetitions for
	 * RDS-TMC groups is 3) */
	priv_state->prev_tmc_group.group_version = 0;

	/* handle the new TMC data depending on the message type */
	/* -> single group message */
	if ((grp->data_b_lsb & V4L2_TMC_SINGLE_GROUP) &&
		!(grp->data_b_lsb & V4L2_TMC_TUNING_INFO)) {
		return rds_decode_tmc_single_group(priv_state);
	}
	/* -> multi group message */
	if (!(grp->data_b_lsb & V4L2_TMC_SINGLE_GROUP) &&
		!(grp->data_b_lsb & V4L2_TMC_TUNING_INFO)) {
		return rds_decode_tmc_multi_group(priv_state);
	}
	/* -> tuning information message, defined for variants 4..9,
	 * submitted in bits 0-3 of block 2 */
	tuning_variant = grp->data_b_lsb & 0x0f;
	if ((grp->data_b_lsb & V4L2_TMC_TUNING_INFO) && tuning_variant >= 4 &&
		tuning_variant <= 9) {
		priv_state->handle.valid_fields |= V4L2_RDS_TMC_TUNING;
		return rds_decode_tmc_tuning(priv_state);
	}

	return 0;
}

/* group 10: Program Type Name */
static uint32_t rds_decode_group10(struct rds_private_state *priv_state)
{
	struct v4l2_rds *handle = &priv_state->handle;
	struct v4l2_rds_group *grp = &priv_state->rds_group;
	uint32_t updated_fields = 0;
	uint8_t ptyn_tmp[4];

	/* bit 0 of block B contain the segment code */
	uint8_t segment_code = grp->data_b_lsb & 0x01;
	/* bit 4 of block b contains the A/B text flag (new ptyn
	 * will be transmitted) */
	bool ptyn_ab_flag_n = grp->data_b_lsb & 0x10;

	if (grp->group_version != 'A')
		return 0;

	/* new Program Type Text will be transmitted */
	if (ptyn_ab_flag_n != handle->ptyn_ab_flag) {
		handle->ptyn_ab_flag = ptyn_ab_flag_n;
		memset(handle->ptyn, 0, 8 * sizeof(char));
		memset(priv_state->new_ptyn, 0, 8 * sizeof(char));
		memset(priv_state->new_ptyn_valid, 0, 2 * sizeof(bool));
		handle->valid_fields &= ~V4L2_RDS_PTYN;
		updated_fields |= V4L2_RDS_PTYN;
	}
	/* copy chars to designated position within temp text field */
	ptyn_tmp[0] = grp->data_c_msb;
	ptyn_tmp[1] = grp->data_c_lsb;
	ptyn_tmp[2] = grp->data_d_msb;
	ptyn_tmp[3] = grp->data_d_lsb;

	/* only validate ptyn segment if the same data is received twice */
	if (memcmp(ptyn_tmp, priv_state->new_ptyn[segment_code], 4) == 0) {
		priv_state->new_ptyn_valid[segment_code] = true;
	} else {
		for (int i = 0; i < 4; i++)
			priv_state->new_ptyn[segment_code][i] = ptyn_tmp[i];
		priv_state->new_ptyn_valid[segment_code] = false;
	}

	/* if both ptyn segments have been validated, accept the new ptyn */
	if (priv_state->new_ptyn_valid[0] && priv_state->new_ptyn_valid[1]) {
		for (int i = 0; i < 4; i++) {
			handle->ptyn[i] = priv_state->new_ptyn[0][i];
			handle->ptyn[4 + i] = priv_state->new_ptyn[1][i];
		}
		handle->valid_fields |= V4L2_RDS_PTYN;
		updated_fields |= V4L2_RDS_PTYN;
	}
	return updated_fields;
}

/* group 14: EON (Enhanced Other Network) information */
static uint32_t rds_decode_group14(struct rds_private_state* priv_state)
{
	struct v4l2_rds *handle = &priv_state->handle;
	struct v4l2_rds_group *grp = &priv_state->rds_group;
	struct v4l2_rds_eon *eon_entry;
	uint32_t updated_fields = 0;
	uint16_t pi_on;
	uint16_t lsf_on;
	uint8_t variant_code;
	uint8_t eon_index;
	uint8_t pty_on;
	bool tp_on, ta_on;
	bool new_a = false, new_b = false;

	if (grp->group_version != 'A')
		return 0;

	/* bits 0-3 of group b contain the variant code */
	variant_code = grp->data_b_lsb & 0x0f;

	/* group d contains the PI code of the ON (Other Network) */
	pi_on = (grp->data_d_msb << 8) | grp->data_d_lsb;

	/* bit 4 of group b contains the TP status of the ON*/
	tp_on = grp->data_b_lsb & 0x10;
	if (rds_check_eon_entry(priv_state, pi_on)) {
		/* if there's an entry for this PI(ON) update the TP field */
		eon_index = rds_add_eon_entry(priv_state, pi_on);
		eon_entry = &handle->rds_eon.eon[eon_index];
		eon_entry->tp = tp_on;
		eon_entry->valid_fields |= V4L2_RDS_TP;
		updated_fields |= V4L2_RDS_EON;
	}

	/* perform group variant dependent decoding */
	if ((variant_code >=5 && variant_code <= 11) || variant_code >= 14) {
		/* 5-9 = mapped FM frequencies -> unsupported
		 * 10-11 = unallocated
		 * 14 = PIN(ON) -> unsupported (unused RDS feature)
		 * 15 = reserved for broadcasters use */
		return updated_fields;
	}
	
	/* retrieve the EON entry corresponding to the PI(ON) code or add a new
	 * entry to the table if no entry exists */
	eon_index = rds_add_eon_entry(priv_state, pi_on);
	eon_entry = &handle->rds_eon.eon[eon_index];

	/* PS Name */
	if (variant_code < 4) {
		eon_entry->ps[variant_code*2] = grp->data_c_msb;
		eon_entry->ps[variant_code*2+1] = grp->data_c_lsb;
		eon_entry->valid_fields |= V4L2_RDS_PS;
		updated_fields |= V4L2_RDS_EON;
	}
	/* Alternative frequencies */
	else if (variant_code == 4) {
		uint8_t c_msb = grp->data_c_msb;
		uint8_t c_lsb = grp->data_c_lsb;

		/* 224..249: announcement of AF count (224=0, 249=25) */
		if (c_msb >= 224 && c_msb <= 249)
			eon_entry->af.announced_af = c_msb - 224;
		/* check if the data represents an AF (for 1 =< val <= 204 the
		 * value represents an AF) */
		if (c_msb < 205)
			new_a = rds_add_af_to_list(&eon_entry->af,
					grp->data_c_msb, true);
		if (c_lsb < 205)
			new_b = rds_add_af_to_list(&eon_entry->af,
					grp->data_c_lsb, true);
		/* check if one of the frequencies was previously unknown */
		if (new_a || new_b) {
			eon_entry->valid_fields |= V4L2_RDS_AF;
			updated_fields |= V4L2_RDS_EON;
		}
	}
	/* Linkage information */
	else if (variant_code == 12) {
		/* group c contains the lsf code */
		lsf_on = (grp->data_c_msb << 8) | grp->data_c_lsb;
		/* check if the lsf code is already known */
		new_a = (eon_entry->lsf == lsf_on);
		if (new_a) {
			eon_entry->lsf = lsf_on;
			eon_entry->valid_fields |= V4L2_RDS_LSF;
			updated_fields |= V4L2_RDS_EON;
		}
	}
	/* PTY(ON) and TA(ON) */
	else if (variant_code == 13) {
		/* bits 15-10 of group c contain the PTY(ON) */
		pty_on = grp->data_c_msb >> 3;
		/* bit 0 of group c contains the TA code */
		ta_on = grp->data_c_lsb & 0x01;
		/* check if the data is new */
		new_a = (eon_entry->pty == pty_on);
		if (new_a) {
			eon_entry->pty = pty_on;
			eon_entry->valid_fields |= V4L2_RDS_PTY;
		}
		new_b = (eon_entry->ta == ta_on);
		eon_entry->ta = ta_on;
		eon_entry->valid_fields |= V4L2_RDS_TA;
		if (new_a || new_b)
			updated_fields |= V4L2_RDS_EON;
	}
	/* set valid field for EON data, if EON table contains entries */
	if (handle->rds_eon.size > 0)
		handle->valid_fields |= V4L2_RDS_EON;

	return updated_fields;
}

typedef uint32_t (*decode_group_func)(struct rds_private_state *);

/* array of function pointers to contain all group specific decoding functions */
static const decode_group_func decode_group[16] = {
	[0] = rds_decode_group0,
	[1] = rds_decode_group1,
	[2] = rds_decode_group2,
	[3] = rds_decode_group3,
	[4] = rds_decode_group4,
	[8] = rds_decode_group8,
	[10] = rds_decode_group10,
	[14] = rds_decode_group14
};

static uint32_t rds_decode_group(struct rds_private_state *priv_state)
{
	struct v4l2_rds *handle = &priv_state->handle;
	uint8_t group_id = priv_state->rds_group.group_id;

	/* count the group type, and decode it if it is supported */
	handle->rds_statistics.group_type_cnt[group_id]++;
	if (decode_group[group_id])
		return (*decode_group[group_id])(priv_state);
	return 0;
}

struct v4l2_rds *v4l2_rds_create(bool is_rbds)
{
	struct rds_private_state *internal_handle =
		calloc(1, sizeof(struct rds_private_state));
	internal_handle->handle.is_rbds = is_rbds;

	return (struct v4l2_rds *)internal_handle;
}

void v4l2_rds_destroy(struct v4l2_rds *handle)
{
	if (handle)
		free(handle);
}

void v4l2_rds_reset(struct v4l2_rds *handle, bool reset_statistics)
{
	/* treat the private & the public part of the handle */
	struct rds_private_state *priv_state = (struct rds_private_state *) handle;

	/* store members of handle that shouldn't be affected by reset */
	bool is_rbds = handle->is_rbds;
	struct v4l2_rds_statistics rds_statistics = handle->rds_statistics;

	/* reset the handle */
	memset(priv_state, 0, sizeof(*priv_state));
	/* re-initialize members */
	handle->is_rbds = is_rbds;
	if (!reset_statistics)
		handle->rds_statistics = rds_statistics;
}

/* function decodes raw RDS data blocks into complete groups. Once a full group is
 * successfully received, the group is decoded into the fields of the RDS handle.
 * Decoding is only done once a complete group was received. This is slower compared
 * to decoding the group type independent information up front, but adds a barrier
 * against corrupted data (happens regularly when reception is weak) */
uint32_t v4l2_rds_add(struct v4l2_rds *handle, struct v4l2_rds_data *rds_data)
{
	struct rds_private_state *priv_state = (struct rds_private_state *) handle;
	struct v4l2_rds_data *rds_data_raw = priv_state->rds_data_raw;
	struct v4l2_rds_statistics *rds_stats = &handle->rds_statistics;
	uint32_t updated_fields = 0;
	uint8_t *decode_state = &(priv_state->decode_state);

	/* get the block id by masking out irrelevant bits */
	int block_id = rds_data->block & V4L2_RDS_BLOCK_MSK;

	rds_stats->block_cnt++;
	/* check for corrected / uncorrectable errors in the data */
	if (rds_data->block & V4L2_RDS_BLOCK_ERROR) {
		block_id = -1;
		rds_stats->block_error_cnt++;
	} else if (rds_data->block & V4L2_RDS_BLOCK_CORRECTED) {
		rds_stats->block_corrected_cnt++;
	}

	switch (*decode_state) {
	case RDS_EMPTY:
		if (block_id == 0) {
			*decode_state = RDS_A_RECEIVED;
			/* begin reception of a new data group, reset raw buffer to 0 */
			memset(rds_data_raw, 0, sizeof(*rds_data_raw));
			rds_data_raw[0] = *rds_data;
		} else {
			/* ignore block if it is not the first block of a group */
			rds_stats->group_error_cnt++;
		}
		break;

	case RDS_A_RECEIVED:
		if (block_id == 1) {
			*decode_state = RDS_B_RECEIVED;
			rds_data_raw[1] = *rds_data;
		} else {
			/* received block with unexpected block id, reset state machine */
			rds_stats->group_error_cnt++;
			*decode_state = RDS_EMPTY;
		}
		break;

	case RDS_B_RECEIVED:
		/* handle type C and C' blocks alike */
		if (block_id == 2 || block_id ==  4) {
			*decode_state = RDS_C_RECEIVED;
			rds_data_raw[2] = *rds_data;
		} else {
			rds_stats->group_error_cnt++;
			*decode_state = RDS_EMPTY;
		}
		break;

	case RDS_C_RECEIVED:
		if (block_id == 3) {
			*decode_state = RDS_EMPTY;
			rds_data_raw[3] = *rds_data;
			/* a full group was received */
			rds_stats->group_cnt++;
			/* decode group type independent fields */
			memset(&priv_state->rds_group, 0, sizeof(priv_state->rds_group));
			updated_fields |= rds_decode_a(priv_state, &rds_data_raw[0]);
			updated_fields |= rds_decode_b(priv_state, &rds_data_raw[1]);
			rds_decode_c(priv_state, &rds_data_raw[2]);
			rds_decode_d(priv_state, &rds_data_raw[3]);
			/* decode group type dependent fields */
			updated_fields |= rds_decode_group(priv_state);
			return updated_fields;
		}
		rds_stats->group_error_cnt++;
		*decode_state = RDS_EMPTY;
		break;

	default:
		/* every unexpected block leads to a reset of the sm */
		rds_stats->group_error_cnt++;
		*decode_state = RDS_EMPTY;
	}
	/* if we reach here, no RDS group was completed */
	return 0;
}

const char *v4l2_rds_get_pty_str(const struct v4l2_rds *handle)
{
	const uint8_t pty = handle->pty;

	if (pty >= 32)
		return NULL;

	static const char *rds_lut[32] = {
		"None", "News", "Affairs", "Info", "Sport", "Education", "Drama",
		"Culture", "Science", "Varied Speech", "Pop Music",
		"Rock Music", "Easy Listening", "Light Classics M",
		"Serious Classics", "Other Music", "Weather", "Finance",
		"Children", "Social Affairs", "Religion", "Phone In",
		"Travel & Touring", "Leisure & Hobby", "Jazz Music",
		"Country Music", "National Music", "Oldies Music", "Folk Music",
		"Documentary", "Alarm Test", "Alarm!"
	};
	static const char *rbds_lut[32] = {
		"None", "News", "Information", "Sports", "Talk", "Rock",
		"Classic Rock", "Adult Hits", "Soft Rock", "Top 40", "Country",
		"Oldies", "Soft", "Nostalgia", "Jazz", "Classical",
		"R&B", "Soft R&B", "Foreign Language", "Religious Music",
		"Religious Talk", "Personality", "Public", "College",
		"Spanish Talk", "Spanish Music", "Hip-Hop", "Unassigned",
		"Unassigned", "Weather", "Emergency Test", "Emergency"
	};

	return handle->is_rbds ? rbds_lut[pty] : rds_lut[pty];
}

const char *v4l2_rds_get_country_str(const struct v4l2_rds *handle)
{
	/* defines the  region of the world
	 * 0x0e = Europe, 0x0d = Africa, 0x0a = ITU Region 2,
	 * 0x0f = ITU Region 3 */
	uint8_t ecc_h = handle->ecc >> 4;
	/* sub identifier for the region, valid range 0..4 */
	uint8_t ecc_l = handle->ecc & 0x0f;
	/* bits 12-15 pi contain the country code */
	uint8_t country_code = handle->pi >> 12;

	/* LUT for European countries
	 * the standard doesn't define every possible value but leaves some
	 * undefined. An exception is e4-7 which is defined as a dash ("-") */
	static const char *e_lut[5][16] = {
	{
		NULL, "DE", "DZ", "AD", "IL", "IT", "BE", "RU", "PS", "AL",
		"AT", "HU", "MT", "DE", NULL, "EG"
	}, {
		NULL, "GR", "CY", "SM", "CH", "JO", "FI", "LU", "BG", "DK",
		"GI", "IQ", "GB", "LY", "RO", "FR"
	}, {
		NULL, "MA", "CZ", "PL", "VA", "SK", "SY", "TN", NULL, "LI",
		"IS", "MC", "LT", "RS", "ES", "NO"
	}, {
		NULL, "ME", "IE", "TR", "MK", NULL, NULL, NULL, "NL", "LV",
		"LB", "AZ", "HR", "KZ", "SE", "BY"
	}, {
		NULL, "MD", "EE", "KG", NULL, NULL, "UA", "-", "PT", "SI",
		"AM", NULL, "GE", NULL, NULL, "BA"
	}
	};

	/* for now only European countries are supported -> ECC E0 - E4
	 * but the standard defines country codes for the whole world,
	 * that's the reason for returning "unknown" instead of a NULL
	 * pointer until all defined countries are supported */
	if (ecc_h == 0x0e && ecc_l <= 0x04)
		return e_lut[ecc_l][country_code];
	return "Unknown";
}

static const char *rds_language_lut(const uint8_t lc)
{
	const uint8_t max_lc = 127;
	const char *language;

	static const char *language_lut[128] = {
		"Unknown", "Albanian", "Breton", "Catalan",
		"Croatian", "Welsh", "Czech", "Danish",
		"German", "English", "Spanish", "Esperanto",
		"Estonian", "Basque", "Faroese", "French",
		"Frisian", "Irish", "Gaelic", "Galician",
		"Icelandic", "Italian", "Lappish", "Latin",
		"Latvian", "Luxembourgian", "Lithuanian", "Hungarian",
		"Maltese", "Dutch", "Norwegian", "Occitan",
		"Polish", "Portuguese", "Romanian", "Ramansh",
		"Serbian", "Slovak", "Slovene", "Finnish",
		"Swedish", "Turkish", "Flemish", "Walloon",
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, "Zulu", "Vietnamese", "Uzbek",
		"Urdu", "Ukrainian", "Thai", "Telugu",
		"Tatar", "Tamil", "Tadzhik", "Swahili",
		"Sranan Tongo", "Somali", "Sinhalese", "Shona",
		"Serbo-Croat", "Ruthenian", "Russian", "Quechua",
		"Pushtu", "Punjabi", "Persian", "Papamiento",
		"Oriya", "Nepali", "Ndebele", "Marathi",
		"Moldavian", "Malaysian", "Malagasay", "Macedonian",
		"Laotian", "Korean", "Khmer", "Kazahkh",
		"Kannada", "Japanese", "Indonesian", "Hindi",
		"Hebrew", "Hausa", "Gurani", "Gujurati",
		"Greek", "Georgian", "Fulani", "Dani",
		"Churash", "Chinese", "Burmese", "Bulgarian",
		"Bengali", "Belorussian", "Bambora", "Azerbaijani",
		"Assamese", "Armenian", "Arabic", "Amharic"
	};

	/* filter invalid values and undefined table entries */
	language = (lc > max_lc) ? "Unknown" : language_lut[lc];
	if (!language)
		return "Unknown";
	return language;
}

const char *v4l2_rds_get_language_str(const struct v4l2_rds *handle)
{
	return rds_language_lut(handle->lc);
}

const char *v4l2_rds_get_coverage_str(const struct v4l2_rds *handle)
{
	/* bits 8-11 contain the area coverage code */
	uint8_t coverage = (handle->pi >> 8) & 0X0f;
	static const char *coverage_lut[16] = {
		"Local", "International", "National", "Supra-Regional",
		"Regional 1", "Regional 2", "Regional 3", "Regional 4",
		"Regional 5", "Regional 6", "Regional 7", "Regional 8",
		"Regional 9", "Regional 10", "Regional 11", "Regional 12"
	};

	return coverage_lut[coverage];
}

const struct v4l2_rds_group *v4l2_rds_get_group(const struct v4l2_rds *handle)
{
	struct rds_private_state *priv_state = (struct rds_private_state *) handle;
	return &priv_state->rds_group;
}
