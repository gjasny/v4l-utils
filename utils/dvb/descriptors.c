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
	int dlen = buf[1] + 1;
	/* FIXME: Not all descriptors are valid for all tables */

	printf("Descriptor 0x%02x, len %d\n", buf[0], dlen);
	switch(buf[0]) {
	case network_name_descriptor:
		parse_string(&dvb_desc->nit_table.network_name,
			     &dvb_desc->nit_table.network_alias,
			     &buf[1], dlen,
			     default_charset, output_charset);
		if (dvb_desc->nit_table.network_name)
			printf("Network %s", dvb_desc->nit_table.network_name);
		if (dvb_desc->nit_table.network_alias)
			printf("(%s)", dvb_desc->nit_table.network_alias);
		printf("\n");
		return;
	case service_list_descriptor:
	case stuffing_descriptor:
	case satellite_delivery_system_descriptor:
	case cable_delivery_system_descriptor:
	case VBI_data_descriptor:
	case VBI_teletext_descriptor:
	case bouquet_name_descriptor:
	case service_descriptor:
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
		return;
	case AAC_descriptor:
		printf("AAC descriptor with len %d\n", dlen);
		return;
	case stream_identifier_descriptor:
		/* Don't need to parse it */
		printf("Component tag 0x%02x\n", buf[2]);
		return;
	default:
		printf("Unknown descriptor 0x%02x\n", buf[0]);
		return;
	}
}

void parse_nit_descriptor(struct dvb_descriptors *dvb_desc,
			  const unsigned char *buf, int len, void *ptr)
{
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
}

void parse_pmt_descriptor(struct dvb_descriptors *dvb_desc,
			  const unsigned char *buf, int len, void *ptr)
{
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
		printf("Invalid or unknown PMT descriptor 0x%02x\n", buf[0]);
		return;
	}
}
