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

#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "descriptors.h"
#include "dvb-fe.h"
#include "dvb-scan.h"
#include "parse_string.h"
#include "dvb-frontend.h"
#include "dvb-v5-std.h"
#include "dvb-log.h"

#include "descriptors/pat.h"
#include "descriptors/pmt.h"
#include "descriptors/nit.h"
#include "descriptors/sdt.h"
#include "descriptors/eit.h"
#include "descriptors/vct.h"
#include "descriptors/desc_language.h"
#include "descriptors/desc_network_name.h"
#include "descriptors/desc_cable_delivery.h"
#include "descriptors/desc_sat.h"
#include "descriptors/desc_terrestrial_delivery.h"
#include "descriptors/desc_service.h"
#include "descriptors/desc_service_list.h"
#include "descriptors/desc_frequency_list.h"
#include "descriptors/desc_event_short.h"
#include "descriptors/desc_event_extended.h"
#include "descriptors/desc_atsc_service_location.h"
#include "descriptors/desc_hierarchy.h"
#include "descriptors/desc_extension.h"

ssize_t dvb_desc_init(const uint8_t *buf, struct dvb_desc *desc)
{
	desc->type   = buf[0];
	desc->length = buf[1];
	desc->next   = NULL;
	return 2;
}

void dvb_desc_default_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	memcpy(desc->data, buf, desc->length);
}

void dvb_desc_default_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	dvb_log("|                   %s (0x%02x)", dvb_descriptors[desc->type].name, desc->type);
	hexdump(parms, "|                       ", desc->data, desc->length);
}

const struct dvb_table_init dvb_table_initializers[] = {
	[DVB_TABLE_PAT] = { dvb_table_pat_init },
	[DVB_TABLE_PMT] = { dvb_table_pmt_init },
	[DVB_TABLE_NIT] = { dvb_table_nit_init },
	[DVB_TABLE_SDT] = { dvb_table_sdt_init },
	[DVB_TABLE_EIT] = { dvb_table_eit_init },
	[DVB_TABLE_TVCT] = { dvb_table_vct_init },
	[DVB_TABLE_CVCT] = { dvb_table_vct_init },
	[DVB_TABLE_EIT_SCHEDULE] = { dvb_table_eit_init },
};

char *default_charset = "iso-8859-1";
char *output_charset = "utf-8";

void dvb_parse_descriptors(struct dvb_v5_fe_parms *parms, const uint8_t *buf, uint16_t section_length, struct dvb_desc **head_desc)
{
	const uint8_t *ptr = buf;
	struct dvb_desc *current = NULL;
	struct dvb_desc *last = NULL;
	while (ptr < buf + section_length) {
		int desc_type = ptr[0];
		int desc_len  = ptr[1];
		size_t size;

		dvb_desc_init_func init = dvb_descriptors[desc_type].init;
		if (!init) {
			init = dvb_desc_default_init;
			size = sizeof(struct dvb_desc) + desc_len;
		} else {
			size = dvb_descriptors[desc_type].size;
		}
		if (!size) {
			dvb_logerr("descriptor type %d has no size defined", current->type);
			size = 4096;
		}
		current = malloc(size);
		if (!current)
			dvb_perror("Out of memory");
		ptr += dvb_desc_init(ptr, current); /* the standard header was read */
		if (ptr >=  buf + section_length) {
			dvb_logerr("descriptor is truncated");
			return;
		}
		init(parms, ptr, current);
		if(!*head_desc)
			*head_desc = current;
		if (last)
			last->next = current;
		last = current;
		ptr += current->length;     /* standard descriptor header plus descriptor length */
	}
}

void dvb_print_descriptors(struct dvb_v5_fe_parms *parms, struct dvb_desc *desc)
{
	while (desc) {
		dvb_desc_print_func print = dvb_descriptors[desc->type].print;
		if (!print)
			print = dvb_desc_default_print;
		print(parms, desc);
		desc = desc->next;
	}
}

void dvb_free_descriptors(struct dvb_desc **list)
{
	struct dvb_desc *desc = *list;
	while (desc) {
		struct dvb_desc *tmp = desc;
		desc = desc->next;
		if (dvb_descriptors[tmp->type].free)
			dvb_descriptors[tmp->type].free(tmp);
		else
			free(tmp);
	}
	*list = NULL;
}

const struct dvb_descriptor dvb_descriptors[] = {
	[0 ...255 ] = { "Unknown descriptor", NULL, NULL, NULL, 0 },
	[video_stream_descriptor] = { "video_stream_descriptor", NULL, NULL, NULL, 0 },
	[audio_stream_descriptor] = { "audio_stream_descriptor", NULL, NULL, NULL, 0 },
	[hierarchy_descriptor] = { "hierarchy_descriptor", dvb_desc_hierarchy_init, dvb_desc_hierarchy_print, NULL, sizeof(struct dvb_desc_hierarchy) },
	[registration_descriptor] = { "registration_descriptor", NULL, NULL, NULL, 0 },
	[ds_alignment_descriptor] = { "ds_alignment_descriptor", NULL, NULL, NULL, 0 },
	[target_background_grid_descriptor] = { "target_background_grid_descriptor", NULL, NULL, NULL, 0 },
	[video_window_descriptor] = { "video_window_descriptor", NULL, NULL, NULL, 0 },
	[conditional_access_descriptor] = { "conditional_access_descriptor", NULL, NULL, NULL, 0 },
	[iso639_language_descriptor] = { "iso639_language_descriptor", dvb_desc_language_init, dvb_desc_language_print, NULL, sizeof(struct dvb_desc_language) },
	[system_clock_descriptor] = { "system_clock_descriptor", NULL, NULL, NULL, 0 },
	[multiplex_buffer_utilization_descriptor] = { "multiplex_buffer_utilization_descriptor", NULL, NULL, NULL, 0 },
	[copyright_descriptor] = { "copyright_descriptor", NULL, NULL, NULL, 0 },
	[maximum_bitrate_descriptor] = { "maximum_bitrate_descriptor", NULL, NULL, NULL, 0 },
	[private_data_indicator_descriptor] = { "private_data_indicator_descriptor", NULL, NULL, NULL, 0 },
	[smoothing_buffer_descriptor] = { "smoothing_buffer_descriptor", NULL, NULL, NULL, 0 },
	[std_descriptor] = { "std_descriptor", NULL, NULL, NULL, 0 },
	[ibp_descriptor] = { "ibp_descriptor", NULL, NULL, NULL, 0 },
	[mpeg4_video_descriptor] = { "mpeg4_video_descriptor", NULL, NULL, NULL, 0 },
	[mpeg4_audio_descriptor] = { "mpeg4_audio_descriptor", NULL, NULL, NULL, 0 },
	[iod_descriptor] = { "iod_descriptor", NULL, NULL, NULL, 0 },
	[sl_descriptor] = { "sl_descriptor", NULL, NULL, NULL, 0 },
	[fmc_descriptor] = { "fmc_descriptor", NULL, NULL, NULL, 0 },
	[external_es_id_descriptor] = { "external_es_id_descriptor", NULL, NULL, NULL, 0 },
	[muxcode_descriptor] = { "muxcode_descriptor", NULL, NULL, NULL, 0 },
	[fmxbuffersize_descriptor] = { "fmxbuffersize_descriptor", NULL, NULL, NULL, 0 },
	[multiplexbuffer_descriptor] = { "multiplexbuffer_descriptor", NULL, NULL, NULL, 0 },
	[content_labeling_descriptor] = { "content_labeling_descriptor", NULL, NULL, NULL, 0 },
	[metadata_pointer_descriptor] = { "metadata_pointer_descriptor", NULL, NULL, NULL, 0 },
	[metadata_descriptor] = { "metadata_descriptor", NULL, NULL, NULL, 0 },
	[metadata_std_descriptor] = { "metadata_std_descriptor", NULL, NULL, NULL, 0 },
	[AVC_video_descriptor] = { "AVC_video_descriptor", NULL, NULL, NULL, 0 },
	[ipmp_descriptor] = { "ipmp_descriptor", NULL, NULL, NULL, 0 },
	[AVC_timing_and_HRD_descriptor] = { "AVC_timing_and_HRD_descriptor", NULL, NULL, NULL, 0 },
	[mpeg2_aac_audio_descriptor] = { "mpeg2_aac_audio_descriptor", NULL, NULL, NULL, 0 },
	[flexmux_timing_descriptor] = { "flexmux_timing_descriptor", NULL, NULL, NULL, 0 },
	[network_name_descriptor] = { "network_name_descriptor", dvb_desc_network_name_init, dvb_desc_network_name_print, NULL, sizeof(struct dvb_desc_network_name) },
	[service_list_descriptor] = { "service_list_descriptor", dvb_desc_service_list_init, dvb_desc_service_list_print, NULL, sizeof(struct dvb_desc_service_list) },
	[stuffing_descriptor] = { "stuffing_descriptor", NULL, NULL, NULL, 0 },
	[satellite_delivery_system_descriptor] = { "satellite_delivery_system_descriptor", dvb_desc_sat_init, dvb_desc_sat_print, NULL, sizeof(struct dvb_desc_sat) },
	[cable_delivery_system_descriptor] = { "cable_delivery_system_descriptor", dvb_desc_cable_delivery_init, dvb_desc_cable_delivery_print, NULL, sizeof(struct dvb_desc_cable_delivery) },
	[VBI_data_descriptor] = { "VBI_data_descriptor", NULL, NULL, NULL, 0 },
	[VBI_teletext_descriptor] = { "VBI_teletext_descriptor", NULL, NULL, NULL, 0 },
	[bouquet_name_descriptor] = { "bouquet_name_descriptor", NULL, NULL, NULL, 0 },
	[service_descriptor] = { "service_descriptor", dvb_desc_service_init, dvb_desc_service_print, dvb_desc_service_free, sizeof(struct dvb_desc_service) },
	[country_availability_descriptor] = { "country_availability_descriptor", NULL, NULL, NULL, 0 },
	[linkage_descriptor] = { "linkage_descriptor", NULL, NULL, NULL, 0 },
	[NVOD_reference_descriptor] = { "NVOD_reference_descriptor", NULL, NULL, NULL, 0 },
	[time_shifted_service_descriptor] = { "time_shifted_service_descriptor", NULL, NULL, NULL, 0 },
	[short_event_descriptor] = { "short_event_descriptor", dvb_desc_event_short_init, dvb_desc_event_short_print, dvb_desc_event_short_free, sizeof(struct dvb_desc_event_short) },
	[extended_event_descriptor] = { "extended_event_descriptor", dvb_desc_event_extended_init, dvb_desc_event_extended_print, dvb_desc_event_extended_free, sizeof(struct dvb_desc_event_extended) },
	[time_shifted_event_descriptor] = { "time_shifted_event_descriptor", NULL, NULL, NULL, 0 },
	[component_descriptor] = { "component_descriptor", NULL, NULL, NULL, 0 },
	[mosaic_descriptor] = { "mosaic_descriptor", NULL, NULL, NULL, 0 },
	[stream_identifier_descriptor] = { "stream_identifier_descriptor", NULL, NULL, NULL, 0 },
	[CA_identifier_descriptor] = { "CA_identifier_descriptor", NULL, NULL, NULL, 0 },
	[content_descriptor] = { "content_descriptor", NULL, NULL, NULL, 0 },
	[parental_rating_descriptor] = { "parental_rating_descriptor", NULL, NULL, NULL, 0 },
	[teletext_descriptor] = { "teletext_descriptor", NULL, NULL, NULL, 0 },
	[telephone_descriptor] = { "telephone_descriptor", NULL, NULL, NULL, 0 },
	[local_time_offset_descriptor] = { "local_time_offset_descriptor", NULL, NULL, NULL, 0 },
	[subtitling_descriptor] = { "subtitling_descriptor", NULL, NULL, NULL, 0 },
	[terrestrial_delivery_system_descriptor] = { "terrestrial_delivery_system_descriptor", dvb_desc_terrestrial_delivery_init, dvb_desc_terrestrial_delivery_print, NULL, sizeof(struct dvb_desc_terrestrial_delivery) },
	[multilingual_network_name_descriptor] = { "multilingual_network_name_descriptor", NULL, NULL, NULL, 0 },
	[multilingual_bouquet_name_descriptor] = { "multilingual_bouquet_name_descriptor", NULL, NULL, NULL, 0 },
	[multilingual_service_name_descriptor] = { "multilingual_service_name_descriptor", NULL, NULL, NULL, 0 },
	[multilingual_component_descriptor] = { "multilingual_component_descriptor", NULL, NULL, NULL, 0 },
	[private_data_specifier_descriptor] = { "private_data_specifier_descriptor", NULL, NULL, NULL, 0 },
	[service_move_descriptor] = { "service_move_descriptor", NULL, NULL, NULL, 0 },
	[short_smoothing_buffer_descriptor] = { "short_smoothing_buffer_descriptor", NULL, NULL, NULL, 0 },
	[frequency_list_descriptor] = { "frequency_list_descriptor", dvb_desc_frequency_list_init, dvb_desc_frequency_list_print, NULL, sizeof(struct dvb_desc_frequency_list) },
	[partial_transport_stream_descriptor] = { "partial_transport_stream_descriptor", NULL, NULL, NULL, 0 },
	[data_broadcast_descriptor] = { "data_broadcast_descriptor", NULL, NULL, NULL, 0 },
	[scrambling_descriptor] = { "scrambling_descriptor", NULL, NULL, NULL, 0 },
	[data_broadcast_id_descriptor] = { "data_broadcast_id_descriptor", NULL, NULL, NULL, 0 },
	[transport_stream_descriptor] = { "transport_stream_descriptor", NULL, NULL, NULL, 0 },
	[DSNG_descriptor] = { "DSNG_descriptor", NULL, NULL, NULL, 0 },
	[PDC_descriptor] = { "PDC_descriptor", NULL, NULL, NULL, 0 },
	[AC_3_descriptor] = { "AC_3_descriptor", NULL, NULL, NULL, 0 },
	[ancillary_data_descriptor] = { "ancillary_data_descriptor", NULL, NULL, NULL, 0 },
	[cell_list_descriptor] = { "cell_list_descriptor", NULL, NULL, NULL, 0 },
	[cell_frequency_link_descriptor] = { "cell_frequency_link_descriptor", NULL, NULL, NULL, 0 },
	[announcement_support_descriptor] = { "announcement_support_descriptor", NULL, NULL, NULL, 0 },
	[application_signalling_descriptor] = { "application_signalling_descriptor", NULL, NULL, NULL, 0 },
	[adaptation_field_data_descriptor] = { "adaptation_field_data_descriptor", NULL, NULL, NULL, 0 },
	[service_identifier_descriptor] = { "service_identifier_descriptor", NULL, NULL, NULL, 0 },
	[service_availability_descriptor] = { "service_availability_descriptor", NULL, NULL, NULL, 0 },
	[default_authority_descriptor] = { "default_authority_descriptor", NULL, NULL, NULL, 0 },
	[related_content_descriptor] = { "related_content_descriptor", NULL, NULL, NULL, 0 },
	[TVA_id_descriptor] = { "TVA_id_descriptor", NULL, NULL, NULL, 0 },
	[content_identifier_descriptor] = { "content_identifier_descriptor", NULL, NULL, NULL, 0 },
	[time_slice_fec_identifier_descriptor] = { "time_slice_fec_identifier_descriptor", NULL, NULL, NULL, 0 },
	[ECM_repetition_rate_descriptor] = { "ECM_repetition_rate_descriptor", NULL, NULL, NULL, 0 },
	[S2_satellite_delivery_system_descriptor] = { "S2_satellite_delivery_system_descriptor", NULL, NULL, NULL, 0 },
	[enhanced_AC_3_descriptor] = { "enhanced_AC_3_descriptor", NULL, NULL, NULL, 0 },
	[DTS_descriptor] = { "DTS_descriptor", NULL, NULL, NULL, 0 },
	[AAC_descriptor] = { "AAC_descriptor", NULL, NULL, NULL, 0 },
	[XAIT_location_descriptor] = { "XAIT_location_descriptor", NULL, NULL, NULL, 0 },
	[FTA_content_management_descriptor] = { "FTA_content_management_descriptor", NULL, NULL, NULL, 0 },
	[extension_descriptor] = { "extension_descriptor", extension_descriptor_init, NULL, NULL, sizeof(struct dvb_extension_descriptor) },

	[CUE_identifier_descriptor] = { "CUE_identifier_descriptor", NULL, NULL, NULL, 0 },

	[component_name_descriptor] = { "component_name_descriptor", NULL, NULL, NULL, 0 },
	[logical_channel_number_descriptor] = { "logical_channel_number_descriptor", NULL, NULL, NULL, 0 },

	[carousel_id_descriptor] = { "carousel_id_descriptor", NULL, NULL, NULL, 0 },
	[association_tag_descriptor] = { "association_tag_descriptor", NULL, NULL, NULL, 0 },
	[deferred_association_tags_descriptor] = { "deferred_association_tags_descriptor", NULL, NULL, NULL, 0 },

	[hierarchical_transmission_descriptor] = { "hierarchical_transmission_descriptor", NULL, NULL, NULL, 0 },
	[digital_copy_control_descriptor] = { "digital_copy_control_descriptor", NULL, NULL, NULL, 0 },
	[network_identifier_descriptor] = { "network_identifier_descriptor", NULL, NULL, NULL, 0 },
	[partial_transport_stream_time_descriptor] = { "partial_transport_stream_time_descriptor", NULL, NULL, NULL, 0 },
	[audio_component_descriptor] = { "audio_component_descriptor", NULL, NULL, NULL, 0 },
	[hyperlink_descriptor] = { "hyperlink_descriptor", NULL, NULL, NULL, 0 },
	[target_area_descriptor] = { "target_area_descriptor", NULL, NULL, NULL, 0 },
	[data_contents_descriptor] = { "data_contents_descriptor", NULL, NULL, NULL, 0 },
	[video_decode_control_descriptor] = { "video_decode_control_descriptor", NULL, NULL, NULL, 0 },
	[download_content_descriptor] = { "download_content_descriptor", NULL, NULL, NULL, 0 },
	[CA_EMM_TS_descriptor] = { "CA_EMM_TS_descriptor", NULL, NULL, NULL, 0 },
	[CA_contract_information_descriptor] = { "CA_contract_information_descriptor", NULL, NULL, NULL, 0 },
	[CA_service_descriptor] = { "CA_service_descriptor", NULL, NULL, NULL, 0 },
	[TS_Information_descriptior] = { "TS_Information_descriptior", NULL, NULL, NULL, 0 },
	[extended_broadcaster_descriptor] = { "extended_broadcaster_descriptor", NULL, NULL, NULL, 0 },
	[logo_transmission_descriptor] = { "logo_transmission_descriptor", NULL, NULL, NULL, 0 },
	[basic_local_event_descriptor] = { "basic_local_event_descriptor", NULL, NULL, NULL, 0 },
	[reference_descriptor] = { "reference_descriptor", NULL, NULL, NULL, 0 },
	[node_relation_descriptor] = { "node_relation_descriptor", NULL, NULL, NULL, 0 },
	[short_node_information_descriptor] = { "short_node_information_descriptor", NULL, NULL, NULL, 0 },
	[STC_reference_descriptor] = { "STC_reference_descriptor", NULL, NULL, NULL, 0 },
	[series_descriptor] = { "series_descriptor", NULL, NULL, NULL, 0 },
	[event_group_descriptor] = { "event_group_descriptor", NULL, NULL, NULL, 0 },
	[SI_parameter_descriptor] = { "SI_parameter_descriptor", NULL, NULL, NULL, 0 },
	[broadcaster_Name_Descriptor] = { "broadcaster_Name_Descriptor", NULL, NULL, NULL, 0 },
	[component_group_descriptor] = { "component_group_descriptor", NULL, NULL, NULL, 0 },
	[SI_prime_TS_descriptor] = { "SI_prime_TS_descriptor", NULL, NULL, NULL, 0 },
	[board_information_descriptor] = { "board_information_descriptor", NULL, NULL, NULL, 0 },
	[LDT_linkage_descriptor] = { "LDT_linkage_descriptor", NULL, NULL, NULL, 0 },
	[connected_transmission_descriptor] = { "connected_transmission_descriptor", NULL, NULL, NULL, 0 },
	[content_availability_descriptor] = { "content_availability_descriptor", NULL, NULL, NULL, 0 },
	[service_group_descriptor] = { "service_group_descriptor", NULL, NULL, NULL, 0 },
	[carousel_compatible_composite_descriptor] = { "carousel_compatible_composite_descriptor", NULL, NULL, NULL, 0 },
	[conditional_playback_descriptor] = { "conditional_playback_descriptor", NULL, NULL, NULL, 0 },
	[ISDBT_delivery_system_descriptor] = { "ISDBT_delivery_system_descriptor", NULL, NULL, NULL, 0 },
	[partial_reception_descriptor] = { "partial_reception_descriptor", NULL, NULL, NULL, 0 },
	[emergency_information_descriptor] = { "emergency_information_descriptor", NULL, NULL, NULL, 0 },
	[data_component_descriptor] = { "data_component_descriptor", NULL, NULL, NULL, 0 },
	[system_management_descriptor] = { "system_management_descriptor", NULL, NULL, NULL, 0 },
	[atsc_stuffing_descriptor] = { "atsc_stuffing_descriptor", NULL, NULL, NULL, 0 },
	[atsc_ac3_audio_descriptor] = { "atsc_ac3_audio_descriptor", NULL, NULL, NULL, 0 },
	[atsc_caption_service_descriptor] = { "atsc_caption_service_descriptor", NULL, NULL, NULL, 0 },
	[atsc_content_advisory_descriptor] = { "atsc_content_advisory_descriptor", NULL, NULL, NULL, 0 },
	[atsc_extended_channel_descriptor] = { "atsc_extended_channel_descriptor", NULL, NULL, NULL, 0 },
	[atsc_service_location_descriptor] = { "atsc_service_location_descriptor", atsc_desc_service_location_init, atsc_desc_service_location_print, NULL, sizeof(struct atsc_desc_service_location) },
	[atsc_time_shifted_service_descriptor] = { "atsc_time_shifted_service_descriptor", NULL, NULL, NULL, 0 },
	[atsc_component_name_descriptor] = { "atsc_component_name_descriptor", NULL, NULL, NULL, 0 },
	[atsc_DCC_departing_request_descriptor] = { "atsc_DCC_departing_request_descriptor", NULL, NULL, NULL, 0 },
	[atsc_DCC_arriving_request_descriptor] = { "atsc_DCC_arriving_request_descriptor", NULL, NULL, NULL, 0 },
	[atsc_redistribution_control_descriptor] = { "atsc_redistribution_control_descriptor", NULL, NULL, NULL, 0 },
	[atsc_ATSC_private_information_descriptor] = { "atsc_ATSC_private_information_descriptor", NULL, NULL, NULL, 0 },
	[atsc_genre_descriptor] = { "atsc_genre_descriptor", NULL, NULL, NULL, 0 },
};

uint32_t bcd(uint32_t bcd)
{
	uint32_t ret = 0, mult = 1;
	while (bcd) {
		ret += (bcd & 0x0f) * mult;
		bcd >>=4;
		mult *= 10;
	}
	return ret;
}

int bcd_to_int(const unsigned char *bcd, int bits)
{
	int nibble = 0;
	int ret = 0;

	while (bits) {
		ret *= 10;
		if (!nibble)
			ret += *bcd >> 4;
		else
			ret += *bcd & 0x0f;
		bits -= 4;
		nibble = !nibble;
		if (!nibble)
			bcd++;
	}
	return ret;
}

void hexdump(struct dvb_v5_fe_parms *parms, const char *prefix, const unsigned char *data, int length)
{
	if (!data)
		return;
	char ascii[17];
	char hex[50];
	int i, j = 0;
	hex[0] = '\0';
	for (i = 0; i < length; i++)
	{
		char t[4];
		snprintf (t, sizeof(t), "%02x ", (unsigned int) data[i]);
		strncat (hex, t, sizeof(hex));
		if (data[i] > 31 && data[i] < 128 )
			ascii[j] = data[i];
		else
			ascii[j] = '.';
		j++;
		if (j == 8)
			strncat(hex, " ", sizeof(hex));
		if (j == 16)
		{
			ascii[j] = '\0';
			dvb_log("%s%s  %s", prefix, hex, ascii);
			j = 0;
			hex[0] = '\0';
		}
	}
	if (j > 0 && j < 16)
	{
		char spaces[47];
		spaces[0] = '\0';
		for (i = strlen(hex); i < 49; i++)
			strncat(spaces, " ", sizeof(spaces));
		ascii[j] = '\0';
		dvb_log("%s%s %s %s", prefix, hex, spaces, ascii);
	}
}