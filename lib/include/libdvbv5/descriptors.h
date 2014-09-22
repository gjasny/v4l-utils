/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 * Copyright (c) 2012-2014 - Andre Roth <neolynx@gmail.com>
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

/**
 * @file descriptors.h
 * @ingroup dvb_table
 * @brief Provides a way to handle MPEG-TS descriptors found on Digital TV
 * 	streams.
 * @copyright GNU General Public License version 2 (GPLv2)
 * @author Mauro Carvalho Chehab
 * @author Andre Roth
 *
 * @par Relevant specs
 * The descriptors herein are defined on the following specs:
 * - ISO/IEC 13818-1
 * - ETSI EN 300 468 V1.11.1 (2010-04)
 * - SCTE 35 2004
 * - http://www.etherguidesystems.com/Help/SDOs/ATSC/Semantics/Descriptors/Default.aspx
 * - http://www.coolstf.com/tsreader/descriptors.html
 * - ABNT NBR 15603-1 2007
 * - ATSC A/65:2009 spec
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */


#ifndef _DESCRIPTORS_H
#define _DESCRIPTORS_H

#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>

/**
 * @brief Maximum size of a table session to be parsed
 * @ingroup dvb_table
 */
#define DVB_MAX_PAYLOAD_PACKET_SIZE 4096

/**
 * @brief number of bytes for the descriptor's CRC check
 * @ingroup dvb_table
 */
#define DVB_CRC_SIZE 4


#ifndef _DOXYGEN
struct dvb_v5_fe_parms;
#endif

/**
 * @brief Function prototype for a function that initializes the
 *	  descriptors parsing on a table
 * @ingroup dvb_table
 *
 * @param parms		Struct dvb_v5_fe_parms pointer
 * @param buf		Buffer with data to be parsed
 * @param buflen	Size of the buffer to be parsed
 * @param table		pointer to a place where the allocated memory with the
 *			table structure will be stored.
 */
typedef void (*dvb_table_init_func)(struct dvb_v5_fe_parms *parms,
				    const uint8_t *buf, ssize_t buflen,
				    void **table);

/**
 * @brief Table with all possible descriptors
 * @ingroup dvb_table
 */
extern const dvb_table_init_func dvb_table_initializers[256];

#ifndef _DOXYGEN
#define bswap16(b) do {\
	b = ntohs(b); \
} while (0)

#define bswap32(b) do {\
	b = ntohl(b); \
} while (0)

/* Deprecated */
#define DVB_DESC_HEADER() \
	uint8_t type; \
	uint8_t length; \
	struct dvb_desc *next

#endif /* _DOXYGEN */

/**
 * @struct dvb_desc
 * @brief Linked list containing the several descriptors found on a
 * 	  MPEG-TS table
 * @ingroup dvb_table
 *
 * @param type		Descriptor type
 * @param length	Length of the descriptor
 * @param next		pointer to the dvb_desc descriptor
 * @param data		Descriptor data
 */
struct dvb_desc {
	uint8_t type;
	uint8_t length;
	struct dvb_desc *next;

	uint8_t data[];
} __attribute__((packed));

#ifndef _DOXYGEN

#define dvb_desc_foreach( _desc, _tbl ) \
	for( struct dvb_desc *_desc = _tbl->descriptor; _desc; _desc = _desc->next ) \

#define dvb_desc_find(_struct, _desc, _tbl, _type) \
	for( _struct *_desc = (_struct *) _tbl->descriptor; _desc; _desc = (_struct *) _desc->next ) \
		if(_desc->type == _type) \

#endif /* _DOXYGEN */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Converts from BCD to CPU integer internal representation
 * @ingroup dvb_table
 *
 * @param bcd	value in BCD encoding
 */
uint32_t dvb_bcd(uint32_t bcd);

/**
 * @brief dumps data into the logs in hexadecimal format
 * @ingroup dvb_table
 *
 * @param parms		Struct dvb_v5_fe_parms pointer
 * @param prefix	String to be printed before the dvb_hexdump
 * @param buf		Buffer to hex dump
 * @param len		Number of bytes to show
 */
void dvb_hexdump(struct dvb_v5_fe_parms *parms, const char *prefix,
		 const unsigned char *buf, int len);

/**
 * @brief parse MPEG-TS descriptors
 * @ingroup dvb_table
 *
 * @param parms		Struct dvb_v5_fe_parms pointer
 * @param buf		Buffer with data to be parsed
 * @param buflen	Size of the buffer to be parsed
 * @param head_desc	pointer to the place to store the parsed data
 *
 * This function takes a buf as argument and parses it to find the
 * MPEG-TS descriptors inside it, creating a linked list.
 *
 * On success, head_desc will be allocated and filled with a linked list
 * with the descriptors found inside the buffer.
 *
 * This function is used by the several MPEG-TS table handlers to parse
 * the entire table that got read by dvb_read_sessions and other similar
 * functions.
 *
 * @return Returns 0 on success, a negative value otherwise.
 */
int  dvb_desc_parse(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
		    uint16_t buflen, struct dvb_desc **head_desc);

/**
 * @brief frees a dvb_desc linked list
 * @ingroup dvb_table
 *
 * @param list	struct dvb_desc pointer.
 */
void dvb_desc_free (struct dvb_desc **list);

/**
 * @brief prints the contents of a struct dvb_desc linked list
 * @ingroup dvb_table
 *
 * @param parms		Struct dvb_v5_fe_parms pointer
 * @param desc		struct dvb_desc pointer.
 */
void dvb_desc_print(struct dvb_v5_fe_parms *parms, struct dvb_desc *desc);

#ifdef __cplusplus
}
#endif

/**
 * @brief Function prototype for the descriptors parsing init code
 * @ingroup dvb_table
 *
 * @param parms		Struct dvb_v5_fe_parms pointer
 * @param buf		buffer with the content of the descriptor
 * @param desc		struct dvb_desc pointer
 */
typedef int (*dvb_desc_init_func) (struct dvb_v5_fe_parms *parms,
				   const uint8_t *buf, struct dvb_desc *desc);

/**
 * @brief Function prototype for the descriptors parsing print code
 * @ingroup dvb_table
 *
 * @param parms		Struct dvb_v5_fe_parms pointer
 * @param desc		struct dvb_desc pointer
 */
typedef void (*dvb_desc_print_func)(struct dvb_v5_fe_parms *parms,
				    const struct dvb_desc *desc);

/**
 * @brief Function prototype for the descriptors memory free code
 * @ingroup dvb_table
 *
 * @param desc		pointer to struct dvb_desc pointer to be freed
 */
typedef void (*dvb_desc_free_func) (struct dvb_desc *desc);

/**
 * @struct dvb_descriptor
 * @brief Contains the parser information for the MPEG-TS parser code
 * @ingroup dvb_table
 *
 * @param name		String containing the name of the descriptor
 * @param init		Pointer to a function to initialize the descriptor
 *			parser. This function fills the descriptor-specific
 *			internal structures
 * @param print		Prints the content of the descriptor
 * @param free		Frees all memory blocks allocated by the init function
 * @param size		Descriptor's size, in bytes.
 */
struct dvb_descriptor {
	const char *name;
	dvb_desc_init_func init;
	dvb_desc_print_func print;
	dvb_desc_free_func free;
	ssize_t size;
};

/**
 * @brief Contains the parsers for the several descriptors
 * @ingroup dvb_table
 */
extern const struct dvb_descriptor dvb_descriptors[];

/**
 * @enum descriptors
 * @brief List containing all descriptors used by Digital TV MPEG-TS
 * @ingroup dvb_table
 *
 * @var video_stream_descriptor
 *	@brief	video_stream descriptor - ISO/IEC 13818-1
 * @var audio_stream_descriptor
 *	@brief	audio_stream descriptor - ISO/IEC 13818-1
 * @var hierarchy_descriptor
 *	@brief	hierarchy descriptor - ISO/IEC 13818-1
 * @var registration_descriptor
 *	@brief	registration descriptor - ISO/IEC 13818-1
 * @var ds_alignment_descriptor
 *	@brief	ds_alignment descriptor - ISO/IEC 13818-1
 * @var target_background_grid_descriptor
 *	@brief	target_background_grid descriptor - ISO/IEC 13818-1
 * @var video_window_descriptor
 *	@brief	video_window descriptor - ISO/IEC 13818-1
 * @var conditional_access_descriptor
 *	@brief	conditional_access descriptor - ISO/IEC 13818-1
 * @var iso639_language_descriptor
 *	@brief	iso639_language descriptor - ISO/IEC 13818-1
 * @var system_clock_descriptor
 *	@brief	system_clock descriptor - ISO/IEC 13818-1
 * @var multiplex_buffer_utilization_descriptor
 *	@brief	multiplex_buffer_utilization descriptor - ISO/IEC 13818-1
 * @var copyright_descriptor
 *	@brief	copyright descriptor - ISO/IEC 13818-1
 * @var maximum_bitrate_descriptor
 *	@brief	maximum_bitrate descriptor - ISO/IEC 13818-1
 * @var private_data_indicator_descriptor
 *	@brief	private_data_indicator descriptor - ISO/IEC 13818-1
 * @var smoothing_buffer_descriptor
 *	@brief	smoothing_buffer descriptor - ISO/IEC 13818-1
 * @var std_descriptor
 *	@brief	std descriptor - ISO/IEC 13818-1
 * @var ibp_descriptor
 *	@brief	ibp descriptor - ISO/IEC 13818-1
 * @var mpeg4_video_descriptor
 *	@brief	mpeg4_video descriptor - ISO/IEC 13818-1
 * @var mpeg4_audio_descriptor
 *	@brief	mpeg4_audio descriptor - ISO/IEC 13818-1
 * @var iod_descriptor
 *	@brief	iod descriptor - ISO/IEC 13818-1
 * @var sl_descriptor
 *	@brief	sl descriptor - ISO/IEC 13818-1
 * @var fmc_descriptor
 *	@brief	fmc descriptor - ISO/IEC 13818-1
 * @var external_es_id_descriptor
 *	@brief	external_es_id descriptor - ISO/IEC 13818-1
 * @var muxcode_descriptor
 *	@brief	muxcode descriptor - ISO/IEC 13818-1
 * @var fmxbuffersize_descriptor
 *	@brief	fmxbuffersize descriptor - ISO/IEC 13818-1
 * @var multiplexbuffer_descriptor
 *	@brief	multiplexbuffer descriptor - ISO/IEC 13818-1
 * @var content_labeling_descriptor
 *	@brief	content_labeling descriptor - ISO/IEC 13818-1
 * @var metadata_pointer_descriptor
 *	@brief	metadata_pointer descriptor - ISO/IEC 13818-1
 * @var metadata_descriptor
 *	@brief	metadata descriptor - ISO/IEC 13818-1
 * @var metadata_std_descriptor
 *	@brief	metadata_std descriptor - ISO/IEC 13818-1
 * @var AVC_video_descriptor
 *	@brief	AVC_video descriptor - ISO/IEC 13818-1
 * @var ipmp_descriptor
 *	@brief	ipmp descriptor - ISO/IEC 13818-1
 * @var AVC_timing_and_HRD_descriptor
 *	@brief	AVC_timing_and_HRD descriptor - ISO/IEC 13818-1
 * @var mpeg2_aac_audio_descriptor
 *	@brief	mpeg2_aac_audio descriptor - ISO/IEC 13818-1
 * @var flexmux_timing_descriptor
 *	@brief	flexmux_timing descriptor - ISO/IEC 13818-1
 * @var network_name_descriptor
 *	@brief	network_name descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var service_list_descriptor
 *	@brief	service_list descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var stuffing_descriptor
 *	@brief	stuffing descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var satellite_delivery_system_descriptor
 *	@brief	satellite_delivery_system descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var cable_delivery_system_descriptor
 *	@brief	cable_delivery_system descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var VBI_data_descriptor
 *	@brief	VBI_data descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var VBI_teletext_descriptor
 *	@brief	VBI_teletext descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var bouquet_name_descriptor
 *	@brief	bouquet_name descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var service_descriptor
 *	@brief	service descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var country_availability_descriptor
 *	@brief	country_availability descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var linkage_descriptor
 *	@brief	linkage descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var NVOD_reference_descriptor
 *	@brief	NVOD_reference descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var time_shifted_service_descriptor
 *	@brief	time_shifted_service descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var short_event_descriptor
 *	@brief	short_event descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var extended_event_descriptor
 *	@brief	extended_event descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var time_shifted_event_descriptor
 *	@brief	time_shifted_event descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var component_descriptor
 *	@brief	component descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var mosaic_descriptor
 *	@brief	mosaic descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var stream_identifier_descriptor
 *	@brief	stream_identifier descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var CA_identifier_descriptor
 *	@brief	CA_identifier descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var content_descriptor
 *	@brief	content descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var parental_rating_descriptor
 *	@brief	parental_rating descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var teletext_descriptor
 *	@brief	teletext descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var telephone_descriptor
 *	@brief	telephone descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var local_time_offset_descriptor
 *	@brief	local_time_offset descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var subtitling_descriptor
 *	@brief	subtitling descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var terrestrial_delivery_system_descriptor
 *	@brief	terrestrial_delivery_system descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var multilingual_network_name_descriptor
 *	@brief	multilingual_network_name descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var multilingual_bouquet_name_descriptor
 *	@brief	multilingual_bouquet_name descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var multilingual_service_name_descriptor
 *	@brief	multilingual_service_name descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var multilingual_component_descriptor
 *	@brief	multilingual_component descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var private_data_specifier_descriptor
 *	@brief	private_data_specifier descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var service_move_descriptor
 *	@brief	service_move descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var short_smoothing_buffer_descriptor
 *	@brief	short_smoothing_buffer descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var frequency_list_descriptor
 *	@brief	frequency_list descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var partial_transport_stream_descriptor
 *	@brief	partial_transport_stream descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var data_broadcast_descriptor
 *	@brief	data_broadcast descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var scrambling_descriptor
 *	@brief	scrambling descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var data_broadcast_id_descriptor
 *	@brief	data_broadcast_id descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var transport_stream_descriptor
 *	@brief	transport_stream descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var DSNG_descriptor
 *	@brief	DSNG descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var PDC_descriptor
 *	@brief	PDC descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var AC_3_descriptor
 *	@brief	AC_3 descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var ancillary_data_descriptor
 *	@brief	ancillary_data descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var cell_list_descriptor
 *	@brief	cell_list descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var cell_frequency_link_descriptor
 *	@brief	cell_frequency_link descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var announcement_support_descriptor
 *	@brief	announcement_support descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var application_signalling_descriptor
 *	@brief	application_signalling descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var adaptation_field_data_descriptor
 *	@brief	adaptation_field_data descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var service_identifier_descriptor
 *	@brief	service_identifier descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var service_availability_descriptor
 *	@brief	service_availability descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var default_authority_descriptor
 *	@brief	default_authority descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var related_content_descriptor
 *	@brief	related_content descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var TVA_id_descriptor
 *	@brief	TVA_id descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var content_identifier_descriptor
 *	@brief	content_identifier descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var time_slice_fec_identifier_descriptor
 *	@brief	time_slice_fec_identifier descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var ECM_repetition_rate_descriptor
 *	@brief	ECM_repetition_rate descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var S2_satellite_delivery_system_descriptor
 *	@brief	S2_satellite_delivery_system descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var enhanced_AC_3_descriptor
 *	@brief	enhanced_AC_3 descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var DTS_descriptor
 *	@brief	DTS descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var AAC_descriptor
 *	@brief	AAC descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var XAIT_location_descriptor
 *	@brief	XAIT_location descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var FTA_content_management_descriptor
 *	@brief	FTA_content_management descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var extension_descriptor
 *	@brief	extension descriptor - ETSI EN 300 468 V1.11.1 (2010-04)
 * @var CUE_identifier_descriptor
 *	@brief	CUE_identifier descriptor - SCTE 35 2004
 * @var extended_channel_name
 *	@brief	extended_channel_name descriptor - SCTE 35 2004
 * @var service_location
 *	@brief service_location descriptor - SCTE 35 2004
 * @var component_name_descriptor
 *	@brief	component_name descriptor - SCTE 35 2004
 *	@see http://www.etherguidesystems.com/Help/SDOs/ATSC/Semantics/Descriptors/Default.aspx
 * @var logical_channel_number_descriptor
 *	@brief	logical_channel_number descriptor - SCTE 35 2004
 *	@see http://www.coolstf.com/tsreader/descriptors.html
 *
 * @var carousel_id_descriptor
 *	@brief	carousel_id descriptor - ABNT NBR 15603-1 2007
 * @var association_tag_descriptor
 *	@brief	association_tag descriptor - ABNT NBR 15603-1 2007
 * @var deferred_association_tags_descriptor
 *	@brief	deferred_association_tags descriptor - ABNT NBR 15603-1 2007

 * @var hierarchical_transmission_descriptor
 *	@brief	hierarchical_transmission descriptor - ABNT NBR 15603-1 2007
 * @var digital_copy_control_descriptor
 *	@brief	digital_copy_control descriptor - ABNT NBR 15603-1 2007
 * @var network_identifier_descriptor
 *	@brief	network_identifier descriptor - ABNT NBR 15603-1 2007
 * @var partial_transport_stream_time_descriptor
 *	@brief	partial_transport_stream_time descriptor - ABNT NBR 15603-1 2007
 * @var audio_component_descriptor
 *	@brief	audio_component descriptor - ABNT NBR 15603-1 2007
 * @var hyperlink_descriptor
 *	@brief	hyperlink descriptor - ABNT NBR 15603-1 2007
 * @var target_area_descriptor
 *	@brief	target_area descriptor - ABNT NBR 15603-1 2007
 * @var data_contents_descriptor
 *	@brief	data_contents descriptor - ABNT NBR 15603-1 2007
 * @var video_decode_control_descriptor
 *	@brief	video_decode_control descriptor - ABNT NBR 15603-1 2007
 * @var download_content_descriptor
 *	@brief	download_content descriptor - ABNT NBR 15603-1 2007
 * @var CA_EMM_TS_descriptor
 *	@brief	CA_EMM_TS descriptor - ABNT NBR 15603-1 2007
 * @var CA_contract_information_descriptor
 *	@brief	CA_contract_information descriptor - ABNT NBR 15603-1 2007
 * @var CA_service_descriptor
 *	@brief	CA_service descriptor - ABNT NBR 15603-1 2007
 * @var TS_Information_descriptior
 *	@brief transport_stream_information descriptor - ABNT NBR 15603-1 2007
 * @var extended_broadcaster_descriptor
 *	@brief	extended_broadcaster descriptor - ABNT NBR 15603-1 2007
 * @var logo_transmission_descriptor
 *	@brief	logo_transmission descriptor - ABNT NBR 15603-1 2007
 * @var basic_local_event_descriptor
 *	@brief	basic_local_event descriptor - ABNT NBR 15603-1 2007
 * @var reference_descriptor
 *	@brief	reference descriptor - ABNT NBR 15603-1 2007
 * @var node_relation_descriptor
 *	@brief	node_relation descriptor - ABNT NBR 15603-1 2007
 * @var short_node_information_descriptor
 *	@brief	short_node_information descriptor - ABNT NBR 15603-1 2007
 * @var STC_reference_descriptor
 *	@brief	STC_reference descriptor - ABNT NBR 15603-1 2007
 * @var series_descriptor
 *	@brief	series descriptor - ABNT NBR 15603-1 2007
 * @var event_group_descriptor
 *	@brief	event_group descriptor - ABNT NBR 15603-1 2007
 * @var SI_parameter_descriptor
 *	@brief	SI_parameter descriptor - ABNT NBR 15603-1 2007
 * @var broadcaster_Name_Descriptor
 * 	@brief broadcaster_Name descriptor - ABNT NBR 15603-1 2007
 * @var component_group_descriptor
 *	@brief	component_group descriptor - ABNT NBR 15603-1 2007
 * @var SI_prime_TS_descriptor
 *	@brief	SI_prime_transport_stream descriptor - ABNT NBR 15603-1 2007
 * @var board_information_descriptor
 *	@brief	board_information descriptor - ABNT NBR 15603-1 2007
 * @var LDT_linkage_descriptor
 *	@brief	LDT_linkage descriptor - ABNT NBR 15603-1 2007
 * @var connected_transmission_descriptor
 *	@brief	connected_transmission descriptor - ABNT NBR 15603-1 2007
 * @var content_availability_descriptor
 *	@brief	content_availability descriptor - ABNT NBR 15603-1 2007
 * @var service_group_descriptor
 *	@brief	service_group descriptor - ABNT NBR 15603-1 2007
 * @var carousel_compatible_composite_descriptor
 *	@brief	carousel_compatible_composite descriptor - ABNT NBR 15603-1 2007
 * @var conditional_playback_descriptor
 *	@brief	conditional_playback descriptor - ABNT NBR 15603-1 2007
 * @var ISDBT_delivery_system_descriptor
 *	@brief	ISDBT terrestrial_delivery_system descriptor - ABNT NBR 15603-1 2007
 * @var partial_reception_descriptor
 *	@brief	partial_reception descriptor - ABNT NBR 15603-1 2007
 * @var emergency_information_descriptor
 *	@brief	emergency_information descriptor - ABNT NBR 15603-1 2007
 * @var data_component_descriptor
 *	@brief	data_component descriptor - ABNT NBR 15603-1 2007
 * @var system_management_descriptor
 *	@brief	system_management descriptor - ABNT NBR 15603-1 2007
 *
 * @var atsc_stuffing_descriptor
 *	@brief	atsc_stuffing descriptor - ATSC A/65:2009
 * @var atsc_ac3_audio_descriptor
 *	@brief	atsc_ac3_audio descriptor - ATSC A/65:2009
 * @var atsc_caption_service_descriptor
 *	@brief	atsc_caption_service descriptor - ATSC A/65:2009
 * @var atsc_content_advisory_descriptor
 *	@brief	atsc_content_advisory descriptor - ATSC A/65:2009
 * @var atsc_extended_channel_descriptor
 *	@brief	atsc_extended_channel descriptor - ATSC A/65:2009
 * @var atsc_service_location_descriptor
 *	@brief	atsc_service_location descriptor - ATSC A/65:2009
 * @var atsc_time_shifted_service_descriptor
 *	@brief	atsc_time_shifted_service descriptor - ATSC A/65:2009
 * @var atsc_component_name_descriptor
 *	@brief	atsc_component_name descriptor - ATSC A/65:2009
 * @var atsc_DCC_departing_request_descriptor
 *	@brief	atsc_DCC_departing_request descriptor - ATSC A/65:2009
 * @var atsc_DCC_arriving_request_descriptor
 *	@brief	atsc_DCC_arriving_request descriptor - ATSC A/65:2009
 * @var atsc_redistribution_control_descriptor
 *	@brief	atsc_redistribution_control descriptor - ATSC A/65:2009
 * @var atsc_ATSC_private_information_descriptor
 *	@brief	atsc_ATSC_private_information descriptor - ATSC A/65:2009
 * @var atsc_genre_descriptor
 *	@brief	atsc_genre descriptor - ATSC A/65:2009
 */
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

	extended_channel_name				= 0xa0,
	service_location				= 0xa1,
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

	/* ATSC descriptors - ATSC A/65:2009 spec */
	atsc_stuffing_descriptor			= 0x80,
	atsc_ac3_audio_descriptor			= 0x81,
	atsc_caption_service_descriptor			= 0x86,
	atsc_content_advisory_descriptor		= 0x87,
	atsc_extended_channel_descriptor		= 0xa0,
	atsc_service_location_descriptor		= 0xa1,
	atsc_time_shifted_service_descriptor		= 0xa2,
	atsc_component_name_descriptor			= 0xa3,
	atsc_DCC_departing_request_descriptor		= 0xa8,
	atsc_DCC_arriving_request_descriptor		= 0xa9,
	atsc_redistribution_control_descriptor		= 0xaa,
	atsc_ATSC_private_information_descriptor	= 0xad,
	atsc_genre_descriptor				= 0xab,
};

/* Please see desc_extension.h for extension_descriptor types */

#endif
