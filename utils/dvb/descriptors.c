#include "libscan.h"
#include "descriptors.h"
#include "parse_string.h"

#include <stdio.h>

static char *default_charset = "iso-8859-1";
static char *output_charset = "utf-8";

static void parse_descriptor(struct dvb_descriptors *dvb_desc,
			      const unsigned char *buf, int len)
{
	int dlen = buf[1] + 1;
	/* FIXME: Not all descriptors are valid for all tables */

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
		/* FIXME: Add parser */
		return;
	default:
		return;
	}
}

void parse_nit_descriptor(struct dvb_descriptors *dvb_desc,
			  const unsigned char *buf, int len)
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
		parse_descriptor(dvb_desc, buf, len);
		break;
	default:
		return;
	}
}