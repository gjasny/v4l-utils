#include "libscan.h"
#include "descriptors.h"
#include "parse_string.h"

#include <stdio.h>

static char *default_charset = "iso-8859-1";
static char *output_charset = "utf-8";

static void parse_descriptor(struct dvb_descriptors *dvb_desc,
			      const unsigned char *buf, int len,
			      void *ptr)
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

		if (dlen > len) {
			fprintf(stderr, "descriptor size %d is longer than %d!\n",
				dlen, len);
			return;
		}
		/* FIXME: Not all descriptors are valid for all tables */

		if (dvb_desc->verbose)
			printf("Descriptor 0x%02x, len %d\n", buf[0], buf[1]);
		switch(buf[0]) {
		case network_name_descriptor:
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
			struct service_table *service_table = ptr;

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
			/* FIXME: Add parser */
			break;

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
		case TS_Information_descriptior:
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
		case ISDBT_delivery_system_descriptor:
		case partial_reception_descriptor:
		case emergency_information_descriptor:
		case data_component_descriptor:
		case system_management_descriptor:

			/* FIXME: Add parser for ISDB descriptors */
			break;
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
			fprintf(stderr, "Unknown descriptor 0x%02x\n", buf[0]);
			break;
		}
		buf += dlen + 2;
		len -= dlen + 2;
	} while (len > 0);
}

void parse_nit_descriptor(struct dvb_descriptors *dvb_desc,
			  const unsigned char *buf, int len, void *ptr)
{
#if 0
	/* Check if the descriptor is valid on a NIT table */
	switch (buf[0]) {
	case network_name_descriptor:
	case service_list_descriptor:
	case stuffing_descriptor:
	case satellite_delivery_system_descriptor:
	case cable_delivery_system_descriptor:
	case linkage_descriptor:
	case terrestrial_delivery_system_descriptor:
	case multilingual_network_name_descriptor:
	case private_data_specifier_descriptor:
	case frequency_list_descriptor:
	case cell_list_descriptor:
	case cell_frequency_link_descriptor:
	case default_authority_descriptor:
	case time_slice_fec_identifier_descriptor:
	case S2_satellite_delivery_system_descriptor:
	case XAIT_location_descriptor:
	case FTA_content_management_descriptor:
	case extension_descriptor:
		parse_descriptor(dvb_desc, buf, len, ptr);
		break;
	default:
		printf("Invalid or unknown NIT descriptor 0x%02x\n", buf[0]);
		return;
	}
#endif
	parse_descriptor(dvb_desc, buf, len, ptr);
}

void parse_pmt_descriptor(struct dvb_descriptors *dvb_desc,
			  const unsigned char *buf, int len, void *ptr)
{
#if 0
	/* Check if the descriptor is valid on a PAT table */
	switch (buf[0]) {
	case VBI_data_descriptor:
	case VBI_teletext_descriptor:
	case mosaic_descriptor:
	case stream_identifier_descriptor:
	case teletext_descriptor:
	case subtitling_descriptor:
	case private_data_specifier_descriptor:
	case service_move_descriptor:
	case scrambling_descriptor:
	case data_broadcast_id_descriptor:
	case AC_3_descriptor:
	case ancillary_data_descriptor:
	case application_signalling_descriptor:
	case adaptation_field_data_descriptor:
	case ECM_repetition_rate_descriptor:
	case enhanced_AC_3_descriptor:
	case DTS_descriptor:
	case AAC_descriptor:
	case XAIT_location_descriptor:
	case FTA_content_management_descriptor:
	case extension_descriptor:
		parse_descriptor(dvb_desc, buf, len, ptr);
		break;
	default:
		fprintf(stderr, "Invalid or unknown PMT descriptor 0x%02x\n", buf[0]);
		return;
	}
#endif
	parse_descriptor(dvb_desc, buf, len, ptr);
}

void parse_sdt_descriptor(struct dvb_descriptors *dvb_desc,
			  const unsigned char *buf, int len, void *ptr)
{
#if 0
	/* Check if the descriptor is valid on a PAT table */
	switch (buf[0]) {
	case stuffing_descriptor:
	case bouquet_name_descriptor:
	case service_descriptor:
	case country_availability_descriptor:
	case linkage_descriptor:
	case NVOD_reference_descriptor:
	case time_shifted_service_descriptor:
	case component_descriptor:
	case mosaic_descriptor:
	case CA_identifier_descriptor:
	case content_descriptor:
	case parental_rating_descriptor:
	case telephone_descriptor:
	case multilingual_service_name_descriptor:
	case private_data_specifier_descriptor:
	case short_smoothing_buffer_descriptor:
	case data_broadcast_descriptor:
	case PDC_descriptor:
	case TVA_id_descriptor:
	case content_identifier_descriptor:
	case XAIT_location_descriptor:
	case FTA_content_management_descriptor:
	case extension_descriptor:
		parse_descriptor(dvb_desc, buf, len, ptr);
		break;
	default:
		fprintf(stderr, "Invalid or unknown SDT descriptor 0x%02x\n", buf[0]);
		return;
	}
#endif
	parse_descriptor(dvb_desc, buf, len, ptr);
}
