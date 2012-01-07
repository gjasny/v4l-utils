#include "libscan.h"
#include "descriptors.h"
#include "parse_string.h"
#include "dvb_frontend.h"

#include <malloc.h>
#include <stdio.h>

static char *default_charset = "iso-8859-1";
static char *output_charset = "utf-8";

static const char *descriptors[] = {
	[0 ...255 ] = "Unknown descriptor ",
	[dvbpsi_registration_descriptor] = "dvbpsi_registration_descriptor",
	[ds_alignment_descriptor] = "ds_alignment_descriptor",
	[iso639_language_descriptor] = "iso639_language_descriptor",
	[network_name_descriptor] = "network_name_descriptor",
	[service_list_descriptor] = "service_list_descriptor",
	[stuffing_descriptor] = "stuffing_descriptor",
	[satellite_delivery_system_descriptor] = "satellite_delivery_system_descriptor",
	[cable_delivery_system_descriptor] = "cable_delivery_system_descriptor",
	[VBI_data_descriptor] = "VBI_data_descriptor",
	[VBI_teletext_descriptor] = "VBI_teletext_descriptor",
	[bouquet_name_descriptor] = "bouquet_name_descriptor",
	[service_descriptor] = "service_descriptor",
	[country_availability_descriptor] = "country_availability_descriptor",
	[linkage_descriptor] = "linkage_descriptor",
	[NVOD_reference_descriptor] = "NVOD_reference_descriptor",
	[time_shifted_service_descriptor] = "time_shifted_service_descriptor",
	[short_event_descriptor] = "short_event_descriptor",
	[extended_event_descriptor] = "extended_event_descriptor",
	[time_shifted_event_descriptor] = "time_shifted_event_descriptor",
	[component_descriptor] = "component_descriptor",
	[mosaic_descriptor] = "mosaic_descriptor",
	[stream_identifier_descriptor] = "stream_identifier_descriptor",
	[CA_identifier_descriptor] = "CA_identifier_descriptor",
	[content_descriptor] = "content_descriptor",
	[parental_rating_descriptor] = "parental_rating_descriptor",
	[teletext_descriptor] = "teletext_descriptor",
	[telephone_descriptor] = "telephone_descriptor",
	[local_time_offset_descriptor] = "local_time_offset_descriptor",
	[subtitling_descriptor] = "subtitling_descriptor",
	[terrestrial_delivery_system_descriptor] = "terrestrial_delivery_system_descriptor",
	[multilingual_network_name_descriptor] = "multilingual_network_name_descriptor",
	[multilingual_bouquet_name_descriptor] = "multilingual_bouquet_name_descriptor",
	[multilingual_service_name_descriptor] = "multilingual_service_name_descriptor",
	[multilingual_component_descriptor] = "multilingual_component_descriptor",
	[private_data_specifier_descriptor] = "private_data_specifier_descriptor",
	[service_move_descriptor] = "service_move_descriptor",
	[short_smoothing_buffer_descriptor] = "short_smoothing_buffer_descriptor",
	[frequency_list_descriptor] = "frequency_list_descriptor",
	[partial_transport_stream_descriptor] = "partial_transport_stream_descriptor",
	[data_broadcast_descriptor] = "data_broadcast_descriptor",
	[scrambling_descriptor] = "scrambling_descriptor",
	[data_broadcast_id_descriptor] = "data_broadcast_id_descriptor",
	[transport_stream_descriptor] = "transport_stream_descriptor",
	[DSNG_descriptor] = "DSNG_descriptor",
	[PDC_descriptor] = "PDC_descriptor",
	[AC_3_descriptor] = "AC_3_descriptor",
	[ancillary_data_descriptor] = "ancillary_data_descriptor",
	[cell_list_descriptor] = "cell_list_descriptor",
	[cell_frequency_link_descriptor] = "cell_frequency_link_descriptor",
	[announcement_support_descriptor] = "announcement_support_descriptor",
	[application_signalling_descriptor] = "application_signalling_descriptor",
	[adaptation_field_data_descriptor] = "adaptation_field_data_descriptor",
	[service_identifier_descriptor] = "service_identifier_descriptor",
	[service_availability_descriptor] = "service_availability_descriptor",
	[default_authority_descriptor] = "default_authority_descriptor",
	[related_content_descriptor] = "related_content_descriptor",
	[TVA_id_descriptor] = "TVA_id_descriptor",
	[content_identifier_descriptor] = "content_identifier_descriptor",
	[time_slice_fec_identifier_descriptor] = "time_slice_fec_identifier_descriptor",
	[ECM_repetition_rate_descriptor] = "ECM_repetition_rate_descriptor",
	[S2_satellite_delivery_system_descriptor] = "S2_satellite_delivery_system_descriptor",
	[enhanced_AC_3_descriptor] = "enhanced_AC_3_descriptor",
	[DTS_descriptor] = "DTS_descriptor",
	[AAC_descriptor] = "AAC_descriptor",
	[XAIT_location_descriptor] = "XAIT_location_descriptor",
	[FTA_content_management_descriptor] = "FTA_content_management_descriptor",
	[extension_descriptor] = "extension_descriptor",

	[CUE_identifier_descriptor] = "CUE_identifier_descriptor",

	[component_name_descriptor] = "component_name_descriptor",
	[logical_channel_number_descriptor] = "logical_channel_number_descriptor",

	[conditional_access_descriptor] = "conditional_access_descriptor",
	[copyright_descriptor] = "copyright_descriptor",
	[carousel_id_descriptor] = "carousel_id_descriptor",
	[association_tag_descriptor] = "association_tag_descriptor",
	[deferred_association_tags_descriptor] = "deferred_association_tags_descriptor",
	[AVC_video_descriptor] = "AVC_video_descriptor",
	[AVC_timing_and_HRD_descriptor] = "AVC_timing_and_HRD_descriptor",
	[hierarchical_transmission_descriptor] = "hierarchical_transmission_descriptor",
	[digital_copy_control_descriptor] = "digital_copy_control_descriptor",
	[network_identifier_descriptor] = "network_identifier_descriptor",
	[partial_transport_stream_time_descriptor] = "partial_transport_stream_time_descriptor",
	[audio_component_descriptor] = "audio_component_descriptor",
	[hyperlink_descriptor] = "hyperlink_descriptor",
	[target_area_descriptor] = "target_area_descriptor",
	[data_contents_descriptor] = "data_contents_descriptor",
	[video_decode_control_descriptor] = "video_decode_control_descriptor",
	[download_content_descriptor] = "download_content_descriptor",
	[CA_EMM_TS_descriptor] = "CA_EMM_TS_descriptor",
	[CA_contract_information_descriptor] = "CA_contract_information_descriptor",
	[CA_service_descriptor] = "CA_service_descriptor",
	[TS_Information_descriptior] = "TS_Information_descriptior",
	[extended_broadcaster_descriptor] = "extended_broadcaster_descriptor",
	[logo_transmission_descriptor] = "logo_transmission_descriptor",
	[basic_local_event_descriptor] = "basic_local_event_descriptor",
	[reference_descriptor] = "reference_descriptor",
	[node_relation_descriptor] = "node_relation_descriptor",
	[short_node_information_descriptor] = "short_node_information_descriptor",
	[STC_reference_descriptor] = "STC_reference_descriptor",
	[series_descriptor] = "series_descriptor",
	[event_group_descriptor] = "event_group_descriptor",
	[SI_parameter_descriptor] = "SI_parameter_descriptor",
	[broadcaster_Name_Descriptor] = "broadcaster_Name_Descriptor",
	[component_group_descriptor] = "component_group_descriptor",
	[SI_prime_TS_descriptor] = "SI_prime_TS_descriptor",
	[board_information_descriptor] = "board_information_descriptor",
	[LDT_linkage_descriptor] = "LDT_linkage_descriptor",
	[connected_transmission_descriptor] = "connected_transmission_descriptor",
	[content_availability_descriptor] = "content_availability_descriptor",
	[service_group_descriptor] = "service_group_descriptor",
	[carousel_compatible_composite_Descriptor] = "carousel_compatible_composite_Descriptor",
	[conditional_playback_descriptor] = "conditional_playback_descriptor",
	[ISDBT_delivery_system_descriptor] = "ISDBT_delivery_system_descriptor",
	[partial_reception_descriptor] = "partial_reception_descriptor",
	[emergency_information_descriptor] = "emergency_information_descriptor",
	[data_component_descriptor] = "data_component_descriptor",
	[system_management_descriptor] = "system_management_descriptor",
};

void parse_descriptor(enum dvb_tables type,
			     struct dvb_descriptors *dvb_desc,
			     const unsigned char *buf, int len)
{
	if (len == 0)
		return;

	/*
	 * FIXME: syntax need to be changed, to avoid parsing an invalid
	 * descriptor here (e. g. a descriptor at the wrong table)
	 */

	if (dvb_desc->verbose)
		printf("Descriptors table len %d\n", len);
	do {
		int dlen = buf[1];
		int err = 0;

		if (dlen > len) {
			fprintf(stderr, "descriptor size %d is longer than %d!\n",
				dlen, len);
			return;
		}
		/* FIXME: Not all descriptors are valid for all tables */

		if (dvb_desc->verbose)
			printf("%s (0x%02x), len %d\n",
			       descriptors[buf[0]], buf[0], buf[1]);
		switch(buf[0]) {
		case iso639_language_descriptor:
		{
			int i;
			const unsigned char *p = &buf[2];

			if (dvb_desc->verbose) {
				for (i = 0; i < dlen; i+= 4, p += 4) {
					printf("Language = %c%c%c, amode = %d\n",
						p[0], p[1], p[2], p[3]);
				}
			}
			break;
		}
		case network_name_descriptor:
			if (type != NIT) {
				err = 1;
				break;
			}
			parse_string(&dvb_desc->nit_table.network_name,
				&dvb_desc->nit_table.network_alias,
				&buf[2], dlen,
				default_charset, output_charset);
			if (dvb_desc->verbose) {
				if (dvb_desc->nit_table.network_name)
					printf("Network %s", dvb_desc->nit_table.network_name);
				if (dvb_desc->nit_table.network_alias)
					printf("(%s)", dvb_desc->nit_table.network_alias);
				printf("\n");
			}
			break;
		case service_descriptor: {
			if (type != SDT) {
				err = 1;
				break;
			}
			struct service_table *service_table = &dvb_desc->sdt_table.service_table[dvb_desc->cur_service];

			service_table->type = buf[2];
			parse_string(&service_table->provider_name,
				&service_table->provider_alias,
				&buf[4], buf[3],
				default_charset, output_charset);
			buf += 4 + buf[3];
			parse_string(&service_table->service_name,
				&service_table->service_alias,
				&buf[1], buf[0],
				default_charset, output_charset);
			if (dvb_desc->verbose) {
				if (service_table->provider_name)
					printf("Provider %s", service_table->provider_name);
				if (service_table->service_alias)
					printf("(%s)", service_table->provider_alias);
				if (service_table->provider_name || service_table->service_alias)
					printf("\n");
				if (service_table->service_name)
					printf("Service %s", service_table->service_name);
				if (service_table->service_alias)
					printf("(%s)", service_table->service_alias);
				printf("\n");
			}
			break;
		}
		case ds_alignment_descriptor:
		case dvbpsi_registration_descriptor:
		case service_list_descriptor:
		case stuffing_descriptor:
		case satellite_delivery_system_descriptor:
		case cable_delivery_system_descriptor:
		case VBI_data_descriptor:
		case VBI_teletext_descriptor:
		case bouquet_name_descriptor:
		case country_availability_descriptor:
		case linkage_descriptor:
		case NVOD_reference_descriptor:
		case time_shifted_service_descriptor:
		case short_event_descriptor:
		case extended_event_descriptor:
		case time_shifted_event_descriptor:
		case component_descriptor:
		case mosaic_descriptor:
		case CA_identifier_descriptor:
		case content_descriptor:
		case parental_rating_descriptor:
		case teletext_descriptor:
		case telephone_descriptor:
		case local_time_offset_descriptor:
		case subtitling_descriptor:
		case terrestrial_delivery_system_descriptor:
		case multilingual_network_name_descriptor:
		case multilingual_bouquet_name_descriptor:
		case multilingual_service_name_descriptor:
		case multilingual_component_descriptor:
		case private_data_specifier_descriptor:
		case service_move_descriptor:
		case short_smoothing_buffer_descriptor:
		case frequency_list_descriptor:
		case partial_transport_stream_descriptor:
		case data_broadcast_descriptor:
		case scrambling_descriptor:
		case data_broadcast_id_descriptor:
		case transport_stream_descriptor:
		case DSNG_descriptor:
		case PDC_descriptor:
		case AC_3_descriptor:
		case ancillary_data_descriptor:
		case cell_list_descriptor:
		case cell_frequency_link_descriptor:
		case announcement_support_descriptor:
		case application_signalling_descriptor:
		case adaptation_field_data_descriptor:
		case service_identifier_descriptor:
		case service_availability_descriptor:
		case default_authority_descriptor:
		case related_content_descriptor:
		case TVA_id_descriptor:
		case content_identifier_descriptor:
		case time_slice_fec_identifier_descriptor:
		case ECM_repetition_rate_descriptor:
		case S2_satellite_delivery_system_descriptor:
		case enhanced_AC_3_descriptor:
		case DTS_descriptor:
		case XAIT_location_descriptor:
		case FTA_content_management_descriptor:
		case extension_descriptor:

		case CUE_identifier_descriptor:
		case component_name_descriptor:
			/* FIXME: Add parser */
			break;

		case logical_channel_number_descriptor:
		{
			int i, n = dvb_desc->nit_table.lcn_len;
			const unsigned char *p = &buf[2];

			for (i = 0; i < dlen; i+= 4, p+= 4) {
				struct lcn_table **lcn = &dvb_desc->nit_table.lcn;

				*lcn = realloc(*lcn, (n + 1) * sizeof(*lcn));
				(*lcn)[n].service_id = p[0] << 8 | p[1];
				(*lcn)[n].lcn = (p[2] << 8 | p[3]) & 0x3ff;
				dvb_desc->nit_table.lcn_len++;

				if (dvb_desc->verbose)
					printf("Service ID: 0x%04x, LCN: %d\n",
					       (*lcn)[n].service_id,
					       (*lcn)[n].lcn);
			}
			break;
		}

		case conditional_access_descriptor:
		case copyright_descriptor:
		case carousel_id_descriptor:
		case association_tag_descriptor:
		case deferred_association_tags_descriptor:
		case AVC_video_descriptor:
		case AVC_timing_and_HRD_descriptor:
		case hierarchical_transmission_descriptor:
		case digital_copy_control_descriptor:
		case network_identifier_descriptor:
		case partial_transport_stream_time_descriptor:
		case audio_component_descriptor:
		case hyperlink_descriptor:
		case target_area_descriptor:
		case data_contents_descriptor:
		case video_decode_control_descriptor:
		case download_content_descriptor:
		case CA_EMM_TS_descriptor:
		case CA_contract_information_descriptor:
		case CA_service_descriptor:
		case extended_broadcaster_descriptor:
		case logo_transmission_descriptor:
		case basic_local_event_descriptor:
		case reference_descriptor:
		case node_relation_descriptor:
		case short_node_information_descriptor:
		case STC_reference_descriptor:
		case series_descriptor:
		case event_group_descriptor:
		case SI_parameter_descriptor:
		case broadcaster_Name_Descriptor:
		case component_group_descriptor:
		case SI_prime_TS_descriptor:
		case board_information_descriptor:
		case LDT_linkage_descriptor:
		case connected_transmission_descriptor:
		case content_availability_descriptor:
		case service_group_descriptor:
		case carousel_compatible_composite_Descriptor:
		case conditional_playback_descriptor:
		case emergency_information_descriptor:
		case data_component_descriptor:
		case system_management_descriptor:
			break;

		case partial_reception_descriptor:
		{
			int i;
			if (dvb_desc->verbose) {
				printf("Service IDs with partial reception = ");
				for (i = dlen + 2; i < dlen; i += 2) {
					if (dvb_desc->verbose)
						printf("%d\n",
						buf[i + 1] << 8 | buf[i]);
				}
				printf("\n");
			}
			break;
		}
		case TS_Information_descriptior:
			if (type != NIT) {
				err = 1;
				break;
			}
			dvb_desc->nit_table.virtual_channel = buf[2];
			if (dvb_desc->verbose)
				printf("Virtual channel = %d\n", buf[2]);
			break;
		case ISDBT_delivery_system_descriptor:
		{
			static const uint32_t interval[] = {
				GUARD_INTERVAL_1_32,
				GUARD_INTERVAL_1_16,
				GUARD_INTERVAL_1_8,
				GUARD_INTERVAL_1_4,
			};
			unsigned tmp = buf[3] >> 4 & 0x3;
			int i;

			if (type != NIT) {
				err = 1;
				break;
			}
			dvb_desc->nit_table.area_code = (buf[3] & 0x0f) << 8 | buf[2];
			dvb_desc->nit_table.guard_interval = interval[tmp];
			if (dvb_desc->verbose)
				printf("Area code: %d, guard interval: %d, mode: %d\n",
				       dvb_desc->nit_table.area_code,
				       tmp, buf[3] >> 6);
			for (i = dlen + 3; i < dlen; i += 2) {
				if (dvb_desc->verbose)
					printf("Frequency %d\n",
					       buf[i + 1] << 8 | buf[i]);
			}
			break;
		}
		case AAC_descriptor:
			if (dvb_desc->verbose)
				printf("AAC descriptor with len %d\n", dlen);
			break;
		case stream_identifier_descriptor:
			/* Don't need to parse it */
			if (dvb_desc->verbose)
				printf("Component tag 0x%02x\n", buf[2]);
			break;
		default:
			if (dvb_desc->verbose)
				printf("Unknown descriptor 0x%02x\n", buf[0]);
			break;
		}
		if (err) {
			fprintf(stderr,
				"descriptor type is invalid on this table\n");
		}
		buf += dlen + 2;
		len -= dlen + 2;
	} while (len > 0);
}
