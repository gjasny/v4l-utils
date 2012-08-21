  /*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab <mchehab@redhat.com>
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
 */

/*
 * Descriptors, as defined on ETSI EN 300 468 V1.11.1 (2010-04)
 */


#ifndef _DESCRIPTORS_H
#define _DESCRIPTORS_H

#include <arpa/inet.h>
#include <unistd.h>
#include <stdint.h>

#define DVB_MAX_PAYLOAD_PACKET_SIZE 4096
#define DVB_PID_SDT      17
#define DVB_PMT_TABLE_ID 2

struct dvb_v5_fe_parms;

typedef void (*dvb_table_init_func)(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, uint8_t *table, ssize_t *table_length);

struct dvb_table_init {
	dvb_table_init_func init;
};

extern const struct dvb_table_init dvb_table_initializers[];
extern char *default_charset;
extern char *output_charset;

#define bswap16(b) do {\
	b = ntohs(b); \
} while (0)

#define bswap32(b) do {\
	b = ntohl(b); \
} while (0)

struct dvb_desc {
	uint8_t type;
	uint8_t length;
	struct dvb_desc *next;

	uint8_t data[];
} __attribute__((packed));

void dvb_desc_default_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc);
void dvb_desc_default_print  (struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc);

#define dvb_desc_foreach( _desc, _tbl ) \
	for( struct dvb_desc *_desc = _tbl->descriptor; _desc; _desc = _desc->next ) \

#define dvb_desc_find(_struct, _desc, _tbl, _type) \
	for( _struct *_desc = (_struct *) _tbl->descriptor; _desc; _desc = (_struct *) _desc->next ) \
		if(_desc->type == _type) \

ssize_t dvb_desc_init(const uint8_t *buf, struct dvb_desc *desc);

uint32_t bcd(uint32_t bcd);

void hexdump(struct dvb_v5_fe_parms *parms, const char *prefix, const unsigned char *buf, int len);

void dvb_parse_descriptors(struct dvb_v5_fe_parms *parms, const uint8_t *buf, uint16_t section_length, struct dvb_desc **head_desc);
void dvb_free_descriptors(struct dvb_desc **list);
void dvb_print_descriptors(struct dvb_v5_fe_parms *parms, struct dvb_desc *desc);

struct dvb_v5_fe_parms;

typedef void (*dvb_desc_init_func) (struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc);
typedef void (*dvb_desc_print_func)(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc);
typedef void (*dvb_desc_free_func) (struct dvb_desc *desc);

struct dvb_descriptor {
	const char *name;
	dvb_desc_init_func init;
	dvb_desc_print_func print;
	dvb_desc_free_func free;
	ssize_t size;
};

extern const struct dvb_descriptor dvb_descriptors[];

enum dvb_tables {
	PAT,
	PMT,
	NIT,
	SDT,
};

enum descriptors {
	/* ISO/IEC 13818-1 */
	video_stream_descriptor				= 0x02,
	audio_stream_descriptor				= 0x03,
	hierarchy_descriptor				= 0x04,
	registration_descriptor				= 0x05,
	ds_alignment_descriptor				= 0x06,
	target_background_grid_descriptor		= 0x07,
	video_window_descriptor				= 0x08,
	conditional_access_descriptor			= 0x09,
	iso639_language_descriptor			= 0x0a,
	system_clock_descriptor				= 0x0b,
	multiplex_buffer_utilization_descriptor		= 0x0c,
	copyright_descriptor				= 0x0d,
	maximum_bitrate_descriptor			= 0x0e,
	private_data_indicator_descriptor		= 0x0f,
	smoothing_buffer_descriptor			= 0x10,
	std_descriptor					= 0x11,
	ibp_descriptor					= 0x12,

	mpeg4_video_descriptor				= 0x1b,
	mpeg4_audio_descriptor				= 0x1c,
	iod_descriptor					= 0x1d,
	sl_descriptor					= 0x1e,
	fmc_descriptor					= 0x1f,
	external_es_id_descriptor			= 0x20,
	muxcode_descriptor				= 0x21,
	fmxbuffersize_descriptor			= 0x22,
	multiplexbuffer_descriptor			= 0x23,
	content_labeling_descriptor			= 0x24,
	metadata_pointer_descriptor			= 0x25,
	metadata_descriptor				= 0x26,
	metadata_std_descriptor				= 0x27,
	AVC_video_descriptor				= 0x28,
	ipmp_descriptor					= 0x29,
	AVC_timing_and_HRD_descriptor			= 0x2a,
	mpeg2_aac_audio_descriptor			= 0x2b,
	flexmux_timing_descriptor			= 0x2c,

	/* ETSI EN 300 468 V1.11.1 (2010-04) */

	network_name_descriptor				= 0x40,
	service_list_descriptor				= 0x41,
	stuffing_descriptor				= 0x42,
	satellite_delivery_system_descriptor		= 0x43,
	cable_delivery_system_descriptor		= 0x44,
	VBI_data_descriptor				= 0x45,
	VBI_teletext_descriptor				= 0x46,
	bouquet_name_descriptor				= 0x47,
	service_descriptor				= 0x48,
	country_availability_descriptor			= 0x49,
	linkage_descriptor				= 0x4a,
	NVOD_reference_descriptor			= 0x4b,
	time_shifted_service_descriptor			= 0x4c,
	short_event_descriptor				= 0x4d,
	extended_event_descriptor			= 0x4e,
	time_shifted_event_descriptor			= 0x4f,
	component_descriptor				= 0x50,
	mosaic_descriptor				= 0x51,
	stream_identifier_descriptor			= 0x52,
	CA_identifier_descriptor			= 0x53,
	content_descriptor				= 0x54,
	parental_rating_descriptor			= 0x55,
	teletext_descriptor				= 0x56,
	telephone_descriptor				= 0x57,
	local_time_offset_descriptor			= 0x58,
	subtitling_descriptor				= 0x59,
	terrestrial_delivery_system_descriptor		= 0x5a,
	multilingual_network_name_descriptor		= 0x5b,
	multilingual_bouquet_name_descriptor		= 0x5c,
	multilingual_service_name_descriptor		= 0x5d,
	multilingual_component_descriptor		= 0x5e,
	private_data_specifier_descriptor		= 0x5f,
	service_move_descriptor				= 0x60,
	short_smoothing_buffer_descriptor		= 0x61,
	frequency_list_descriptor			= 0x62,
	partial_transport_stream_descriptor		= 0x63,
	data_broadcast_descriptor			= 0x64,
	scrambling_descriptor				= 0x65,
	data_broadcast_id_descriptor			= 0x66,
	transport_stream_descriptor			= 0x67,
	DSNG_descriptor					= 0x68,
	PDC_descriptor					= 0x69,
	AC_3_descriptor					= 0x6a,
	ancillary_data_descriptor			= 0x6b,
	cell_list_descriptor				= 0x6c,
	cell_frequency_link_descriptor			= 0x6d,
	announcement_support_descriptor			= 0x6e,
	application_signalling_descriptor		= 0x6f,
	adaptation_field_data_descriptor		= 0x70,
	service_identifier_descriptor			= 0x71,
	service_availability_descriptor			= 0x72,
	default_authority_descriptor			= 0x73,
	related_content_descriptor			= 0x74,
	TVA_id_descriptor				= 0x75,
	content_identifier_descriptor			= 0x76,
	time_slice_fec_identifier_descriptor		= 0x77,
	ECM_repetition_rate_descriptor			= 0x78,
	S2_satellite_delivery_system_descriptor		= 0x79,
	enhanced_AC_3_descriptor			= 0x7a,
	DTS_descriptor					= 0x7b,
	AAC_descriptor					= 0x7c,
	XAIT_location_descriptor			= 0x7d,
	FTA_content_management_descriptor		= 0x7e,
	extension_descriptor				= 0x7f,

	/* SCTE 35 2004 */
	CUE_identifier_descriptor			= 0x8a,

	/* From http://www.etherguidesystems.com/Help/SDOs/ATSC/Semantics/Descriptors/Default.aspx */
	component_name_descriptor			= 0xa3,

	/* From http://www.coolstf.com/tsreader/descriptors.html */
	logical_channel_number_descriptor		= 0x83,

	/* ISDB Descriptors, as defined on ABNT NBR 15603-1 2007 */

	carousel_id_descriptor				= 0x13,
	association_tag_descriptor			= 0x14,
	deferred_association_tags_descriptor		= 0x15,

	hierarchical_transmission_descriptor		= 0xc0,
	digital_copy_control_descriptor			= 0xc1,
	network_identifier_descriptor			= 0xc2,
	partial_transport_stream_time_descriptor	= 0xc3,
	audio_component_descriptor			= 0xc4,
	hyperlink_descriptor				= 0xc5,
	target_area_descriptor				= 0xc6,
	data_contents_descriptor			= 0xc7,
	video_decode_control_descriptor			= 0xc8,
	download_content_descriptor			= 0xc9,
	CA_EMM_TS_descriptor				= 0xca,
	CA_contract_information_descriptor		= 0xcb,
	CA_service_descriptor				= 0xcc,
	TS_Information_descriptior			= 0xcd,
	extended_broadcaster_descriptor			= 0xce,
	logo_transmission_descriptor			= 0xcf,
	basic_local_event_descriptor			= 0xd0,
	reference_descriptor				= 0xd1,
	node_relation_descriptor			= 0xd2,
	short_node_information_descriptor		= 0xd3,
	STC_reference_descriptor			= 0xd4,
	series_descriptor				= 0xd5,
	event_group_descriptor				= 0xd6,
	SI_parameter_descriptor				= 0xd7,
	broadcaster_Name_Descriptor			= 0xd8,
	component_group_descriptor			= 0xd9,
	SI_prime_TS_descriptor				= 0xda,
	board_information_descriptor			= 0xdb,
	LDT_linkage_descriptor				= 0xdc,
	connected_transmission_descriptor		= 0xdd,
	content_availability_descriptor			= 0xde,
	service_group_descriptor			= 0xe0,
	carousel_compatible_composite_descriptor	= 0xf7,
	conditional_playback_descriptor			= 0xf8,
	ISDBT_delivery_system_descriptor		= 0xfa,
	partial_reception_descriptor			= 0xfb,
	emergency_information_descriptor		= 0xfc,
	data_component_descriptor			= 0xfd,
	system_management_descriptor			= 0xfe,
};


enum extension_descriptors {
	image_icon_descriptor				= 0x00,
	cpcm_delivery_signalling_descriptor		= 0x01,
	CP_descriptor					= 0x02,
	CP_identifier_descriptor			= 0x03,
	T2_delivery_system_descriptor			= 0x04,
	SH_delivery_system_descriptor			= 0x05,
	supplementary_audio_descriptor			= 0x06,
	network_change_notify_descriptor		= 0x07,
	message_descriptor				= 0x08,
	target_region_descriptor			= 0x09,
	target_region_name_descriptor			= 0x0a,
	service_relocated_descriptor			= 0x0b,
};

struct pmt_table {
	uint16_t program_number, pcr_pid;
	unsigned char version;
};

struct el_pid {
	uint8_t  type;
	uint16_t pid;
};

struct pid_table {
	uint16_t service_id;
	uint16_t pid;
	struct pmt_table pmt_table;
	unsigned video_pid_len, audio_pid_len, other_el_pid_len;
	uint16_t *video_pid;
	uint16_t *audio_pid;
	struct el_pid *other_el_pid;
};

struct pat_table {
	uint16_t  ts_id;
	unsigned char version;
	struct pid_table *pid_table;
	unsigned pid_table_len;
};

struct transport_table {
	uint16_t tr_id;
};

struct lcn_table {
	uint16_t service_id;
	uint16_t lcn;
};

struct nit_table {
	uint16_t network_id;
	unsigned char version;
	char *network_name, *network_alias;
	struct transport_table *tr_table;
	unsigned tr_table_len;
	unsigned virtual_channel;
	unsigned area_code;

	/* Network Parameters */
	uint32_t delivery_system;
	uint32_t guard_interval;
	uint32_t fec_inner, fec_outer;
	uint32_t pol;
	uint32_t modulation;
	uint32_t rolloff;
	uint32_t symbol_rate;
	uint32_t bandwidth;
	uint32_t code_rate_hp;
	uint32_t code_rate_lp;
	uint32_t transmission_mode;
	uint32_t hierarchy;
	uint32_t plp_id;
	uint32_t system_id;

	unsigned has_dvbt:1;
	unsigned is_hp:1;
	unsigned has_time_slicing:1;
	unsigned has_mpe_fec:1;
	unsigned has_other_frequency:1;
	unsigned is_in_depth_interleaver:1;

	char *orbit;
	uint32_t *frequency;
	unsigned frequency_len;

	uint32_t *other_frequency;
	unsigned other_frequency_len;

	uint16_t *partial_reception;
	unsigned partial_reception_len;

	struct lcn_table *lcn;
	unsigned lcn_len;
};

struct service_table {
	uint16_t service_id;
	char running;
	char scrambled;
	unsigned char type;
	char *service_name, *service_alias;
	char *provider_name, *provider_alias;
};

struct sdt_table {
	unsigned char version;
	uint16_t ts_id;
	struct service_table *service_table;
	unsigned service_table_len;
};
struct dvb_v5_descriptors {
	int verbose;
	uint32_t delivery_system;

	struct pat_table pat_table;
	struct nit_table nit_table;
	struct sdt_table sdt_table;

	/* Used by descriptors to know where to update a PMT/Service/TS */
	unsigned cur_pmt;
	unsigned cur_service;
	unsigned cur_ts;
};

void parse_descriptor(struct dvb_v5_fe_parms *parms, enum dvb_tables type,
		struct dvb_v5_descriptors *dvb_desc,
		const unsigned char *buf, int len);

int has_descriptor(struct dvb_v5_descriptors *dvb_desc,
		unsigned char needed_descriptor,
		const unsigned char *buf, int len);


#endif
